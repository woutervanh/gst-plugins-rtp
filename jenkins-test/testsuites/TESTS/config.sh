#!/usr/bin/env bats
export LAUNCHER="/usr/bin/gst-launch-1.0"
teardown() {
  echo $status
  popd
}

setup() {
  TEST=TEST${BATS_TEST_NUMBER}_${BATS_TEST_NAME}
  rm -rf /test/reports/test-results/${TEST}
  mkdir -p /test/reports/test-results/${TEST}
  pushd /test/reports/test-results/${TEST}
  export GST_DEBUG_DUMP_DOT_DIR=$(pwd)
  export GST_DEBUG='*barco*:6,*:3'
  export HOME=$(pwd)
  export GST_DEBUG_NO_COLOR=1
}
