/*
 * crc7.c
 *
 *  Created on: 28 мая 2020 г.
 *      Author: spide
 */

#define CRC7_Poly 0x89

unsigned char MMC_CalcCRC7(unsigned char crc, unsigned char data)
{
	unsigned char treg = (crc | data);

	for (int BitCount = 0; BitCount <8; BitCount++)
	{
		if (treg & 0x80)
			treg ^= CRC7_Poly; // if carry out of CRC, then EOR with Poly (0x89)

		treg <<= 1; // Shift CRC-Data register left 1 bit
	}

	return treg;
}
