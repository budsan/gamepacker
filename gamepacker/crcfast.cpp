#include "crcfast.h"

//-CRC-16--------------------------------------------------------------------//

typedef unsigned short  crc;

#define POLYNOMIAL			0x8005
#define INITIAL_REMAINDER	0x0000
#define FINAL_XOR_VALUE		0x0000
#define REFLECT_DATA		TRUE
#define REFLECT_REMAINDER	TRUE

#define WIDTH  (8 * sizeof(crc))
#define TOPBIT (1 << (WIDTH - 1))

#if (REFLECT_DATA == TRUE)
#undef  REFLECT_DATA
#define REFLECT_DATA(X)			((unsigned char) reflect((X), 8))
#else
#undef  REFLECT_DATA
#define REFLECT_DATA(X)			(X)
#endif

#if (REFLECT_REMAINDER == TRUE)
#undef  REFLECT_REMAINDER
#define REFLECT_REMAINDER(X)	((crc) reflect((X), WIDTH))
#else
#undef  REFLECT_REMAINDER
#define REFLECT_REMAINDER(X)	(X)
#endif

crc crcFast::crcTable[256];
bool crcFast::init = false;

static unsigned long reflect(unsigned long data, unsigned char nBits)
{
	unsigned long reflection = 0x00000000;
	unsigned char bit;

	for (bit = 0; bit < nBits; ++bit) {
		if (data & 0x01)
			reflection |= (1 << ((nBits - 1) - bit));

		data = (data >> 1);
	}

	return (reflection);
}

void crcFast::crcInit()
{
	crc remainder;
	int			   dividend;
	unsigned char  bit;

	for (dividend = 0; dividend < 256; ++dividend)
	{
		remainder = dividend << (WIDTH - 8);
		for (bit = 8; bit > 0; --bit)
		{
			if (remainder & TOPBIT)
				remainder = (remainder << 1) ^ POLYNOMIAL;
			else
				remainder = (remainder << 1);
		}

		crcTable[dividend] = remainder;
	}

	init = true;
}

crcFast::crcFast() : remainder(INITIAL_REMAINDER)
{
	if (!init)
		crcInit();
}

void crcFast::Append(unsigned char byte)
{
	unsigned char data = REFLECT_DATA(byte) ^ (remainder >> (WIDTH - 8));
	remainder = crcTable[data] ^ (remainder << 8);
}

crc crcFast::CRC()
{
	return (REFLECT_REMAINDER(remainder) ^ FINAL_XOR_VALUE);
}