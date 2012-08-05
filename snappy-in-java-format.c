/* -*- indent-tabs-mode: nil -*-
 *
 * Copyright 2011-2012 Kubo Takehiro <kubo@jiubao.org>
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
#include <arpa/inet.h>
#include "snzip.h"
#include "crc32.h"

#define SNAPPY_IN_JAVA_MAGIC "snappy\x00"
#define SNAPPY_IN_JAVA_MAGIC_LEN 7
#define SNAPPY_IN_JAVA_FILE_VERSION 1
#define DEFAULT_BLOCK_SIZE (1 << 15)
#define MAX_BLOCK_SIZE 0xFFFF

#define COMPRESSED_FLAG 0x01
#define UNCOMPRESSED_FLAG 0x00

typedef struct {
  char magic[SNAPPY_IN_JAVA_MAGIC_LEN];
} snappy_in_java_header_t;

static const snappy_in_java_header_t snappy_in_java_header = {
  SNAPPY_IN_JAVA_MAGIC,
};

static int write_block(FILE *outfp, const char *buffer, size_t length, int compressed, unsigned int crc32c);
static int check_and_write_block(int outfd, const char *buffer, size_t length, int verify_checksum, unsigned int crc32c);

static int snappy_in_java_compress(FILE *infp, FILE *outfp, size_t block_size)
{
  work_buffer_t wb;
  size_t uncompressed_length;
  int err = 1;

  wb.c = NULL;
  wb.uc = NULL;

  if (block_size == 0) {
    block_size = DEFAULT_BLOCK_SIZE;
  }
  if (block_size > MAX_BLOCK_SIZE) {
    print_error("Too large block size: %lu. (default: %d, max: %d)\n",
                (unsigned long)block_size, DEFAULT_BLOCK_SIZE, MAX_BLOCK_SIZE);
    goto cleanup;
  }

  if (fwrite_unlocked(&snappy_in_java_header, sizeof(snappy_in_java_header), 1, outfp) != 1) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    goto cleanup;
  }

  /* write file body */
  work_buffer_init(&wb, block_size);
  while ((uncompressed_length = fread_unlocked(wb.uc, 1, wb.uclen, infp)) > 0) {
    size_t compressed_length = wb.clen;
    unsigned int crc32c = masked_crc32c(wb.uc, uncompressed_length);

    trace("read %lu bytes.\n", (unsigned long)uncompressed_length);

    /* compress the block. */
    snappy_compress(wb.uc, uncompressed_length, wb.c, &compressed_length);
    trace("compressed_legnth is %lu.\n", (unsigned long)compressed_length);

    if (compressed_length >= (uncompressed_length - (uncompressed_length / 8))) {
      trace("write uncompressed data\n");
      if (write_block(outfp, wb.uc, uncompressed_length, FALSE, crc32c) != 0) {
        goto cleanup;
      }
    } else {
      trace("write compressed data\n");
      if (write_block(outfp, wb.c, compressed_length, TRUE, crc32c) != 0) {
        goto cleanup;
      }
    }
  }
  if (!feof_unlocked(infp)) {
    /* fread_unlocked() failed. */
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }
  err = 0;
 cleanup:
  work_buffer_free(&wb);
  return err;
}

static int write_block(FILE *outfp, const char *buffer, size_t length, int compressed, unsigned int crc32c)
{
  /* write compressed flag */
  putc_unlocked(compressed ? COMPRESSED_FLAG : UNCOMPRESSED_FLAG, outfp);
  trace("write 1 byte for compressed flag.\n");

  /* write data length. */
  putc_unlocked((length >>  8), outfp);
  putc_unlocked((length >>  0), outfp);
  trace("write 2 bytes for data length.\n");

  /* write crc32c */
  putc_unlocked((crc32c >> 24), outfp);
  putc_unlocked((crc32c >> 16), outfp);
  putc_unlocked((crc32c >>  8), outfp);
  putc_unlocked((crc32c >>  0), outfp);
  trace("write 4 bytes for crc32c.\n");

  if (fwrite_unlocked(buffer, length, 1, outfp) != 1) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    return -1;
  }
  trace("write %ld bytes for data.\n", (long)length);
  return 0;
}

