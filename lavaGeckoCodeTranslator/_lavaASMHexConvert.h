#ifndef LAVA_ASM_HEX_CONVERT_V1_H
#define LAVA_ASM_HEX_CONVERT_V1_H

#include "stdafx.h"
#include <functional>
#include <string>
#include <sstream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <fstream>
#include "_AdditionalCode_Util.h"

namespace lava::ppc
{
	// lavaASMHexConvert v1.0.5 - A utility for disassembling 32-bit IBM PowerPC Assembly Instruction Hex into readable PPC ASM.
	// This library provides support for the 32-bit PPC ASM instruction set, along with most additional instructions introduced by
	// or required by the Broadway instruction set (Paired Single instructions being the most significant addition from that set).
	// It offers no support for 64-bit exclusive instructions whatsoever, nor for any optional instructions not used in the Broadway
	// instruction set.
	// Based Directly on Info From:
	// - "PowerPC Microprocessor Family: The Programming Environments Manual for 32 and 64-bit Microprocessors (Version 2.3)" - IBM
	//		URL: http://refspecs.linux-foundation.org/PPC_hrm.2005mar31.pdf
	//		Used for: Primary reference for info on PowerPC Assembly instruction set and conventions.
	// - Dolphin Emulator (and associated Github Repository) - Dolphin-EMU Team
	//		URL: https://github.com/dolphin-emu/dolphin
	//		Used for: Validating disassembled output, as well as extra formatting reference (particularly with Paired Single instructions).
	// - ppc750cl: Rust tools for working with the PowerPC 750CL family of processors. - terorie
	//		URL: https://github.com/terorie/ppc750cl
	//		Used for: Primary reference for Broadway-specific instruction set.

	// Enums
	enum class asmPrimaryOpCodes
	{
		aPOC_NULL = -1,
		aPOC_31 = 31,
		// Branching Instructions
		aPOC_BC = 16,
		aPOC_B = 18,
		aPOC_B_SpReg = 19,
		// Comparison Instructions
		aPOC_CMPLWI = 10,
		aPOC_CMPWI = 11,
		// Arithmetic Instructions
		aPOC_MULLI = 7,
		aPOC_ADDIC = 12,
		aPOC_ADDIC_DOT = 13,
		aPOC_ADDI = 14,
		aPOC_ADDIS = 15,
		aPOC_SUBFIC = 8,
		aPOC_FLOAT_D_ARTH = 63,
		aPOC_FLOAT_S_ARTH = 59,
		// Load Instructions
		aPOC_LWZ = 32,
		aPOC_LWZU = 33,
		aPOC_LBZ = 34,
		aPOC_LBZU = 35,
		aPOC_LHZ = 40,
		aPOC_LHZU = 41,
		aPOC_LHA = 42,
		aPOC_LHAU = 43,
		aPOC_LMW = 46,
		aPOC_LFS = 48,
		aPOC_LFSU = 49,
		aPOC_LFD = 50,
		aPOC_LFDU = 51,
		// Store Instructions
		aPOC_STW = 36,
		aPOC_STWU = 37,
		aPOC_STB = 38,
		aPOC_STBU = 39,
		aPOC_STH = 44,
		aPOC_STHU = 45,
		aPOC_STMW = 47,
		aPOC_STFS = 52,
		aPOC_STFSU = 53,
		aPOC_STFD = 54,
		aPOC_STFDU = 55,
		// Logical Integer Instructions
		aPOC_ORI = 24,
		aPOC_ORIS = 25,
		aPOC_XORI = 26,
		aPOC_XORIS = 27,
		aPOC_ANDI = 28,
		aPOC_ANDIS = 29,
		// Bitwise Rotation Instructions
		aPOC_RLWIMI = 20,
		aPOC_RLWINM = 21,
		aPOC_RLWNM = 23,
		// Paired Single Instructions
		aPOC_PS_GENERAL = 4,
		aPOC_PSQ_L = 56,
		aPOC_PSQ_LU = 57,
		aPOC_PSQ_ST = 60,
		aPOC_PSQ_STU = 61,
		// Misc.
		aPOC_SC = 17,
		aPOC_TWI = 3,
	};
	enum class asmInstructionArgLayout
	{
		aIAL_NULL = -1,
		aIAL_B,
		aIAL_BC,
		aIAL_BCLR,
		aIAL_BCCTR,
		aIAL_CMPW,
		aIAL_CMPWI,
		aIAL_CMPLWI,
		aIAL_IntADDI,
		aIAL_IntORI,
		aIAL_IntLogicalIMM,
		aIAL_IntLoadStore,
		aIAL_IntLoadStoreIdx,
		aIAL_IntArith2RegWithOEAndRC,
		aIAL_IntArith3RegWithOEAndRC,
		aIAL_IntArith3RegWithNoOEAndRC,
		aIAL_Int2RegWithSIMM,
		aIAL_STWCX,
		aIAL_Int3RegNoRC,
		aIAL_Int3RegWithRC,
		aIAL_Int2RegSASwapWithRC,
		aIAL_Int3RegSASwapWithRC,
		aIAL_Int2RegSASwapWithSHAndRC,
		aIAL_LSWI,
		aIAL_RLWNM,
		aIAL_RLWINM,
		aIAL_FltCompare,
		aIAL_FltLoadStore,
		aIAL_FltLoadStoreIndexed,
		aIAL_Flt1RegWithRC,
		aIAL_Flt2RegOmitAWithRC,
		aIAL_Flt3RegOmitBWithRC,
		aIAL_Flt3RegOmitCWithRC,
		aIAL_Flt3RegOmitACWithRC,
		aIAL_Flt4RegBCSwapWithRC,
		aIAL_MoveToFromSPReg,
		aIAL_MoveToFromMSReg,
		aIAL_MoveToFromSegReg,
		aIAL_MoveToFromSegInReg,
		aIAL_ConditionRegLogicals,
		aIAL_ConditionRegMoveField,
		aIAL_PairedSingleCompare,
		aIAL_PairedSingleQLoadStore,
		aIAL_PairedSingleQLoadStoreIdx,
		aIAL_PairedSingle3Reg,
		aIAL_PairedSingle3RegOmitA,
		aIAL_PairedSingle4Reg,
		aIAL_PairedSingle4RegOmitB,
		aIAL_PairedSingle4RegOmitC,
		aIAL_PairedSingle4RegOmitAC,
		aIAL_DataCache3RegOmitD,
		aIAL_MemSyncNoReg,
		aIAL_MFTB,
		aIAL_MCRXR,
		aIAL_MTCRF,
		aIAL_MTFSB0,
		aIAL_MTFSF,
		aIAL_MTFSFI,
		aIAL_TW,
		aIAL_TWI,
		aIAL_RFI,
		aIAL_SC,
		aIAL_TLBIE,
		aIAL_LAYOUT_COUNT,
	};
	enum class asmInstructionArgResStatus
	{
		aIARS_NULL = -1,
		aIARS_MUST_BE_ZERO = 0,
		aIARS_MUST_BE_ONE = 1,
	};

