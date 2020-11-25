
#if defined (_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

#ifndef __CRC32_H
#define __CRC32_H

void crc32_init (void);
unsigned _crc32_update (const unsigned char *p, unsigned len, unsigned crc);

#endif /* __CRC32_H */