static int snappy_in_java_uncompress(FILE *infp, FILE *outfp, int skip_magic)
{
  snappy_in_java_header_t header;
  work_buffer_t wb;
  int err = 1;
  int outfd;

  wb.c = NULL;
  wb.uc = NULL;

  if (!skip_magic) {
    /* read header */
    if (fread_unlocked(&header, sizeof(header), 1, infp) != 1) {
      print_error("Failed to read a file: %s\n", strerror(errno));
      goto cleanup;
    }

    /* check header */
    if (memcmp(header.magic, SNAPPY_IN_JAVA_MAGIC, SNAPPY_IN_JAVA_MAGIC_LEN) != 0) {
      print_error("This is not a snappy-java file.\n");
      goto cleanup;
    }
  }

  /* Use a file descriptor 'outfd' instead of the stdio file pointer 'outfp'
   * to reduce the number of write system calls.
   */
  fflush(outfp);
  outfd = fileno(outfp);

  /* read body */
  work_buffer_init(&wb, MAX_BLOCK_SIZE);
  for (;;) {
    int compressed_flag;
    size_t length = 0;
    unsigned int crc32c = 0;

    /* read compressed flag */
    compressed_flag = getc_unlocked(infp);
    switch (compressed_flag) {
    case EOF:
      /* read all blocks */
      err = 0;
      goto cleanup;
    case COMPRESSED_FLAG:
    case UNCOMPRESSED_FLAG:
      /* pass */
      break;
    default:
      print_error("Unknown compressed flag 0x%02x\n", compressed_flag);
      goto cleanup;
    }

    /* read data length. */
    length |= (getc_unlocked(infp) << 8);
    length |= (getc_unlocked(infp) << 0);

    /* read crc32c. */
    crc32c |= (getc_unlocked(infp) << 24);
    crc32c |= (getc_unlocked(infp) << 16);
    crc32c |= (getc_unlocked(infp) <<  8);
    crc32c |= (getc_unlocked(infp) <<  0);

    /* check read error */
    if (feof_unlocked(infp)) {
      print_error("Unexpected end of file.\n");
      goto cleanup;
    } else if (ferror_unlocked(infp)) {
      print_error("Failed to read a file: %s\n", strerror(errno));
      goto cleanup;
    }

    /* read data */
    if (fread_unlocked(wb.c, length, 1, infp) != 1) {
      if (feof_unlocked(infp)) {
        print_error("Unexpected end of file\n");
      } else {
        print_error("Failed to read a file: %s\n", strerror(errno));
      }
      goto cleanup;
    }
    trace("read %ld bytes.\n", (long)(length));

    if (compressed_flag == COMPRESSED_FLAG) {
      /* check the uncompressed length */
      size_t uncompressed_length;
      err = snappy_uncompressed_length(wb.c, length, &uncompressed_length);
      if (err != 0) {
        print_error("Invalid data: GetUncompressedLength failed %d\n", err);
        goto cleanup;
      }
      err = 1;
      if (uncompressed_length > wb.uclen) {
        print_error("Invalid data: too long uncompressed length\n");
        goto cleanup;
      }

      /* uncompress and write */
      if (snappy_uncompress(wb.c, length, wb.uc, &uncompressed_length)) {
        print_error("Invalid data: RawUncompress failed\n");
        goto cleanup;
      }
      if (check_and_write_block(outfd, wb.uc, uncompressed_length, 1, crc32c)) {
        goto cleanup;
      }
    } else {
      if (check_and_write_block(outfd, wb.c, length, 1, crc32c)) {
        goto cleanup;
      }
    }
  }
 cleanup:
  work_buffer_free(&wb);
  return err;
}

static int check_and_write_block(int outfd, const char *buffer, size_t length, int verify_checksum, unsigned int crc32c)
{
  if (verify_checksum) {
    unsigned int actual_crc32c = masked_crc32c(buffer, length);
    if (actual_crc32c != crc32c) {
      print_error("Invalid crc code (expected 0x%08x but 0x%08x)\n", crc32c, actual_crc32c);
      return 1;
    }
  }
  if (write_full(outfd, buffer, length) != length) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    return 1;
  }
  trace("write %ld bytes\n", (long)length);
  return 0;
}

stream_format_t snappy_in_java_format = {
  "snappy-in-java",
  "https://github.com/dain/snappy",
  "snappy",
  SNAPPY_IN_JAVA_MAGIC,
  SNAPPY_IN_JAVA_MAGIC_LEN,
  snappy_in_java_compress,
  snappy_in_java_uncompress,
};
