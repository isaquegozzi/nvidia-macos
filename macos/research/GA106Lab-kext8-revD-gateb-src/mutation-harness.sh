#!/bin/zsh
# mutation-harness.sh — executa mutações test-only do shared sysmem flow.
# Cada mutação é um -DGA106LAB_SYSMEM_MUT_* (hooks default-off no header).
# Para cada mutação: rebuilda test-gsp-sysmem + test-sysmem-concurrency,
# executa ambos; a mutação é CAUGHT se PELO MENOS UM falhar (exit != 0).
# Uso: ./mutation-harness.sh (lê SRC_DIR do próprio path; escreve Mutation dir).
set -uo pipefail
SRC_DIR="${0:A:h}"
MUT_DIR="$SRC_DIR/build/mutations"
mkdir -p "$MUT_DIR"
ARCH=x86_64
MUTS=(
  GA106LAB_SYSMEM_MUT_NO_COMPLETE
  GA106LAB_SYSMEM_MUT_DOUBLE_COMPLETE
  GA106LAB_SYSMEM_MUT_NO_CLEAR
  GA106LAB_SYSMEM_MUT_DOUBLE_CLEAR
  GA106LAB_SYSMEM_MUT_REL_CMD_BEFORE_CLEAR
  GA106LAB_SYSMEM_MUT_REL_DESC_BEFORE_CLEAR
  GA106LAB_SYSMEM_MUT_SECOND_ALLOC
  GA106LAB_SYSMEM_MUT_NO_INPROGRESS
  GA106LAB_SYSMEM_MUT_ACCEPT_MULTI_SEG
  GA106LAB_SYSMEM_MUT_EXPOSE_IOVA
  GA106LAB_SYSMEM_MUT_IGNORE_CLEAR_FAIL
  GA106LAB_SYSMEM_MUT_COMPLETE_ONLY_ON_SUCCESS
  GA106LAB_SYSMEM_MUT_FREE_RELEASE_ON_CLEANUP_FAILED
  GA106LAB_SYSMEM_MUT_FREE_NULL_LOCK
  GA106LAB_INITFREE_MUT_PROD_FREE_RELEASE_CLEANUP_FAILED
  GA106LAB_INITFREE_MUT_PROD_NULL_LOCK_FREE
)
CAUGHT=0
if [ "$#" -gt 0 ]; then
  # Subconjunto explícito (para fatiar execuções longas).
  MUTS=("$@")
fi
TOTAL=${#MUTS[@]}
# Watchdog portátil (macOS não tem `timeout`): mata o teste após N s.
# rc 137/143 = HANG. HANG conta como CAUGHT somente nas mutações que
# removem sincronização/reserva (NO_INPROGRESS, SECOND_ALLOC); nas
# demais, HANG = bug do harness/teste (HARNESS-FAIL, alto e fatal).
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
echo "== mutation harness =="
for M in "${MUTS[@]}"; do
  echo "-- $M --"
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M \
    -o "$MUT_DIR/test-gsp-sysmem-$M" \
    "$SRC_DIR/test-gsp-sysmem.cpp" "$SRC_DIR/ga106ctl-validate.c" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL (conta como caught? NÃO — build deve passar)"; continue; }
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M -pthread \
    -o "$MUT_DIR/test-conc-$M" \
    "$SRC_DIR/test-sysmem-concurrency.cpp" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL"; continue; }
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M -pthread \
    -o "$MUT_DIR/test-stopfree-$M" \
    "$SRC_DIR/test-stop-free-regression.cpp" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL"; continue; }
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M \
    -o "$MUT_DIR/test-lockfail-$M" \
    "$SRC_DIR/test-lock-failure.cpp" \
    -I"$SRC_DIR" 2>> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL"; continue; }
  xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M \
    -o "$MUT_DIR/test-binding-$M" \
    "$SRC_DIR/test-prod-binding.cpp" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL"; continue; }
  R1=0; R2=0; R3=0; R4=0; R5=0
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run1.log" "$MUT_DIR/test-gsp-sysmem-$M" || R1=$?
  echo "$M run1 exit=$R1"
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run2.log" "$MUT_DIR/test-conc-$M" || R2=$?
  echo "$M run2 exit=$R2"
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run3.log" "$MUT_DIR/test-stopfree-$M" || R3=$?
  echo "$M run3 exit=$R3"
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run4.log" "$MUT_DIR/test-lockfail-$M" || R4=$?
  echo "$M run4 exit=$R4"
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run5.log" "$MUT_DIR/test-binding-$M" || R5=$?
  echo "$M run5 exit=$R5"
  HANG=0
  { [ "$R1" = "137" ] || [ "$R1" = "143" ] || [ "$R2" = "137" ] || [ "$R2" = "143" ] || [ "$R3" = "137" ] || [ "$R3" = "143" ] || [ "$R4" = "137" ] || [ "$R4" = "143" ] || [ "$R5" = "137" ] || [ "$R5" = "143" ]; } && HANG=1
  [ "$HANG" = "1" ] && echo "$M HANG matado pelo watchdog @ ${TIMEOUT_SECS}s"
  case "$M" in
    *NO_INPROGRESS|*SECOND_ALLOC)
      if [ "$R1" != "0" ] || [ "$R2" != "0" ] || [ "$R3" != "0" ] || [ "$R4" != "0" ] || [ "$R5" != "0" ]; then echo "$M: CAUGHT (incl. HANG-esperado se houver)"; CAUGHT=$((CAUGHT + 1)); else echo "$M: ESCAPED"; fi ;;
    *)
      if [ "$HANG" = "1" ]; then
        echo "$M: HARNESS-FAIL (hang inesperado — bug, não PASS)"; exit 1
      elif [ "$R1" != "0" ] || [ "$R2" != "0" ] || [ "$R3" != "0" ] || [ "$R4" != "0" ] || [ "$R5" != "0" ]; then echo "$M: CAUGHT"; CAUGHT=$((CAUGHT + 1)); else echo "$M: ESCAPED"; fi ;;
  esac
done
echo "MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
} 2>&1 | tee "$MUT_DIR/mutations.log"
