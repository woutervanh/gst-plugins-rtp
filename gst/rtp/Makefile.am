# plugindir is set in configure

plugin_LTLIBRARIES = libgstbarcortp.la

# sources used to compile this plug-in
libgstbarcortp_la_SOURCES = \
		gstbarcomgs_common.c \
		gst-plugins-barcortp.c \
		gstrtpsink.c \
		gstrtpsrc.c 

libgstbarcortp_la_CFLAGS = $(GST_PLUGINS_BARCORTP_CFLAGS) $(GST_BARCORTP_CFLAGS) $(GST_CFLAGS)
libgstbarcortp_la_LIBADD = $(GST_PLUGINS_BASE_LIBS) $(GST_NET_LIBS) \
	-lgstrtsp-$(GST_API_VERSION) 
libgstbarcortp_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstbarcortp_la_LIBTOOLFLAGS = $(GST_PLUGIN_LIBTOOLFLAGS)

# headers we need but don't want installed
noinst_HEADERS = \
		gstbarcomgs_common.h \
		gstrtpparameters.h \
		gstrtpsink.h \
		gstrtpsrc.h
