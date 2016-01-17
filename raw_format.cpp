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
#include <snappy.h>
#include <snappy-sinksource.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef WIN32
#include <io.h>
#define stat _stati64
#define fstat _fstati64
#define read _read
#endif
extern "C" {
#include "snzip.h"
}

namespace snzip {

  class FileSink : public snappy::Sink {
  public:

    FileSink(int fd) : fd_(fd), err_(false) {
      trace("Initialize FileSink\n");
    }

    ~FileSink() {
      trace("Uninitialize FileSink\n");
    }

    virtual void Append(const char* bytes, size_t n) {
      if (err_) {
        return;
      }
      trace("write %lu bytes\n", n);
      if (write_full(fd_, bytes, n) != (int)n) {
        print_error("Failed to write a file: %s\n", strerror(errno));
        err_ = true;
      }
    }

    bool err() {
      return err_;
    }
  private:
    int fd_;
    bool err_;
  };

  class FileSource : public snappy::Source {
  public:

    FileSource(int fd, int64_t filesize, size_t bufsiz) : fd_(fd), restsize_(filesize), err_(false) {
      ptr_ = end_ = base_ = new char[bufsiz];
      limit_ = base_ + bufsiz;
    }

    virtual ~FileSource() {
      delete[] base_;
    }

    virtual size_t Available() const {
      trace("Available %lu bytes\n", restsize_);
      return restsize_;
    }

    virtual const char* Peek(size_t* len) {
      if (restsize_ == 0) {
        *len = 0;
        trace("Peek 0 bytes\n");
        return NULL;
      }
      if (ptr_ == end_) {
        int read_len;
        do {
          read_len = read(fd_, base_, limit_ - base_);
        } while (read_len == -1 && errno == EINTR);
        if (read_len == -1) {
          print_error("Failed to read a file: %s\n", strerror(errno));
          restsize_ = 0;
          err_ = true;
          *len = 0;
          return NULL;
        }
        trace("Read %d bytes\n", read_len);
        ptr_ = base_;
        end_ = base_ + read_len;
        *len = read_len;
      } else {
        *len = end_ - ptr_;
      }
      if (restsize_ > 0 && (int64_t)*len > restsize_) {
        *len = restsize_;
      }
      trace("Peek %lu bytes\n", *len);
      return ptr_;
    }

    virtual void Skip(size_t n) {
      ptr_ += n;
      if (restsize_ > 0) {
        restsize_ -= n;
      }
      trace("Skip %lu bytes\n", n);
    }

    bool err() {
      return err_;
    }

  private:
    int fd_;
    char *ptr_;
    char *end_;
    char *base_;
    char *limit_;
    int64_t restsize_;
    bool err_;
  };
}

static int raw_compress(FILE *infp, FILE *outfp, size_t block_size)
{
  int64_t filesize = uncompressed_source_len;

  if (block_size == 0) {
    block_size = snappy::kBlockSize;
  }

  if (filesize == -1) {
    struct stat sbuf;

    if (fstat(fileno(infp), &sbuf) == 0) {
      filesize = sbuf.st_size;
    } else {
      print_error("ERROR: Cannot get file size\n");
      return 1;
    }
  }

  snzip::FileSource src(fileno(infp), filesize, block_size);
  snzip::FileSink dst(fileno(outfp));
  if (!snappy::Compress(&src, &dst)) {
    print_error("Invalid data: snappy::Compress failed\n");
    return 1;
  }
  return src.err() || dst.err();
}

static int raw_uncompress(FILE *infp, FILE *outfp, int skip_magic)
{
  snzip::FileSource src(fileno(infp), -1, snappy::kBlockSize);
  snzip::FileSink dst(fileno(outfp));
  if (!snappy::Uncompress(&src, &dst)) {
    print_error("Invalid data: snappy::Uncompress failed\n");
    return 1;
  }
  return src.err() || dst.err();
}

stream_format_t raw_format = {
  "raw",
  "https://github.com/google/snappy/blob/master/format_description.txt",
  "raw",
  raw_compress,
  raw_uncompress,
};
