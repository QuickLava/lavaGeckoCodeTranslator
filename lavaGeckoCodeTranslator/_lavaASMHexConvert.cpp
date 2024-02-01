#include "stdafx.h"
#include "_lavaASMHexConvert.h"
#include "_lavaBytes.h"

namespace lava::ppc
{
	constexpr unsigned char isSecOpArgFlag = 0b10000000;
	const std::string opName_WithOverflowString = " w/ Overflow";
	const std::string opName_WithUpdateString = " w/ Update";
	const std::string opName_IndexedString = " Indexed";
	const std::string opName_WithUpdateIndexedString = opName_WithUpdateString + opName_IndexedString;
	const std::string opName_WithComplString = " w/ Complement";
	const std::string opName_DoublePrecision = " (Double-Precision)";
	const std::string opName_SinglePrecision = " Single";

	// Utility
	void printStringWithComment(std::ostream& outputStream, const std::string& primaryString, const std::string& commentString, bool printNewLine, unsigned long relativeCommentLoc, unsigned long commentIndentationLevel)
	{
		unsigned long originalFlags = outputStream.flags();
		if (!commentString.empty())
		{
			outputStream << std::left << std::setw(relativeCommentLoc) << primaryString <<
				((primaryString.find('#') == std::string::npos) ? "# " : "| ") << std::string(commentIndentationLevel, '\t') << commentString;
		}
		else
		{
			outputStream << primaryString;
		}
		if (printNewLine)
		{
			outputStream << "\n";
		}
		outputStream.setf(originalFlags);
	}
	std::string getStringWithComment(const std::string& primaryString, const std::string& commentString, unsigned long relativeCommentLoc, unsigned long commentIndentationLevel)
	{
		static std::stringstream result("");
		result.str("");
		printStringWithComment(result, primaryString, commentString, 0, relativeCommentLoc, commentIndentationLevel);
		return result.str();
	}
	unsigned long extractInstructionArg(unsigned long hexIn, unsigned char startBitIndex, unsigned char length)
	{
		unsigned long result = ULONG_MAX;

		if (startBitIndex < 32)
		{
			if ((startBitIndex + length) > 32)
			{
				length = 32 - startBitIndex;
			}

			result = hexIn & ((unsigned long long(2) << (31 - startBitIndex)) - 1);
			result = result >> ((32 - startBitIndex) - length);
		}

		return result;
	}
	unsigned long getInstructionOpCode(unsigned long hexIn)
	{
		return extractInstructionArg(hexIn, 0, 6);
	}
	std::vector<std::string> formatRawDataEmbedOutput(const std::vector<unsigned long>& hexVecIn, std::string linePrefixIn, std::string wordPrefixIn, unsigned char wordsPerLineIn, unsigned long relativeLabelLocIn, std::string labelStringIn)
	{
		// Ensure wordsPerLine is at least 1.
		wordsPerLineIn = (wordsPerLineIn > 0) ? wordsPerLineIn : 1;

		// Initialize and pre-size our string vec.
		std::vector<std::string> result{};
		result.reserve(hexVecIn.size() / wordsPerLineIn);

		// Array for splitting the bytes of incoming words.
		std::array<unsigned char, 4> hexWordByteBuff{};

		// Strings for building our main and comment lines
		std::stringstream currentLine("");
		currentLine << linePrefixIn;
		std::stringstream commentLine("");

		// Counter for tracking when to move to new line.
		unsigned char wordCounter = wordsPerLineIn;

		// For each hex word...
		for (std::size_t i = 0; i < hexVecIn.size(); i++)
		{
			// ... write its string representation to the line buffer.
			currentLine << wordPrefixIn << lava::numToHexStringWithPadding(hexVecIn[i], 8);

			// Then, get the bytes in the word...
			lava::writeFundamentalToBuffer(hexVecIn[i], &hexWordByteBuff[0]);
			// ... and for each byte...
			for (unsigned char hexWordByte : hexWordByteBuff)
			{
				// ... write its native representation to the comment buffer if it's a printable character, and '.' otherwise.
				if (std::isprint(hexWordByte))
				{
					commentLine << hexWordByte;
				}
				else
				{
					commentLine << ".";
				}
			}

			// Decrement word counter...
			wordCounter--;
			// ... and if we've either written the requested number of words to this line, or we're at the end of the embed...
			if (wordCounter == 0 || (i == (hexVecIn.size() - 1)))
			{
				// ... push our finished line to the results vector...
				result.push_back(lava::ppc::getStringWithComment(currentLine.str(), commentLine.str()));

				// ... and reset our per-line variables.
				wordCounter = wordsPerLineIn;
				currentLine.str("");
				currentLine << linePrefixIn;
				commentLine.str("");
			}
		}
		// And lastly, on the first line of the embed, write in a comment labeling the EMBED itself.
		result.front() = lava::ppc::getStringWithComment(result.front(), labelStringIn, relativeLabelLocIn);

		return result;
	}

	std::string unsignedImmArgToSignedString(unsigned long argIn, unsigned char argLengthInBitsIn, bool hexMode = 1)
	{
		std::stringstream result;

		unsigned long adjustedArg = argIn;

		unsigned long signMask = 1 << (argLengthInBitsIn - 1);
		bool signBit = argIn & signMask;
		if (signBit)
		{
			result << "-";
			adjustedArg = (~argIn & (signMask - 1)) + 1;
		}
		if (hexMode)
		{
			result << std::hex << "0x";
		}
		result << adjustedArg;

		return result.str();
	}
	std::string parseBOAndBIToBranchMnem(unsigned char BOIn, unsigned char BIIn, std::string suffixIn)
	{
		std::stringstream result;
		
		char crField = BIIn >> 2;
		char crBit = BIIn & 0b1111;

		if (crField == 0)
		{
			// If decrementing CTR is disabled, parse for the full set of simplified mnemonics.
			if (BOIn & 0b00100)
			{
				// This is the always branch condition, mostly used for blr and bctr
				if (BOIn & 0b10000)
				{
					result << "b";
				}
				else
				{
					// This is the CR Bit True condition
					if (BOIn & 0b01000)
					{
						switch (crBit)
						{
						case 0:
						{
							result << "blt";
							break;
						}
						case 1:
						{
							result << "bgt";
							break;
						}
						case 2:
						{
							result << "beq";
							break;
						}
						case 3:
						{
							result << "bso";
							break;
						}
						default:
						{
							break;
						}
						}
					}
					// This is the CR Bit False condition
					else
					{
						switch (crBit)
						{
						case 0:
						{
							result << "bge";
							break;
						}
						case 1:
						{
							result << "ble";
							break;
						}
						case 2:
						{
							result << "bne";
							break;
						}
						case 3:
						{
							result << "bns";
							break;
						}
						default:
						{
							break;
						}
						}

					}
				}
			}
			// If it isn't disabled though...
			else
			{
				// ... then we can only offer a mnemonic for the always branch case.
				if (BOIn & 0b10000)
				{
					// Assign mnem based on == 0 bit (0b00010)
					if (BOIn & 0b00010) { result << "bdz"; }
					else { result << "bdnz"; }
				}
			}

			// If we've written to result (if a mnemonic was found)...
			if (result.tellp() != 0)
			{
				// ... attach the provided extra suffix to the end.
				result << suffixIn;
				// Additionally, check y-bit of BO; if set...
				if ((BOIn & 0b1) != 0)
				{
					// ... append positive branch prediction mark.
					result << "+";
				}
			}
		}

		return result.str();
	}
	unsigned long maskBetweenBitsInclusive(unsigned char maskStartBit, unsigned char maskEndBit, bool allowWrapAround)
	{
		unsigned long result = 0;

		if (maskStartBit < 32 && maskEndBit < 32)
		{
			bool invertMask = maskStartBit > maskEndBit;
			unsigned long maskComp1 = (unsigned long long(1) << (32 - maskStartBit)) - 1;
			unsigned long maskComp2 = ~((unsigned long long(1) << (31 - maskEndBit)) - 1);
			if (invertMask && allowWrapAround)
			{
				result = maskComp1 | maskComp2;
			}
			else
			{
				result = maskComp1 & maskComp2;
			}
		}

		return result;
	}
	std::string getMaskFromMBMESH(unsigned char MBIn, unsigned char MEIn, unsigned char SHIn)
	{
		std::stringstream result;

		unsigned long finalMask = maskBetweenBitsInclusive(MBIn, MEIn, 1);
		finalMask = (finalMask >> SHIn) | (finalMask << (32 - SHIn));
		result << std::hex << finalMask << std::dec;
		std::string maskStr = result.str();
		maskStr = std::string(8 - maskStr.size(), '0') + maskStr;

		return maskStr;
	}
	std::string getTOEncodingString(unsigned char TOIn)
	{
		std::string result = "";

		switch (TOIn)
		{
		case 16: {result = "lt"; break; } // Lesser
		case 20: {result = "le"; break; } // Lesser or Equal
		case 8: {result = "gt"; break; } // Greater
		case 12: {result = "ge"; break; } // Greater or Equal
		case 2: {result = "llt"; break; } // Logically Lesser
		case 6: {result = "lle"; break; } // Logically Lesser or Equal
		case 1: {result = "lgt"; break; } // Logically Greater
		case 5: {result = "lge"; break; } // Logically Greater or Equal
		case 4: {result = "eq"; break; } // Equal
		case 24: {result = "ne"; break; } // Not Equal
		case 31: {result = "-"; break; } // Unconditional
		default: {break; }
		}

		return result;
	}

	struct branchEventSummary
	{
		enum destinationLabelCondition
		{
			dlc_THRESHOLD = 0,
			dlc_DATA_EMBED,
			dlc_END_BRANCH,
		};

		bool outgoingBranchIsUnconditional = 0;
		bool outgoingBranchGoesBackwards = 0;
		bool outgoingBranchSetsLinkRegister = 0;
		std::size_t outgoingBranchDestIndex = SIZE_MAX;
		std::set<std::size_t> incomingBranchStartIndices{};
		destinationLabelCondition labelCondition = destinationLabelCondition::dlc_THRESHOLD;
	};
	struct
	{
	public:
		bool blockIsGeckoCompliant = 0;
		bool disableDataEmbedOutput = 0;
		std::vector<asmInstruction*> instructionPtrVec{};
		std::map<std::size_t, branchEventSummary> relativeBranchEventsMap{};
		std::map<std::size_t, std::size_t> dataEmbedStartsToLengths{};

