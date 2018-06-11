project('gst-plugins-barcortp', 'c')

gnome = import('gnome')

# Options
buildtype = get_option('buildtype')
build_gir = get_option('build-gir')
build_tests = get_option('build-tests')

# Version
# This is a quick one-liner to get the package version.
pkg_build_version = run_command('sed', '-e', '1!d', '-e', 's/.*(\([^)]*\)).*/\1/', files('debian/changelog')).stdout().strip()
pkg_version = pkg_build_version.split('~')[0]

# General variables
config_h_dir = include_directories('.')
is_windows = host_machine.system() == 'windows'

# Dependencies
cc = meson.get_compiler('c')

libmath = cc.find_library('m')
glib = dependency('glib-2.0')
gobject = dependency('gobject-2.0')
gio = dependency('gio-2.0')
if is_windows
  gio_os = dependency('gio-windows-2.0')
else
  gio_os = dependency('gio-unix-2.0')
endif

gstreamer = dependency('gstreamer-1.0')
gstreamer_base = dependency('gstreamer-base-1.0')
gstreamer_check = dependency('gstreamer-check-1.0')
gstreamer_rtp = dependency('gstreamer-rtp-1.0')
gstreamer_rtsp = dependency('gstreamer-rtsp-1.0')
gstreamer_app = dependency('gstreamer-app-1.0')
gstreamer_video = dependency('gstreamer-video-1.0')
gstreamer_pbutils = dependency('gstreamer-pbutils-1.0')
gstreamer_sdp = dependency('gstreamer-sdp-1.0')
gnu_patch = find_program('patch', required: false) # Needed for post-commit hook
# shmmutex
threads = dependency('threads')
librt = cc.find_library('rt')

# Configuration
conf = configuration_data()
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_VERSION', pkg_version)
conf.set10('HAVE_CONFIG_H', true)
configure_file(
  output: 'config.h',
  configuration: conf,
)

# Global C flags
global_c_args = [
  # Config
  '-include', 'config.h',
  # General
  '-std=gnu99',
  '-D_FORTIFY_SOURCE=2',
  '-DGST_USE_UNSTABLE_API',
  '-DG_THREADS_MANDATORY',
  '-DG_DISABLE_CAST_CHECKS',
  '-DG_DISABLE_ASSERT',
  '-Wall',
  '-Wdeclaration-after-statement',
  '-Wvla',
  '-Wpointer-arith',
  '-Wmissing-declarations',
  '-Wmissing-prototypes',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Wold-style-definition',
  '-Waggregate-return',
  '-Winit-self',
  '-Wmissing-include-dirs',
  '-Waddress',
  '-Wno-multichar',
  '-Wnested-externs',
  '-DGST_DISABLE_DEPRECATED',
  '-fstack-protector',
  '--param=ssp-buffer-size=4',
  '-Wformat',
  '-Werror=format-security',
  '-Wno-error',
  '-DDISABLE_DEPRECATED',
  '-DGST_USE_UNSTABLE_API',
  '-DHAVE_GST_OBJECT_SET_PROPERTIES_FROM_URI_QUERY_PARAMETERS'
]

if buildtype != 'debug'
  global_c_args += [
    '-O2'
  ]
else
  global_c_args += [
    '-O0',
    '-g'
  ]
endif

if is_windows
  global_c_args += '-fPIC'
endif

add_project_arguments(global_c_args, language: 'c')

# Subdirectories
subdir('src')
if build_tests
  subdir('tests')
endif
