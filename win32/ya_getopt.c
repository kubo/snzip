/* -*- indent-tabs-mode: nil -*-
 *
 * ya_getopt  - Yet another getopt
 * https://github.com/kubo/ya_getopt
 *
 * Copyright 2015 Kubo Takehiro <kubo@jiubao.org>
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ya_getopt.h"

char *ya_optarg = NULL;
int ya_optind = 1;
int ya_opterr = 1;
int ya_optopt = '?';

static int optstarts(const char *os, char opt)
{
    while (1) {
        switch (*os) {
        case ':':
        case '+':
        case '-':
            if (*os == opt) {
                return 1;
            }
            break;
        default:
            return 0;
        }
        os++;
    }
}

int ya_getopt(int argc, char * const argv[], const char *optstring)
{
    static int idx = 0;
    static int start, end;
    const char *arg;
    const char *os;
    int opt;

    if (ya_optopt == '?') {
        ya_optopt = 0;
    }
    if (start != 0) {
        int last_pos = ya_optind - 1;

        ya_optind -= end - start;
        while (start < end--) {
            int i;
            arg = argv[end];

            for (i = end; i < last_pos; i++) {
                ((char **)argv)[i] = argv[i + 1];
            }
            ((char const **)argv)[i] = arg;
            last_pos--;
        }
        start = 0;
    }

    if (ya_optind >= argc) {
        ya_optarg = NULL;
        return -1;
    }
    arg = argv[ya_optind];
    if (idx == 0) {
        if (*arg != '-') {
            if (optstring[0] != '+' && getenv("POSIXLY_CORRECT") == NULL) {
                /* GNU extension */
                int i;
                if (optstring[0] == '-') {
                    ya_optarg = argv[optind++];
                    start = 0;
                    return 1;
                }

                start = ya_optind;
                for (i = ya_optind + 1; i < argc; i++) {
                    if (argv[i][0] == '-') {
                        end = i;
                        break;
                    }
                }
                if (i == argc) {
                    ya_optarg = NULL;
                    return -1;
                }
                ya_optind = i;
                arg = argv[ya_optind];
            } else {
                /* POSIX  */
                if (optstring[0] == '-') {
                    ya_optarg = argv[optind++];
                    start = 0;
                    return 1;
                }
                ya_optarg = NULL;
                return -1;
            }
        }
        if (strcmp(arg, "--") == 0) {
            ya_optind++;
            return -1;
        }
        idx = 1;
    }
    opt = arg[idx];
    for (os = optstring; *os != 0; os++) {
        if (os == optstring && *os == '-') {
            continue;
        }
        if (opt == *os) {
            break;
        }
    }
    if (*os == 0) {
        ya_optarg = NULL;
        ya_optopt = opt;
        if (ya_opterr && !optstarts(optstring, ':')) {
            fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], opt);
        }
        if (arg[++idx] == 0) {
            ya_optind++;
            idx = 0;
        }
        return '?';
    }
    if (os[1] == ':') {
        if (argv[ya_optind][idx + 1] == 0) {
            ya_optind++;
            if (os[2] == ':') {
                /* optional argument */
                ya_optarg = NULL;
            } else {
                if (ya_optind == argc) {
                    ya_optarg = NULL;
                    ya_optopt = opt;
                    if (ya_opterr && !optstarts(optstring, ':')) {
                        fprintf(stderr, "%s: option requires an argument -- '%c'\n", argv[0], opt);
                    }
                    if (optstarts(optstring, ':')) {
                        return ':';
                    } else {
                        return '?';
                    }
                }
                ya_optarg = argv[ya_optind];
                ya_optind++;
            }
        } else {
            ya_optarg = argv[ya_optind] + idx + 1;
            ya_optind++;
        }
        idx = 0;
    } else {
        ya_optarg = NULL;
        if (argv[ya_optind][idx + 1] == 0) {
            idx = 0;
            ya_optind++;
        } else {
            idx++;
        }
    }
    return opt;
}
