/* -*- indent-tabs-mode: nil -*-
 *
 * Copyright 2011-2016 Kubo Takehiro <kubo@jiubao.org>
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
#ifndef SNZIP_H
#define SNZIP_H 1
#include <stdio.h>
#include <stdint.h>

#ifndef __GNUC__
#define __attribute__(attr)
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* unlocked stdio functions */
#if defined _IO_getc_unlocked
#define getc_unlocked _IO_getc_unlocked
#elif !defined HAVE_GETC_UNLOCKED
#define getc_unlocked getc
#endif

#if defined _IO_putc_unlocked
#define putc_unlocked _IO_putc_unlocked
#elif !defined HAVE_PUTC_UNLOCKED
#define putc_unlocked putc
#endif

#if !defined HAVE_FREAD_UNLOCKED
#define fread_unlocked fread
#endif

#if !defined HAVE_FWRITE_UNLOCKED
#define fwrite_unlocked fwrite
#endif

#if defined _IO_ferror_unlocked
#define ferror_unlocked _IO_ferror_unlocked
#elif !defined HAVE_FERROR_UNLOCKED
#define ferror_unlocked ferror
#endif

#if defined _IO_feof_unlocked
#define feof_unlocked _IO_feof_unlocked
#elif !defined HAVE_FEOF_UNLOCKED
#define feof_unlocked feof
#endif

#ifdef HAVE_BYTESWAP_H
#include <byteswap.h>
#endif

#if defined bswap_32
#define SNZ_BSWAP32(x) bswap_32(x)  /* in byteswap.h (linux) */
#elif defined _MSC_VER
#include <intrin.h>
#define SNZ_BSWAP32(x) _byteswap_ulong(x)  /* in intrin.h (msvc) */
#else
#define SNZ_BSWAP32(x) \
    ((((x) >> 24) & 0x000000ffu) | \
     (((x) >> 8)  & 0x0000ff00u) | \
     (((x) << 8)  & 0x00ff0000u) | \
     (((x) << 24) & 0xff000000u))
#endif

#ifdef WORDS_BIGENDIAN
#define SNZ_TO_LE32(x)    SNZ_BSWAP32(x)
#define SNZ_FROM_LE32(x)  SNZ_BSWAP32(x)
#define SNZ_TO_BE32(x)    (x)
#define SNZ_FROM_BE32(x)  (x)
#else
#define SNZ_TO_LE32(x)    (x)
#define SNZ_FROM_LE32(x)  (x)
#define SNZ_TO_BE32(x)    SNZ_BSWAP32(x)
#define SNZ_FROM_BE32(x)  SNZ_BSWAP32(x)
#endif

/* logging functions */
extern int trc_lineno;
extern const char *trc_filename;
void print_error_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void trace_(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#define print_error (trc_filename = __FILE__, trc_lineno = __LINE__, print_error_)
#define trace (trc_filename = __FILE__, trc_lineno = __LINE__, trace_)

/* utility functions */
typedef struct {
  size_t clen; /* maximum length of compressed data */
  size_t uclen; /* maximum length of uncompressed data */
  char *c; /* buffer for compressed data */
  char *uc; /* buffer for uncompressed data */
} work_buffer_t;

int work_buffer_init(work_buffer_t *wb, size_t block_size);
void work_buffer_free(work_buffer_t *wb);
void work_buffer_resize(work_buffer_t *wb, size_t clen, size_t uclen);

int write_full(int fd, const void *buf, size_t count);

/* */
typedef struct {
  const char *name;
  const char *url;
  const char *suffix;
  int (*compress)(FILE *infp, FILE *outfp, size_t block_size);
  int (*uncompress)(FILE *infp, FILE *outfp, int skip_magic);
} stream_format_t;

extern int64_t uncompressed_source_len;
extern int32_t snzip_format_block_size;
extern uint32_t hadoop_snappy_source_length;
extern uint32_t hadoop_snappy_compressed_length;

extern stream_format_t snzip_format;
extern stream_format_t framing_format;
extern stream_format_t framing2_format;
extern stream_format_t snappy_java_format;
extern stream_format_t snappy_in_java_format;
extern stream_format_t comment_43_format;
extern stream_format_t raw_format;
extern stream_format_t hadoop_snappy_format;

/* hadoop-snapp-format.c */
size_t hadoop_snappy_max_input_size(size_t block_size);

#endif /* SNZIP_H */
