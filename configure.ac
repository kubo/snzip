# The autoconf version must be at least 2.64 to correctly detect
# endianess of Mac OS X universal binary.
AC_PREREQ(2.64)
AC_INIT([snzip], [1.0.1])

AM_INIT_AUTOMAKE

AC_ARG_WITH([default-format],
    [AS_HELP_STRING([--with-default-format=snzip|framing|framing2],
        [Set default format @<:@default=framing2@:>@])],
    [
        AS_CASE(["$with_default_format"],
            [snzip],          [],
            [framing],        [],
            [framing2],       [],
            [snappy-java],    [with_default_format=snappy_java],
            [snappy-in-java], [with_default_format=snappy_in_java],
            [AC_MSG_ERROR([Unknown --with-default-format argument: $with_default_format.
   It should be snzip, framing or framing2.
   ])])
    ],
    [with_default_format=framing2])
AC_DEFINE_UNQUOTED([DEFAULT_FORMAT], [${with_default_format}_format], [Default format for compression])

# These are flags passed to automake (though they look like gcc flags!)
AC_PROG_CC
AC_GNU_SOURCE
AC_C_BIGENDIAN
AC_CANONICAL_HOST

if test "$GCC"; then
   CFLAGS="$CFLAGS -Wall"
fi

AC_CHECK_HEADERS([unistd.h])

AC_SYS_LARGEFILE
AC_CHECK_FUNCS(posix_fadvise futimens futimes)
AC_CHECK_MEMBERS([struct stat.st_mtimensec, struct stat.st_mtim.tv_nsec, struct stat.st_mtimespec.tv_nsec], [], [], [[
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
]])

# unlocked stdio functions
AC_CHECK_FUNCS(getc_unlocked putc_unlocked fread_unlocked fwrite_unlocked ferror_unlocked feof_unlocked)

# introduce the optional configure parameter for a non-standard install prefix of snappy
AC_ARG_WITH([snappy],
    [AS_HELP_STRING([--with-snappy=prefix],
        [try this for a non-standard install prefix of the snappy library])],
    [
        CFLAGS="$CFLAGS -I$with_snappy/include"
        AS_CASE(["$host_os"],
            [linux*],   [LDFLAGS="$LDFLAGS -L$with_snappy/lib -Wl,-rpath,$with_snappy/lib"],
            [solaris*], [LDFLAGS="$LDFLAGS -L$with_snappy/lib -R$with_snappy/lib"],
                        [LDFLAGS="$LDFLAGS -L$with_snappy/lib"])
    ],
    [])

AC_CHECK_LIB([snappy], [snappy_compress], [],
[AC_MSG_ERROR([Snappy library is not found.])])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile spec/snzip.spec])
AC_OUTPUT