	private:
		void populateInstructionPtrVec(const std::vector<unsigned long>& hexVecIn)
		{
			instructionPtrVec.resize(hexVecIn.size(), nullptr);
			for (std::size_t i = 0; i < hexVecIn.size(); i++)
			{
				instructionPtrVec[i] = getInstructionPtrFromHex(hexVecIn[i]);
			}
		}
		void populateBranchEventMap(const std::vector<unsigned long>& hexVecIn)
		{
			relativeBranchEventsMap.clear();
			for (std::size_t i = 0; i < hexVecIn.size(); i++)
			{
				const lava::ppc::asmInstruction* instructionPtr = instructionPtrVec[i];

				// If this instruction's instructionPtr was null...
				if (instructionPtr == nullptr) continue;
				// ... or doesnt' belong to a branch condition, skip to the next instruction.
				if (!(instructionPtr->primaryOpCode == asmPrimaryOpCodes::aPOC_B) && !(instructionPtr->primaryOpCode == asmPrimaryOpCodes::aPOC_BC)) continue;

				const lava::ppc::argumentLayout* instructionArgLayoutPtr = instructionPtr->getArgLayoutPtr();
				std::vector<unsigned long> instructionArgs = instructionArgLayoutPtr->splitHexIntoArguments(hexVecIn[i]);

				// Immediate Arg Index is 1 for B instructions, 3 for BC instructions. AA bit is always this plus 1, LK is this plus 2.
				unsigned char immArgIndex = (instructionPtr->primaryOpCode == asmPrimaryOpCodes::aPOC_B) ? 1 : 3;

				// If the AA bit is set, we can skip to the next instruction.
				if (instructionArgs[immArgIndex + 1]) continue;

				// Convert the immediate arg to num...
				signed long branchDistance = lava::stringToNum(
					unsignedImmArgToSignedString(instructionArgs[immArgIndex] << 2, instructionArgLayoutPtr->getArgLengthInBits(immArgIndex), 0),
					1, LONG_MAX, 0);

				// ... and if the result was valid and non-zero...
				if (branchDistance != LONG_MAX && branchDistance != 0)
				{
					// ... divide the distance by 4, to see how many instructions forwards or backwards we'd be travelling.
					branchDistance /= 0x4;

					// Use that to determine the index of the instruction index we'd end up at relative to this branch instruction...
					signed long destinationInstructionIndex = signed long(i) + branchDistance;
					// ... and if that maps to another instruction within this block...
					if ((destinationInstructionIndex > 0) && (destinationInstructionIndex < hexVecIn.size()))
					{
						// ... set the appropriate values in our current entry.
						relativeBranchEventsMap[i].outgoingBranchGoesBackwards = i > destinationInstructionIndex;
						relativeBranchEventsMap[i].outgoingBranchDestIndex = destinationInstructionIndex;
						relativeBranchEventsMap[i].outgoingBranchSetsLinkRegister = instructionArgs[immArgIndex + 2];
						if (instructionPtr->primaryOpCode == asmPrimaryOpCodes::aPOC_B)
						{
							// B instructions are by definition unconditioned.
							relativeBranchEventsMap[i].outgoingBranchIsUnconditional = 1;
						}
						else
						{
							// ... and BC instructions are unconditioned if BO sets the unconditional branch bits.
							relativeBranchEventsMap[i].outgoingBranchIsUnconditional = (instructionArgs[1] & 0b10100) == 0b10100;
						}

						// And add it to the list of incoming branches in the destination entry.
						relativeBranchEventsMap[destinationInstructionIndex].incomingBranchStartIndices.insert(i);
					}
				}
			}
			// Lastly though, if we actually catalogued some branch events, and our block is Gecko-compliant (ie. last word is 0x00, and aligned to 8 bytes)...
			if (!relativeBranchEventsMap.empty() && blockIsGeckoCompliant)
			{
				// ... and the final branch event we recorded was a destination event for the very last instruction in that block (ie. the 0x00 word)...
				auto finalEvent = relativeBranchEventsMap.rbegin();
				if (finalEvent->first == (hexVecIn.size() - 1) && !finalEvent->second.incomingBranchStartIndices.empty())
				{
					// ... mark it as a end branch, so we can do our %END% output later! 
					finalEvent->second.labelCondition = branchEventSummary::dlc_END_BRANCH;
				}
			}
		}
		void populateDataEmbedMap(const std::vector<unsigned long>& hexVecIn)
		{
			// Clear the map first to start.
			dataEmbedStartsToLengths.clear();

			// Before we look for embeds, we need to see if the block uses *both* a BL/BCL and a BCLR instruction.
			// If so, we can't confidently guarantee that unreached regions are *actually* unreachable, since
			// it's possible that we'll non-relatively branch back to the region skipped by the BL/BCL call!
			bool foundBL = 0;
			bool foundBLR = 0;
			// So, iterate through all the instructions in our block, for as long we haven't found both a BL and a BLR
			for (std::size_t i = 0; (!foundBL || !foundBLR) && i < instructionPtrVec.size(); i++)
			{
				asmInstruction* currPtr = instructionPtrVec[i];
				// Skip if the instructionPtr was null (generally because parsing the incoming hex failed).
				if (currPtr == nullptr) continue;

				// If we're looking at a BCLR of any kind...
				if (!foundBLR && currPtr->primaryOpCode == asmPrimaryOpCodes::aPOC_B_SpReg && currPtr->secondaryOpCode == 16)
				{
					// ... mark that down.
					foundBLR = 1;
				}
				// If we're looking at a B or BC instruction...
				else if (!foundBL && (currPtr->primaryOpCode == asmPrimaryOpCodes::aPOC_B) || (currPtr->primaryOpCode == asmPrimaryOpCodes::aPOC_BC))
				{
					lava::ppc::argumentLayout* currLayoutPtr = currPtr->getArgLayoutPtr();
					if (currLayoutPtr != nullptr)
					{
						// Grab its set of instructions...
						std::vector<unsigned long> instructionArgs = currLayoutPtr->splitHexIntoArguments(hexVecIn[i]);
						// ... and if the LK bit is set, mark that down as well.
						foundBL |= instructionArgs.back();
					}
				}
			}
			// If we did find the pair of instructions we're looking for we need to disable Embed output.
			disableDataEmbedOutput = foundBL && foundBLR;

			std::size_t currentEmbedStart = SIZE_MAX;
			std::size_t currentEmbedExpectedEnd = SIZE_MAX;
			// If embeds *aren't* disabled, use the catalogue of branch events to detect any data embeds.
			for (auto i = relativeBranchEventsMap.begin(); !disableDataEmbedOutput && i != relativeBranchEventsMap.end(); i++)
			{
				// If we've got an embed open, and we've reached a destination event...
				if ((currentEmbedStart != SIZE_MAX) && (!i->second.incomingBranchStartIndices.empty()))
				{
					// ... and we've branched over at least 1 instruction/word of data (ie. our embed length > 0)...
					if ((currentEmbedStart + 1) < i->first)
					{
						
						// ... record it.
						dataEmbedStartsToLengths[currentEmbedStart] = i->first - currentEmbedStart;
						// Additionally, set its label condition appropriately!
						if (i->second.labelCondition != branchEventSummary::destinationLabelCondition::dlc_END_BRANCH)
						{
							i->second.labelCondition = branchEventSummary::destinationLabelCondition::dlc_DATA_EMBED;
						}
					}

					// In any case, close the open embed.
					currentEmbedStart = SIZE_MAX;
				}
				// If there is no embed open, and this event is a unconditional forward branch...
				if ((currentEmbedStart == SIZE_MAX) && (i->second.outgoingBranchIsUnconditional && !i->second.outgoingBranchGoesBackwards))
				{
					// ... and this instruction branches over at least 1 instruction/word of data (ie. our embed length > 0)...
					if ((i->first + 1) < i->second.outgoingBranchDestIndex)
					{
						// ... note the instruction immediately following it as the prospective start of an embed...
						currentEmbedStart = i->first + 1;
						// ... and the targeted instruction as the prospective end.
						currentEmbedExpectedEnd = i->second.outgoingBranchDestIndex;
					}
				}
			}
		}

	public:
		// Takes in a block of PPC Hex Instructions and analyzes it for information to aid the Block Parsing process.
		bool populateProfileFromHexBlock(const std::vector<unsigned long>& hexVecIn)
		{
			if (hexVecIn.empty()) return 0;

			// Determine whether or not the block contains an even number of instructions, and ends with null word, as Gecko expects.
			blockIsGeckoCompliant = !(hexVecIn.size() % 2) && (hexVecIn.back() == 0x00);
			// Stores the asmInstruction pointer for each instruction in the block.
			populateInstructionPtrVec(hexVecIn);
			// Log all the branch events in the block, for data embed tracking.
			populateBranchEventMap(hexVecIn);
			// Determine whether or not data embed output needed to be disabled, and if they aren't, find any data embeds in the block.
			populateDataEmbedMap(hexVecIn);

			return 1;
		}

	} currentBlockInfo;
	
