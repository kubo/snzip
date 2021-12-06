#! /bin/sh
set -e
./autogen.sh
mkdir work
cd work
../configure
make dist
mv snzip-*.tar.gz ..
cd ..
rm -rf work

