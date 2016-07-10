/* -*- indent-tabs-mode: nil -*-
 *
 * Copyright 2016 Kubo Takehiro <kubo@jiubao.org>
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

#define COMPRESSED_DATA_IDENTIFIER 0

#define MAX_DATA_LEN 16777215 /* maximum chunk data length */
#define MAX_UNCOMPRESSED_DATA_LEN 65536 /* maximum uncompressed data length excluding checksum */

static int iwa_compress(FILE *infp, FILE *outfp, size_t block_size)
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

  /* write file body */
  while ((uncompressed_data_len = fread_unlocked(uncompressed_data, 1, max_uncompressed_data_len, infp)) > 0) {
    /* compress the block. */
    compressed_data_len = max_compressed_data_len;
    snappy_compress(uncompressed_data, uncompressed_data_len, compressed_data, &compressed_data_len);

    /* write block type */
    putc_unlocked(COMPRESSED_DATA_IDENTIFIER, outfp);
    /* write data length */
    putc_unlocked((compressed_data_len >> 0), outfp);
    putc_unlocked((compressed_data_len >> 8), outfp);
    putc_unlocked((compressed_data_len >> 16), outfp);
    /* write data */
    if (fwrite_unlocked(compressed_data, compressed_data_len, 1, outfp) != 1) {
      print_error("Failed to write a file: %s\n", strerror(errno));
      goto cleanup;
    }
  }
  /* check stream errors */
  if (ferror_unlocked(infp)) {
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }
  if (ferror_unlocked(outfp)) {
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
  if (fread_unlocked(buf, buflen, 1, fp) != 1) {
    if (feof_unlocked(fp)) {
      print_error("Unexpected end of file\n");
    } else {
      print_error("Failed to read a file: %s\n", strerror(errno));
    }
    return -1;
  }
  return 0;
}

static int iwa_uncompress(FILE *infp, FILE *outfp, int skip_magic)
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

  for (;;) {
    int id = getc_unlocked(infp);
    if (id == EOF) {
      break;
    }
    if (id != COMPRESSED_DATA_IDENTIFIER) {
      print_error("Invalid data identifier: 0x%02x\n", id);
      goto cleanup;
    }
    data_len = getc_unlocked(infp);
    data_len |= getc_unlocked(infp) << 8;
    data_len |= getc_unlocked(infp) << 16;
    if (data_len == (size_t)EOF) {
      print_error("Unexpected end of file\n");
      goto cleanup;
    }
    if (data_len < 4) {
      print_error("too short data length %lu\n", data_len);
      goto cleanup;
    }
    if (read_data(data, data_len, infp) != 0) {
      goto cleanup;
    }
    uncompressed_data_len = max_uncompressed_data_len;
    if (snappy_uncompress(data, data_len, uncompressed_data, &uncompressed_data_len)) {
      print_error("Invalid data: snappy_uncompress failed\n");
      goto cleanup;
    }
    if (fwrite_unlocked(uncompressed_data, uncompressed_data_len, 1, outfp) != 1) {
      break;
    }
  }
  /* check stream errors */
  if (ferror_unlocked(infp)) {
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }
  if (ferror_unlocked(outfp)) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    goto cleanup;
  }
  err = 0;
 cleanup:
  free(data);
  free(uncompressed_data);
  return err;
}

stream_format_t iwa_format = {
  "iwa",
  "https://github.com/obriensp/iWorkFileFormat/blob/master/Docs/index.md#snappy-compression",
  "iwa",
  iwa_compress,
  iwa_uncompress,
};
