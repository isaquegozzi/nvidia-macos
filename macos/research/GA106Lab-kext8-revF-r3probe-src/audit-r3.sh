#!/bin/zsh
# audit-r3.sh — auditoria read-only dos probes R3 (fontes do TU de probe).
# Escopo: probe entrypoints + call graph alcançável + dispatcher fixo.
# NÃO afirma que o KEXT inteiro não tem writers (GateB/BME existem fora do TU).
set -euo pipefail
SRC_DIR="${0:A:h}"
echo "== R3 source audit (probe TU) =="
# A-R3-1: zero writers de TRFCMD/engine no TU de probe.
if grep -n "trfCmdWrites\|launchTrf\|DMATRFCMD.*=\|TRFBASE.*=" "$SRC_DIR/GA106LabR3ProbeFlow.hpp" "$SRC_DIR/GA106LabR3MockOps.hpp" "$SRC_DIR/test-r3-probes.cpp" | grep -v "^.*://" | grep -v "fullPre\|idle1\|idle2\|fullPost\|TRFCMD_REL\|DMATRFCMD (full"; then
  echo "R3_AUDIT_TRFCMD_WRITER = FAIL"; exit 1
fi
echo "R3_TRFCMD_WRITERS = 0"
# A-R3-2: denylist de writers (qualquer ocorrência = FAIL).
if grep -n "configWrite\|OSWrite\|write32\|write16\|write8\|program_brom\|BROM.*=\|sequencer\|gsp_boot\|firmware_load\|doorbell\|GPFIFO\|LAUNCH_DMA\|DMEMC.*=\|IMEMC.*=" "$SRC_DIR/GA106LabR3ProbeFlow.hpp" | grep -v "^.*://"; then
  echo "R3_AUDIT_WRITER_TOKEN = FAIL"; exit 1
fi
echo "R3_WRITER_TOKENS = 0"
# A-R3-3: sem DMA mapping chain no TU (prepare/gen64/synchronize/complete/alloc).
if grep -n "IODMACommand::prepare\|gen64IOVMSegments\|->prepare(\|->complete(\|IOBuffer.*alloc\|getPhysicalSegment" "$SRC_DIR/GA106LabR3ProbeFlow.hpp" | grep -v "^.*://"; then
  echo "R3_AUDIT_DMA_CHAIN = FAIL"; exit 1
fi
echo "R3_DMA_CHAIN_CALLS = 0"
# A-R3-4: só os 3 Probe* entrypoints (sem RunPrepared/LaunchDma/GenericDma).
if grep -n "RunPrepared\|LaunchDma\|GenericDma" "$SRC_DIR/GA106LabR3ProbeFlow.hpp" | grep -v "^.*://"; then
  echo "R3_AUDIT_ENTRYPOINTS = FAIL"; exit 1
fi
echo "R3_ENTRYPOINTS = 3 (Falcon/Mapping/Quies)"
# A-R3-5: sem BME/GateB writers no TU.
if grep -n "setBusLeadEnable\|setBusMasterEnable\|writeBmeCommand\|writeFlushHi\|writeFlushLo" "$SRC_DIR/GA106LabR3ProbeFlow.hpp" "$SRC_DIR/GA106LabR3MockOps.hpp" | grep -v "^.*://"; then
  echo "R3_AUDIT_BME_GATEB = FAIL"; exit 1
fi
echo "R3_BME_GATEB_WRITERS = 0"
# A-R3-6: produção (RealOps R3 + 3 wrappers) sem writers — do marcador ao EOF.
R3BLOCK="$(awk '/R3 read-only probes/,0' "$SRC_DIR/GA106Lab.cpp")"
if print -r -- "$R3BLOCK" | grep -E "configWrite|OSWrite|setBusLeadEnable|setBusMasterEnable|program_brom|doorbell|GPFIFO|LAUNCH_DMA|writeBmeCommand|writeFlush" | grep -v "^.*//"; then
  echo "R3_AUDIT_PROD_WRITERS = FAIL"; exit 1
fi
echo "R3_PROD_WRITERS = 0 (bloco R3RealOps+wrappers)"
# A-R3-7: macros MUT só no harness (nunca em build normal).
if grep -n "^xcrun clang.*-DR3MUT_" "$SRC_DIR/build.sh"; then
  echo "R3MUT_IN_PROD_BUILD = FAIL"; exit 1
fi
echo "R3MUT_ONLY_IN_HARNESS = YES"
echo "R3_SOURCE_AUDIT = PASS"
echo "== R3 object audit =="
xcrun clang++ -Os -std=c++17 -arch x86_64 -c -o "$SRC_DIR/build/r3-audit.o" \
  "$SRC_DIR/test-r3-probes.cpp" -I"$SRC_DIR"
if nm -u "$SRC_DIR/build/r3-audit.o" 2>/dev/null | grep -qi "IODMACommand\|IOServiceOpen\|configWrite\|GSP\|firmware"; then
  echo "R3_OBJECT_AUDIT = FAIL"; exit 1
fi
echo "R3_OBJECT_AUDIT = PASS"
