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
gst_barco_is_ipv4 (SoupURI * uri)
{
  gboolean res = TRUE;

  if (uri->query) {
    GInetAddress *addr = g_inet_address_new_from_string (uri->host);
    if (g_inet_address_get_family (addr) == G_SOCKET_FAMILY_IPV4) {
      g_print ("IPv4 based on %s\n", uri->host);
    } else {
      g_print ("No IPv4 based on %s\n", uri->host);
      res = FALSE;
    }
    g_object_unref (addr);
  }

  return res;
}
