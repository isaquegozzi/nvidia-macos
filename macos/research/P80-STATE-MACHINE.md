> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# P80 — State Machine (production offline)

Author: Muse, 2026-09-09 -0300. Reflete `GA106LabProductionPhase.h` + fiação em
`GA106Lab-kext8-production-architecture-src/GA106Lab.cpp` (1.8.0, offline).

## Estados

```text
0 Detached → 1 Attached → 2 Bar0Mapped → 3 BootIdentityVerified
  → 4 SysmemPrepared → 5 GfwReady → 6 FlushPreconditionsVerified
  → 7 ChannelPrepared → 8 VmPrepared → 9 SubmissionPrepared
10 LiveBlocked (terminal até re-init; teto do P80)
```

## Mapeamento para milestones reais

| Fase | Fonte (sucesso) |
|---|---|
| Attached | `start()` START_SUCCESS |
| Bar0Mapped | selector 4 `probeBar0Map` |
| BootIdentityVerified | selector 6 `readStaticIdentity` (selector 5 não avança: corroboração) |
| SysmemPrepared | selector 7 `prepareGspSysmem` |
| GfwReady | selector 8 `readGfwBootReadiness` |
| FlushPreconditionsVerified | selector 9 `verifySysmemFlushPreconditions` |
| ChannelPrepared | `prepareChannelModel()` (contrato P77 válido) |
| VmPrepared | `prepareVmModel()` (contrato P78 válido) |
| SubmissionPrepared | `prepareSubmissionModel()` (cross-contrato P77+P78+P79) |
| LiveBlocked | qualquer `blockRealOperation(i)` com fase ≥ Attached |

## Regras (ver `ProductionPhaseCanAdvance`)

- Avanço normal: exatamente +1 (`Skip`/`Reenter` caso contrário).
- Notas de milestone são best-effort: falha de avanço NÃO altera selectors
  (fail-closed só nos gates `prepare*`/`blockRealOperation`).
- Caminho ordenado exigido para os prepares (uso fora de ordem deixa a fase para
  trás — conservador, nunca inseguro).
- `LiveBlocked`: terminal; limpa só no próximo `start()` com sucesso.
- Teardown: qualquer estado drenável (1..9) → Attached; `Detached` nada tem a
  drenar; `LiveBlocked` exige re-init.
- Teto: nada sai de `SubmissionPrepared` no P80 (toda op real → NotPermitted).

## Provas

`test-production-phase` (5 grupos): happy-path 0→9, skips/reentradas ilegais,
teto+terminal, mapa de teardown por estado, alvo Attached.
