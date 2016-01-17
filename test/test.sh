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
        status=
        for f in $TESTDIR/$format/$testfile.$ext*; do
            if cmp $TESTDIR/$testfile.tmp.$ext $f >/dev/null 2>&1; then
                status=OK
            fi
        done
        if ! test "$status"; then
            echo "$TESTDIR/$testfile.tmp.$ext doesn't match: `echo $TESTDIR/$format/$testfile.$ext*`"
            exit 1
        fi

        cat $TESTDIR/plain/$testfile | $SNZIP -t $format -s $(stat -c %s $TESTDIR/plain/$testfile) > $TESTDIR/$testfile.tmp.$ext
        status=
        for f in $TESTDIR/$format/$testfile.$ext*; do
            if cmp $TESTDIR/$testfile.tmp.$ext $f >/dev/null 2>&1; then
                status=OK
            fi
        done
        if ! test "$status"; then
            echo "$TESTDIR/$testfile.tmp.$ext doesn't match: `echo $TESTDIR/$format/$testfile.$ext*`"
            exit 1
        fi

        echo uncompress $testfile.tmp.$ext without autodetect
        cat $TESTDIR/$testfile.tmp.$ext | $SNZIP -t $format -d > $TESTDIR/$testfile.tmp
        cmp $TESTDIR/$testfile.tmp $TESTDIR/plain/$testfile

        if test $format != raw; then
            echo uncompress $testfile.tmp.$ext with autodetect
            $SNZIP -d $TESTDIR/$testfile.tmp.$ext
            cmp $TESTDIR/$testfile.tmp $TESTDIR/plain/$testfile
        fi

        rm $TESTDIR/$testfile.tmp
    done
    echo ""
}

run_test comment-43     snappy  alice29.txt house.jpg
run_test framing        sz      alice29.txt house.jpg
run_test framing2       sz      alice29.txt house.jpg
run_test snappy-in-java snappy  alice29.txt house.jpg
run_test snappy-java    snappy  alice29.txt house.jpg
run_test snzip          snz     alice29.txt house.jpg
if $SNZIP -h 2>&1 | grep ' raw ' > /dev/null; then
  run_test raw            raw     alice29.txt house.jpg
else
  echo 'Skip raw format tests'
fi
echo Success
