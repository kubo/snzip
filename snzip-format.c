/* -*- indent-tabs-mode: nil -*-
 *
 * Copyright 2011 Kubo Takehiro <kubo@jiubao.org>
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

#define SNZ_MAGIC "SNZ"
#define SNZ_MAGIC_LEN 3
#define SNZ_FILE_VERSION 1

#define SNZ_DEFAULT_BLOCK_SIZE 16 /* (1 << 16) => 64 KiB */
#define SNZ_MAX_BLOCK_SIZE 27 /* (1 << 27) => 128 MiB */

#define VARINT_MAX 5

typedef struct {
  char magic[SNZ_MAGIC_LEN]; /* SNZ_MAGIC */
  char version;  /* SNZ_FILE_VERSION */
  unsigned char block_size; /* nth power of two. */
} snz_header_t;

static int snzip_compress(FILE *infp, FILE *outfp, size_t block_size)
{
  snz_header_t header;
  work_buffer_t wb;
  size_t uncompressed_length;
  int err = 1;
  int nshift;

  wb.c = NULL;
  wb.uc = NULL;

  if (block_size == 0) {
    block_size = 1ul << SNZ_DEFAULT_BLOCK_SIZE;
    nshift = SNZ_DEFAULT_BLOCK_SIZE;
  } else {
    if (block_size > (1ul << SNZ_MAX_BLOCK_SIZE)) {
      print_error("too large block size: %lu\n", block_size);
      goto cleanup;
    }

    for (nshift = 1; nshift <= SNZ_MAX_BLOCK_SIZE; nshift++) {
      if (1ul << nshift == block_size) {
        break;
      }
    }
    if (nshift == SNZ_MAX_BLOCK_SIZE) {
      print_error("The block size must be power of two\n");
      goto cleanup;
    }
  }

  /* write the file header */
  memcpy(header.magic, SNZ_MAGIC, SNZ_MAGIC_LEN);
  header.version = SNZ_FILE_VERSION;
  header.block_size = nshift;

  if (fwrite_unlocked(&header, sizeof(header), 1, outfp) != 1) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    goto cleanup;
  }

  /* write file body */
  work_buffer_init(&wb, block_size);
  while ((uncompressed_length = fread_unlocked(wb.uc, 1, wb.uclen, infp)) > 0) {
    size_t compressed_length = wb.clen;

    trace("read %lu bytes.\n", (unsigned long)uncompressed_length);

    /* compress the block. */
    snappy_compress(wb.uc, uncompressed_length, wb.c, &compressed_length);
    trace("compressed_legnth is %lu.\n", (unsigned long)compressed_length);

    /* write the compressed length. */
    if (compressed_length < (1ul << 7)) {
      putc_unlocked(compressed_length, outfp);
      trace("write 1 byte for compressed data length.\n");
    } else if (compressed_length < (1ul << 14)) {
      putc_unlocked((compressed_length >> 0) | 0x80, outfp);
      putc_unlocked((compressed_length >> 7), outfp);
      trace("write 2 bytes for compressed data length.\n");
    } else if (compressed_length < (1ul << 21)) {
      putc_unlocked((compressed_length >> 0) | 0x80, outfp);
      putc_unlocked((compressed_length >> 7) | 0x80, outfp);
      putc_unlocked((compressed_length >> 14), outfp);
      trace("write 3 bytes for compressed data length.\n");
    } else if (compressed_length < (1ul << 28)) {
      putc_unlocked((compressed_length >> 0) | 0x80, outfp);
      putc_unlocked((compressed_length >> 7) | 0x80, outfp);
      putc_unlocked((compressed_length >> 14)| 0x80, outfp);
      putc_unlocked((compressed_length >> 21), outfp);
      trace("write 4 bytes for compressed data length.\n");
    } else {
      putc_unlocked((compressed_length >> 0) | 0x80, outfp);
      putc_unlocked((compressed_length >> 7) | 0x80, outfp);
      putc_unlocked((compressed_length >> 14)| 0x80, outfp);
      putc_unlocked((compressed_length >> 21)| 0x80, outfp);
      putc_unlocked((compressed_length >> 28), outfp);
      trace("write 5 bytes for compressed data length.\n");
    }

    /* write the compressed data. */
    if (fwrite_unlocked(wb.c, compressed_length, 1, outfp) != 1) {
      print_error("Failed to write a file: %s\n", strerror(errno));
      goto cleanup;
    }
    trace("write %ld bytes for compressed data.\n", (long)compressed_length);
  }
  if (!feof_unlocked(infp)) {
    /* fread_unlocked() failed. */
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }
  putc_unlocked('\0', outfp);
  trace("write 1 byte\n");
  err = 0;
 cleanup:
  work_buffer_free(&wb);
  return err;
}

