#! /bin/sh

set -e

TESTDIR=`dirname $0`
SNZIP="$TESTDIR/../snzip"

run_test() {
    format=$1
    ext=$2
    shift 2

    echo Checking $format format
    while test $# -ge 1
    do
        testfile=$1
        shift

        echo compress $testfile
        $SNZIP -t $format -c $TESTDIR/plain/$testfile > $TESTDIR/$testfile.tmp.$ext
        cmp $TESTDIR/$testfile.tmp.$ext $TESTDIR/$format/$testfile.$ext

        echo uncompress $testfile.tmp.$ext without autodetect
        cat $TESTDIR/$testfile.tmp.$ext | $SNZIP -t $format -d > $TESTDIR/$testfile.tmp
        cmp $TESTDIR/$testfile.tmp $TESTDIR/plain/$testfile

        echo uncompress $testfile.tmp.$ext with autodetect
        $SNZIP -d $TESTDIR/$testfile.tmp.$ext
        cmp $TESTDIR/$testfile.tmp $TESTDIR/plain/$testfile

        rm $TESTDIR/$testfile.tmp
    done
    echo ""
}

run_test comment-43     snappy  alice29.txt house.jpg
run_test snappy-framed  sz      alice29.txt house.jpg
run_test snappy-in-java snappy  alice29.txt house.jpg
run_test snappy-java    snappy  alice29.txt house.jpg
run_test snzip          snz     alice29.txt house.jpg
echo Success
