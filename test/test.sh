#! /bin/sh

set -e

TESTDIR=`dirname $0`
SNZIP="$TESTDIR/../snzip"

run_test() {
    format=$1
    ext=$2
    opt=$3
    shift 3

    echo Checking $format format
    while test $# -ge 1
    do
        testfile=$1
        shift

        echo compress and decompress $testfile on disk
        # compress a file on disk
        $SNZIP $opt -t $format -c $TESTDIR/plain/$testfile > $TESTDIR/$testfile.tmp.$ext
        # decompress a file on disk
        $SNZIP $opt -t $format -d $TESTDIR/$testfile.tmp.$ext
        # check result
        cmp $TESTDIR/$testfile.tmp $TESTDIR/plain/$testfile
        rm $TESTDIR/$testfile.tmp

        echo compress and decompress $testfile from stdin
        # compress stdin
        cat $TESTDIR/plain/$testfile | $SNZIP $opt -t $format -s $(stat -c %s $TESTDIR/plain/$testfile) > $TESTDIR/$testfile.tmp.$ext
        # decompress
        cat $TESTDIR/$testfile.tmp.$ext | $SNZIP $opt -t $format -d > $TESTDIR/$testfile.tmp
        # check result
        cmp $TESTDIR/$testfile.tmp $TESTDIR/plain/$testfile
        rm $TESTDIR/$testfile.tmp
        if test $format = framing -o $format = framing2; then
            echo uncompress concatincated compressed $testfile
            cat $TESTDIR/$testfile.tmp.$ext $TESTDIR/$testfile.tmp.$ext | $SNZIP $opt -t $format -d > $TESTDIR/$testfile.tmp
            cat $TESTDIR/plain/$testfile $TESTDIR/plain/$testfile | cmp $TESTDIR/$testfile.tmp -
            rm $TESTDIR/$testfile.tmp
        fi
        rm $TESTDIR/$testfile.tmp.$ext

        for f in $TESTDIR/$format/$testfile.$ext*; do
            echo uncompress compressed $f without autodetect
            cat $f | $SNZIP $opt -t $format -d > $TESTDIR/$testfile.tmp
            cmp $TESTDIR/$testfile.tmp $TESTDIR/plain/$testfile
            rm $TESTDIR/$testfile.tmp
            if test $format != raw -a $format != iwa; then
                echo uncompress compressed $f with autodetect
                cat $f | $SNZIP $opt -d > $TESTDIR/$testfile.tmp
                cmp $TESTDIR/$testfile.tmp $TESTDIR/plain/$testfile
                rm $TESTDIR/$testfile.tmp
            fi
        done
    done
    echo ""
}

run_test comment-43     snappy  "" alice29.txt house.jpg
run_test framing        sz      "" alice29.txt house.jpg
run_test framing2       sz      "" alice29.txt house.jpg
run_test hadoop-snappy  snappy  "-b 65536" alice29.txt house.jpg
run_test iwa            iwa     "" alice29.txt house.jpg
run_test snappy-in-java snappy  "" alice29.txt house.jpg
run_test snappy-java    snappy  "" alice29.txt house.jpg
run_test snzip          snz     "" alice29.txt house.jpg
if $SNZIP -h 2>&1 | grep ' raw ' > /dev/null; then
  run_test raw          raw     "" alice29.txt house.jpg
else
  echo 'Skip raw format tests'
fi

echo uncompress a file with unknown suffix
cp $TESTDIR/plain/alice29.txt $TESTDIR/alice29.txt
$SNZIP -t framing2 $TESTDIR/alice29.txt
mv $TESTDIR/alice29.txt.sz $TESTDIR/alice29.txt.snappy
$SNZIP -d -t framing2 $TESTDIR/alice29.txt.snappy
cmp $TESTDIR/alice29.txt.snappy.out $TESTDIR/plain/alice29.txt
rm $TESTDIR/alice29.txt.snappy.out
echo ""

echo Success
