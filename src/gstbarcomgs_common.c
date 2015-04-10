/* ex: set tabstop=2 shiftwidth=2 expandtab: */
/** \class gstsbarcomgs_common
 *
 * \brief common functions to use in the gst barco bins
 *
 * \author Marc Leeman
 * \date 2009
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>

#ifdef WIN32
#include "../src/gstbarcomgs_common.h"
#else
#include "src/gstbarcomgs_common.h"
#endif
/** 
 * @brief return TRUE/FALSE based on string values accepted in GST
 * 
 * @param value: string to analyse
 * 
 * @return TRUE if GST true value; false otherwise
 */
gboolean
gst_barco_query_to_boolean (gchar * value)
{
  gchar *down;

  g_return_val_if_fail (value != NULL, FALSE);

  down = g_ascii_strdown (value, -1);
  if (g_strcmp0 (down, "true") == 0 ||
      g_strcmp0 (down, "1") == 0 || g_strcmp0 (down, "on") == 0) {
    g_free (down);
    return TRUE;
  }
  g_free (down);
  return FALSE;
}

gboolean
gst_barco_is_ipv4 (GstUri* uri)
{
  gboolean res = TRUE;

  if (uri && 0 != *(gst_uri_get_host(uri))) {
    GInetAddress *addr = g_inet_address_new_from_string (gst_uri_get_host(uri));
    if (g_inet_address_get_family (addr) == G_SOCKET_FAMILY_IPV4) {
      g_print ("IPv4 based on %s\n", gst_uri_get_host(uri));
    } else {
      g_print ("No IPv4 based on %s\n", gst_uri_get_host(uri));
      res = FALSE;
    }
    g_object_unref (addr);
  }

  return res;
}

void
gst_barco_parse_uri (GObject * obj, GstUri* uri, GstDebugCategory * cat)
{
  GHashTable *hash_table = gst_uri_get_query_table (uri);
  GList *keys = NULL, *key;

  if (hash_table != NULL){
    keys = g_hash_table_get_keys (hash_table);

    for (key = keys; key; key = key->next) {
      GParamSpec *spec;
      spec = g_object_class_find_property (G_OBJECT_GET_CLASS (obj), key->data);
      if (spec) {
        switch (spec->value_type) {
          case G_TYPE_BOOLEAN:
            g_object_set (obj, key->data, gst_barco_query_to_boolean ((gchar *)
                    g_hash_table_lookup (hash_table, key->data)), NULL);
            break;
          case G_TYPE_INT:
            g_object_set (obj, key->data,
                (gint) g_ascii_strtoll (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_UINT:
            g_object_set (obj, key->data,
                (guint) g_ascii_strtoull (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_INT64:
            g_object_set (obj, key->data,
                g_ascii_strtoll (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_UINT64:
            g_object_set (obj, key->data,
                g_ascii_strtoull (
                    (gchar *) g_hash_table_lookup (hash_table, key->data),
                    NULL, 0), NULL);
            break;
          case G_TYPE_STRING:
            g_object_set (obj, key->data,
                (gchar *) g_hash_table_lookup (hash_table, key->data), NULL);
            break;
          default:
            /* Not fundamental types or unknown */
            if (spec->value_type == GST_TYPE_CAPS) {
              GstCaps *caps = gst_caps_from_string (
                  (gchar *) g_hash_table_lookup (hash_table, key->data));
              g_object_set (obj, key->data, caps, NULL);
              gst_caps_unref (caps);
            } else {
              GST_CAT_WARNING_OBJECT (cat, obj,
                  "Unknown type or not yet supported: %s "
                  "(Maybe it should be added)", g_type_name (spec->value_type));
              continue;
            }
            break;
        }
        GST_CAT_LOG_OBJECT (cat, obj, "Set property %s: %s",
            (const gchar *) key->data,
            (const gchar *) g_hash_table_lookup (hash_table, key->data));
      } else
        GST_CAT_LOG_OBJECT (cat, obj, "Property %s not supported",
            (const gchar *) key->data);
    }

    g_list_free (keys);
    g_hash_table_unref (hash_table);
  }
}
