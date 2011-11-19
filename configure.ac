# The autoconf version must be at least 2.64 to correctly detect
# endianess of Mac OS X universal binary.
AC_PREREQ(2.64)
AC_INIT([snzip], [0.0.2])

AM_INIT_AUTOMAKE

# These are flags passed to automake (though they look like gcc flags!)
AC_PROG_CC
AC_C_BIGENDIAN

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

# introduce the optional configure parameter for a non-standard install prefix of snappy
AC_ARG_WITH([snappy],
    [AS_HELP_STRING([--with-snappy=prefix],
        [try this for a non-standard install prefix of the snappy library])],
    [
        CFLAGS="$CFLAGS -I$with_snappy/include"
        LDFLAGS="$LDFLAGS -L$with_snappy/lib"
    ],
    [])

AC_CHECK_LIB([snappy], [snappy_compress], [],
[AC_MSG_ERROR([No snappy library is not found.])])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
