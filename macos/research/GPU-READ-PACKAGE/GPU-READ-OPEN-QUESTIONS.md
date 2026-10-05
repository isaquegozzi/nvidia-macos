# GPU-READ — Open Questions (pacote offline)

```text
SESSION_ID: V4-GPU-READ-PACKAGE / STATE: MUSE_WORK / LIVE_EXECUTION_AUTHORIZED = NO
live = 0 / nada bloqueia o review
```

| ID | Pergunta | Status |
|---|---|---|
| Q-GR-01 | Binario-em-RAM == source auditado? (Q10 herdado P83 Q-P83-01) | PARCIAL (offline): source<->manifest fechado por referencia (`MANIFEST-P80-1.8.0.txt:1-5` + `P80-FINAL-HANDOFF.md:15-19`); RAM segue BLOCKED ate metodo aprovado — declarado, nao fingido |
| Q-GR-02 | Caller `ga106ctl identity` byte-for-byte + gate hash FULL pre-live | PINADO (forma): `build/obj/ga106ctl identity` (open `ga106ctl.c:20-52`, sel1 `ga106ctl.c:119-141`, close incondicional); gate `shasum -a 256 == 9ab351f5…40` antes de qualquer passo live futuro |
| Q-GR-03 | Privilegio root-only (DIRECT-first, HUMAN_SUDO-para-em-READY) herdado de P83 se aplica a sel1? | ABERTA (reviewer; default: SIM — mesma superficie `IOServiceOpen`, sem senha no executor) |
| Q-GR-04 | `providerEntryID != 0` deve ser PASS-gate rigido ou apenas evidencia? (entryID e estavel por boot mas nao e constante) | ABERTA (default: gate `!=0`, sem igualdade a valor fixo) |
| Q-GR-05 | Re-uso desta auditoria sel1 para sel2/sel3 (mesmo padrao cache/snapshot) ou nova auditoria por selector? | ABERTA (default: nova auditoria; sel3 tem PCI-live, sel2 tem semantica distinta) |
| Q-GR-06 | Ordem pos-sel1: sel3 (PCI snapshot) antes de qualquer BAR/MMIO (sel4-6)? | ABERTA (default: SIM — escada de risco cache -> PCI -> BAR-meta -> MMIO -> DMA/FW) |
| Q-GR-07 | Gap: nenhum teste/mutante offline cobre o handler sel1/sel2 (C-022 passou batido). Adicionar mutante bool->IOReturn + teste de handler antes do live? | ABERTA (default: SIM — registrar gap R2; cobertura futura, nao bloqueia review) |

```text
live = 0 / perguntas acompanham o pacote, nao o APPROVE automatico
```
