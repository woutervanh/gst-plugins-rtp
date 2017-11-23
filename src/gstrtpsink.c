/* ex: set tabstop=2 shiftwidth=2 expandtab: */
/*
 * GStreamer
 * Copyright (C) 2009-2017 BARCO
 *
 * Author: Marc Leeman <marc.leeman@barco.com>
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gsturi.h>
#include <gio/gio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>

#include "gstrtpsink.h"
#include "gstbarcomgs_common.h"

GST_DEBUG_CATEGORY_STATIC (rtp_sink_debug);
#define GST_CAT_DEFAULT rtp_sink_debug

struct _GstRtpSink
{
  GstBin parent_instance;

  GstUri *uri;
  gint npads;
  gchar *last_uri;


  guint cidr;
  gint ttl;
  gint ttl_mc;
  gint pt;
  gint src_port;

  GstElement *rtpbin;

  GMutex lock;
};

enum
{
  PROP_0,
  PROP_CIDR,
  PROP_NPADS,
  PROP_SRC_PORT,
  PROP_TTL,
  PROP_TTL_MC,
  PROP_URI,
  PROP_LAST
};

#define DEFAULT_PROP_URI              "rtp://0.0.0.0:5004"
#define DEFAULT_PROP_MUXER            NULL
#define DEFAULT_PROP_CIDR             (32)
#define DEFAULT_PROP_TTL              (64)
#define DEFAULT_PROP_TTL_MC           (8)
#define DEFAULT_SRC_PORT              (0)

/*Investigate "application/x-rtp, rtcp-fb-ccm-fir=(boolean)true"*/
static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink_%u",
    GST_PAD_SINK,
    GST_PAD_REQUEST,
    GST_STATIC_CAPS ("application/x-rtp"));

static void gst_rtp_sink_uri_handler_init (gpointer g_iface,
    gpointer iface_data);

#define gst_rtp_sink_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstRtpSink, gst_rtp_sink, GST_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (GST_TYPE_URI_HANDLER,
        gst_rtp_sink_uri_handler_init));

#define GST_RTP_SINK_GET_LOCK(obj) (&((GstRtpSink*)(obj))->lock)
#define GST_RTP_SINK_LOCK(obj) (g_mutex_lock (GST_RTP_SINK_GET_LOCK(obj)))
#define GST_RTP_SINK_UNLOCK(obj) (g_mutex_unlock (GST_RTP_SINK_GET_LOCK(obj)))

static gboolean gst_rtp_sink_is_multicast (const gchar * ip_addr);
static GstPad* gst_rtp_sink_create_udp (GstRtpSink *self, const gchar *name);

static GstPad *
gst_rtp_sink_request_new_pad (GstElement * element,
    GstPadTemplate * templ, const gchar * name, const GstCaps * caps)
{
  GstRtpSink *self = GST_RTP_SINK (element);
  GstPad *ghost = NULL;

  GST_DEBUG_OBJECT (self, "Request new pad with caps: %" GST_PTR_FORMAT, caps);
  g_return_val_if_fail (self->uri != NULL, NULL);

  if (templ->direction != GST_PAD_SINK) {
    GST_WARNING_OBJECT (self, "Request pad that is not a SINK pad");
    return NULL;
  }

  GST_RTP_SINK_LOCK(self);
  ghost = gst_rtp_sink_create_udp(self, name);

  /* Increment the number of pads that is being used. */
  GST_DEBUG_OBJECT(self, "Exposing pad %" GST_PTR_FORMAT, ghost);

  self->npads++;
  GST_RTP_SINK_UNLOCK(self);

  return ghost;
}

static void
gst_rtp_sink_cleanup_send_chain(GstElement *self, GstPad* sinkpad)
{
  GstElement *parent = NULL;

  g_return_if_fail(sinkpad != NULL);

  parent = gst_pad_get_parent(sinkpad);
  GST_DEBUG_OBJECT(self, "Cleaning up element %" GST_PTR_FORMAT, parent);

  gst_element_release_request_pad (parent, sinkpad);

  gst_object_unref(parent);
}

