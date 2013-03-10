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

#define MAGIC "snappy"
#define MAGIC_LEN 6u

#define COMPRESSED_TYPE_CODE 0x00
#define UNCOMPRESSED_TYPE_CODE 0x01
#define END_OF_STREAM_TYPE_CODE 0xfe
#define HEADER_TYPE_CODE 0xff

#define SUCCESS 0
#define TOO_SHORT_DATA_BLOCK 1

typedef enum  {
  ERROR_STATE,
  INITIAL_STATE,
  PROCESSING_STATE,
  END_OF_STREAM_STATE,
} stream_state_t;

typedef struct {
  uint8_t type;
  uint16_t data_len;
  char data[UINT16_MAX];
} block_data_t;

/*
 * read_block() returns SUCCESS, TOO_SHORT_BLOCK or EOF.
 */
static int read_block(FILE *fp, block_data_t *bd);

static stream_state_t process_block(FILE *fp, stream_state_t state, block_data_t *bd, char *work, size_t work_len);

static int comment_43_compress(FILE *infp, FILE *outfp, size_t block_size)
{
  const size_t max_raw_data_len = 32 * 1024; /* maximum data length */
  const size_t max_compressed_data_len = snappy_max_compressed_length(max_raw_data_len); /* maximum compressed length */
  size_t raw_data_len;
  size_t compressed_data_len;
  char *raw_data = malloc(max_raw_data_len);
  char *compressed_data = malloc(max_compressed_data_len);
  int err = 1;

  if (raw_data == NULL || compressed_data == NULL) {
    print_error("out of memory\n");
    goto cleanup;
  }

  putc_unlocked(HEADER_TYPE_CODE, outfp);
  putc_unlocked(MAGIC_LEN, outfp);
  putc_unlocked(MAGIC_LEN >> 8, outfp);
  fwrite_unlocked(MAGIC, MAGIC_LEN, 1, outfp);

  /* write file body */
  while ((raw_data_len = fread_unlocked(raw_data, 1, max_raw_data_len, infp)) > 0) {
    unsigned int crc32c = masked_crc32c(raw_data, raw_data_len);
    char type_code;
    size_t write_len;
    const char *write_data;

    /* compress the block. */
    compressed_data_len = max_compressed_data_len;
    snappy_compress(raw_data, raw_data_len, compressed_data, &compressed_data_len);

    if (compressed_data_len >= (raw_data_len - (raw_data_len / 8))) {
      /* write uncompressed data */
      type_code = UNCOMPRESSED_TYPE_CODE;
      write_len = raw_data_len;
      write_data = raw_data;
    } else {
      /* write compressed data */
      type_code = COMPRESSED_TYPE_CODE;
      write_len = compressed_data_len;
      write_data = compressed_data;
    }

    /* block type */
    putc_unlocked(type_code, outfp);
    /* data length */
    putc_unlocked(((write_len + 4) >> 0), outfp);
    putc_unlocked(((write_len + 4) >> 8), outfp);
    /* data */
    putc_unlocked((crc32c >>  0), outfp);
    putc_unlocked((crc32c >>  8), outfp);
    putc_unlocked((crc32c >> 16), outfp);
    putc_unlocked((crc32c >> 24), outfp);
    if (fwrite_unlocked(write_data, write_len, 1, outfp) != 1) {
      print_error("Failed to write a file: %s\n", strerror(errno));
      goto cleanup;
    }
  }
  if (!feof_unlocked(infp)) {
    /* fread_unlocked() failed. */
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }
  putc_unlocked(END_OF_STREAM_TYPE_CODE, outfp);
  putc_unlocked(0, outfp);
  putc_unlocked(0, outfp);
  if (ferror_unlocked(outfp)) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    goto cleanup;
  }
  err = 0;
 cleanup:
  free(raw_data);
  free(compressed_data);
  return err;
}


static int comment_43_uncompress(FILE *infp, FILE *outfp, int skip_magic)
{
  block_data_t *block_data = malloc(sizeof(block_data_t));
  size_t work_len = snappy_max_compressed_length(UINT16_MAX); /* length of worst case */
  char *work = malloc(work_len);
  int err = 1;
  stream_state_t state = skip_magic ? PROCESSING_STATE : INITIAL_STATE;

  if (block_data == NULL || work == NULL) {
    print_error("out of memory\n");
    goto cleanup;
  }

  while (state != ERROR_STATE) {
    switch (read_block(infp, block_data)) {
    case EOF:
      if (state == END_OF_STREAM_STATE) {
        err = 0; /* success */
        goto cleanup;
      }
      /* FALLTHROUGH */
    case TOO_SHORT_DATA_BLOCK:
      if (feof_unlocked(infp)) {
        print_error("Unexpected end of file\n");
      } else {
        print_error("Failed to read a file: %s\n", strerror(errno));
      }
      goto cleanup;
    }
    state = process_block(outfp, state, block_data, work, work_len);
  }
 cleanup:
  free(block_data);
  free(work);
  return err;
}


