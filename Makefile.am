## Process this file with automake to produce Makefile.in

# Dbus service file
servicedir = ${datadir}/dbus-1/services
service_in_files = net.jarre_de_the.Xvidcap.service.in
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with bindir expanded
$(service_DATA): $(service_in_files) Makefile
	@SED@ -e "s|\@bindir\@|$(bindir)|" $<> $@

SUBDIRS = doc \
    src \
    po

pkgdata_DATA = xvidcap.desktop \
	xvidcap.png \
    ppm2mpeg.sh

EXTRA_DIST = autogen.sh \
    xvidcap.png \
    xvidcap.desktop \
    ppm2mpeg.sh \
    INSTALL \
    net.jarre_de_the.Xvidcap.service.in


# Copy all the spec files. Of cource, only one is actually used.
dist-hook:
	for specfile in *.spec; do \
		if test -f $$specfile; then \
			cp -p $$specfile $(distdir); \
		fi \
	done; \
    rm -rf `find $(distdir) -name CVS` ; \
    rm -rf `find $(distdir) -name .svn` ;

install-exec-hook:
	mkdir $(DESTDIR)$(datadir)/pixmaps ; \
	cp $(top_srcdir)/xvidcap.png $(DESTDIR)$(datadir)/pixmaps/ ; \
	mkdir $(DESTDIR)$(datadir)/applications ; \
	cp $(top_srcdir)/xvidcap.desktop $(DESTDIR)$(datadir)/applications/

install-data-hook:
	chmod 755 $(DESTDIR)$(datadir)/xvidcap/ppm2mpeg.sh

uninstall-hook:
	rm $(DESTDIR)$(datadir)/pixmaps/xvidcap.png ; \
	rm $(DESTDIR)$(datadir)/applications/xvidcap.desktop ; \
	rm -r $(DESTDIR)$(datadir)/doc/xvidcap ; \
	rm -r $(DESTDIR)$(datadir)/xvidcap

install-doc: 
	cd doc && $(MAKE) $(AM_MAKEFLAGS) install-doc

uninstall-doc: 
	cd doc && $(MAKE) $(AM_MAKEFLAGS) uninstall-doc