set (C_FILES
  "gst-plugins-barcortp.c"
  "gstbarcomgs_common.c"
  "gstrtpsink.c"
  "gstrtpsrc.c"
)

add_definitions ("-DHAVE_CONFIG_H")

include_directories (
  ${CMAKE_BINARY_DIR}
  ${CMAKE_SOURCE_DIR}
  ${GST_INCLUDE_DIRS}
  ${GSTPBUTILS_INCLUDE_DIRS}
  ${SOUP_INCLUDE_DIRS}
)

add_library (gst-plugins-barcortp SHARED ${C_FILES})

target_link_libraries (gst-plugins-barcortp
  ${GST_LIBRARIES}
  ${GSTBASE_LIBRARIES}
  ${GSTRTP_LIBRARIES}
  ${GSTNET_LIBRARIES}
  ${GSTRTSP_LIBRARIES}
  ${GSTPBUTILS_LIBRARIES}
  ${GSTVIDEO_LIBRARIES}
  ${SOUP_LIBRARIES}
)

if (WIN32)
    target_link_libraries (gst-plugins-barcortp
        ws2_32
    )
endif (WIN32)

# For a GNU/Debian based system, the CMAKE_ARCH_TRIPLET needs to be inserted.
# However this is a problem for jhbuild that requires it to be installed in
# the normal location.
SET(CMAKE_UNDER_JHBUILD $ENV{UNDER_JHBUILD})
if(CMAKE_UNDER_JHBUILD)
  MESSAGE(STATUS "Installing libgst-plugins-barcortp.so non-multiarchi (JHBUILD)")
  install (
    TARGETS gst-plugins-barcortp
    DESTINATION lib/gstreamer-1.0
  )
else (CMAKE_UNDER_JHBUILD)
  MESSAGE(STATUS "Installing libgst-plugins-barcortp.so multiarch (DEBIAN)")
  install (
    TARGETS gst-plugins-barcortp
    DESTINATION lib/${CMAKE_ARCH_TRIPLET}/gstreamer-1.0
  )
endif (CMAKE_UNDER_JHBUILD)
