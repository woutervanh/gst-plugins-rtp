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
  PROP_ENCRYPT,
  PROP_KEY_DERIV_RATE,
  PROP_LAST
};

#define DEFAULT_PROP_URI    		      "rtp://0.0.0.0:5004"
#define DEFAULT_PROP_MUXER  		      NULL
#define DEFAULT_PROP_TTL              (64)
#define DEFAULT_PROP_TTL_MC           (8)
#define DEFAULT_SRC_PORT              (0)
#define LOCAL_ADDRESS_IPV4			      "0.0.0.0" /* "127.0.0.1" */
#define LOCAL_ADDRESS_IPV6			      "::"      /* "::1" */
#define DEFAULT_PROP_ENCRYPT          (FALSE)

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

  g_object_class_install_property (oclass, PROP_NPADS,
      g_param_spec_uint ("n-pads", "Number of sink pads",
          "Read the number of sink pads", 0, G_MAXUINT, 0, G_PARAM_READABLE));

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
gst_rtp_sink_create_and_bind_rtp_socket (GstRtpSink * self)
{
  GError *err = NULL;
  GInetAddress *bind_addr;
  GSocketAddress *bind_saddr;

  if (0 == self->src_port) {
    GST_INFO_OBJECT (self, "src-port is set to 0 => Don't bind");
    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Create rtp_sink socket");

  /* create sender sockets try IPV6, fall back to IPV4 */
  if ((self->rtp_sink_socket =
          g_socket_new (G_SOCKET_FAMILY_IPV6,
              G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &err)) == NULL) {
    if ((self->rtp_sink_socket = g_socket_new (G_SOCKET_FAMILY_IPV4,
                G_SOCKET_TYPE_DATAGRAM, G_SOCKET_PROTOCOL_UDP, &err)) == NULL)
      goto no_socket;
  }

  /* Allow broadcast on sockets */
  g_socket_set_broadcast (self->rtp_sink_socket, TRUE);

  GST_DEBUG_OBJECT (self, "Bind rtp_sink socket on port %d",
      self->src_port);

  /*  */
  if (G_SOCKET_FAMILY_IPV6 == g_socket_get_family (self->rtp_sink_socket))
    bind_addr = g_inet_address_new_from_string (LOCAL_ADDRESS_IPV6);
  else
    bind_addr = g_inet_address_new_from_string (LOCAL_ADDRESS_IPV4);

  bind_saddr = g_inet_socket_address_new (bind_addr, self->src_port);

  if (!g_socket_bind (self->rtp_sink_socket, bind_saddr, TRUE, &err))
    goto no_bind;

  GST_DEBUG_OBJECT (self->rtp_sink,
      "RTP UDP sink has sock %p which binds on port %d",
      self->rtp_sink_socket, self->src_port);

  return TRUE;

  /* ERRORS */
no_socket:
  {
    GST_ELEMENT_ERROR ((GstElement *) self, RESOURCE, FAILED, (NULL),
        ("Could not create sockets: %s", err->message));
    g_clear_error (&err);
    return FALSE;
  }
no_bind:
  {
    GST_ELEMENT_WARNING (self, RESOURCE, SETTINGS, (NULL),
        ("Could not bind socket: %s (%d)", err->message, err->code));
    g_clear_error (&err);
    g_object_unref (self->rtp_sink_socket);
    return FALSE;
  }
}

static void
gst_rtp_sink_retrieve_rtcp_src_socket (GstRtpSink * self)
{
  if (NULL == self->rtcp_src_socket) {
    g_object_get (G_OBJECT (self->rtcp_src), "used-socket",
        &(self->rtcp_src_socket), NULL);
    if (!G_IS_SOCKET (self->rtcp_src_socket))
      GST_WARNING_OBJECT (self, "No valid socket retrieved from udpsrc");
    else
      GST_DEBUG_OBJECT (self->rtcp_sink, "RTCP UDP src has sock %p",
          self->rtcp_src_socket);
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
gst_rtp_sink_check_uri (GstRtpSink * self)
{
  if (self->uri) {
    gst_barco_parse_uri (G_OBJECT (self), self->uri, GST_CAT_DEFAULT);
  }
}

static void
gst_rtp_sink_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSink *self = GST_RTP_SINK (object);

  switch (prop_id) {
    case PROP_URI:
      if (self->uri)
        gst_uri_unref (self->uri);
      self->uri = gst_uri_from_string (g_value_get_string (value));
      gst_rtp_sink_check_uri (self);
      break;
    case PROP_TTL:
      self->ttl = g_value_get_int (value);
      break;
    case PROP_TTL_MC:
      self->ttl_mc = g_value_get_int (value);
      break;
    case PROP_SRC_PORT:
      self->src_port = g_value_get_int (value);
      close_and_unref_socket (self->rtp_sink_socket);
      gst_rtp_sink_create_and_bind_rtp_socket (self);
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
  GstRtpSink *self = GST_RTP_SINK (object);

  switch (prop_id) {
    case PROP_URI:
      if (self->uri)
        g_value_take_string (value, gst_uri_to_string (self->uri));
      else
        g_value_set_string (value, NULL);
      break;
    case PROP_TTL:
      g_value_set_int (value, self->ttl);
      break;
    case PROP_TTL_MC:
      g_value_set_int (value, self->ttl_mc);
      break;
    case PROP_SRC_PORT:
      g_value_set_int (value, self->src_port);
      break;
    case PROP_ENCRYPT:
      g_value_set_boolean (value, self->encrypt);
      break;
    case PROP_KEY_DERIV_RATE:
      g_value_set_uint (value, self->key_derivation_rate);
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
  GstRtpSink *self = GST_RTP_SINK (data);
  GstElement *elt;
  GstPad *target;
  GstCaps *caps;
  gchar *name;

  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK) {
    GST_DEBUG_OBJECT (self, "not interested in sink pads");
    caps = gst_pad_query_caps (pad, NULL);
    GST_DEBUG_OBJECT (self, "caps are %" GST_PTR_FORMAT, caps);
    gst_caps_unref (caps);
    return;
  }

  caps = gst_pad_query_caps (pad, NULL);
  if (caps) {
    GstCaps *rtcp_caps = gst_caps_new_empty_simple ("application/x-rtcp");

    if (gst_caps_can_intersect (caps, rtcp_caps)) {
      GST_DEBUG_OBJECT (self, "not interested in pad with rtcp caps");
      gst_caps_unref (rtcp_caps);
      gst_caps_unref (caps);
      return;
    }
    gst_caps_unref (rtcp_caps);
    gst_caps_unref (caps);
  }

  elt = self->rtp_sink;

  target = gst_element_get_static_pad (elt, "sink");
  if (gst_pad_is_linked (target)) {
    GST_WARNING_OBJECT (self,
        "new pad on rtpbin, but there was already a src pad");
    gst_caps_unref (caps);
    gst_object_unref (target);
    return;
  }

  name = gst_element_get_name (elt);
  GST_DEBUG_OBJECT (self, "linking new rtpbin src pad to %s sink pad", name);
  g_free (name);
  gst_pad_link (pad, target);
  gst_object_unref (target);
  gst_caps_unref (caps);
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
    GST_DEBUG_OBJECT(self, "element supports " property); \
    g_object_set (G_OBJECT(new_element), property, value, NULL); \
  }

static void
gst_rtp_sink_rtpsink_element_added (GstRtpSink * self,
    GstElement * new_element, gpointer data)
{
  g_return_if_fail (new_element != NULL);

  /* set minimal interval on 0.5s */
  gst_rtp_sink_find_property ("rtcp-min-interval", (guint64) 500000000);
}

static gboolean
gst_rtp_sink_start (GstRtpSink * self)
{
  GstElement *rtpbin;
  GstPad *pad;
  GstCaps *caps = NULL;
  gchar *uri = NULL;

  GST_DEBUG_OBJECT (self, "Creating correct modules");
  rtpbin = gst_element_factory_make ("rtpbin", NULL);
  self->rtp_sink = gst_element_factory_make ("udpsink", NULL);
  self->rtcp_sink = gst_element_factory_make ("udpsink", NULL);
  self->rtcp_src = gst_element_factory_make ("udpsrc", NULL);

  g_return_val_if_fail (rtpbin != NULL, FALSE);
  g_return_val_if_fail (self->rtp_sink != NULL, FALSE);
  g_return_val_if_fail (self->rtcp_sink != NULL, FALSE);
  g_return_val_if_fail (self->rtcp_src != NULL, FALSE);

  /* Set properties */
  g_object_set (G_OBJECT (self->rtp_sink),
      "async", FALSE,
      "ttl", self->ttl,
      "ttl-mc", self->ttl_mc,
      "host", gst_uri_get_host(self->uri), "port", gst_uri_get_port(self->uri),
      "socket", self->rtp_sink_socket, "auto-multicast", TRUE, NULL);

  /* auto-multicast should be set to false as rtcp_src will already
   * join the multicast group */
  g_object_set (G_OBJECT (self->rtcp_sink),
      "sync", FALSE,
      "async", FALSE,
      "ttl", self->ttl,
      "ttl-mc", self->ttl_mc,
      "host", gst_uri_get_host(self->uri), "port", gst_uri_get_port(self->uri) + 1,
      "close-socket", FALSE, "auto-multicast", FALSE, NULL);

  if (gst_rtp_sink_is_multicast (gst_uri_get_host(self->uri))) {
    uri = g_strdup_printf ("udp://%s:%u",
        gst_uri_get_host(self->uri), gst_uri_get_port(self->uri) + 1);
    g_object_set (G_OBJECT (self->rtcp_src), "uri", uri, NULL);
    g_free (uri);
  } else
    g_object_set (G_OBJECT (self->rtcp_src), "port", gst_uri_get_port(self->uri) + 1,
        NULL);

  caps = gst_caps_from_string ("application/x-rtcp");
  g_object_set (G_OBJECT (self->rtcp_src),
      "caps", caps, "auto-multicast", TRUE, NULL);
  gst_caps_unref (caps);

  if (g_object_class_find_property (G_OBJECT_GET_CLASS (rtpbin),
          "use-pipeline-clock")) {
    g_object_set (G_OBJECT (rtpbin), "use-pipeline-clock", TRUE, NULL);
  } else {
    GST_WARNING_OBJECT (self,
        "rtpbin has no use-pipeline-clock, running old version?");
  }

  GST_DEBUG_OBJECT (self, "Connecting callbacks");
  g_signal_connect (rtpbin, "pad-added",
      G_CALLBACK (gst_rtp_sink_rtpbin_pad_added_cb), self);
  g_signal_connect (rtpbin, "element-added",
      G_CALLBACK (gst_rtp_sink_rtpsink_element_added), NULL);

  gst_bin_add_many (GST_BIN (self), rtpbin,
      self->rtp_sink, self->rtcp_sink, self->rtcp_src, NULL);

  GST_DEBUG_OBJECT (rtpbin, "Connecting pads");
  pad = gst_element_get_request_pad (rtpbin, "send_rtp_sink_0");
  gst_element_link_pads (rtpbin, "send_rtp_src_0", self->rtp_sink, "sink");
  gst_element_link_pads (rtpbin, "send_rtcp_src_0", self->rtcp_sink, "sink");
  gst_element_link_pads (self->rtcp_src, "src", rtpbin, "recv_rtcp_sink_0");
  gst_pad_set_event_function (pad,
      (GstPadEventFunction) gst_rtp_sink_rtp_bin_event);

  gst_element_sync_state_with_parent (GST_ELEMENT (rtpbin));
  gst_element_sync_state_with_parent (self->rtp_sink);

  /** The order of these lines is really important **/
  /* First we update the state of rtcp_src so that it creates a socket and
   * binds on the port gst_uri_get_port(self->uri) + 1 */
  if (!gst_element_sync_state_with_parent (self->rtcp_src))
    GST_ERROR_OBJECT (self, "Could not set RTCP source to playing");
  /* Now we can retrieve rtcp_src socket and set it for rtcp_sink element */
  gst_rtp_sink_retrieve_rtcp_src_socket (self);
  g_object_set (G_OBJECT (self->rtcp_sink), "socket",
      self->rtcp_src_socket, NULL);
  /* And we sync the state of rtcp_sink */
  if (!gst_element_sync_state_with_parent (self->rtcp_sink))
    GST_ERROR_OBJECT (self, "Could not set RTCP sink to playing");

  return TRUE;
}

static GstStateChangeReturn
gst_rtp_sink_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpSink *self = GST_RTP_SINK (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (self, "Configuring rtpsink");
      if (!gst_rtp_sink_start (self)) {
        GST_DEBUG_OBJECT (self, "Start failed");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      GST_DEBUG_OBJECT (self, "Shutting down");
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
  GstRtpSink *self = GST_RTP_SINK (gobject);

  if (self->uri)
    gst_uri_unref (self->uri);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_rtp_sink_init (GstRtpSink * self)
{
  self->npads = 0;
  self->uri = gst_uri_from_string(DEFAULT_PROP_URI);
  self->ttl = DEFAULT_PROP_TTL;
  self->ttl_mc = DEFAULT_PROP_TTL_MC;
  self->src_port = DEFAULT_SRC_PORT;
  self->rtp_sink_socket = NULL;
  self->rtcp_src_socket = NULL;

  GST_OBJECT_FLAG_SET (GST_OBJECT (self), GST_ELEMENT_FLAG_SINK);
  GST_DEBUG_OBJECT (self, "rtpsink initialised");
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
  GstRtpSink *self = (GstRtpSink *) handler;

  self->last_uri = gst_uri_to_string (self->uri);

  return self->last_uri;
}

static gboolean
gst_rtp_sink_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstRtpSink *self = (GstRtpSink *) handler;

  g_object_set (G_OBJECT (self), "uri", uri, NULL);

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
