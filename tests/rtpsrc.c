#include <gst/check/gstcheck.h>

GST_START_TEST (test_pads)
{
  GstElement *element;

  element = gst_element_factory_make ("rtpsrc", NULL);
  g_object_set (element, "uri", "rtp://239.1.2.3:4321", NULL);

  gst_object_unref (element);
}

GST_END_TEST;

static Suite *
rtpsrc_suite (void)
{
  Suite *s = suite_create ("rtpsrc");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_pads);

  return s;
}

GST_CHECK_MAIN (rtpsrc);