static int snzip_uncompress(FILE *infp, FILE *outfp)
{
  snz_header_t header;
  work_buffer_t wb;
  int err = 1;
  int outfd;

  wb.c = NULL;
  wb.uc = NULL;

  /* read header */
  if (fread_unlocked(&header, sizeof(header), 1, infp) != 1) {
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }

  /* check header */
  if (memcmp(header.magic, SNZ_MAGIC, SNZ_MAGIC_LEN) != 0) {
    print_error("This is not a snz file.\n");
    goto cleanup;
  }
  if (header.version != SNZ_FILE_VERSION) {
    print_error("Unknown snz version %d\n", header.version);
    goto cleanup;
  }
  if (header.block_size > SNZ_MAX_BLOCK_SIZE) {
    print_error("Invalid block size %d (max %d)\n", header.block_size, SNZ_MAX_BLOCK_SIZE);
    goto cleanup;
  }

  /* Use a file descriptor 'outfd' instead of the stdio file pointer 'outfp'
   * to reduce the number of write system calls.
   */
  fflush(outfp);
  outfd = fileno(outfp);

  /* read body */
  work_buffer_init(&wb, (1 << header.block_size));
  for (;;) {
    /* read the compressed length in a block */
    size_t compressed_length = 0;
    size_t uncompressed_length = wb.uclen;
    int idx;

    for (idx = 0; idx < VARINT_MAX; idx++) {
      int chr = getc_unlocked(infp);
      if (chr == -1) {
        print_error("Unexpected end of file.\n");
        goto cleanup;
      }
      compressed_length |= ((chr & 127) << (idx * 7));
      if ((chr & 128) == 0) {
        break;
      }
    }
    trace("read %d bytes (compressed_length = %ld)\n", idx + 1, (long)compressed_length);
    if (idx == VARINT_MAX) {
      print_error("Invalid format.\n");
      goto cleanup;
    }
    if (compressed_length == 0) {
      /* read all blocks */
      err = 0;
      goto cleanup;
    }
    if (compressed_length > wb.clen) {
      print_error("Invalid data: too long compressed length\n");
      goto cleanup;
    }

    /* read the compressed data */
    if (fread_unlocked(wb.c, compressed_length, 1, infp) != 1) {
      if (feof_unlocked(infp)) {
        print_error("Unexpected end of file\n");
      } else {
        print_error("Failed to read a file: %s\n", strerror(errno));
      }
      goto cleanup;
    }
    trace("read %ld bytes.\n", (long)(compressed_length));

    /* check the uncompressed length */
    err = snappy_uncompressed_length(wb.c, compressed_length, &uncompressed_length);
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
    if (snappy_uncompress(wb.c, compressed_length, wb.uc, &uncompressed_length)) {
      print_error("Invalid data: RawUncompress failed\n");
      goto cleanup;
    }
    if (write_full(outfd, wb.uc, uncompressed_length) != uncompressed_length) {
      print_error("Failed to write a file: %s\n", strerror(errno));
      goto cleanup;
    }
    trace("write %ld bytes\n", (long)uncompressed_length);
  }
 cleanup:
  work_buffer_free(&wb);
  return err;
}

stream_format_t snzip_format = {
  "snzip",
  "https://github.com/kubo/snzip",
  "snz",
  'S',
  snzip_compress,
  snzip_uncompress,
};
