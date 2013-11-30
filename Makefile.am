# Command line tool.
snzip_SOURCES = snzip.c snzip.h snzip-format.c framing-format.c framing2-format.c snappy-java-format.c snappy-in-java-format.c comment-43-format.c crc32.c crc32.h
PROGS = snzip
bin_PROGRAMS = $(PROGS)

EXTRA_DIST = autogen.sh spec/snzip.spec
dist_doc_DATA = AUTHORS ChangeLog COPYING INSTALL NEWS README.md

dist-hook:
	git log > $(distdir)/ChangeLog
