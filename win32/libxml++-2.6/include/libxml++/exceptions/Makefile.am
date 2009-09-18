INCLUDES = -I$(top_srcdir) @LIBXML_CFLAGS@
DEFS = @DEFS@ -DLIBXMLPP_BUILD

h_sources_public = exception.h parse_error.h validity_error.h internal_error.h
cc_sources = exception.cc parse_error.cc validity_error.cc internal_error.cc

noinst_LTLIBRARIES = libexceptions.la
libexceptions_la_SOURCES = $(h_sources_public) $(cc_sources)

# Install the headers:
library_includedir=$(includedir)/libxml++-2.6/libxml++/exceptions
library_include_HEADERS = $(h_sources_public)
