#include "stdafx.h"
#include "_lavaOutputSplitter.h"

namespace lava
{
	// Null output stream trick adapted from an answer on StackOverflow by Xeo:
	// - https://stackoverflow.com/a/6240980
	std::ostream cnull = std::ostream(0);

	bool outputEntry::write(std::string content)
	{
		*targetStream << content;
		return targetStream->good();
	}

	unsigned long outputSplitter::generateUniqueStreamID()
	{
		bool foundValidID = 0;
		unsigned long newID = 0x00;

		auto end = targettedStreams.cend();
		for (auto itr = targettedStreams.cbegin(); !foundValidID && itr != end; itr++)
		{
			if (newID != itr->first)
			{
				foundValidID = 1;
			}
			else
			{
				newID++;
			}
		}

		return newID;
	}
	outputSplitter::outputSplitter(stdOutStreamEnum standardOutputStreamIn)
	{
		setStandardOutputStream(standardOutputStreamIn);
	}

	bool outputSplitter::getAllStreamsGood()
	{
		bool result = 0;

		if (!targettedStreams.empty())
		{
			result = 1;
			auto end = targettedStreams.cend();
			for (auto itr = targettedStreams.cbegin(); itr != end; itr++)
			{
				result &= itr->second.targetStream->good();
			}
		}

		return result;
	}
	outputEntry* outputSplitter::getOutputEntry(unsigned long streamIDIn)
	{
		outputEntry* result = nullptr;

		auto itr = targettedStreams.find(streamIDIn);
		if (itr != targettedStreams.end())
		{
			result = &itr->second;
		}

		return result;
	}
	outputSplitter::stdOutStreamEnum outputSplitter::getStandardOutputStream()
	{
		return standardOutput;
	}
	unsigned long outputSplitter::getOperatorChannelMask()
	{
		return operatorChannelMask;
	}
	unsigned long outputSplitter::getStandardOutputChannelMask()
	{
		return standardOutputStreamChannelMask;
	}

	void outputSplitter::setStandardOutputStream(stdOutStreamEnum standardOutputStreamIn)
	{
		standardOutput = standardOutputStreamIn;
		switch (standardOutput)
		{
			case stdOutStreamEnum::sOS_COUT:
			{
				standardOutputStream = &std::cout;
				break;
			}
			case stdOutStreamEnum::sOS_CERR:
			{
				standardOutputStream = &std::cerr;
				break;
			}
			case stdOutStreamEnum::sOS_CLOG:
			{
				standardOutputStream = &std::clog;
				break;
			}
			case stdOutStreamEnum::sOS_DISABLED:
			{
				standardOutputStream = &lava::cnull;
				break;
			}
			default:
			{
				break;
			}
		}
	}
	void outputSplitter::setStandardOutputStreamChannelMask(unsigned long streamMask)
	{
		standardOutputStreamChannelMask = streamMask;
	}
	void outputSplitter::setOperatorChannelMask(unsigned long channelMaskIn)
	{
		operatorChannelMask = channelMaskIn;
	}

	bool outputSplitter::pushStream(outputEntry streamIn, unsigned long streamIDIn, unsigned long* streamIDOut)
	{
		bool result = 0;

		if (streamIDIn == ULONG_MAX)
		{
			streamIDIn = generateUniqueStreamID();
		}
		auto emplaceRes = targettedStreams.emplace(streamIDIn, streamIn);
		if (emplaceRes.second)
		{
			if (streamIDOut != nullptr)
			{
				*streamIDOut = streamIDIn;
			}
			result = 1;
		}
		else
		{
			if (streamIDOut != nullptr)
			{
				*streamIDOut = ULONG_MAX;
			}
		}

		return result;
	}

	bool outputSplitter::removeStream(unsigned long streamIDIn)
	{
		bool result = 0;

		auto findItr = targettedStreams.find(streamIDIn);
		if (findItr != targettedStreams.end())
		{
			targettedStreams.erase(streamIDIn);
			result = 1;
		}

		return result;
	}
	bool outputSplitter::write(std::string content, unsigned long channelMask, stdOutStreamEnum standardOutputRedirect)
	{
		bool result = 0;

		if (content.empty())
		{
			result = 1;
		}
		else if (!targettedStreams.empty())
		{
			result = 1;

			auto end = targettedStreams.end();
			for (auto itr = targettedStreams.begin(); itr != end; itr++)
			{
				if (channelMask & itr->second.channelMask)
				{
					*itr->second.targetStream << content;
				}
			}

			if ((standardOutputRedirect != stdOutStreamEnum::sOS_DISABLED) && (channelMask & standardOutputStreamChannelMask))
			{
				stdOutStreamEnum stdOutStreamBak = getStandardOutputStream();
				setStandardOutputStream(standardOutputRedirect);
				*standardOutputStream << content;
				setStandardOutputStream(stdOutStreamBak);
			}
		}

		return result;
	}


	template<>
	outputSplitter& operator<< <int>(outputSplitter& os, const int s)
	{
		if (os.integerPrintMode == 0)
		{
			os << lava::numToDecStringWithPadding((signed long long) s, os.integerPrintPaddingLength);
		}
		else
		{
			os << numToHexStringWithPadding(s, os.integerPrintPaddingLength);
		}
		return os;
	}
	template<>
	outputSplitter& operator<< <unsigned int>(outputSplitter& os, const  unsigned int s)
	{
		if (os.integerPrintMode == 0)
		{
			os << numToDecStringWithPadding((unsigned long long) s, os.integerPrintPaddingLength);
		}
		else
		{
			os << numToHexStringWithPadding(s, os.integerPrintPaddingLength);
		}
		return os;
	}
	template<>
	outputSplitter& operator<< <unsigned long>(outputSplitter& os, const  unsigned long s)
	{
		return operator<<(os, (unsigned int)s);
	}
	template<>
	outputSplitter& operator<< <float>(outputSplitter& os, const float s)
	{
		os << floatToStringWithPadding(s, os.floatPrintPaddingLength, os.floatPrintPrecision);
		return os;
	}
	template<>
	outputSplitter& operator<< <double>(outputSplitter& os, const double s)
	{
		os << doubleToStringWithPadding(s, os.floatPrintPaddingLength, os.floatPrintPrecision);
		return os;
	}
	template<>
	outputSplitter& operator<< <outputSplitterChannelMask>(outputSplitter& os, const outputSplitterChannelMask s)
	{
		os.setOperatorChannelMask(s.channelMask);
		return os;
	}
	template<>
	outputSplitter& operator<< <outputSplitterIntMode>(outputSplitter& os, const outputSplitterIntMode s)
	{
		os.integerPrintMode = s.mode;
		return os;
	}
	template<>
	outputSplitter& operator<< <outputSplitter::stdOutStreamEnum>(outputSplitter& os, const outputSplitter::stdOutStreamEnum s)
	{
		os.setStandardOutputStream(s);
		return os;
	}
}