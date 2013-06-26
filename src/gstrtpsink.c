/* ex: set tabstop=2 shiftwidth=2 expandtab: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <netinet/in.h>
#include <arpa/inet.h>

#include "gstrtpsink.h"
#include "gstbarcomgs_common.h"

GST_DEBUG_CATEGORY_STATIC (rtp_sink_debug);
#define GST_CAT_DEFAULT rtp_sink_debug

enum
{
  PROP_0,
  PROP_URI,
  PROP_TTL,
  PROP_TTL_MC,
  PROP_SRC_PORT,
  PROP_FORCE_IPV4,
  PROP_ENCRYPT,
  PROP_KEY_DERIV_RATE,
  PROP_LAST
};

#define DEFAULT_PROP_URI    		      "udp://0.0.0.0:5004"
#define DEFAULT_PROP_MUXER  		      NULL
#define DEFAULT_PROP_TTL              (64)
#define DEFAULT_PROP_TTL_MC           (8)
#define DEFAULT_SRC_PORT              (0)
#define DEFAULT_PROP_FORCE_IPV4       (FALSE)
#define LOCAL_ADDRESS_IPV4			      "0.0.0.0" /* "127.0.0.1" */
#define LOCAL_ADDRESS_IPV6			      "::"      /* "::1" */
#define DEFAULT_PROP_ENCRYPT          (TRUE)
#define DEFAULT_PROP_KEY_DERIV_RATE   (0)

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-rtp"));

static void gst_rtp_sink_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

#define gst_rtp_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSink, gst_rtp_sink, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_rtp_sink_uri_handler_init));

static void gst_rtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_rtp_sink_finalize (GObject * gobject);
static GstStateChangeReturn
gst_rtp_sink_change_state (GstElement * element, GstStateChange transition);
static gboolean gst_rtp_sink_is_multicast (const gchar * ip_addr);

