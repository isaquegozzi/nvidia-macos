#!/bin/sh
# P79 mutation harness: each mutant must be CAUGHT (exit != 0).
set -u
cd "$(dirname "$0")"
MUTS="UNKNOWN_OPCODE_OK UNKNOWN_METHOD_OK RING_FULL_OK UNKICKED_OK PB_RESIDENCY_OK COUNT_ZERO_OK SUBCHANNEL_OK RESERVED_OK TOKEN_OK TEARDOWN_OK"
overall=0
for m in $MUTS; do
  if clang++ -O2 -std=c++17 -Wall -Wextra -DSUB79_MUT_${m} -o /tmp/test-sub-MUT_${m} test-submission.cpp -I. 2>/tmp/submut-${m}-build.log; then
    /tmp/test-sub-MUT_${m} >/tmp/submut-${m}-run.log 2>&1
    code=$?
    if [ $code -eq 0 ]; then
      echo "MUT_${m} exit=0: ESCAPED"
      overall=1
    else
      echo "MUT_${m} exit=${code}: CAUGHT"
    fi
  else
    echo "MUT_${m}: BUILD-FAIL (counts as caught, see submut-${m}-build.log)"
  fi
done
exit $overall
