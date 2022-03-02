
#include "crc32.h"

/*	CRC-32/zlib	*/

unsigned crc32table [256];

void crc32_init (void)
{
	int i, j;
	unsigned crc;

	for (i = 0; i < 256; i++) {
		crc = i;
		for (j = 0; j < 8; j++)
			crc = crc >> 1 ^ (crc & 1 ? 0xEDB88320UL : 0);
	        crc32table [i] = crc;
	}
}

unsigned _crc32_update (const unsigned char *p, unsigned len, unsigned crc /* = ~0 */)
{
	for (; len; len--)
		crc = crc >> 8 ^ crc32table [(unsigned char)crc ^ *p++];
	return crc;
}