static void
gst_rtp_sink_class_init (GstRtpSinkClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  oclass->set_property = gst_rtp_sink_set_property;
  oclass->get_property = gst_rtp_sink_get_property;
  oclass->finalize = gst_rtp_sink_finalize;

  /**
   * GstRtpSink::uri
   *
   * uri to establish a stream to. All GStreamer parameters can be
   * encoded in the URI, this URI format is RFC compliant.
   *
   * Since: 0.10.5
   */
  g_object_class_install_property (oclass, PROP_URI,
      g_param_spec_string ("uri", "URI", "URI to save",
          DEFAULT_PROP_URI, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink::ttl
   *
   * Unicast TTL (for use in routed networks)
   *
   * Since: 0.10.5
   */
  g_object_class_install_property (oclass, PROP_TTL,
      g_param_spec_int ("ttl", "Unicast TTL",
          "Used for setting the unicast TTL parameter",
          0, 255, DEFAULT_PROP_TTL,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink::ttl-mc
   *
   * Multicast TTL (for use in routed networks)
   *
   * Since: 0.10.5
   */
  g_object_class_install_property (oclass, PROP_TTL_MC,
      g_param_spec_int ("ttl-mc", "Multicast TTL",
          "Used for setting the multicast TTL parameter", 0, 255,
          DEFAULT_PROP_TTL_MC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink::src-port
   *
   * Set source port for sending data (default random)
   *
   * Since: 0.10.5
   */
  g_object_class_install_property (oclass, PROP_SRC_PORT,
      g_param_spec_int ("src-port", "src-port", "The sender source port"
          " (0 = dynamic)",
          0, 65535, DEFAULT_SRC_PORT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink::force-ipv4
   *
   * Force the use of IPv4
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass, PROP_FORCE_IPV4,
      g_param_spec_boolean ("force-ipv4", "Force IPv4",
          "Force only IPv4 sockets", DEFAULT_PROP_FORCE_IPV4,
          G_PARAM_READWRITE));

  /**
   * GstRtpSink::encrypt
   *
   * Are RTP packets encrypted using AES?
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass, PROP_ENCRYPT,
      g_param_spec_boolean ("encrypt", "Data encrypted",
          "Data are encrypted using AES", DEFAULT_PROP_ENCRYPT,
          G_PARAM_READWRITE));

  /**
   * GstRtpSink::rate
   *
   * Key derivation rate for AES
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass, PROP_KEY_DERIV_RATE,
      g_param_spec_uint ("rate",
          "Key derivation rate",
          "Key derivation rate (This value should be a power of 2)"
          " (not yet implemented)",
          0, G_MAXUINT32, DEFAULT_PROP_KEY_DERIV_RATE, G_PARAM_READWRITE));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_rtp_sink_change_state);

  gst_element_class_set_static_metadata (gstelement_class,
      "RtpSink",
      "Generic/Bin/Sink",
      "Barco Rtp sink",
      "Thijs Vermeir <thijs.vermeir@barco.com>, "
      "Marc Leeman <marc.leeman@barco.com>, "
      "Paul Henrys <visechelle@gmail.com>");

  GST_DEBUG_CATEGORY_INIT (rtp_sink_debug,
      "barcortpsink", 0, "Barco rtp send bin");

}

static gboolean
gst_rtp_sink_create_and_bind_rtp_socket (GstRtpSink * rtpsink)
{
  GError *err = NULL;
  GInetAddress *bind_addr;
  GSocketAddress *bind_saddr;

  if (0 == rtpsink->src_port) {
    GST_INFO_OBJECT (rtpsink, "src-port is set to 0 => Don't bind");
    return FALSE;
  }

  GST_DEBUG_OBJECT (rtpsink, "Create rtp_sink socket");

  /* create sender sockets try IPV6, fall back to IPV4 */
  if (rtpsink->force_ipv4 || (rtpsink->rtp_sink_socket =
          g_socket_new (G_SOCKET_FAMILY_IPV6,
              G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &err)) == NULL) {
    if ((rtpsink->rtp_sink_socket = g_socket_new (G_SOCKET_FAMILY_IPV4,
                G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &err)) == NULL)
      goto no_socket;
  }

  /* Allow broadcast on sockets */
  g_socket_set_broadcast (rtpsink->rtp_sink_socket, TRUE);

  GST_DEBUG_OBJECT (rtpsink, "Bind rtp_sink socket on port %d",
      rtpsink->src_port);

  /*  */
  if (G_SOCKET_FAMILY_IPV6 == g_socket_get_family (rtpsink->rtp_sink_socket))
    bind_addr = g_inet_address_new_from_string (LOCAL_ADDRESS_IPV6);
  else
    bind_addr = g_inet_address_new_from_string (LOCAL_ADDRESS_IPV4);

  bind_saddr = g_inet_socket_address_new (bind_addr, rtpsink->src_port);

  if (!g_socket_bind (rtpsink->rtp_sink_socket, bind_saddr, TRUE, &err))
    goto no_bind;

  GST_DEBUG_OBJECT (rtpsink->rtp_sink,
      "RTP UDP sink has sock %p which binds on port %d",
      rtpsink->rtp_sink_socket, rtpsink->src_port);

  return TRUE;

  /* ERRORS */
no_socket:
  {
    GST_ELEMENT_ERROR ((GstElement *) rtpsink, RESOURCE, FAILED, (NULL),
        ("Could not create sockets: %s", err->message));
    g_clear_error (&err);
    return FALSE;
  }
no_bind:
  {
    GST_ELEMENT_WARNING (rtpsink, RESOURCE, SETTINGS, (NULL),
        ("Could not bind socket: %s (%d)", err->message, err->code));
    g_clear_error (&err);
    g_object_unref (rtpsink->rtp_sink_socket);
    return FALSE;
  }
}

static void
gst_rtp_sink_retrieve_rtcp_src_socket (GstRtpSink * rtpsink)
{
  if (NULL == rtpsink->rtcp_src_socket) {
    g_object_get (G_OBJECT (rtpsink->rtcp_src), "used-socket",
        &(rtpsink->rtcp_src_socket), NULL);
    if (!G_IS_SOCKET (rtpsink->rtcp_src_socket))
      GST_WARNING_OBJECT (rtpsink, "No valid socket retrieved from udpsrc");
    else
      GST_DEBUG_OBJECT (rtpsink->rtcp_sink, "RTCP UDP src has sock %p",
          rtpsink->rtcp_src_socket);
  }
}

static void
close_and_unref_socket (GSocket * socket)
{
  GError *err = NULL;

  if (socket != NULL && !g_socket_close (socket, &err)) {
    GST_ERROR ("Failed to close socket %p: %s", socket, err->message);
    g_clear_error (&err);
  }
  if (socket)
    g_object_unref (socket);
}

static void
gst_rtp_sink_check_uri (GstRtpSink * rtpsink)
{
  if (rtpsink->uri) {
    GInetAddress *addr = g_inet_address_new_from_string (rtpsink->uri->host);
    GST_DEBUG_OBJECT (rtpsink, "host is %s", rtpsink->uri->host);
    if (g_inet_address_get_family (addr) == G_SOCKET_FAMILY_IPV4) {
      GST_DEBUG_OBJECT (rtpsink, "forcing use of IPv4 based on %s",
          rtpsink->uri->host);
      g_object_set (G_OBJECT (rtpsink), "force-ipv4", TRUE, NULL);
    }
    g_object_unref (addr);

    if (rtpsink->uri->query) {
      GHashTable *hash_table = NULL;
      GList *keys = NULL, *key;

      hash_table = soup_form_decode (rtpsink->uri->query);
      keys = g_hash_table_get_keys (hash_table);

      for (key = keys; key; key = key->next) {
        if (g_strcmp0 ((gchar *) key->data, "ttl") == 0) {
          g_object_set (G_OBJECT (rtpsink), "ttl",
              g_ascii_strtoll (
                  (gchar *) g_hash_table_lookup (hash_table, key->data),
                  NULL, 0), NULL);
          GST_DEBUG_OBJECT (rtpsink, "ttl: %d", rtpsink->ttl);
        } else if (g_strcmp0 ((gchar *) key->data, "ttl-mc") == 0) {
          g_object_set (G_OBJECT (rtpsink), "ttl-mc",
              g_ascii_strtoll (
                  (gchar *) g_hash_table_lookup (hash_table, key->data),
                  NULL, 0), NULL);
          GST_DEBUG_OBJECT (rtpsink, "ttl-mc: %d", rtpsink->ttl_mc);
        } else if (g_strcmp0 ((gchar *) key->data, "src-port") == 0) {
          g_object_set (G_OBJECT (rtpsink), "src-port",
              g_ascii_strtoll (
                  (gchar *) g_hash_table_lookup (hash_table, key->data),
                  NULL, 0), NULL);
          GST_DEBUG_OBJECT (rtpsink, "src-port: %d", rtpsink->src_port);
        } else if (g_strcmp0 ((gchar *) key->data, "force-ipv4") == 0) {
          g_object_set (G_OBJECT (rtpsink), "force-ipv4",
              gst_barco_query_to_boolean ((gchar *)
                  g_hash_table_lookup (hash_table, key->data)), NULL);
          GST_DEBUG_OBJECT (rtpsink, "force-ipv4: %d", rtpsink->force_ipv4);
        } else if (g_strcmp0 ((gchar *) key->data, "encrypt") == 0) {
          g_object_set (G_OBJECT (rtpsink), "encrypt",
              gst_barco_query_to_boolean ((gchar *)
                  g_hash_table_lookup (hash_table, key->data)), NULL);
          GST_DEBUG_OBJECT (rtpsink, "encrypt: %s",
              rtpsink->encrypt ? "TRUE" : "FALSE");
        } else if (g_strcmp0 ((gchar *) key->data, "rate") == 0) {
          g_object_set (G_OBJECT (rtpsink), "rate",
              g_ascii_strtoull (
                  (gchar *) g_hash_table_lookup (hash_table, key->data),
                  NULL, 0), NULL);
          GST_DEBUG_OBJECT (rtpsink, "rate: %u", rtpsink->key_derivation_rate);
        }
      }
    }
    GST_DEBUG_OBJECT (rtpsink, "Configuring udp sinks/sources");
  }
}

static void
gst_rtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSink *rtpsink = GST_RTP_SINK (object);

  switch (prop_id) {
    case PROP_URI:
      if (rtpsink->uri)
        soup_uri_free (rtpsink->uri);
      rtpsink->uri = soup_uri_new (g_value_get_string (value));
      gst_rtp_sink_check_uri (rtpsink);
      break;
    case PROP_TTL:
      rtpsink->ttl = g_value_get_int (value);
      break;
    case PROP_TTL_MC:
      rtpsink->ttl_mc = g_value_get_int (value);
      break;
    case PROP_SRC_PORT:
      rtpsink->src_port = g_value_get_int (value);
      close_and_unref_socket (rtpsink->rtp_sink_socket);
      gst_rtp_sink_create_and_bind_rtp_socket (rtpsink);
      break;
    case PROP_FORCE_IPV4:
      rtpsink->force_ipv4 = g_value_get_boolean (value);
      xgst_barco_propagate_setting (rtpsink, "force-ipv4", rtpsink->force_ipv4);
      break;
    case PROP_ENCRYPT:
      rtpsink->encrypt = g_value_get_boolean (value);
      break;
    case PROP_KEY_DERIV_RATE:
      rtpsink->key_derivation_rate = g_value_get_uint (value);
      if (rtpsink->rtpencrypt)
        g_object_set (G_OBJECT (rtpsink->rtpencrypt), "rate",
            rtpsink->key_derivation_rate, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_sink_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtpSink *rtpsink = GST_RTP_SINK (object);

  switch (prop_id) {
    case PROP_URI:
      if (rtpsink->uri) {
        gchar *string = soup_uri_to_string (rtpsink->uri, FALSE);

        g_value_set_string (value, string);
        g_free (string);
      } else {
        g_value_set_string (value, NULL);
      }
      break;
    case PROP_TTL:
      g_value_set_int (value, rtpsink->ttl);
      break;
    case PROP_TTL_MC:
      g_value_set_int (value, rtpsink->ttl_mc);
      break;
    case PROP_SRC_PORT:
      g_value_set_int (value, rtpsink->src_port);
      break;
    case PROP_FORCE_IPV4:
      g_value_set_boolean (value, rtpsink->force_ipv4);
      break;
    case PROP_ENCRYPT:
      g_value_set_boolean (value, rtpsink->encrypt);
      break;
    case PROP_KEY_DERIV_RATE:
      g_value_set_uint (value, rtpsink->key_derivation_rate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_sink_rtpbin_pad_added_cb (GstElement * element,
    GstPad * pad, gpointer data)
{
  GstRtpSink *rtpsink = GST_RTP_SINK (data);
  GstElement *elt;
  GstPad *target;
  GstCaps *caps;
  gchar *name;

  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK) {
    GST_DEBUG_OBJECT (rtpsink, "not interested in sink pads");
    caps = gst_pad_query_caps (pad, NULL);
    GST_DEBUG_OBJECT (rtpsink, "caps are %" GST_PTR_FORMAT, caps);
    gst_caps_unref (caps);
    return;
  }

  caps = gst_pad_query_caps (pad, NULL);
  if (caps) {
    GstCaps *rtcp_caps = gst_caps_new_empty_simple ("application/x-rtcp");

    if (gst_caps_can_intersect (caps, rtcp_caps)) {
      GST_DEBUG_OBJECT (rtpsink, "not interested in pad with rtcp caps");
      gst_caps_unref (rtcp_caps);
      gst_caps_unref (caps);
      return;
    }
    gst_caps_unref (rtcp_caps);
    gst_caps_unref (caps);
  }

  elt = (rtpsink->encrypt) ? rtpsink->rtpencrypt : rtpsink->rtp_sink;

  target = gst_element_get_static_pad (elt, "sink");
  if (gst_pad_is_linked (target)) {
    GST_WARNING_OBJECT (rtpsink,
        "new pad on rtpbin, but there was already a src pad");
    gst_object_unref (target);
    return;
  }

  name = gst_element_get_name (elt);
  GST_DEBUG_OBJECT (rtpsink, "linking new rtpbin src pad to %s sink pad", name);
  g_free (name);
  gst_pad_link (pad, target);
  gst_object_unref (target);
}

static void
gst_rtp_sink_set_sdes (GstElement * rtpbin, const gchar * prop,
    const gchar * val)
{
  GstStructure *s;

  g_object_get (G_OBJECT (rtpbin), "sdes", &s, NULL);
  gst_structure_set (s, prop, G_TYPE_STRING, val, NULL);
  g_object_set (G_OBJECT (rtpbin), "sdes", s, NULL);

  gst_structure_free (s);
}

static gboolean
gst_rtp_sink_rtp_bin_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  gboolean ret = FALSE;
  GstElement *rtpbin;

  rtpbin = GST_ELEMENT (parent);
  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CUSTOM_DOWNSTREAM:
    {
      const GstStructure *s = gst_event_get_structure (event);

      /* New source signal, send uri over SDES in RTCP */
      if (gst_structure_has_name (s, "GstTouringNewSource")) {
        const gchar *uri = gst_structure_get_string (s, "uri");

        gst_rtp_sink_set_sdes (rtpbin, "note", uri);
      }

      ret = TRUE;
    }
      break;
    default:
      ret = gst_pad_event_default (pad, NULL, event);
      break;
  }

  return ret;
}

#define gst_rtp_sink_find_property(property, value) \
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (new_element), property)){ \
    GST_DEBUG_OBJECT(rtpsink, "element supports " property); \
    g_object_set (G_OBJECT(new_element), property, value, NULL); \
  }

static void
gst_rtp_sink_rtpsink_element_added (GstRtpSink * rtpsink,
    GstElement * new_element, gpointer data)
{
  g_return_if_fail (new_element != NULL);

  /* set minimal interval on 0.5s */
  gst_rtp_sink_find_property ("rtcp-min-interval", (guint64) 500000000);
}

static gboolean
gst_rtp_sink_start (GstRtpSink * rtpsink)
{
  GstElement *rtpbin;
  GstPad *pad;
  GstCaps *caps = NULL;
  gchar *uri = NULL;

  GST_DEBUG_OBJECT (rtpsink, "Creating correct modules");
  rtpbin = gst_element_factory_make ("rtpbin", NULL);
  rtpsink->rtp_sink = gst_element_factory_make ("udpsink", NULL);
  rtpsink->rtcp_sink = gst_element_factory_make ("udpsink", NULL);
  rtpsink->rtcp_src = gst_element_factory_make ("udpsrc", NULL);

  g_return_val_if_fail (rtpbin != NULL, FALSE);
  g_return_val_if_fail (rtpsink->rtp_sink != NULL, FALSE);
  g_return_val_if_fail (rtpsink->rtcp_sink != NULL, FALSE);
  g_return_val_if_fail (rtpsink->rtcp_src != NULL, FALSE);

  if (rtpsink->encrypt) {
    rtpsink->rtpencrypt = gst_element_factory_make ("rtpencrypt", NULL);
    g_return_val_if_fail (rtpsink->rtpencrypt != NULL, FALSE);
    g_object_set (G_OBJECT (rtpsink->rtpencrypt), "rate",
        rtpsink->key_derivation_rate, NULL);
  }

  /* Set properties */
  g_object_set (G_OBJECT (rtpsink->rtp_sink),
      "async", FALSE,
      "ttl", rtpsink->ttl,
      "ttl-mc", rtpsink->ttl_mc,
      "force-ipv4", rtpsink->force_ipv4,
      "host", rtpsink->uri->host, "port", rtpsink->uri->port,
      "socket", rtpsink->rtp_sink_socket, "auto-multicast", TRUE, NULL);

  /* auto-multicast should be set to false as rtcp_src will already
   * join the multicast group */
  g_object_set (G_OBJECT (rtpsink->rtcp_sink),
      "sync", FALSE,
      "async", FALSE,
      "ttl", rtpsink->ttl,
      "ttl-mc", rtpsink->ttl_mc,
      "force-ipv4", rtpsink->force_ipv4,
      "host", rtpsink->uri->host, "port", rtpsink->uri->port + 1,
      "close-socket", FALSE, "auto-multicast", FALSE, NULL);

  if (gst_rtp_sink_is_multicast (rtpsink->uri->host)) {
    uri = g_strdup_printf ("udp://%s:%u",
        rtpsink->uri->host, rtpsink->uri->port + 1);
    g_object_set (G_OBJECT (rtpsink->rtcp_src), "uri", uri, NULL);
    g_free (uri);
  } else
    g_object_set (G_OBJECT (rtpsink->rtcp_src), "port", rtpsink->uri->port + 1,
        NULL);

  caps = gst_caps_from_string ("application/x-rtcp");
  g_object_set (G_OBJECT (rtpsink->rtcp_src),
      "caps", caps, "auto-multicast", TRUE, NULL);
  gst_caps_unref (caps);

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (rtpbin),
          "use-pipeline-clock")) {
    g_object_set (G_OBJECT (rtpbin), "use-pipeline-clock", TRUE, NULL);
  } else {
    GST_WARNING_OBJECT (rtpsink,
        "rtpbin has no use-pipeline-clock, running old version?");
  }

  GST_DEBUG_OBJECT (rtpsink, "Connecting callbacks");
  g_signal_connect (rtpbin, "pad-added",
      G_CALLBACK (gst_rtp_sink_rtpbin_pad_added_cb), rtpsink);
  g_signal_connect (rtpbin, "element-added",
      G_CALLBACK (gst_rtp_sink_rtpsink_element_added), NULL);

  gst_bin_add_many (GST_BIN (rtpsink), rtpbin,
      rtpsink->rtp_sink, rtpsink->rtcp_sink, rtpsink->rtcp_src, NULL);

  if (rtpsink->encrypt) {
    gst_bin_add (GST_BIN (rtpsink), rtpsink->rtpencrypt);
    gst_element_link (rtpsink->rtpencrypt, rtpsink->rtp_sink);
  }

  GST_DEBUG_OBJECT (rtpbin, "Connecting pads");
  pad = gst_element_get_request_pad (rtpbin, "send_rtp_sink_0");
  gst_element_link_pads (rtpbin, "send_rtp_src_0", rtpsink->rtp_sink, "sink");
  gst_element_link_pads (rtpbin, "send_rtcp_src_0", rtpsink->rtcp_sink, "sink");
  gst_element_link_pads (rtpsink->rtcp_src, "src", rtpbin, "recv_rtcp_sink_0");
  gst_pad_set_event_function (pad,
      (GstPadEventFunction) gst_rtp_sink_rtp_bin_event);

  GST_DEBUG_OBJECT (rtpsink, "Setting and connecting ghostpad");
  gst_ghost_pad_set_target (GST_GHOST_PAD (rtpsink->sinkpad), pad);
  gst_object_unref (pad);

  gst_element_sync_state_with_parent (GST_ELEMENT (rtpbin));
  if (rtpsink->encrypt)
    gst_element_sync_state_with_parent (rtpsink->rtpencrypt);
  gst_element_sync_state_with_parent (rtpsink->rtp_sink);

  /** The order of these lines is really important **/
  /* First we update the state of rtcp_src so that it creates a socket and
   * binds on the port rtpsink->uri->port + 1 */
  gst_element_sync_state_with_parent (rtpsink->rtcp_src);
  /* Now we can retrieve rtcp_src socket and set it for rtcp_sink element */
  gst_rtp_sink_retrieve_rtcp_src_socket (rtpsink);
  g_object_set (G_OBJECT (rtpsink->rtcp_sink), "socket",
      rtpsink->rtcp_src_socket, NULL);
  /* And we sync the state of rtcp_sink */
  gst_element_sync_state_with_parent (rtpsink->rtcp_sink);

  return TRUE;
}

static GstStateChangeReturn
gst_rtp_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpSink *rtpsink = GST_RTP_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (rtpsink, "Configuring rtpsink");
      if (!gst_rtp_sink_start (rtpsink)) {
        GST_DEBUG_OBJECT (rtpsink, "Start failed");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      GST_DEBUG_OBJECT (rtpsink, "Shutting down");
    }
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_rtp_sink_finalize (GObject * gobject)
{
  GstRtpSink *rtpsink = GST_RTP_SINK (gobject);

  if (rtpsink->uri)
    soup_uri_free (rtpsink->uri);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_rtp_sink_init (GstRtpSink * rtpsink)
{
  rtpsink->uri = soup_uri_new (DEFAULT_PROP_URI);
  rtpsink->ttl = DEFAULT_PROP_TTL;
  rtpsink->ttl_mc = DEFAULT_PROP_TTL_MC;
  rtpsink->src_port = DEFAULT_SRC_PORT;
  rtpsink->force_ipv4 = DEFAULT_PROP_FORCE_IPV4;
  rtpsink->rtp_sink_socket = NULL;
  rtpsink->rtcp_src_socket = NULL;
  rtpsink->encrypt = DEFAULT_PROP_ENCRYPT;
  rtpsink->key_derivation_rate = DEFAULT_PROP_KEY_DERIV_RATE;

  rtpsink->sinkpad = gst_ghost_pad_new_no_target ("sink", GST_PAD_SINK);
  gst_element_add_pad (GST_ELEMENT (rtpsink), rtpsink->sinkpad);

  GST_OBJECT_FLAG_SET (GST_OBJECT (rtpsink), GST_ELEMENT_FLAG_SINK);
  GST_DEBUG_OBJECT (rtpsink, "rtpsink initialised");
}

gboolean
rtp_sink_init (GstPlugin * plugin)
{
  return gst_element_register (plugin,
      "rtpsink", GST_RANK_NONE, GST_TYPE_RTP_SINK);
}

static guint
gst_rtp_sink_uri_get_type (GType type)
{
  return GST_URI_SINK;
}

static const gchar *const *
gst_rtp_sink_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { (char *) "rtp", NULL };

  return protocols;
}

static gchar *
gst_rtp_sink_uri_get_uri (GstURIHandler * handler)
{
  GstRtpSink *rtpsink = (GstRtpSink *) handler;

  rtpsink->last_uri = soup_uri_to_string (rtpsink->uri, FALSE);

  return rtpsink->last_uri;
}

static gboolean
gst_rtp_sink_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstRtpSink *rtpsink = (GstRtpSink *) handler;

  g_object_set (G_OBJECT (rtpsink), "uri", uri, NULL);

  return TRUE;
}

static void
gst_rtp_sink_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtp_sink_uri_get_type;
  iface->get_protocols = gst_rtp_sink_uri_get_protocols;
  iface->get_uri = gst_rtp_sink_uri_get_uri;
  iface->set_uri = gst_rtp_sink_uri_set_uri;
}

static gboolean
gst_rtp_sink_is_multicast (const gchar * ip_addr)
{
  in_addr_t host;
  struct in6_addr host6;

  if ((inet_pton (AF_INET6, ip_addr, &host6) == 1 &&
          IN6_IS_ADDR_MULTICAST (host6.__in6_u.__u6_addr8)) ||
      (inet_pton (AF_INET, ip_addr, &host) == 1 &&
          (host = ntohl (host)) && IN_MULTICAST (host)))
    return TRUE;

  return FALSE;
}