static void
gst_rtp_sink_release_pad (GstElement * element, GstPad * pad)
{
  GstRtpSink *self = GST_RTP_SINK (element);
  GstPad* target = NULL;

  GST_DEBUG_OBJECT (self, "Release pad with name %" GST_PTR_FORMAT, pad);

  g_return_if_fail (GST_IS_GHOST_PAD (pad));
  g_return_if_fail (GST_IS_RTP_SINK (element));

  GST_RTP_SINK_LOCK(self);
  GST_FIXME_OBJECT (self, "Clean up RTP resources if not used.");
  /* This is a ghost pad, first print some information before following
   * the chain downstream to clean up. */
  target = gst_ghost_pad_get_target(GST_GHOST_PAD(pad));
  if (target != NULL) {
    GST_DEBUG_OBJECT(self, "Processing target pad %" GST_PTR_FORMAT, target);

    GST_DEBUG_OBJECT(self, "Pad %" GST_PTR_FORMAT " is not linked, clean up.", target);
    gst_rtp_sink_cleanup_send_chain(GST_ELEMENT(self), target);

    gst_object_unref(target);
  }

  gst_pad_set_active (pad, FALSE);
  gst_element_remove_pad (GST_ELEMENT (self), pad);

  self->npads--;
  GST_RTP_SINK_UNLOCK(self);
}

static void
gst_rtp_sink_rtpbin_pad_added_cb (GstElement * element,
    GstPad * pad, gpointer data)
{
  GstRtpSink *self = GST_RTP_SINK (data);
  GstPad *target;
  GstCaps *caps;

  GST_INFO_OBJECT(self, "Pad %" GST_PTR_FORMAT " was added on %" GST_PTR_FORMAT, pad, element);

  caps = gst_pad_query_caps (pad, NULL);
  GST_INFO_OBJECT(self, "Pad has caps %" GST_PTR_FORMAT, caps);
  if (GST_PAD_DIRECTION (pad) == GST_PAD_SINK) {
    GST_DEBUG_OBJECT (self, "Not interested in SINK pad with caps %" GST_PTR_FORMAT, caps);
    gst_caps_unref (caps);
    return;
  }

  if (caps) {
    GstCaps *rtcp_caps = gst_caps_new_empty_simple ("application/x-rtcp");

    if (gst_caps_can_intersect (caps, rtcp_caps)) {
      GST_DEBUG_OBJECT (self, "Not interested in pad with RTCP caps %" GST_PTR_FORMAT, caps);
      gst_caps_unref (rtcp_caps);
      gst_caps_unref (caps);
      return;
    }
    gst_caps_unref (rtcp_caps);
  }

  /* Go over the src pads of this element and get the reference to the
   * udp sink was stored there */
  {
    GstIterator *iter = gst_element_iterate_sink_pads (GST_ELEMENT(self));
    GstPad *ipad = NULL;
    GValue data = { 0, };

    while ((gst_iterator_next (iter, &data)) == GST_ITERATOR_OK) {
      ipad = g_value_get_object (&data);
      g_value_unset (&data);
      if (ipad){
        GstElement *sink;

        GST_INFO_OBJECT (self, "Found a sink pad that can be used: %" GST_PTR_FORMAT, ipad);
        sink = g_object_get_data (G_OBJECT (ipad), "rtpsink.rtp_sink");
        GST_DEBUG_OBJECT(self, "Retrieved reference %" GST_PTR_FORMAT, sink);
        target = gst_element_get_compatible_pad(sink, pad, NULL);
        if (target){
          GST_INFO_OBJECT(self, "Found a valid candidate to link against %" GST_PTR_FORMAT, target);
          break;
        }
      }
    }

    /* clean up list */
    gst_iterator_free (iter);
  }

  if (target == NULL){
    GST_WARNING_OBJECT (self,
        "new pad on rtpbin, but there was already a src pad %" GST_PTR_FORMAT, pad);
    gst_caps_unref (caps);
    return;
  }

  gst_pad_link (pad, target);
  gst_object_unref (target);
  gst_caps_unref (caps);
}

