/* ex: set tabstop=2 shiftwidth=2 expandtab: */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/net/gstnetaddressmeta.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include "gstrtpsrc.h"
#include "gstrtpparameters.h"
#include "gstbarcomgs_common.h"

GST_DEBUG_CATEGORY_STATIC (rtp_src_debug);
#define GST_CAT_DEFAULT rtp_src_debug

enum
{
  PROP_0,
  PROP_URI,
  PROP_ENCODING_NAME,
  PROP_ENABLE_RTCP,
  PROP_LATENCY,
  PROP_PT_CHANGE,
  PROP_PT_SELECT,
  PROP_SSRC_CHANGE,
  PROP_SSRC_SELECT,
  PROP_MULTICAST_IFACE,
  PROP_BUFFER_SIZE,
  PROP_ENCRYPT,
  PROP_KEY_DERIV_RATE,
  PROP_TIMEOUT,
  PROP_LAST
};

#define DEFAULT_PROP_URI          		(NULL)
#define DEFAULT_PROP_MUXER        		(NULL)
#define DEFAULT_LATENCY_MS        		(200)
#define DEFAULT_BUFFER_SIZE       		(0)
#define DEFAULT_ENABLE_RTCP	      		(TRUE)
#define DEFAULT_PROP_MULTICAST_IFACE 	(NULL)
#define DEFAULT_PROP_ENCRYPT          (FALSE)
#define DEFAULT_PROP_KEY_DERIV_RATE   (0)
#define DEFAULT_PROP_TIMEOUT          (0)

/* 0 size means just pass the buffer along */
#define GST_RTPPTCHANGE_DEFAULT_PT_NUMBER (0)
#define GST_RTPPTCHANGE_DEFAULT_PT_SELECT (0)
#define GST_RTPPTCHANGE_MIN_PT_NUMBER (0)
#define GST_RTPPTCHANGE_MAX_PT_NUMBER (G_MAXINT8)

/* 0 size means just pass the buffer along */
#define GST_RTPPTCHANGE_DEFAULT_SSRC_NUMBER (0)
#define GST_RTPPTCHANGE_DEFAULT_SSRC_SELECT (0)
#define GST_RTPPTCHANGE_MIN_SSRC_NUMBER (0)
#define GST_RTPPTCHANGE_MAX_SSRC_NUMBER (G_MAXUINT32)


static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src%d",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS ("application/x-rtp"));

static void gst_rtp_src_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

#define gst_rtp_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSrc, gst_rtp_src, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER, gst_rtp_src_uri_handler_init));

static void gst_rtp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_rtp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_rtp_src_finalize (GObject * gobject);
static GstCaps *gst_rtp_src_request_pt_map (GstElement * sess, guint sess_id,
    guint pt, GstRtpSrc * rtpsrc);
