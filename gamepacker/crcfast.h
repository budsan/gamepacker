#pragma once

typedef unsigned short  crc;

struct crcFast
{
private:
	crc remainder;
	static crc crcTable[256];
	static bool init;

	static void crcInit();

public:
	crcFast();

	void Append(unsigned char byte);
	crc CRC();
};