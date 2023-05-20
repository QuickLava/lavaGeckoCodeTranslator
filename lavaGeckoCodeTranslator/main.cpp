#include "stdafx.h"
#include "lavaGeckoCodeTranslator.h"

const std::string version = "v1.0.0";
const std::string suffixString = "_conv";

int main(int argc, char** argv)
{
	std::cout << "lavaGeckoCodeTranslator " << version << "\n";
	std::cout << "Written by QuickLava\n\n";
	if (argc > 1)
	{
		lava::ppc::buildInstructionDictionary();
		lava::gecko::buildGeckoCodeDictionary();

		std::string outputPath("");
		for (unsigned long i = 1; i < argc; i++)
		{
			std::cout << "Translating \"" << argv[i] << "\"... ";

			outputPath = lava::applyFilenameSuffix(argv[i], suffixString);
			if (lava::translateFile(argv[i], outputPath, 0))
			{
				std::cout << "Success!\n";
				std::cout << "\tTranslation written to \"" << outputPath << "\"!\n";
			}
			else
			{
				std::cerr << "Failure!\n";
				std::cerr << "\tPlease ensure that the specified file exists, and that \"" << outputPath << "\" can be written to!\n";
			}
			std::cout << "\n";
		}
	}
	else
	{
		std::cout << "Usage: Drag files to translate onto this executable, or pass their file paths as command line arguments.\n";
		std::cout << "Translated results are placed in the same directory as their source files, using the same name (suffixed with \"" << suffixString << "\").\n";
	}
	return 0;
}