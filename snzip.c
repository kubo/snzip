/* -*- indent-tabs-mode: nil -*-
 *
 * Copyright 2011 Kubo Takehiro <kubo@jiubao.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *     * Neither the name of authors nor the names of its contributors
 *       may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <fcntl.h>
#include <snappy-c.h>
#ifdef WIN32
/* Windows */
#include <windows.h>
#include <io.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#define PATH_DELIMITER '\\'
#define OPTIMIZE_SEQUENTIAL "S" /* flag to optimize sequential access */
#else
/* Unix */
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#define PATH_DELIMITER '/'
#define OPTIMIZE_SEQUENTIAL ""
#endif

#define SNZ_SUFFIX ".snz"
#define SNZ_SUFFIX_LEN 4

#define SNZ_MAGIC "SNZ"
#define SNZ_MAGIC_LEN 3
#define SNZ_FILE_VERSION 1

#define SNZ_DEFAULT_BLOCK_SIZE 16 /* (1 << 16) => 64 KiB */
#define SNZ_MAX_BLOCK_SIZE 27 /* (1 << 27) => 128 MiB */

#if defined HAVE_STRUCT_STAT_ST_MTIMENSEC
#define SNZ_ST_TIME_NSEC(sbuf, type) ((sbuf).st_##type##timensec)
#elif defined HAVE_STRUCT_STAT_ST_MTIM_TV_NSEC
#define SNZ_ST_TIME_NSEC(sbuf, type) ((sbuf).st_##type##tim.tv_nsec)
#elif defined HAVE_STRUCT_STAT_ST_MTIMESPEC_TV_NSEC
#define SNZ_ST_TIME_NSEC(sbuf, type) ((sbuf).st_##type##timespec.tv_nsec)
#else
#define SNZ_ST_TIME_NSEC(sbuf, type) (0)
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#define VARINT_MAX 5

#ifndef fputc_unlocked
#define fputc_unlocked fputc
#endif

#ifndef fgetc_unlocked
#define fgetc_unlocked fgetc
#endif

static int trace_flag = FALSE;

typedef struct {
  char magic[SNZ_MAGIC_LEN]; /* SNZ_MAGIC */
  char version;  /* SNZ_FILE_VERSION */
  unsigned char block_size; /* nth power of two. */
} snz_header_t;

typedef struct {
  size_t clen; /* maximum length of compressed data */
  size_t uclen; /* maximum length of uncompressed data */
  char *c; /* buffer for compressed data */
  char *uc; /* buffer for uncompressed data */
} work_buffer_t;

static int compress(FILE *infp, FILE *outfp, int block_size);
static int uncompress(FILE *infp, FILE *outfp);
static void copy_file_attributes(int infd, int outfd, const char *outfile);
static void show_usage(const char *progname, int exit_code);
static int work_buffer_init(work_buffer_t *wb, int block_size);
static void work_buffer_free(work_buffer_t *wb);

static int lineno;