static void
gst_rtp_sink_rtpbin_pad_removed_cb (GstElement * element,
    GstPad * pad, gpointer data)
{
  GstRtpSink *self = GST_RTP_SINK (data);
  GstElement *sink = NULL;
  GstPad *peer = NULL;
  GstElement *parent = NULL;

  GST_INFO_OBJECT(self, "Pad %" GST_PTR_FORMAT " was removed on %" GST_PTR_FORMAT, pad, element);

  peer = gst_pad_get_peer(pad);
  if (peer) gst_pad_get_parent(peer);

  GST_INFO_OBJECT(self, "Pad %" GST_PTR_FORMAT ", linked to %" GST_PTR_FORMAT " was removed on %" GST_PTR_FORMAT, pad, peer, parent);

  sink = g_object_get_data (G_OBJECT (pad), "rtpsink.rtp_sink");
  if (GST_IS_ELEMENT(sink)){
    gst_element_set_locked_state (sink, TRUE);
    gst_element_set_state (sink, GST_STATE_NULL);
    gst_bin_remove_many (GST_BIN_CAST (self), sink, NULL);

    GST_INFO_OBJECT(self, "Removed element %" GST_PTR_FORMAT, sink);
  }

  sink = g_object_get_data (G_OBJECT (pad), "rtpsink.rtcp_sink");
  if (GST_IS_ELEMENT(sink)){
    gst_element_set_locked_state (sink, TRUE);
    gst_element_set_state (sink, GST_STATE_NULL);
    gst_bin_remove_many (GST_BIN_CAST (self), sink, NULL);

    GST_INFO_OBJECT(self, "Removed element %" GST_PTR_FORMAT, sink);
  }

  sink = g_object_get_data (G_OBJECT (pad), "rtpsink.rtcp_src");
  if (GST_IS_ELEMENT(sink)){
    gst_element_set_locked_state (sink, TRUE);
    gst_element_set_state (sink, GST_STATE_NULL);
    gst_bin_remove_many (GST_BIN_CAST (self), sink, NULL);

    GST_INFO_OBJECT(self, "Removed element %" GST_PTR_FORMAT, sink);
  }

  if (peer) gst_object_unref(peer);
  if (parent) gst_object_unref(parent);
}

/* When a stream is sent on a socket, send out the information on the
 * relevant pad to let upper bins know what the combination of
 * Pad/Socket/Caps is */
static void
gst_rtp_sink_send_uri_info(GstRtpSink *self, const GstPad * pad, const gchar* uri)
{
  GstStructure *s1, *s2;

  g_return_if_fail(self != NULL);
  g_return_if_fail(pad != NULL);
  g_return_if_fail(uri != NULL);

  GST_DEBUG_OBJECT(self, "Sending signal uri %s on %" GST_PTR_FORMAT, uri, pad);

  /*The message will take ownership of the structure */
  s1 = gst_structure_new ("GstRtpSinkUriInfo",
      "uri", G_TYPE_STRING, uri,
      NULL);

  s2 = gst_structure_copy (s1);
  /* post element message with codec */
  gst_element_post_message (GST_ELEMENT (self),
      gst_message_new_element (GST_OBJECT_CAST (self), s1)
      );

  gst_pad_push_event(pad,
      gst_event_new_custom (GST_EVENT_CUSTOM_UPSTREAM, s2));
}

static void
gst_rtp_sink_set_sdes (GstElement * rtpbin, const gchar * prop,
    const gchar * val)
{
  GstStructure *s;

  GST_DEBUG("Setting \"%s\" to \"%s\",", prop, val);

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
  GST_DEBUG_OBJECT(self, "New element added %" GST_PTR_FORMAT, new_element);

  g_return_if_fail (new_element != NULL);

  /* set minimal interval on 0.5s */
  gst_rtp_sink_find_property ("rtcp-min-interval", (guint64) 500000000);
}

/* In order for RTP to work by passing the data through the same bin;
 * a number of udpsrc/udpsinks need to be created per media that is sent
 * out. While media can be fed through the same rtpbin in order to keep them
 * in the same session; each media sent should be sent on a different socket
 * (See RFC 3550).
 */