static int read_block(FILE *fp, block_data_t *bd)
{
  int chr;

  /* read block type */
  chr = getc_unlocked(fp);
  if (chr == EOF) {
    return EOF;
  }
  bd->type = chr;

  /* read data length */
  bd->data_len = chr = getc_unlocked(fp);
  if (chr == EOF) {
    return TOO_SHORT_DATA_BLOCK;
  }

  bd->data_len |= ((chr = getc_unlocked(fp)) << 8);
  if (chr == EOF) {
    return TOO_SHORT_DATA_BLOCK;
  }

  /* read data */
  if (bd->data_len > 0 && fread_unlocked(bd->data, bd->data_len, 1, fp) != 1) {
    return TOO_SHORT_DATA_BLOCK;
  }
  return SUCCESS;
}

static stream_state_t process_block(FILE *fp, stream_state_t state, block_data_t *bd, char *work, size_t work_len)
{
  unsigned int crc32c;
  size_t outlen;

  switch (state) {
  case INITIAL_STATE:
  case END_OF_STREAM_STATE:
    /* the next block must be a header block. */

    if (bd->type != HEADER_TYPE_CODE) {
      print_error("Invaid file format\n");
      return ERROR_STATE;
    }
    if (bd->data_len != 6) {
      print_error("invalid data length %d for header block\n", bd->data_len);
      return ERROR_STATE;
    }
    if (memcmp(bd->data, "snappy", 6) != 0) {
      print_error("invalid file header\n");
      return ERROR_STATE;
    }
    return PROCESSING_STATE;

  case PROCESSING_STATE:

    switch (bd->type) {
    case COMPRESSED_TYPE_CODE:
      if (bd->data_len <= 4) {
        print_error("too short data length for compressed data block\n");
        return ERROR_STATE;
      }
      crc32c  = ((unsigned char)bd->data[0] << 0);
      crc32c |= ((unsigned char)bd->data[1] << 8);
      crc32c |= ((unsigned char)bd->data[2] << 16);
      crc32c |= ((unsigned char)bd->data[3] << 24);

      /* uncompress and write */
      outlen = work_len;
      if (snappy_uncompress(bd->data + 4, bd->data_len - 4, work, &outlen)) {
        print_error("Invalid data: RawUncompress failed\n");
        return ERROR_STATE;
      }
      if (crc32c != masked_crc32c(work, outlen)) {
        print_error("Invalid data: CRC32c error\n");
        return ERROR_STATE;
      }
      if (fwrite_unlocked(work, outlen, 1, fp) != 1) {
        print_error("Failed to write: %s\n", strerror(errno));
        return ERROR_STATE;
      }
      break;
    case UNCOMPRESSED_TYPE_CODE:
      if (bd->data_len <= 4) {
        print_error("too short data length for uncompressed data block\n");
        return ERROR_STATE;
      }
      crc32c  = ((unsigned char)bd->data[0] << 0);
      crc32c |= ((unsigned char)bd->data[1] << 8);
      crc32c |= ((unsigned char)bd->data[2] << 16);
      crc32c |= ((unsigned char)bd->data[3] << 24);

      if (crc32c != masked_crc32c(bd->data + 4, bd->data_len - 4)) {
        print_error("Invalid data: CRC32c error\n");
        return ERROR_STATE;
      }
      if (fwrite_unlocked(bd->data + 4, bd->data_len - 4, 1, fp) != 1) {
        print_error("Failed to write: %s\n", strerror(errno));
        return ERROR_STATE;
      }
      break;
    case END_OF_STREAM_TYPE_CODE:
      if (bd->data_len != 0) {
        print_error("invalid data length for end-of-stream block\n");
        return ERROR_STATE;
      }
      return END_OF_STREAM_STATE;
    case HEADER_TYPE_CODE:
      print_error("Invalid data: unexpected header\n");
      return ERROR_STATE;
    default:
      if (bd->type < 0x80) {
        print_error("Invalid data: unknown block type %d\n", bd->type);
        return ERROR_STATE;
      }
    }
    return PROCESSING_STATE;

  case ERROR_STATE:
    ;
  }
  /* never reach here. This is added to suppress a warning */
  return ERROR_STATE;
}

stream_format_t comment_43_format = {
  "comment-43",
  "http://code.google.com/p/snappy/issues/detail?id=34#c43",
  "snappy",
  comment_43_compress,
  comment_43_uncompress,
};
