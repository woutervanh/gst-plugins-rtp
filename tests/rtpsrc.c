#include <gst/check/gstcheck.h>

/* Performs a basic check whether we are able to map URI query parameters to
 * properties in the rtpsrc. Note that it doesn't test whether these are also
 * propagated to the correct child elements or anything. */
GST_START_TEST (test_uri_to_properties)
{
  GstElement *rtpsrc;
  guint buffer_size, latency, pt_change, pt_select, ssrc_change, ssrc_select,
      ttl_mc;
  guint64 timeout;
  gchar *caps, *encoding_name, multicast_iface;
  gboolean enable_rtcp;


  rtpsrc = gst_element_factory_make ("rtpsrc", NULL);

  /* Sets properties to non-default values (make sure this stays in sync) */
  g_object_set (rtpsrc, "uri", "rtp://1.230.1.2?"
      "buffer-size=8192"
      "&caps=application/x-rtp"
      "&latency=300"
      "&multicast-iface=eth0"
      "&pt-change=96"
      "&pt-select=98"
      "&ssrc-change=123456"
      "&ssrc-select=654321" "&timeout=10000" "&ttl-mc=9", NULL);

  g_object_get (rtpsrc,
      "buffer-size", &buffer_size,
      "caps", &caps,
      "latency", &latency,
      "multicast-iface", &multicast_iface,
      "pt-change", &pt_change,
      "pt-select", &pt_select,
      "ssrc-change", &ssrc_change,
      "ssrc-select", &ssrc_select,
      "timeout", &timeout, "ttl-mc", &ttl_mc, NULL);

  /* Make sure these values are in sync with the one from the URI. */
  g_assert_cmpuint (buffer_size, ==, 8192);
  g_assert_cmpuint (latency, ==, 300);
  g_assert_cmpuint (pt_change, ==, 96);
  g_assert_cmpuint (pt_select, ==, 98);
  g_assert_cmpuint (ssrc_change, ==, 123456);
  g_assert_cmpuint (ssrc_select, ==, 654321);
  g_assert_cmpuint (timeout, ==, 10000);
  g_assert_cmpint (ttl_mc, ==, 9);

  gst_object_unref (rtpsrc);
}

GST_END_TEST;

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
  tcase_add_test (tc_chain, test_uri_to_properties);

  return s;
}

GST_CHECK_MAIN (rtpsrc);
