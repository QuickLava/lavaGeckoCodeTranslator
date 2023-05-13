#include "stdafx.h"
#include "lavaGeckoCodeTranslator.h"

namespace lava
{
	const std::string allowedNonHexChars = " \t*";

	bool translateFile(std::istream& inputStream, std::ostream& outputStream)
	{
		bool result = 0;

		if (inputStream.good() && outputStream.good())
		{
			std::string currentLine("");
			std::string codeString("");
			std::string commentString("");
			std::size_t commentStartLoc = SIZE_MAX;
			// Holds code to be translated.
			std::stringstream translationBufferIn("");
			// Holds result of translation.
			std::stringstream translationBufferOut("");

			while (std::getline(inputStream, currentLine))
			{
				// Disregard the current line if it's empty, or is marked as a comment
				if (currentLine.empty())
				{
					// If there is code to convert
					std::streampos bytesToTranslate = translationBufferIn.tellp();
					if (bytesToTranslate > 0)
					{
						translationBufferIn.seekg(0);
						translationBufferOut.str("");
						translationBufferOut.clear();
						lava::gecko::parseGeckoCode(translationBufferOut, translationBufferIn, bytesToTranslate);
						while (std::getline(translationBufferOut, currentLine))
						{
							outputStream << currentLine << "\n";
						}
						translationBufferIn.str("");
						translationBufferIn.clear();
					}
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
						currChar = currentLine[i];
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
							invalidHexCharFound = 1;
						}
					}

					if (!invalidHexCharFound)
					{
						if (codeString.size() % 8 == 0)
						{
							translationBufferIn << codeString;
						}
						else
						{
							outputStream << currentLine << "\n";
						}
					}
					else
					{
						outputStream << currentLine << "\n";
					}
				}
				else
				{
					outputStream << currentLine << "\n";
				}
			}
		}

		return result;
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
				std::filesystem::path sourceFilePathObj(inputFilepath);
				outputFilepath = sourceFilePathObj.parent_path().string() + "/" + sourceFilePathObj.stem().string() + outputFilepath + sourceFilePathObj.extension().string();
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