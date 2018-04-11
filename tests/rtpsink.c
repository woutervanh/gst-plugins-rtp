#include <gst/check/gstcheck.h>

GST_START_TEST (test_pads)
{
  GstElement *element;
  GstPad *sink_pad;

  element = gst_check_setup_element ("rtpsink");
  fail_if (element == NULL);
  g_object_set (element, "uri", "rtp://239.1.2.3:6000", NULL);

  sink_pad = gst_element_get_request_pad (element, "sink_%u");
  fail_if (sink_pad == NULL);
  gst_element_release_request_pad (element, sink_pad);
  gst_object_unref (sink_pad);

  gst_check_teardown_element (element);
}

GST_END_TEST;

GST_START_TEST (test_pads_localhost)
{
  GstElement *element;
  GstPad *sink_pad;

  element = gst_check_setup_element ("rtpsink");
  fail_if (element == NULL);
  g_object_set (element, "uri", "rtp://localhost", NULL);

  sink_pad = gst_element_get_request_pad (element, "sink_%u");
  fail_if (sink_pad == NULL);
  gst_element_release_request_pad (element, sink_pad);
  gst_object_unref (sink_pad);

  gst_check_teardown_element (element);
}

GST_END_TEST;

GST_START_TEST (test_pads_localhost_3_slashes)
{
  GstElement *element;
  GstPad *sink_pad;

  element = gst_check_setup_element ("rtpsink");
  fail_if (element == NULL);
  g_object_set (element, "uri", "rtp:///localhost", NULL);

  sink_pad = gst_element_get_request_pad (element, "sink_%u");
  fail_if (sink_pad != NULL);

  gst_check_teardown_element (element);
}

GST_END_TEST;

static Suite *
rtpsink_suite (void)
{
  Suite *s = suite_create ("rtpsink");
  TCase *tc_chain = tcase_create ("general");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, test_pads);
  tcase_add_test (tc_chain, test_pads_localhost);
  tcase_add_test (tc_chain, test_pads_localhost_3_slashes);

  return s;
}

GST_CHECK_MAIN (rtpsink);
