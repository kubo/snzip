/* -*- indent-tabs-mode: nil -*-
 *
 * Copyright 2012-2013 Kubo Takehiro <kubo@jiubao.org>
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of the authors.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <snappy-c.h>
#include "snzip.h"
#include "crc32.h"

#define COMPRESSED_DATA_IDENTIFIER 0x00
#define UNCOMPRESSED_DATA_IDENTIFIER 0x01

/* 4.1. Stream identifier (0xff) */
static const char stream_header[10] = {0xff, 0x06, 0x00, 0x00, 0x73, 0x4e, 0x61, 0x50, 0x70, 0x59};

#define MAX_DATA_LEN 16777215 /* maximum chunk data length */
#define MAX_UNCOMPRESSED_DATA_LEN 65536 /* maximum uncompressed data length excluding checksum */

static int framing_format_compress(FILE *infp, FILE *outfp, size_t block_size)
{
  const size_t max_uncompressed_data_len = MAX_UNCOMPRESSED_DATA_LEN;
  const size_t max_compressed_data_len = snappy_max_compressed_length(max_uncompressed_data_len);
  size_t uncompressed_data_len;
  size_t compressed_data_len;
  char *uncompressed_data = malloc(max_uncompressed_data_len);
  char *compressed_data = malloc(max_compressed_data_len);
  int err = 1;

  if (uncompressed_data == NULL || compressed_data == NULL) {
    print_error("out of memory\n");
    goto cleanup;
  }

  /* write the steam header */
  fwrite(stream_header, sizeof(stream_header), 1, outfp);

  /* write file body */
  while ((uncompressed_data_len = fread(uncompressed_data, 1, max_uncompressed_data_len, infp)) > 0) {
    unsigned int crc32c = masked_crc32c(uncompressed_data, uncompressed_data_len);
    char type_code;
    size_t write_len;
    const char *write_data;

    /* compress the block. */
    compressed_data_len = max_compressed_data_len;
    snappy_compress(uncompressed_data, uncompressed_data_len, compressed_data, &compressed_data_len);

    if (compressed_data_len >= (uncompressed_data_len - (uncompressed_data_len / 8))) {
      /* uncompressed data */
      type_code = UNCOMPRESSED_DATA_IDENTIFIER;
      write_len = uncompressed_data_len;
      write_data = uncompressed_data;
    } else {
      /* compressed data */
      type_code = COMPRESSED_DATA_IDENTIFIER;
      write_len = compressed_data_len;
      write_data = compressed_data;
    }

    /* write block type */
    putc(type_code, outfp);
    /* write data length */
    putc(((write_len + 4) >> 0), outfp);
    putc(((write_len + 4) >> 8), outfp);
    putc(((write_len + 4) >> 16), outfp);
    /* write checksum */
    putc((crc32c >>  0), outfp);
    putc((crc32c >>  8), outfp);
    putc((crc32c >> 16), outfp);
    putc((crc32c >> 24), outfp);
    /* write data */
    if (fwrite(write_data, write_len, 1, outfp) != 1) {
      print_error("Failed to write a file: %s\n", strerror(errno));
      goto cleanup;
    }
  }
  /* check stream errors */
  if (ferror(infp)) {
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }
  if (ferror(outfp)) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    goto cleanup;
  }
  err = 0;
 cleanup:
  free(uncompressed_data);
  free(compressed_data);
  return err;
}

static int read_data(char *buf, size_t buflen, FILE *fp)
{
  if (fread(buf, buflen, 1, fp) != 1) {
    if (feof(fp)) {
      print_error("Unexpected end of file\n");
    } else {
      print_error("Failed to read a file: %s\n", strerror(errno));
    }
    return -1;
  }
  return 0;
}

/*
 * Callers must ensure that the checksum pointer is aligned to a 4 byte boundary
 * if the CPU disallows unaligned accesss.
 */
