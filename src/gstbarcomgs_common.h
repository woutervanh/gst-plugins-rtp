/* ex: set tabstop=2 shiftwidth=2 expandtab: */
/** \class gstsbarcomgs_common
 *
 * \brief common functions to use in the gst barco bins
 *
 * \author Marc Leeman
 * \date 2009
 *
 */

#ifndef _GSTBARCOMGS_COMMON_H_
#define _GSTBARCOMGS_COMMON_H_

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/gsturi.h>
#include <gio/gio.h>
#include <gst/rtp/gstrtppayloads.h>

#define xgst_barco_set_supported_parameter(element, name, val) \
{ \
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (element), name)){ \
    GST_DEBUG("Parameter \"%s\" supported.", name); \
    g_object_set(G_OBJECT(element), name, val, NULL); \
  } \
}

#define xgst_barco_propagate_setting(bin, property, value) \
  { \
    GstIterator *it; \
    GValue data = { 0, }; \
    it = gst_bin_iterate_recurse (GST_BIN(bin)); \
    while(gst_iterator_next(it, &data) == GST_ITERATOR_OK){ \
      GstElement *elem = g_value_get_object(&data); \
      xgst_barco_set_supported_parameter(elem, property, value); \
      g_value_unset (&data); \
    } \
    gst_iterator_free(it); \
  }

gboolean gst_barco_query_to_boolean(gchar * value);
gboolean gst_barco_is_ipv4(GstUri *uri);
void gst_barco_parse_uri (GObject * obj, GstUri* uri, GstDebugCategory * cat);

#endif
