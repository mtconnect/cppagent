AC_DEFUN([BB_ENABLE_DOXYGEN], [
AC_ARG_ENABLE([doxygen],
	[AS_HELP_STRING([--disable-doxygen], [Disable documentation generation with doxygen])],
	[enable_doxygen="$enableval"],
	[enable_doxygen=yes]
)
AC_ARG_ENABLE([dot],
	[AS_HELP_STRING([--disable-dot], [Disable graph generating using 'dot'])],
	[enable_dot="$enableval"],
	[enable_dot=yes]
)
AC_ARG_ENABLE([html-docs],
	[AS_HELP_STRING([--disable-html-docs], [Disable HTML generation with doxygen])],
	[enable_html_docs="$enableval"],
	[enable_html_docs=yes]
)
AC_ARG_ENABLE([latex-docs],
	[AS_HELP_STRING([--enable-latex-docs], [Enable LaTeX generation with doxygen])],
	[enable_latex_docs="$enableval"],
	[enable_latex_docs=no]
)
AS_IF([test "x$enable_doxygen" = "xno"], [
	enable_doc=no
], [
	AC_PATH_PROG(DOXYGEN, [doxygen])
	AS_IF([test -z "$DOXYGEN"], [
		AC_MSG_WARN([*** Could not find doxygen in your PATH.])
		AC_MSG_WARN([*** The documentation will not be built.])
		enable_doc=no
	], [
		enable_doc=yes
		AC_PATH_PROG(DOT, [dot])
	])
])
AM_CONDITIONAL(DOC, [test "x$enable_doc" = "xyes"])

AS_IF([test -z "$DOT"], [
	AS_IF([test "xenable_dot" = "xyes"], [
		AC_MSG_WARN([*** Could not find dot in your PATH.])
		AC_MSG_WARN([*** The documentation graphs will not be generated.])
		enable_dot=no
	])
])

AC_SUBST(enable_dot)
AC_SUBST(enable_html_docs)
AC_SUBST(enable_latex_docs)
])
