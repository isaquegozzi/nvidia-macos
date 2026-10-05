#!/bin/zsh
# mutation-gfw-negative-capability.sh — categoria B (ASTRA-B-FIX §13).
#
# Capacidades negativas (store MMIO / chamada ao caminho selector7) NÃO
# existem no flow compartilhado nem nos Ops: não há hooks -D para elas
# (seria adicionar capacidade perigosa ao driver só para teste).
# Em vez disso, este script prova que os audits estáticos as detectariam:
#   1. copia a árvore para cópias isoladas (nunca buildadas, nunca live);
#   2. aplica source-transform realista em cada cópia (store de verdade no
#      caminho GFW; chamada de verdade ao template sysmem no caminho GFW);
#   3. executa os MESMOS greps de auditoria do build.sh contra as cópias;
#   4. espera que AMBOS falhem (= detectados).
# Controle: os mesmos greps passam na árvore real (senão o teste é vazio).
# Resultado: NEGATIVE_CAPABILITY_MUTATIONS = 2/2 (por auditoria).
# Uso: ./mutation-gfw-negative-capability.sh (lê SRC_DIR do próprio path).
set -uo pipefail
SRC_DIR="${0:A:h}"
NEG_DIR="$SRC_DIR/build/negcap"
rm -rf "$NEG_DIR"
mkdir -p "$NEG_DIR"
CAUGHT=0
TOTAL=2
{
echo "== negative capability mutations (audit-detected) =="
echo "-- controle: árvore real passa nos audits --"
if grep -n "OSWrite\|configWrite\|setBusLeadEnable\|setBusMasterEnable" "$SRC_DIR/GA106LabGfwReadinessFlow.hpp" | grep -v "^.*://"; then
  echo "CONTROL_STORE: FAIL inesperado na árvore real"; exit 1
fi
if grep -n "RunPrepareGspSysmemFlow\|prepareGspSysmem\|completeDma\|clearDma" "$SRC_DIR/GA106LabGfwReadinessFlow.hpp" | grep -v "^.*://"; then
  echo "CONTROL_SEL7: FAIL inesperado na árvore real"; exit 1
fi
echo "CONTROL: PASS (árvore real limpa; audits não são vacuamente falhos)"
echo "-- NEGCAP_STORE: store real inserido em cópia isolada --"
mkdir -p "$NEG_DIR/store"
cp "$SRC_DIR/GA106LabGfwReadinessFlow.hpp" "$NEG_DIR/store/"
# Transform realista: store 32-bit no registrador de gate após a leitura.
python3 - "$NEG_DIR/store/GA106LabGfwReadinessFlow.hpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
anchor = "        gateRaw = ops.read32(kGfwFlowGateOffset);\n"
assert anchor in s, "âncora de transform não encontrada"
s = s.replace(anchor, anchor + "        OSWriteLittleInt32((volatile void *)(uintptr_t)va, kGfwFlowGateOffset, gateRaw);\n", 1)
open(p, "w").write(s)
print("transform aplicado: OSWriteLittleInt32 no caminho GFW")
PY
if grep -n "OSWrite\|configWrite\|setBusLeadEnable\|setBusMasterEnable" "$NEG_DIR/store/GA106LabGfwReadinessFlow.hpp" | grep -v "^.*://"; then
  echo "NEGCAP_STORE: CAUGHT (audit detecta store)"
  CAUGHT=$((CAUGHT + 1))
else
  echo "NEGCAP_STORE: ESCAPED"
fi
echo "-- NEGCAP_SEL7: chamada sysmem real inserida em cópia isolada --"
mkdir -p "$NEG_DIR/sel7"
cp "$SRC_DIR/GA106LabGfwReadinessFlow.hpp" "$NEG_DIR/sel7/"
python3 - "$NEG_DIR/sel7/GA106LabGfwReadinessFlow.hpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
anchor = "        // 11c. gate read (sempre).\n"
assert anchor in s, "âncora de transform não encontrada"
s = s.replace(anchor, anchor + "        RunPrepareGspSysmemFlow(sel7ops);\n", 1)
open(p, "w").write(s)
print("transform aplicado: RunPrepareGspSysmemFlow no caminho GFW")
PY
if grep -n "RunPrepareGspSysmemFlow\|prepareGspSysmem\|completeDma\|clearDma" "$NEG_DIR/sel7/GA106LabGfwReadinessFlow.hpp" | grep -v "^.*://"; then
  echo "NEGCAP_SEL7: CAUGHT (audit detecta chamada selector7)"
  CAUGHT=$((CAUGHT + 1))
else
  echo "NEGCAP_SEL7: ESCAPED"
fi
echo "NEGATIVE_CAPABILITY_MUTATIONS_CAUGHT = $CAUGHT/$TOTAL"
if [ "$CAUGHT" != "$TOTAL" ]; then
  exit 1
fi
} 2>&1 | tee "$NEG_DIR/negative-capability.log"
