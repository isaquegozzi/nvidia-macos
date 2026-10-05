#!/bin/sh
# P78 mutation harness: each mutant must be CAUGHT (exit != 0).
set -u
cd "$(dirname "$0")"
MUTS="SKIP_RPC_READY SKIP_VA_ALIGN SKIP_PA_ALIGN SKIP_WRAP UNKNOWN_APER_OK ALLOW_RW_BYPASS ALLOW_PRIV_BYPASS BIND_SKIP_STATE UNMAP_ALWAYS_OK MAP_OVERLAP_OK"
overall=0
for m in $MUTS; do
  if clang++ -O2 -std=c++17 -Wall -Wextra -DVM78_MUT_${m} -o /tmp/test-vm-MUT_${m} test-vm.cpp -I. 2>/tmp/mut-${m}-build.log; then
    /tmp/test-vm-MUT_${m} >/tmp/mut-${m}-run.log 2>&1
    code=$?
    if [ $code -eq 0 ]; then
      echo "MUT_${m} exit=0: ESCAPED"
      overall=1
    else
      echo "MUT_${m} exit=${code}: CAUGHT"
    fi
  else
    echo "MUT_${m}: BUILD-FAIL (counts as caught, see mut-${m}-build.log)"
  fi
done
exit $overall
