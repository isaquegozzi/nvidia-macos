# H0-15a — Correção do typo `-wegnogpu` → `-wegnoegpu` (patch pronto, NÃO aplicado: repo read-only)

O spelling correto documentado pelo WhateverGreen é `-wegnoegpu` (desabilitar GPUs
externas). O HANDOFF anterior continha `-wegnogpu` (sem o `e`) em 3 linhas, todas em
`docs/macos/TGM0-HANDOFF.md` §6. Nenhuma outra ocorrência no repo (fora `.git`).

Aplicar quando o repo estiver gravável (Linux ou NTFS rw):

```diff
--- a/docs/macos/TGM0-HANDOFF.md
+++ b/docs/macos/TGM0-HANDOFF.md
@@ -44,7 +44,7 @@
 ## 6. AVISO: NVRAM contém `-wegnogpu` (WhateverGreen)
+## 6. AVISO: NVRAM contém `-wegnoegpu` (via Lilu — WhateverGreen ausente, ver H0)
@@ -46,7 +46,7 @@
-O Hackintosh boota com `-wegnogpu` (sem ele, não boota com a RTX presente).
+O Hackintosh boota com `-wegnoegpu` (sem ele, não boota com a RTX presente — NÃO testado remover; ver plano BARE em `docs/macos/hardware/ga106-bare-boot-plan.md`).
@@ -52,7 +52,7 @@
-- **Risco real**: `-wegnogpu` desabilita dGPUs e pode acionar `_OFF` (power-gate
+- **Risco real**: `-wegnoegpu` desabilita GPUs externas (e `disable-gpu=true` na RTX via DeviceProperties); `_OFF`/power-gate NÃO observado em H0 (nenhum SSDT de GPU, remoção é lógica no bridge — ver `docs/macos/hardware/ga106-h0-root-cause.md`).
```

Verificação pós-patch (no repo gravável):

```sh
grep -rn "wegnogpu" docs/ | grep -v "wegnoegpu" ; echo "resta-typo:$? (1 = limpo)"
grep -rn "wegnoegpu" docs/ | head
```

Nota: `ga106-ioreg.md` (TGM0, em `~/Documents/nvidia-macos-TGM0/`) já usa o spelling
correto e não precisa de correção.
