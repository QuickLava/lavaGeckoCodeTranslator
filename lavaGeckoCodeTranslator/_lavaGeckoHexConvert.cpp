#include "stdafx.h"
#include "_lavaGeckoHexConvert.h"

namespace lava::gecko
{
	// Constants
	constexpr unsigned long signatureBaPoMask = 0x10000000;
	constexpr unsigned long signatureAddressMask = 0x1FFFFFF;
	const std::string withEndifString = " (With Endif)";
	const std::set<std::string> disallowedMnemonics = {"mfspr", "mtspr"};

	// Dynamic Values
	// These are used to try to keep track of BA and PO so they can be used in GCTRM syntax instructions.
	// We can't *always* know what these values are, since they can be loaded from and stored to memory technically,
	// and we have no guarantees about what's in memory at any given moment; so we have to settle for just tracking
	// what we can, and paying close attention to invalidate our stored values when we lose their values. For this
	// purpose, ULONG_MAX (0xFFFFFFFF) will be used as our invalidation value.
	unsigned long currentBAValue = 0x80000000;
	unsigned long currentPOValue = 0x80000000;
	std::array<unsigned long, 0xF> geckoRegisters{};
	bool validateCurrentBAValue()
	{
		return currentBAValue != ULONG_MAX;
	}
	void invalidateCurrentBAValue()
	{
		currentBAValue = ULONG_MAX;
	}
	void resetBAValue()
	{
		currentBAValue = 0x80000000;
	}
	bool validateCurrentPOValue()
	{
		return currentPOValue != ULONG_MAX;
	}
	void invalidateCurrentPOValue()
	{
		currentPOValue = ULONG_MAX;
	}
	void resetPOValue()
	{
		currentPOValue = 0x80000000;
	}
	bool validateGeckoRegister(unsigned char regIndex)
	{
		bool result = 0;

		if (regIndex < geckoRegisters.size())
		{
			result = geckoRegisters[regIndex] != ULONG_MAX;
		}

		return result;
	}
	bool invalidateGeckoRegister(unsigned char regIndex)
	{
		bool result = 0;

		if (regIndex < geckoRegisters.size())
		{
			geckoRegisters[regIndex] = ULONG_MAX;
			result = 1;
		}

		return result;
	}
	void invalidateAllGeckoRegisters()
	{
		geckoRegisters.fill(ULONG_MAX);
	}
	void resetParserDynamicValues(bool skipRegisterReset)
	{
		resetBAValue();
		resetPOValue();
		if (!skipRegisterReset)
		{
			invalidateAllGeckoRegisters();
		}
	}

	// Data Embed Detection
	// These are used to track locations where raw hex data may be embedded in the code stream.
	// Parsing these as code can lead to bad times™, so we need to ensure we skip them wherever
	// possible, hence this system. Core idea is: as we process 46/4E codes, we check if they're
	// specifying an offset that might be signalling an embed. If so, we flag that location as a
	// potential embed to look out for. From there, we look out for any GOTO codes which occur just
	// before the suspected embed statement; and, if we find one, we know we've reached an embed, and
	// we can dump it next go round of the parser, and continue translation from there.
	unsigned short detectedDataEmbedLineCount = 0;
	bool didBAPOStoreToAddressWhileSuspectingEmbed = 0;
	std::set<unsigned long> suspectedEmbedLocations{};
	std::set<unsigned long> activeGotoEndLocations{};
	void removeExpiredLocationsFromSet(unsigned long currentStreamLocation, std::set<unsigned long>& targetSet)
	{
		for (auto i = targetSet.begin(); i != targetSet.end();)
		{
			if (*i <= currentStreamLocation)
			{
				i = targetSet.erase(i);
			}
			else
			{
				// The locations are sorted in ascending order, lowest first
				// If a given number in the list is higher than our current location,
				// everything after it is guaranteed higher as well.
				break;
			}
		}
	}
	void removeExpiredEmbedSuspectLocations(unsigned long currentStreamLocation)
	{
		removeExpiredLocationsFromSet(currentStreamLocation, suspectedEmbedLocations);
	}
	void removeExpiredGotoEndLocations(unsigned long currentStreamLocation)
	{
		removeExpiredLocationsFromSet(currentStreamLocation, activeGotoEndLocations);
	}
	// Gecko Loop Tracking
	std::stack<unsigned long> activeRepeatStartLocations{};
	void resetParserTrackingValues()
	{
		detectedDataEmbedLineCount = 0;
		didBAPOStoreToAddressWhileSuspectingEmbed = 0;
		suspectedEmbedLocations.clear();
		activeGotoEndLocations.clear();
		while (!activeRepeatStartLocations.empty())
		{
			activeRepeatStartLocations.pop();
		}
	}

	// Misc Settings
	bool doPPCHexEncodingComments = 1;


	// Utility
	unsigned long getAddressFromCodeSignature(unsigned long codeSignatureIn)
	{
		unsigned long result = ULONG_MAX;

		// If we're using BA
		if ((codeSignatureIn & signatureBaPoMask) == 0)
		{
			result = (currentBAValue != ULONG_MAX) ? currentBAValue + (signatureAddressMask & codeSignatureIn) : ULONG_MAX;
		}
		// Else, if we're using PO
		else
		{
			result = (currentPOValue != ULONG_MAX) ? currentPOValue + (signatureAddressMask & codeSignatureIn) : ULONG_MAX;
		}

		return result;
	}
	std::string getAddressComponentString(unsigned long codeSignatureIn)
	{
		std::string result = "$(";
		if (codeSignatureIn & signatureBaPoMask)
		{
			result += "po";
		}
		else
		{
			result += "ba";
		}
		result += " + 0x" + lava::numToHexStringWithPadding(codeSignatureIn & signatureAddressMask, 8) + ")";
		return result;
	}
	std::string getCodeExecStatusString(unsigned long codeSignatureIn)
	{
		std::string result = "";

		unsigned char executionCondition = (codeSignatureIn & 0x00F00000) >> 0x14;
		switch (executionCondition)
		{
		case 0: { result = "If Exec Status is True"; break; }
		case 1: { result = "If Exec Status is False"; break; }
		case 2: { result = "Regardless of Execution Status"; break; }
		default: { break; }
		}

		return result;
	}
	void printStringWithComment(std::ostream& outputStream, const std::string& primaryString, const std::string& commentString, bool printNewLine = 1, unsigned long relativeCommentLoc = 0x20)
	{
		lava::ppc::printStringWithComment(outputStream, primaryString, commentString, printNewLine, relativeCommentLoc, activeRepeatStartLocations.size());
	}
	std::string getStringWithComment(const std::string& primaryString, const std::string& commentString, unsigned long relativeCommentLoc = 0x20)
	{
		return lava::ppc::getStringWithComment(primaryString, commentString, relativeCommentLoc, activeRepeatStartLocations.size());
	}
	std::size_t dumpUnannotatedHexToStream(std::istream& codeStreamIn, std::ostream& output, std::size_t linesToDump, std::string commentStr = "")
	{
		std::size_t result = 0;

		if (linesToDump > 0)
		{
			// Buffer for reading in bytes from stream.
			std::string dumpStr("");
			dumpStr.reserve(8);
			// Build the hexVec we'll be passing to the formatting function.
			// We need 2 hex words per line we're dumping, so initialize those entries to 0x00.
			std::vector<unsigned long> hexVec(linesToDump * 2, 0x00);
			for (std::size_t i = 0; codeStreamIn.good() && i < hexVec.size(); i++)
			{
				result += 0x8;
				// and for each, pull 8 chars from the code stream...
				lava::readNCharsFromStream(dumpStr, codeStreamIn, 0x8, 0);
				// ... and convert them to a proper hex number, writing them into the appropriate line of the hexVec.
				hexVec[i] = lava::stringToNum<unsigned long>(dumpStr, 0, ULONG_MAX, 1);
			}

			// Pass that hexVec to the embed formatting func to get our formatted output back...
			std::vector<std::string> outputVec = lava::ppc::formatRawDataEmbedOutput(hexVec, "*", " ", 2, 0x30, commentStr);
			// ... and finally write each formatted line to the output!
			for (std::size_t i = 0; i < outputVec.size(); i++)
			{
				output << outputVec[i] << "\n";
			}
		}

		return result;
	}

