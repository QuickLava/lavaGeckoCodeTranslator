#include "stdafx.h"
#include "_lavaBytes.h"

namespace lava
{
	std::vector<unsigned char> streamContentsToVec(std::istream& streamIn)
	{
		std::vector<unsigned char> result;

		std::streampos currPos = streamIn.tellg();
		streamIn.seekg(0, std::ios::end);
		unsigned long streamLength = streamIn.tellg();
		streamIn.seekg(0, std::ios::beg);
		result.resize(streamLength, 0x00);
		streamIn.read((char*)result.data(), result.size());
		streamIn.seekg(currPos);

		return result;
	}

	template<>
	bool writeFundamentalToBuffer<float>(float objectIn, unsigned char* destinationBuffer, endType endianIn)
	{
		return writeFundamentalToBuffer<unsigned long>(*(unsigned long*)&objectIn, destinationBuffer, endianIn);
	}
	template<>
	bool writeFundamentalToBuffer<double>(double objectIn, unsigned char* destinationBuffer, endType endianIn)
	{
		return writeFundamentalToBuffer<unsigned long long>(*(unsigned long long*) & objectIn, destinationBuffer, endianIn);
	}
}