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

/* same with CommonConfigurationKeys.IO_COMPRESSION_CODEC_SNAPPY_BUFFERSIZE_DEFAULT in hadoop */
#define SNAPPY_BUFFER_SIZE_DEFAULT (256 * 1024)

/* Calculate max_input_size from block_size as in hadoop-snappy.
 *
 * In SnappyCodec.createOutputStream(OutputStream out, Compressor compressor)
 *
 *     int compressionOverhead = (bufferSize / 6) + 32;
 *
 * In BlockCompressorStream(OutputStream out, Compressor compressor, int bufferSize, int compressionOverhead)
 *
 *     MAX_INPUT_SIZE = bufferSize - compressionOverhead;
 */
size_t hadoop_snappy_max_input_size(size_t block_size)
{
  const size_t buffer_size = block_size ? block_size : SNAPPY_BUFFER_SIZE_DEFAULT;
  const size_t compression_overhead = (buffer_size / 6) + 32;
  return buffer_size - compression_overhead;
}

static inline int write_num(FILE *fp, size_t num)
{
  unsigned int n = SNZ_TO_BE32((unsigned int)num);
  if (fwrite_unlocked(&n, sizeof(n), 1, fp) != 1) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    return 0;
  }
  return 1;
}

static int hadoop_snappy_format_compress(FILE *infp, FILE *outfp, size_t block_size)
{
  work_buffer_t wb;
  size_t uncompressed_data_len;
  int err = 1;

  work_buffer_init(&wb, hadoop_snappy_max_input_size(block_size));

  /* write file body */
  while ((uncompressed_data_len = fread_unlocked(wb.uc, 1, wb.uclen, infp)) > 0) {
    size_t compressed_data_len;
    /* write length before compression */
    if (write_num(outfp, uncompressed_data_len) == 0) {
      goto cleanup;
    }

    /* compress the block. */
    compressed_data_len = wb.clen;
    snappy_compress(wb.uc, uncompressed_data_len, wb.c, &compressed_data_len);

    /* write compressed length */
    if (write_num(outfp, compressed_data_len) == 0) {
      goto cleanup;
    }
    /* write data */
    if (fwrite_unlocked(wb.c, compressed_data_len, 1, outfp) != 1) {
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
  work_buffer_free(&wb);
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

static int hadoop_snappy_format_uncompress(FILE *infp, FILE *outfp, int skip_magic)
{
  work_buffer_t wb;
  size_t source_len = 0;
  size_t compressed_len = 0;
  int err = 1;

  work_buffer_init(&wb, hadoop_snappy_max_input_size(0));

  if (skip_magic) {
    source_len = hadoop_snappy_source_length;
    compressed_len = hadoop_snappy_compressed_length;
    trace("source_len = %ld.\n", (long)source_len);
    trace("compressed_len = %ld.\n", (long)compressed_len);
    goto after_reading_compressed_len;
  }

  for (;;) {
    unsigned int n;

    if (fread_unlocked(&n, sizeof(n), 1, infp) != 1) {
      if (feof_unlocked(infp)) {
        err = 0;
      } else {
        print_error("Failed to read a file: %s\n", strerror(errno));
      }
      goto cleanup;
    }
    source_len = SNZ_FROM_BE32(n);
    trace("source_len = %ld.\n", (long)source_len);

    while (source_len > 0) {
      size_t uncompressed_len;

      if (read_data((char*)&n, sizeof(n), infp) != 0) {
        goto cleanup;
      }
      compressed_len = SNZ_FROM_BE32(n);
      trace("compressed_len = %ld.\n", (long)compressed_len);
    after_reading_compressed_len:
      if (compressed_len > wb.clen) {
        work_buffer_resize(&wb, compressed_len, 0);
      }

      /* read the compressed data */
      if (read_data(wb.c, compressed_len, infp) != 0) {
        goto cleanup;
      }
      trace("read %ld bytes.\n", (long)(compressed_len));

      /* check the uncompressed length */
      err = snappy_uncompressed_length(wb.c, compressed_len, &uncompressed_len);
      if (err != 0) {
        print_error("Invalid data: GetUncompressedLength failed %d\n", err);
        goto cleanup;
      }
      err = 1;
      if (uncompressed_len > source_len) {
        print_error("Invalid data: uncompressed_length > source_len\n");
        goto cleanup;
      }

      if (uncompressed_len > wb.uclen) {
        work_buffer_resize(&wb, 0, uncompressed_len);
      }

      /* uncompress and write */
      if (snappy_uncompress(wb.c, compressed_len, wb.uc, &uncompressed_len)) {
        print_error("Invalid data: RawUncompress failed\n");
        goto cleanup;
      }
      if (fwrite_unlocked(wb.uc, uncompressed_len, 1, outfp) != 1) {
        print_error("Failed to write a file: %s\n", strerror(errno));
        goto cleanup;
      }
      trace("write %ld bytes\n", (long)uncompressed_len);

      source_len -= uncompressed_len;
      trace("uncompressed_len = %ld, source_len -> %ld\n", (long)uncompressed_len, (long)source_len);
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
  work_buffer_free(&wb);
  return err;
}

stream_format_t hadoop_snappy_format = {
  "hadoop-snappy",
  "https://code.google.com/p/hadoop-snappy/",
  "snappy",
  hadoop_snappy_format_compress,
  hadoop_snappy_format_uncompress,
};
