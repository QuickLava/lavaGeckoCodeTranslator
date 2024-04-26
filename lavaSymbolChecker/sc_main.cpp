#include "../lavaGeckoCodeTranslator/stdafx.h"
#include "../lavaGeckoCodeTranslator/lavaGeckoCodeTranslator.h"
#include "../lavaGeckoCodeTranslator/whereami/whereami.h"

const std::string version = "v1.1.0";
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
void trySymbolLookup(unsigned long addressIn, std::ostream& outputStream = std::cout)
{
	if (addressIn != ULONG_MAX)
	{
		outputStream << "0x" << lava::numToHexStringWithPadding(addressIn, 0x8) << " ";

		lava::ppc::mapSymbol* targetMapSymbol = nullptr;
		if (targetMapSymbol = lava::ppc::getSymbolFromAddress(addressIn), targetMapSymbol != nullptr)
		{
			outputStream << "lands in symbol \"" << targetMapSymbol->symbolName << "\" @ 0x" << lava::numToHexStringWithPadding(targetMapSymbol->virtualAddr, 0x8) << "\n";
		}
		else
		{
			outputStream << "doesn't land in any known symbol!\n";
		}
	}
	else
	{
		outputStream << "Invalid Address, try again.\n";
	}
}

int main(int argc, char** argv)
{
	std::cout << "lavaSymbolChecker " << version << "\n";
	std::cout << "Written by QuickLava\n\n";

	if (tryParseLocalMapFile())
	{
		std::cout << "\n";
		std::string input("");
		unsigned long parsedInput = ULONG_MAX;
		lava::ppc::mapSymbol* targetMapSymbol = nullptr;

		if (argc > 1)
		{
			for (unsigned long i = 1; i < argc; i++)
			{
				trySymbolLookup(lava::stringToNum<unsigned long>(argv[i], 0, ULONG_MAX, 1));
				std::cout << "\n";
			}
			std::cout << "\nPress any key to close.\n";
			_getch();
		}
		else
		{
			while (1)
			{
				std::cout << "Enter Address to Look Up:\n";
				std::cin >> input;
				trySymbolLookup(lava::stringToNum<unsigned long>(input, 0, ULONG_MAX, 1));
				std::cout << "\n";
			}
		}
	}
	else
	{
		std::cerr << "Unable to parse map! Ensure a validly formatted \"" << localMapFilePath << "\" file exists in this folder and try again!\n";
	}

	std::cout << "\nPress any key to exit.\n";
	_getch();
	return 0;
}