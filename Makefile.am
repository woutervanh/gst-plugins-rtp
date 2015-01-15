DISTCHECK_CONFIGURE_FLAGS=--enable-gtk-doc

ALWAYS_SUBDIRS =		\
	gst 			\
	common			\
	pkgconfig

SUBDIRS = $(ALWAYS_SUBDIRS)
DIST_SUBDIRS = $(ALWAYS_SUBDIRS)

EXTRA_DIST = \
	gst-plugins-barcortp.spec depcomp \
	AUTHORS COPYING NEWS README RELEASE REQUIREMENTS \
	ChangeLog gst-plugins-barcortp.doap autogen.sh \
	$(win32)

ACLOCAL_AMFLAGS = -I m4 -I common/m4

include $(top_srcdir)/common/release.mak
include $(top_srcdir)/common/po.mak
include $(top_srcdir)/common/coverage/lcov.mak
