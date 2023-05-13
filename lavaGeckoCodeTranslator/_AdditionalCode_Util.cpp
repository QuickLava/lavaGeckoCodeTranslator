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
}
