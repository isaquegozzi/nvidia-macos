# P83 — Plano de invocação GetVersion (1x, sem loops, sem retry)

```text
SESSION_ID: V4-P83-GETVERSION / LIVE_EXECUTION_AUTHORIZED = NO (este plano NÃO autoriza; executa só após APPROVE)
SELECTOR = 0 / CALLS = 1 / RETRY = 0 / LOOPS = 0
```

## Sequência (ordem fixa, parar no 1º FAIL/ABORT)

| Passo | Ação | Bound | Evidência |
|---|---|---|---|
| T+00 | Snapshot pré: `kextstat` (GA106Lab 1.8.0 idx único) + `date` | read-only, ≤30s | salvar bruto |
| T+01 | `open` via caller pinado `~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl` (`openService`, `IOServiceOpen` em `ga106ctl.c:40`); pré-gate hash FULL conferido (ver Gate pré-live) | 1 tentativa, timeout 5s, sem retry | `kr == 0` senão ABORT |
| T+02 | GetVersion 1x via `~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl version` (`P83-PRIVILEGE.md` item 4): `selector 0`, scalar-in 0, scalar-out 2 | 1 chamada, timeout 5s, sem retry/loop | `kr == 0` + `scalar == [1, 0x00010800]` + stdout `protocol: 1` / `driver: 0x00010800` |
| T+03 | `close` incondicional via caller pinado (`IOServiceClose` em `ga106ctl.c:66`) | 1 tentativa, timeout 5s | `kr == 0` senão registrar + ABORT |
| T+04 | Snapshot pós: `kextstat` + panic-scan (`ls DiagnosticReports`, sem `.panic` novo) | read-only, ≤30s | diff vs T+00 |
| T+05 | SAVE + STOP: arquivar brutos, encerrar; nenhuma ação adicional | — | pacote de evidência |
NOTA (F4 SMOKE-05, 2026-09-09): T+01/T+02/T+03 são FASES INTERNAS de UM único processo `ga106ctl version` (open+call+close em `ga106ctl.c:54-74`), não 3 invocações separadas; `CALLS = 1`.


## Gate pré-live (hash FULL 64-hex, F2)

APPROVE exige ANTES de T+00, sem exceção: `shasum -a 256 ~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl` com igualdade byte-for-byte contra `9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840` (64 hex; sem prefixo, sem elipse); sem match FULL → ABORT antes de T+00, nenhum passo live executa.

## Regras duras

- Sem loops, sem retry, sem fallback para outro selector, sem re-open após FAIL.
- Nada além de open/selector-0/close + snapshots read-only (sem `kmutil`, sem `ioreg -w`, sem MMIO/PCI-write/BME/DMA/GSP/VRAM/IRQ/reset).
- Qualquer `kr != 0`, resposta ≠ `[1, 0x00010800]`, `.panic` novo, hang ou `close != 0` → FAIL + STOP (T+05 direto).

## PASS iff (tudo, sem exceção)

```text
PASS = open OK + resposta [1, 0x00010800] + close OK + sem panic/hang
```

```text
live = 0 (plano publicado; execução SOMENTE após review APPROVE)
```
