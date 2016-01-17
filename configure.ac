# The autoconf version must be at least 2.64 to correctly detect
# endianess of Mac OS X universal binary.
AC_PREREQ(2.64)
AC_INIT([snzip], [1.0.2])

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
AC_PROG_CXX
AC_GNU_SOURCE
AC_C_BIGENDIAN
AC_CANONICAL_HOST

if test "$GCC"; then
   CFLAGS="$CFLAGS -Wall"
   CXXFLAGS="$CXXFLAGS -Wall"
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

AC_ARG_ENABLE([sse4_2],
    [AS_HELP_STRING([--disable-sse4_2],
        [don't use sse4.2 to calculate crc32])],
    [],
    [])

CFLAGS_SSE4_2=
LDFLAGS_SSE4_2=
AS_IF([test "x$enable_sse4_2" != xno],
    [
        AC_MSG_CHECKING(SSE4.2 CFLAGS)
        AS_IF([test "x$GCC" = xyes],
              [CFLAGS_SSE4_2=-msse4.2],
              [AS_CASE(["$host_os"],
                  [solaris*], [CFLAGS_SSE4_2=-xarch=sse4_2;
                               LDFLAGS_SSE4_2=-Wl,-M,clearcap.map
                              ])
              ])
        AS_IF([test "$CFLAGS_SSE4_2"],
              [AC_MSG_RESULT($CFLAGS_SSE4_2)],
              [AC_MSG_RESULT()])
    ])

AS_IF([test "$CFLAGS_SSE4_2"],
    [
        AC_MSG_CHECKING([whether $CFLAGS_SSE4_2 is accepted as CFLAGS])
        saved_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $CFLAGS_SSE4_2"
        AC_TRY_COMPILE([], [],
            [AC_MSG_RESULT(yes)],
            [AC_MSG_RESULT(no); CFLAGS_SSE4_2=; LDFLAGS_SSE4_2=])

        AC_CHECK_DECLS([_mm_crc32_u32], [], [], [#include <nmmintrin.h>])
        CFLAGS="$saved_CFLAGS"
    ])
AC_SUBST([CFLAGS_SSE4_2])
AC_SUBST([LDFLAGS_SSE4_2])
AS_IF([test "x$ac_cv_have_decl__mm_crc32_u32" = xyes],
    [AC_DEFINE([HAVE_SSE4_2], 1, [Define to 1 if you have SSE4.2])])
AM_CONDITIONAL([HAVE_SSE4_2], [test "x$ac_cv_have_decl__mm_crc32_u32" = xyes])

# introduce the optional configure parameter for a non-standard install prefix of snappy
AC_ARG_WITH([snappy],
    [AS_HELP_STRING([--with-snappy=prefix],
        [try this for a non-standard install prefix of the snappy library])],
    [
        CFLAGS="$CFLAGS -I$with_snappy/include"
        CXXFLAGS="$CXXFLAGS -I$with_snappy/include"
        AS_CASE(["$host_os"],
            [linux*],   [LDFLAGS="$LDFLAGS -L$with_snappy/lib -Wl,-rpath,$with_snappy/lib"],
            [solaris*], [LDFLAGS="$LDFLAGS -L$with_snappy/lib -R$with_snappy/lib"],
                        [LDFLAGS="$LDFLAGS -L$with_snappy/lib"])
    ],
    [])

AC_CHECK_LIB([snappy], [snappy_compress], [],
[AC_MSG_ERROR([Snappy library is not found.])])

AC_LANG_PUSH(C++)
AC_MSG_CHECKING([snappy::Uncompress(snappy::Source*, snappy::Sink*)])
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include <snappy.h>],
    [snappy::Source *src=NULL; snappy::Sink *sink=NULL; snappy::Uncompress(src, sink);])],
  [have_snappy_uncompress_src_sink=yes],
  [have_snappy_uncompress_src_sink=no])
AC_MSG_RESULT($have_snappy_uncompress_src_sink)
AC_LANG_POP()

AS_IF([test "x$have_snappy_uncompress_src_sink" = xyes],
    [AC_DEFINE([SUPPORT_RAW_FORMAT], 1, [Define to 1 if snappy::Uncompress(snappy::Source*, snappy::Sink*) is available])],
    [AC_MSG_WARN([raw format is not supported with this snappy version.])])
AM_CONDITIONAL([SUPPORT_RAW_FORMAT], [test "x$have_snappy_uncompress_src_sink" = xyes])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile spec/snzip.spec win32/config.h])
AC_OUTPUT