	// Utility
	void printStringWithComment(std::ostream& outputStream, const std::string& primaryString, const std::string& commentString, bool printNewLine = 1, unsigned long relativeCommentLoc = 0x20, unsigned long commentIndentationLevel = 0x00);
	std::string getStringWithComment(const std::string& primaryString, const std::string& commentString, unsigned long relativeCommentLoc = 0x20, unsigned long commentIndentationLevel = 0x00);
	unsigned long extractInstructionArg(unsigned long hexIn, unsigned char startBitIndex, unsigned char length);
	unsigned long getInstructionOpCode(unsigned long hexIn);
	std::vector<std::string> formatRawDataEmbedOutput(const std::vector<unsigned long>& hexVecIn, std::string linePrefixIn, std::string wordPrefixIn, unsigned char wordsPerLineIn, unsigned long relativeLabelLoc, std::string labelStringIn);

	// Argument Layouts
	struct asmInstruction;
	// Default function, just returns mnemonic.
	std::string defaultAsmInstrToStrFunc(asmInstruction* instructionIn, unsigned long hexIn);
	struct argumentLayout
	{
		asmInstructionArgLayout layoutID = asmInstructionArgLayout::aIAL_NULL;
		std::vector<unsigned char> argumentStartBits{};
		unsigned long reservedZeroMask = 0;
		unsigned long reservedOneMask = 0;
		unsigned char secOpArgIndex = UCHAR_MAX;
		std::string(*conversionFunc)(asmInstruction*, unsigned long) = defaultAsmInstrToStrFunc;
		
		argumentLayout() {};

