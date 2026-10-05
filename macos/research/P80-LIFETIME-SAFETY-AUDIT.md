> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# P80 — Lifetime Safety Audit (ownership + teardown + panic context)

Author: Muse, 2026-09-09 -0300. Tree: `GA106Lab-kext8-production-architecture-src/` (1.8.0).

## 1. Panic context (sem atribuição de causalidade)

Histórico recente no macOS: `03:21 ipc ports use-after-free`, `10:13 _copyinmsg`
general protection. Sem evidência ligando ao GA106Lab (árvore 1.7.3 offline para o
candidato; live 1.5.3 estável). Por segurança, P80: zero teste live, zero KEXT
load/unload, zero EFI change — e endurece lifetime/teardown como abaixo.

## 2. Matriz de ownership (1.8.0 = 1.7.3 + camada P80)

| Objeto | Owner | Criação | Release | stop | clientClose | Reuso |
|---|---|---|---|---|---|---|
| `fSysmemDesc`/`fSysmemCmd` (+Flush) | serviço | selector 7/9 sob reserva | cleanup/stop template 1x | drena+teardown | NÃO libera | nunca entre domínios |
| `fSlotLock`/`fSysmemLock` | serviço | `InitOwnerLocksFlow` | `ReleaseOwnerLocksFlow` | intactos | intacto | — |
| `fActiveClient` | NÃO retido | `newUserClient` | `clientClosed` NULL | NULL sob trava | NULL se ativo | single-client |
| `fProductionPhase` + contratos | serviço | milestones/prepares | `resetProductionPhaseForTeardown` | → Attached | intacta | prepares re-validados |
| `fBlockLog` | serviço | `init` | nunca (ledger) | intacto | intacto | acumulativo |
| Futuro DMA-map handle | serviço (reservado) | — (bloqueado) | — | — | — | — |
| Futuros handles GSP/RM | serviço (reservado) | — (bloqueados) | — | — | — | — |

Regras herdadas e preservadas: UserClient NUNCA guarda desc/cmd/endereços
(`USERCLIENT_DMA_OBJECT_OWNERSHIP = NO`); ABI sem ponteiros/DMA (`= NO`);
`free()` só asserta + soltura defensiva; CleanupFailed retém de propósito
(controlled retention, nunca release inseguro).

## 3. Como a P80 reduz risco de UAF (resposta ao contexto de panics)

1. **Fase explícita**: teardown tem alvo determinístico (Attached) por estado —
   sem caminho que "esquece" domínio; `test-production-phase` prova o mapa.
2. **Contratos invalidados no teardown**: `resetProductionPhaseForTeardown` limpa
   `fChannelContractOk/fVmContractOk` — handles de modelo nunca sobrevivem ao dreno
   (anti-stale por construção; `test-production-lifetime` grupo 2).
3. **Ledger em vez de ponteiro pós-free**: auditoria de release usa contadores
   capturados antes do delete (o próprio teste prova sem UAF — o padrão antigo de
   segurar ponteiro foi removido do teste).
4. **Reserva nunca retida em poll longo**: `prepareOnce` libera o mutex durante a
   espera; `stop()` drena sem lock retido (disciplina P75/P76 preservada e testada
   com pthreads + TSAN).
5. **LiveBlocked terminal**: tentativa de HW trava o caminho até re-init — nenhum
   estado parcialmente "armado" sobrevive.
6. **ClientClose não libera, stop não compete**: reserva `busy` + flags `stopping`
   terminais por domínio; double-close/teardown idempotentes (provas nos grupos 1–2
   de lifetime + concurrency).

## 4. Provas

- `test-production-lifetime` (5 grupos): stop/close em todo estado, fault injection
  por estágio, no-double-release/stale (ledger), retry converge, reserva/dreno.
- `test-production-concurrency` (2 grupos) + TSAN: corrida prepare×stop×close,
  double-stop/close, start único.
- Build: `STOP_USES_SHARED_FLOW`, `FREE_CLEANUP_FAILED_SHARED_PATH`,
  `USERCLIENT_DMA_OBJECT_OWNERSHIP = NO`, `DMA_ADDRESS_EXPOSED = NO` (todos PASS).
