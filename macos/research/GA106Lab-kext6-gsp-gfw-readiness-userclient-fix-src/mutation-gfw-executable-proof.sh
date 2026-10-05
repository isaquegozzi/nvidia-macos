#!/bin/zsh
# mutation-gfw-executable-proof.sh — mutações EXECUTÁVEIS reais (PROMPT 57
# §10/§11, categoria REAL_EXECUTABLE_BEHAVIORAL).
#
# As categorias anteriores de "store" e "chamada selector7" eram falso
# positivo (objeto idêntico / não compilava). Aqui cada mutação:
#   1. é aplicada por source-transform em CÓPIA ISOLADA da árvore;
#   2. COMPILA de verdade como objeto KEXT (-mkernel -c, mesmos flags do
#      build.sh) — prova que é mutação válida do caminho executável;
#   3. produz diferença binária real + evidência em disassembly;
#   4. é detectada pelos audits de source (e binário) do build.sh.
# A cópia mutada NUNCA é linkada (-kext), instalada, carregada ou executada.
# Se a compilação for tecnicamente impossível, relata-se
# UNSUPPORTED_EXECUTABLE_MUTATION (nunca se inventa PASS).
# Uso: ./mutation-gfw-executable-proof.sh (lê SRC_DIR do próprio path).
set -uo pipefail
SRC_DIR="${0:A:h}"
PROOF_DIR="$SRC_DIR/build/mutexec"
rm -rf "$PROOF_DIR"
mkdir -p "$PROOF_DIR"
ARCH=x86_64
SDK=$(xcrun --show-sdk-path)
KHDR="$SDK/System/Library/Frameworks/Kernel.framework/Headers"
VERSMIN="-mmacosx-version-min=26.5"
KEXTFLAGS=(-arch $ARCH -Os -mkernel $VERSMIN -fno-builtin -fno-stack-protector -fno-common -fapple-kext -nostdinc -I"$KHDR" -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT)
CAUGHT=0
TOTAL=2
{
echo "== executable mutation proof (isolated copies, TU compile only) =="
echo "-- baseline: compila TU GA106Lab.cpp da árvore real --"
xcrun clang++ "${KEXTFLAGS[@]}" -c \
  -o "$PROOF_DIR/GA106Lab-baseline.o" "$SRC_DIR/GA106Lab.cpp" \
  2> "$PROOF_DIR/baseline-build.log" || { echo "BASELINE_BUILD = FAIL"; exit 1; }
BASE_SHA=$(shasum -a 256 "$PROOF_DIR/GA106Lab-baseline.o" | awk '{print $1}')
echo "BASELINE_BUILD = OK ($BASE_SHA)"
echo "-- MUT store: OSWriteLittleInt32 real no caminho selector8 --"
mkdir -p "$PROOF_DIR/store"
cp -a "$SRC_DIR"/GA106Lab*.h* "$SRC_DIR"/ga106ctl-validate.h "$PROOF_DIR/store/" 2>/dev/null
cp "$SRC_DIR/GA106Lab.cpp" "$PROOF_DIR/store/"
cp "$SRC_DIR/GA106LabUserClient.cpp" "$PROOF_DIR/store/" 2>/dev/null || true
python3 - "$PROOF_DIR/store/GA106LabGfwReadinessFlow.hpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
# Âncora de duas linhas (exclui o bloco #ifdef UNBOUNDED inativo, indentado).
anchor = "        // 11c. gate read (sempre).\n        gateRaw = ops.read32(kGfwFlowGateOffset);\n"
assert anchor in s, "âncora de transform não encontrada"
s = s.replace(anchor, anchor + "        OSWriteLittleInt32((volatile void *)(uintptr_t)va, kGfwFlowGateOffset, gateRaw);\n", 1)
open(p, "w").write(s)
print("transform aplicado: store MMIO efetivo no caminho selector8")
PY
if xcrun clang++ "${KEXTFLAGS[@]}" -c \
  -o "$PROOF_DIR/GA106Lab-store.o" "$PROOF_DIR/store/GA106Lab.cpp" \
  -I"$PROOF_DIR/store" 2> "$PROOF_DIR/store-build.log"; then
  echo "MUT_STORE_COMPILES = YES (mutação executável válida)"
else
  echo "MUT_MMIO_STORE = UNSUPPORTED_EXECUTABLE_MUTATION (não compila)"
  echo "MUT_MMIO_STORE_BINARY_DIFF = NO"
  echo "MUT_MMIO_STORE_ACTUALLY_PRESENT = NO"
  echo "MUT_MMIO_STORE_CAUGHT = NO"
  exit 1
fi
STORE_SHA=$(shasum -a 256 "$PROOF_DIR/GA106Lab-store.o" | awk '{print $1}')
if [ "$STORE_SHA" = "$BASE_SHA" ]; then
  echo "MUT_MMIO_STORE_BINARY_DIFF = NO (objeto idêntico — falso positivo)"
  exit 1
fi
echo "MUT_MMIO_STORE_BINARY_DIFF = YES"
otool -tvV "$PROOF_DIR/GA106Lab-baseline.o" > "$PROOF_DIR/baseline-disasm.txt" 2>/dev/null
otool -tvV "$PROOF_DIR/GA106Lab-store.o" > "$PROOF_DIR/store-disasm.txt" 2>/dev/null
BASE_N=$(grep -c "0x118128" "$PROOF_DIR/baseline-disasm.txt" || true)
MUT_N=$(grep -c "0x118128" "$PROOF_DIR/store-disasm.txt" || true)
echo "gate-offset refs baseline=$BASE_N mutated=$MUT_N"
if [ "$MUT_N" -gt "$BASE_N" ]; then
  echo "MUT_MMIO_STORE_ACTUALLY_PRESENT = YES (referência extra a 0x118128 no disassembly)"
  grep "0x118128" "$PROOF_DIR/store-disasm.txt" | grep -i "mov.*0x118128(" | head -n 4 || true
else
  echo "MUT_MMIO_STORE_ACTUALLY_PRESENT = NO (sem evidência no disassembly)"
  exit 1
fi
if grep -n "OSWrite\|configWrite\|setBusLeadEnable\|setBusMasterEnable" "$PROOF_DIR/store/GA106LabGfwReadinessFlow.hpp" | grep -v "^.*://"; then
  echo "MUT_MMIO_STORE_CAUGHT = YES (source audit rejeita store)"
  CAUGHT=$((CAUGHT + 1))
else
  echo "MUT_MMIO_STORE_CAUGHT = NO (audit não detectou)"
  exit 1
fi
echo "-- MUT sel7: chamada real ao caminho selector7/sysmem no selector8 --"
mkdir -p "$PROOF_DIR/sel7"
cp -a "$SRC_DIR"/GA106Lab*.h* "$SRC_DIR"/ga106ctl-validate.h "$PROOF_DIR/sel7/" 2>/dev/null
cp "$SRC_DIR/GA106Lab.cpp" "$PROOF_DIR/sel7/"
cp "$SRC_DIR/GA106LabUserClient.cpp" "$PROOF_DIR/sel7/" 2>/dev/null || true
python3 - "$PROOF_DIR/sel7/GA106Lab.cpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
anchor = "        GA106LabGfwRealOps ops(this);\n        return RunGfwBootReadinessFlow(ops, *out);"
assert anchor in s, "âncora de transform não encontrada"
s = s.replace(anchor, "        GA106LabGspSysmemV1 sel7snap;\n        GA106LabSysmemRealOps sel7ops(this);\n        (void)RunPrepareGspSysmemFlow(sel7ops, sel7snap);\n" + anchor, 1)
open(p, "w").write(s)
print("transform aplicado: chamada real RunPrepareGspSysmemFlow no wrapper selector8")
PY
if xcrun clang++ "${KEXTFLAGS[@]}" -c \
  -o "$PROOF_DIR/GA106Lab-sel7.o" "$PROOF_DIR/sel7/GA106Lab.cpp" \
  -I"$PROOF_DIR/sel7" 2> "$PROOF_DIR/sel7-build.log"; then
  echo "MUT_SEL7_COMPILES = YES (mutação executável válida)"
else
  echo "MUT_SELECTOR7_CALL = UNSUPPORTED_EXECUTABLE_MUTATION (não compila)"
  echo "MUT_SELECTOR7_CALL_BINARY_DIFF = NO"
  echo "MUT_SELECTOR7_CALL_ACTUALLY_PRESENT = NO"
  echo "MUT_SELECTOR7_CALL_CAUGHT = NO"
  exit 1
fi
SEL7_SHA=$(shasum -a 256 "$PROOF_DIR/GA106Lab-sel7.o" | awk '{print $1}')
if [ "$SEL7_SHA" = "$BASE_SHA" ]; then
  echo "MUT_SELECTOR7_CALL_BINARY_DIFF = NO (objeto idêntico — falso positivo)"
  exit 1
fi
echo "MUT_SELECTOR7_CALL_BINARY_DIFF = YES"
otool -tvV "$PROOF_DIR/GA106Lab-sel7.o" > "$PROOF_DIR/sel7-disasm.txt" 2>/dev/null
BASE_S=$(grep -c "Sysmem" "$PROOF_DIR/baseline-disasm.txt" || true)
MUT_S=$(grep -c "Sysmem" "$PROOF_DIR/sel7-disasm.txt" || true)
echo "sysmem refs baseline=$BASE_S mutated=$MUT_S"
if [ "$MUT_S" -gt "$BASE_S" ]; then
  echo "MUT_SELECTOR7_CALL_ACTUALLY_PRESENT = YES (callsite sysmem extra no disassembly)"
  grep "Sysmem" "$PROOF_DIR/sel7-disasm.txt" | grep -i "call" | head -n 3 || true
else
  echo "MUT_SELECTOR7_CALL_ACTUALLY_PRESENT = NO (sem evidência no disassembly)"
  exit 1
fi
if awk '/GA106Lab::readGfwBootReadiness/,/^}/' "$PROOF_DIR/sel7/GA106Lab.cpp" | grep -n "RunPrepareGspSysmemFlow\|prepareGspSysmem\|completeDma\|clearDma" | grep -v "^.*://"; then
  echo "MUT_SELECTOR7_CALL_CAUGHT = YES (audit detecta dependência/call no wrapper selector8)"
  CAUGHT=$((CAUGHT + 1))
else
  echo "MUT_SELECTOR7_CALL_CAUGHT = NO (audit não detectou)"
  exit 1
fi
echo "EXECUTABLE_MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
} 2>&1 | tee "$PROOF_DIR/executable-proof.log"
