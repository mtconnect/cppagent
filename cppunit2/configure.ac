# Process this file with autoconf to create configure.

AC_PREREQ([2.65])

# ====================
# Version informations
# ====================
m4_define([cppunit_version_major],[1])
m4_define([cppunit_version_minor],[14])
m4_define([cppunit_version_micro],[0])
m4_define([cppunit_version],[cppunit_version_major.cppunit_version_minor.cppunit_version_micro])
m4_define([cppunit_interface_age], [0])
m4_define([cppunit_binary_age], [0])

# =============
# Automake init
# =============
AC_INIT([cppunit],[cppunit_version])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.11 foreign dist-xz dist-bzip2])
AM_SILENT_RULES([yes])
AC_LANG([C++])

# ===========================
# Find required base packages
# ===========================
AC_PROG_CPP
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL
AC_LIBTOOL_WIN32_DLL

# ======================================
# Check for various headers and settings
# ======================================
AC_HEADER_STDC
AC_LTDL_DLLIB
AC_CHECK_HEADERS(cmath,[],[],[/**/])
AX_CXX_HAVE_ISFINITE
AC_CHECK_FUNCS(finite)
AX_CXX_HAVE_SSTREAM
AC_CXX_HAVE_STRSTREAM

# Enable doxygen
BB_ENABLE_DOXYGEN

# Compiler characteristics
AX_CXX_GCC_ABI_DEMANGLE
AC_CXX_STRING_COMPARE_STRING_FIRST
AX_CXX_COMPILE_STDCXX_11(noext, mandatory)

# =================================
# Libtool/Version Makefile settings
# =================================
AC_SUBST(CPPUNIT_MAJOR_VERSION, [cppunit_version_major])
AC_SUBST(CPPUNIT_MINOR_VERSION, [cppunit_version_minor])
AC_SUBST(CPPUNIT_MICRO_VERSION, [cppunit_version_micro])
AC_SUBST(CPPUNIT_BINARY_AGE, [cppunit_binary_age])
AC_SUBST(CPPUNIT_INTERFACE_AGE, [cppunit_interface_age])
LT_RELEASE=$CPPUNIT_MAJOR_VERSION.$CPPUNIT_MINOR_VERSION
LT_CURRENT=`expr $CPPUNIT_MICRO_VERSION - $CPPUNIT_INTERFACE_AGE`
LT_AGE=`expr $CPPUNIT_BINARY_AGE - $CPPUNIT_INTERFACE_AGE`

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION, [cppunit_interface_age])
AC_SUBST(LT_AGE)


# ================
# Check for cflags
# ================

# =====
# Debug
# =====
AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--enable-debug], [Build with debug symbols.]),
	[enable_debug="$enableval"],
	[enable_debug=no]
)
AS_IF([test "x$enable_debug" != "xno"], [
	CFLAGS="$CFLAGS -g"
	CXXFLAGS="$CXXFLAGS -g"
], [
	AC_DEFINE([NDEBUG], [], [Disable debugging information])
])

AC_ARG_ENABLE([werror],
	[AS_HELP_STRING([--disable-werror], [Treat all warnings as errors, useful for development])],
	[enable_werror="$enableval"],
	[enable_werror=yes]
)
AS_IF([test x"$enable_werror" != "xno"], [
	CFLAGS="$CFLAGS -Werror"
	CXXFLAGS="$CXXFLAGS -Werror"
])
AS_IF([test x"$GCC" = xyes], [
	# Be tough with warnings and produce less careless code
	# Weffc++ currently prints too much warnings
	CFLAGS="$CFLAGS -Wall -Wextra -pedantic"
	CXXFLAGS="$CXXFLAGS -Wall -Wextra -pedantic" # -Weffc++"
])
CPPUNIT_CXXFLAGS="$CPPUNIT_CFLAGS"
AC_SUBST(CPPUNIT_CXXFLAGS)

# ==============
# Setup config.h
# ==============
AC_CONFIG_HEADERS([config.h])
AX_PREFIX_CONFIG_H([include/cppunit/config-auto.h])

# =====================
# Prepare all .in files
# =====================
AC_CONFIG_FILES([
	Makefile
	cppunit.pc
	cppunit.spec
	src/Makefile
	src/DllPlugInTester/Makefile
	src/cppunit/Makefile
	include/Makefile
	include/cppunit/Makefile
	include/cppunit/config/Makefile
	include/cppunit/extensions/Makefile
	include/cppunit/plugin/Makefile
	include/cppunit/portability/Makefile
	include/cppunit/tools/Makefile
	include/cppunit/ui/Makefile
	include/cppunit/ui/text/Makefile
	doc/Makefile
	doc/Doxyfile
	examples/Makefile
	examples/simple/Makefile
	examples/hierarchy/Makefile
	examples/cppunittest/Makefile
	examples/ClockerPlugIn/Makefile
	examples/DumperPlugIn/Makefile
	examples/money/Makefile
])
AC_OUTPUT

# ==============================================
# Display final informations about configuration
# ==============================================
AC_MSG_NOTICE([
==============================================================================
Build configuration:
	debug:              ${enable_debug}
	docs:               ${enable_doc}
	werror:             ${enable_werror}
==============================================================================
])