		// First is Reserved Zero Mask, Second is Reserved One Mask (If Bit == 1, Must Respect Reservation)
		void setArgumentReservations(std::vector<std::pair<char, asmInstructionArgResStatus>> reservationsIn);
		unsigned long getSecOpMask() const;
		bool validateReservedArgs(unsigned long instructionHexIn) const;
		std::vector<unsigned long> splitHexIntoArguments(unsigned long instructionHexIn) const;
		unsigned char getArgLengthInBits(unsigned char argIndex) const;
		asmInstructionArgResStatus getArgReservation(unsigned char argIndex) const;
	};
	extern std::array<argumentLayout, (int)asmInstructionArgLayout::aIAL_LAYOUT_COUNT> layoutDictionary;
	argumentLayout* defineArgLayout(asmInstructionArgLayout IDIn, std::vector<unsigned char> argStartsIn, 
		std::string(*convFuncIn)(asmInstruction*, unsigned long) = defaultAsmInstrToStrFunc);
	
	// Instructions
	struct asmInstruction
	{
		asmPrimaryOpCodes primaryOpCode = asmPrimaryOpCodes::aPOC_NULL;
		std::string name = "";
		std::string mnemonic = "";
		unsigned long canonForm = ULONG_MAX;
		unsigned short secondaryOpCode = USHRT_MAX;
		asmInstructionArgLayout layoutID = asmInstructionArgLayout::aIAL_NULL;

		asmInstruction() {};
		asmInstruction(asmPrimaryOpCodes prOpIn, std::string nameIn, std::string mnemIn, unsigned short secOpIn, unsigned long canonIn) :
			primaryOpCode(prOpIn), name(nameIn), mnemonic(mnemIn), secondaryOpCode(secOpIn), canonForm(canonIn) {};

		argumentLayout* getArgLayoutPtr() const;
		unsigned long getTestHex() const;
		std::string getArgLayoutString() const;
	};

	// Primary Op Code Groups
	struct asmPrOpCodeGroup
	{
	private:
		struct startLenPairCompare
		{
			bool operator() (std::pair<unsigned char, unsigned char> a, std::pair<unsigned char, unsigned char> b) const;
		};
	public:
		asmPrimaryOpCodes primaryOpCode = asmPrimaryOpCodes::aPOC_NULL;
		std::set<std::pair<unsigned char, unsigned char>, startLenPairCompare> secOpCodeStartsAndLengths;
		std::map<unsigned short, asmInstruction> secondaryOpCodeToInstructions{};

		asmPrOpCodeGroup() {};

		asmInstruction* pushInstruction(std::string nameIn, std::string mnemIn, asmInstructionArgLayout layoutIDIn, unsigned short secOpIn = USHRT_MAX);
	};

	// Dictionary
	extern std::map<unsigned short, asmPrOpCodeGroup> instructionDictionary;
	asmPrOpCodeGroup* pushOpCodeGroupToDict(asmPrimaryOpCodes opCodeIn, unsigned char secOpCodeStart = UCHAR_MAX, unsigned char secOpCodeLength = UCHAR_MAX);
	void buildInstructionDictionary();
	asmInstruction* getInstructionPtrFromHex(unsigned long hexIn);
	std::string convertInstructionHexToString(unsigned long hexIn);
	std::vector<std::string> convertInstructionHexBlockToStrings(const std::vector<unsigned long>& hexVecIn, const std::set<std::string>& disallowedMnemonics = {}, std::size_t refsNeededForBranchLabel = SIZE_MAX, bool applyHexComments = 0);
	bool summarizeInstructionDictionary(std::ostream& output);
	bool summarizeInstructionDictionary(std::string outputFilepath);

	// MAP File Processing
	struct mapSymbol
	{
		std::string symbolName = "";
		unsigned long physicalAddr = ULONG_MAX;
		unsigned long symbolSize = ULONG_MAX;
		unsigned long virtualAddr = ULONG_MAX;
		unsigned long fileOff = ULONG_MAX;
		unsigned long alignVal = ULONG_MAX;
		// Calc'd, not directly recorded in map files
		unsigned long physicalEnd = ULONG_MAX;
		unsigned long virtualEnd = ULONG_MAX;

		mapSymbol() {};
		mapSymbol(std::string symbolNameIn, unsigned long physicalAddrIn, unsigned long symbolSizeIn, unsigned long virtualAddrIn = ULONG_MAX, unsigned long fileOffIn = ULONG_MAX, unsigned long alignValIn = ULONG_MAX);

		unsigned long positionWithinSymbol(unsigned long addressIn);
	};
	extern std::map<unsigned long, mapSymbol> mapSymbolStartsToStructs;
	bool parseMapFile(std::istream& inputStreamIn);
	bool parseMapFile(std::string filepathIn);
	mapSymbol* getSymbolFromAddress(unsigned long addressIn);
}

#endif