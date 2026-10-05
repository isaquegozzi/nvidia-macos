#!/bin/zsh
# mutation-channel-harness.sh — P77 M10 channel mutations (-D hooks default-off). CAUGHT = exit != 0.
set -uo pipefail
SRC_DIR="${0:A:h}"
MUT_DIR="$SRC_DIR/build-mutations"
mkdir -p "$MUT_DIR"
ARCH=x86_64
TIMEOUT_SECS=30
MUTS=(
  MUT_SKIP_RPC_READY
  MUT_SKIP_CHID_RANGE
  MUT_SKIP_RUNQ
  MUT_UNKNOWN_ENGINE_OK
  MUT_SKIP_GPFIFO_ALIGN
  MUT_SKIP_INST_ALIGN
  MUT_BIND_SKIP_STATE
  MUT_SCHED_SKIP_STATE
  MUT_BLOCK_CHANGES_STATE
  MUT_ALLOW_CHANGES_STATE
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
for M in "${MUTS[@]}"; do
  echo "-- $M --"
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M \
    -o "$MUT_DIR/test-channel-$M" "$SRC_DIR/test-channel.cpp" -I"$SRC_DIR" \
    2> "$MUT_DIR/$M-build.log" || { echo "$M: BUILD_FAIL"; continue; }
  R=0
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run.log" "$MUT_DIR/test-channel-$M" || R=$?
  echo "$M exit=$R"
  if [ "$R" != "0" ]; then echo "$M: CAUGHT"; CAUGHT=$((CAUGHT + 1)); else echo "$M: ESCAPED"; fi
done
echo "CHANNEL_MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
[ "$CAUGHT" = "$TOTAL" ] || exit 1
