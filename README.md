Snzip, a compression/decompression tool based on snappy.
========================================================

What is snzip.
--------------

Snzip is one of command line tools using [snappy][]. This supports five types of
file formats; [framing-format][], [old framing-format][] and obsolete three formats
used by snzip, [snappy-java][] and [snappy-in-java][] before official framing-format
was defined. The default format is [framing-format][].

Notable Changes
---------------

The default format was changed to [framing-format][] in 1.0.0.
Set `--with-default-format=snzip` as a configure option to use obsolete snzip
format as the default format as before.

Installation
------------

### Install from a tar-ball

Download snzip-1.0.2.tar.gz from https://bintray.com/kubo/generic/snzip,
uncompress and untar it, and run configure.

    tar xvfz snzip-1.0.2.tar.gz
    cd snzip-1.0.2
    ./configure

If you didn't install snappy under `/usr` or `/usr/local`, you need to specify
the location by `--with-snappy` as follows.

    # install snappy
    tar xvfz snappy-1.1.3.tar.gz
    cd snappy-1.1.3
    ./configure --prefix=/usr/local/snappy
    make
    make install
    cd ..
    
    # install snzip
    tar xvfz snzip-1.0.2.tar.gz
    cd snzip-1.0.2
    ./configure --with-snappy=/usr/local/snappy

You can use `--with-default-format` to change the default compression format.

    ./configure --with-default-format=snzip

### Install as a rpm package

We don't provide rpm packages. You need to download snzip-1.0.2.tar.gz
from https://bintray.com/kubo/generic/snzip, create a rpm package as follows and
install it.

    # The rpm package will be created under $HOME/rpmbuild/RPMS.
    rpmbuild -tb snzip-1.0.2.tar.gz 

### Install from the latest source

To use source code in the github repository.

    git clone git://github.com/kubo/snzip.git
    cd snzip
    ./autogen.sh
    ./configure

### Install a Windows package.

Download `snzip-1.0.2-win32.zip` or `snzip-1.0.2-win64.zip` from
https://bintray.com/kubo/generic/snzip and copy `snzip.exe` and `snunzip.exe`
to a directory in the PATH environment variable.

Usage
-----

### To compress file.tar:

    snzip file.tar

Compressed file name is `file.tar.sz` and the original file is deleted.
Timestamp, mode and permissions are not changed as possible as it can.

The compressed file's format is [framing-format][]. You need to add an option `-t snappy-java` or
`-t snappy-in-java` to use other formats.

    snzip -t snappy-java file.tar

or

    snzip -t snappy-in-java file.tar

### To compress file.tar and output to standard out.

    snzip -c file.tar > file.tar.sz

or

    cat file.tar | snzip > file.tar.sz

You need to add an option `-t [format-name]` to use formats except [framing-format][].

### To create a new tar file and compress it.

    tar cf - files-to-be-archived | snzip > archive.tar.sz

### To uncompress file.tar.sz:

    snzip -d file.tar.sz

or

    snunzip file.tar.sz

Uncompressed file name is `file.tar` and the original file is deleted.
Timestamp, mode and permissions are not changed as possible as it can.

If the program name includes `un` such as `snunzip`, it acts as `-d` is set.

The file format is automatically determined from the file header.

### To uncompress file.tar.sz and output to standard out.

    snzip -dc file.tar.sz > file.tar
    snunzip -c file.tar.sz > file.tar
    snzcat file.tar.sz > file.tar
    cat file.tar.sz | snzcat > file.tar

If the program name includes `cat` such as snzcat, it acts as `-dc` is set.

### To uncompress a tar file and extract it.

    snzip -dc archive.tar.sz | tar xf -

SNZ File format
---------------

Note: This is obsolete format. The default format was changed to [framing-format].

The first three bytes are magic characters 'SNZ'.

The fourth byte is the file format version. It is 0x01.

The fifth byte is the order of the block size. The input data
is divided into fixed-length blocks and each block is compressed
by snappy. When it is 16 (default value), the block size is 16th
power of 2; 64 kilobytes.

The rest is pairs of a compressed data length and a compressed data block
The compressed data length is encoded as `snappy::Varint::Encode32()` does.
If the length is zero, it is the end of data.

Though the rest after the end of data is ignored for now, they
may be continuously read as a next compressed file as gzip does.

Note that the uncompressed length of each compressed data block must be
less than or equal to the block size specified by the fifth byte.

License
-------

2-clause BSD-style license.

[snappy]: http://google.github.io/snappy/
[framing-format]: https://github.com/google/snappy/blob/master/framing_format.txt
[old framing-format]: https://github.com/google/snappy/blob/0755c815197dacc77d8971ae917c86d7aa96bf8e/framing_format.txt
[snappy-java]: https://github.com/xerial/snappy-java
[snappy-in-java]: https://github.com/dain/snappy