static int check_crc32c(const char *data, size_t datalen, const char *checksum)
{
  unsigned int actual_crc32c = masked_crc32c(data, datalen);
  unsigned int expected_crc32c = SNZ_FROM_LE32(*(unsigned int*)checksum);
  if (actual_crc32c != expected_crc32c) {
    print_error("CRC32C error! (expected 0x%08x but 0x%08x)\n", expected_crc32c, actual_crc32c);
    return -1;
  }
  return 0;
}

static int framing_format_uncompress(FILE *infp, FILE *outfp, int skip_magic)
{
  const size_t max_data_len = MAX_DATA_LEN;
  const size_t max_uncompressed_data_len = MAX_UNCOMPRESSED_DATA_LEN;
  size_t data_len;
  size_t uncompressed_data_len;
  char *data = malloc(max_data_len);
  char *uncompressed_data = malloc(max_uncompressed_data_len);
  int err = 1;

  if (data == NULL || uncompressed_data == NULL) {
    print_error("out of memory\n");
    goto cleanup;
  }

  if (!skip_magic) {
    /* read the steam header */
    if (read_data(data, sizeof(stream_header), infp) != 0) {
      goto cleanup;
    }
    if (memcmp(data, stream_header, sizeof(stream_header)) != 0) {
      print_error("Invalid stream identfier\n");
      goto cleanup;
    }
  }

  for (;;) {
    int id = getc(infp);
    if (id == EOF) {
      break;
    }
    data_len = getc(infp);
    data_len |= getc(infp) << 8;
    data_len |= getc(infp) << 16;
    if (data_len == (size_t)EOF) {
      print_error("Unexpected end of file\n");
      goto cleanup;
    }
    if (id == COMPRESSED_DATA_IDENTIFIER) {
      /* 4.2. Compressed data (chunk type 0x00) */
      if (data_len < 4) {
        print_error("too short data length %lu\n", data_len);
        goto cleanup;
      }
      if (read_data(data, data_len, infp) != 0) {
        goto cleanup;
      }
      uncompressed_data_len = max_uncompressed_data_len;
      if (snappy_uncompress(data + 4, data_len - 4, uncompressed_data, &uncompressed_data_len)) {
        print_error("Invalid data: snappy_uncompress failed\n");
        goto cleanup;
      }
      if (check_crc32c(uncompressed_data, uncompressed_data_len, data) != 0) {
        goto cleanup;
      }
      if (fwrite(uncompressed_data, uncompressed_data_len, 1, outfp) != 1) {
        break;
      }
    } else if (id == UNCOMPRESSED_DATA_IDENTIFIER) {
      /* 4.3. Uncompressed data (chunk type 0x01) */
      if (data_len < 4) {
        print_error("too short data length %lu\n", data_len);
        goto cleanup;
      }
      if (read_data(data, data_len, infp) != 0) {
        goto cleanup;
      }
      if (check_crc32c(data + 4, data_len - 4, data) != 0) {
        goto cleanup;
      }
      if (fwrite(data + 4, data_len - 4, 1, outfp) != 1) {
        break;
      }
    } else if (id < 0x80) {
      /* 4.4. Reserved unskippable chunks (chunk types 0x02-0x7f) */
      print_error("Unsupported identifier 0x%02x\n", id);
      goto cleanup;
    } else {
      /* 4.5. Reserved skippable chunks (chunk types 0x80-0xfe) */
      while (data_len-- > 0) {
        if (getc(infp) == EOF) {
          print_error("Unexpected end of file\n");
          goto cleanup;
        }
      }
    }
  }
  /* check stream errors */
  if (ferror(infp)) {
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }
  if (ferror(outfp)) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    goto cleanup;
  }
  err = 0;
 cleanup:
  free(data);
  free(uncompressed_data);
  return err;
}

stream_format_t framing2_format = {
  "framing2",
  "https://github.com/google/snappy/blob/master/framing_format.txt",
  "sz",
  framing_format_compress,
  framing_format_uncompress,
};
