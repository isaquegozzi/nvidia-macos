#!/bin/zsh
# mutation-gateb-harness.sh — mutações comportamentais do shared Gate B flow.
# Cada mutação é um -DGA106LAB_GATEB_MUT_* (hooks default-off no header
# GA106LabGateBWriteFlow.hpp, que é shared production path; MockOps carrega
# os hooks de mock). Para cada mutação: rebuilda test-gateb-live-path e
# executa; CAUGHT se exit != 0 (algum assert do contrato quebra).
# Uso: ./mutation-gateb-harness.sh (lê SRC_DIR do próprio path).
set -uo pipefail
SRC_DIR="${0:A:h}"
MUT_DIR="$SRC_DIR/build/mutations-gateb"
mkdir -p "$MUT_DIR"
ARCH=x86_64
# NOTA: sem MUT_NO_ROUNDTRIP — round-trip é identidade aritmética ∀addr
# (bits 0..7+8..39+40..63 particionam os 64 bits), inkillable por construção;
# mantido como defesa em profundidade, documentado no flow header.
# Sem MUT_CLEAR_LATCH_CLOSE separado — nenhum caminho close toca o latch em
# produção (L6 trava o positivo); CLEAR_LATCH_TEARDOWN cobre a classe.
MUTS=(
  GA106LAB_GATEB_MUT_REVERSE_ORDER
  GA106LAB_GATEB_MUT_SKIP_HI
  GA106LAB_GATEB_MUT_SKIP_LO
  GA106LAB_GATEB_MUT_DUP_HI
  GA106LAB_GATEB_MUT_DUP_LO
  GA106LAB_GATEB_MUT_ALLOW_THIRD
  GA106LAB_GATEB_MUT_ALLOW_BME
  GA106LAB_GATEB_MUT_NO_VALID47
  GA106LAB_GATEB_MUT_NO_ALIGN
  GA106LAB_GATEB_MUT_SEGCOUNT
  GA106LAB_GATEB_MUT_SEGLEN
  GA106LAB_GATEB_MUT_CLEAR_LATCH_TEARDOWN
  GA106LAB_GATEB_MUT_ZEROING_STORE
  GA106LAB_GATEB_MUT_ROLLBACK_WRITE
  GA106LAB_GATEB_MUT_WC_MAP
  GA106LAB_GATEB_MUT_SECOND_INVOCATION
  GA106LAB_GATEB_MUT_RETRY_AFTER_HI
  GA106LAB_GATEB_MUT_GENERIC_WRITER
  GA106LAB_GATEB_MUT_DTOR_STORE
  GA106LAB_GATEB_MUT_SKIP_ENTRY_STOP
  GA106LAB_GATEB_MUT_SKIP_BUSY
  GA106LAB_GATEB_MUT_SKIP_PROVIDER
  GA106LAB_GATEB_MUT_SKIP_MSE_PRE
  GA106LAB_GATEB_MUT_SKIP_BAR
  GA106LAB_GATEB_MUT_SKIP_GFW
  GA106LAB_GATEB_MUT_SKIP_REREAD
  GA106LAB_GATEB_MUT_SKIP_PRE_HI_STOP
  GA106LAB_GATEB_MUT_SKIP_MID_STOP
  GA106LAB_GATEB_MUT_SKIP_LATCH_SET
  GA106LAB_GATEB_MUT_SKIP_MAP_RELEASE
  GA106LAB_GATEB_MUT_SKIP_PROV_RELEASE
  GA106LAB_GATEB_MUT_SKIP_BUSY_CLEAR
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
echo "== mutation gateb harness =="
CAUGHT=0
for M in "${MUTS[@]}"; do
  echo "-- $M --"
  xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -D$M \
    -o "$MUT_DIR/test-gateb-$M" \
    "$SRC_DIR/test-gateb-live-path.cpp" "$SRC_DIR/ga106ctl-validate.c" \
    -I"$SRC_DIR" 2> "$MUT_DIR/$M-build.log" || {
      echo "$M: BUILD_FAIL (mutação deve compilar; investigar)"; continue; }
  run_timeout $TIMEOUT_SECS "$MUT_DIR/$M-run.log" "$MUT_DIR/test-gateb-$M"
  code=$?
  if [ $code -eq 0 ]; then
    echo "$M exit=0: SURVIVED (explicar ou corrigir)"
  else
    echo "$M exit=${code}: CAUGHT"
    CAUGHT=$((CAUGHT + 1))
  fi
done
echo "GATEB_MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
[ $CAUGHT -eq $TOTAL ] || exit 1
