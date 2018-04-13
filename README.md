# gst-plugins-rtp
GStreamer elements to handle RTP streams with an rtp:// uri

This project is a stand-alone and up-to-date version of the code submitted
back in 2013.

These elements help using RTP streams in a proper way by linking up the UDP src/sink elements to rtpbin to provide proper RTP and RTCP use.
This is essential to stream media over a network while maintaining correct timing behaviour.

Use as follows:

```
$ gst-launch-1.0 videotestsrc horizontal-speed=1 ! videoconvert ! x264enc ! rtph264pay config-interval=1 ! rtpsink uri=rtp://239.1.2.3:1234
$ gst-launch-1.0 rtpsrc uri=rtp://239.1.2.3:1234?encoding-name=H264 ! decodebin ! videoconvert ! autovideosink
```

The 'encoding-name' is used to hint for the correct caps and
they typically map on the encoding-name used in the caps of the
(de)payloaders. See src/gstrtpparameters.h for the detailed definition.

The modules no longer depend on
'gst_object_set_properties_from_uri_query_parameters'; see
https://bugzilla.gnome.org/show_bug.cgi?id=779765. If this patch is not
applied to GStreamer Core, the code will be linked in in this project.

