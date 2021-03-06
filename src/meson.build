# Now do our GStreamer plugin
gst_barcortp_sources = [
  'barcortp.c',
  'gstbarcomgs_common.c',
  'gstrtpsink.c',
  'gstrtpsrc.c'
]

gst_barcortp_headers = [
   'gstbarcomgs_common.h',
   'gstrtpparameters.h',
   'gstrtpsink.h'
]

gst_barcortp_dependencies = [
  libmath,
  gstreamer,
  glib ,
  gobject,
  gio ,
  gio_os,
  gstreamer_rtp,
  gstreamer_rtsp,
  gstreamer_app,
  gstreamer_video,
  gstreamer_pbutils,
  gstreamer_sdp
]

include_default = '''
#include <gst/gst.h>
'''

if cc.has_function(
            'gst_object_set_properties_from_uri_query_parameters',
            prefix: include_default,
            dependencies: gst_barcortp_dependencies,
            include_directories: config_h_dir)
  define_c_flags = '-DHAVE_GST_OBJECT_SET_PROPERTIES_FROM_URI_QUERY_PARAMETERS'
else
  gst_barcortp_sources += 'gst_object_set_properties_from_uri_query_parameters.c'
  gst_barcortp_headers += 'gst_object_set_properties_from_uri_query_parameters.h'
  define_c_flags = []
endif

gst_barcortp = shared_library('gstbarcortp',
  gst_barcortp_sources,
  dependencies: gst_barcortp_dependencies,
  include_directories: config_h_dir,
  install: true,
  c_args: define_c_flags,
  install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'gstreamer-1.0'),
)

if build_gir

  gir_init_section = [
    '--add-init-section=extern void gst_init(gint*,gchar**);' + \
      'g_setenv("GST_REGISTRY_1.0", "@0@", TRUE);'.format(meson.current_build_dir() + '/gir_empty_registry.reg') + \
      'g_setenv("GST_PLUGIN_PATH_1_0", "", TRUE);' + \
      'g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", "", TRUE);' + \
      'gst_init(NULL,NULL);',
  ]

  gst_barcortp_gir = gnome.generate_gir(gst_barcortp,
    sources: [ gst_barcortp_sources, gst_barcortp_headers ],
    dependencies: gst_barcortp_dependencies,
    namespace: 'Gst',
    nsversion: meson.project_version(),
    identifier_prefix : 'Gst',
    symbol_prefix : 'gst',
    includes : ['Gst-1.0'],
    extra_args : gir_init_section + ['-DGST_USE_UNSTABLE_API'],
    install: true,
  )
endif
