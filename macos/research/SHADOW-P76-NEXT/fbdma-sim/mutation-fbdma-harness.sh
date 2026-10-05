#!/bin/zsh
# mutation-fbdma-harness.sh — M7 FBDMA mutations (-D hooks default-off). CAUGHT = exit != 0.
set -uo pipefail
SRC_DIR="${0:A:h}"
MUT_DIR="$SRC_DIR/build-mutations"
mkdir -p "$MUT_DIR"
ARCH=x86_64
TIMEOUT_SECS=30
MUTS=(
  MUT_SKIP_ALIGN_CHECK
  MUT_IGNORE_TIMEOUT
  MUT_WRONG_TARGET_OK
  MUT_CHUNK_SIZE_MISMATCH
  MUT_ZERO_LEN_OK
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
    -o "$MUT_DIR/test-fbdma-$M" "$SRC_DIR/test-fbdma.cpp" -I"$SRC_DIR" \
    2> "$MUT_DIR/$M-build.log" || { echo "$M: BUILD_FAIL"; continue; }
  R=0
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run.log" "$MUT_DIR/test-fbdma-$M" || R=$?
  echo "$M exit=$R"
  if [ "$R" != "0" ]; then echo "$M: CAUGHT"; CAUGHT=$((CAUGHT + 1)); else echo "$M: ESCAPED"; fi
done
echo "FBDMA_MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
[ "$CAUGHT" = "$TOTAL" ] || exit 1
