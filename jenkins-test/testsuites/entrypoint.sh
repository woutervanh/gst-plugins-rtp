#!/usr/bin/env bash
set -e
USER_ID=${LOCAL_USER_ID:-9001}
useradd --shell /bin/bash -u $USER_ID -d /home/user -o -c "" -m user
sudo -E -u user rm -rf /test/reports/test-results || true
sudo -E -u user mkdir -p /test/reports/test-results
FILES=$(pushd /test/testsuites/TESTS/ >/dev/null && ls *.bats && popd >/dev/null )
for file in $FILES; do
  sudo -E -u user  bats --tap /test/testsuites/TESTS/$file | tee /test/reports/test-results/report-"$(uname -m)"-${file//.bats/}.tap
done
