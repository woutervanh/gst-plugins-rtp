/* ex: set tabstop=2 shiftwidth=2 expandtab: */
/** \class gstsbarco
 *
 * \brief Initialisation for the overall barcomgs
 *
 * \author Marc Leeman
 * \date 2009
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstrtpsink.h"
#include "gstrtpsrc.h"

/* top level library code; initialise the plugins part of this library */

/** 
 * @brief Initialise the barco library
 * 
 * @param plugin: plugin to initialise
 * 
 * @return TRUE if all plugins were initialised properly
 */
static gboolean
plugin_init (GstPlugin * plugin)
{

  if (!rtp_sink_init (plugin))
    return FALSE;

  if (!rtp_src_init (plugin))
    return FALSE;

  return TRUE;
}

#ifndef VERSION
#define VERSION "1.0"
#endif

#ifndef PACKAGE
#define PACKAGE "gst-plugins-barcortp"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    barco,
    "Barco RTP Plugins",
    plugin_init, VERSION, "LGPL", "Barco", "http://www.barco.com/");
