#include "stdafx.h"
#include "lavaGeckoCodeTranslator.h"

int main()
{
	lava::ppc::buildInstructionDictionary();
	lava::gecko::buildGeckoCodeDictionary();
	lava::translateFile("./Junk/codeset.txt", "_conv", 1);
	return 0;
}