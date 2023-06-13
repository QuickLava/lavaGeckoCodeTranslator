#ifndef ADDITIONAL_CODE_UTIL_H
#define ADDITIONAL_CODE_UTIL_H

#include "stdafx.h"
#include <iomanip>
#include <type_traits>
#include <sstream>

namespace lava
{
	// General Utility
	struct decConvStream
	{
		std::stringstream buf;
		decConvStream();
	};
	struct hexConvStream
	{
		std::stringstream buf;
		hexConvStream();
	};
	struct fltConvStream
	{
		std::stringstream buf;
		fltConvStream();
	};

	template <typename numType>
	numType ___stringToNumImpl(const char* stringIn, char** res, int base, std::true_type)
	{
		return (numType)std::strtoll(stringIn, res, base);
	}
	template <typename numType>
	numType ___stringToNumImpl(const char* stringIn, char** res, int base, std::false_type)
	{
		return (numType)std::strtoull(stringIn, res, base);
	}
	template <typename numType>
	numType stringToNum(const std::string& stringIn, bool allowNeg, numType defaultVal, bool forceHex = 0)
	{
		static_assert(std::is_integral<numType>::value == 1, "Type must be an integer primitve.");

		numType result = defaultVal;

		if (!stringIn.empty())
		{
			std::size_t firstNonWhitespaceCharFound = SIZE_MAX;
			for (std::size_t i = 0; i < stringIn.size() && firstNonWhitespaceCharFound == SIZE_MAX; i++)
			{
				if (!isblank(static_cast<unsigned char>(stringIn[i])))
				{
					firstNonWhitespaceCharFound = i;
				}
			}
			if (firstNonWhitespaceCharFound != SIZE_MAX)
			{
				int base = (forceHex || stringIn.find("0x") == firstNonWhitespaceCharFound) ? 16 : 10;
				char* res = nullptr;
				result = ___stringToNumImpl<numType>(stringIn.c_str(), &res, base, std::is_signed<numType>());
				if (res != (stringIn.c_str() + stringIn.size()))
				{
					result = defaultVal;
				}
				if (result < 0 && !allowNeg)
				{
					result = defaultVal;
				}
			}
		}

		return result;
	}
	template <typename numType>
	std::string numToHexStringWithPadding(numType numIn, unsigned char paddingLength)
	{
		static_assert(std::is_integral<numType>::value, "Template type must be a valid integer primitve.");

		static hexConvStream conv;
		conv.buf.str("");
		conv.buf << std::setw(paddingLength) << +numIn;
		return conv.buf.str();
	}
	template <typename numType>
	std::string numToDecStringWithPadding(numType numIn, unsigned char paddingLength)
	{
		static_assert(std::is_integral<numType>::value, "Template type must be a valid integer primitve.");

		static decConvStream conv;
		conv.buf.str("");
		conv.buf << std::setw(paddingLength) << +numIn;
		return conv.buf.str();
	}
	std::string doubleToStringWithPadding(double dblIn, unsigned char paddingLength, unsigned long precisionIn = 2);
	std::string floatToStringWithPadding(float fltIn, unsigned char paddingLength, unsigned long precisionIn = 2);
	bool readNCharsFromStream(char* destination, std::istream& source, std::size_t numToRead, bool resetStreamPos = 0);
	bool readNCharsFromStream(std::string& destination, std::istream& source, std::size_t numToRead, bool resetStreamPos = 0);
	bool readNCharsFromStream(std::vector<unsigned char>& destination, std::istream& source, std::size_t numToRead, bool resetStreamPos = 0);
	bool readNCharsFromStream(std::vector<char>& destination, std::istream& source, std::size_t numToRead, bool resetStreamPos = 0);
	template <typename numType>
	numType padLengthTo(numType lengthIn, numType padTo, bool allowZeroPaddingLength = 1)
	{
		numType padLength = padTo - (lengthIn % padTo);
		if (allowZeroPaddingLength && padLength == padTo)
		{
			padLength = 0x00;
		}
		return lengthIn + padLength;
	}
	std::vector<std::string> splitString(const std::string& sourceStr, std::string delimiter, std::size_t maxSplits = SIZE_MAX);
}

#endif
