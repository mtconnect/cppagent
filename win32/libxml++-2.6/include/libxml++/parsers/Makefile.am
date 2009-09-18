INCLUDES = -I$(top_srcdir) @LIBXML_CFLAGS@
DEFS = @DEFS@ -DLIBXMLPP_BUILD

h_sources_public = parser.h saxparser.h domparser.h textreader.h
cc_sources = parser.cc saxparser.cc domparser.cc textreader.cc


noinst_LTLIBRARIES = libparsers.la
libparsers_la_SOURCES = $(h_sources_public) $(cc_sources)

# Install the headers:
library_includedir=$(includedir)/libxml++-2.6/libxml++/parsers
library_include_HEADERS = $(h_sources_public)
