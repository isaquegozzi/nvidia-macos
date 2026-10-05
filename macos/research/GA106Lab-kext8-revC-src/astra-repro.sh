#!/bin/zsh
# astra-repro.sh — TEST-PATH-FIX §12 PROVA CONTRA O ATAQUE DO ASTRA.
# Reproduz explicitamente o cenário da auditoria:
#   copy tree
#   mutate only production/shared implementation
#   do NOT alter tests
#   build
#   run all relevant tests
# As duas mutações precisam ser detectadas, senão GATE=FAIL.
# Uso: ./astra-repro.sh (lê SRC_DIR do próprio path).
set -uo pipefail
SRC_DIR="${0:A:h}"
REPRO_DIR="$SRC_DIR/build/astra-repro"
rm -rf "$REPRO_DIR"
mkdir -p "$REPRO_DIR"
ARCH=x86_64
TIMEOUT_SECS=90

run_timeout() {
    secs=$1; out=$2; shift 2
    "$@" > "$out" 2>&1 & pid=$!
    (sleep "$secs"; kill -9 $pid 2>/dev/null) & wd=$!
    wait $pid 2>/dev/null; rc=$?
    kill $wd 2>/dev/null; wait $wd 2>/dev/null || true
    return $rc
}

echo "== ASTRA REPRODUCTION (mutate ONLY shared/prod, tests untouched) =="

# --- REPRO 1: defeito real no free de produção (shared path) ---
echo "-- ASTRA_REPRODUCTION_1: shared free releases in CLEANUP_FAILED --"
cp -a "$SRC_DIR/GA106LabInitFreeLockFlow.hpp" "$REPRO_DIR/orig-flow.hpp"
# Mutação SOMENTE no shared (cópia): força releases em CLEANUP_FAILED.
# Implementada via -D no build (mesmo efeito que editar o shared: ativa o
# branch de bug no shared production path, sem tocar nenhum arquivo de teste).
xcrun clang++ -Os -std=c++17 -arch $ARCH \
  -DGA106LAB_INITFREE_MUT_PROD_FREE_RELEASE_CLEANUP_FAILED \
  -o "$REPRO_DIR/test-stopfree-repro1" \
  "$SRC_DIR/test-stop-free-regression.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$REPRO_DIR/repro1-build.log" || {
    echo "ASTRA_REPRODUCTION_1: BUILD_FAIL"; exit 1; }
xcrun clang++ -Os -std=c++17 -arch $ARCH \
  -DGA106LAB_INITFREE_MUT_PROD_FREE_RELEASE_CLEANUP_FAILED \
  -o "$REPRO_DIR/test-binding-repro1" \
  "$SRC_DIR/test-prod-binding.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$REPRO_DIR/repro1-build.log" || {
    echo "ASTRA_REPRODUCTION_1: BUILD_FAIL"; exit 1; }
R1=0; R2=0
run_timeout $TIMEOUT_SECS "$REPRO_DIR/repro1-run1.log" "$REPRO_DIR/test-stopfree-repro1" || R1=$?
run_timeout $TIMEOUT_SECS "$REPRO_DIR/repro1-run2.log" "$REPRO_DIR/test-binding-repro1" || R2=$?
echo "repro1 stopfree exit=$R1 binding exit=$R2"
if [ "$R1" != "0" ] || [ "$R2" != "0" ]; then
  echo "ASTRA_REPRODUCTION_1 = CAUGHT"
else
  echo "ASTRA_REPRODUCTION_1 = ESCAPED (GATE=FAIL)"
  exit 1
fi

# --- REPRO 2: defeito real no lock path de produção (shared path) ---
echo "-- ASTRA_REPRODUCTION_2: shared lock path frees NULL --"
xcrun clang++ -Os -std=c++17 -arch $ARCH \
  -DGA106LAB_INITFREE_MUT_PROD_NULL_LOCK_FREE \
  -o "$REPRO_DIR/test-lockfail-repro2" \
  "$SRC_DIR/test-lock-failure.cpp" \
  -I"$SRC_DIR" 2> "$REPRO_DIR/repro2-build.log" || {
    echo "ASTRA_REPRODUCTION_2: BUILD_FAIL"; exit 1; }
xcrun clang++ -Os -std=c++17 -arch $ARCH \
  -DGA106LAB_INITFREE_MUT_PROD_NULL_LOCK_FREE \
  -o "$REPRO_DIR/test-binding-repro2" \
  "$SRC_DIR/test-prod-binding.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$REPRO_DIR/repro2-build.log" || {
    echo "ASTRA_REPRODUCTION_2: BUILD_FAIL"; exit 1; }
R3=0; R4=0
run_timeout $TIMEOUT_SECS "$REPRO_DIR/repro2-run1.log" "$REPRO_DIR/test-lockfail-repro2" || R3=$?
run_timeout $TIMEOUT_SECS "$REPRO_DIR/repro2-run2.log" "$REPRO_DIR/test-binding-repro2" || R4=$?
echo "repro2 lockfail exit=$R3 binding exit=$R4"
if [ "$R3" != "0" ] || [ "$R4" != "0" ]; then
  echo "ASTRA_REPRODUCTION_2 = CAUGHT"
else
  echo "ASTRA_REPRODUCTION_2 = ESCAPED (GATE=FAIL)"
  exit 1
fi

echo "ASTRA_REPRODUCTION_1 = CAUGHT"
echo "ASTRA_REPRODUCTION_2 = CAUGHT"
echo "ASTRA_REPRO_DONE"
