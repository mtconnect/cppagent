EXTRA_DIST = DllPlugInTester.vcxproj

AM_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include

bin_PROGRAMS = DllPlugInTester

TESTS = DllPlugInTesterTest
check_PROGRAMS = $(TESTS)

DllPlugInTester_SOURCES= DllPlugInTester.cpp \
	CommandLineParser.h \
	CommandLineParser.cpp

DllPlugInTester_LDADD= \
  $(top_builddir)/src/cppunit/libcppunit.la \
  $(LIBADD_DL)

DllPlugInTesterTest_SOURCES = DllPlugInTesterTest.cpp \
	CommandLineParser.cpp \
	CommandLineParser.h \
	CommandLineParserTest.cpp \
	CommandLineParserTest.h

DllPlugInTesterTest_LDADD= \
  $(top_builddir)/src/cppunit/libcppunit.la \
  $(LIBADD_DL)