static GstPad*
gst_rtp_sink_create_udp (GstRtpSink *self, const gchar *name)
{
  GstElement *rtp_sink, *rtcp_sink, *rtcp_src;
  GstCaps *caps;
  GstUri *uri = gst_uri_copy(self->uri);
  guint v=0;
  GstPad *pad;

  GST_DEBUG_OBJECT(self, "Hooking up UDP elements.");

  g_return_val_if_fail(self->uri, NULL);

  /* Adjust the URI to match with the CIDR notation */
  if (gst_barco_is_ipv4(uri)){
    guint u[4];
    const gchar* host = gst_uri_get_host(uri);
    if( host == NULL){
      GST_ERROR_OBJECT(self, "Could not get a valid host.");
      return NULL;
    }

    if (sscanf(host, "%d.%d.%d.%d", &u[0], &u[1], &u[2], &u[3]) == 4){
      gchar *nhost;

      GST_DEBUG_OBJECT(self, "Scanned %d.%d.%d.%d", u[0], u[1], u[2], u[3]);
      v = (u[0]<<24) | (u[1]<<16) | (u[2]<<8) | u[3];
      v += self->npads;

      nhost = g_strdup_printf("%d.%d.%d.%d",
          (v>>24), (v>>16)&0xff, (v>>8)&0xff, (v&0xff));

      GST_INFO_OBJECT(self, "Updating host location from %s to %s.", host, nhost);
      GST_FIXME_OBJECT(self, "Implement CIDR checking.");
      gst_uri_set_host(uri, nhost);
      g_free(nhost);
    }
  }

  rtp_sink = gst_element_factory_make ("udpsink", NULL);
  rtcp_sink = gst_element_factory_make ("udpsink", NULL);
  rtcp_src = gst_element_factory_make ("udpsrc", NULL);

  g_return_val_if_fail (rtp_sink != NULL, NULL);
  g_return_val_if_fail (rtcp_sink != NULL, NULL);
  g_return_val_if_fail (rtcp_src != NULL, NULL);

  gst_bin_add_many (GST_BIN (self), rtp_sink, rtcp_sink, rtcp_src, NULL);

  /* Set properties */
  GST_DEBUG_OBJECT(self, "Configuring the RTP/RTCP sink elements.");
  g_object_set (G_OBJECT (rtp_sink),
      "async", FALSE,
      "ttl", self->ttl,
      "ttl-mc", self->ttl_mc,
      "host", gst_uri_get_host(uri),
      "port", gst_uri_get_port(uri),
      "auto-multicast", TRUE, NULL);

  /* auto-multicast should be set to false as rtcp_src will already
   * join the multicast group */
  g_object_set (G_OBJECT (rtcp_sink),
      "sync", FALSE,
      "async", FALSE,
      "ttl", self->ttl,
      "ttl-mc", self->ttl_mc,
      "host", gst_uri_get_host(uri),
      "port", gst_uri_get_port(uri) + 1,
      "close-socket", FALSE, "auto-multicast", FALSE, NULL);

  GST_DEBUG_OBJECT(self, "Configuring the RTCP source element.");
  if (gst_rtp_sink_is_multicast (gst_uri_get_host(uri))) {
    g_object_set (G_OBJECT (rtcp_src),
        "address", gst_uri_get_host(uri),
        "port", gst_uri_get_port(uri) + 1,
        NULL);
  } else {
    g_object_set (G_OBJECT (rtcp_src),
        "port", gst_uri_get_port(uri) + 1,
        NULL);
  }

  GST_DEBUG_OBJECT(self, "Set RTCP caps.");
  caps = gst_caps_from_string ("application/x-rtcp");
  g_object_set (G_OBJECT (rtcp_src),
      "caps", caps, "auto-multicast", TRUE, NULL);
  gst_caps_unref (caps);

  {
    /* Link the UDP sources and sinks to the RTP bin element. This should
       be done for each stream that is added while only using one single
       rtpbin element. */
    gchar *name;
    GST_DEBUG_OBJECT (self, "Connecting pads");

    /* Get the RTP (data) pad on the rtpbin to reuse later on, this pad
     * will be ghosted to the rtpsink bin to allow feeding in data. */
    name = g_strdup_printf ("send_rtp_sink_%d", self->npads);
    pad = gst_element_get_request_pad (self->rtpbin, name);
    g_free(name);

    /* This is very bad, there is a mixup with the pads */
    g_return_val_if_fail ( pad != NULL, NULL);

    /* Link up RTP bin data pad to the udpsink to send on.
     *
     * It looks as if that this link can only be used when a pad is
     * spawned; the link up will be a bit later as a result. */
    name = g_strdup_printf ("send_rtp_src_%d", self->npads);
    if (!gst_element_link_pads (self->rtpbin, name, rtp_sink, "sink"))
      GST_ERROR_OBJECT(self, "Problem linking up outgoing RTP data (%s).", name);
    g_free(name);

    {
      gchar* suri = gst_uri_to_string(uri);
      gst_rtp_sink_send_uri_info(self, pad, suri);
      g_free(suri);
    }

    /* Link up the RTCP control data pad to the udpsink to send on. */
    name = g_strdup_printf ("send_rtcp_src_%d", self->npads);
    if (!gst_element_link_pads (self->rtpbin, name, rtcp_sink, "sink"))
      GST_ERROR_OBJECT(self, "Problem linking up outgoing RTCP data (%s).", name);
    g_free(name);

    /* Link up the udpsrc incoming RTCP data to the RTCP control sink bin */
    name = g_strdup_printf ("recv_rtcp_sink_%d", self->npads);
    if (!gst_element_link_pads (rtcp_src, "src", self->rtpbin, name))
      GST_ERROR_OBJECT(self, "Problem linking up incoming RTCP data (%s).", name);
    g_free(name);
  }

  gst_pad_set_event_function (pad,
      (GstPadEventFunction) gst_rtp_sink_rtp_bin_event);

  if(!gst_element_sync_state_with_parent (rtp_sink))
    GST_ERROR_OBJECT (self, "Could not set RTP sink to playing.");

  /* First we update the state of rtcp_src so that it creates a socket and
   * binds on the port gst_uri_get_port(self->uri) + 1 */
  if (!gst_element_sync_state_with_parent (rtcp_src))
    GST_ERROR_OBJECT (self, "Could not set RTCP source to playing");

  /* Now we can retrieve rtcp_src socket and set it for rtcp_sink element */
  {
    GSocket *rtcp_src_socket;

    g_object_get (G_OBJECT (rtcp_src), "used-socket",
        &(rtcp_src_socket), NULL);
    g_object_set (G_OBJECT (rtcp_sink),
        "socket", rtcp_src_socket,
        NULL);
  }
  /* And we sync the state of rtcp_sink */
  if (!gst_element_sync_state_with_parent (rtcp_sink))
    GST_ERROR_OBJECT (self, "Could not set RTCP sink to playing");

  g_object_set_data (G_OBJECT (pad), "rtpsink.rtp_sink", rtp_sink);
  g_object_set_data (G_OBJECT (pad), "rtpsink.rtcp_sink", rtcp_sink);
  g_object_set_data (G_OBJECT (pad), "rtpsink.rtcp_src", rtcp_src);
  {
    GstPad *ghost;
    GstPadTemplate *pad_tmpl;

    pad_tmpl = gst_static_pad_template_get (&sink_template);
    ghost = gst_ghost_pad_new_from_template (name, pad, pad_tmpl);
    gst_object_unref (pad_tmpl);
    gst_object_unref(pad);

    gst_pad_set_active(ghost, TRUE);
    gst_element_add_pad(GST_ELEMENT (self), ghost);

    /* Store last references. There are needed further on to link up the
     * new pads. */
    GST_DEBUG_OBJECT(self, "Storing reference to %" GST_PTR_FORMAT, rtp_sink);
    g_object_set_data (G_OBJECT (ghost), "rtpsink.rtp_sink", rtp_sink);
    g_object_set_data (G_OBJECT (ghost), "rtpsink.rtp_uri", uri);

    /*gst_uri_unref(uri);*/

    return ghost;
  }
  /* Bad */
  return NULL;
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
      if(self->uri){
        gst_object_set_properties_from_uri_query_parameters (G_OBJECT (self), self->uri);
      }
      break;
    case PROP_CIDR:
      self->cidr = g_value_get_uint (value);
      break;
    case PROP_TTL:
      self->ttl = g_value_get_int (value);
      break;
    case PROP_TTL_MC:
      self->ttl_mc = g_value_get_int (value);
      break;
    case PROP_SRC_PORT:
      self->src_port = g_value_get_int (value);
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
    case PROP_CIDR:
      g_value_set_uint (value, self->cidr);
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
    case PROP_NPADS:
      g_value_set_uint (value, self->npads);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_rtp_sink_finalize (GObject * gobject)
{
  GstRtpSink *self = GST_RTP_SINK (gobject);

  if (self->uri)
    gst_uri_unref (self->uri);

  g_mutex_clear (&self->lock);
  G_OBJECT_CLASS (parent_class)->finalize (gobject);
}

static void
gst_rtp_sink_class_init (GstRtpSinkClass * klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (klass);

  oclass->set_property = gst_rtp_sink_set_property;
  oclass->get_property = gst_rtp_sink_get_property;
  oclass->finalize = gst_rtp_sink_finalize;

  gstelement_class->request_new_pad = GST_DEBUG_FUNCPTR (gst_rtp_sink_request_new_pad);
  gstelement_class->release_pad = GST_DEBUG_FUNCPTR (gst_rtp_sink_release_pad);

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
   * GstRtpSink::cidr
   *
   * CIDR Network Mask (for use in routed networks)
   *
   * Since: 1.10.0
   */
  g_object_class_install_property (oclass, PROP_CIDR,
      g_param_spec_uint ("cidr", "CIDR Network Mask to generate new IPs with",
          "CIDR Network Mask",
          0, 32, DEFAULT_PROP_CIDR,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstRtpSink: n-pads
   *
   * The number of active pads (requested).
   *
   * Since: 1.10.0
   */
  g_object_class_install_property (oclass, PROP_NPADS,
      g_param_spec_uint ("n-pads", "Number of sink pads",
          "Read the number of sink pads", 0, G_MAXUINT, 0, G_PARAM_READABLE));

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_template));

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

static void
gst_rtp_sink_init (GstRtpSink * self)
{
  self->npads = 0;
  self->rtpbin = NULL;
  self->uri = gst_uri_from_string(DEFAULT_PROP_URI);
  self->cidr = DEFAULT_PROP_CIDR;
  self->ttl = DEFAULT_PROP_TTL;
  self->ttl_mc = DEFAULT_PROP_TTL_MC;
  self->src_port = DEFAULT_SRC_PORT;
  g_mutex_init (&self->lock);

  {
    GST_INFO_OBJECT(self, "Initialising rtpbin element.");

    self->rtpbin = gst_element_factory_make ("rtpbin", NULL);
    g_return_if_fail (self->rtpbin != NULL);

    if (g_object_class_find_property (G_OBJECT_GET_CLASS (self->rtpbin),
          "use-pipeline-clock")) {
      g_object_set (G_OBJECT (self->rtpbin), "use-pipeline-clock", TRUE, NULL);
    } else {
      GST_WARNING_OBJECT (self,
          "rtpbin has no use-pipeline-clock, running old version?");
    }

    GST_DEBUG_OBJECT (self, "Connecting callbacks");
    g_signal_connect (self->rtpbin, "pad-added",
        G_CALLBACK (gst_rtp_sink_rtpbin_pad_added_cb), self);
    g_signal_connect (self->rtpbin, "pad-removed",
        G_CALLBACK (gst_rtp_sink_rtpbin_pad_removed_cb), self);
    g_signal_connect (self->rtpbin, "element-added",
        G_CALLBACK (gst_rtp_sink_rtpsink_element_added), NULL);

    gst_bin_add_many (GST_BIN (self), self->rtpbin, NULL);

    gst_element_sync_state_with_parent (GST_ELEMENT (self->rtpbin));
  }

  GST_OBJECT_FLAG_SET (GST_OBJECT (self), GST_ELEMENT_FLAG_SINK);
  GST_DEBUG_OBJECT (self, "rtpsink initialised");
}

gboolean
rtp_sink_init (GstPlugin * plugin)
{
  return gst_element_register (plugin,
      "rtpsink", GST_RANK_NONE, GST_TYPE_RTP_SINK);
}