static GstStateChangeReturn
gst_rtp_src_change_state (GstElement * element, GstStateChange transition);
static gboolean gst_rtp_src_is_multicast (const gchar * ip_addr);
GSocket *gst_rtp_src_retrieve_rtcpsrc_socket (GstRtpSrc * self);
static void
gst_rtp_src_class_init (GstRtpSrcClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  oclass->set_property = gst_rtp_src_set_property;
  oclass->get_property = gst_rtp_src_get_property;
  oclass->finalize = gst_rtp_src_finalize;

  /**
   * GstRtpSrc::uri
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
   * GstRtpSrc::encoding-name
   *
   * Specify the encoding name of the stream, typically when no SDP is
   * obtained. This steers auto plugging and avoids wrong detection.
   *
   * Since: 0.10.5
   */
  g_object_class_install_property (oclass, PROP_ENCODING_NAME,
      g_param_spec_string ("encoding-name", "Encoding name",
          "force encoding-name on depayloader", DEFAULT_PROP_URI,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc::latency
   *
   * Default latency is 50, for MPEG4 large GOP sizes, this needs to be
   * increased
   *
   * Since: 0.10.5
   */
  g_object_class_install_property (oclass, PROP_LATENCY,
      g_param_spec_uint ("latency", "Buffer latency in ms",
          "Default amount of ms to buffer in the jitterbuffers", 0, G_MAXUINT,
          DEFAULT_LATENCY_MS, G_PARAM_READWRITE));

  /**
   * GstRtpSrc::enable-rtcp
   *
   * Enable RTCP (Real Time Control Protocol)
   *
   * Since: 0.10.5
   */
  g_object_class_install_property (oclass, PROP_ENABLE_RTCP,
      g_param_spec_boolean ("enable-rtcp", "Enable RTCP",
          "Enable RTCP feedback in RTP", DEFAULT_ENABLE_RTCP,
          G_PARAM_READWRITE));

  /**
   * GstRtpSrc::multicast-iface
   *
   * On machines with multiple interfaces, lock the socket to the
   * interface instead of the routing.
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass, PROP_MULTICAST_IFACE,
      g_param_spec_string ("multicast-iface", "Multicast Interface",
          "Multicast Interface", DEFAULT_PROP_MULTICAST_IFACE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc::buffer-size
   *
   * Size of the kernel receive buffer in bytes
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass, PROP_BUFFER_SIZE,
      g_param_spec_uint ("buffer-size", "Kernel receive buffer size",
          "Size of the kernel receive buffer in bytes, 0=default", 0, G_MAXUINT,
          DEFAULT_LATENCY_MS, G_PARAM_READWRITE));

  /**
   * GstRtpSrc::timeout
   *
   * UDP timeout value
   *
   * Since: 0.10.5
   */
  g_object_class_install_property (oclass, PROP_TIMEOUT,
      g_param_spec_uint64 ("timeout", "Timeout",
          "Post a message after timeout microseconds (0 = disabled)", 0,
          G_MAXUINT64, DEFAULT_PROP_TIMEOUT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSrc::encrypt
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
   * GstRtpSrc::rate
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

  /**
   * GstRtpSrc::pt-change
   *
   * Change the payload type value in the packet
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass,
      PROP_PT_CHANGE,
      g_param_spec_uint ("pt-change",
          "Payload type to change to",
          "Payload type with which to overwrite a previous value (0 = disabled)",
          GST_RTPPTCHANGE_MIN_PT_NUMBER,
          GST_RTPPTCHANGE_MAX_PT_NUMBER,
          GST_RTPPTCHANGE_DEFAULT_PT_NUMBER, G_PARAM_READWRITE));

  /**
   * GstRtpSrc::pt-select
   *
   * Select based on the payload type value in the packet
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass,
      PROP_PT_SELECT,
      g_param_spec_uint ("pt-select",
          "Payload type to select",
          "Payload type to select, others are dropped (0 = disabled)",
          GST_RTPPTCHANGE_MIN_PT_NUMBER,
          GST_RTPPTCHANGE_MAX_PT_NUMBER,
          GST_RTPPTCHANGE_DEFAULT_PT_SELECT, G_PARAM_READWRITE));

  /**
   * GstRtpSrc::ssrc-change
   *
   * Change the SSRC value in the packet
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass,
      PROP_SSRC_CHANGE,
      g_param_spec_uint ("ssrc-change",
          "Payload type to change to",
          "Payload type with which to overwrite a previous value (0 = disabled)",
          GST_RTPPTCHANGE_MIN_SSRC_NUMBER,
          GST_RTPPTCHANGE_MAX_SSRC_NUMBER,
          GST_RTPPTCHANGE_DEFAULT_SSRC_NUMBER, G_PARAM_READWRITE));

  /**
   * GstRtpSrc::ssrc-select
   *
   * Select based on the SSRC value in the packet
   *
   * Since: 1.0.0
   */
  g_object_class_install_property (oclass,
      PROP_SSRC_SELECT,
      g_param_spec_uint ("ssrc-select",
          "Payload type to select",
          "Payload type to select, others are dropped (0 = disabled)",
          GST_RTPPTCHANGE_MIN_SSRC_NUMBER,
          GST_RTPPTCHANGE_MAX_SSRC_NUMBER,
          GST_RTPPTCHANGE_DEFAULT_SSRC_SELECT, G_PARAM_READWRITE));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  gstelement_class->change_state = GST_DEBUG_FUNCPTR (gst_rtp_src_change_state);

  gst_element_class_set_static_metadata (gstelement_class,
      "rtpsrc",
      "Generic/Bin/Src",
      "Barco Rtp src",
      "Thijs Vermeir <thijs.vermeir@barco.com>, "
      "Marc Leeman <marc.leeman@barco.com>, "
      "Paul Henrys <paul.henrys'daubigny@barco.com>");

  GST_DEBUG_CATEGORY_INIT (rtp_src_debug,
      "barcortpsrc", 0, "Barco rtp send bin");
}

GSocket *
gst_rtp_src_retrieve_rtcpsrc_socket (GstRtpSrc * self)
{
  GSocket *rtcpfd;

  g_object_get (G_OBJECT (self->rtcp_src), "used-socket", &rtcpfd, NULL);

  if (!G_IS_SOCKET (rtcpfd))
    GST_WARNING_OBJECT (self, "No valid socket retrieved from udpsrc");
  else
    GST_DEBUG_OBJECT (self, "RTP UDP source has sock %p", rtcpfd);

  return rtcpfd;
}

static void
gst_rtp_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstRtpSrc *self = GST_RTP_SRC (object);
  GstCaps *caps = NULL;

  switch (prop_id) {
    case PROP_URI:{
      gchar *uri = NULL;
      if (self->uri)
        soup_uri_free (self->uri);
      self->uri = soup_uri_new (g_value_get_string (value));

      gst_barco_parse_uri (G_OBJECT (self), self->uri, GST_CAT_DEFAULT);
      if (self->rtp_src) {
        uri = g_strdup_printf ("udp://%s:%d", self->uri->host, self->uri->port);
        g_object_set (G_OBJECT (self->rtp_src), "uri", uri, NULL);
        g_free (uri);
      }
      if (self->enable_rtcp && self->rtcp_src) {
        if (gst_rtp_src_is_multicast (self->uri->host)) {
          uri =
              g_strdup_printf ("udp://%s:%d", self->uri->host,
              self->uri->port + 1);
          g_object_set (G_OBJECT (self->rtcp_src), "uri", uri, NULL);
          g_free (uri);
        } else {
          g_object_set (G_OBJECT (self->rtcp_src),
              "port", self->uri->port + 1, NULL);
        }
        g_object_set (G_OBJECT (self->rtcp_src), "closefd", FALSE, NULL);
      }
    }
      break;
    case PROP_ENCODING_NAME:
      if (self->encoding_name)
        g_free (self->encoding_name);
      self->encoding_name = g_value_dup_string (value);
      GST_INFO_OBJECT (self,
          "Force encoding name (%s), do you know what you are doing?",
          self->encoding_name);
      if (self->rtp_src) {
        GST_INFO_OBJECT (self, "Requesting PT map");
        caps = gst_rtp_src_request_pt_map (NULL, 0, 96, self);
        g_object_set (G_OBJECT (self->rtp_src), "caps", caps, NULL);
        gst_caps_unref (caps);
      }
      break;
    case PROP_LATENCY:
      self->latency = g_value_get_uint (value);
      if (self->rtpbin)
        g_object_set (G_OBJECT (self->rtpbin), "latency", self->latency, NULL);
      break;
    case PROP_ENABLE_RTCP:
      self->enable_rtcp = g_value_get_boolean (value);
      GST_DEBUG_OBJECT (self, "set enable-rtcp: %d", self->enable_rtcp);
      break;
    case PROP_MULTICAST_IFACE:
      if (self->multicast_iface)
        g_free (self->multicast_iface);
      self->multicast_iface = g_value_dup_string (value);
      GST_DEBUG_OBJECT (self, "set multicast-iface: %s", self->multicast_iface);
      xgst_barco_propagate_setting (self, "multicast-iface",
          self->multicast_iface);
      break;
    case PROP_BUFFER_SIZE:
      self->buffer_size = g_value_get_uint (value);
      GST_DEBUG_OBJECT (self, "set buffer-size: %u", self->buffer_size);
      xgst_barco_propagate_setting (self, "buffer-size", self->buffer_size);
      break;
    case PROP_ENCRYPT:
      self->encrypt = g_value_get_boolean (value);
      GST_DEBUG_OBJECT (self, "set encrypt: %d", self->encrypt);
      break;
    case PROP_KEY_DERIV_RATE:
      self->key_derivation_rate = g_value_get_uint (value);
      if (self->rtpdecrypt)
        g_object_set (G_OBJECT (self->rtpdecrypt), "rate",
            self->key_derivation_rate, NULL);
      break;
    case PROP_TIMEOUT:
      self->timeout = g_value_get_uint64 (value);
      xgst_barco_propagate_setting (self, "timeout", self->timeout);
      break;
    case PROP_PT_CHANGE:
      self->pt_change = g_value_get_uint (value);
      if (self->rtpheaderchange)
        g_object_set (G_OBJECT (self->rtpheaderchange), "pt-change",
            self->pt_change, NULL);
      GST_DEBUG_OBJECT (self, "set pt change: %u", self->pt_change);
      break;
    case PROP_PT_SELECT:
      self->pt_select = g_value_get_uint (value);
      if (self->rtpheaderchange)
        g_object_set (G_OBJECT (self->rtpheaderchange), "pt-select",
            self->pt_select, NULL);
      GST_DEBUG_OBJECT (self, "set pt select: %u", self->pt_select);
      break;
    case PROP_SSRC_CHANGE:
      self->ssrc_change = g_value_get_uint (value);
      if (self->rtpheaderchange)
        g_object_set (G_OBJECT (self->rtpheaderchange), "ssrc-change",
            self->ssrc_change, NULL);
      GST_DEBUG_OBJECT (self, "set ssrc change: %u", self->ssrc_change);
      break;
    case PROP_SSRC_SELECT:
      self->ssrc_select = g_value_get_uint (value);
      if (self->rtpheaderchange)
        g_object_set (G_OBJECT (self->rtpheaderchange), "ssrc-select",
            self->ssrc_select, NULL);
      GST_DEBUG_OBJECT (self, "set ssrc select: %u", self->ssrc_select);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstRtpSrc *self = GST_RTP_SRC (object);

  switch (prop_id) {
    case PROP_URI:
      if (self->uri) {
        gchar *string = soup_uri_to_string (self->uri, FALSE);

        g_value_set_string (value, string);
        g_free (string);
      } else {
        g_value_set_string (value, NULL);
      }
      break;
    case PROP_ENCODING_NAME:
      g_value_set_string (value, self->encoding_name);
      break;
      break;
    case PROP_LATENCY:
      g_value_set_uint (value, self->latency);
      break;
    case PROP_ENABLE_RTCP:
      g_value_set_boolean (value, self->enable_rtcp);
      break;
    case PROP_MULTICAST_IFACE:
      g_value_set_string (value, self->multicast_iface);
      break;
    case PROP_BUFFER_SIZE:
      g_value_set_uint (value, self->buffer_size);
      break;
    case PROP_ENCRYPT:
      g_value_set_boolean (value, self->encrypt);
      break;
    case PROP_KEY_DERIV_RATE:
      g_value_set_uint (value, self->key_derivation_rate);
      break;
    case PROP_TIMEOUT:
      g_value_set_uint64 (value, self->timeout);
      break;
    case PROP_PT_CHANGE:
      g_value_set_uint (value, self->pt_change);
      break;
    case PROP_PT_SELECT:
      g_value_set_uint (value, self->pt_select);
      break;
    case PROP_SSRC_CHANGE:
      g_value_set_uint (value, self->ssrc_change);
      break;
    case PROP_SSRC_SELECT:
      g_value_set_uint (value, self->ssrc_select);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_src_rtpbin_pad_added_cb (GstElement * element,
    GstPad * pad, gpointer data)
{
  GstCaps *caps;
  gchar *name;
  GstRtpSrc *self = GST_RTP_SRC (data);
  GstStructure *s = NULL;

  caps = gst_pad_query_caps (pad, NULL);

  name = gst_pad_get_name (pad);
  GST_DEBUG_OBJECT (self, "Adding a pad %s with caps %" GST_PTR_FORMAT, name,
      caps);
  g_free (name);

  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK) {
    GST_DEBUG_OBJECT (element, "Ignoring sink pad.");
    gst_caps_unref (caps);
    return;
  }

  if (G_LIKELY (caps)) {
    GstCaps *rtcp_caps = gst_caps_new_empty_simple ("application/x-rtcp");

    if (gst_caps_can_intersect (caps, rtcp_caps)) {
      GST_DEBUG_OBJECT (self, "Ignoring RTCP pad.");
      gst_caps_unref (rtcp_caps);
      gst_caps_unref (caps);
      return;
    }

    gst_caps_unref (rtcp_caps);

    if (G_UNLIKELY (self->ssrc_select > 0)) {
      rtcp_caps = gst_caps_new_simple ("application/x-rtp",
          "payload", G_TYPE_INT, self->ssrc_select, NULL);
      if (G_UNLIKELY (!gst_caps_can_intersect (caps, rtcp_caps))) {
        s = gst_caps_get_structure (caps, 0);

        /* This should not happened as rtpheaderchange drops buffers with wrong SSRC */
        GST_ERROR_OBJECT (self,
            "Received SSRC %d whereas ssrc-select equals %d.",
            g_value_get_int (gst_structure_get_value (s, "ssrc")),
            self->ssrc_select);

        gst_caps_unref (rtcp_caps);
        gst_caps_unref (caps);

        return;
      }
      gst_caps_unref (rtcp_caps);
    }

    if (G_UNLIKELY (self->pt_select > 0)) {
      rtcp_caps = gst_caps_new_simple ("application/x-rtp",
          "payload", G_TYPE_INT, self->pt_select, NULL);
      if (G_UNLIKELY (!gst_caps_can_intersect (caps, rtcp_caps))) {
        s = gst_caps_get_structure (caps, 0);

        /* This should not happened as rtpheaderchange drops buffers with wrong payload */
        GST_ERROR_OBJECT (self, "Received pt %d whereas pt-select equals %d.",
            g_value_get_int (gst_structure_get_value (s, "payload")),
            self->pt_select);

        gst_caps_unref (rtcp_caps);
        gst_caps_unref (caps);

        return;
      }
      gst_caps_unref (rtcp_caps);
    }
    /*gst_caps_unref (caps); */
  } else {
    GST_WARNING_OBJECT (self, "Pad has NO caps, this is not good.");
  }

  gst_object_ref (pad);

  name = gst_pad_get_name (pad);
  GST_DEBUG_OBJECT (self, "New pad %s on rtpbin with caps %" GST_PTR_FORMAT,
      name, caps);
  g_free (name);

  if (G_UNLIKELY (self->ssrc_change)) {
    GST_DEBUG_OBJECT (self, "SSRC ignored, reconnecting on last pad.");
    if (self->n_rtpbin_pads != 1) {
      /* which one should be reconnected; cannot determine atm */
      GST_WARNING ("Ghost pads not equal to 1; can't reconnect");
    } else {
      gst_ghost_pad_set_target ((GstGhostPad *) self->ghostpad, pad);
      gst_object_unref (pad);

      return;
    }
  }

  if (G_UNLIKELY (self->pt_change)) {
    GstCaps *caps = gst_rtp_src_request_pt_map (NULL, 0, 96, self);
    GstElement *filter = gst_element_factory_make ("capsfilter", NULL);
    GstPad *sinkpad;

    GST_DEBUG_OBJECT (self,
        "PT ignored, need to set caps to caps %" GST_PTR_FORMAT, caps);
    g_object_set (G_OBJECT (filter), "caps", caps, NULL);
    gst_caps_unref (caps);

    gst_bin_add (GST_BIN (self), filter);
    gst_element_sync_state_with_parent (filter);

    sinkpad = gst_element_get_static_pad (filter, "sink");
    gst_object_ref (pad);
    gst_pad_link (pad, sinkpad);
    gst_object_unref (pad);
    gst_object_unref (sinkpad);

    pad = gst_element_get_static_pad (filter, "src");
  }

  name = g_strdup_printf ("src%d", self->n_rtpbin_pads++);
  self->ghostpad = gst_ghost_pad_new (name, pad);
  g_free (name);

  gst_pad_set_active (self->ghostpad, TRUE);
  gst_element_add_pad (GST_ELEMENT (self), self->ghostpad);

  /* FIXME: check what happens when there are multiple pads added */
  gst_element_no_more_pads (GST_ELEMENT (self));

  gst_object_unref (pad);
}

static void
gst_rtp_src_fixup_caps (GstCaps * ret, const gchar * encoding_name)
{
  /*caps="application/x-rtp, media=(string)audio, clock-rate=(int)32000, encoding-name=(string)MP4A-LATM, payload=(int)96" */
  /*caps="application/x-rtp, media=(string)audio, clock-rate=(int)32000, encoding-name=MPEG4-GENERIC,config=(string)1288,sizelength=(string)13" */
  /*caps="application/x-rtp, media=(string)audio, clock-rate=(int)48000, encoding-name=(string)L16, encoding-params=(string)2, channels=(int)2, payload=(int)96" */
  /* application/x-rtp, media=(string)audio, clock-rate=(int)48000, encoding-name=(string)MP4A-LATM" */
  if (g_strcmp0 (encoding_name, "MPEG4-GENERIC-AUDIO") == 0) {
    gst_caps_set_simple (ret,
        "media", G_TYPE_STRING, "audio",
        "clock-rate", G_TYPE_INT, 48000,
        "encoding-name", G_TYPE_STRING, "MPEG4-GENERIC",
        "mode", G_TYPE_STRING, "AAC-hbr",
        "config", G_TYPE_STRING, "1190",
        "sizelength", G_TYPE_STRING, "13", NULL);
  }
  if (g_strcmp0 (encoding_name, "L16") == 0) {
    gst_caps_set_simple (ret,
        "clock-rate", G_TYPE_INT, 48000,
        "encoding-name", G_TYPE_STRING, "L16", "channels", G_TYPE_INT, 2, NULL);
  }
  if (g_strcmp0 (encoding_name, "MP4A-LATM") == 0) {
    gst_caps_set_simple (ret,
        "media", G_TYPE_STRING, "audio",
        "clock-rate", G_TYPE_INT, 48000,
        "encoding-name", G_TYPE_STRING, "MP4A-LATM",
        "channels", G_TYPE_INT, 2, NULL);
  }
  if (g_strcmp0 (encoding_name, "RAW-RGB24") == 0) {
    gst_caps_set_simple (ret,
        "media", G_TYPE_STRING, "video",
        "clock-rate", G_TYPE_INT, 90000,
        "encoding-name", G_TYPE_STRING, "RAW",
        "sampling", G_TYPE_STRING, "RGB",
        "depth", G_TYPE_INT, 24,
        "width", G_TYPE_INT, 800, "height", G_TYPE_INT, 600, NULL);
  }
}

static GstCaps *
gst_rtp_src_request_pt_map (GstElement * sess, guint sess_id, guint pt,
    GstRtpSrc * rtpsrc)
{
  const RtpParameters *p;
  GstCaps *ret = NULL;
  int i = 0;

  GST_DEBUG_OBJECT (rtpsrc, "requesting caps for pt %u in session %u", pt,
      sess_id);

  if (rtpsrc->encoding_name)
    goto dynamic;

  i = 0;
  while (RTP_STATIC_PARAMETERS[i].pt >= 0) {
    p = &(RTP_STATIC_PARAMETERS[i++]);
    if (p->pt == pt) {
      GST_DEBUG_OBJECT (rtpsrc, "found as static param: %s", p->encoding_name);
      goto beach;
    }
  }
  GST_DEBUG_OBJECT (rtpsrc, "no static parameters found");

  i = 0;
  if (rtpsrc->encoding_name == NULL) {
    GST_INFO_OBJECT (rtpsrc, "no encoding name set, assuming MP4V-ES");
    rtpsrc->encoding_name = g_strdup ("MP4V-ES");
  }

dynamic:
  while (RTP_DYNAMIC_PARAMETERS[i].pt >= 0) {
    p = &(RTP_DYNAMIC_PARAMETERS[i++]);
    if (g_strcmp0 (p->encoding_name, rtpsrc->encoding_name) == 0) {
      GST_DEBUG_OBJECT (rtpsrc, "found dynamic parameters [%s]",
          rtpsrc->encoding_name);
      goto beach;
    }
  }

  i = 0;
  /* just in case it was botched; go through the static ones too */
  while (RTP_STATIC_PARAMETERS[i].pt >= 0) {
    p = &(RTP_STATIC_PARAMETERS[i++]);
    if (g_strcmp0 (p->encoding_name, rtpsrc->encoding_name) == 0) {
      GST_DEBUG_OBJECT (rtpsrc, "found static parameters [%s]",
          rtpsrc->encoding_name);
      goto beach;
    }
  }
  i = 0;
  /* this is really desperate, some encoders claim to be a, while they
   * are being b (Bosch). */
  while (RTP_STATIC_PARAMETERS[i].pt >= 0) {
    p = &(RTP_STATIC_PARAMETERS[i++]);
    if (p->pt == pt) {
      GST_DEBUG_OBJECT (rtpsrc, "found as static param: %s", p->encoding_name);
      goto beach;
    }
  }

  GST_WARNING_OBJECT (rtpsrc,
      "no rtp parameters found for this payload type %s,... :-(",
      rtpsrc->encoding_name);
  p = NULL;
  return NULL;

beach:

  ret = gst_caps_new_simple ("application/x-rtp",
      "media", G_TYPE_STRING, p->media,
      "clock-rate", G_TYPE_INT, p->clock_rate,
      "encoding-name", G_TYPE_STRING, p->encoding_name,
      "payload", G_TYPE_INT, (p->pt) ? p->pt : pt, NULL);

  gst_rtp_src_fixup_caps (ret, p->encoding_name);
  GST_DEBUG_OBJECT (rtpsrc, "Decided on caps %" GST_PTR_FORMAT, ret);

  return ret;
}

static gboolean
gst_rtp_src_start (GstRtpSrc * rtpsrc)
{
  gchar *uri = NULL;
  GstElement *lastelt;

  /* Create elements */
  GST_DEBUG_OBJECT (rtpsrc, "Creating elements");

  rtpsrc->rtp_src = gst_element_factory_make ("udpsrc", NULL);
  g_return_val_if_fail (rtpsrc->rtp_src != NULL, FALSE);

  rtpsrc->rtpbin = gst_element_factory_make ("rtpbin", NULL);
  g_return_val_if_fail (rtpsrc->rtpbin != NULL, FALSE);

  if (rtpsrc->enable_rtcp) {
    GST_DEBUG_OBJECT (rtpsrc, "Enabling RTCP");
    rtpsrc->rtcp_src = gst_element_factory_make ("udpsrc", NULL);
    rtpsrc->rtcp_sink = gst_element_factory_make ("udpsink", NULL);
    g_return_val_if_fail (rtpsrc->rtcp_src != NULL, FALSE);
    g_return_val_if_fail (rtpsrc->rtcp_sink != NULL, FALSE);
  }

  if (G_UNLIKELY (rtpsrc->encrypt)) {
    rtpsrc->rtpdecrypt = gst_element_factory_make ("rtpdecrypt", NULL);
    g_return_val_if_fail (rtpsrc->rtpdecrypt != NULL, FALSE);
  }

  if (G_UNLIKELY (rtpsrc->pt_select > 0) ||
      G_UNLIKELY (rtpsrc->pt_change > 0) ||
      G_UNLIKELY (rtpsrc->ssrc_select > 0) ||
      G_UNLIKELY (rtpsrc->ssrc_change > 0)) {
    GST_LOG_OBJECT (rtpsrc,
        "Inserting rtpheaderchange PT (select %u, change %u), SSRC (select %u, change %u)",
        rtpsrc->pt_select, rtpsrc->pt_change, rtpsrc->ssrc_select,
        rtpsrc->ssrc_change);
    rtpsrc->rtpheaderchange =
        gst_element_factory_make ("rtpheaderchange", NULL);
    g_return_val_if_fail (rtpsrc->rtpheaderchange != NULL, FALSE);
    g_object_set (G_OBJECT (rtpsrc->rtpheaderchange),
        "pt-select", rtpsrc->pt_select,
        "pt-change", rtpsrc->pt_change,
        "ssrc-select", rtpsrc->ssrc_select,
        "ssrc-change", rtpsrc->ssrc_change, NULL);
  }

  /* Set properties */
  if (gst_rtp_src_is_multicast (rtpsrc->uri->host)) {
    uri = g_strdup_printf ("udp://%s:%d", rtpsrc->uri->host, rtpsrc->uri->port);
    g_object_set (G_OBJECT (rtpsrc->rtp_src), "uri", uri, NULL);
    g_free (uri);
  } else {
    g_object_set (G_OBJECT (rtpsrc->rtp_src), "port", rtpsrc->uri->port, NULL);
  }

  g_object_set (G_OBJECT (rtpsrc->rtp_src),
      "reuse", TRUE,
      "timeout", rtpsrc->timeout,
      "multicast-iface", rtpsrc->multicast_iface,
      "buffer-size", rtpsrc->buffer_size, "auto-multicast", TRUE, NULL);

  if (rtpsrc->enable_rtcp) {
    if (gst_rtp_src_is_multicast (rtpsrc->uri->host)) {
      uri =
          g_strdup_printf ("udp://%s:%d", rtpsrc->uri->host,
          rtpsrc->uri->port + 1);
      g_object_set (G_OBJECT (rtpsrc->rtcp_src), "uri", uri, NULL);
      g_free (uri);
    } else {
      g_object_set (G_OBJECT (rtpsrc->rtcp_src),
          "port", rtpsrc->uri->port + 1, NULL);
    }
    g_object_set (G_OBJECT (rtpsrc->rtcp_src),
        "multicast-iface", rtpsrc->multicast_iface,
        "close-socket", FALSE,
        "buffer-size", rtpsrc->buffer_size,
        "timeout", rtpsrc->timeout, "auto-multicast", TRUE, NULL);

    /* auto-multicast should be set to false as rtcp_src will already
     * join the multicast group */

    g_object_set (G_OBJECT (rtpsrc->rtcp_sink),
        "host", rtpsrc->uri->host,
        "port", rtpsrc->uri->port + 1,
        "sync", FALSE,
        "async", FALSE,
        "buffer-size", rtpsrc->buffer_size,
        "multicast-iface", rtpsrc->multicast_iface,
        "auto-multicast", FALSE, "close-socket", FALSE, NULL);
  }

  g_object_set (G_OBJECT (rtpsrc->rtpbin),
      "do-lost", TRUE,
      "autoremove", TRUE,
      "ignore-pt", rtpsrc->pt_change, "latency", rtpsrc->latency, NULL);

  if (rtpsrc->encrypt)
    g_object_set (G_OBJECT (rtpsrc->rtpdecrypt), "rate",
        rtpsrc->key_derivation_rate, NULL);


  /* Add elements to the bin and link them */
  gst_bin_add_many (GST_BIN (rtpsrc), rtpsrc->rtp_src, rtpsrc->rtpbin, NULL);
  lastelt = rtpsrc->rtp_src;
  if (rtpsrc->encrypt) {
    GST_DEBUG_OBJECT (rtpsrc, "Adding decryption");
    gst_bin_add (GST_BIN (rtpsrc), rtpsrc->rtpdecrypt);
    gst_element_link (lastelt, rtpsrc->rtpdecrypt);

    lastelt = rtpsrc->rtpdecrypt;
  }
  if (rtpsrc->rtpheaderchange) {
    GST_DEBUG_OBJECT (rtpsrc, "Adding PT change");
    gst_bin_add (GST_BIN (rtpsrc), rtpsrc->rtpheaderchange);
    gst_element_link (lastelt, rtpsrc->rtpheaderchange);
    lastelt = rtpsrc->rtpheaderchange;
  }
  gst_element_link_pads (lastelt, "src", rtpsrc->rtpbin, "recv_rtp_sink_0");

  g_signal_connect (rtpsrc->rtpbin, "request-pt-map",
      G_CALLBACK (gst_rtp_src_request_pt_map), rtpsrc);

  g_signal_connect (rtpsrc->rtpbin, "pad-added",
      G_CALLBACK (gst_rtp_src_rtpbin_pad_added_cb), rtpsrc);

  if (rtpsrc->enable_rtcp) {
    GST_DEBUG_OBJECT (rtpsrc, "Adding elements and linking up.");
    gst_bin_add_many (GST_BIN (rtpsrc), rtpsrc->rtcp_src, rtpsrc->rtcp_sink,
        NULL);

    gst_element_link_pads (rtpsrc->rtcp_src, "src", rtpsrc->rtpbin,
        "recv_rtcp_sink_0");
    gst_element_link_pads (rtpsrc->rtpbin, "send_rtcp_src_0",
        rtpsrc->rtcp_sink, "sink");
  }

  /* Sync elements states to the parent bin */
  if (!gst_element_sync_state_with_parent (rtpsrc->rtp_src))
    GST_ERROR_OBJECT (rtpsrc, "Could not set RTP source to playing");
  if (rtpsrc->encrypt)
    gst_element_sync_state_with_parent (rtpsrc->rtpdecrypt);
  if (rtpsrc->rtpheaderchange)
    gst_element_sync_state_with_parent (rtpsrc->rtpheaderchange);
  if (!gst_element_sync_state_with_parent (rtpsrc->rtpbin))
    GST_ERROR_OBJECT (rtpsrc, "Could not set RTP bin to playing");

  if (rtpsrc->enable_rtcp) {
    GSocket *rtcpfd = NULL;
    /** The order of these lines is really important **/
    /* First we update the state of rtcp_src so that it creates a socket */
    if (!gst_element_sync_state_with_parent (rtpsrc->rtcp_src))
      GST_ERROR_OBJECT (rtpsrc, "Could not set RTCP source to playing");

    /* Now we can retrieve rtcp_src socket and set it for rtcp_sink element */
    rtcpfd = gst_rtp_src_retrieve_rtcpsrc_socket (rtpsrc);
    g_object_set (G_OBJECT (rtpsrc->rtcp_sink), "socket", rtcpfd, NULL);

    /* And we sync the state of rtcp_sink */
    if (!gst_element_sync_state_with_parent (rtpsrc->rtcp_sink))
      GST_ERROR_OBJECT (rtpsrc, "Could not set RTCP sink to playing");
  }

  return TRUE;
}

static GstStateChangeReturn
gst_rtp_src_change_state (GstElement * element, GstStateChange transition)
{
  GstRtpSrc *rtpsrc = GST_RTP_SRC (element);

  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      GST_DEBUG_OBJECT (rtpsrc, "Configuring rtpsrc");
      if (!gst_rtp_src_start (rtpsrc)) {
        GST_DEBUG_OBJECT (rtpsrc, "Start failed");
        return GST_STATE_CHANGE_FAILURE;
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
    {
      GST_DEBUG_OBJECT (rtpsrc, "Shutting down");
    }
      break;
    default:
      break;
  }

  return GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
}

static void
gst_rtp_src_finalize (GObject * gobject)
{
  GstRtpSrc *src = GST_RTP_SRC (gobject);

  if (src->uri)
    soup_uri_free (src->uri);
  if (src->encoding_name)
    g_free (src->encoding_name);

  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_rtp_src_init (GstRtpSrc * self)
{
  self->uri = NULL;
  self->rtcp_remote_uri = NULL;
  self->encoding_name = NULL;
  self->ghostpad = NULL;
  self->n_ptdemux_pads = 0;
  self->n_rtpbin_pads = 0;
  self->enable_rtcp = DEFAULT_ENABLE_RTCP;
  self->multicast_iface = DEFAULT_PROP_MULTICAST_IFACE;
  self->buffer_size = DEFAULT_BUFFER_SIZE;
  self->latency = DEFAULT_LATENCY_MS;
  self->timeout = DEFAULT_PROP_TIMEOUT;
  self->key_derivation_rate = DEFAULT_PROP_KEY_DERIV_RATE;
  self->encrypt = DEFAULT_PROP_ENCRYPT;
  self->pt_change = GST_RTPPTCHANGE_DEFAULT_PT_NUMBER;
  self->pt_change = GST_RTPPTCHANGE_DEFAULT_PT_NUMBER;
  self->ssrc_select = GST_RTPPTCHANGE_DEFAULT_SSRC_SELECT;
  self->ssrc_select = GST_RTPPTCHANGE_DEFAULT_SSRC_SELECT;

  self->rtpheaderchange = NULL;

  GST_DEBUG_OBJECT (self, "rtpsrc initialised");
}

gboolean
rtp_src_init (GstPlugin * plugin)
{
  return gst_element_register (plugin,
      "rtpsrc", GST_RANK_NONE, GST_TYPE_RTP_SRC);
}

static guint
gst_rtp_src_uri_get_type (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_rtp_src_uri_get_protocols (GType type)
{
  static const gchar *protocols[] = { (char *) "rtp", NULL };

  return protocols;
}

static gchar *
gst_rtp_src_uri_get_uri (GstURIHandler * handler)
{
  GstRtpSrc *rtpsrc = (GstRtpSrc *) handler;

  rtpsrc->last_uri = soup_uri_to_string (rtpsrc->uri, FALSE);

  return rtpsrc->last_uri;
}

static gboolean
gst_rtp_src_uri_set_uri (GstURIHandler * handler, const gchar * uri,
    GError ** error)
{
  GstRtpSrc *rtpsrc = (GstRtpSrc *) handler;

  g_object_set (G_OBJECT (rtpsrc), "uri", uri, NULL);

  return TRUE;
}

static void
gst_rtp_src_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_rtp_src_uri_get_type;
  iface->get_protocols = gst_rtp_src_uri_get_protocols;
  iface->get_uri = gst_rtp_src_uri_get_uri;
  iface->set_uri = gst_rtp_src_uri_set_uri;
}

static gboolean
gst_rtp_src_is_multicast (const gchar * ip_addr)
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
