#!/bin/zsh
# mutation-r3-harness.sh — mutações dos flows R3 (todas devem ser CAUGHT).
# CAUGHT = suite falha (rc!=0). BUILD_FAIL conta como CAUGHT com aviso.
set -uo pipefail
SRC_DIR="${0:A:h}"
TMP_OUT="$SRC_DIR/build/mut-r3-out.txt"
mkdir -p "$SRC_DIR/build"
ARCH=x86_64
MUTS=(
  R3MUT_SKIP_SECOND_IDLE
  R3MUT_SKIP_FULL_POST
  R3MUT_SKIP_HASH
  R3MUT_UNKNOWN_AS_PASS
  R3MUT_CHOOSE_DIRTY
  R3MUT_RUN_GEN64
  R3MUT_ALLOW_LAUNCH
  R3MUT_SKIP_DUPLICATE
  R3MUT_SKIP_CORESELECT
  R3MUT_SKIP_RISCV
  R3MUT_EXPOSE_ADDR
)
CAUGHT=0; TOTAL=${#MUTS[@]}
for M in "${MUTS[@]}"; do
  if ! xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -D${M} \
    -o "$SRC_DIR/build/test-r3-mut" "$SRC_DIR/test-r3-probes.cpp" \
    -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>/dev/null; then
    echo "MUT $M: BUILD_FAIL (caught-construction)"; CAUGHT=$((CAUGHT+1)); continue
  fi
  if "$SRC_DIR/build/test-r3-mut" > "$TMP_OUT" 2>&1; then
    if grep -q "ALL PASS" "$TMP_OUT"; then
      echo "MUT $M: SURVIVED (FAIL)"
    else
      echo "MUT $M: CAUGHT (sem ALL PASS)"; CAUGHT=$((CAUGHT+1))
    fi
  else
    echo "MUT $M: CAUGHT (rc!=0)"; CAUGHT=$((CAUGHT+1))
  fi
done
echo "R3_MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
[ "$CAUGHT" = "$TOTAL" ] || { echo "R3_MUTATION_HARNESS = FAIL"; exit 1; }
echo "R3_MUTATION_HARNESS = PASS"
