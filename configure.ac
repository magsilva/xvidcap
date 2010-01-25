#########################################################
# GENERAL STUFF
#########################################################
AC_PREREQ(2.60)
AC_INIT([xvidcap], [1.2], [magsilva@gmail.com])
AM_INIT_AUTOMAKE([1.10 foreign])
AM_CONFIG_HEADER(config.h)
AC_USE_SYSTEM_EXTENSIONS(_GNU_SOURCE)


################################################################
# Checks compiler
################################################################
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_CC_C99
AC_PROG_CPP_WERROR
AC_C_CONST
AC_C_RESTRICT
AC_C_INLINE
AC_C_VARARRAYS
AC_C_TYPEOF
AC_CACHE_SAVE


################################################################
# Checks for header files.
################################################################

AC_HEADER_STDC
AC_HEADER_TIME
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h stdint.h ctype.h errno.h netinet/in.h],, [exit 1],)
AC_CACHE_SAVE



################################################################
# Checks for general library functions.
################################################################

AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([fdatasync gettimeofday memmove memset munmap strcasecmp strchr strdup strstr getopt_long], , [exit 1],)
AC_SYS_LARGEFILE
AC_SYS_LONG_FILE_NAMES
AC_CACHE_SAVE



#########################################################
# SCROLLKEEPER
#########################################################
SCROLLKEEPER_BUILD_REQUIRED=0.3.5
AC_SUBST(SCROLLKEEPER_BUILD_REQUIRED)

