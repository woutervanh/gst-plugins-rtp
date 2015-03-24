#ifndef _GST_RTP_SINK_H_
#define _GST_RTP_SINK_H_

#include <gst/gst.h>
#include <gst/gsturi.h>
#include <gio/gio.h>

G_BEGIN_DECLS
#define GST_TYPE_RTP_SINK             (gst_rtp_sink_get_type ())
#define GST_RTP_SINK(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTP_SINK, GstRtpSink))
#define GST_RTP_SINK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_RTP_SINK, GstRtpSinkClass))
#define GST_IS_RTP_SINK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTP_SINK))
#define GST_IS_RTP_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_RTP_SINK))
#define GST_RTP_SINK_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_RTP_SINK, GstRtpSinkClass))
typedef struct _GstRtpSinkClass GstRtpSinkClass;
typedef struct _GstRtpSink GstRtpSink;

struct _GstRtpSinkClass
{
  GstBinClass parent_class;
};

struct _GstRtpSink
{
  GstBin parent_instance;

  GstUri *uri;
  gchar *last_uri;

  GSocket *rtp_sink_socket;
	GSocket *rtcp_src_socket;

  gint ttl;
  gint ttl_mc;
  gint pt;
  gint src_port;
  gboolean encrypt;
  guint32 key_derivation_rate;

  GstElement *rtp_sink;
  GstElement *rtcp_sink;
  GstElement *rtcp_src;
  GstElement *rtpencrypt;
  GstPad *sinkpad;
};

GType
gst_rtp_sink_get_type (void)
    G_GNUC_CONST;

     gboolean rtp_sink_init (GstPlugin * plugin);

G_END_DECLS
#endif /* _GST_RTP_SINK_H_ */