#ifdef __GNUC__
static void print_error(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void trace(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#endif

static void print_error(const char *fmt, ...)
{
  va_list ap;

  if (trace_flag) {
    fprintf(stderr, "%s:%3d: ", __FILE__, lineno);
  }
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
#define print_error (lineno = __LINE__, print_error)

static void trace(const char *fmt, ...)
{
  va_list ap;

  if (!trace_flag) {
    return;
  }
  fprintf(stderr, "%s:%3d: ", __FILE__, lineno);
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
}
#define trace (lineno = __LINE__, trace)

static int write_full(int fd, const void *buf, size_t count)
{
  const char *ptr = (const char *)buf;

  while (count > 0) {
    int rv = write(fd, ptr, count);
    if (rv == -1) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    ptr += rv;
    count -= rv;
  }
  return (ptr - (const char *)buf);
}

int main(int argc, char **argv)
{
  int opt;
  int opt_uncompress = FALSE;
  int opt_stdout = FALSE;
  int block_size = SNZ_DEFAULT_BLOCK_SIZE;
  size_t rsize = 0;
  size_t wsize = 0;

  char *progname = strrchr(argv[0], PATH_DELIMITER);
  if (progname != NULL) {
    progname++;
  } else {
    progname = argv[0];
  }

  trace("progname = %s\n", progname);
  if (strstr(progname, "un") != NULL) {
    trace("\"un\" is found in %s\n", progname);
    opt_uncompress = TRUE;
  }
  if (strstr(progname, "cat") != NULL) {
    trace("\"cat\" is found in %s\n", progname);
    opt_stdout = TRUE;
    opt_uncompress = TRUE;
  }

  while ((opt = getopt(argc, argv, "cdhB:R:W:T")) != -1) {
    switch (opt) {
    case 'c':
      opt_stdout = TRUE;
      break;
    case 'd':
      opt_uncompress = TRUE;
      break;
    case 'h':
      show_usage(progname, 0);
      break;
    case 'B':
      block_size = atoi(optarg);
      if (block_size < 1 || block_size > SNZ_MAX_BLOCK_SIZE) {
        print_error("Invalid block size %d (max %d)\n", block_size, SNZ_MAX_BLOCK_SIZE);
        exit(1);
      }
      break;
    case 'R':
      rsize = strtoul(optarg, NULL, 10);
      break;
    case 'W':
      wsize = strtoul(optarg, NULL, 10);
      break;
    case 'T':
      trace_flag = TRUE;
      break;
    case '?':
      show_usage(progname, 1);
      break;
    }
  }

#ifdef WIN32
  _setmode(0, _O_BINARY);
  _setmode(1, _O_BINARY);
#endif

  if (optind == argc) {
    trace("no arguments are set.\n");
    if (isatty(1)) {
      /* stdout is a terminal */
      fprintf(stderr, "I won't write compressed data to a terminal.\n");
      fprintf(stderr, "For help, type: '%s -h'.\n", progname);
      return 1;
    }

    if (opt_uncompress) {
      return uncompress(stdin, stdout);
    } else {
      return compress(stdin, stdout, block_size);
    }
  }

  while (optind < argc) {
    char *infile = argv[optind++];
    size_t infilelen = strlen(infile);
    char outfile[PATH_MAX];
    int has_snp_suffix;
    FILE *infp;
    FILE *outfp;

    /* check suffix */
    const char *suffix = strrchr(infile, '.');
    if (suffix == NULL) {
      suffix = "";
    }
    has_snp_suffix = (strcmp(suffix, SNZ_SUFFIX) == 0);

    if (opt_uncompress) {

      if (!has_snp_suffix) {
        print_error("%s has unknown suffix.\n", infile);
        continue;
      }
      if (opt_stdout) {
        strcpy(outfile, "-");
      } else {
        if (infilelen - SNZ_SUFFIX_LEN >= sizeof(outfile)) {
          print_error("%s has too long file name.\n", infile);
        }
        memcpy(outfile, infile, infilelen - SNZ_SUFFIX_LEN);
        outfile[infilelen - SNZ_SUFFIX_LEN] = '\0';
      }
    } else {
      if (has_snp_suffix) {
        print_error("%s already has %s suffix\n", infile, SNZ_SUFFIX);
        continue;
      }
      if (opt_stdout) {
        strcpy(outfile, "-");
      } else {
        if (infilelen + SNZ_SUFFIX_LEN >= sizeof(outfile)) {
          print_error("%s has too long file name.\n", infile);
        }
        strcpy(outfile, infile);
        strcat(outfile, SNZ_SUFFIX);
      }
    }

    infp = fopen(infile, "rb" OPTIMIZE_SEQUENTIAL);
    if (infp == NULL) {
      print_error("Failed to open %s for read\n", infile);
      exit(1);
    }
    if (rsize != 0) {
      trace("setvbuf(infp, NULL, _IOFBF, %ld)\n", (long)rsize);
      setvbuf(infp, NULL, _IOFBF, rsize);
    }
#ifdef HAVE_POSIX_FADVISE
    posix_fadvise(fileno(infp), 0, 0, POSIX_FADV_SEQUENTIAL);
#endif

    if (opt_stdout) {
      outfp = stdout;
    } else {
      outfp = fopen(outfile, "wb" OPTIMIZE_SEQUENTIAL);
      if (outfp == NULL) {
        print_error("Failed to open %s for write\n", outfile);
        exit(1);
      }
    }
    if (wsize != 0) {
      trace("setvbuf(outfp, NULL, _IOFBF, %ld)\n", (long)wsize);
      setvbuf(outfp, NULL, _IOFBF, wsize);
    }

    if (opt_uncompress) {
      trace("uncompress %s\n", infile);
      if (uncompress(infp, outfp) != 0) {
        if (outfp != stdout) {
          unlink(outfile);
        }
        return 1;
      }
    } else {
      trace("compress %s\n", infile);
      if (compress(infp, outfp, block_size) != 0) {
        if (outfp != stdout) {
          unlink(outfile);
        }
        return 1;
      }
    }

    if (!opt_stdout) {
      fflush(outfp);
      copy_file_attributes(fileno(infp), fileno(outfp), outfile);
    }

    fclose(infp);
    if (outfp != stdout) {
      fclose(outfp);
    }

    if (!opt_stdout) {
      int rv = unlink(infile);
      trace("unlink(\"%s\") => %d (errno = %d)\n",
            infile, rv, rv ? errno : 0);
    }
  }
  return 0;
}

static int compress(FILE *infp, FILE *outfp, int block_size)
{
  snz_header_t header;
  work_buffer_t wb;
  size_t uncompressed_length;
  int err = 1;

  wb.c = NULL;
  wb.uc = NULL;

  /* write the file header */
  memcpy(header.magic, SNZ_MAGIC, SNZ_MAGIC_LEN);
  header.version = SNZ_FILE_VERSION;
  header.block_size = block_size;

  if (fwrite(&header, sizeof(header), 1, outfp) != 1) {
    print_error("Failed to write a file: %s\n", strerror(errno));
    goto cleanup;
  }

  /* write file body */
  work_buffer_init(&wb, block_size);
  while ((uncompressed_length = fread(wb.uc, 1, wb.uclen, infp)) > 0) {
    size_t compressed_length = wb.clen;

    trace("read %lu bytes.\n", (unsigned long)uncompressed_length);

    /* compress the block. */
    snappy_compress(wb.uc, uncompressed_length, wb.c, &compressed_length);
    trace("compressed_legnth is %lu.\n", (unsigned long)compressed_length);

    /* write the compressed length. */
    if (compressed_length < (1ul << 7)) {
      fputc_unlocked(compressed_length, outfp);
      trace("write 1 byte for compressed data length.\n");
    } else if (compressed_length < (1ul << 14)) {
      fputc_unlocked((compressed_length >> 0) | 0x80, outfp);
      fputc_unlocked((compressed_length >> 7), outfp);
      trace("write 2 bytes for compressed data length.\n");
    } else if (compressed_length < (1ul << 21)) {
      fputc_unlocked((compressed_length >> 0) | 0x80, outfp);
      fputc_unlocked((compressed_length >> 7) | 0x80, outfp);
      fputc_unlocked((compressed_length >> 14), outfp);
      trace("write 3 bytes for compressed data length.\n");
    } else if (compressed_length < (1ul << 28)) {
      fputc_unlocked((compressed_length >> 0) | 0x80, outfp);
      fputc_unlocked((compressed_length >> 7) | 0x80, outfp);
      fputc_unlocked((compressed_length >> 14)| 0x80, outfp);
      fputc_unlocked((compressed_length >> 21), outfp);
      trace("write 4 bytes for compressed data length.\n");
    } else {
      fputc_unlocked((compressed_length >> 0) | 0x80, outfp);
      fputc_unlocked((compressed_length >> 7) | 0x80, outfp);
      fputc_unlocked((compressed_length >> 14)| 0x80, outfp);
      fputc_unlocked((compressed_length >> 21)| 0x80, outfp);
      fputc_unlocked((compressed_length >> 28), outfp);
      trace("write 5 bytes for compressed data length.\n");
    }

    /* write the compressed data. */
    if (fwrite(wb.c, compressed_length, 1, outfp) != 1) {
      print_error("Failed to write a file: %s\n", strerror(errno));
      goto cleanup;
    }
    trace("write %ld bytes for compressed data.\n", (long)compressed_length);
  }
  if (!feof(infp)) {
    /* fread() failed. */
    print_error("Failed to read a file: %s\n", strerror(errno));
    goto cleanup;
  }
  fputc_unlocked('\0', outfp);
  trace("write 1 byte\n");
  err = 0;
 cleanup:
  work_buffer_free(&wb);
  return err;
}

static int uncompress(FILE *infp, FILE *outfp)
{
  snz_header_t header;
  work_buffer_t wb;
  int err = 1;
  int outfd;

  wb.c = NULL;
  wb.uc = NULL;

  /* read header */
  if (fread(&header, sizeof(header), 1, infp) != 1) {
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
  work_buffer_init(&wb, header.block_size);
  for (;;) {
    /* read the compressed length in a block */
    size_t compressed_length = 0;
    size_t uncompressed_length = wb.uclen;
    int idx;

    for (idx = 0; idx < VARINT_MAX; idx++) {
      int chr = fgetc_unlocked(infp);
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
    if (fread(wb.c, compressed_length, 1, infp) != 1) {
      if (feof(infp)) {
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

static void copy_file_attributes(int infd, int outfd, const char *outfile)
{
#ifdef WIN32
  BY_HANDLE_FILE_INFORMATION fi;
  BOOL bOk;

  bOk = GetFileInformationByHandle((HANDLE)_get_osfhandle(infd), &fi);
  trace("GetFileInformationByHandle(...) => %s\n", bOk ? "TRUE" : "FALSE");
  if (bOk) {
    bOk = SetFileTime((HANDLE)_get_osfhandle(outfd), NULL, &fi.ftLastAccessTime, &fi.ftLastWriteTime);
    trace("SetFileTime(...) => %s\n", bOk ? "TRUE" : "FALSE");
    bOk = SetFileAttributesA(outfile, fi.dwFileAttributes);
    trace("SetFileAttributesA(...) => %s\n", bOk ? "TRUE" : "FALSE");
  }
#else
  struct stat sbuf;
#ifdef HAVE_FUTIMENS
  struct timespec times[2];
#else
  struct timeval times[2];
#endif
  int rv;

  if ((rv = fstat(infd, &sbuf)) != 0) {
    trace("fstat(%d, &sbuf) => %d (errno = %d)\n",
          infd, rv, errno);
    return;
  }

  /* copy file times. */
#ifdef HAVE_FUTIMENS
  times[0].tv_sec = sbuf.st_atime;
  times[0].tv_nsec = SNZ_ST_TIME_NSEC(sbuf, a);
  times[1].tv_sec = sbuf.st_mtime;
  times[1].tv_nsec = SNZ_ST_TIME_NSEC(sbuf, m);
  rv = futimens(outfd, times);
  trace("futimens(%d, [{%ld, %ld}, {%ld, %ld}]) => %d\n",
        outfd, times[0].tv_sec, times[0].tv_nsec,
        times[1].tv_sec, times[1].tv_nsec, rv);
#else /* HAVE_FUTIMENS */
  times[0].tv_sec = sbuf.st_atime;
  times[0].tv_usec = SNZ_ST_TIME_NSEC(sbuf, a) / 1000;
  times[1].tv_sec = sbuf.st_mtime;
  times[1].tv_usec = SNZ_ST_TIME_NSEC(sbuf, m) / 1000;
#ifdef HAVE_FUTIMES
  rv = futimes(outfd, times);
  trace("futimes(%d, [{%ld, %ld}, {%ld, %ld}]) => %d\n",
        outfd, times[0].tv_sec, times[0].tv_usec,
        times[1].tv_sec, times[1].tv_usec, rv);
#else /* HAVE_FUTIMES */
  rv = utimes(outfile, times);
  trace("utimes(\"%s\", [{%ld, %ld}, {%ld, %ld}]) => %d\n",
        outfile, times[0].tv_sec, times[0].tv_usec,
        times[1].tv_sec, times[1].tv_usec, rv);
#endif /* HAVE_FUTIMES */
#endif /* HAVE_FUTIMENS */

  /* copy other attributes */
  rv = fchown(outfd, sbuf.st_uid, sbuf.st_gid);
  trace("fchown(%d, %d, %d) => %d\n",
        outfd, sbuf.st_uid, sbuf.st_gid, rv);
  rv = fchmod(outfd, sbuf.st_mode);
  trace("fchmod(%d, 0%o) => %d\n",
        outfd, sbuf.st_mode, rv);
#endif
}

static void show_usage(const char *progname, int exit_code)
{
  fprintf(stderr,
          PACKAGE_STRING "\n"
          "\n"
          "  usage: %s [option ...] [file ...]\n"
          "\n"
          "  general options:\n"
          "   -c       output to standard output, keep original files unchanged\n"
          "   -d       decompress\n"
          "   -h       give this help\n"
          "\n"
          "  tuning options:\n"
          "   -B num   internal block size. 'num'-th power of two. (default is %d.)\n"
          "   -R num   size of read buffer in bytes\n"
          "   -W num   size of write buffer in bytes\n"
          "   -T       trace for debug\n",
          progname, SNZ_DEFAULT_BLOCK_SIZE);
  exit(exit_code);
}

static int work_buffer_init(work_buffer_t *wb, int block_size)
{
  wb->uclen = (1 << block_size);
  wb->uc = malloc(wb->uclen);
  if (wb->uc == NULL) {
    return ENOMEM;
  }
  wb->clen = snappy_max_compressed_length(wb->uclen);
  wb->c = malloc(wb->clen);
  if (wb->c == NULL) {
    return ENOMEM;
  }
  trace("max length of compressed data = %lu\n", wb->clen);
  trace("max length of uncompressed data = %lu\n", wb->uclen);
  return 0;
}

static void work_buffer_free(work_buffer_t *wb)
{
  free(wb->c);
  free(wb->uc);
}
