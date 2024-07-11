#include "stdafx.h"
#include "lavaGeckoCodeTranslator.h"

namespace lava
{
	const std::string allowedNonHexChars = " \t*";

	std::string applyFilenameSuffix(std::filesystem::path filepath, std::string suffix)
	{
		return filepath.parent_path().string() + "/" + filepath.stem().string() + suffix + filepath.extension().string();
	}
	std::string applyFileExtension(std::filesystem::path filepath, std::string extension)
	{
		return filepath.replace_extension(extension).string();
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
	bool translateTextFile(std::istream& inputStream, std::ostream& outputStream)
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
	bool translateGCTFile(std::istream& inputStream, std::ostream& outputStream)
	{
		std::stringstream translationBuffer("");
		translationBuffer << std::hex;

		unsigned char _readBuf[4];
		unsigned long currentHex = ULONG_MAX;
		inputStream.seekg(0x8);
		while (inputStream.good())
		{
			inputStream.read((char*)&_readBuf[0], sizeof(_readBuf));
			currentHex = lava::bytesToFundamental<unsigned long>(&_readBuf[0]);
			translationBuffer << lava::numToHexStringWithPadding<unsigned long>(currentHex, 0x8);
		}

		std::size_t translationBufferLength = translationBuffer.tellp();
		lava::gecko::parseGeckoCode(outputStream, translationBuffer, translationBufferLength - 0x10, 1, 1, 1);
		return 1;
	}
	bool translateFile(const std::string& inputFilepath, const std::string& outputSuffix, bool useSuffixAsAbsoluteOutPath, std::string* pathOut)
	{
		bool result = 0;

		std::filesystem::path outputPath;
		if (useSuffixAsAbsoluteOutPath)
		{
			outputPath = outputSuffix;
		}
		else
		{
			outputPath = applyFilenameSuffix(inputFilepath, outputSuffix);
		}

		if (std::filesystem::is_regular_file(inputFilepath))
		{
			std::ifstream inputStream;
			std::ofstream outputStream;

			if (lava::copyStringToLower(std::filesystem::path(inputFilepath).extension().string()) == ".gct")
			{
				inputStream.open(inputFilepath, std::ios_base::in | std::ios_base::binary);
				if (!useSuffixAsAbsoluteOutPath)
				{
					outputPath = applyFileExtension(outputPath, ".txt");
				}
				outputStream.open(outputPath);
				if (outputStream.is_open() && inputStream.is_open())
				{
					outputStream << std::filesystem::path(inputFilepath).stem().string() << "\n\n";
					result = translateGCTFile(inputStream, outputStream);
				}
			}
			else
			{
				inputStream.open(inputFilepath);
				outputStream.open(outputPath);
				if (outputStream.is_open() && inputStream.is_open())
				{
					result = translateTextFile(inputStream, outputStream);
				}
			}
		}

		if (result)
		{
			*pathOut = outputPath.string();
		}
		else
		{
			*pathOut = "";
		}

		return result;
	}
}