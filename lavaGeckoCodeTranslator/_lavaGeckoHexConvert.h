#ifndef GECKO_HEX_CONVERT_V1_H
#define GECKO_HEX_CONVERT_V1_H

#include "stdafx.h"
#include <cctype>
#include "_lavaBytes.h"
#include "_lavaASMHexConvert.h"

namespace lava::gecko
{
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

	std::size_t parseGeckoCode(std::ostream& output, std::istream& codeStreamIn, std::size_t expectedLength);

}

#endif