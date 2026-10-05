#!/bin/zsh
# mutation-gfw-harness.sh — mutações comportamentais do shared GFW readiness
# flow (ASTRA-B-FIX: categoria A — compilam pelo shared production path).
# Cada mutação é um -DGA106LAB_GFW_MUT_* (hooks default-off no header
# GA106LabGfwReadinessFlow.hpp, que é shared production path).
# Para cada mutação: rebuilda test-gfw-readiness + test-gfw-concurrency e
# executa ambos; CAUGHT se PELO MENOS UM falhar (exit != 0).
# Capacidades negativas (store MMIO / chamada selector7) NÃO estão aqui:
# o flow compartilhado e os Ops NÃO possuem tais hooks (ver
# mutation-gfw-negative-capability.sh, categoria B por auditoria).
# MUT_UNBOUNDED_POLL remove o bound de propósito: o hang é capturado pelo
# watchdog (rc 137/143) e conta como CAUGHT documentado (única exceção onde
# HANG é esperado; nas demais, HANG = HARNESS-FAIL fatal).
# Uso: ./mutation-gfw-harness.sh (lê SRC_DIR do próprio path).
set -uo pipefail
SRC_DIR="${0:A:h}"
MUT_DIR="$SRC_DIR/build/mutations-gfw"
mkdir -p "$MUT_DIR"
ARCH=x86_64
MUTS=(
  GA106LAB_GFW_MUT_WRONG_GATE_OFFSET
  GA106LAB_GFW_MUT_WRONG_PROGRESS_OFFSET
  GA106LAB_GFW_MUT_WRONG_GATE_MASK
  GA106LAB_GFW_MUT_WRONG_COMPLETION_VALUE
  GA106LAB_GFW_MUT_REMOVE_GATE_CHECK
  GA106LAB_GFW_MUT_UNBOUNDED_POLL
  GA106LAB_GFW_MUT_REMOVE_TIMEOUT
  GA106LAB_GFW_MUT_WAIT_UNDER_LOCK
  GA106LAB_GFW_MUT_EXPOSE_MMIO_ADDRESS
  GA106LAB_GFW_MUT_LEAK_MAP
  GA106LAB_GFW_MUT_ALLOW_SECOND_CONCURRENT_CALL
  GA106LAB_GFW_MUT_STOP_RELEASE_EARLY
)
CAUGHT=0
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
{
echo "== mutation gfw harness =="
for M in "${MUTS[@]}"; do
  echo "-- $M --"
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M -pthread \
    -o "$MUT_DIR/test-gfw-$M" \
    "$SRC_DIR/test-gfw-readiness.cpp" "$SRC_DIR/ga106ctl-validate.c" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL (conta como caught? NÃO — build deve passar)"; continue; }
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M -pthread \
    -o "$MUT_DIR/test-gfwconc-$M" \
    "$SRC_DIR/test-gfw-concurrency.cpp" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL"; continue; }
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M -pthread \
    -o "$MUT_DIR/test-gfwlife-$M" \
    "$SRC_DIR/test-gfw-lifetime.cpp" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL"; continue; }
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M -pthread \
    -o "$MUT_DIR/test-gfwdeadline-$M" \
    "$SRC_DIR/test-gfw-deadline.cpp" "$SRC_DIR/ga106ctl-validate.c" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL"; continue; }
  R1=0; R2=0; R3=0; R4=0
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run1.log" "$MUT_DIR/test-gfw-$M" || R1=$?
  echo "$M run1 exit=$R1"
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run2.log" "$MUT_DIR/test-gfwconc-$M" || R2=$?
  echo "$M run2 exit=$R2"
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run3.log" "$MUT_DIR/test-gfwlife-$M" || R3=$?
  echo "$M run3 exit=$R3"
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run4.log" "$MUT_DIR/test-gfwdeadline-$M" || R4=$?
  echo "$M run4 exit=$R4"
  HANG=0
  { [ "$R1" = "137" ] || [ "$R1" = "143" ] || [ "$R2" = "137" ] || [ "$R2" = "143" ] || [ "$R3" = "137" ] || [ "$R3" = "143" ] || [ "$R4" = "137" ] || [ "$R4" = "143" ]; } && HANG=1
  [ "$HANG" = "1" ] && echo "$M HANG matado pelo watchdog @ ${TIMEOUT_SECS}s"
  case "$M" in
    GA106LAB_GFW_MUT_UNBOUNDED_POLL)
      if [ "$R1" != "0" ] || [ "$R2" != "0" ] || [ "$R3" != "0" ] || [ "$R4" != "0" ]; then echo "$M: CAUGHT (HANG-esperado documentado)"; CAUGHT=$((CAUGHT + 1)); else echo "$M: ESCAPED"; fi ;;
    *)
      if [ "$HANG" = "1" ]; then
        echo "$M: HARNESS-FAIL (hang inesperado — bug, não PASS)"; exit 1
      elif [ "$R1" != "0" ] || [ "$R2" != "0" ] || [ "$R3" != "0" ] || [ "$R4" != "0" ]; then echo "$M: CAUGHT"; CAUGHT=$((CAUGHT + 1)); else echo "$M: ESCAPED"; fi ;;
  esac
done
echo "MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
} 2>&1 | tee "$MUT_DIR/mutations-gfw.log"
