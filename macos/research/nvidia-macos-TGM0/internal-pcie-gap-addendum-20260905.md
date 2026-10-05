# Addendum TGM0-macOS (2026-09-05, Tahoe 25G83) — `ioreg` vivo: RTX HIDDEN, A/B/C ainda em aberto

> Sessão macOS Tahoe 26.6.2 (25G83) MacPro7,1, SOMENTE LEITURA. Doc-fonte:
> `docs/macos/hardware/ga106-ioreg.md` (novo, desta sessão) + snapshots sanitizados
> (repo NTFS read-only → crus em `/tmp/macos-tgm0/`, sanitizados em
> `~/Documents/nvidia-macos-TGM0/artifacts-macos/`; copiar para `artifacts/macos/` quando gravável).
> Nenhum código, nenhum NVRAM/OpenCore alterado, nenhum dext instalado.
> `python3` indisponível (sem CLT) → `tinygpu-match-check` via port Node fiel (`--self-test ALL PASS`).

## Resultado

**A/B/C permanecem TODOS em aberto. Motivo novo e bloqueador: RTX `10de:2504` = HIDDEN
(não BARE), logo nenhum teste de matching foi possível.**

| Resultado | Status após `ioreg` vivo 2026-09-05 | Porquê (evidência) |
|---|---|---|
| A — tunneled casa | Em aberto (`LIKELY`, sem observação nova) | Sem HW tunneled nesta bancada; `IOPCITunnelCompatible=Yes` em 11 personalities do sistema, `IOPCITunnelled` global = 0. Nada muda. |
| B — interna vs `IOPCITunnelCompatible` | Em aberto (`UNKNOWN`, intestável) | Exige nó interno BARE para testar se o dext shipped casa não-tunneled. Nó ausente (HIDDEN) → B não testado. Tunnel segue informativo apenas (Correção TGM0). |
| C — interna vs `built-in` | Em aberto (`UNCERTAIN` para a RTX; mecanismo reforçado pelo controle AMD) | RTX: `built-in`/`tunnelled` = `UNKNOWN` (sem nó). Controle AMD `1002:1638`: `built-in=<00>` presente + `AAPL,slot-name=built-in` → `match-check LIKELY_NO_MATCH` na perna `built-in` (sem `...builtin` na shipped). Isso sustenta o mecanismo C sem decidir a RTX. |
| H0 — RTX HIDDEN por `-wegnoegpu` (novo, fora de A/B/C) | `CONFIRMED` como bloqueador de teste | `log show --last boot`: `Found 0x10de:0x2504 (030000)` + `Found 0x10de:0x228e` → `bridge 0:1:1 removing child` ambos. `ioreg -c IOPCIDevice`: 0 nós `10de`; `SPPCIDataType`: só AMD; `GPP0@1,1` com filho vazio `D004@ff`. `boot-args` literal `-wegnoegpu` (com `e`; corrigir HANDOFF que cita `-wegnogpu`). `kextstat`: `Lilu 1.7.3` sem `WEG`, mas remoção ocorreu (mecanismo exato exige `config.plist`, EFI não montada aqui). |

## `match-check` (resumo honesto)

- Observado HIDDEN (`10DE:2504`, `built-in UNKNOWN`, `tunnelled UNKNOWN`) → `INDETERMINATE`
  (`gate de produção não decidível`). Não é `NO_MATCH` (nada prova filtro) nem `MATCH`.
- Controle AMD (`1002:1638`, `built-in YES`) → `LIKELY_NO_MATCH` (perna `built-in`).
  Sustenta `docs/tinygpu/amd-igpu-match-risk.md = RISK`: **não instalar a DEXT**.
- Hipótese BARE (`ABSENT/ABSENT`) → `LIKELY_MATCH`. Prova que o bloqueador atual é o
  HIDDEN, não classe/vendor/entitlement (`0x2504` passa em classe `0x03` + allowlist `[10de,1002]`).

## `MAC-COMPUTE-0` recalculado: permanece `LOW` (com bloqueador H0 explícito)

**Veredito: `LOW` (inalterado desde ADR-0002 §8.3).** Não sobe (sem BARE, sem `GSP_INIT_DONE`,
sem `BOOT_0`, sem `Start_Impl` vivo) e não desce para `BLOCKED` (HIDDEN é reversível em
princípio — remover a flag —, não impossibilidade provada). Leitura honesta:
`LOW-com-H0` = “hipótese testável com plano conhecido, mas intestável até o UM fechar”.

Três incertezas de §8.3 permanecem + H0 como pré-condição:

1. **H0 (nova, bloqueia A/B/C):** tornar a RTX BARE sem perder o boot (UM em `ga106-ioreg.md` §7).
2. **Matching vivo (B `UNKNOWN` + C `UNCERTAIN`):** só decidível com nó BARE + `built-in`/`tunnelled` vivos.
3. **FW/ctx GA106-como-`GA1` + I/O real:** inalterados (0% validação).

## O que falta (não executar sem revisão)

1. Ler `EFI/OC/config.plist` read-only (SSDT/`DeviceProperties`/`boot-args`) para caracterizar H0.
2. Plano descartável sem `-wegnoegpu` (monitor só placa-mãe, IGD primário, recovery pronto).
3. Se BARE: repetir `ga106-ioreg-runbook.md` e reavaliar B/C; só então discutir fork lab.
