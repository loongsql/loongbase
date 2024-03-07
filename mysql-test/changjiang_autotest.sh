#!/usr/bin/env bash

./mysql-test-run.pl \
  --suites=main \
  --parallel=4 \
  --timer --suite-timeout=120 --max-test-fail=10000 \
  --force --vardir=var \
  --skip-ndb --max-connections=2048 \
  --user=root --clean-vardir --report-unstable-tests \
  --retry-failure=1 \
  --mysqld="--changjiang-engine-force=on"
# --mysqld="--columnstore_vectorized_execution" --mysqld="--columnstore_vectorized_scan" > mtr-csi-ve.0530
# ./mysql-test-run.pl --mysqld="--changjiang-engine-force=on" --user=root main.ctype_latin1_de
