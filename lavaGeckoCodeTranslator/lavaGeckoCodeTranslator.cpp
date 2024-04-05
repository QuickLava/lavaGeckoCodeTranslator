#include "stdafx.h"
#include "lavaGeckoCodeTranslator.h"

namespace lava
{
	const std::string allowedNonHexChars = " \t*";

	std::string applyFilenameSuffix(std::string filepath, std::string suffix)
	{
		std::string result = "";

		std::filesystem::path sourceFilePathObj(filepath);
		result = sourceFilePathObj.parent_path().string() + "/" + sourceFilePathObj.stem().string() + suffix + sourceFilePathObj.extension().string();

		return result;
	}

	bool dumpTranslationBuffer(std::stringstream& translationBuffer, std::ostream& outputStream)
	{
		bool result = 0;

		// If there is code to convert
		std::streampos bytesToTranslate = translationBuffer.tellp();
		if (bytesToTranslate > 0)
		{
			result = 1;

			translationBuffer.seekg(0);
			lava::gecko::parseGeckoCode(outputStream, translationBuffer, bytesToTranslate, 0, 0, 1);
			translationBuffer.str("");
			translationBuffer.clear();
		}

		return result;
	}
	bool translateFile(std::istream& inputStream, std::ostream& outputStream)
	{
		std::size_t outputStrInitialPos = outputStream.tellp();

		if (inputStream.good() && outputStream.good())
		{
			std::string currentLine("");
			std::string codeString("");
			std::string commentString("");
			std::size_t commentStartLoc = SIZE_MAX;
			// Holds code to be translated.
			std::stringstream translationBuffer("");

			// For each file, reset dynamic and tracking values before translating.
			lava::gecko::resetParserDynamicValues(1);
			lava::gecko::resetParserTrackingValues();

			while (std::getline(inputStream, currentLine))
			{
				// Disregard the current line if it's empty, or is marked as a comment
				if (currentLine.empty())
				{
					dumpTranslationBuffer(translationBuffer, outputStream);
					outputStream << "\n";
				}
				else if (currentLine[0] != '#' && currentLine[0] != '/')
				{
					commentStartLoc = std::min(currentLine.find('#'), currentLine.find('/'));

					bool invalidHexCharFound = 0;
					char currChar = CHAR_MAX;
					codeString.clear();
					for (std::size_t i = 0; !invalidHexCharFound && i < currentLine.size() && i < commentStartLoc; i++)
					{
						// Convert to uppercase to make GCTRM directive parsing more consistent, see below.
						currChar = std::toupper(currentLine[i]);
						if (allowedNonHexChars.find(currChar) != std::string::npos)
						{
							// Do nothing.
						}
						else if (std::isxdigit(currentLine[i]))
						{
							codeString += currChar;
						}
						else
						{
							// Some GCTRM directives relevant to our parsing start with periods, check for those:
							if (currChar == '.')
							{
								// If we find a ".RESET" directive...
								if (currentLine.find(".RESET", i) == i)
								{
									// ... ensure we reset BA and PO.
									// Important cuz we have to keep track of those to ensure accurate locations for GCTRM
									// syntax commands that require absolute addresses (HOOKS, RAM Writes, etc.).
									lava::gecko::resetParserDynamicValues(1);
								}
							}
							invalidHexCharFound = 1;
						}
					}

					if (!invalidHexCharFound)
					{
						if (!codeString.empty() && codeString.size() % 8 == 0)
						{
							translationBuffer << codeString;
						}
						else
						{
							dumpTranslationBuffer(translationBuffer, outputStream);
							outputStream << currentLine << "\n";
						}
					}
					else
					{
						dumpTranslationBuffer(translationBuffer, outputStream);
						outputStream << currentLine << "\n";
					}
				}
				else
				{
					dumpTranslationBuffer(translationBuffer, outputStream);
					outputStream << currentLine << "\n";
				}
			}
			if (translationBuffer.tellp())
			{
				dumpTranslationBuffer(translationBuffer, outputStream);
			}
		}

		return outputStrInitialPos < outputStream.tellp();
	}
	bool translateFile(std::string inputFilepath, std::string outputFilepath, bool useOutpathAsFilenameSuffix)
	{
		bool result = 0;

		std::ifstream inputStream;
		inputStream.open(inputFilepath);
		if (inputStream.is_open())
		{
			if (useOutpathAsFilenameSuffix)
			{
				outputFilepath = applyFilenameSuffix(inputFilepath, outputFilepath);
			}

			std::ofstream outputStream;
			outputStream.open(outputFilepath);
			if (outputStream.is_open())
			{
				result = translateFile(inputStream, outputStream);
			}
		}

		return result;
	}
}