Snzip, a compression/decompression tool based on snappy.
========================================================

What is snzip.
--------------

Snzip is one of command line tools using [snappy][]. This supports several file
formats; [framing-format][], [old framing-format][], [hadoop-snappy format][], [raw format][]
and obsolete three formats used by snzip, [snappy-java][] and [snappy-in-java][]
before official framing-format was defined. The default format is [framing-format][].

Notable Changes
---------------

The default format was changed to [framing-format][] in 1.0.0.
Set `--with-default-format=snzip` as a configure option to use obsolete snzip
format as the default format as before.

Installation
------------

### Install from a tar-ball

Download snzip-1.0.5.tar.gz from https://github.com/kubo/snzip/releases,
uncompress and untar it, and run configure.

    tar xvfz snzip-1.0.5.tar.gz
    cd snzip-1.0.5
    ./configure
    make
    make install

If you didn't install snappy under `/usr` or `/usr/local`, you need to specify
the location by `--with-snappy` as follows.

    # install snzip
    tar xvfz snzip-1.0.5.tar.gz
    cd snzip-1.0.5
    ./configure --with-snappy=/xxx/yyy/
    make
    make install

When both dynamic and static snappy libraries are available, the former
is used by default. The compiled `snzip` depends on `libsnappy.so`.
When `--with-static-snappy` is passed as a configure option, the latter
is used. The compiled `snzip` includes snappy library.

Note: `--with-static-snappy` isn't available on some platforms.

You can use `--with-default-format` to change the default compression format.

    ./configure --with-default-format=snzip

### Install as a rpm package

We don't provide rpm packages. You need to download snzip-1.0.5.tar.gz
from https://github.com/kubo/snzip/releases, create a rpm package as follows and
install it.

    # The rpm package will be created under $HOME/rpmbuild/RPMS.
    rpmbuild -tb snzip-1.0.5.tar.gz 

### Install from the latest source

To use source code in the github repository.

    git clone git://github.com/kubo/snzip.git
    cd snzip
    ./autogen.sh
    ./configure
    make
    make install

### Install a Windows package.

Download `snzip-1.0.5-win32.zip` or `snzip-1.0.5-win64.zip` from
https://github.com/kubo/snzip/releases and copy `snzip.exe` and `snunzip.exe`
to a directory in the PATH environment variable.

Usage
-----

### To compress file.tar:

    snzip file.tar

Compressed file name is `file.tar.sz` and the original file is deleted.
The file attributes such as timestamp, mode and permissions are not changed
as possible as it can.

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
The file attributes such as timestamp, mode and permissions are not changed
as possible as it can.

If the program name includes `un` such as `snunzip`, it acts as `-d` is set.

The file format is automatically determined from the file header.
However it doesn't work for some file formats such as raw and Apple iWork .iwa.

### To uncompress file.tar.sz and output to standard out.

    snzip -dc file.tar.sz > file.tar
    snunzip -c file.tar.sz > file.tar
    snzcat file.tar.sz > file.tar
    cat file.tar.sz | snzcat > file.tar

If the program name includes `cat` such as snzcat, it acts as `-dc` is set.

### To uncompress a tar file and extract it.

    snzip -dc archive.tar.sz | tar xf -

Raw format
----------

Raw format is native format of snappy.
Unlike other formats, there are a few limitations:
(1) The total data length before compression must be known on compression.
(2) Automatic file format detection doesn't work on uncompression.
(3) The raw format support is enabled only when snzip is compiled for snappy 1.1.3 or upper.

### To compress file.tar as raw format:

    snzip -t raw file.tar

or

    snzip -t raw < file.tar > file.tar.raw

In these examples, snzip uses a file descriptor, which directly opens
the `file.tar` file, and gets the file length to be compressed.
However the following command doesn't work.

    cat file.tar | snzip -t raw > file.tar.raw

It uses a pipe. snzip cannot get the total length before compression.
The total length must be specified by the `-s` option in this case.

    cat file.tar | snzip -t raw -s "size of file.tar" > file.tar.raw

### To uncompress file.tar.sz compressed as raw format

    snzip -t raw -d file.tar.sz

or

    snunzip -t raw file.tar.sz

You need to set the `-t raw` option to tell snzip the format of the
file to be uncompressed.

Hadoop-snappy format
--------------------

Hadoop-snappy format is one of the compression formats used in Hadoop.
It uses its own framing format as follows:

* A compressed file consists of one or more blocks.
* A block consists of uncompressed length (big endian 4 byte integer) and one or more subblocks.
* A subblock consists of compressed length (big endian 4 byte integer) and raw compressed data.

### To compress a file as hadoop-snappy format

    snzip -t hadoop-snappy file_name

The default block size used by `snzip` for hadoop-snappy format is 256k.
It is same with the default value of the `io.compression.codec.snappy.buffersize`
parameter. If the block size used by `snzip` is larger than the parameter,
you would get an InternalError `Could not decompress data. Buffer length is too small`
while hadoop is reading a file compressed by snzip. You need to change the block
size by the `-b` option as follows if you get the error.

    # if  io.compression.codec.snappy.buffersize is 32768
    snzip -t hadoop-snappy -b 32768 file_name_to_be_compressed

### To uncompress a file compressed as haddoop-snappy format

    snzip -d compressed_file.snappy

The file format is guessed by the first 8 bytes of the file.

Apple iWork .iwa format
-----------------------

Apple iWork .iwa format is a file format used by Apple iWork. The format was
demystified [here](https://github.com/obriensp/iWorkFileFormat).
Basically the .iwa format consists of a Protobuf stream [compressed by Snappy](https://github.com/obriensp/iWorkFileFormat/blob/master/Docs/index.md#snappy-compression).

Snzip uncompresses .iwa files to Protbuf streams and compresses Protobuf streams
to .iwa files. You need to set `-t iwa` on compression and uncompression to
specify the file format.

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

[snappy]: https://github.com/google/snappy/blob/master/docs/README.md
[framing-format]: https://github.com/google/snappy/blob/master/framing_format.txt
[old framing-format]: https://github.com/google/snappy/blob/0755c815197dacc77d8971ae917c86d7aa96bf8e/framing_format.txt
[snappy-java]: https://github.com/xerial/snappy-java
[snappy-in-java]: https://github.com/dain/snappy
[raw format]: https://github.com/kubo/snzip#raw-format
[Hadoop-snappy format]: https://github.com/kubo/snzip#hadoop-snappy-format
