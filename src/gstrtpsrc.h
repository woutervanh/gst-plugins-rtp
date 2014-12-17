#ifndef _GST_RTP_SRC_H_
#define _GST_RTP_SRC_H_

#include "gst/gst.h"
#include <libsoup/soup.h>

G_BEGIN_DECLS
#define GST_TYPE_RTP_SRC             (gst_rtp_src_get_type ())
#define GST_RTP_SRC(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_RTP_SRC, GstRtpSrc))
#define GST_RTP_SRC_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_RTP_SRC, GstRtpSrcClass))
#define GST_IS_RTP_SRC(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_RTP_SRC))
#define GST_IS_RTP_SRC_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_RTP_SRC))
#define GST_RTP_SRC_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_RTP_SRC, GstRtpSrcClass))
typedef struct _GstRtpSrcClass GstRtpSrcClass;
typedef struct _GstRtpSrc GstRtpSrc;

struct _GstRtpSrcClass
{
  GstBinClass parent_class;
};

struct _GstRtpSrc
{
  GstBin parent_instance;

  SoupURI *uri, *rtcp_remote_uri;
  gchar *last_uri;
  gchar *encoding_name;
  guint pt_select;
  guint pt_change;
  guint ssrc_select;
  guint ssrc_change;
  gchar *multicast_iface;
  guint buffer_size;
  guint latency;
  gboolean encrypt;
  guint32 key_derivation_rate;

  gboolean enable_rtcp;

  GstElement *rtp_src;
  GstElement *rtcp_src;
  GstElement *rtcp_sink;
  GstElement *rtpbin;
  GstElement *rtpheaderchange;
  GstElement *rtpdecrypt;

  gint n_ptdemux_pads;
  gint n_rtpbin_pads;
  GstPad *ghostpad;
};

GType
gst_rtp_src_get_type (void)
    G_GNUC_CONST;

     gboolean rtp_src_init (GstPlugin * plugin);

G_END_DECLS
#endif /* _GST_RTP_SRC_H_ */
