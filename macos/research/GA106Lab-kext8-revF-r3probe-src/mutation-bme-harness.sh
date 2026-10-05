#!/bin/zsh
# mutation-bme-harness.sh — mutações comportamentais do shared BME flow.
# Cada mutação é um -DGA106LAB_BME_MUT_* (hooks default-off no header
# GA106LabBmeFlow.hpp; Mock carrega hooks de mock). Para cada mutação:
# rebuilda test-bme-enable + executa; CAUGHT se exit != 0.
# Uso: ./mutation-bme-harness.sh (lê SRC_DIR do próprio path).
set -uo pipefail
SRC_DIR="${0:A:h}"
MUT_DIR="$SRC_DIR/build/mutations-bme"
mkdir -p "$MUT_DIR"
ARCH=x86_64
MUTS=(
  GA106LAB_BME_MUT_NO_BOOT_NONCE
  GA106LAB_BME_MUT_LATCH_ONLY
  GA106LAB_BME_MUT_SKIP_GATEA
  GA106LAB_BME_MUT_SKIP_GATEB
  GA106LAB_BME_MUT_SKIP_LATCH_CHECK
  GA106LAB_BME_MUT_ALLOW_RELOAD
  GA106LAB_BME_MUT_SKIP_PREREAD
  GA106LAB_BME_MUT_SKIP_READBACK
  GA106LAB_BME_MUT_IGNORE_RECOVERY
  GA106LAB_BME_MUT_ALLOW_SECOND
  GA106LAB_BME_MUT_HARDCODE_CMD
  GA106LAB_BME_MUT_CLOBBER_MSE
  GA106LAB_BME_MUT_SKIP_PREWRITE_STOP
  GA106LAB_BME_MUT_AUTO_DISABLE
  GA106LAB_BME_MUT_START_DMA
  GA106LAB_BME_MUT_GENERIC_PCI
)
if [ "$#" -gt 0 ]; then
  MUTS=("$@")
fi
TOTAL=${#MUTS[@]}
TIMEOUT_SECS=90
run_timeout() {
    secs=$1; out=$2; shift 2
    "$@" > "$out" 2>&1 & pid=$!
    (sleep "$secs"; kill -9 $pid 2>/dev/null) & wd=$!
    wait $pid 2>/dev/null; rc=$?
    kill $wd 2>/dev/null; wait $wd 2>/dev/null || true
    return $rc
}
echo "== mutation bme harness =="
CAUGHT=0
for M in "${MUTS[@]}"; do
  echo "-- $M --"
  xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -D$M \
    -o "$MUT_DIR/test-bme-$M" \
    "$SRC_DIR/test-bme-enable.cpp" "$SRC_DIR/ga106ctl-validate.c" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL (mutação deve compilar; investigar)"; continue; }
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run.log" "$MUT_DIR/test-bme-$M"
  code=$?
  if [ $code -eq 0 ]; then
    echo "$M exit=0: SURVIVED (explicar ou corrigir)"
  else
    echo "$M exit=${code}: CAUGHT"
    CAUGHT=$((CAUGHT + 1))
  fi
done
echo "BME_MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
[ $CAUGHT -eq $TOTAL ] || exit 1
