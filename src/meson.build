# Now do our GStreamer plugin
gst_barcortp_sources = [
  'barcortp.c',
  'gstbarcomgs_common.c',
  'gstrtpsink.c',
  'gstrtpsrc.c'
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

gst_barcortp = shared_library('gstbarcortp',
  gst_barcortp_sources,
  version: pkg_version,
  dependencies: gst_barcortp_dependencies,
  include_directories: config_h_dir,
  install: true,
  install_dir: join_paths(get_option('prefix'), get_option('libdir'), 'gstreamer-1.0'),
)

if build_gir
  gst_barcomgs_headers = [
     'gst_object_set_properties_from_uri_query_parameters.h',
     'gstbarcomgs_common.h',
     'gstrtpparameters.h',
     'gstrtpsink.h'
  ]

  gir_init_section = [
    '--add-init-section=extern void gst_init(gint*,gchar**);' + \
      'g_setenv("GST_REGISTRY_1.0", "@0@", TRUE);'.format(meson.current_build_dir() + '/gir_empty_registry.reg') + \
      'g_setenv("GST_PLUGIN_PATH_1_0", "", TRUE);' + \
      'g_setenv("GST_PLUGIN_SYSTEM_PATH_1_0", "", TRUE);' + \
      'gst_init(NULL,NULL);',
  ]

  gst_barcomgs_gir = gnome.generate_gir(gst_barcomgs,
    sources: [ gst_barcortp_sources, gst_barcomgs_headers ],
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
