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
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "ya_getopt.h"

char *ya_optarg = NULL;
int ya_optind = 1;
int ya_opterr = 1;
int ya_optopt = '?';
static int ya_charidx = 1;

static int optstarts(const char *os, char opt);
static void ya_getopt_error(const char *optstring, const char *format, ...);
static int ya_getopt_internal(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex, int long_only);
static int ya_getopt_shortopts(int argc, char * const argv[], const char *optstring, int long_only);
static int ya_getopt_longopts(int argc, char * const argv[], int offset, const char *optstring, const struct option *longopts, int *longindex, int *long_only_flag);

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

static void ya_getopt_error(const char *optstring, const char *format, ...)
{
    if (ya_opterr && !optstarts(optstring, ':')) {
        va_list ap;
        va_start(ap, format);
        vfprintf(stderr, format, ap);
        va_end(ap);
    }
}

int ya_getopt(int argc, char * const argv[], const char *optstring)
{
    return ya_getopt_internal(argc, argv, optstring, NULL, NULL, 0);
}

int ya_getopt_long(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex)
{
    return ya_getopt_internal(argc, argv, optstring, longopts, longindex, 0);
}

int ya_getopt_long_only(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex)
{
    return ya_getopt_internal(argc, argv, optstring, longopts, longindex, 1);
}

static int ya_getopt_internal(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex, int long_only)
{
    static int start, end;
    const char *arg;

    if (ya_optopt == '?') {
        ya_optopt = 0;
    }
    if (ya_charidx == 1 && start != 0) {
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
    if (ya_charidx == 1) {
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
        if (longopts != NULL && arg[1] == '-') {
            return ya_getopt_longopts(argc, argv, 2, optstring, longopts, longindex, NULL);
        }
    }

    if (long_only) {
        int long_only_flag = 0;
        int rv = ya_getopt_longopts(argc, argv, ya_charidx, optstring, longopts, longindex, &long_only_flag);
        if (!long_only_flag) {
            ya_charidx = 1;
            return rv;
        }
    }

    return ya_getopt_shortopts(argc, argv, optstring, long_only);
}

static int ya_getopt_shortopts(int argc, char * const argv[], const char *optstring, int long_only)
{
    const char *arg = argv[ya_optind];
    int opt = arg[ya_charidx];
    const char *os = optstring;

    switch (*os) {
    case '+':
    case '-':
    case ':':
        os++;
    }

    while (*os != 0) {
        if (opt == *os) {
            break;
        }
        os++;
    }
    if (*os == 0) {
        ya_optarg = NULL;
        if (long_only) {
            ya_getopt_error(optstring, "%s: unrecognized option '-%s'\n", argv[0], argv[ya_optind] + ya_charidx);
            ya_optind++;
            ya_charidx = 1;
        } else {
            ya_optopt = opt;
            ya_getopt_error(optstring, "%s: invalid option -- '%c'\n", argv[0], opt);
            if (arg[++ya_charidx] == 0) {
                ya_optind++;
                ya_charidx = 1;
            }
        }
        return '?';
    }
    if (os[1] == ':') {
        if (argv[ya_optind][ya_charidx + 1] == 0) {
            ya_optind++;
            if (os[2] == ':') {
                /* optional argument */
                ya_optarg = NULL;
            } else {
                if (ya_optind == argc) {
                    ya_optarg = NULL;
                    ya_optopt = opt;
                    ya_getopt_error(optstring, "%s: option requires an argument -- '%c'\n", argv[0], opt);
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
            ya_optarg = argv[ya_optind] + ya_charidx + 1;
            ya_optind++;
        }
        ya_charidx = 1;
    } else {
        ya_optarg = NULL;
        if (argv[ya_optind][ya_charidx + 1] == 0) {
            ya_charidx = 1;
            ya_optind++;
        } else {
            ya_charidx++;
        }
    }
    return opt;
}

static int ya_getopt_longopts(int argc, char * const argv[], int offset, const char *optstring, const struct option *longopts, int *longindex, int *long_only_flag)
{
    char *arg = argv[ya_optind] + offset;
    char *val = NULL;
    const struct option *opt;
    size_t namelen;
    int idx;

    for (idx = 0; longopts[idx].name != NULL; idx++) {
        opt = &longopts[idx];
        namelen = strlen(opt->name);
        if (strncmp(arg, opt->name, namelen) == 0) {
            switch (arg[namelen]) {
            case '\0':
                switch (opt->has_arg) {
                case ya_required_argument:
                    ya_optind++;
                    if (ya_optind == argc) {
                        ya_optarg = NULL;
                        ya_optopt = opt->val;
                        ya_getopt_error(optstring, "%s: option '--%s' requires an argument\n", argv[0], opt->name);
                        if (optstarts(optstring, ':')) {
                            return ':';
                        } else {
                            return '?';
                        }
                    }
                    val = argv[ya_optind];
                    break;
                }
                goto found;
            case '=':
                if (opt->has_arg == ya_no_argument) {
                    const char *hyphens = (argv[ya_optind][1] == '-') ? "--" : "-";

                    ya_optind++;
                    ya_optarg = NULL;
                    ya_optopt = opt->val;
                    ya_getopt_error(optstring, "%s: option '%s%s' doesn't allow an argument\n", argv[0], hyphens, opt->name);
                    return '?';
                }
                val = arg + namelen + 1;
                goto found;
            }
        }
    }
    if (long_only_flag) {
        *long_only_flag = 1;
    } else {
        ya_getopt_error(optstring, "%s: unrecognized option '%s'\n", argv[0], argv[ya_optind]);
        ya_optind++;
    }
    return '?';
found:
    ya_optarg = val;
    ya_optind++;
    if (opt->flag) {
        *opt->flag = opt->val;
    }
    if (longindex) {
        *longindex = idx;
    }
    return opt->flag ? 0 : opt->val;
}
