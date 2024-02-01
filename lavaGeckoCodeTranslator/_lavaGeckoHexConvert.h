#ifndef GECKO_HEX_CONVERT_V1_H
#define GECKO_HEX_CONVERT_V1_H

#include "stdafx.h"
#include <cctype>
#include "_lavaBytes.h"
#include "_lavaASMHexConvert.h"

namespace lava::gecko
{
	// lavaGeckoHexConvert v1.0.5 - A utility for annotating raw Gecko hex code with descriptive comments, and converting that hex
	// into GCTRM compliant syntax where possible. Requires lavaASMHexConvert to function.
	// Based Directly on Info From:
	// - "WiiRD Code Database: Gecko Codetype Documentation"
	//		URL: https://wiigeckocodes.github.io/codetypedocumentation.html
	//		Mirror: https://web.archive.org/web/20200717160740/https://www.geckocodes.org/index.php?arsenal=1
	//		Used for: Primary reference for info on Gecko codetype formatting and function.

	enum class geckoPrimaryCodeTypes
	{
		gPCT_NULL = -1,
		gPCT_RAMWrite = 0x0,
		gPCT_If = 0x2,
		gPCT_BaseAddr = 0x4,
		gPCT_FlowControl = 0x6,
		gPCT_GeckoReg = 0x8,
		gPCT_RegAndCounterIf = 0xA,
		gPCT_Assembly = 0xC,
		gPCT_Misc = 0xE,
		gPCT_EndOfCodes = 0xF,
	};

	// Resets BA, PO, and Gecko Register values
	void resetParserDynamicValues(bool skipRegisterReset);
	// Resets Loop, Goto, and Date Embed tracking values
	void resetParserTrackingValues();

	struct geckoCodeType;
	std::size_t defaultGeckoCodeConv(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn);

	// Code Type
	struct geckoCodeType
	{
		geckoPrimaryCodeTypes primaryCodeType = geckoPrimaryCodeTypes::gPCT_NULL;
		std::string name = "";
		unsigned char secondaryCodeType = UCHAR_MAX;
		// Return Value is Number of Consumed Bytes
		std::size_t(*conversionFunc)(geckoCodeType* codeTypeIn, std::istream& codeStreamIn, std::ostream& outputStreamIn) = defaultGeckoCodeConv;
	};
	// Code Group
	struct geckoPrTypeGroup
	{
		geckoPrimaryCodeTypes primaryCodeType = geckoPrimaryCodeTypes::gPCT_NULL;
		std::map<unsigned char, geckoCodeType> secondaryCodeTypesToCodes{};

		geckoPrTypeGroup() {};

		geckoCodeType* pushInstruction(std::string nameIn, unsigned char secOpIn, std::size_t(*conversionFuncIn)(geckoCodeType*, std::istream&, std::ostream&));
	};
	// Dictionary
	extern std::map<unsigned char, geckoPrTypeGroup> geckoCodeDictionary;
	geckoPrTypeGroup* pushPrTypeGroupToDict(geckoPrimaryCodeTypes opCodeIn);
	void buildGeckoCodeDictionary();

	geckoCodeType* findRelevantGeckoCodeType(unsigned char primaryType, unsigned char secondaryType);
	// If resetDynamicValues is set to 1, BA, PO, and Gecko Register values will be reset to their default states before parsing.
	// If resetTrackingValues is set to 1, Loop, Goto, and Date Embed tracking values will also be reset before parsing.
	std::size_t parseGeckoCode(std::ostream& output, std::istream& codeStreamIn, std::size_t expectedLength, bool resetDynamicValues, bool resetTrackingValues, bool doPPCHexComments);
}

#endif