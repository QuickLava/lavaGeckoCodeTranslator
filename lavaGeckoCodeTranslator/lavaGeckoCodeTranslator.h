#ifndef LAVA_GECKO_CODE_TRANSLATOR_V1_H
#define LAVA_GECKO_CODE_TRANSLATOR_V1_H

#include "stdafx.h"
#include "_lavaGeckoHexConvert.h"

namespace lava
{
	bool translateTextFile(std::istream& inputStream, std::ostream& outputStream);
	bool translateGCTFile(std::istream& inputStream, std::ostream& outputStream);
	bool translateFile(const std::string& inputFilepath, const std::string& outputSuffix, bool useSuffixAsAbsoluteOutPath, std::string* pathOut = nullptr);
}

#endif