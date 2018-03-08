# gst-plugins-rtp
GStreamer elements to handle RTP streams with an rtp:// uri
 
These elements help using RTP streams in a proper way by linking up the UDP src/sink elements to rtpbin to provide proper RTP and RTCP use.
This is essential to stream media over a network while maintaining correct timing behaviour.

Use as follows:

```
$ gst-launch-1.0 videotestsrc horizontal-speed=1 ! videoconvert ! x264enc ! rtph264pay config-interval=1 ! rtpsink uri=rtp://239.1.2.3:1234
$ gst-launch-1.0 rtpsrc uri=rtp://239.1.2.3:1234?encoding-name=H264 ! decodebin ! videoconvert ! autovideosink
```

The 'encoding-name' is used to hint for the correct caps.

The modules do depend on 'gst_object_set_properties_from_uri_query_parameters'; see https://bugzilla.gnome.org/show_bug.cgi?id=779765
 