	// Instruction to String Conversion Predicates
	std::string defaultAsmInstrToStrFunc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		return instructionIn->mnemonic;
	}
	std::string bcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			bool useAbsolute = 0;
			unsigned char BO = argumentsIn[1];
			unsigned char BI = argumentsIn[2];

			// Get the string to be used for the offset value.
			std::string immediateStr("");
			// If AA is set, then it'll just be the shifted value in hex
			if (argumentsIn[4] != 0)
			{
				immediateStr = "0x" + lava::numToHexStringWithPadding(argumentsIn[3] << 2, 0);
			}
			// Otherwise though...
			else
			{
				// ... we need to convert it to signed and get our string from there.
				immediateStr = unsignedImmArgToSignedString((argumentsIn[3] << 2), 14, 1);
				// Additionally, check if the offset value is negative...
				if (lava::stringToNum<signed long>(immediateStr, 1, LONG_MAX, 1) < 0)
				{
					// ... and if it is, we need to invert the y-bit in BO. Why? No clue. But we do.
					// Shoutouts to Gaberboo for the initial tip on this lol.
					BO ^= 0b1;
				}
			}

			std::string baseMnemonicSuffix = "";
			// If link flag is set...
			if (argumentsIn[5] != 0)
			{
				// ... append 'l'
				baseMnemonicSuffix += "l";
			}
			if (argumentsIn[4] != 0)
			{
				// ... append 'a'
				baseMnemonicSuffix += "a";
			}

			std::string crfString = (BI / 4) ? (" cr" + lava::numToDecStringWithPadding(BI / 4, 0) + ",") : ("");

			std::string simpleMnem = parseBOAndBIToBranchMnem(BO, BI % 4, baseMnemonicSuffix);
			if (!simpleMnem.empty() && (simpleMnem != "b"))
			{
				result << simpleMnem << crfString;
			}
			else
			{
				result << instructionIn->mnemonic;
				if (argumentsIn[5] != 0)
				{
					result << "l";
				}
				if (argumentsIn[4] != 0)
				{
					result << "a";
				}

				result << " " << (unsigned long)BO << ", " << (unsigned long)BI << ",";
			}
			
			result << " " << immediateStr;
		}

		return result.str();
	}
	std::string bclrConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			bool useAbsolute = 0;
			unsigned char BO = argumentsIn[1];
			unsigned char BI = argumentsIn[2];
			unsigned char BH = argumentsIn[4];

			std::string baseMnemonicSuffix = "lr";
			// If link flag is set...
			if (argumentsIn[6])
			{
				// ... append 'l'
				baseMnemonicSuffix += "l";
			}

			std::string crfString = (BI / 4) ? (" cr" + lava::numToDecStringWithPadding(BI / 4, 0) + ",") : ("");

			std::string simpleMnem = (BH == 0) ? parseBOAndBIToBranchMnem(BO, BI % 4, baseMnemonicSuffix) : "";
			if (!simpleMnem.empty())
			{
				result << simpleMnem << crfString;
			}
			else
			{
				result << instructionIn->mnemonic;
				if (argumentsIn[6] != 0)
				{
					result << "l";
				}

				result << " " << (int)BO << ", " << (int)BI << ", " << (int)BH;
			}
		}

		return result.str();
	}
	std::string bcctrConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			bool useAbsolute = 0;
			unsigned char BO = argumentsIn[1];
			unsigned char BI = argumentsIn[2];
			unsigned char BH = argumentsIn[4];

			std::string baseMnemonicSuffix = "ctr";
			// If link flag is set...
			if (argumentsIn[6])
			{
				// ... append 'l'
				baseMnemonicSuffix += "l";
			}

			std::string crfString = (BI / 4) ? (" cr" + lava::numToDecStringWithPadding(BI / 4, 0) + ",") : ("");

			std::string simpleMnem = (BH == 0) ? parseBOAndBIToBranchMnem(BO, BI % 4, baseMnemonicSuffix) : "";
			if (!simpleMnem.empty())
			{
				result << simpleMnem << crfString;
			}
			else
			{
				result << instructionIn->mnemonic;
				if (argumentsIn[6] != 0)
				{
					result << "l";
				}

				result << " " << (int)BO << ", " << (int)BI << ", " << (int)BH;
			}
		}

		return result.str();
	}
	std::string bConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 4)
		{
			bool useAbsolute = 0;
			result << instructionIn->mnemonic;
			if (argumentsIn[3] != 0)
			{
				result << "l";
			}
			if (argumentsIn[2] != 0)
			{
				result << "a";
				useAbsolute = 1;
			}
			result << " ";
			if (useAbsolute)
			{
				result << "0x" << std::hex << (argumentsIn[1] << 2);
			}
			else
			{
				result << unsignedImmArgToSignedString((argumentsIn[1] << 2), 24, 1);
			}
		}

		return result.str();
	}
	std::string cmpwConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 8)
		{
			result << instructionIn->mnemonic << " ";
			if (argumentsIn[1] != 0)
			{
				result << "cr" << argumentsIn[1] << ", ";
			}
			result << "r" << argumentsIn[4];
			result << ", r" << argumentsIn[5];
		}

		return result.str();
	}
	std::string cmpwiConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic << " ";
			if (argumentsIn[1] != 0)
			{
				result << "cr" << argumentsIn[1] << ", ";
			}
			result << "r" << argumentsIn[4];
			result << ", " << unsignedImmArgToSignedString(argumentsIn[5], 16, 1);
		}

		return result.str();
	}
	std::string cmplwiConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic << " ";
			if (argumentsIn[1] != 0)
			{
				result << "cr" << argumentsIn[1] << ", ";
			}
			result << "r" << argumentsIn[4];
			result << ", 0x" << std::hex << argumentsIn[5];
		}

		return result.str();
	}
	std::string integerAddImmConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 4)
		{
			// Activate "li"/"lis" mnemonic if rA == 0
			if (argumentsIn[2] == 0)
			{
				if (instructionIn->mnemonic.back() == 's')
				{
					result << "lis";
				}
				else
				{
					result << "li";
				}
				result << " r" << argumentsIn[1];
				result << ", " << std::hex << "0x" << argumentsIn[3];
			}
			else
			{
				std::string immediateString = unsignedImmArgToSignedString(argumentsIn[3], 16, 1);
				std::size_t minusLoc = immediateString.find('-');
				if (minusLoc != std::string::npos)
				{
					if (instructionIn->mnemonic.back() == 's')
					{
						result << "subis";
					}
					else
					{
						result << "subi";
					}
					immediateString.erase(minusLoc, 1);
				}
				else
				{
					result << instructionIn->mnemonic;
				}
				result << " r" << argumentsIn[1];
				result << ", r" << argumentsIn[2] << ", ";
				result << immediateString;
			}
		}

		return result.str();
	}
	std::string integerORImmConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 4)
		{
			if (argumentsIn[1] == 0 && argumentsIn[2] == 0 && argumentsIn[3] == 0)
			{
				result << "nop";
			}
			else
			{
				result << instructionIn->mnemonic;
				result << " r" << argumentsIn[2];
				result << ", r" << argumentsIn[1];
				result << ", " << std::hex << "0x" << argumentsIn[3];
			}
		}

		return result.str();
	}
	std::string integerLogicalIMMConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 4)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[2];
			result << ", r" << argumentsIn[1] << ", ";
			result << std::hex << "0x" << argumentsIn[3];
		}

		return result.str();
	}
	std::string integerLoadStoreConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 4)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[1] << ", ";
			result << unsignedImmArgToSignedString(argumentsIn[3], 16, 1);
			result << "(r" << argumentsIn[2] << ")";
		}

		return result.str();
	}
	std::string integerArith2RegWithOEAndRc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn[4])
			{
				result << "o";
			}
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " r" << argumentsIn[1];
			result << ", r" << argumentsIn[2];
		}

		return result.str();
	}
	std::string integerArith3RegWithOEAndRc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn[4])
			{
				result << "o";
			}
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " r" << argumentsIn[1];
			result << ", r" << argumentsIn[2];
			result << ", r" << argumentsIn[3];
		}

		return result.str();
	}
	std::string integer2RegWithSIMMConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 4)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[1];
			result << ", r" << argumentsIn[2] << ", ";
			result << unsignedImmArgToSignedString(argumentsIn[3], 16, 1);
		}

		return result.str();
	}
	std::string integer3RegNoRc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[1];
			result << ", r" << argumentsIn[2];
			result << ", r" << argumentsIn[3];
		}

		return result.str();
	}
	std::string integer3RegWithRc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " r" << argumentsIn[1];
			result << ", r" << argumentsIn[2];
			result << ", r" << argumentsIn[3];
		}

		return result.str();
	}
	std::string integer2RegSASwapWithRc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " r" << argumentsIn[2];
			result << ", r" << argumentsIn[1];
		}

		return result.str();
	}
	std::string integer3RegSASwapWithRc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			if (instructionIn->mnemonic == "or" && argumentsIn[1] == argumentsIn[3])
			{
				result << "mr r" << argumentsIn[2] << ", r" << argumentsIn[1];
			}
			else
			{
				result << instructionIn->mnemonic;
				if (argumentsIn.back())
				{
					result << '.';
				}
				result << " r" << argumentsIn[2];
				result << ", r" << argumentsIn[1];
				result << ", r" << argumentsIn[3];
			}
		}

		return result.str();
	}
	std::string integer2RegSASwapWithSHAndRc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " r" << argumentsIn[2];
			result << ", r" << argumentsIn[1];
			result << ", " << argumentsIn[3];
		}

		return result.str();
	}
	std::string lswiConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " r" << argumentsIn[1];
			result << ", r" << argumentsIn[2];
			result << ", " << argumentsIn[3];
		}

		return result.str();
	}
	std::string rlwnmConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			unsigned char MB = argumentsIn[4];
			unsigned char ME = argumentsIn[5];

			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " r" << argumentsIn[2];
			result << ", r" << argumentsIn[1];
			result << ", r" << argumentsIn[3];
			result << ", " << argumentsIn[4];
			result << ", " << argumentsIn[5];
			result << std::string(std::max<signed long>(0x20 - result.tellp(), 0x00), ' ');
			result << "# (Mask: 0x" << getMaskFromMBMESH(MB, ME, 0) << ")";
		}

		return result.str();
	}
	std::string rlwinmConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			unsigned char SH = argumentsIn[3];
			unsigned char MB = argumentsIn[4];
			unsigned char ME = argumentsIn[5];

			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " r" << argumentsIn[2];
			result << ", r" << argumentsIn[1];
			result << ", " << argumentsIn[3];
			result << ", " << argumentsIn[4];
			result << ", " << argumentsIn[5];
			result << std::string(std::max<signed long>(0x20 - result.tellp(), 0x00), ' ');
			result << "# (Mask: 0x" << getMaskFromMBMESH(MB, ME, SH) << ")";
		}

		return result.str();
	}
	std::string floatCompareConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			result << " cr" << argumentsIn[1];
			result << ", f" << argumentsIn[3];
			result << ", f" << argumentsIn[4];
		}

		return result.str();
	}
	std::string floatLoadStoreConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 4)
		{
			result << instructionIn->mnemonic;
			result << " f" << argumentsIn[1] << ", ";
			result << unsignedImmArgToSignedString(argumentsIn[3], 16, 1);
			result << "(r" << argumentsIn[2] << ")";
		}

		return result.str();
	}
	std::string floatLoadStoreIndexedConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			result << " f" << argumentsIn[1] << ", ";
			result << " r" << argumentsIn[2] << ", ";
			result << " r" << argumentsIn[3];
		}

		return result.str();
	}
	std::string float1RegWithRc(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
		}

		return result.str();
	}
	std::string float2RegOmitAWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string float3RegOmitCWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[2];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string float3RegOmitACWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string float3RegOmitBWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[2];
			result << ", f" << argumentsIn[4];
		}

		return result.str();
	}
	std::string float4RegBCSwapWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[2];
			result << ", f" << argumentsIn[4];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string moveToFromSPRegConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 5)
		{
			unsigned long flippedSpRegID = ((argumentsIn[2] & 0b11111) << 5) | (argumentsIn[2] >> 5);
			switch (flippedSpRegID)
			{
			case 1:
			{
				result << instructionIn->mnemonic.substr(0, 2) << "xer";
				break;
			}
			case 8:
			{
				result << instructionIn->mnemonic.substr(0, 2) << "lr";
				break;
			}
			case 9:
			{
				result << instructionIn->mnemonic.substr(0, 2) << "ctr";
				break;
			}
			default:
			{
				result << instructionIn->mnemonic << " " << flippedSpRegID << ",";
				break;
			}
			}
			result << " r" << argumentsIn[1];
		}

		return result.str();
	}
	std::string moveToFromMSRegConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[1];
		}

		return result.str();
	}
	std::string moveToFromSegRegConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[1];
			result << ", " << argumentsIn[3];
		}

		return result.str();
	}
	std::string moveToFromSegInRegConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[1];
			result << ", r" << argumentsIn[3];
		}

		return result.str();
	}
	std::string conditionRegLogicalsConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			result << " " << argumentsIn[1];
			result << ", " << argumentsIn[2];
			result << ", " << argumentsIn[3];
		}

		return result.str();
	}
	std::string conditionRegMoveFieldConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 8)
		{
			result << instructionIn->mnemonic;
			result << " cr" << argumentsIn[1];
			result << ", cr" << argumentsIn[3];
		}

		return result.str();
	}
	std::string pairedSingleCompareConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			result << " cr" << argumentsIn[1];
			result << ", f" << argumentsIn[3];
			result << ", f" << argumentsIn[4];
		}

		return result.str();
	}
	std::string pairedSingleQLoadStoreConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			result << " f" << argumentsIn[1] << ", ";
			result << unsignedImmArgToSignedString(argumentsIn[5], 16, 1);
			result << "(r" << argumentsIn[2] << "), ";
			result << argumentsIn[3] << ", ";
			result << argumentsIn[4];
		}

		return result.str();
	}
	std::string pairedSingleQLoadStoreIndexedConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 8)
		{
			result << instructionIn->mnemonic;
			result << " f" << argumentsIn[1] << ", ";
			result << "r" << argumentsIn[2] << ", ";
			result << "r" << argumentsIn[3] << ", ";
			result << argumentsIn[4] << ", ";
			result << argumentsIn[5];
		}

		return result.str();
	}
	std::string pairedSingle3RegWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[2];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string pairedSingle3RegOmitAWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string pairedSingle4RegWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[2];
			result << ", f" << argumentsIn[4];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string pairedSingle4RegOmitBWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[2];
			result << ", f" << argumentsIn[4];
		}

		return result.str();
	}
	std::string pairedSingle4RegOmitCWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[2];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string pairedSingle4RegOmitACWithRcConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " f" << argumentsIn[1];
			result << ", f" << argumentsIn[3];
		}

		return result.str();
	}
	std::string dataCache3RegOmitDConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[2];
			result << ", r" << argumentsIn[3];
		}

		return result.str();
	}
	std::string mftbConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 5)
		{
			int fixedTBRArg = (argumentsIn[2] >> 5) | ((argumentsIn[2] & 0b11111) << 5);
			result << instructionIn->mnemonic;
			switch (fixedTBRArg)
			{
			case 268:
			{
				result << ", r" << argumentsIn[1];
				break;
			}
			case 269:
			{
				result << "u, r" << argumentsIn[1];
			}
			default:
			{
				result << ", r" << argumentsIn[1] << ", " << fixedTBRArg;
				break;
			}
			}
		}

		return result.str();
	}
	std::string mcrxrConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			result << " cr" << argumentsIn[1];
		}

		return result.str();
	}
	std::string mtcrfConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			if (argumentsIn[3] == 0xFF)
			{
				result << "mtcr";
			}
			else
			{
				result << instructionIn->mnemonic;
				result << " 0x" << std::hex << argumentsIn[3] << std::dec << ",";
			}
			result << " r" << argumentsIn[1];
		}

		return result.str();
	}
	std::string mtfsb0Conv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " cr" << argumentsIn[1];
		}

		return result.str();
	}
	std::string mtfsfConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 7)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " 0x" << std::hex << argumentsIn[2] << std::dec;
			result << ", f" << argumentsIn[4];
		}

		return result.str();
	}
	std::string mtfsfiConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 8)
		{
			result << instructionIn->mnemonic;
			if (argumentsIn.back())
			{
				result << '.';
			}
			result << " cr" << argumentsIn[1];
			result << ", " << argumentsIn[4];
		}

		return result.str();
	}
	std::string twConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			std::string simpleTOMnem = getTOEncodingString(argumentsIn[1]);
			if (!simpleTOMnem.empty())
			{
				if (simpleTOMnem.front() == '-')
				{
					result << "trap";
				}
				else
				{
					result << instructionIn->mnemonic << simpleTOMnem << " r" << argumentsIn[2] << ", r" << argumentsIn[3];
				}
			}
			else
			{
				result << instructionIn->mnemonic << " " << argumentsIn[1] << ", r" << argumentsIn[2] << ", r" << argumentsIn[3];

			}
		}

		return result.str();
	}
	std::string twiConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 4)
		{
			std::string simpleTOMnem = getTOEncodingString(argumentsIn[1]);
			std::string signedImmString = unsignedImmArgToSignedString(argumentsIn[3], instructionIn->getArgLayoutPtr()->getArgLengthInBits(3));
			if (!simpleTOMnem.empty() && simpleTOMnem.front() != '-')
			{
				result << "tw" << simpleTOMnem << "i r" << argumentsIn[2] << ", " << signedImmString;
			}
			else
			{
				result << instructionIn->mnemonic << " " << argumentsIn[1] << ", r" << argumentsIn[2] << ", " << signedImmString;
			}
		}

		return result.str();
	}
	std::string tlbieConv(asmInstruction* instructionIn, unsigned long hexIn)
	{
		std::stringstream result;

		std::vector<unsigned long> argumentsIn = instructionIn->getArgLayoutPtr()->splitHexIntoArguments(hexIn);
		if (argumentsIn.size() >= 6)
		{
			result << instructionIn->mnemonic;
			result << " r" << argumentsIn[3];
		}

		return result.str();
	}


	// argumentLayout Functions
	std::array<argumentLayout, (int)asmInstructionArgLayout::aIAL_LAYOUT_COUNT> layoutDictionary{};
	void argumentLayout::setArgumentReservations(std::vector<std::pair<char, asmInstructionArgResStatus>> reservationsIn)
	{
		reservedZeroMask = 0;
		reservedOneMask = 0;

		unsigned char argMaskStart = UCHAR_MAX;
		unsigned char argMaskEnd = UCHAR_MAX;
		std::pair<char, asmInstructionArgResStatus>* currRes = nullptr;
		unsigned char currResTargetIndex = UCHAR_MAX;
		for (std::size_t i = 0; i < reservationsIn.size(); i++)
		{
			currRes = &reservationsIn[i];
			if (currRes->first < 0)
			{
				currResTargetIndex = argumentStartBits.size() + currRes->first;
			}
			else
			{
				currResTargetIndex = currRes->first;
			}
			if (currResTargetIndex < argumentStartBits.size())
			{
				argMaskStart = argumentStartBits[currResTargetIndex];
				argMaskEnd = argMaskStart + (getArgLengthInBits(currResTargetIndex) - 1);
				if (currRes->second == asmInstructionArgResStatus::aIARS_MUST_BE_ZERO)
				{
					reservedZeroMask |= maskBetweenBitsInclusive(argMaskStart, argMaskEnd, 0);
				}
				else
				{
					reservedOneMask |= maskBetweenBitsInclusive(argMaskStart, argMaskEnd, 0);
				}
			}
		}
	}
	unsigned long argumentLayout::getSecOpMask() const
	{
		unsigned long result = 0;

		if (secOpArgIndex < argumentStartBits.size())
		{
			unsigned char maskStart = argumentStartBits[secOpArgIndex];
			unsigned char maskEnd = ((secOpArgIndex + 1) < argumentStartBits.size()) ? argumentStartBits[secOpArgIndex + 1] - 1 : 31;

			result = maskBetweenBitsInclusive(maskStart, maskEnd, 0);
		}

		return result;
	}
	bool argumentLayout::validateReservedArgs(unsigned long instructionHexIn) const
	{
		bool result = 1;

		result &= (instructionHexIn & reservedZeroMask) == 0;
		result &= (instructionHexIn & reservedOneMask) == reservedOneMask;

		return result;
	}
	std::vector<unsigned long> argumentLayout::splitHexIntoArguments(unsigned long instructionHexIn) const
	{
		std::vector<unsigned long> result{};

		for (unsigned long i = 0; i < argumentStartBits.size(); i++)
		{
			result.push_back(extractInstructionArg(instructionHexIn, argumentStartBits[i], getArgLengthInBits(i)));
		}

		return result;
	}
	unsigned char argumentLayout::getArgLengthInBits(unsigned char argIndex) const
	{
		unsigned char result = UCHAR_MAX;

		if (argIndex < argumentStartBits.size())
		{
			if ((argIndex + 1) < argumentStartBits.size())
			{
				result = argumentStartBits[argIndex + 1] - argumentStartBits[argIndex];
			}
			else
			{
				result = 32 - argumentStartBits.back();
			}
		}

		return result;
	}
	asmInstructionArgResStatus argumentLayout::getArgReservation(unsigned char argIndex) const
	{
		asmInstructionArgResStatus result = asmInstructionArgResStatus::aIARS_NULL;

		if (argIndex < argumentStartBits.size())
		{
			unsigned long argBottomBitMask = 0b1 << (31 - argumentStartBits[argIndex]);
			if (argBottomBitMask & reservedZeroMask)
			{
				result = asmInstructionArgResStatus::aIARS_MUST_BE_ZERO;
			}
			else if (argBottomBitMask & reservedOneMask)
			{
				result = asmInstructionArgResStatus::aIARS_MUST_BE_ONE;
			}
		}

		return result;
	}
	argumentLayout* defineArgLayout(asmInstructionArgLayout IDIn, std::vector<unsigned char> argStartsIn,
		std::string(*convFuncIn)(asmInstruction*, unsigned long))
	{
		argumentLayout* targetLayout = &layoutDictionary[(int)IDIn];
		targetLayout->layoutID = IDIn;
		targetLayout->argumentStartBits = argStartsIn;
		for (std::size_t i = 0; targetLayout->secOpArgIndex == UCHAR_MAX && i < targetLayout->argumentStartBits.size(); i++)
		{
			if (targetLayout->argumentStartBits[i] & isSecOpArgFlag)
			{
				targetLayout->argumentStartBits[i] &= ~isSecOpArgFlag;
				targetLayout->secOpArgIndex = i;
			}
		}
		targetLayout->conversionFunc = convFuncIn;
		return targetLayout;
	}


	// asmInstruction Functions
	argumentLayout* asmInstruction::getArgLayoutPtr() const
	{
		argumentLayout* result = nullptr;

		if (layoutID != asmInstructionArgLayout::aIAL_NULL)
		{
			result = &layoutDictionary[(int)layoutID];
		}

		return result;
	}
	unsigned long asmInstruction::getTestHex() const
	{
		unsigned long result = ULONG_MAX;

		argumentLayout* parentLayout = getArgLayoutPtr();

		if (parentLayout)
		{
			result = 0;

			for (std::size_t i = 1; (i + 1) < parentLayout->argumentStartBits.size(); i++)
			{
				result |= 1 << (31 - (parentLayout->argumentStartBits[i + 1] - 1));
			}
			if (parentLayout->argumentStartBits.size() > 1)
			{
				result |= 1;
			}

			result &= ~parentLayout->getSecOpMask();
			result &= ~parentLayout->reservedZeroMask;
			result |= parentLayout->reservedOneMask;
			result |= canonForm;
		}

		return result;
	}
	std::string asmInstruction::getArgLayoutString() const
	{
		std::stringstream tempArgString("");
		std::string argContentStr = "";
		std::string argPaddingStr = "";
		constexpr unsigned char bitLength = 4;
		argContentStr.reserve(16 * bitLength);
		std::size_t targetStrLen = SIZE_MAX;

		argumentLayout* currInstrLayout = getArgLayoutPtr();
		if (currInstrLayout != nullptr)
		{
			if (currInstrLayout->argumentStartBits.size() > 1)
			{
				for (std::size_t argIdx = 0; argIdx < currInstrLayout->argumentStartBits.size(); argIdx++)
				{
					if (argIdx == 0)
					{
						argContentStr = "PrOp: " + std::to_string((int)primaryOpCode);
					}
					else if (argIdx == currInstrLayout->secOpArgIndex)
					{
						argContentStr = "SecOp: " + std::to_string(secondaryOpCode);
					}
					else if (currInstrLayout->getArgReservation(argIdx) != asmInstructionArgResStatus::aIARS_NULL)
					{
						if (currInstrLayout->getArgReservation(argIdx) == asmInstructionArgResStatus::aIARS_MUST_BE_ZERO)
						{
							argContentStr =  "R0";
						}
						else
						{
							argContentStr = "R1";
						}
					}
					else
					{
						argContentStr = "A" + std::to_string(argIdx);
					}

					targetStrLen = (currInstrLayout->getArgLengthInBits(argIdx) * bitLength) - 2;
					argPaddingStr = (targetStrLen > argContentStr.size()) ? std::string((targetStrLen - argContentStr.size()) / 2, ' ') : "";

					tempArgString << "[" << argPaddingStr << argContentStr << argPaddingStr;
					if (argContentStr.size() % 2)
					{
						tempArgString << " ";
					}
					tempArgString << "]";
				}
			}
		}
		
		return tempArgString.str();
	}


	// asmPrOpCodeGroup Functions
	bool asmPrOpCodeGroup::startLenPairCompare::operator() (std::pair<unsigned char, unsigned char> a, std::pair<unsigned char, unsigned char> b) const
	{
		return (a.first != b.first) ? a.first < b.first : a.second > b.second;
	}
	unsigned long instructionCount = 0x00;
	asmInstruction* asmPrOpCodeGroup::pushInstruction(std::string nameIn, std::string mnemIn, asmInstructionArgLayout layoutIDIn, unsigned short secOpIn)
	{
		asmInstruction* result = nullptr;

		if (secondaryOpCodeToInstructions.find(secOpIn) == secondaryOpCodeToInstructions.end())
		{
			result = &secondaryOpCodeToInstructions[secOpIn];
			result->primaryOpCode = this->primaryOpCode;
			result->name = nameIn;
			result->mnemonic = mnemIn;
			result->secondaryOpCode = secOpIn;
			result->canonForm = (int)result->primaryOpCode << (32 - 6);
			result->layoutID = layoutIDIn;

			argumentLayout* layoutPtr = result->getArgLayoutPtr();

			if (!secOpCodeStartsAndLengths.empty())
			{
				unsigned char expectedSecOpCodeEnd = secOpCodeStartsAndLengths.begin()->first + secOpCodeStartsAndLengths.begin()->second;
				result->canonForm |= result->secondaryOpCode << (32 - expectedSecOpCodeEnd);
			}

			result->canonForm |= result->getArgLayoutPtr()->reservedOneMask;

			instructionCount++;
		}

		return result;
	}


	// instructionDictionary
	std::map<unsigned short, asmPrOpCodeGroup> instructionDictionary{};
	asmPrOpCodeGroup* pushOpCodeGroupToDict(asmPrimaryOpCodes opCodeIn, unsigned char secOpCodeStart, unsigned char secOpCodeLength)
	{
		asmPrOpCodeGroup* result = nullptr;

		if (instructionDictionary.find((int)opCodeIn) == instructionDictionary.end())
		{
			result = &instructionDictionary[(int)opCodeIn];
			result->primaryOpCode = opCodeIn;
			if (secOpCodeStart != UCHAR_MAX && secOpCodeLength != UCHAR_MAX)
			{
				result->secOpCodeStartsAndLengths.insert({ secOpCodeStart, secOpCodeLength });
			}
		}

		return result;
	}
	void buildInstructionDictionary()
	{
		argumentLayout* currLayout = nullptr;
		asmPrOpCodeGroup* currentOpGroup = nullptr;
		asmInstruction* currentInstruction = nullptr;

		// Setup Instruction Argument Layouts
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_B, { 0, 6, 30, 31 }, bConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_BC, { 0, 6, 11, 16, 30, 31 }, bcConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_BCLR, { 0, 6, 11, 16, 19, isSecOpArgFlag | 21, 31 }, bclrConv); {
			currLayout->setArgumentReservations({
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_BCCTR, { 0, 6, 11, 16, 19, isSecOpArgFlag | 21, 31 }, bcctrConv); {
			currLayout->setArgumentReservations({
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_CMPW, { 0, 6, 9, 10, 11, 16, isSecOpArgFlag | 21, 31 }, cmpwConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_CMPWI, { 0, 6, 9, 10, 11, 16 }, cmpwiConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_CMPLWI, { 0, 6, 9, 10, 11, 16 }, cmplwiConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_IntADDI, { 0, 6, 11, 16 }, integerAddImmConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_IntORI, { 0, 6, 11, 16 }, integerORImmConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_IntLogicalIMM, { 0, 6, 11, 16 }, integerLogicalIMMConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_IntLoadStore, { 0, 6, 11, 16 }, integerLoadStoreConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_IntLoadStoreIdx, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, integer3RegWithRc); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_IntArith2RegWithOEAndRC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 22, 31 }, integerArith2RegWithOEAndRc); {
			currLayout->setArgumentReservations({
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 22, 31 }, integerArith3RegWithOEAndRc);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_IntArith3RegWithNoOEAndRC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 22, 31 }, integerArith3RegWithOEAndRc); {
			currLayout->setArgumentReservations({
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Int2RegWithSIMM, { 0, 6, 11, 16 }, integer2RegWithSIMMConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_STWCX, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, integer3RegNoRc); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ONE },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Int3RegNoRC, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, integer3RegNoRc); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Int3RegWithRC, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, integer3RegWithRc);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Int2RegSASwapWithRC, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, integer2RegSASwapWithRc); {
			currLayout->setArgumentReservations({
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, integer3RegSASwapWithRc);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Int2RegSASwapWithSHAndRC, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, integer2RegSASwapWithSHAndRc);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_LSWI, {0, 6, 11, 16, isSecOpArgFlag | 21, 31}, lswiConv); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_RLWNM, { 0, 6, 11, 16, 21, 26, 31 }, rlwnmConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_RLWINM, { 0, 6, 11, 16, 21, 26, 31 }, rlwinmConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_FltCompare, { 0, 6, 9, 11, 16, isSecOpArgFlag | 21, 31 }, floatCompareConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_FltLoadStore, { 0, 6, 11, 16 }, floatLoadStoreConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, floatLoadStoreIndexedConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Flt1RegWithRC, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, float1RegWithRc); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Flt2RegOmitAWithRC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, float2RegOmitAWithRcConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Flt3RegOmitBWithRC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, float3RegOmitBWithRcConv); {
			currLayout->setArgumentReservations({
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Flt3RegOmitCWithRC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, float3RegOmitCWithRcConv); {
			currLayout->setArgumentReservations({ 
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Flt3RegOmitACWithRC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, float3RegOmitACWithRcConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, float4RegBCSwapWithRcConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MoveToFromSPReg, { 0, 6, 11, isSecOpArgFlag | 21, 31 }, moveToFromSPRegConv); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MoveToFromMSReg, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, moveToFromMSRegConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MoveToFromSegReg, { 0, 6, 11, 12, 16, isSecOpArgFlag | 21, 31 }, moveToFromSegRegConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MoveToFromSegInReg, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, moveToFromSegInRegConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_ConditionRegLogicals, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, conditionRegLogicalsConv); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_ConditionRegMoveField, { 0, 6, 9, 11, 14, 16, isSecOpArgFlag | 21, 31 }, conditionRegMoveFieldConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 5, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingleCompare, { 0, 6, 9, 11, 16, isSecOpArgFlag | 21, 31 }, pairedSingleCompareConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingleQLoadStore, { 0, 6, 11, 16, 17, 20 }, pairedSingleQLoadStoreConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingleQLoadStoreIdx, { 0, 6, 11, 16, 21, 22, isSecOpArgFlag | 25, 31 }, pairedSingleQLoadStoreIndexedConv); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingle3Reg, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, pairedSingle3RegWithRcConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingle3RegOmitA, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, pairedSingle3RegOmitAWithRcConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingle4Reg, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, pairedSingle4RegWithRcConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingle4RegOmitB, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, pairedSingle4RegOmitBWithRcConv); {
			currLayout->setArgumentReservations({
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingle4RegOmitC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, pairedSingle4RegOmitCWithRcConv); {
			currLayout->setArgumentReservations({
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_PairedSingle4RegOmitAC, { 0, 6, 11, 16, 21, isSecOpArgFlag | 26, 31 }, pairedSingle4RegOmitACWithRcConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_DataCache3RegOmitD, {0, 6, 11, 16, isSecOpArgFlag | 21, 31}, dataCache3RegOmitDConv); {
			currLayout->setArgumentReservations({
				{ 1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MemSyncNoReg, {0, 6, 11, 16, isSecOpArgFlag | 21, 31}, defaultAsmInstrToStrFunc); {
			currLayout->setArgumentReservations({
				{ 1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MFTB, { 0, 6, 11, isSecOpArgFlag | 21, 31 }, mftbConv); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MCRXR, { 0, 6, 9, 11, 16, isSecOpArgFlag | 21, 31 }, mcrxrConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MTCRF, { 0, 6, 11, 12, 20, isSecOpArgFlag | 21, 31 }, mtcrfConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MTFSB0, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, mtfsb0Conv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MTFSF, { 0, 6, 7, 15, 16, isSecOpArgFlag | 21, 31 }, mtfsfConv); {
			currLayout->setArgumentReservations({
				{ 1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_MTFSFI, { 0, 6, 9, 11, 16, 20, isSecOpArgFlag | 21, 31 }, mtfsfiConv); {
			currLayout->setArgumentReservations({
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 5, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_TW, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, twConv); {
			currLayout->setArgumentReservations({
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_TWI, { 0, 6, 11, 16 }, twiConv);
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_TLBIE, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, tlbieConv); {
			currLayout->setArgumentReservations({
				{ 1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_RFI, { 0, 6, 11, 16, isSecOpArgFlag | 21, 31 }, defaultAsmInstrToStrFunc); {
			currLayout->setArgumentReservations({
				{ 1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}
		currLayout = defineArgLayout(asmInstructionArgLayout::aIAL_SC, { 0, 6, 11, 16, 30, 31 }, defaultAsmInstrToStrFunc); {
			currLayout->setArgumentReservations({
				{ 1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 2, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 3, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				{ 4, asmInstructionArgResStatus::aIARS_MUST_BE_ONE },
				{ -1, asmInstructionArgResStatus::aIARS_MUST_BE_ZERO },
				});
		}

		// Branch Instructions
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_BC);
		{
			currentInstruction = currentOpGroup->pushInstruction("Branch Conditional", "bc", asmInstructionArgLayout::aIAL_BC);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_B);
		{
			currentInstruction = currentOpGroup->pushInstruction("Branch", "b", asmInstructionArgLayout::aIAL_B);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_B_SpReg, 21, 10);
		{
			// Operation:: BCCTR
			currentInstruction = currentOpGroup->pushInstruction("Branch Conditional to Count Register", "bcctr", asmInstructionArgLayout::aIAL_BCCTR, 528);
			// Operation:: BCLR
			currentInstruction = currentOpGroup->pushInstruction("Branch Conditional to Link Register", "bclr", asmInstructionArgLayout::aIAL_BCLR, 16);

			// Operation:: CRAND, CRANDC
			currentInstruction = currentOpGroup->pushInstruction("Condition Register AND", "crand", asmInstructionArgLayout::aIAL_ConditionRegLogicals, 257);
			currentInstruction = currentOpGroup->pushInstruction("Condition Register AND" + opName_WithComplString, "crandc", asmInstructionArgLayout::aIAL_ConditionRegLogicals, 129);
			// Operation:: CREQV
			currentInstruction = currentOpGroup->pushInstruction("Condition Register Equivalent", "creqv", asmInstructionArgLayout::aIAL_ConditionRegLogicals, 289);
			// Operation:: CRNAND
			currentInstruction = currentOpGroup->pushInstruction("Condition Register NAND", "crnand", asmInstructionArgLayout::aIAL_ConditionRegLogicals, 225);
			// Operation:: CNOR
			currentInstruction = currentOpGroup->pushInstruction("Condition Register NOR", "crnor", asmInstructionArgLayout::aIAL_ConditionRegLogicals, 33);
			// Operation:: CROR, CRORC
			currentInstruction = currentOpGroup->pushInstruction("Condition Register OR", "cror", asmInstructionArgLayout::aIAL_ConditionRegLogicals, 499);
			currentInstruction = currentOpGroup->pushInstruction("Condition Register OR" + opName_WithComplString, "crorc", asmInstructionArgLayout::aIAL_ConditionRegLogicals, 417);
			// Operation:: CRXOR
			currentInstruction = currentOpGroup->pushInstruction("Condition Register XOR", "crxor", asmInstructionArgLayout::aIAL_ConditionRegLogicals, 193);
			
			// Operation: MCRF
			currentInstruction = currentOpGroup->pushInstruction("Move Condition Register Field", "mcrf", asmInstructionArgLayout::aIAL_ConditionRegMoveField, 0);

			// Operation: ISYNC
			currentInstruction = currentOpGroup->pushInstruction("Instruction Synchronize", "isync", asmInstructionArgLayout::aIAL_MemSyncNoReg, 150);

			// Operation: RFI
			currentInstruction = currentOpGroup->pushInstruction("Return from Interrupt", "rfi", asmInstructionArgLayout::aIAL_RFI, 50);
		}

		// Compare Instructions
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_CMPWI);
		{
			currentInstruction = currentOpGroup->pushInstruction("Compare Word Immediate", "cmpwi", asmInstructionArgLayout::aIAL_CMPWI);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_CMPLWI);
		{
			currentInstruction = currentOpGroup->pushInstruction("Compare Logical Word Immediate", "cmplwi", asmInstructionArgLayout::aIAL_CMPLWI);
		}

		// Integer Arithmetic Instructions
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_MULLI);
		{
			currentInstruction = currentOpGroup->pushInstruction("Multiply Low Immediate", "mulli", asmInstructionArgLayout::aIAL_Int2RegWithSIMM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_SUBFIC);
		{
			currentInstruction = currentOpGroup->pushInstruction("Subtract From Immediate Carrying", "subfic", asmInstructionArgLayout::aIAL_Int2RegWithSIMM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_ADDIC);
		{
			currentInstruction = currentOpGroup->pushInstruction("Add Immediate Carrying", "addic", asmInstructionArgLayout::aIAL_Int2RegWithSIMM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_ADDIC_DOT);
		{
			currentInstruction = currentOpGroup->pushInstruction("Add Immediate Carrying and Record", "addic.", asmInstructionArgLayout::aIAL_Int2RegWithSIMM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_ADDI);
		{
			currentInstruction = currentOpGroup->pushInstruction("Add Immediate", "addi", asmInstructionArgLayout::aIAL_IntADDI);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_ADDIS);
		{
			currentInstruction = currentOpGroup->pushInstruction("Add Immediate Shifted", "addis", asmInstructionArgLayout::aIAL_IntADDI);
		}


		// Float Arithmetic Instructions
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_FLOAT_D_ARTH, 26, 5);
		{
			currentInstruction = currentOpGroup->pushInstruction("Floating Add" + opName_DoublePrecision, "fadd", asmInstructionArgLayout::aIAL_Flt3RegOmitCWithRC, 21);
			currentInstruction = currentOpGroup->pushInstruction("Floating Divide" + opName_DoublePrecision, "fdiv", asmInstructionArgLayout::aIAL_Flt3RegOmitCWithRC, 18);
			currentInstruction = currentOpGroup->pushInstruction("Floating Multiply" + opName_DoublePrecision, "fmul", asmInstructionArgLayout::aIAL_Flt3RegOmitBWithRC, 25);
			currentInstruction = currentOpGroup->pushInstruction("Floating Subtract" + opName_DoublePrecision, "fsub", asmInstructionArgLayout::aIAL_Flt3RegOmitCWithRC, 20);
			currentInstruction = currentOpGroup->pushInstruction("Floating Multiply-Add" + opName_DoublePrecision, "fmadd", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 29);
			currentInstruction = currentOpGroup->pushInstruction("Floating Multiply-Subtract" + opName_DoublePrecision, "fmsub", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 28);
			currentInstruction = currentOpGroup->pushInstruction("Floating Negative Multiply-Add" + opName_DoublePrecision, "fnmadd", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 31);
			currentInstruction = currentOpGroup->pushInstruction("Floating Negative Multiply-Subtract" + opName_DoublePrecision, "fnmsub", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 30);
			currentInstruction = currentOpGroup->pushInstruction("Floating Select", "fsel", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 23);
			currentInstruction = currentOpGroup->pushInstruction("Floating Reciprocal Square Root Estimate", "frsqrte", asmInstructionArgLayout::aIAL_Flt3RegOmitACWithRC, 26);
			

			// Instructions Which Use Extended Length SecOp
			currentOpGroup->secOpCodeStartsAndLengths.insert({ 21, 10 });
			currentInstruction = currentOpGroup->pushInstruction("Floating Convert to Integer Word", "fctiw", asmInstructionArgLayout::aIAL_Flt2RegOmitAWithRC, 14);
			currentInstruction = currentOpGroup->pushInstruction("Floating Convert to Integer Word with Round toward Zero", "fctiwz", asmInstructionArgLayout::aIAL_Flt2RegOmitAWithRC, 15);
			currentInstruction = currentOpGroup->pushInstruction("Floating Round to Single", "frsp", asmInstructionArgLayout::aIAL_Flt2RegOmitAWithRC, 12);
			currentInstruction = currentOpGroup->pushInstruction("Floating Move Register" + opName_DoublePrecision, "fmr", asmInstructionArgLayout::aIAL_Flt2RegOmitAWithRC, 72);
			currentInstruction = currentOpGroup->pushInstruction("Floating Negate", "fneg", asmInstructionArgLayout::aIAL_Flt2RegOmitAWithRC, 40);
			currentInstruction = currentOpGroup->pushInstruction("Floating Absolute Value", "fabs", asmInstructionArgLayout::aIAL_Flt2RegOmitAWithRC, 264);
			currentInstruction = currentOpGroup->pushInstruction("Floating Negative Absolute Value", "fnabs", asmInstructionArgLayout::aIAL_Flt2RegOmitAWithRC, 136);
			currentInstruction = currentOpGroup->pushInstruction("Floating Compare Unordered", "fcmpu", asmInstructionArgLayout::aIAL_FltCompare, 0);
			currentInstruction = currentOpGroup->pushInstruction("Floating Compare Ordered", "fcmpo", asmInstructionArgLayout::aIAL_FltCompare, 32);
			currentInstruction = currentOpGroup->pushInstruction("Move to Condition Register from FPSCR", "mcrfs", asmInstructionArgLayout::aIAL_ConditionRegMoveField, 64);
			currentInstruction = currentOpGroup->pushInstruction("Move from FPSCR", "mffs", asmInstructionArgLayout::aIAL_Flt1RegWithRC, 583);
			currentInstruction = currentOpGroup->pushInstruction("Move to FPSCR Bit 0", "mtfsb0", asmInstructionArgLayout::aIAL_MTFSB0, 70);
			currentInstruction = currentOpGroup->pushInstruction("Move to FPSCR Bit 1", "mtfsb1", asmInstructionArgLayout::aIAL_MTFSB0, 38);
			currentInstruction = currentOpGroup->pushInstruction("Move to FPSCR Fields", "mtfsf", asmInstructionArgLayout::aIAL_MTFSF, 711);
			currentInstruction = currentOpGroup->pushInstruction("Move to FPSCR Field Immediate", "mtfsfi", asmInstructionArgLayout::aIAL_MTFSFI, 134);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_FLOAT_S_ARTH, 26, 5);
		{
			currentInstruction = currentOpGroup->pushInstruction("Floating Add" + opName_SinglePrecision, "fadds", asmInstructionArgLayout::aIAL_Flt3RegOmitCWithRC, 21);
			currentInstruction = currentOpGroup->pushInstruction("Floating Divide" + opName_SinglePrecision, "fdivs", asmInstructionArgLayout::aIAL_Flt3RegOmitCWithRC, 18);
			currentInstruction = currentOpGroup->pushInstruction("Floating Multiply" + opName_SinglePrecision, "fmuls", asmInstructionArgLayout::aIAL_Flt3RegOmitBWithRC, 25);
			currentInstruction = currentOpGroup->pushInstruction("Floating Subtract" + opName_SinglePrecision, "fsubs", asmInstructionArgLayout::aIAL_Flt3RegOmitCWithRC, 20);
			currentInstruction = currentOpGroup->pushInstruction("Floating Multiply-Add" + opName_SinglePrecision, "fmadds", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 29);
			currentInstruction = currentOpGroup->pushInstruction("Floating Multiply-Subtract" + opName_SinglePrecision, "fmsubs", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 28);
			currentInstruction = currentOpGroup->pushInstruction("Floating Negative Multiply-Add" + opName_SinglePrecision, "fnmadds", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 31);
			currentInstruction = currentOpGroup->pushInstruction("Floating Negative Multiply-Subtract" + opName_SinglePrecision, "fnmsubs", asmInstructionArgLayout::aIAL_Flt4RegBCSwapWithRC, 30);
			currentInstruction = currentOpGroup->pushInstruction("Floating Reciprocal Estimate Single", "fres", asmInstructionArgLayout::aIAL_Flt3RegOmitACWithRC, 24);
		}


		// Load Instructions
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LWZ);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Word and Zero", "lwz", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LWZU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Word and Zero" + opName_WithUpdateString, "lwzu", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LBZ);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Byte and Zero", "lbz", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LBZU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Byte and Zero" + opName_WithUpdateString, "lbzu", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LHZ);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word and Zero", "lhz", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LHZU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word and Zero" + opName_WithUpdateString, "lhzu", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LHA);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word Algebraic", "lha", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LHAU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word Algebraic" + opName_WithUpdateString, "lhau", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LMW);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Multiple Word", "lmw", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LFS);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Floating-Point Single", "lfs", asmInstructionArgLayout::aIAL_FltLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LFSU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Floating-Point Single" + opName_WithUpdateString, "lfsu", asmInstructionArgLayout::aIAL_FltLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LFD);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Floating-Point Double", "lfd", asmInstructionArgLayout::aIAL_FltLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_LFDU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Load Floating-Point Double" + opName_WithUpdateString, "lfdu", asmInstructionArgLayout::aIAL_FltLoadStore);
		}
		

		// Store Instructions
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STW);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Word", "stw", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STWU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Word" + opName_WithUpdateString, "stwu", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STB);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Byte", "stb", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STBU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Byte" + opName_WithUpdateString, "stbu", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STH);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Half Word", "sth", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STHU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Half Word" + opName_WithUpdateString, "sthu", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STMW);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Multiple Word", "stmw", asmInstructionArgLayout::aIAL_IntLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STFS);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Floating-Point Single", "stfs", asmInstructionArgLayout::aIAL_FltLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STFSU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Half Floating-Point Single" + opName_WithUpdateString, "stfsu", asmInstructionArgLayout::aIAL_FltLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STFD);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Floating-Point Double", "stfd", asmInstructionArgLayout::aIAL_FltLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_STFDU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Store Half Floating-Point Double" + opName_WithUpdateString, "stfdu", asmInstructionArgLayout::aIAL_FltLoadStore);
		}

		
		// Logical Integer Instructions
			
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_ORI);
		{
			currentInstruction = currentOpGroup->pushInstruction("OR Immediate", "ori", asmInstructionArgLayout::aIAL_IntORI);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_ORIS);
		{
			currentInstruction = currentOpGroup->pushInstruction("OR Immediate Shifted", "oris", asmInstructionArgLayout::aIAL_IntLogicalIMM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_XORI);
		{
			currentInstruction = currentOpGroup->pushInstruction("XOR Immediate", "xori", asmInstructionArgLayout::aIAL_IntLogicalIMM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_XORIS);
		{
			currentInstruction = currentOpGroup->pushInstruction("XOR Immediate Shifted", "xoris", asmInstructionArgLayout::aIAL_IntLogicalIMM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_ANDI);
		{
			currentInstruction = currentOpGroup->pushInstruction("AND Immediate" + opName_WithUpdateString, "andi.", asmInstructionArgLayout::aIAL_IntLogicalIMM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_ANDIS);
		{
			currentInstruction = currentOpGroup->pushInstruction("AND Immediate Shifted" + opName_WithUpdateString, "andis.", asmInstructionArgLayout::aIAL_IntLogicalIMM);
		}

		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_RLWINM);
		{
			currentInstruction = currentOpGroup->pushInstruction("Rotate Left Word Immediate then AND with Mask", "rlwinm", asmInstructionArgLayout::aIAL_RLWINM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_RLWIMI);
		{
			currentInstruction = currentOpGroup->pushInstruction("Rotate Left Word Immediate then Mask Insert", "rlwimi", asmInstructionArgLayout::aIAL_RLWINM);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_RLWNM);
		{
			currentInstruction = currentOpGroup->pushInstruction("Rotate Left Word then AND with Mask", "rlwnm", asmInstructionArgLayout::aIAL_RLWNM);
		}

		// Op Code 31
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_31, 21, 10);
		{
			// Arithmetic Instructions with OE (Sec Op Starts at bit 22)
			currentOpGroup->secOpCodeStartsAndLengths.insert({ 22, 9 });
			// Operation: ADD
			currentInstruction = currentOpGroup->pushInstruction("Add", "add", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 266);
			// Operation: ADDC
			currentInstruction = currentOpGroup->pushInstruction("Add Carrying", "addc", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 10);
			// Operation: ADDE
			currentInstruction = currentOpGroup->pushInstruction("Add Extended", "adde", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 138);
			// Operation: ADDME
			currentInstruction = currentOpGroup->pushInstruction("Add to Minus One Extended", "addme", asmInstructionArgLayout::aIAL_IntArith2RegWithOEAndRC, 234);
			// Operation: ADDZE
			currentInstruction = currentOpGroup->pushInstruction("Add to Zero Extended", "addze", asmInstructionArgLayout::aIAL_IntArith2RegWithOEAndRC, 202);

			// Operation: DIVW
			currentInstruction = currentOpGroup->pushInstruction("Divide Word", "divw", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 491);
			// Operation: DIVWU
			currentInstruction = currentOpGroup->pushInstruction("Divide Word Unsigned", "divwu", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 459);

			// Operation: MULHW
			currentInstruction = currentOpGroup->pushInstruction("Multiply High Word", "mulhw", asmInstructionArgLayout::aIAL_IntArith3RegWithNoOEAndRC, 75);
			// Operation: MULHWU
			currentInstruction = currentOpGroup->pushInstruction("Multiply High Word Unsigned", "mulhwu", asmInstructionArgLayout::aIAL_IntArith3RegWithNoOEAndRC, 11);
			// Operation: MULLW
			currentInstruction = currentOpGroup->pushInstruction("Multiply Low Word", "mullw", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 235);

			// Operation: SUBF
			currentInstruction = currentOpGroup->pushInstruction("Subtract From", "subf", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 40);
			// Operation: SUBFC
			currentInstruction = currentOpGroup->pushInstruction("Subtract From Carrying", "subfc", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 8);
			// Operation: SUBFE
			currentInstruction = currentOpGroup->pushInstruction("Subtract From Extended", "subfe", asmInstructionArgLayout::aIAL_IntArith3RegWithOEAndRC, 136);
			// Operation: SUBFME
			currentInstruction = currentOpGroup->pushInstruction("Subtract From Minus One Extended", "subfme", asmInstructionArgLayout::aIAL_IntArith2RegWithOEAndRC, 232);
			// Operation: SUBFZE
			currentInstruction = currentOpGroup->pushInstruction("Subtract From Zero Extended", "subfze", asmInstructionArgLayout::aIAL_IntArith2RegWithOEAndRC, 200);

			// Operation: NEG
			currentInstruction = currentOpGroup->pushInstruction("Negate", "neg", asmInstructionArgLayout::aIAL_IntArith2RegWithOEAndRC, 104);


			// Operation: LBZX, LBZUX
			currentInstruction = currentOpGroup->pushInstruction("Load Byte and Zero" + opName_IndexedString, "lbzx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 87);
			currentInstruction = currentOpGroup->pushInstruction("Load Byte and Zero" + opName_WithUpdateIndexedString, "lbzux", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 119);
			// Operation: LHZX, LHZUX
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word and Zero" + opName_IndexedString, "lhzx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 279);
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word and Zero" + opName_WithUpdateIndexedString, "lhzux", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 311);
			// Operation: LHAX, LHAUX
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word Algebraic" + opName_IndexedString, "lhax", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 343);
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word Algebraic" + opName_WithUpdateIndexedString, "lhaux", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 375);
			// Operation: LHBRX
			currentInstruction = currentOpGroup->pushInstruction("Load Half Word Byte-Reverse" + opName_IndexedString, "lhbrx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 790);
			// Operation: LWZX, LWZUX
			currentInstruction = currentOpGroup->pushInstruction("Load Word and Zero" + opName_IndexedString, "lwzx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 23);
			currentInstruction = currentOpGroup->pushInstruction("Load Word and Zero" + opName_WithUpdateIndexedString, "lwzux", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 55);
			// Operation: LWBRX
			currentInstruction = currentOpGroup->pushInstruction("Load Word Byte-Reverse" + opName_IndexedString, "lwbrx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 534);
			// Operation: LFDX, LFDUX
			currentInstruction = currentOpGroup->pushInstruction("Load Floating-Point Double" + opName_IndexedString, "lfdx", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 599);
			currentInstruction = currentOpGroup->pushInstruction("Load Floating-Point Double" + opName_WithUpdateIndexedString, "lfdux", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 631);
			// Operation: LFSX, LFSUX
			currentInstruction = currentOpGroup->pushInstruction("Load Floating-Point Single" + opName_IndexedString, "lfsx", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 535);
			currentInstruction = currentOpGroup->pushInstruction("Load Floating-Point Single" + opName_WithUpdateIndexedString, "lfsux", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 567);


			// Operation: STBX, STBUX
			currentInstruction = currentOpGroup->pushInstruction("Store Byte" + opName_IndexedString, "stbx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 215);
			currentInstruction = currentOpGroup->pushInstruction("Store Byte" + opName_WithUpdateIndexedString, "stbux", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 247);
			// Operation: STHX, STHUX
			currentInstruction = currentOpGroup->pushInstruction("Store Half Word" + opName_IndexedString, "sthx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 407);
			currentInstruction = currentOpGroup->pushInstruction("Store Half Word" + opName_WithUpdateIndexedString, "sthux", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 439);
			// Operation: STHBRX
			currentInstruction = currentOpGroup->pushInstruction("Store Half Word Byte-Reverse" + opName_WithUpdateIndexedString, "sthbrx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 918);
			// Operation: STWX, STWUX
			currentInstruction = currentOpGroup->pushInstruction("Store Word" + opName_IndexedString, "stwx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 151);
			currentInstruction = currentOpGroup->pushInstruction("Store Word" + opName_WithUpdateIndexedString, "stwux", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 183);
			// Operation: STWBRX
			currentInstruction = currentOpGroup->pushInstruction("Store Word Byte-Reverse" + opName_WithUpdateIndexedString, "stwbrx", asmInstructionArgLayout::aIAL_IntLoadStoreIdx, 662);
			// Operation: STFDX, STFDUX
			currentInstruction = currentOpGroup->pushInstruction("Store Floating-Point Double" + opName_IndexedString, "stfdx", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 727);
			currentInstruction = currentOpGroup->pushInstruction("Store Floating-Point Double" + opName_WithUpdateIndexedString, "stfdux", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 759);
			// Operation: STFSX, STFSUX
			currentInstruction = currentOpGroup->pushInstruction("Store Floating-Point Single" + opName_IndexedString, "stfsx", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 663);
			currentInstruction = currentOpGroup->pushInstruction("Store Floating-Point Single" + opName_WithUpdateIndexedString, "stfsux", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 695);
			// Operation: STFIWX
			currentInstruction = currentOpGroup->pushInstruction("Store Floating-Point as Integer Word Indexed", "stfiwx", asmInstructionArgLayout::aIAL_FltLoadStoreIndexed, 983);

			
			// Operation: AND, ANDC
			currentInstruction = currentOpGroup->pushInstruction("AND", "and", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 28);
			currentInstruction = currentOpGroup->pushInstruction("AND" + opName_WithComplString, "andc", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 60);
			// Operation: OR, ORC
			currentInstruction = currentOpGroup->pushInstruction("OR", "or", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 444);
			currentInstruction = currentOpGroup->pushInstruction("OR" + opName_WithComplString, "orc", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 412);
			// Operation: EQV
			currentInstruction = currentOpGroup->pushInstruction("Equivalent", "eqv", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 284);
			// Operation: NOR
			currentInstruction = currentOpGroup->pushInstruction("NOR", "nor", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 124);
			// Operation: XOR
			currentInstruction = currentOpGroup->pushInstruction("XOR", "xor", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 316);
			// Operation: NAND
			currentInstruction = currentOpGroup->pushInstruction("NAND", "nand", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 476);

			// Operation: SLW
			currentInstruction = currentOpGroup->pushInstruction("Shift Left Word", "slw", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 24);
			// Operation: SRW
			currentInstruction = currentOpGroup->pushInstruction("Shift Right Word", "srw", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 536);
			// Operation: SRAW
			currentInstruction = currentOpGroup->pushInstruction("Shift Right Algebraic Word", "sraw", asmInstructionArgLayout::aIAL_Int3RegSASwapWithRC, 792);
			// Operation: SRAWI
			currentInstruction = currentOpGroup->pushInstruction("Shift Right Algebraic Word Immediate", "srawi", asmInstructionArgLayout::aIAL_Int2RegSASwapWithSHAndRC, 824);

			// Operation: CNTLZW
			currentInstruction = currentOpGroup->pushInstruction("Count Leading Zeros Word", "cntlzw", asmInstructionArgLayout::aIAL_Int2RegSASwapWithRC, 26);
			// Operation: EXTSB
			currentInstruction = currentOpGroup->pushInstruction("Extend Sign Byte", "extsb", asmInstructionArgLayout::aIAL_Int2RegSASwapWithRC, 954);
			// Operation: EXTSH
			currentInstruction = currentOpGroup->pushInstruction("Extend Sign Half Word", "extsh", asmInstructionArgLayout::aIAL_Int2RegSASwapWithRC, 922);

			// Operation: LSWI, STSWI
			currentInstruction = currentOpGroup->pushInstruction("Load String Word Immediate", "lswi", asmInstructionArgLayout::aIAL_LSWI, 597);
			currentInstruction = currentOpGroup->pushInstruction("Store String Word Immediate", "stswi", asmInstructionArgLayout::aIAL_LSWI, 725);

			// Operation: CMPW, CMPLW
			currentInstruction = currentOpGroup->pushInstruction("Compare Word", "cmpw", asmInstructionArgLayout::aIAL_CMPW, 0);
			currentInstruction = currentOpGroup->pushInstruction("Compare Word Logical", "cmplw", asmInstructionArgLayout::aIAL_CMPW, 32);

			// Operation: MFSPR, MTSPR
			currentInstruction = currentOpGroup->pushInstruction("Move from Special-Purpose Register", "mfspr", asmInstructionArgLayout::aIAL_MoveToFromSPReg, 339);
			currentInstruction = currentOpGroup->pushInstruction("Move to Special-Purpose Register", "mtspr", asmInstructionArgLayout::aIAL_MoveToFromSPReg, 467);

			// Operation: MFMSR, MTMSR, MFCR
			currentInstruction = currentOpGroup->pushInstruction("Move from Machine State Register", "mfmsr", asmInstructionArgLayout::aIAL_MoveToFromMSReg, 83);
			currentInstruction = currentOpGroup->pushInstruction("Move to Machine State Register", "mtmsr", asmInstructionArgLayout::aIAL_MoveToFromMSReg, 146);
			currentInstruction = currentOpGroup->pushInstruction("Move from Condition Register", "mfcr", asmInstructionArgLayout::aIAL_MoveToFromMSReg, 19);

			// Operation: MFSR, MTSR
			currentInstruction = currentOpGroup->pushInstruction("Move from Segment Register", "mfsr", asmInstructionArgLayout::aIAL_MoveToFromSegReg, 595);
			currentInstruction = currentOpGroup->pushInstruction("Move to Segment Register", "mtsr", asmInstructionArgLayout::aIAL_MoveToFromSegReg, 210);

			// Operation: MFSRIN, MTSRIN
			currentInstruction = currentOpGroup->pushInstruction("Move from Segment Register Indirect", "mfsrin", asmInstructionArgLayout::aIAL_MoveToFromSegInReg, 659);
			currentInstruction = currentOpGroup->pushInstruction("Move to Segment Register Indirect", "mtsrin", asmInstructionArgLayout::aIAL_MoveToFromSegInReg, 242);

			// Operation: DCBF, DCBI
			currentInstruction = currentOpGroup->pushInstruction("Data Cache Block Flush", "dcbf", asmInstructionArgLayout::aIAL_DataCache3RegOmitD, 86);
			currentInstruction = currentOpGroup->pushInstruction("Data Cache Block Invalidate", "dcbi", asmInstructionArgLayout::aIAL_DataCache3RegOmitD, 470);
			// Operation: DCBST, DCBT, DCBTST, DCBZ
			currentInstruction = currentOpGroup->pushInstruction("Data Cache Block Store", "dcbst", asmInstructionArgLayout::aIAL_DataCache3RegOmitD, 54);
			currentInstruction = currentOpGroup->pushInstruction("Data Cache Block Touch", "dcbt", asmInstructionArgLayout::aIAL_DataCache3RegOmitD, 278);
			currentInstruction = currentOpGroup->pushInstruction("Data Cache Block Touch for Store", "dcbtst", asmInstructionArgLayout::aIAL_DataCache3RegOmitD, 246);
			currentInstruction = currentOpGroup->pushInstruction("Data Cache Block Clear to Zero", "dcbz", asmInstructionArgLayout::aIAL_DataCache3RegOmitD, 1014);
			// Operation:: ICBI
			currentInstruction = currentOpGroup->pushInstruction("Instruction Cache Block Invalidate", "icbi", asmInstructionArgLayout::aIAL_DataCache3RegOmitD, 982);

			// Operation: EIEIO, SYNC
			currentInstruction = currentOpGroup->pushInstruction("Enforce In-Order Execution of I/O", "eieio", asmInstructionArgLayout::aIAL_MemSyncNoReg, 854);
			currentInstruction = currentOpGroup->pushInstruction("Synchronize", "sync", asmInstructionArgLayout::aIAL_MemSyncNoReg, 598);

			// Operation: LWARX, LSWX, STSWX, ECIWX, ECOWX
			currentInstruction = currentOpGroup->pushInstruction("Load Word and Reserve" + opName_IndexedString, "lwarx", asmInstructionArgLayout::aIAL_Int3RegNoRC, 20);
			currentInstruction = currentOpGroup->pushInstruction("Load String Word" + opName_IndexedString, "lswx", asmInstructionArgLayout::aIAL_Int3RegNoRC, 533);
			currentInstruction = currentOpGroup->pushInstruction("Store String Word" + opName_IndexedString, "stswx", asmInstructionArgLayout::aIAL_Int3RegNoRC, 661);
			currentInstruction = currentOpGroup->pushInstruction("External Control In Word" + opName_IndexedString, "eciwx", asmInstructionArgLayout::aIAL_Int3RegNoRC, 310);
			currentInstruction = currentOpGroup->pushInstruction("External Control Out Word" + opName_IndexedString, "ecowx", asmInstructionArgLayout::aIAL_Int3RegNoRC, 438);
			// Operation: STWCX.
			currentInstruction = currentOpGroup->pushInstruction("Store Word Conditional" + opName_IndexedString, "stwcx.", asmInstructionArgLayout::aIAL_STWCX, 150);

			// Operation: MFTB
			currentInstruction = currentOpGroup->pushInstruction("Move from Time Base", "mftb", asmInstructionArgLayout::aIAL_MFTB, 371);

			// Operation: MCRXR
			currentInstruction = currentOpGroup->pushInstruction("Move to Condition Register from XER", "mcrxr", asmInstructionArgLayout::aIAL_MCRXR, 512);

			// Operation: MTCRF
			currentInstruction = currentOpGroup->pushInstruction("Move to Condition Register Fields", "mtcrf", asmInstructionArgLayout::aIAL_MTCRF, 144);

			// Operation: TW
			currentInstruction = currentOpGroup->pushInstruction("Trap Word", "tw", asmInstructionArgLayout::aIAL_TW, 4);

			// Operation: TLBIE, TLBSYNC
			currentInstruction = currentOpGroup->pushInstruction("Translation Lookaside Buffer Invalidate Entry", "tlbie", asmInstructionArgLayout::aIAL_TLBIE, 306);
			currentInstruction = currentOpGroup->pushInstruction("TLB Synchronize", "tlbsync", asmInstructionArgLayout::aIAL_RFI, 566);
		}


		// Paired Single Instructions
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_PS_GENERAL, 21, 10);
		{
			// Compare Instructions
			currentInstruction = currentOpGroup->pushInstruction("Paired Singles Compare Ordered High", "ps_cmpo0", asmInstructionArgLayout::aIAL_PairedSingleCompare, 32);
			currentInstruction = currentOpGroup->pushInstruction("Paired Singles Compare Ordered Low", "ps_cmpo1", asmInstructionArgLayout::aIAL_PairedSingleCompare, 96);
			currentInstruction = currentOpGroup->pushInstruction("Paired Singles Compare Unordered High", "ps_cmpu0", asmInstructionArgLayout::aIAL_PairedSingleCompare, 0);
			currentInstruction = currentOpGroup->pushInstruction("Paired Singles Compare Unordered Low", "ps_cmpu1", asmInstructionArgLayout::aIAL_PairedSingleCompare, 64);
			// Manip Instructions
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Negate", "ps_neg", asmInstructionArgLayout::aIAL_PairedSingle3RegOmitA, 40);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Move Register", "ps_mr", asmInstructionArgLayout::aIAL_PairedSingle3RegOmitA, 72);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Absolute Value", "ps_abs", asmInstructionArgLayout::aIAL_PairedSingle3RegOmitA, 264);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Negative Absolute Value", "ps_nabs", asmInstructionArgLayout::aIAL_PairedSingle3RegOmitA, 136);
			// Merge Instructions
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Merge High", "ps_merge00", asmInstructionArgLayout::aIAL_PairedSingle3Reg, 528);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Merge Direct", "ps_merge01", asmInstructionArgLayout::aIAL_PairedSingle3Reg, 560);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Merge Swapped", "ps_merge10", asmInstructionArgLayout::aIAL_PairedSingle3Reg, 592);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Merge Low", "ps_merge11", asmInstructionArgLayout::aIAL_PairedSingle3Reg, 624);
			// Data Cache Instructions
			currentInstruction = currentOpGroup->pushInstruction("Data Cache Block Set to Zero Locked", "dcbz_l", asmInstructionArgLayout::aIAL_DataCache3RegOmitD, 1014);

			// Math Instructions (Sec Op Starts at bit 26)
			currentOpGroup->secOpCodeStartsAndLengths.insert({ 26, 5 });
			// Full 4 Reg
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Vector Sum High", "ps_sum0", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 10);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Vector Sum Low", "ps_sum1", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 11);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Multiply-Add Scalar High", "ps_madds0", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 14);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Multiply-Add Scalar Low", "ps_madds1", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 15);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Select", "ps_sel", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 23);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Multiply-Add", "ps_madd", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 29);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Multiply-Subtract", "ps_msub", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 28);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Negative Multiply-Add", "ps_nmadd", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 31);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Negative Multiply-Subtract", "ps_nmsub", asmInstructionArgLayout::aIAL_PairedSingle4Reg, 30);
			// 4 Reg Omit B
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Multiply", "ps_mul", asmInstructionArgLayout::aIAL_PairedSingle4RegOmitB, 25);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Multiply Scalar High", "ps_muls0", asmInstructionArgLayout::aIAL_PairedSingle4RegOmitB, 12);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Multiply Scalar Low", "ps_muls1", asmInstructionArgLayout::aIAL_PairedSingle4RegOmitB, 13);
			// 4 Reg Omit C
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Add", "ps_add", asmInstructionArgLayout::aIAL_PairedSingle4RegOmitC, 21);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Subtract", "ps_sub", asmInstructionArgLayout::aIAL_PairedSingle4RegOmitC, 20);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Divide", "ps_div", asmInstructionArgLayout::aIAL_PairedSingle4RegOmitC, 18);
			// 4 Reg Omit AC
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Reciprocal Estimate", "ps_res", asmInstructionArgLayout::aIAL_PairedSingle4RegOmitAC, 24);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Reciprocal Square Root Estimate", "ps_rsqrte", asmInstructionArgLayout::aIAL_PairedSingle4RegOmitAC, 26);


			// Indexed LoadStore Instructions (Sec Op Starts at bit 25)
			currentOpGroup->secOpCodeStartsAndLengths.insert({ 25, 6 });
			// PSQ_LX, PSQ_LUX
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Quantized Load" + opName_IndexedString, "psq_lx", asmInstructionArgLayout::aIAL_PairedSingleQLoadStoreIdx, 6);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Quantized Load" + opName_WithUpdateIndexedString, "psq_lux", asmInstructionArgLayout::aIAL_PairedSingleQLoadStoreIdx, 38);
			// PSQ_STX, PSQ_STUX
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Quantized Store" + opName_IndexedString, "psq_stx", asmInstructionArgLayout::aIAL_PairedSingleQLoadStoreIdx, 7);
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Quantized Store" + opName_WithUpdateIndexedString, "psq_stux", asmInstructionArgLayout::aIAL_PairedSingleQLoadStoreIdx, 39);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_PSQ_L);
		{
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Quantized Load", "psq_l", asmInstructionArgLayout::aIAL_PairedSingleQLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_PSQ_LU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Quantized Load" + opName_WithUpdateString, "psq_lu", asmInstructionArgLayout::aIAL_PairedSingleQLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_PSQ_ST);
		{
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Quantized Store", "psq_st", asmInstructionArgLayout::aIAL_PairedSingleQLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_PSQ_STU);
		{
			currentInstruction = currentOpGroup->pushInstruction("Paired Single Quantized Store" + opName_WithUpdateString, "psq_stu", asmInstructionArgLayout::aIAL_PairedSingleQLoadStore);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_TWI);
		{
			currentInstruction = currentOpGroup->pushInstruction("Trap Word Immediate", "twi", asmInstructionArgLayout::aIAL_TWI);
		}
		currentOpGroup = pushOpCodeGroupToDict(asmPrimaryOpCodes::aPOC_SC);
		{
			currentInstruction = currentOpGroup->pushInstruction("System Call", "sc", asmInstructionArgLayout::aIAL_SC);
		}
		return;
	}
	asmInstruction* getInstructionPtrFromHex(unsigned long hexIn)
	{
		asmInstruction* result = nullptr;

		unsigned long opCode = extractInstructionArg(hexIn, 0, 6);
		if (instructionDictionary.find(opCode) != instructionDictionary.end())
		{
			asmPrOpCodeGroup* opCodeGroup = &instructionDictionary[opCode];

			if (opCodeGroup->secOpCodeStartsAndLengths.empty())
			{
				result = &opCodeGroup->secondaryOpCodeToInstructions.begin()->second;
			}
			else
			{
				unsigned short secondaryOpCode = USHRT_MAX;
				for (auto currPair = opCodeGroup->secOpCodeStartsAndLengths.cbegin(); result == nullptr && currPair != opCodeGroup->secOpCodeStartsAndLengths.cend(); currPair++)
				{
					secondaryOpCode = (unsigned short)extractInstructionArg(hexIn, currPair->first, currPair->second);
					if (opCodeGroup->secondaryOpCodeToInstructions.find(secondaryOpCode) != opCodeGroup->secondaryOpCodeToInstructions.end())
					{
						result = &opCodeGroup->secondaryOpCodeToInstructions[secondaryOpCode];
					}
				}
			}
		}

		return result;
	}
	std::string convertInstructionHexToString(unsigned long hexIn)
	{
		std::stringstream result;

		asmInstruction* targetInstruction = getInstructionPtrFromHex(hexIn);
		if (targetInstruction != nullptr && targetInstruction->getArgLayoutPtr()->validateReservedArgs(hexIn))
		{
			result << targetInstruction->getArgLayoutPtr()->conversionFunc(targetInstruction, hexIn);
		}

		return result.str();
	}
	std::vector<std::string> convertInstructionHexBlockToStrings(const std::vector<unsigned long>& hexVecIn, const std::set<std::string>& disallowedMnemonics, std::size_t refsNeededForBranchLabel, bool applyHexComments)
	{
		// Declare and pre-size the results vector.
		std::vector<std::string> result{};
		result.reserve(hexVecIn.size());

		// Do our analysis on the incoming block.
		currentBlockInfo.populateProfileFromHexBlock(hexVecIn);
		
		// Do initial conversion pass on the block.
		for (std::size_t i = 0; i < hexVecIn.size(); i++)
		{
			std::string conversion = convertInstructionHexToString(hexVecIn[i]);
			// If a conversion wasn't empty (ie. was successful)...
			if (!conversion.empty())
			{
				// ... but its mnemonic is disallowed...
				if (disallowedMnemonics.find(conversion.substr(0, conversion.find(' '))) != disallowedMnemonics.end())
				{
					// ... use word encoding instead, but keep the .
					conversion = lava::ppc::getStringWithComment("word 0x" + lava::numToHexStringWithPadding(hexVecIn[i], 8), conversion);
				}
			}
			// ... otherwise, if the conversion simply failed...
			else
			{
				// ... use word encoding.
				conversion = "word 0x" + lava::numToHexStringWithPadding(hexVecIn[i], 8);
			}
			// Push the result to our result vector.
			result.push_back(conversion);
		}

		// If branch labels aren't disabled, and there are enough lines that it's at least *possible* that we'd end up using a label, try to apply them.
		if ((refsNeededForBranchLabel != SIZE_MAX) && (hexVecIn.size() >= refsNeededForBranchLabel))
		{
			// For each existing Branch Event...
			for (auto i : currentBlockInfo.relativeBranchEventsMap)
			{
				// ... check if we're looking at a destination with enough references to trigger a label (or if one's being forced on). If so...
				bool doLabel = 0;
				
				if (i.second.labelCondition || i.second.incomingBranchStartIndices.size() >= refsNeededForBranchLabel)
				{
					// ... generate the name for our label:
					std::string labelName = "";

					switch (i.second.labelCondition)
					{
						case branchEventSummary::destinationLabelCondition::dlc_THRESHOLD:
						{
							labelName = "loc_0x" + lava::numToHexStringWithPadding(i.first, 3);
							break; 
						}
						case branchEventSummary::destinationLabelCondition::dlc_DATA_EMBED:
						{
							labelName = "data_0x" + lava::numToHexStringWithPadding(i.first, 3);
							break;
						}
						case branchEventSummary::destinationLabelCondition::dlc_END_BRANCH:
						{ 
							labelName = "%END%";
							break;
						}
						default:
						{
							break;
						}
					}
					
					// Then, prepend that name to the destination line.
					// Note: we're delimiting with a newline here, so if these lines need to be printed with a prefix, that
					// needs to be handled on the output side, like the PPC Conversion predicate in lavaGeckoHexConvert does.
					result[i.first] = labelName + ":\n" + result[i.first];
					// Additionally, find each of the branch lines themselves...
					for (auto u : i.second.incomingBranchStartIndices)
					{
						// ... and overwrite the immediate branch with our branch label.
						result[u] = result[u].substr(0, result[u].find_last_of(' ')) + " " + labelName;
					}
				}
			}
		}

		// Lastly, if the conversion was successful, and hex comments were requested... 
		if (applyHexComments)
		{
			// ... iterate through every line of the output...
			for (std::size_t i = 0; i < result.size(); i++)
			{
				// ... and if a given line isn't using embed encoding...
				if (result[i].find("word 0x") != 0x00)
				{
					// ... append a comment to it with the hex encoding of the line.
					// Note: Lines with branch destinations prepended to them need to have their relativeCommentPosition adjusted to account
					// for the destination tag's length; otherwise the comment appears too close to the end of the instruction line itself.
					// We're doing that here by strategically making use of an unsigned integer overflow lol:
					//	- If we prepended a branch destination line: relativeCommentPosition adjustment should equal length of that prepended bit
					//	- If we *didn't*: relativeCommentPosition adjustment should be 0
					//	- We delimit the destination tag with a '\n' character, and we know for certain that character is never used otherwise
					//	- Additionally, .find() returns std::string::npos (ULONG_MAX) if it fails to find the search term, and ULONG_MAX + 1 == 0
					//	- We can take advantage of that, by adding 1 to the return value of .find('\n')!
					//		- If there *is* a newline, then we push the comment out by exactly the right length
					//		- If there *isn't* a newline, then we don't push the comment out *at all*!
					result[i] = lava::ppc::getStringWithComment(result[i], "0x" + lava::numToHexStringWithPadding(hexVecIn[i], 8), 0x20 + (result[i].find('\n') + 1));
				}
			}
		}

		// Lastly, if data embeds weren't disabled, go through and write those in.
		if (!currentBlockInfo.disableDataEmbedOutput)
		{
			// For each embed...
			for (auto i = currentBlockInfo.dataEmbedStartsToLengths.begin(); i != currentBlockInfo.dataEmbedStartsToLengths.end(); i++)
			{
				// Get the relevant slice of our vector...
				std::vector<unsigned long> embedHexVec(hexVecIn.begin() + i->first, hexVecIn.begin() + i->first + i->second);
				// ... use it to generate the formatted output for our embed...
				std::vector<std::string> formattedEmbedVec = formatRawDataEmbedOutput(embedHexVec, "", "word 0x", 1, 0x2C,
					"DATA_EMBED (0x" + lava::numToHexStringWithPadding(embedHexVec.size() * 4, 0) + " bytes)");
				// ... and write each line of it to the results vector.
				for (std::size_t u = 0; u < i->second; u++)
				{
					result[i->first + u] = formattedEmbedVec[u];
				}
			}
		}

		// And lastly, if our block was Gecko Encoded, with the code-final 0x00
		if (currentBlockInfo.blockIsGeckoCompliant)
		{
			// ...then we can drop that 0x00 from the output.
			result.resize(result.size() - 1);
		}

		return result;
	}
	bool summarizeInstructionDictionary(std::ostream& output)
	{
		bool result = 0;

		if (output.good())
		{
			output << "PowerPC Assembly Instruction Dictionary - lavaASMHexConvert\n\n";
			output << "Definitions (" << instructionCount << " Instruction(s)):\n";

			argumentLayout* currInstrLayout = nullptr;
			unsigned long currInstrTestHex = ULONG_MAX;
			for (auto i = instructionDictionary.cbegin(); i != instructionDictionary.end(); i++)
			{
				for (auto u = i->second.secondaryOpCodeToInstructions.cbegin(); u != i->second.secondaryOpCodeToInstructions.end(); u++)
				{
					output << "\t";
					currInstrLayout = u->second.getArgLayoutPtr();

					output << "[" << i->first;
					if (u->second.secondaryOpCode != USHRT_MAX)
					{
						output << ", " << u->first;
					}
					output << "] ";
					output << u->second.mnemonic << " (" << u->second.name << ") [0x" << std::hex << u->second.canonForm << std::dec << "]";
					currInstrTestHex = u->second.getTestHex();
					output << " {Ex: " << convertInstructionHexToString(currInstrTestHex) << "\t# 0x" << std::hex << currInstrTestHex << std::dec << "}\n";
					output << "\t\tArgs: " << u->second.getArgLayoutString() << "\n";
				}
			}

			result = output.good();
		}

		return result;
	}
	bool summarizeInstructionDictionary(std::string outputFilepath)
	{
		bool result = 0;

		std::ofstream output(outputFilepath, std::ios_base::out);
		if (output.is_open())
		{
			result = summarizeInstructionDictionary(output);
		}

		return result;
	}

	// MAP File Processing
	std::map<unsigned long, mapSymbol> mapSymbolStartsToStructs{};
	mapSymbol::mapSymbol(std::string symbolNameIn, unsigned long physicalAddrIn, unsigned long symbolSizeIn, unsigned long virtualAddrIn, unsigned long fileOffIn, unsigned long alignValIn)
	{
		symbolName = symbolNameIn;
		physicalAddr = physicalAddrIn;
		symbolSize = symbolSizeIn;
		virtualAddr = (virtualAddrIn != ULONG_MAX) ? virtualAddrIn : physicalAddr;
		fileOff = fileOffIn;
		alignVal = alignValIn;
		physicalEnd = physicalAddr + symbolSize;
		virtualEnd = virtualAddr + symbolSize;
	};
	unsigned long mapSymbol::positionWithinSymbol(unsigned long addressIn)
	{
		unsigned long result = ULONG_MAX;

		if (addressIn >= virtualAddr && addressIn < virtualEnd)
		{
			result = addressIn - virtualAddr;
		}

		return result;
	}

	bool parseMapFile(std::istream& inputStreamIn)
	{
		bool result = 0;

		if (inputStreamIn.good())
		{
			std::string currentLine("");
			std::string commentChars = "/#";
			while (std::getline(inputStreamIn, currentLine))
			{
				// Skip if the line is empty
				if (currentLine.empty()) continue;

				// Scrub leading and trailing whitespace chars
				currentLine = currentLine.substr(currentLine.find_first_not_of(" \t"));
				currentLine = currentLine.substr(0, currentLine.find_last_not_of(" \t") + 1);
				// If the resulting line isn't either empty or commented out
				if (!currentLine.empty() && commentChars.find(currentLine[0]) == std::string::npos)
				{
					// Split line into space-delimited segments
					// Also, we allow up to seven segments because that allows for the max of four primary numerical args, and the optional alignment value,
					// then additionally (in the event it has spaces in it) for the first string-delimited segment of what would be the symbol name to be
					// split from the remainder of that string. This is desirable because sometimes additional information is included past the symbol name
					// which doesn't matter for our purposes.
					std::vector<std::string> lineSegments = lava::splitString(currentLine, " ", 7);

					// Record all the numerically parse-able arguments in the line.
					std::vector<unsigned long> parsedNumSegments{};
					for (std::size_t i = 0; i < lineSegments.size(); i++)
					{
						// Try parsing the number...
						std::size_t parsedNum = lava::stringToNum<unsigned long>(lineSegments[i], 0, ULONG_MAX, 1);
						// ... and if it parses successfully...
						if (parsedNum != ULONG_MAX)
						{
							// ... record it in the segments vector.
							parsedNumSegments.push_back(parsedNum);
						}
						else
						{
							// Quit once we find a non-numeric argument.
							break;
						}
					}

					// Skip if there aren't at least 2 numeric elements, that's the minimum allowed in a map file format.
					// Also require that the number of validly parsed numbers is less than the number of segments in the line, since
					// we require that a name string be present at the end of the line, which shouldn't be parseable as a number.
					if (parsedNumSegments.size() < 2 && parsedNumSegments.size() < lineSegments.size()) continue;

					// Assign values based on the number of numeric elements *not including* the alignment value
					bool hasAlignmentVal = parsedNumSegments.size() > 2 && parsedNumSegments.back() <= 0xF;
					unsigned long effectiveColumnCount = parsedNumSegments.size() - hasAlignmentVal;
					unsigned long physicalAddr = parsedNumSegments[0];
					unsigned long symbolSize = parsedNumSegments[1];
					unsigned long virtualAddr = (effectiveColumnCount > 2) ? parsedNumSegments[2] : ULONG_MAX;
					unsigned long fileOff = (effectiveColumnCount > 3) ? parsedNumSegments[3] : ULONG_MAX;
					unsigned long alignmentVal = (hasAlignmentVal) ? parsedNumSegments.back() : ULONG_MAX;
					// Store the resulting values in the map.
					mapSymbol symbObj(lineSegments[parsedNumSegments.size()], physicalAddr, symbolSize, virtualAddr, fileOff, alignmentVal);
					mapSymbolStartsToStructs[symbObj.virtualAddr] = symbObj;
				}
			}

			result = !mapSymbolStartsToStructs.empty();
		}

		return result;
	}
	bool parseMapFile(std::string filepathIn)
	{
		bool result = 0;

		std::ifstream inputStream(filepathIn);
		if (inputStream.is_open())
		{
			result = parseMapFile(inputStream);
		}

		return result;
	}
	mapSymbol* getSymbolFromAddress(unsigned long addressIn)
	{
		mapSymbol* result = nullptr;

		if (!mapSymbolStartsToStructs.empty())
		{
			// Get the first symbol that starts either at or after the address we're looking for.
			auto nextHighestItr = mapSymbolStartsToStructs.lower_bound(addressIn);
			// If we find a symbol, and that symbol starts *after* our address, and there's a symbol before this one...
			if ((nextHighestItr != mapSymbolStartsToStructs.end()) && 
				(nextHighestItr->first > addressIn) &&
				(nextHighestItr != mapSymbolStartsToStructs.begin()))
			{
				// ... then move backwards to the previous symbol.
				nextHighestItr--;
			}
			if ((nextHighestItr != mapSymbolStartsToStructs.end()) && (nextHighestItr->second.positionWithinSymbol(addressIn) != ULONG_MAX))
			{
				// If so, that's our result.
				result = &nextHighestItr->second;
			}
		}

		return result;
	}
}