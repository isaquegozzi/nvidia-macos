#!/bin/zsh
# mutation-flush-harness.sh — mutações do template Gate A 1.7.1 (P74 §14).
# Cada mutação é um -DGA106LAB_FLUSH_MUT_* (hooks default-off em
# GA106LabFlushPrewriteFlow.hpp, shared production path). Para cada uma:
# rebuilda test-flush-verify e executa; CAUGHT se exit != 0 (incl. watchdog).
set -uo pipefail
SRC_DIR="${0:A:h}"
MUT_DIR="$SRC_DIR/build/mutations-flush"
mkdir -p "$MUT_DIR"
ARCH=x86_64
TIMEOUT_SECS=20
MUTS=(
  GA106LAB_FLUSH_MUT_DOUBLE_RESERVE
  GA106LAB_FLUSH_MUT_FORCE_FIRST_BUSY
  GA106LAB_FLUSH_MUT_NO_RESERVE
  GA106LAB_FLUSH_MUT_RELEASE_AFTER_FAILED_CLEAR
  GA106LAB_FLUSH_MUT_ZERO_AFTER_FAILED_CLEAR
  GA106LAB_FLUSH_MUT_SECOND_MAPPING_ON_READY
)
run_timeout() {
  local secs=$1; local log=$2; shift 2
  "$@" > "$log" 2>&1 & local pid=$!
  ( sleep $secs; kill -9 $pid 2>/dev/null ) & local watcher=$!
  wait $pid 2>/dev/null; local rc=$?
  kill $watcher 2>/dev/null; wait 2>/dev/null
  return $rc
}
CAUGHT=0; TOTAL=${#MUTS[@]}
{
for M in "${MUTS[@]}"; do
  echo "-- $M --"
  xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -D$M \
    -o "$MUT_DIR/test-flush-verify-$M" \
    "$SRC_DIR/test-flush-verify.cpp" "$SRC_DIR/ga106ctl-validate.c" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation \
    2> "$MUT_DIR/$M-build.log" || { echo "$M: BUILD_FAIL"; continue; }
  R=0
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run.log" "$MUT_DIR/test-flush-verify-$M" || R=$?
  echo "$M exit=$R"
  if [ "$R" != "0" ]; then echo "$M: CAUGHT"; CAUGHT=$((CAUGHT + 1)); else echo "$M: ESCAPED"; fi
done
echo "FLUSH_MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
} 2>&1 | tee "$MUT_DIR/mutations-flush.log"
