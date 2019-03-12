ACLOCAL_AMFLAGS = -I m4

.PHONY: release rpm docs doc-dist distclean-local dist-hook

SUBDIRS = src include examples doc

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cppunit.pc

doc_DATA = \
	BUGS \
	INSTALL \
	INSTALL-unix \
	INSTALL-WIN32.txt \
	CodingGuideLines.txt \
	AUTHORS \
	doc/FAQ
docinternal = \
	doc/cookbook.dox \
	doc/other_documentation.dox \
	doc/header.html \
	doc/footer.html \
	doc/Money.dox \
	doc/CppUnit-win.dox \
	doc/makedox.bat

m4dir = $(datadir)/aclocal
m4_DATA =

m4internal = \
	m4/ac_cxx_have_strstream.m4 \
	m4/ac_cxx_string_compare_string_first.m4 \
	m4/ac_dll.m4 \
	m4/ax_cxx_gcc_abi_demangle.m4 \
	m4/ax_cxx_have_isfinite.m4 \
	m4/ax_cxx_have_sstream.m4 \
	m4/ax_cxx_namespaces.m4 \
	m4/ax_cxx_compile_stdcxx_11.m4 \
	m4/ax_prefix_config_h.m4 \
	m4/bb_enable_doxygen.m4 \
	m4/README

unusedfolders =

EXTRA_DIST = \
	autogen.sh \
	contrib \
	examples \
	lib/.keepme \
	$(pkgconfig_DATA) \
	$(man_MANS) \
	$(doc_DATA) \
	$(m4_DATA) \
	$(m4internal) \
	$(docinternal) \
	$(unusedfolders)


dist-hook:
	git log --date=short --pretty="format:@%cd  %an  <%ae>  [%H]%n%n%s%n%n%e%b" | sed -e "s|^\([^@]\)|\t\1|" -e "s|^@||" >$(distdir)/ChangeLog
	perl -pi -e 's/\n/\r\n/g' `find $(distdir) -name '*.ds?'` \
		$(distdir)/contrib/msvc/*			  \
		$(distdir)/INSTALL-WIN32.txt

release:
	$(MAKE) distcheck

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.xz
	@rm -f $(PACKAGE)-$(VERSION).tar.xz

doc-dist:
	$(MAKE) -C doc doc-dist
	mv -f doc/$(PACKAGE)-docs-$(VERSION).tar.xz .

distclean-local:
	rm -rf *.cache *~ $(PACKAGE)*.tar.*

DISTCLEANFILES = _configs.sed
