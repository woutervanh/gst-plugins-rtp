#!/usr/bin/env bats
source /test/testsuites/TESTS/config.sh

@test rtpsink {
    run  ${LAUNCHER}  videotestsrc horizontal-speed=1 num_buffers=200 ! videoconvert ! x264enc ! rtph264pay config-interval=1 ! rtpsink uri=rtp://239.1.2.3:1234
    echo "${output}"
    [   -f *PAUSED_PLAYING.dot ]
    [ ! -f *error.dot ]
}


@test rtpsrc {
    run ${LAUNCHER}  videotestsrc horizontal-speed=1 num-buffers=120 rtpsrc uri=rtp://239.1.2.3:1234?encoding-name=H264 ! decodebin ! videoconvert ! fakesink
    echo "${output}"
    [   -f *PAUSED_PLAYING.dot ]
    [ ! -f *error.dot ]
}
