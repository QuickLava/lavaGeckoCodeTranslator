#include "stdafx.h"
#include "_AdditionalCode_Util.h"

namespace lava
{
	decConvStream::decConvStream() { buf << std::setfill('0'); };
	hexConvStream::hexConvStream() { buf << std::hex << std::uppercase << std::internal << std::setfill('0'); };
	fltConvStream::fltConvStream() { buf << std::fixed << std::showpoint << std::uppercase << std::internal << std::setfill('0'); };
	std::string doubleToStringWithPadding(double dblIn, unsigned char paddingLength, unsigned long precisionIn)
	{
		static fltConvStream conv;
		conv.buf.str("");
		conv.buf.precision(precisionIn);
		conv.buf << std::setw(paddingLength) << dblIn;
		return conv.buf.str();
	}
	std::string floatToStringWithPadding(float fltIn, unsigned char paddingLength, unsigned long precisionIn)
	{
		static fltConvStream conv;
		conv.buf.str("");
		conv.buf.precision(precisionIn);
		conv.buf << std::setw(paddingLength) << fltIn;
		return conv.buf.str();
	}
	bool readNCharsFromStream(char* destination, std::istream& source, std::size_t numToRead, bool resetStreamPos)
	{
		bool result = 0;

		if (source.good())
		{
			std::size_t originalPos = source.tellg();
			source.read(destination, numToRead);
			result = source.gcount() == numToRead;

			if (resetStreamPos)
			{
				source.seekg(originalPos);
				source.clear();
			}
		}

		return result;
	}
	bool readNCharsFromStream(std::string& destination, std::istream& source, std::size_t numToRead, bool resetStreamPos)
	{
		bool result = 0;

		if (source.good())
		{
			std::size_t originalPos = source.tellg();
			if (destination.size() < numToRead)
			{
				destination.resize(numToRead);
			}
			readNCharsFromStream(&destination[0], source, numToRead, resetStreamPos);
		}

		return result;
	}
	bool readNCharsFromStream(std::vector<char>& destination, std::istream& source, std::size_t numToRead, bool resetStreamPos)
	{
		bool result = 0;

		if (source.good())
		{
			std::size_t originalPos = source.tellg();
			if (destination.size() < numToRead)
			{
				destination.resize(numToRead);
			}
			readNCharsFromStream(&destination[0], source, numToRead, resetStreamPos);
		}

		return result;
	}
	bool readNCharsFromStream(std::vector<unsigned char>& destination, std::istream& source, std::size_t numToRead, bool resetStreamPos)
	{
		bool result = 0;

		if (source.good())
		{
			std::size_t originalPos = source.tellg();
			if (destination.size() < numToRead)
			{
				destination.resize(numToRead);
			}
			readNCharsFromStream((char*)&destination[0], source, numToRead, resetStreamPos);
		}

		return result;
	}
	std::vector<std::string> splitString(const std::string& sourceStr, std::string delimiter, std::size_t maxSplits)
	{
		std::vector<std::string> result{};

		// String View of the source string, to simplify and speed up splitting.
		std::string_view sourceView(&sourceStr[0], sourceStr.size());
		for (std::size_t splitCount = 0; !sourceView.empty() && splitCount < maxSplits; splitCount = result.size())
		{
			// Find the next occurence of the delimiter
			std::size_t delimLoc = sourceView.find(delimiter);
			// If we've found an instance of the delimiter, and this isn't the last split we're allowed to do...
			if (delimLoc < sourceView.size() && (splitCount + 1) < maxSplits)
			{
				// ... and the length of the delimited string would be greater than 0...
				if (delimLoc > 0)
				{
					// ... push the portion of the string preceding it to the results vector.
					result.push_back(std::string(sourceView.substr(0, delimLoc)));
				}
				// Then move the front of the view forwards past the end of the delimiter
				sourceView.remove_prefix(delimLoc + delimiter.size());
			}
			// Otherwise...
			else
			{
				// ... just push the rest of the view.
				result.push_back(std::string(sourceView));
				sourceView.remove_prefix(sourceView.size());
			}
		}

		return result;
	}
}
