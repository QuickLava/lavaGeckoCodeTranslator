#ifndef LAVA_BYTES_V1_H
#define LAVA_BYTES_V1_H

#include "stdafx.h"
#include <sstream>
#include <vector>

namespace lava
{
	enum class endType
	{
		et_NULL = 0,
		et_BIG_ENDIAN,
		et_LITTLE_ENDIAN,
	};

	std::vector<unsigned char> streamContentsToVec(std::istream& streamIn);

	template<typename objectType>
	bool writeFundamentalToBuffer(objectType objectIn, unsigned char* destinationBuffer, endType endianIn = endType::et_BIG_ENDIAN)
	{
		bool result = 0;
		if (destinationBuffer != nullptr)
		{
			unsigned long long objectProxy = objectIn;
			if (endianIn == endType::et_BIG_ENDIAN)
			{
				unsigned long shiftDistance = (sizeof(objectType) - 1) * 8;
				for (std::size_t i = 0; i < sizeof(objectType); i++)
				{
					*destinationBuffer = 0x000000FF & (objectProxy >> shiftDistance);
					destinationBuffer++;
					shiftDistance -= 8;
				}
			}
			else
			{
				unsigned long shiftDistance = 0;
				for (std::size_t i = 0; i < sizeof(objectType); i++)
				{
					*destinationBuffer = 0x000000FF & (objectProxy >> shiftDistance);
					destinationBuffer++;
					shiftDistance += 8;
				}
			}
		}
		return result;
	}
	template<>
	bool writeFundamentalToBuffer<float>(float objectIn, unsigned char* destinationBuffer, endType endianIn);
	template<>
	bool writeFundamentalToBuffer<double>(double objectIn, unsigned char* destinationBuffer, endType endianIn);

	template<typename objectType>
	std::vector<unsigned char> fundamentalToBytes(const objectType& objectIn, endType endianIn = endType::et_BIG_ENDIAN)
	{
		std::vector<unsigned char> result(sizeof(objectType), 0x00);
		writeFundamentalToBuffer(objectIn, result.data(), endianIn);
		return result;
	}
	template<typename objectType>
	objectType bytesToFundamental(const unsigned char* bytesIn, endType endianIn = endType::et_BIG_ENDIAN)
	{
		objectType result = 0;
		if (endianIn == endType::et_BIG_ENDIAN)
		{
			for (std::size_t i = 0; i < sizeof(objectType); i++)
			{
				result |= *bytesIn;
				if (i < (sizeof(objectType) - 1))
				{
					bytesIn++;
					result = result << 0x08;
				}
			}
		}
		else
		{
			bytesIn += sizeof(objectType) - 1;
			for (std::size_t i = 0; i < sizeof(objectType); i++)
			{
				result |= *bytesIn;
				if (i < (sizeof(objectType) - 1))
				{
					bytesIn--;
					result = result << 0x08;
				}
			}
		}
		return result;
	}

	template<typename objectType>
	bool writeRawDataToStream(std::ostream& out, const objectType& objectIn, endType endianIn = endType::et_BIG_ENDIAN)
	{
		unsigned char tempBuffer[sizeof(objectType)];
		writeFundamentalToBuffer(objectIn, tempBuffer, endianIn);
		out.write((char*)tempBuffer, sizeof(objectType));
		return out.good();
	}
	template<>
	inline bool writeRawDataToStream<char>(std::ostream& out, const char& objectIn, endType endianIn)
	{
		out.put(objectIn);
		return out.good();
	}

}

#endif