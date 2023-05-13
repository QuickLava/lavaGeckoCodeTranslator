#ifndef LAVA_OUTPUT_BUNDLE_V1
#define LAVA_OUTPUT_BUNDLE_V1

#include "stdafx.h"
#include <map>
#include <memory>
#include <iostream>
#include <fstream>
#include "_AdditionalCode_Util.h"

namespace lava
{
	// lavaOutputSplitter v1.2.0 - A utility for managing output to multiple output streams at once.
	// OutputEntries:
	// - An object which contains a shared_ptr to an output stream, as well as a groupIdentifierMask
	// - Group identifier masks are bitfields which specifiy which "stream groups" an object belongs to.
	// - Each bit in the value should be considered a distinct group, such that setting a bit marks the entry as part of the corresponding group.
	// OutputSplitterss:
	// - An object which collects outputEntry objects in order to write to all of them at once.
	// - When invoking the write() function, the user may provide a Channel Mask value.
	//   - This value is used to determine which included outputEntries will be written to.
	//   - Each bit in this Channel Identifier Mask notes whether or not to write to outputEntries whose channel masks have that bit set.
	//   - That is to say, if the specified Group Identifier Mask bitwise ANDed together with an entry's Group Identifier is non-zero, it'll be written to.
	// - Additionally, the '<<' operator can be used to write to collected streams.
	//   - This operator can make use of channels as well through the use of the operatorChannelMask member.
	//   - Use the associated get and set functions, or pass a outputSplitterChannelMask object to the '<<' operator to set its value.
	//

	// Null Buffer
	extern std::ostream cnull;

	struct outputEntry
	{
		unsigned long channelMask = ULONG_MAX;
		std::shared_ptr<std::ostream> targetStream{};

		outputEntry(std::shared_ptr<std::ostream> targetStreamIn, unsigned long channelMaskIn = ULONG_MAX)
			: targetStream(targetStreamIn), channelMask(channelMaskIn) {};
		bool write(std::string content);
	};
	class outputSplitter
	{
	public:
		enum stdOutStreamEnum
		{
			sOS_NULL = -1,
			sOS_DISABLED = 0,
			sOS_COUT,
			sOS_CERR,
			sOS_CLOG,
		};

	private:
		std::map<unsigned long, outputEntry> targettedStreams{};
		stdOutStreamEnum standardOutput = stdOutStreamEnum::sOS_DISABLED;
		std::ostream* standardOutputStream = &cnull;
		unsigned long standardOutputStreamChannelMask = ULONG_MAX;

		// Channel Mask used when printing via the left-shift operator.
		unsigned long operatorChannelMask = ULONG_MAX;

		unsigned long generateUniqueStreamID();

	public:
		bool integerPrintMode = 0; // 0 = Dec, 1 = Hex
		unsigned long integerPrintPaddingLength = 0;
		unsigned long floatPrintPaddingLength = 0;
		unsigned long floatPrintPrecision = 2;

		outputSplitter(stdOutStreamEnum standardOutputStreamIn = stdOutStreamEnum::sOS_DISABLED);

		bool getAllStreamsGood();
		outputEntry* getOutputEntry(unsigned long streamIDIn);
		stdOutStreamEnum getStandardOutputStream();
		unsigned long getOperatorChannelMask();
		unsigned long getStandardOutputChannelMask();

		void setStandardOutputStream(stdOutStreamEnum outputStream);
		void setStandardOutputStreamChannelMask(unsigned long streamMask);
		void setOperatorChannelMask(unsigned long channelMaskIn);

		bool pushStream(outputEntry streamIn, unsigned long streamIDIn = ULONG_MAX, unsigned long* streamIDOut = nullptr);
		bool removeStream(unsigned long streamIDIn);

		// Note: Output to collected streams can be momentarily disabled by passing in a channelMask of 0.
		bool write(std::string content, unsigned long channelMask = ULONG_MAX, stdOutStreamEnum standardOutputRedirect = stdOutStreamEnum::sOS_NULL);

		template<typename T>
		friend outputSplitter& operator<<(outputSplitter& os, const T s);
	};

	struct outputSplitterIntMode
	{
		bool mode = 0; // Decimal, by default.
		outputSplitterIntMode(bool modeIn) : mode(modeIn) {};
	};
	struct outputSplitterChannelMask
	{
		unsigned long channelMask = 0b00;
		outputSplitterChannelMask(unsigned long channelMaskIn) :channelMask(channelMaskIn) {};
	};
	template<typename T>
	outputSplitter& operator<<(outputSplitter& os, const T s)
	{
		auto end = os.targettedStreams.end();

		for (auto itr = os.targettedStreams.begin(); itr != end; itr++)
		{
			if (itr->second.channelMask & os.operatorChannelMask)
			{
				*itr->second.targetStream << s;
			}
		}

		*os.standardOutputStream << s;

		return os;
	}
	template<>
	outputSplitter& operator<< <int>(outputSplitter& os, const int s);
	template<>
	outputSplitter& operator<< <unsigned int>(outputSplitter& os, const unsigned int s);
	template<>
	outputSplitter& operator<< <unsigned long>(outputSplitter& os, const unsigned long s);
	template<>
	outputSplitter& operator<< <float>(outputSplitter& os, const float s);
	template<>
	outputSplitter& operator<< <double>(outputSplitter& os, const double s);
	template<>
	outputSplitter& operator<< <outputSplitterChannelMask>(outputSplitter& os, const outputSplitterChannelMask s);
	template<>
	outputSplitter& operator<< <outputSplitterIntMode>(outputSplitter& os, const outputSplitterIntMode s);
	template<>
	outputSplitter& operator<< <outputSplitter::stdOutStreamEnum>(outputSplitter& os, const outputSplitter::stdOutStreamEnum s);
}

#endif