
#ifndef __SOFAWARE_CRC_H__
#define __SOFAWARE_CRC_H__

void SWcrc_char(const char buf, unsigned int *crc);
int sofaware_calc_crc(ulong data, unsigned long len, unsigned int last_crc);

#endif /* #ifndef __SOFAWARE_CRC_H__ */

