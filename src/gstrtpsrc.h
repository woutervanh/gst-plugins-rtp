#ifndef _GST_RTP_SRC_H_
#define _GST_RTP_SRC_H_

#include <gst/gst.h>
#include <gio/gio.h>

G_BEGIN_DECLS
#define GST_TYPE_RTP_SRC (gst_rtp_src_get_type ())
G_DECLARE_FINAL_TYPE (GstRtpSrc, gst_rtp_src, GST, RTP_SRC, GstBin);

gboolean rtp_src_init (GstPlugin * plugin);

G_END_DECLS
#endif /* _GST_RTP_SRC_H_ */
