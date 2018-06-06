test_srcs = [
  'rtpsink',
]

test_dependencies = [
  gstreamer,
  gstreamer_check,
]

foreach _test: test_srcs
  test_bin = executable(_test,
    '@0@.c'.format(_test),
    dependencies: test_dependencies,
  )
  test('TEST : '+_test, test_bin,
    args: [ '--gst-plugin-path=@0@'.format(meson.build_root())  ],
    env: [ 'GST_DEBUG=*barco*:6']
  )
endforeach
