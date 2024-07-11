#include "stdafx.h"
#include "lavaGeckoCodeTranslator.h"
#include "whereami/whereami.h"

const std::string version = "v1.0.8";
const std::string suffixString = "_conv";
const std::string localMapFilePath = "./symbols.map";

bool tryParseLocalMapFile()
{
	bool result = 0;

	std::filesystem::path originalWorkingDir = std::filesystem::current_path();
	int length = wai_getModulePath(NULL, 0, NULL);
	std::string path(length, 0x00);
	wai_getModulePath(&path[0], length, NULL);
	if (path[0] != 0x00)
	{
		std::filesystem::current_path(std::filesystem::path(path).parent_path());
		if (std::filesystem::is_regular_file(localMapFilePath))
		{
			std::cout << "Symbol map file detected! Parsing \"" << localMapFilePath << "\"... ";
			if (lava::ppc::parseMapFile(localMapFilePath))
			{
				std::cout << "Success!\n";
				result = 1;
			}
			else
			{
				std::cerr << "Failure!\n";
			}
		}
		std::filesystem::current_path(originalWorkingDir);
	}

	return result;
}

int main(int argc, char** argv)
{
	std::cout << "lavaGeckoCodeTranslator " << version << "\n";
	std::cout << "Written by QuickLava\n\n";
	if (argc > 1)
	{
		lava::ppc::buildInstructionDictionary();
		lava::gecko::buildGeckoCodeDictionary();

		std::vector<unsigned long> argsParsedAsMaps{};
		for (unsigned long i = 1; i < argc; i++)
		{
			std::filesystem::path currArg(argv[i]);
			if (currArg.extension() == ".map")
			{
				std::cout << "Parsing \"" << argv[i] << "\"... ";
				if (lava::ppc::parseMapFile(argv[i]))
				{
					std::cout << "Success!\n";
				}
				else
				{
					std::cerr << "Failure!\n";
				}
				argsParsedAsMaps.push_back(i);
			}
		}

		if (argsParsedAsMaps.empty())
		{
			tryParseLocalMapFile();
		}

		for (unsigned long i = 1; i < argc; i++)
		{
			// Skip the argument if it's already been parsed as a .map file.
			if (std::find(argsParsedAsMaps.begin(), argsParsedAsMaps.end(), i) != argsParsedAsMaps.end())
			{
				continue;
			}

			std::cout << "Translating \"" << argv[i] << "\"... ";

			std::string outputPath("");
			if (lava::translateFile(argv[i], suffixString, 0, &outputPath))
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

		std::cout << "\nPress any key to exit.\n";
		_getch();
	}
	return 0;
}