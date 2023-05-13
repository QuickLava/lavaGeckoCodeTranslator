#include "stdafx.h"
#include "_lavaGeckoHexConvert.h"

namespace lava::gecko
{
	// Constants
	constexpr unsigned long signatureBaPoMask = 0x10000000;
	constexpr unsigned long signatureAddressMask = 0x1FFFFFF;
	constexpr unsigned long signatureAddressBase = 0x80000000;
	const std::string withEndifString = " (With Endif)";

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
	bool validateCurrentPOValue()
	{
		return currentPOValue != ULONG_MAX;
	}
	void invalidateCurrentPOValue()
	{
		currentPOValue = ULONG_MAX;
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

		unsigned char executionCondition = (codeSignatureIn & 0x000F0000) >> 0x10;
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
		unsigned long originalFlags = outputStream.flags();
		outputStream << std::left << std::setw(relativeCommentLoc) << primaryString << "# " << commentString;
		if (printNewLine)
		{
			outputStream << "\n";
		}
		outputStream.setf(originalFlags);
	}
	std::string getStringWithComment(const std::string& primaryString, const std::string& commentString, unsigned long relativeCommentLoc = 0x20)
	{
		static std::stringstream result("");
		result.str("");
		printStringWithComment(result, primaryString, commentString, 0, relativeCommentLoc);
		return result.str();
	}

	std::string convertPPCInstructionHex(unsigned long hexIn, bool enableComment)
	{
		std::string result = "";

		result = lava::ppc::convertInstructionHexToString(hexIn);
		if (enableComment && !result.empty())
		{
			result = getStringWithComment(result, "0x" + lava::numToHexStringWithPadding(hexIn, 8));
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
			printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

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
			printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

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
						commentString << "...";
					}
					for (unsigned long u = 0; (u < 8) && ((u + cursor) < immNum); u++)
					{
						currCharacter = bytesToWrite[u + cursor];
						if ((stringCurrentlyOpen && currCharacter == 0x00) || (!stringCurrentlyOpen && currCharacter != 0x00))
						{
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
								commentString << "...";
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
				printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

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
			printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			outputString = "* " + settingsWord + " " + valIncrWord;
			commentString.str("");
			commentString << "\tDo " << (numWrites + 1) << " write(s)";
			commentString << ", Increment Addr by 0x" << lava::numToHexStringWithPadding(addressIncrValue, 4);
			commentString << ", Increment Value by 0x" << lava::numToHexStringWithPadding(valIncrNum, 8);
			printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

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

			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

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
			printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
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
			printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

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
			printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

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
			printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
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
			else if (setLoadStoreMode == 6)
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

			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
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

			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

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

			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

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

			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

			result = codeStreamIn.tellg() - initialPos;
		}

		return result;
	}
	std::size_t geckoC2CodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn)
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
			
			unsigned long inferredHookAddress = getAddressFromCodeSignature(signatureNum);
			bool canDoGCTRMOutput = inferredHookAddress != ULONG_MAX;

			std::string hexWord("");
			std::string conversion("");
			std::string outputString("");
			std::stringstream commentString("");
			if (canDoGCTRMOutput)
			{
				outputString = "HOOK @ $" + lava::numToHexStringWithPadding(inferredHookAddress, 8);
				commentString << "Address = " << getAddressComponentString(signatureNum);
				printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);
				outputStreamIn << "{\n";
				for (unsigned long i = 0; i < lengthNum * 2; i++)
				{
					lava::readNCharsFromStream(hexWord, codeStreamIn, 8, 0);
					conversion = convertPPCInstructionHex(hexWord, 1);
					if (!conversion.empty())
					{
						outputStreamIn << "\t" << conversion << "\n";
					}
				}
				outputStreamIn << "}\n";
			}
			else
			{
				outputString = "* " + signatureWord + " " + lengthWord;
				commentString << codeTypeIn->name << " (" << lengthNum << " line(s)) @ " << getAddressComponentString(signatureNum) << ":";
				printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);
				for (unsigned long i = 0; i < lengthNum; i++)
				{
					commentString.str("");

					lava::readNCharsFromStream(hexWord, codeStreamIn, 8, 0);
					outputString = "* " + hexWord + " ";
					commentString << "\t";
					conversion = convertPPCInstructionHex(hexWord, 0);
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

					printStringWithComment(outputStreamIn, outputString, commentString.str(), 1);
				}
			}
			

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

			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

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

			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

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

			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

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
			
			printStringWithComment(outputStreamIn, outputStr, commentStr.str(), 1);

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
			currentCodeType = currentCodeTypeGroup->pushInstruction("Load Pointer Offset", 0x8, geckoSetLoadStoreAddressCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Set Pointer Offset", 0xA, geckoSetLoadStoreAddressCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Store Pointer Offset", 0xC, geckoSetLoadStoreAddressCodeConv);
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
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_Assembly);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("Insert ASM", 0x2, geckoC2CodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("On/Off Switch", 0xC, geckoNameOnlyCodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Address Range Check", 0xE, geckoCECodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_Misc);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("Full Terminator", 0, geckoE0CodeConv);
			currentCodeType = currentCodeTypeGroup->pushInstruction("Endif", 2, geckoE2CodeConv);
		}
		currentCodeTypeGroup = pushPrTypeGroupToDict(geckoPrimaryCodeTypes::gPCT_EndOfCodes);
		{
			currentCodeType = currentCodeTypeGroup->pushInstruction("End of Codes", 0, geckoNameOnlyCodeConv);
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
	std::size_t parseGeckoCode(std::ostream& output, std::istream& codeStreamIn, std::size_t expectedLength)
	{
		std::size_t result = SIZE_MAX;

		if (output.good() && codeStreamIn.good())
		{
			result = 0;

			unsigned char currCodeType = UCHAR_MAX;
			unsigned char currCodePrType = UCHAR_MAX;
			unsigned char currCodeScType = UCHAR_MAX;

			std::string codeTypeStr("");
			geckoCodeType* targetedGeckoCodeType = nullptr;
			while (result < expectedLength)
			{
				lava::readNCharsFromStream(codeTypeStr, codeStreamIn, 2, 1);

				currCodeType = lava::stringToNum<unsigned long>(codeTypeStr, 1, UCHAR_MAX, 1);
				currCodePrType = (currCodeType & 0b11100000) >> 4;	// First hex digit (minus bit 4) is primary code type.
				currCodeScType = (currCodeType & 0b00001110);		// Second hex digit (minus bit 8) is secondary code type.

				targetedGeckoCodeType = findRelevantGeckoCodeType(currCodePrType, currCodeScType);
				if (targetedGeckoCodeType != nullptr)
				{
					result += targetedGeckoCodeType->conversionFunc(targetedGeckoCodeType, codeStreamIn, output);
				}
				else
				{
					bool startingNewLine = 1;
					std::string dumpStr = "";
					dumpStr.reserve(8);
					while (result < expectedLength)
					{
						if (startingNewLine)
						{
							output << "*";
						}
						lava::readNCharsFromStream(dumpStr, codeStreamIn, 0x8, 0);
						output << " " << dumpStr;
						if (!startingNewLine)
						{
							output << "\n";
						}
						startingNewLine = !startingNewLine;
						result += 0x8;
					}
				}
			}
		}

		return result;
	}
}