	std::string convertPPCInstructionHex(unsigned long hexIn, bool enableComment)
	{
		std::string result = "";

		result = lava::ppc::convertInstructionHexToString(hexIn);
		if (enableComment && !result.empty())
		{
			result = lava::gecko::getStringWithComment(result, "0x" + lava::numToHexStringWithPadding(hexIn, 8));
		}

		return result;
	}
	std::string convertPPCInstructionHex(std::string hexIn, bool enableComment)
	{
		std::string result = "";

		unsigned long integerConversion = lava::stringToNum<unsigned long>(hexIn, 0, ULONG_MAX, 1);
		result = convertPPCInstructionHex(integerConversion, enableComment);

		return result;
	}

	// Conversion Predicates
	std::size_t defaultGeckoCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		return 0;
	}
	std::size_t geckoBasicRAMWriteCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			// Initialize Strings For Output
			std::string outputString("");
			std::stringstream commentString("");

			// Output First Line
			outputString = "* " + signatureWord + " " + immWord;
			commentString << codeTypeIn->name << " @ " << getAddressComponentString(signatureNum) << ": ";
			switch (codeTypeIn->secondaryCodeType)
			{
			case 00:
			{
				commentString << " 0x" << lava::numToHexStringWithPadding((immNum & 0xFF), 2);
				if (immNum >> 0x10)
				{
					commentString << " (" << ((immNum >> 0x10) + 1) << " times)";
				}
				break;
			}
			case 02:
			{
				commentString << " 0x" << lava::numToHexStringWithPadding((immNum & 0xFFFF), 4);
				if (immNum >> 0x10)
				{
					commentString << " (" << ((immNum >> 0x10) + 1) << " times)";
				}
				break;
			}
			case 04:
			{
				commentString << " 0x" << lava::numToHexStringWithPadding(immNum, 8);
				break;
			}
			default:
			{
				break;
			}
			}
			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t gecko06CodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);
			std::vector<unsigned char> bytesToWrite(lava::padLengthTo<unsigned long>(immNum, 0x08, 1), UCHAR_MAX);

			std::string byteInStr("");
			unsigned char byteIn = CHAR_MAX;
			bool allPrintChars = 1;
			bool lastByteWasZero = 0;
			bool isNullTerminated = 0;
			unsigned long terminatorCount = 0x00;
			for (unsigned long i = 0; i < bytesToWrite.size(); i++)
			{
				lava::readNCharsFromStream(byteInStr, codeStreamIn, 2, 0);
				byteIn = lava::stringToNum<unsigned char>(byteInStr, 0, UCHAR_MAX, 1);
				bytesToWrite[i] = byteIn;
				if (i < immNum)
				{
					// We need to allow null terminators between strings, so we have to allow 0x00.
					if (byteIn == 0x00)
					{
						if (allPrintChars && !lastByteWasZero)
						{
							terminatorCount++;
						}
					}
					else if (!std::isprint(byteIn))
					{
						allPrintChars = 0;
					}
				}
				lastByteWasZero = byteIn == 0x00;
			}
			isNullTerminated = allPrintChars && (bytesToWrite[immNum - 1] == 0x00);

			// Initialize Strings For Output
			std::string outputString("");
			std::stringstream commentString("");

			// Output First Line
			outputString = "* " + signatureWord + " " + immWord;
			commentString << codeTypeIn->name << " (" << std::to_string(immNum) << " characters";
			if (allPrintChars && isNullTerminated && terminatorCount > 1)
			{
				commentString << ", " << terminatorCount << " strings";
			}
			commentString << ") @ " << getAddressComponentString(signatureNum) << ":";
			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			// Loop through the rest of the output
			std::size_t cursor = 0;
			std::size_t numFullLines = bytesToWrite.size() / 8;
			bool stringCurrentlyOpen = 0;
			for (unsigned long i = 0; i < numFullLines; i++)
			{
				outputString = "* " + lava::numToHexStringWithPadding(lava::bytesToFundamental<unsigned long>(bytesToWrite.data() + cursor), 8);
				outputString += " " + lava::numToHexStringWithPadding(lava::bytesToFundamental<unsigned long>(bytesToWrite.data() + cursor + 4), 8);
				commentString.str("");
				commentString << "\t";
				// String Comment Output
				if (allPrintChars)
				{
					unsigned char currCharacter = UCHAR_MAX;
					if (stringCurrentlyOpen)
					{
						commentString << "... ";
					}
					for (unsigned long u = 0; (u < 8) && ((u + cursor) < immNum); u++)
					{
						currCharacter = bytesToWrite[u + cursor];
						if ((stringCurrentlyOpen && currCharacter == 0x00) || (!stringCurrentlyOpen && currCharacter != 0x00))
						{
							// If we're starting a string on an empty line, and there are enough characters
							// left to output to fill spill over into the next line...
							if (!stringCurrentlyOpen && u == 0 && ((immNum - cursor) > 8))
							{
								// ... print some spaces before the line to align it with the rest of the string.
								commentString << "   ";
							}
							commentString << "\"";
							stringCurrentlyOpen = !stringCurrentlyOpen;
						}
						if (currCharacter != 0x00)
						{
							commentString << currCharacter;
						}
					}
					if (stringCurrentlyOpen)
					{
						if ((i + 1) < numFullLines)
						{
							if (bytesToWrite[cursor + 8] == 0x00)
							{
								commentString << "\"";
								stringCurrentlyOpen = 0;
							}
							else
							{
								commentString << " ...";
							}
						}
						else
						{
							commentString << "\" (Note: Not Null-Terminated!)";
						}
					}
				}
				// Raw Hex Output
				else
				{
					commentString << "0x" << lava::numToHexStringWithPadding(bytesToWrite[cursor], 2);
					for (unsigned long u = 1; (u < 8) && ((u + cursor) < immNum); u++)
					{
						commentString << ", 0x" << lava::numToHexStringWithPadding(bytesToWrite[u + cursor], 2);
					}
				}
				lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

				cursor += 8;
			}

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t gecko08CodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string valInitWord("");
			std::string settingsWord("");
			std::string valIncrWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(valInitWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(settingsWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(valIncrWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long valInitNum = lava::stringToNum<unsigned long>(valInitWord, 0, ULONG_MAX, 1);
			unsigned long settingsNum = lava::stringToNum<unsigned long>(settingsWord, 0, ULONG_MAX, 1);
			unsigned long valIncrNum = lava::stringToNum<unsigned long>(valIncrWord, 0, ULONG_MAX, 1);

			unsigned char valueSize = (settingsNum & 0xF0000000) >> 0x1C;
			unsigned short numWrites = (settingsNum & 0x0FFF0000) >> 0x10;
			unsigned short addressIncrValue = (settingsNum & 0xFFFF);

			// Initialize Strings For Output
			std::string outputString("");
			std::stringstream commentString("");

			// Output First Line
			outputString = "* " + signatureWord + " " + valInitWord;
			commentString << codeTypeIn->name << " (";
			switch (valueSize)
			{
			case 0: { commentString << "8-bit"; break; }
			case 1: { commentString << "16-bit"; break; }
			case 2: { commentString << "32-bit"; break; }
			default: { break; }
			}
			commentString << "): Start @ " << getAddressComponentString(signatureNum) << ", Initial Value = ";
			switch (valueSize)
			{
			case 0: 
			{ 
				commentString << "0x" << lava::numToHexStringWithPadding(valInitNum & 0xFF, 2);
				break; 
			}
			case 1:
			{
				commentString << "0x" << lava::numToHexStringWithPadding(valInitNum & 0xFFFF, 4);
				break;
			}
			case 2:
			{
				commentString << "0x" << lava::numToHexStringWithPadding(valInitNum, 8);
				break;
			}
			default:
			{
				break; 
			}
			}
			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			outputString = "* " + settingsWord + " " + valIncrWord;
			commentString.str("");
			commentString << "\tDo " << (numWrites + 1) << " write(s)";
			commentString << ", Increment Addr by 0x" << lava::numToHexStringWithPadding(addressIncrValue, 4);
			commentString << ", Increment Value by 0x" << lava::numToHexStringWithPadding(valIncrNum, 8);
			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoCompareCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			std::string outputStr = "* " + signatureWord + " " + immWord;

			std::stringstream commentStr("");
			commentStr << codeTypeIn->name;
			if (signatureNum & 1)
			{
				commentStr << withEndifString;
				signatureNum &= ~1;
			}
			commentStr << ": If Val @ " << getAddressComponentString(signatureNum) << " ";

			if (codeTypeIn->secondaryCodeType > 6 && ((immNum >> 0x10) != 0))
			{
				commentStr << "& 0x" << lava::numToHexStringWithPadding<unsigned short>(~(immNum >> 0x10), 4) << " ";
			}

			switch (codeTypeIn->secondaryCodeType % 8)
			{
			case 0: { commentStr << "=="; break; }
			case 2: { commentStr << "!="; break; }
			case 4: { commentStr << ">"; break; }
			case 6: { commentStr << "<"; break; }
			default: {break; }
			}

			if (codeTypeIn->secondaryCodeType <= 6)
			{
				commentStr << " 0x" << lava::numToHexStringWithPadding(immNum, 8);
			}
			else
			{
				commentStr << " 0x" << lava::numToHexStringWithPadding<unsigned short>(immNum & 0xFFFF, 4);
			}

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoSetRepeatConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			// Initialize Strings For Output
			std::string outputString("");
			std::stringstream commentString("");

			// Build and Print Line
			outputString = "* " + signatureWord + " " + immWord;
			commentString << codeTypeIn->name << ": Repeat " << (signatureNum & 0xFFFF) << " times, Store in b" << (immNum & 0xF);
			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;

			// Lastly, signal that we've entered a Repeat block.
			activeRepeatStartLocations.push(initialPos);
		}

		return result;
	}
	std::size_t geckoExecuteRepeatConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			// Initialize Strings For Output
			std::string outputString("");
			std::stringstream commentString("");

			// Build and Print Line
			outputString = "* " + signatureWord + " " + immWord;
			commentString << codeTypeIn->name << ": Execute Repeat in b" << (immNum & 0xF);

			// Before we print, signal that we're leaving a Repeat block (before print to un-indent this line).
			activeRepeatStartLocations.pop();

			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoReturnConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			// Initialize Strings For Output
			std::string outputString("");
			std::stringstream commentString("");

			// Start Building Lines
			outputString = "* " + signatureWord + " " + immWord;
			commentString << codeTypeIn->name << ": Jump to Addr. in b" << (immNum & 0xF) << " " << getCodeExecStatusString(signatureNum);
			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoGotoGosubConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			unsigned char executionCondition = (signatureNum & 0x00F00000) >> 0x14;

			// Initialize Strings For Output
			std::string outputString("");
			std::stringstream commentString("");

			// Start Building Lines
			outputString = "* " + signatureWord + " " + immWord;
			commentString << codeTypeIn->name << ": "; 
			if (codeTypeIn->secondaryCodeType == 8)
			{
				commentString << "Store Next Line Addr. in b" << (immNum & 0xF) << ", ";
			}

			commentString << "Jump to Next Line";
			signed short lineOffset = static_cast<signed short>(signatureNum & 0xFFFF);
			if (lineOffset != 0)
			{
				if (lineOffset > 0)
				{
					commentString << ", then forward " << lineOffset << " more";
				}
				else
				{
					commentString << ", then backwards " << -lineOffset;
				}
				commentString << " Line(s)";
			}
			commentString << " " << getCodeExecStatusString(signatureNum);
			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;

			// If we've arrived at a GOTO that skips forwards...
			if ((codeTypeIn->secondaryCodeType == 6) && (lineOffset > 0))
			{
				unsigned long currentGotoEndLocation = unsigned long(codeStreamIn.tellg()) + (lineOffset * 0x10);
				// ... and we're currently suspecting a data embed...
				if (!suspectedEmbedLocations.empty())
				{
					// ... check if this GOTO corresponds to a suspected Data Embed location.
					if (suspectedEmbedLocations.find(codeStreamIn.tellg()) != suspectedEmbedLocations.end())
					{
						// If so, signal the number of lines the embed takes up so we can dump it.
						detectedDataEmbedLineCount = unsigned short(lineOffset);
					}
					// Otherwise, if we've done a BAPO store since we established our suspicion...
					else if (didBAPOStoreToAddressWhileSuspectingEmbed)
					{
						// ... AND this GOTO would take us *past* a suspected embed location...
						if (currentGotoEndLocation > *suspectedEmbedLocations.begin())
						{
							// ... also signal, as this is probably an embed as well.
							detectedDataEmbedLineCount = unsigned short(lineOffset);
						}
					}
				}
				// The only other situation under which we can expect we're looking at an embed is if this Goto
				// is set to activate regardles of the current Execution Status, AND we can be sure that there isn't another
				// Goto that would land us within the region skipped by this potential embed Goto. If both of those conditiosn are true...
				else if ((executionCondition == 2) && (activeGotoEndLocations.empty() || (currentGotoEndLocation < *activeGotoEndLocations.begin())))
				{
					// ... signal that we've reached an embed.
					detectedDataEmbedLineCount = unsigned short(lineOffset);
				}

				// Finally, record the expected end location for this Goto, it'll be removed once we pass it.
				activeGotoEndLocations.insert(currentGotoEndLocation);
			}
		}

		return result;
	}
	std::size_t geckoSetLoadStoreAddressCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string addrWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(addrWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long addrNum = lava::stringToNum<unsigned long>(addrWord, 0, ULONG_MAX, 1);

			// 1 if we're overwriting PO, 0 if overwriting BA
			bool settingPO = codeTypeIn->secondaryCodeType >= 8;
			// Is 0 for Load, 2 for Set, 6 for Store
			unsigned char setLoadStoreMode = codeTypeIn->secondaryCodeType % 8;
			// 1 if we're adding result to current value, 0 if we're just overwriting it.
			bool incrementMode = signatureNum & 0x00100000;
			// If UCHAR_MAX, no Add. If 0, add BA. If 1, add PO.
			unsigned char bapoAdd = UCHAR_MAX;
			if (signatureNum & 0x00010000)
			{
				// If adding BA
				if ((signatureNum & signatureBaPoMask) == 0)
				{
					bapoAdd = 0;
				}
				// If adding PO
				else
				{
					bapoAdd = 1;
				}
			}
			// If UCHAR_MAX, no Add. Otherwise, use the indexed register.
			unsigned char geckoRegisterAddIndex = UCHAR_MAX;
			if (signatureNum & 0x00001000)
			{
				geckoRegisterAddIndex = signatureNum & 0xF;
			}

			std::string outputStr = "* " + signatureWord + " " + addrWord;
			std::stringstream commentStr("");

			// Build string for value we'll actually be using to set/load/store.
			std::stringstream setLoadStoreString("");
			if (bapoAdd != UCHAR_MAX)
			{
				if (bapoAdd == 0)
				{
					setLoadStoreString << "ba + ";
				}
				else
				{
					setLoadStoreString << "po + ";
				}
			}
			if (geckoRegisterAddIndex != UCHAR_MAX)
			{
				setLoadStoreString << "gr" << +geckoRegisterAddIndex << " + ";
			}
			setLoadStoreString << "0x" << addrWord;

			commentStr << codeTypeIn->name << ": ";

			// If this is a Load Mode codetype...
			if (setLoadStoreMode == 0)
			{
				// ... build Load Mode comment string.
				if (!settingPO)
				{
					commentStr << "ba ";
				}
				else
				{
					commentStr << "po ";
				}
				if (incrementMode)
				{
					commentStr << "+";
				}
				commentStr << "= Val @ $(" << setLoadStoreString.str() << ")";

				// Invalidate targeted value; we can't know what it is after loading from RAM.
				if (!settingPO)
				{
					invalidateCurrentBAValue();
				}
				else
				{
					invalidateCurrentPOValue();
				}
			}
			// Else, if this is a Set Mode codetype...
			else if (setLoadStoreMode == 2)
			{
				// ... build Set Mode comment string.
				if (!settingPO)
				{
					commentStr << "ba ";
				}
				else
				{
					commentStr << "po ";
				}
				if (incrementMode)
				{
					commentStr << "+";
				}
				commentStr << "= " << setLoadStoreString.str();

				// Apply changes to saved BAPO values.
				bool componentsAllValid = 1;
				// If the current value is implicated in the result of the calculation...
				if (incrementMode)
				{
					// ... and we're setting BA...
					if (!settingPO)
					{
						// ... require that BA is valid.
						componentsAllValid &= validateCurrentBAValue();
					}
					// Otherwise, if we're setting PO...
					else
					{
						// ... require that PO is valid.
						componentsAllValid &= validateCurrentPOValue();
					}
				}
				// If we're adding either BA or PO separately...
				if (bapoAdd != UCHAR_MAX)
				{
					// ... if we're adding BA...
					if (bapoAdd == 0)
					{
						// ... require that it's valid.
						componentsAllValid &= validateCurrentBAValue();
					}
					// Else, if we're adding PO instead...
					else
					{
						// ... require that *that's* valid.
						componentsAllValid &= validateCurrentPOValue();
					}
				}
				// Last, if we're adding a Gecko Register...
				if (geckoRegisterAddIndex != UCHAR_MAX)
				{
					// ... require that it too is valid.
					componentsAllValid &= validateGeckoRegister(geckoRegisterAddIndex);
				}
				// If after all that, our components are all valid, we can overwrite our target value!
				if (componentsAllValid)
				{
					// We can start with the passed in address value, since that's always part of the result.
					unsigned long result = addrNum;
					// If we need to add either BA or PO, add that.
					if (bapoAdd != UCHAR_MAX)
					{
						if (bapoAdd == 0)
						{
							result += currentBAValue;
						}
						else
						{
							result += currentPOValue;;
						}
					}
					// If we need to add a geckoRegister's value, add that too.
					if (geckoRegisterAddIndex != UCHAR_MAX)
					{
						result += geckoRegisters[geckoRegisterAddIndex];
					}
					// Lastly, if we're in increment mode, we can add our result to our target value.
					if (incrementMode)
					{
						if (!settingPO)
						{
							currentBAValue += result;
						}
						else
						{
							currentPOValue += result;
						}
					}
					// Otherwise, we can just overwrite it.
					else
					{
						if (!settingPO)
						{
							currentBAValue = result;
						}
						else
						{
							currentPOValue = result;
						}
					}
				}
				// Otherwise, we have to invalidate our target value, cuz we can't calculate it.
				else
				{
					if (!settingPO)
					{
						invalidateCurrentBAValue();
					}
					else
					{
						invalidateCurrentPOValue();
					}
				}
			}
			// Else, if this is a Store Mode codetype...
			else if (setLoadStoreMode == 4)
			{
				// ... build Store Mode comment string.
				commentStr << "Val @ $(" << setLoadStoreString.str() << ") = ";
				if (!settingPO)
				{
					commentStr << "ba";
				}
				else
				{
					commentStr << "po";
				}

				// Note, we don't do anything to the targeted BAPO value here cuz we aren't changing it.
			}

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;

			// If we're performing a BAPO store, and we're currently suspecting an embed somewhere...
			if (setLoadStoreMode == 4 && !suspectedEmbedLocations.empty())
			{
				// ... and we're either not using BAPO in the address calculation, or at least aren't using the one we're storing...
				if ((bapoAdd == UCHAR_MAX) || (bool(signatureNum & signatureBaPoMask) != bool(bapoAdd)))
				{
					// ... note that it's occurred; this is an extra hint that we're approaching an embed.
					didBAPOStoreToAddressWhileSuspectingEmbed = 1;
				}
			}
		}

		return result;
	}
	std::size_t gecko464ECodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);
			signed short addressOffset = static_cast<signed short>(signatureNum & 0xFFFF);

			std::string outputStr = "* " + signatureWord + " " + immWord;
			std::stringstream commentStr("");
			commentStr << codeTypeIn->name << ": ";
			// If we're setting BA
			if ((signatureNum >> 0x18) == 0x46)
			{
				commentStr << "ba";
				// We won't know the value for this, so invalidate.
				invalidateCurrentBAValue();
			}
			// If we're setting PO
			else
			{
				commentStr << "po";
				// Same as above, we won't know the value, so we invalidate.
				invalidateCurrentPOValue();
			}
			commentStr << " = (Next Code Address)";
			if (addressOffset > 0)
			{
				commentStr << " + " << addressOffset;
			}
			else if (addressOffset < 0)
			{
				commentStr << " - " << -addressOffset;
			}

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;

			// If this offset is both properly aligned and large enough to be used for a Data Embed...
			if (addressOffset >= 0x8 && (addressOffset % 0x8) == 0)
			{
				// ... report the suspected data location so we can check for it later.
				suspectedEmbedLocations.insert(unsigned long(codeStreamIn.tellg()) + (addressOffset * 2));
			}
		}

		return result;
	}
	std::size_t geckoSetGeckoRegCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string addrWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(addrWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long addrNum = lava::stringToNum<unsigned long>(addrWord, 0, ULONG_MAX, 1);

			// 1 if we're adding result to current value, 0 if we're just overwriting it.
			bool incrementMode = signatureNum & 0x00100000;
			// If UCHAR_MAX, no Add. If 0, add BA. If 1, add PO.
			unsigned char bapoAdd = UCHAR_MAX;
			if (signatureNum & 0x00010000)
			{
				// If adding BA
				if ((signatureNum & signatureBaPoMask) == 0)
				{
					bapoAdd = 0;
				}
				// If adding PO
				else
				{
					bapoAdd = 1;
				}
			}
			// Gecko Register to work with
			unsigned char targetGeckoRegister = signatureNum & 0xF;

			std::string outputStr = "* " + signatureWord + " " + addrWord;
			std::stringstream commentStr("");

			// Build string for value we'll actually be using to set/load/store.
			std::stringstream setLoadStoreString("");
			if (bapoAdd != UCHAR_MAX)
			{
				if (bapoAdd == 0)
				{
					setLoadStoreString << "ba + ";
				}
				else
				{
					setLoadStoreString << "po + ";
				}
			}
			setLoadStoreString << "0x" + addrWord;

			commentStr << codeTypeIn->name << ": ";

			commentStr << "gr" << +targetGeckoRegister << " ";
			if (incrementMode)
			{
				commentStr << "+";
			}
			commentStr << "= " << setLoadStoreString.str();

			// Apply changes to saved BAPO values.
			bool componentsAllValid = 1;
			// If our target gecko register is implicated in the result of the calculation...
			if (incrementMode)
			{
				// ... ensure its current value is valid.
				componentsAllValid &= validateGeckoRegister(targetGeckoRegister);
			}
			// If we're adding either BA or PO...
			if (bapoAdd != UCHAR_MAX)
			{
				// ... if we're adding BA...
				if (bapoAdd == 0)
				{
					// ... require that it's valid.
					componentsAllValid &= validateCurrentBAValue();
				}
				// Else, if we're adding PO instead...
				else
				{
					// ... require that *that's* valid.
					componentsAllValid &= validateCurrentPOValue();
				}
			}
			// If after all that, our components are all valid, we can overwrite our target value!
			if (componentsAllValid)
			{
				// We can start with the passed in address value, since that's always part of the result.
				unsigned long result = addrNum;
				// If we need to add either BA or PO, add that.
				if (bapoAdd != UCHAR_MAX)
				{
					if (bapoAdd == 0)
					{
						result += currentBAValue;
					}
					else
					{
						result += currentPOValue;;
					}
				}
				// And if we're in increment mode...
				if (incrementMode)
				{
					// ... then we'll add the result directly to the current register value.
					geckoRegisters[targetGeckoRegister] += result;
				}
				// Otherwise...
				else
				{
					// ... just overwrite the value.
					geckoRegisters[targetGeckoRegister] = result;
				}
			}
			// Otherwise, we have to invalidate our target value, cuz we can't calculate it.
			else
			{
				invalidateGeckoRegister(targetGeckoRegister);
			}

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoLoadStoreGeckoRegCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string addrWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(addrWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long addrNum = lava::stringToNum<unsigned long>(addrWord, 0, ULONG_MAX, 1);

			// Is 0 for Load, 1 for Store
			bool loadStoreMode = codeTypeIn->secondaryCodeType == 2;
			// Determines whether to treat the register's value	a byte, short, or long.
			unsigned char valueSize = (signatureNum & 0x00F00000) >> 0x14;
			// If UCHAR_MAX, no Add. If 0, add BA. If 1, add PO.
			unsigned char bapoAdd = UCHAR_MAX;
			if (signatureNum & 0x00010000)
			{
				// If adding BA
				if ((signatureNum & signatureBaPoMask) == 0)
				{
					bapoAdd = 0;
				}
				// If adding PO
				else
				{
					bapoAdd = 1;
				}
			}
			// Only used in store mode; number of times to store the register's value in subsequent addresses
			unsigned short valueStoreCount = signatureNum & 0x0000FFF0;
			// Gecko Register to work with
			unsigned char targetGeckoRegister = signatureNum & 0xF;

			std::string outputStr = "* " + signatureWord + " " + addrWord;
			std::stringstream commentStr("");

			// Build string for value we'll actually be using to set/load/store.
			std::stringstream setLoadStoreString("");
			if (bapoAdd != UCHAR_MAX)
			{
				if (bapoAdd == 0)
				{
					setLoadStoreString << "ba + ";
				}
				else
				{
					setLoadStoreString << "po + ";
				}
			}
			setLoadStoreString << "0x" + addrWord;

			commentStr << codeTypeIn->name << " (";
			switch (valueSize)
			{
			case 0: { commentStr << "8-bit"; break; }
			case 1: { commentStr << "16-bit"; break; }
			case 2: { commentStr << "32-bit"; break; }
			default: { break; }
			}
			commentStr << "): ";

			// If in load mode
			if (loadStoreMode)
			{
				commentStr << "gr" << +targetGeckoRegister << "= Val @ $(" << setLoadStoreString.str() << ")";

				// And invalidate the register value cuz we can't know what it is after loading from RAM.
				invalidateGeckoRegister(targetGeckoRegister);
			}
			// Else, if we're in store mode
			else
			{
				commentStr << " Store ";
				switch (valueSize)
				{
				case 0: { commentStr << "8 bits of"; break; }
				case 1: { commentStr << "16 bits of"; break; }
				default: { break; }
				}
				commentStr << "gr" << +targetGeckoRegister << " @ $(" << setLoadStoreString.str() << ")";
				if (valueStoreCount > 0)
				{
					commentStr << " (move forward and repeat " << (valueStoreCount + 1) << " times)";
				}

				// And don't invalidate the register value cuz it hasn't been altered at all.
			}

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoRegisterArithCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string rightHandWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(rightHandWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long rightHandNum = lava::stringToNum<unsigned long>(rightHandWord, 0, ULONG_MAX, 1);

			// 1 if Immediate is to be used as a Gecko Register ID, 0 otherwise
			bool registerMode = codeTypeIn->secondaryCodeType == 0x8;
			// Determines math operation to be performed
			unsigned char operationType = (signatureNum & 0x00F00000) >> 0x14;
			// Determines whether or not we're operating on the register/immediate values themselves, or the values at the
			// addresses *pointed to* by them.
			bool destinationRegIsAddress = signatureNum & 0x00010000;
			bool rightHandIsAddress = signatureNum & 0x00020000;
			// Gecko Register to work with
			unsigned char targetGeckoRegister = signatureNum & 0xF;

			std::string outputStr = "* " + signatureWord + " " + rightHandWord;
			std::stringstream commentStr("");

			std::string destinationRegString = "gr" + lava::numToDecStringWithPadding(targetGeckoRegister, 0);
			if (destinationRegIsAddress)
			{
				destinationRegString = "Val @ $(" + destinationRegString + ")";
			}

			std::string rightHandString("");
			if (registerMode)
			{
				rightHandString = "gr" + lava::numToDecStringWithPadding(rightHandNum & 0xF, 0);
			}
			else
			{
				rightHandString = "0x" + rightHandWord;
			}
			if (rightHandIsAddress)
			{
				rightHandString = "Val @ $(" + rightHandString + ")";
			}

			commentStr << codeTypeIn->name << ": " << destinationRegString << " = " << destinationRegString << " ";
			switch (operationType)
			{
			case 0x0: { commentStr << "+"; break; }
			case 0x1: { commentStr << "*"; break; }
			case 0x2: { commentStr << "|"; break; }
			case 0x3: { commentStr << "&"; break; }
			case 0x4: { commentStr << "^"; break; }
			case 0x5: { commentStr << "<<"; break; }
			case 0x6: { commentStr << ">>"; break; }
			case 0x7: { commentStr << "Rot.Left"; break; }
			case 0x8: { commentStr << "Rot.Right.Arith"; break; }
			case 0x9: { commentStr << "Single.Float.Add"; break; }
			case 0xA: { commentStr << "Single.Float.Mull"; break; }
			default: { break; }
			}
			commentStr << " " << rightHandString;

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoMemoryCopyCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			unsigned short numBytesToCopy = (signatureNum & 0x00FFFF00) >> 0x8;
			bool immSideIndex = codeTypeIn->secondaryCodeType == 0xA;
			// 0 is left hand side, 1 is right hand side
			std::array<unsigned char, 2> leftRightHandReg = { (signatureNum & 0xF0) >> 0x4,  signatureNum & 0x0F };
			// 0 is left hand side, 1 is right hand side
			std::array<std::stringstream, 2> leftRightHandStr{};

			std::string outputStr = "* " + signatureWord + " " + immWord;
			std::stringstream commentStr("");
			commentStr << codeTypeIn->name << ": Copy 0x" << lava::numToHexStringWithPadding(numBytesToCopy, 0) << " byte(s) from ";

			leftRightHandStr[!immSideIndex] << "$(gr" << +leftRightHandReg[!immSideIndex] << ")";
			leftRightHandStr[immSideIndex] << "$(";
			if (leftRightHandReg[immSideIndex] == 0xF)
			{
				if ((signatureNum & signatureBaPoMask) == 0)
				{
					leftRightHandStr[immSideIndex] << "ba";
				}
				else
				{
					leftRightHandStr[immSideIndex] << "po";
				}
			}
			else
			{
				leftRightHandStr[immSideIndex] << "gr" << +leftRightHandReg[immSideIndex];
			}
			if (immNum)
			{
				leftRightHandStr[immSideIndex] << " + 0x" << lava::numToHexStringWithPadding(immNum, 8);
			}
			leftRightHandStr[immSideIndex] << ")";
			
			commentStr << leftRightHandStr[0].str() << " to " << leftRightHandStr[1].str();

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoRegisterIfCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			// 0 is left hand side, 1 is right hand side
			std::array<unsigned char, 2> leftRightHandReg = { (immNum & 0x0F000000) >> 0x18,  (immNum & 0xF0000000) >> 0x1C };
			// 9 is left hand side, 1 is right hand side
			std::array<std::stringstream, 2> leftRightHandStr{};
			// Value Mask
			unsigned short valueMask = immNum & 0xFFFF;

			// Prepare strings for each side.
			for (unsigned long i = 0; i < 2; i++)
			{
				leftRightHandStr[i] << "Val @ ";
				// If we're using BAPO for this reg...
				if (leftRightHandReg[i] == 0xF)
				{
					// ... use component string.
					leftRightHandStr[i] << getAddressComponentString(signatureNum) << " ";
				}
				// Otherwise...
				else
				{
					// ... just print geck reg.
					leftRightHandStr[i] << "$(gr" << +leftRightHandReg[i] << ") ";
				}
				// And apply value mask if necessary.
				if (valueMask != 0)
				{
					leftRightHandStr[i] << "& 0x" << lava::numToHexStringWithPadding<unsigned short>(~valueMask, 4) << " ";
				}
			}

			std::string outputStr = "* " + signatureWord + " " + immWord;
			std::stringstream commentStr("");
			commentStr << codeTypeIn->name;
			if (signatureNum & 1)
			{
				commentStr << withEndifString;
			}
			
			commentStr << ": " << leftRightHandStr[0].str() << " ";
			switch (codeTypeIn->secondaryCodeType % 8)
			{
			case 0: { commentStr << "=="; break; }
			case 2: { commentStr << "!="; break; }
			case 4: { commentStr << ">"; break; }
			case 6: { commentStr << "<"; break; }
			default: {break; }
			}
			commentStr << " " << leftRightHandStr[1].str();

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoCounterIfCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			bool resetCountIfTrue = signatureNum & 0x8;

			std::string outputStr = "* " + signatureWord + " " + immWord;

			std::stringstream commentStr("");
			commentStr << codeTypeIn->name;
			if (signatureNum & 1)
			{
				commentStr << withEndifString;
				signatureNum &= ~1;
			}
			commentStr << ": If 0x" << lava::numToHexStringWithPadding<unsigned short>(immNum & 0xFFFF, 4);
			if ((immNum >> 0x10) != 0)
			{
				commentStr << " & 0x" << lava::numToHexStringWithPadding<unsigned short>(~(immNum >> 0x10), 4);
			}
			commentStr << " ";

			switch (codeTypeIn->secondaryCodeType % 8)
			{
			case 0: { commentStr << "=="; break; }
			case 2: { commentStr << "!="; break; }
			case 4: { commentStr << ">"; break; }
			case 6: { commentStr << "<"; break; }
			default: {break; }
			}

			commentStr << " Counter, Execute";
			if (resetCountIfTrue)
			{
				commentStr << " (and Reset Counter to Zero)";
			}

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoASMOutputodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string lengthWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(lengthWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long lengthNum = lava::stringToNum<unsigned long>(lengthWord, 0, ULONG_MAX, 1);
			
			bool canDoGCTRMOutput = 0;
			unsigned char codeTypeHex = ((unsigned char)codeTypeIn->primaryCodeType << 4) | codeTypeIn->secondaryCodeType;
			std::string hexWord("");
			std::string conversion("");
			std::string outputString("");
			std::stringstream commentString("");

			// Handle codetype-unique output needs:
			switch (codeTypeHex)
			{
			case 0xC0:
			{
				canDoGCTRMOutput = 1;
				outputStreamIn << "PULSE\n";
				break;
			}
			case 0xC2:
			{
				unsigned long inferredHookAddress = getAddressFromCodeSignature(signatureNum);
				canDoGCTRMOutput = inferredHookAddress != ULONG_MAX;
				if (canDoGCTRMOutput)
				{
					outputString = "HOOK @ $" + lava::numToHexStringWithPadding(inferredHookAddress, 8);
					commentString << "Address = " << getAddressComponentString(signatureNum);
					lava::ppc::mapSymbol* parentSymbol = lava::ppc::getSymbolFromAddress(inferredHookAddress);
					if (parentSymbol != nullptr)
					{
						commentString << " [in \"" << parentSymbol->symbolName << "\" @ $" << lava::numToHexStringWithPadding(parentSymbol->virtualAddr, 8) << "]";
					}
					lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);
				}
				else
				{
					outputString = "* " + signatureWord + " " + lengthWord;
					commentString << codeTypeIn->name << " (" << lengthNum << " line(s)) @ " << getAddressComponentString(signatureNum) << ":";
					lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);
				}
				break;
			}
			case 0xF2:
			case 0xF4:
			{
				canDoGCTRMOutput = 0;
				// These codes actually embed extra information in what normally is the length parameter.
				// So, we need to get that info...
				unsigned short checksum = (lengthNum >> 8) & 0xFFFF;
				signed char valueCount = static_cast<unsigned char>(lengthNum >> 0x18);
				// Then zero out all but the bottom 8 bits of the number (for use in the code output for loop later).
				lengthNum &= 0xFF;
				// This codetype also accesses PO in a slightly weird way; it can't set the normal BaPo
				// bit to do it, so it instead sets the codetype to 0xF4. So, just for the sake of grabbing
				// the address string in a way consistent with everything else, we unset the bapo bit then
				// set it ourselves if the PO codetype is used.
				unsigned long adjustedSignatureNum = signatureNum & ~signatureBaPoMask;
				if (codeTypeHex == 0xF4)
				{
					adjustedSignatureNum |= signatureBaPoMask;
				}
				outputString = "* " + signatureWord + " " + lengthWord;
				commentString << codeTypeIn->name << ": If XORing the " << std::abs(valueCount) << " Half-Words";
				if (valueCount > 0)
				{
					commentString << " Following";
				}
				else
				{
					commentString << " Preceding";
				}
				commentString << " " << getAddressComponentString(adjustedSignatureNum) << " == 0x" << lava::numToHexStringWithPadding(checksum, 4) << ":";
				lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);
				break;
			}
			default:
			{
				break;
			}
			}

			// Handle rest of PPC output:
			if (canDoGCTRMOutput)
			{
				// Build PPC Hex Vector to pass to block parser.
				std::vector<unsigned long> hexVec{};
				unsigned long convertedHex = ULONG_MAX;
				unsigned long instructionsToConvert = lengthNum * 2;
				for (unsigned long i = 0; i < instructionsToConvert; i++)
				{
					lava::readNCharsFromStream(hexWord, codeStreamIn, 8, 0);
					hexVec.push_back(lava::stringToNum<unsigned long>(hexWord, 0, ULONG_MAX, 1));
				}

				std::vector<std::string> convertedHexVec = lava::ppc::convertInstructionHexBlockToStrings(hexVec, disallowedMnemonics, 1, doPPCHexEncodingComments);
				outputStreamIn << "{\n";
				// Do indented output, accounting for newline characters!
				for (unsigned long i = 0; i < convertedHexVec.size(); i++)
				{
					// Stringview of the converted line, which we'll be using to simplify moving past newline chars
					std::string_view conversionView = convertedHexVec[i];
					// Keeps track of the position of the next newline char on each iteration.
					std::size_t newlineLoc = SIZE_MAX;
					// We're gonna do the first print regardless, so we do-while
					do
					{
						// Get the location of the next newline char within the current view of the conversion string...
						newlineLoc = conversionView.find('\n');
						// ... and get the region before the newline.
						std::string_view segmentView = conversionView.substr(0, newlineLoc);
						// Only do indentation if we aren't looking at a branch destination tag segment!
						// Literally: if there's a colon in the line, and it doesn't come after a '#' (ie. isn't in a comment), skip indentation
						if (!(segmentView.find(':') < segmentView.find('#')))
						{
							outputStreamIn << "\t";
						}
						// Output the segment that comes before the newline (or the whole thing, if there isn't one).
						outputStreamIn << segmentView << "\n";
						// And push our stringview forward, past the last found newline, to move further iterations forwards.
						conversionView.remove_prefix(newlineLoc + 1);
					} while (newlineLoc != std::string::npos); // And repeat, for as long the previous iteration found a newline!
				}
				outputStreamIn << "}\n";
			}
			else
			{
				for (unsigned long i = 0; i < lengthNum; i++)
				{
					commentString.str("");

					lava::readNCharsFromStream(hexWord, codeStreamIn, 8, 0);
					outputString = "* " + hexWord + " ";
					commentString << "\t";
					conversion = convertPPCInstructionHex(hexWord, 0);
					commentString << std::left << std::setw(0x18);
					if (!conversion.empty())
					{
						commentString << conversion;
					}
					else
					{
						commentString << "---";
					}

					lava::readNCharsFromStream(hexWord, codeStreamIn, 8, 0);
					outputString += hexWord;
					commentString << ", ";
					conversion = convertPPCInstructionHex(hexWord, 0);
					if (!conversion.empty())
					{
						commentString << conversion;
					}
					else
					{
						commentString << "---";
					}

					lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);
				}
			}
			

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoC6CodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			// Initialize Strings For Output
			std::string outputString("");
			std::stringstream commentString("");

			// Output First Line
			outputString = "* " + signatureWord + " " + immWord;
			commentString << codeTypeIn->name << " @ " << getAddressComponentString(signatureNum) << ": b 0x" << lava::numToHexStringWithPadding(immNum, 8);
			
			lava::gecko::printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoCECodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			std::string outputStr = "* " + signatureWord + " " + immWord;
			std::stringstream commentStr("");
			commentStr << codeTypeIn->name;
			if (signatureNum & 1)
			{
				commentStr << withEndifString;
			}
			commentStr << ": If 0x" << lava::numToHexStringWithPadding(immNum & 0xFFFF0000, 0x08) << " <= ";
			// If we're checking BA
			if ((signatureNum & signatureBaPoMask) == 0)
			{
				commentStr << "ba";
			}
			else
			{
				commentStr << "po";
			}
			commentStr << " < " << lava::numToHexStringWithPadding(immNum << 0x10, 0x08);

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoE0CodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			std::string outputStr = "* " + signatureWord + " " + immWord;
			std::stringstream commentStr("");
			commentStr << codeTypeIn->name << ": ";
			if (immNum & 0xFFFF0000)
			{
				currentBAValue = immNum & 0xFFFF0000;
				commentStr << "ba = 0x" << lava::numToHexStringWithPadding(currentBAValue, 8);
			}
			else
			{
				commentStr << "ba unchanged";
			}
			commentStr << ", ";
			if (immNum & 0xFFFF)
			{
				currentPOValue = (immNum << 0x10);
				commentStr << "po = 0x" << lava::numToHexStringWithPadding(currentPOValue, 8);
			}
			else
			{
				commentStr << "po unchanged";
			}

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoE2CodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			std::string outputStr = "* " + signatureWord + " " + immWord;
			std::stringstream commentStr("");
			commentStr << codeTypeIn->name;
			if (signatureNum & 0x00F00000)
			{
				commentStr << " (Invert Exec Status)";
			}
			commentStr << ": Apply " << (signatureNum & 0xFF) << " Endifs, ";

			if (immNum & 0xFFFF0000)
			{
				currentBAValue = immNum & 0xFFFF0000;
				commentStr << "ba = 0x" << lava::numToHexStringWithPadding(currentBAValue, 8);
			}
			else
			{
				commentStr << "ba unchanged";
			}
			commentStr << ", ";
			if (immNum & 0xFFFF)
			{
				currentPOValue = (immNum << 0x10);
				commentStr << "po = 0x" << lava::numToHexStringWithPadding(currentPOValue, 8);
			}
			else
			{
				commentStr << "po unchanged";
			}

			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoF6CodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			// Number of lines to search for
			unsigned char lineCount = signatureNum & 0xFF;
			unsigned long searchRegionStart = immNum & 0xFFFF0000;
			if (searchRegionStart == 0x80000000)
			{
				searchRegionStart =  0x80003000;
			}
			unsigned long searchRegionEnd = immNum << 0x10;

			// Print first line:
			std::string outputStr = "* " + signatureWord + " " + immWord;
			std::stringstream commentStr("");
			commentStr << codeTypeIn->name << ": Search for Following " << +lineCount << " line(s) between " <<
				"$" << lava::numToHexStringWithPadding(searchRegionStart, 8) << " and "
				"$" << lava::numToHexStringWithPadding(searchRegionEnd, 8);
			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			// Dump remaining lines:
			dumpUnannotatedHexToStream(codeStreamIn, outputStreamIn, lineCount, "\tSearch Criteria:");

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoNameOnlyCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
	{
		std::size_t result = SIZE_MAX;

		if (codeStreamIn.good() && outputStreamIn.good())
		{
			std::streampos initialPos = codeStreamIn.tellg();

			std::string signatureWord("");
			std::string immWord("");

			lava::readNCharsFromStream(signatureWord, codeStreamIn, 8, 0);
			lava::readNCharsFromStream(immWord, codeStreamIn, 8, 0);

			unsigned long signatureNum = lava::stringToNum<unsigned long>(signatureWord, 0, ULONG_MAX, 1);
			unsigned long immNum = lava::stringToNum<unsigned long>(immWord, 0, ULONG_MAX, 1);

			std::string outputStr = "* " + signatureWord + " " + immWord;
			std::stringstream commentStr("");
			commentStr << codeTypeIn->name;
			
			lava::gecko::printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}

	// Code Type Group
	unsigned long codetypeCount = 0x00;
	geckoCodeType* geckoPrTypeGroup::pushInstruction(std::string nameIn, unsigned char secOpIn, std::size_t(*conversionFuncIn)(geckoCodeType*, std::istream&, std::ostream&))
	{
		geckoCodeType* result = nullptr;

		if (secondaryCodeTypesToCodes.find(secOpIn) == secondaryCodeTypesToCodes.end())
		{
			result = &secondaryCodeTypesToCodes[secOpIn];
			result->name = nameIn;
			result->primaryCodeType = primaryCodeType;
			result->secondaryCodeType = secOpIn;
			result->conversionFunc = conversionFuncIn;
			codetypeCount++;
		}

		return result;
	}

	// Dictionary
	std::map<unsigned char, geckoPrTypeGroup> geckoCodeDictionary{};
	geckoPrTypeGroup* pushPrTypeGroupToDict(geckoPrimaryCodeTypes codeTypeIn)
	{
		geckoPrTypeGroup* result = nullptr;

		if (geckoCodeDictionary.find((unsigned char)codeTypeIn) == geckoCodeDictionary.end())
		{
			result = &geckoCodeDictionary[(unsigned char)codeTypeIn];
			result->primaryCodeType = codeTypeIn;
		}

		return result;
	}
	void buildGeckoCodeDictionary()
	{
		geckoPrTypeGroup* currentCodeTypeGroup = nullptr;
		geckoCodeType* currentCodeType = nullptr;

		geckoRegisters.fill(ULONG_MAX);

		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_RAMWrite);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("8-Bit Write", 0x0,  geckoBasicRAMWriteCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("16-Bit Write", 0x2, geckoBasicRAMWriteCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("32-Bit Write", 0x4, geckoBasicRAMWriteCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("String Write", 0x6, gecko06CodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Serial Write", 0x8, gecko08CodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_If);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("32-Bit If Equal", 0x0, geckoCompareCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("32-Bit If Not Equal", 0x2, geckoCompareCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("32-Bit If Greater", 0x4, geckoCompareCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("32-Bit If Lesser", 0x6, geckoCompareCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("16-Bit If Equal", 0x8, geckoCompareCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("16-Bit If Not Equal", 0xA, geckoCompareCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("16-Bit If Greater", 0xC, geckoCompareCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("16-Bit If Lesser", 0xE, geckoCompareCodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_BaseAddr);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("Load Base Address", 0x0, geckoSetLoadStoreAddressCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Set Base Address", 0x2, geckoSetLoadStoreAddressCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Store Base Address", 0x4, geckoSetLoadStoreAddressCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Put Next Code Loc in BA", 0x6, gecko464ECodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Load Pointer Offset", 0x8, geckoSetLoadStoreAddressCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Set Pointer Offset", 0xA, geckoSetLoadStoreAddressCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Store Pointer Offset", 0xC, geckoSetLoadStoreAddressCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Put Next Code Loc in PO", 0xE, gecko464ECodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_FlowControl);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("Set Repeat", 0x0, geckoSetRepeatConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Execute Repeat", 0x2, geckoExecuteRepeatConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Return", 0x4, geckoReturnConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Goto", 0x6, geckoGotoGosubConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Gosub", 0x8, geckoGotoGosubConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_GeckoReg);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("Set Gecko Register", 0x0, geckoSetGeckoRegCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Load Gecko Register", 0x2, geckoLoadStoreGeckoRegCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Store Gecko Register", 0x4, geckoLoadStoreGeckoRegCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Gecko Reg Arith (Immediate)", 0x6, geckoRegisterArithCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Gecko Reg Arith", 0x8, geckoRegisterArithCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Memory Copy 1", 0xA, geckoMemoryCopyCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Memory Copy 2", 0xC, geckoMemoryCopyCodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_RegAndCounterIf);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("Gecko Reg 16-Bit If Equal", 0x0, geckoRegisterIfCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Gecko Reg 16-Bit If Not Equal", 0x2, geckoRegisterIfCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Gecko Reg 16-Bit If Greater", 0x4, geckoRegisterIfCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Gecko Reg 16-Bit If Lesser", 0x6, geckoRegisterIfCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Counter 16-Bit If Equal", 0x8, geckoCounterIfCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Counter 16-Bit If Not Equal", 0xA, geckoCounterIfCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Counter 16-Bit If Greater", 0xC, geckoCounterIfCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Counter 16-Bit If Lesser", 0xE, geckoCounterIfCodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_Assembly);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("Execute ASM", 0x0, geckoASMOutputodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Insert ASM", 0x2, geckoASMOutputodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Create Branch", 0x6, geckoC6CodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("On/Off Switch", 0xC, geckoNameOnlyCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Address Range Check", 0xE, geckoCECodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_Misc);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("Full Terminator", 0x0, geckoE0CodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Endif", 0x2, geckoE2CodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_EndOfCodes);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("End of Codes", 0x0, geckoNameOnlyCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Insert ASM w/ Checksum", 0x2, geckoASMOutputodeConv);
			// PO variant of above codetype, since it can't use the normal PO bit to do that.
			currentCodeType = currentCodeTypeGroup->pushInstruction("Insert ASM w/ Checksum", 0x4, geckoASMOutputodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("If Search, Set Pointer", 0x6, geckoF6CodeConv);
		}

		return;
	}

	
	geckoCodeType* findRelevantGeckoCodeType(unsigned char primaryType, unsigned char secondaryType)
	{
		geckoCodeType* result = nullptr;

		auto codeGroupSearchRes = geckoCodeDictionary.find(primaryType);
		if (codeGroupSearchRes != geckoCodeDictionary.end())
		{
			geckoPrTypeGroup* targetGroup = &codeGroupSearchRes->second;
			if (targetGroup->secondaryCodeTypesToCodes.find(secondaryType) != targetGroup->secondaryCodeTypesToCodes.end())
			{
				result = &targetGroup->secondaryCodeTypesToCodes[secondaryType];
			}
		}

		return result;
	}
	std::size_t parseGeckoCode(std::ostream& output, std::istream& codeStreamIn, std::size_t expectedLength, bool resetDynamicValues, bool resetTrackingValues, bool doPPCHexComments)
	{
		std::size_t result = SIZE_MAX;

		if (output.good() && codeStreamIn.good())
		{
			result = 0;

			unsigned char currCodeType = UCHAR_MAX;
			unsigned char currCodePrType = UCHAR_MAX;
			unsigned char currCodeScType = UCHAR_MAX;

			if (resetDynamicValues)
			{
				resetParserDynamicValues(0);
			}
			if (resetTrackingValues)
			{
				resetParserTrackingValues();
			}
			doPPCHexEncodingComments = doPPCHexComments;

			std::string codeTypeStr("");
			geckoCodeType* targetedGeckoCodeType = nullptr;
			while (codeStreamIn.good() && result < expectedLength)
			{
				if (detectedDataEmbedLineCount > 0x00)
				{
					result += dumpUnannotatedHexToStream(codeStreamIn, output, detectedDataEmbedLineCount, 
						"DATA_EMBED (0x" + lava::numToHexStringWithPadding(detectedDataEmbedLineCount * 0x8, 0) + " bytes)");
					detectedDataEmbedLineCount = 0;
					continue;
				}
				removeExpiredEmbedSuspectLocations(codeStreamIn.tellg());
				if (suspectedEmbedLocations.empty())
				{
					didBAPOStoreToAddressWhileSuspectingEmbed = 0;
				}
				removeExpiredGotoEndLocations(codeStreamIn.tellg());

				lava::readNCharsFromStream(codeTypeStr, codeStreamIn, 2, 1);

				currCodeType = lava::stringToNum<unsigned char>(codeTypeStr, 1, UCHAR_MAX, 1);
				if (currCodeType < 0xF0)
				{
					currCodePrType = (currCodeType & 0b11100000) >> 4;	// First hex digit (minus bit 4) is primary code type.
					currCodeScType = (currCodeType & 0b00001110);		// Second hex digit (minus bit 8) is secondary code type.
				}
				else
				{
					currCodePrType = (currCodeType & 0b11110000) >> 4;	// First hex digit (including bit 4, 0xF is special) is primary code type.
					currCodeScType = (currCodeType & 0b00001110);		// Second hex digit (minus bit 8) is secondary code type.
				}

				targetedGeckoCodeType = findRelevantGeckoCodeType(currCodePrType, currCodeScType);
				if (targetedGeckoCodeType != nullptr)
				{
					result += targetedGeckoCodeType->conversionFunc(targetedGeckoCodeType, codeStreamIn, output);
				}
				else
				{
					result += dumpUnannotatedHexToStream(codeStreamIn, output, (expectedLength - result) / 0x10,
						"UNRECOGNIZED CODETYPE, Parsing Aborted");
				}
			}
		}

		return result;
	}
}