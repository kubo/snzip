Snzip, a compression/decompression tool based on snappy.
========================================================

What is snzip.
--------------

Snzip is one of command line tools using [snappy][].
This supports four type of file formats; snzip format,
[snappy-framed][] format [snappy-java][] format and [snappy-in-java][]
format. The default format is snzip.

Note that the default format is changed to [snappy-framed][] when
the program name includes "snappy." If you want to use snzip
as a [snappy-framed][] compressor/decompressor, rename snzip
to snappy, unsnappy or so.

Installation
------------

Download snzip-0.9.0.tar.gz from https://github.com/kubo/snzip,
uncompress and untar it, and run configure.

    tar xvfz snzip-0.9.0.tar.gz
    cd snzip-0.9.0
    ./configure

If you didn't install snappy under /usr or /usr/local, you need to specify
the location by '--with-snappy' as follows.

    # install snappy
    tar xvfz snappy-1.0.4.tar.gz
    cd snappy-1.0.4
    ./configure --prefix=/usr/local/snappy
    make
    make install
    cd ..
    
    # install snzip
    tar xvfz snzip-0.9.0.tar.gz
    cd snzip-0.9.0
    ./configure --with-snappy=/usr/local/snappy

To use source code in the github repository.

    git clone git://github.com/kubo/snzip.git
    cd snzip
    ./autogen.sh
    ./configure

Usage
-----

### To compress file.tar:

    snzip file.tar

Compressed file name is 'file.tar.snz' and the original file is deleted.
Timestamp, mode and permissions are not changed as possible as it can.

The file format is SNZ. You need to add an option '-t snappy-java' or
'-t snappy-in-java' to use other formats.

    snzip -t snappy-java file.tar

or

    snzip -t snappy-in-java file.tar

### To compress file.tar and output to standard out.

    snzip -c file.tar > file.tar.snz

or

    cat file.tar | snzip > file.tar.snz

You need to add an option '-t [format-name]' to use formats except SNZ.


### To uncompress file.tar.snz:

    snzip -d file.tar.snz

or

    snunzip file.tar.snz

Uncompressed file name is 'file.tar' and the original file is deleted.
Timestamp, mode and permissions are not changed as possible as it can.

If the program name includes 'un' such as snunzip, it acts as '-d' is set.

The file format is automatically determined from the first byte of the file.

### To uncompress file.tar.snz and output to standard out.

    snzip -dc file.tar.snz > file.tar
    snunzip -c file.tar.snz > file.tar
    snzcat file.tar.snz > file.tar
    cat file.tar.snz | snzcat > file.tar

If the program name includes 'cat' such as snzcat, it acts as '-dc' is set.

SNZ File format
---------------

The first three bytes are magic characters 'SNZ'.

The fourth byte is the file format version. It is 0x01.

The fifth byte is the order of the block size. The input data
is divided into fixed-length blocks and each block is compressed
by snappy. When it is 16 (default value), the block size is 16th
power of 2; 64 kilobytes.

The rest is pairs of a compressed data length and a compressed data block
The compressed data length is encoded as [snappy::Varint::Encode32()][] does.
If the length is zero, it is the end of data.

Though the rest after the end of data is ignored for now, they
may be continuously read as a next compressed file as gzip does.

Note that the uncompressed length of each compressed data block must be
less than or equal to the block size specified by the fifth byte.

License
-------

2-clause BSD-style license.

[snappy]: http://code.google.com/p/snappy/
[snappy-framed]: http://code.google.com/p/snappy/source/browse/trunk/framing_format.txt?r=55
[Issue 34: Command line tool]: http://code.google.com/p/snappy/issues/detail?id=34
[snappy::Varint::Encode32()]: http://code.google.com/p/snappy/source/browse/trunk/snappy-stubs-internal.h?r=51#461
[snappy-java]: http://code.google.com/p/snappy-java/
[snappy-in-java]: https://github.com/dain/snappy
