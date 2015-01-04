# Command line tool.
snzip_SOURCES = snzip.c snzip.h snzip-format.c framing-format.c framing2-format.c snappy-java-format.c snappy-in-java-format.c comment-43-format.c crc32.c crc32.h
if HAVE_SSE4_2
snzip_SOURCES += crc32_sse4_2.c
endif
snzip_LDFLAGS = @LDFLAGS_SSE4_2@
CFLAGS_SSE4_2 = @CFLAGS_SSE4_2@
PROGS = snzip
bin_PROGRAMS = $(PROGS)

EXTRA_DIST = autogen.sh spec/snzip.spec
dist_doc_DATA = AUTHORS ChangeLog COPYING INSTALL NEWS README.md

dist-hook:
	git log > $(distdir)/ChangeLog

# Use CFLAGS_SSE4_2 only for crc32_sse4_2.c.
# Otherwise, SSE4.2 instructions may be used elsewhere.
crc32_sse4_2.o: crc32_sse4_2.c crc32.h
	$(COMPILE) $(CFLAGS_SSE4_2) -c crc32_sse4_2.c
