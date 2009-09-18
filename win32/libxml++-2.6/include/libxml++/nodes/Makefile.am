INCLUDES = -I$(top_srcdir) @LIBXML_CFLAGS@
DEFS = @DEFS@ -DLIBXMLPP_BUILD

h_sources_public = \
	cdatanode.h \
	commentnode.h \
	contentnode.h \
	element.h \
	entityreference.h \
	node.h \
	processinginstructionnode.h \
	textnode.h

cc_sources = \
	cdatanode.cc \
	commentnode.cc \
	contentnode.cc \
	entityreference.cc \
	element.cc \
	node.cc \
	processinginstructionnode.cc \
	textnode.cc


noinst_LTLIBRARIES = libnodes.la
libnodes_la_SOURCES = $(h_sources_public) $(cc_sources)

# Install the headers:
library_includedir=$(includedir)/libxml++-2.6/libxml++/nodes
library_include_HEADERS = $(h_sources_public)
