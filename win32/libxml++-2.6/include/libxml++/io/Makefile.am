INCLUDES = -I$(top_srcdir) @LIBXML_CFLAGS@
DEFS = @DEFS@ -DLIBXMLPP_BUILD

h_sources_public = \
  istreamparserinputbuffer.h \
  outputbuffer.h \
  ostreamoutputbuffer.h \
  parserinputbuffer.h
h_sources_private = 
cc_sources = \
  istreamparserinputbuffer.cc \
  outputbuffer.cc \
  ostreamoutputbuffer.cc \
  parserinputbuffer.cc
cc_sources_private =

noinst_LTLIBRARIES = libio.la
libio_la_SOURCES = $(cc_sources) $(h_sources_public) $(h_sources_private) $(cc_sources_private)

# Install the headers:
library_includedir=$(includedir)/libxml++-2.6/libxml++/io
library_include_HEADERS = $(h_sources_public)

