#ifndef LAVA_GECKO_CODE_TRANSLATOR_V1_H
#define LAVA_GECKO_CODE_TRANSLATOR_V1_H

#include "stdafx.h"
#include "_lavaGeckoHexConvert.h"

namespace lava
{
	std::string applyFilenameSuffix(std::string filepath, std::string suffix);
	bool translateFile(std::istream& inputStream, std::ostream& outputStream);
	bool translateFile(std::string inputFilepath, std::string outputFilepath, bool useOutpathAsFilenameSuffix);
}

#endif