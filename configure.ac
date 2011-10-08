AC_PREREQ(2.50)
AC_INIT([snzip], [0.0.1])

AM_INIT_AUTOMAKE

# These are flags passed to automake (though they look like gcc flags!)
AC_PROG_CC

if test "$GCC"; then
   CFLAGS="$CFLAGS -Wall -Werror"
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
