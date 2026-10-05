> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# P80 — Production Architecture (offline, 1.8.0)

Author: Muse, 2026-09-09 -0300. Tree: `GA106Lab-kext8-production-architecture-src/`
(`TG-KEXT8-PRODUCTION-ARCHITECTURE`, 1.8.0). Cópia exata do 1.7.3 + camada P80.
NÃO live-ready: sem stage, sem load, sem EFI. Leia com `P80-STATE-MACHINE.md`,
`P80-LIFETIME-SAFETY-AUDIT.md`, `P80-HARDWARE-BLOCKS.md`, `P80-OPEN-QUESTIONS.md`.

## 1. Separação PREPARED / READY / ARMED / LIVE-AUTHORIZED (§3)

- `PREPARED`: fases 4/6/7/8/9 (sysmem, precondições, channel/VM/submission models).
- `READY`: portas de entrada validadas (contratos + ordem). Nada executa.
- `ARMED`: **não existe estado armado no P80** — não há transição que arme HW.
- `LIVE-AUTHORIZED`: **inexistente no P80** — todo stub retorna NotPermitted e
  trava em `LiveBlocked`. Futura autorização exigirá milestone próprio + review.

## 2. Componentes novos (3 headers, estilo do projeto)

- `GA106LabProductionPhase.h` — máquina de estados pura (kernel+host).
- `GA106LabHardwareBlocks.h` — 13 stubs fail-closed + ledger (só IOReturn).
- `GA106LabModelContracts.h` — contratos Channel/Vm/Submission + cross-check
  P77+P78+P79 (puros; simuladores como oracles nos testes, sem cópia de lógica).

## 3. Fiação mínima no serviço (sem reestruturar fluxos provados)

- `init`: fase Detached + ledger zerado + contratos inválidos.
- `start` OK → Attached. `stop`: teardowns existentes + reset de fase para
  Attached + descarte de contratos. `clientClosed`: inalterado (não libera).
- Milestones 2..6 anotados nos wrappers existentes (best-effort, sem mudar selectors).
- `prepareChannelModel/Vm/SubmissionModel`: validam contrato + ordem; zero HW.
- `blockRealOperation(0..12)`: stub + latch LiveBlocked; sempre NotPermitted.
- Nenhum selector novo (`Count=10` auditado); ABI intacta (sem leaks, auditado).

## 4. Concurrency (P75/P76 preservadas)

Reserva `busy` por domínio, flags `stopping` terminais, seções curtas sob spinlock,
polls destravados, dreno sem lock retido. Prova: `test-production-concurrency`
(pthreads) + TSAN + grupos de reserva/dreno no lifetime.

## 5. Testes (5 suítes novas, todas no build)

`test-production-phase` (5) + `test-production-blocks` (3) +
`test-production-contracts` (3, com oracles P77/P78/P79) +
`test-production-lifetime` (5) + `test-production-concurrency` (2, pthreads).
Mutations: `mutation-production-harness.sh` 4/4 CAUGHT (+ 16/16 + GFW + flush
herdados). Sanitizers: ASAN/UBSAN nas novas suítes; TSAN na concorrência.
`clang --analyze` (relatado no pacote Gemini).

## 6. Build / reproducibilidade

`build.sh` estendido (mesmos padrões 1.7.3 + auditorias P80). `BUILD_OK` exige
compilação, todos os testes, harnesses e auditorias. Manifest
`MANIFEST-P80-1.8.0.txt` com hashes de fontes + artefatos; rebuild em cópia
`/tmp` pelo método P64. Versão consistente plist×2 + protocolo, sem resíduo 1.7.3.
