#! /bin/sh -e
rm -rf autom4te.cache
aclocal
autoheader
automake --add-missing --copy
autoconf