AC_PATH_PROG(SK_CONFIG, scrollkeeper-config, no)
if test x$SK_CONFIG = xno; then
	AC_MSG_ERROR(Couldn't find scrollkeeper-config. Please install the scrollkeeper package: http://scrollkeeper.sourceforge.net if you plan to install the documentation through the install-doc target.)
fi
# here follows a hack to get the docs installed in a separate, recursive install-doc target
MK=''; AC_SUBST(MK)
AC_CACHE_SAVE


#########################################################
# Man pages
#########################################################
AC_PATH_PROG(DOCBOOK2X_MAN, docbook2x-man, no)
if test x$DOCBOOK2X_MAN = xno; then
  AC_MSG_NOTICE(Couldn't find docbook2x-man to generate current man pages. Will try db2x_docbook2man.)
  unset ac_cv_path_DOCBOOK2X_MAN
  AC_PATH_PROG(DOCBOOK2X_MAN, db2x_docbook2man, no)
  if test x$DOCBOOK2X_MAN = xno; then
  	AC_MSG_ERROR(Couldn't find neither docbook2x-man nor db2x_docbook2man to generate current man pages.)
  fi
fi

AC_PATH_PROG(XML2PO, xml2po, no)
if test x$XML2PO = xno; then
  AC_MSG_ERROR(Couldn't find xml2po to translate current man pages.)
fi
AC_CACHE_SAVE


########################################################
# i18n
#########################################################
GETTEXT_PACKAGE=xvidcap
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [GETTEXT package name])

ALL_LINGUAS="de en it es"
AM_GLIB_GNU_GETTEXT
AC_CACHE_SAVE



########################################################
# PACKAGE CHECKS
#########################################################
PKG_CHECK_MODULES(PACKAGE, [gtk+-2.0 >= 2.4.0 libglade-2.0 glib-2.0 gthread-2.0])
PKG_CHECK_EXISTS([dbus-1 dbus-glib-1], [ac_my_dbus_usable=yes], [ac_my_dbus_usable=no])
if ( test x${ac_my_dbus_usable} = "xyes" ) ; then
	_PKG_CONFIG([dbus_CFLAGS], [cflags], [dbus-1 dbus-glib-1])
	PACKAGE_CFLAGS="${PACKAGE_CFLAGS} ${pkg_cv_dbus_CFLAGS}"
	_PKG_CONFIG([dbus_LIBS], [libs], [dbus-1 dbus-glib-1])
	PACKAGE_LIBS="${PACKAGE_LIBS} ${pkg_cv_dbus_LIBS}"
fi
AC_SUBST(PACKAGE_CFLAGS)
AC_SUBST(PACKAGE_LIBS)
AC_CACHE_SAVE


#########################################################
# X11 libraries
#########################################################

AC_PATH_X
AC_PATH_XTRA

# first check for Xmu without tweaking, then in X11 paths
AC_CHECK_LIB(Xmu, XmuClientWindow,,[echo "Couldn't find libXmu in LD_LIBRARY_PATH, checking X11 paths"; AC_CHECK_LIB(Xmu,XmuClientWindow,LDFLAGS="${LDFLAGS} -L${ac_x_libraries} -Xlinker -R${ac_x_libraries}"; LIBS="${LIBS} -lXmu",echo "libXmu required for drawing the frame around the area to capture" && exit 1,[-L${ac_x_libraries}])])

# first check for Xfixes without tweaking, then in X11 paths
AC_CHECK_LIB(Xfixes, XFixesGetCursorImage,,[echo "Couldn't find libXfixes in LD_LIBRARY_PATH, checking X11 paths"; AC_CHECK_LIB(Xfixes,XFixesGetCursorImage,LDFLAGS="${LDFLAGS} -L${ac_x_libraries} -Xlinker -R${ac_x_libraries}"; LIBS="${LIBS} -lXfixes",[echo "libXfixes not available, cannot capture real mouse pointer"],[-L${ac_x_libraries}])])

# first check for Xdamage without tweaking, then in X11 paths
AC_CHECK_LIB(Xdamage, XDamageSubtract,,[echo "Couldn't find libXdamage in LD_LIBRARY_PATH, checking X11 paths"; AC_CHECK_LIB(Xdamage,XDamageSubtract,LDFLAGS="${LDFLAGS} -L${ac_x_libraries} -Xlinker -R${ac_x_libraries}"; LIBS="${LIBS} -lXdamage",[echo "libXdamage not available, cannot use delta screenshots"],[-L${ac_x_libraries}])])
AC_CACHE_SAVE


#########################################################

AC_CHECK_LIB(m,pow,,echo "libm required by ffmpeg")

AC_CHECK_LIB(mp3lame, lame_init,, [echo "To use mp3 audio codecs you need liblame/liblame-dev"], [-lmp3lame])
AC_CACHE_SAVE

AC_CHECK_LIB(vorbisenc, vorbis_encode_init,, [echo "To use the vorbis audio codec you need libvorbisenc/libvorbisenc-dev"], [-lvorbis])
AC_CACHE_SAVE

AC_CHECK_LIB(ogg, ogg_sync_init,,[echo "To use the theora video codec you also need libogg/libogg-dev"])
AC_CACHE_SAVE

AC_CHECK_LIB(theora, theora_encode_init,, [echo "To use the theora video codec you need libtheora/libtheora-dev"], [-logg])
AC_CACHE_SAVE

ACX_PTHREAD([
	LIBS="$PTHREAD_LIBS $LIBS"
	CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
	CC="$PTHREAD_CC"
],[
	echo "xvidcap requires libpthread for recording. This machine doesn't have the libraries and/or header files installed."
	exit 1
])

AC_CHECK_LIB(avutil, av_rescale,, [echo "You need FFMPEG"])
AC_CHECK_LIB(swscale, sws_scale,, [echo "You need FFMPEG"])
AC_CHECK_LIB(avcodec, avcodec_encode_video,, [echo "You need FFMPEG"])
AC_CHECK_LIB(avformat, av_write_header,, [echo "You need FFMPEG"])
AC_CHECK_LIB(avdevice, avdevice_register_all,, [echo "You need FFMPEG"])
AC_CHECK_LIB(avcodec, avcodec_decode_audio,, [echo "You need FFMPEG"])
AC_CACHE_SAVE


AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_PROG_SED
AC_OUTPUT([
doc/Makefile
doc/xvidcap/Makefile
doc/xvidcap/C/Makefile
doc/xvidcap/de/Makefile
doc/xvidcap/it/Makefile
doc/xvidcap/es/Makefile
doc/man/Makefile
doc/man/C/Makefile
doc/man/de/Makefile
doc/man/it/Makefile
doc/man/es/Makefile
po/Makefile.in
src/Makefile
Makefile
])



