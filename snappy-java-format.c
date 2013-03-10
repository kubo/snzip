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

#define SNAPPY_JAVA_MAGIC "\x82SNAPPY\x00"
#define SNAPPY_JAVA_MAGIC_LEN 8
#define SNAPPY_JAVA_FILE_VERSION 1
#define DEFAULT_BLOCK_SIZE (32 * 1024) // Use 32kb for the default block size

typedef struct {
  char magic[SNAPPY_JAVA_MAGIC_LEN];
  int version;
  int compatible_version;
} snappy_java_header_t;

static int snappy_java_compress(FILE *infp, FILE *outfp, size_t block_size)
{
  snappy_java_header_t header;
  work_buffer_t wb;
  size_t uncompressed_length;
  int err = 1;

  wb.c = NULL;
  wb.uc = NULL;

  if (block_size == 0) {
    block_size = DEFAULT_BLOCK_SIZE;
  }

  /* write the file header */
  memcpy(header.magic, SNAPPY_JAVA_MAGIC, SNAPPY_JAVA_MAGIC_LEN);
  header.version = htonl(SNAPPY_JAVA_FILE_VERSION);
  header.compatible_version = htonl(SNAPPY_JAVA_FILE_VERSION);

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
    putc_unlocked((compressed_length >> 24), outfp);
    putc_unlocked((compressed_length >> 16), outfp);
    putc_unlocked((compressed_length >>  8), outfp);
    putc_unlocked((compressed_length >>  0), outfp);
    trace("write 4 bytes for compressed data length.\n");

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
  err = 0;
 cleanup:
  work_buffer_free(&wb);
  return err;
}

static int snappy_java_uncompress(FILE *infp, FILE *outfp, int skip_magic)
{
  snappy_java_header_t header;
  work_buffer_t wb;
  int err = 1;
  int outfd;

  wb.c = NULL;
  wb.uc = NULL;

  if (skip_magic) {
    /* read header except magic */
    if (fread_unlocked(&header.version, sizeof(header) - sizeof(header.magic), 1, infp) != 1) {
      print_error("Failed to read a file: %s\n", strerror(errno));
      goto cleanup;
    }
  } else {
    /* read header */
    if (fread_unlocked(&header, sizeof(header), 1, infp) != 1) {
      print_error("Failed to read a file: %s\n", strerror(errno));
      goto cleanup;
    }

    /* check magic */
    if (memcmp(header.magic, SNAPPY_JAVA_MAGIC, SNAPPY_JAVA_MAGIC_LEN) != 0) {
      print_error("This is not a snappy-java file.\n");
      goto cleanup;
    }
  }

  /* check rest header */
  header.version = ntohl(header.version);
  if (header.version != SNAPPY_JAVA_FILE_VERSION) {
    print_error("Unknown snappy-java version %d\n", header.version);
    goto cleanup;
  }

  header.compatible_version = ntohl(header.compatible_version);
  if (header.compatible_version != SNAPPY_JAVA_FILE_VERSION) {
    print_error("Unknown snappy-java compatible version %d\n", header.compatible_version);
    goto cleanup;
  }

  /* Use a file descriptor 'outfd' instead of the stdio file pointer 'outfp'
   * to reduce the number of write system calls.
   */
  fflush(outfp);
  outfd = fileno(outfp);

  /* read body */
  work_buffer_init(&wb, DEFAULT_BLOCK_SIZE);
  for (;;) {
    /* read the compressed length in a block */
    size_t compressed_length = 0;
    size_t uncompressed_length = wb.uclen;
    int idx;

    for (idx = 3; idx >= 0; idx--) {
      int chr = getc_unlocked(infp);
      if (chr == -1) {
        if (idx == 3) {
          /* read all blocks */
          err = 0;
          goto cleanup;
        }
        print_error("Unexpected end of file.\n");
        goto cleanup;
      }
      compressed_length |= (chr << (idx * 8));
    }

    trace("read 4 bytes (compressed_length = %ld)\n", (long)compressed_length);
    if (compressed_length == 0) {
      print_error("Invalid compressed length %ld\n", (long)compressed_length);
      goto cleanup;
    }
    if (compressed_length > wb.clen) {
      work_buffer_resize(&wb, compressed_length, 0);
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
      work_buffer_resize(&wb, 0, uncompressed_length);
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

stream_format_t snappy_java_format = {
  "snappy-java",
  "http://code.google.com/p/snappy-java/",
  "snappy",
  snappy_java_compress,
  snappy_java_uncompress,
};
