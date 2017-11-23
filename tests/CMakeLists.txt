include_directories (
  ${GLIB_INCLUDE_DIRS}
  ${GTK_INCLUDE_DIRS}
  ${GST_INCLUDE_DIRS}
  ${GSTCHECK_INCLUDE_DIRS}
)

add_executable (rtpsinktest rtpsink.c)
add_test(rtpsink rtpsinktest)

target_link_libraries (rtpsinktest
	${GLIB_LIBRARIES}
	${GST_LIBRARIES}
	${GSTBASE_LIBRARIES}
	${GSTCHECK_LIBRARIES}
)

add_executable (rtpsrctest rtpsrc.c)
add_test(rtpsrc rtpsrctest)

target_link_libraries (rtpsrctest
	${GLIB_LIBRARIES}
	${GST_LIBRARIES}
	${GSTBASE_LIBRARIES}
	${GSTCHECK_LIBRARIES}
)
