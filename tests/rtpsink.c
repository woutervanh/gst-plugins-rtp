#include <gst/check/gstcheck.h>

/* Performs a basic check whether we are able to map URI query parameters to
 * properties in the rtpsink. Note that it doesn't test whether these are also
 * propagated to the correct child elements or anything. */
GST_START_TEST (test_uri_to_properties)
{
  GstElement *rtpsink;

  gchar *bind_address;
  guint cidr;
  gint bind_port, ttl, ttl_mc, src_port;

  rtpsink = gst_element_factory_make ("rtpsink", NULL);

  /* Sets properties to non-default values (make sure this stays in sync) */
  g_object_set (rtpsink, "uri", "rtp://1.230.1.2?"
      "cidr=24"
      "&bind-address=1.2.3.4"
      "&bind-port=5678"
      "&ttl=8"
      "&ttl-mc=9",
      NULL);

  g_object_get (rtpsink,
      "cidr", &cidr,
      "bind_port", &bind_port,
      "ttl", &ttl,
      "ttl_mc", &ttl_mc,
      "bind-address", &bind_address,
      "src-port", &src_port,
      NULL);

  /* Make sure these values are in sync with the one from the URI. */
  g_assert_cmpint (ttl , ==, 8);
  g_assert_cmpint (ttl_mc , ==, 9);
  g_assert_cmpuint (cidr , ==, 24);
  g_assert_cmpuint (bind_port, ==, 5678);
  g_assert_cmpuint (src_port, ==, 5678);
  g_assert_cmpstr (bind_address, ==, "1.2.3.4");

  g_object_set (rtpsink, "src-port", 1234,
      NULL);

  g_object_get (rtpsink,
      "src-port", &src_port,
      "bind-port", &bind_port,
      NULL);

  g_assert_cmpuint (src_port, ==, 1234);
  g_assert_cmpuint (bind_port, ==, 1234);

  gst_object_unref (rtpsink);
}
GST_END_TEST;

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
  tcase_add_test (tc_chain, test_uri_to_properties);

  /*tcase_add_test (tc_chain, test_pads);*/
  /*tcase_add_test (tc_chain, test_pads_localhost);*/
  /*tcase_add_test (tc_chain, test_pads_localhost_3_slashes);*/

  return s;
}

GST_CHECK_MAIN (rtpsink);
