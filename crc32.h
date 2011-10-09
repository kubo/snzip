#ifndef CRC32_H
#define CRC32_H 1
#include <stdint.h>

#define MASK_DELTA 0xa282ead8

uint32_t calculate_crc32c(uint32_t crc32c, const unsigned char *buffer,
			  unsigned int length);

static inline unsigned int masked_crc32c(const char *buf, size_t len)
{
  unsigned int crc = ~calculate_crc32c(~0, (const unsigned char *)buf, len);
  return ((crc >> 15) | (crc << 17)) + MASK_DELTA;
}

#endif /* CRC32_H */
