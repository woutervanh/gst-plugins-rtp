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

gboolean
gst_barco_is_ipv4 (GstUri* uri)
{
  gboolean res = TRUE;

  g_return_val_if_fail(uri != NULL, FALSE);

  if (uri && 0 != *(gst_uri_get_host(uri))) {
    GInetAddress *addr = g_inet_address_new_from_string (gst_uri_get_host(uri));
    if (g_inet_address_get_family (addr) == G_SOCKET_FAMILY_IPV4) {
      /*g_print ("IPv4 based on %s\n", gst_uri_get_host(uri));*/
    } else {
      /*g_print ("No IPv4 based on %s\n", gst_uri_get_host(uri));*/
      res = FALSE;
    }
    g_object_unref (addr);
  }

  return res;
}
