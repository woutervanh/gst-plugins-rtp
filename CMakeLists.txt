project (GSTBARCORTP)
cmake_minimum_required(VERSION 2.8)

set(GSTMGS_PACKAGE "gst-plugins-barcortp")

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/gsoap)
include (CheckIncludeFiles)

# Generate multi arch triplet
EXECUTE_PROCESS(COMMAND dpkg-architecture -qDEB_HOST_MULTIARCH OUTPUT_VARIABLE
CMAKE_ARCH_TRIPLET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(CMAKE_ARCH_TRIPLET)
	message("-- mutliarch triplet detected: " ${CMAKE_ARCH_TRIPLET})
	LIST(INSERT CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES 0
		/lib/${CMAKE_ARCH_TRIPLET}
		/usr/lib/${CMAKE_ARCH_TRIPLET})
endif()

find_package(PkgConfig REQUIRED)

set(CMAKE_C_FLAGS "-g -Wall")

#Required packages for main functionality
pkg_check_modules (GLIB REQUIRED glib-2.0)
pkg_check_modules (GOBJECT REQUIRED gobject-2.0)
if(WIN32)
pkg_check_modules (GIO REQUIRED gio-2.0)
else(WIN32)
pkg_check_modules (GIOUNIX REQUIRED gio-unix-2.0)
endif(WIN32)

pkg_check_modules (GST REQUIRED gstreamer-1.0)
pkg_check_modules (GSTBASE REQUIRED gstreamer-base-1.0)
pkg_check_modules (SOUP REQUIRED libsoup-2.4)

if (NOT WIN32)
add_definitions (${CFLAGS} "-fPIC")
endif (NOT WIN32)

add_subdirectory (src)

EXECUTE_PROCESS(COMMAND head -n 1 ${CMAKE_SOURCE_DIR}/debian/changelog
                COMMAND "awk"  "{print $2}"
                COMMAND "sed" "-e s/-/./g"
                COMMAND "sed" "-e s/(//g"
                COMMAND "sed" "-e s/)//g"
                OUTPUT_VARIABLE GSTMGS_VERSION
                OUTPUT_STRIP_TRAILING_WHITESPACE)

# Version number
MESSAGE(STATUS "Got version " ${GSTMGS_VERSION})

add_definitions(-DDISABLE_DEPRECATED -DGST_USE_UNSTABLE_API)

CONFIGURE_FILE(config.h.cmake config.h)
