# P67 P66-CONTRADICTION-AUDIT — endurecimento offline (read-only HW/projeto)

Inputs lidos: P66 `.../SYSMEM-FLUSH-SPEC-P66/` (10 docs) + P65 `GSP-NEXTSTEP-P65/` + snapshots `UPSTREAM-EVIDENCE/` + handoffs. Nada modificado nos inputs.

## A — Rollback / unregister (0,0) — CONTRADIÇÃO CONFIRMADA E CORRIGIDA

- P66 dizia `ROLLBACK_MMIO_WRITE = NOT_AUTHORIZED` e ao mesmo tempo `... unregister (0,0) se dono`.
- Auditoria P65 (`nova-core-fb.rs` Drop): `if read()==dma { write(0) } else { warn }` — `write(0)` chama `write_sysmem_flush_page(bar,0)` = 2 stores (HI 0x100c40 + LO 0x100c10).
- Veredito:

```text
UNREGISTER_00_IS_MMIO_WRITE = YES (2 MMIO writes)
UNREGISTER_00_SOURCE_SEMANTICS = Nova Drop cleanup condicional, apenas teardown de driver, nao parte do bring-up nem do first-write gate
FIRST_WRITE_GATE_TEARDOWN_MMIO_WRITES = 0
ROLLBACK_MMIO_WRITE = NOT_AUTHORIZED
AUTOMATIC_UNREGISTER_MMIO_WRITE = NO
```

- Correção: remover qualquer zeroing/restauração do Gate B e do stop automático. Página/mapping permanecer viva é separado de desprogramar registradores. Futuro teardown com MMIO, se algum dia proposto, é gate separado com autorização própria + Astra, nunca implícito.

## B — Lifetime — SUBESPECIFICADO EM P66, CORRIGIDO

`fb.rs` + `gpu.rs`: `SysmemFlush::register` antes de `GspResources`; struct declara `gsp_resources` antes de `sysmem_flush` para que Drop do GSP ainda tenha página. Doc: "must be registered as early as possible, before any falcon is reset".

```text
FLUSH_PAGE_REQUIRED_LIFETIME = desde pre-Falcon (pos-GFW) ate driver unload / teardown dedicado futuro (persiste durante todo boot GSP, ate reboot/reset)
FLUSH_PAGE_MAPPING_REQUIRED_LIFETIME = igual, IOVA estavel, prepared+mapped continuamente
FLUSH_PAGE_OWNER = provider/service dono do objeto equivalente a SysmemFlush (pinned+retained)
CAN_RELEASE_AFTER_GATE_B = NO
```

Gate B não faz cleanup que invalide IOVA.

## C — Read count ambíguo — CORRIGIDO

P66: `0 ou 2`. Operação fixa não pode ser ambígua.

```text
EXPECTED_MMIO_WRITE_COUNT = 2
EXPECTED_MMIO_READ_COUNT = 2
Política: HI write, LO write, HI read, LO read (ordem de readback igual à de composição, sem retry)
```

Motivo: readback é suportado (`read_sysmem_flush_page_ga100`), confirma programação, custo mínimo (reads), sem risco adicional. Fixa em B.

## D — BME known vs OFF — CORRIGIDO PARA DETERMINÍSTICO

P65 BME-TIMING: `BME_REQUIRED_BEFORE_CPU_MMIO_WRITE=NO`, `BEFORE_FIRST_GPU_DMA=YES`; live Command 0003 prova CPU MMIO sem BME. Nova faz `set_master` em probe (antes de GFW), mas nosso experimento quer isolar primeiro store de capacidade DMA.

```text
PREWRITE_BME_REQUIREMENT = BME==OFF (Gate A exige OFF, nao apenas "conhecido")
FIRST_WRITE_WITH_BME_OFF = YES
```

Sem conflito de fonte para programar SYSMEM_FLUSH com BME OFF (CPU store requer MSE/BAR0, não BME). Se fonte futura exigir BME → parar para Astra.

## E — HI/LO==0 — REBAIXADO DE REQUISITO PARA DIAGNÓSTICO

Upstream não documenta "must be zero"; zero é baseline esperado (reset) + invariante de ownership do Drop. Transformar em requisito HW seria hipótese nossa.

```text
PREEXISTING_FLUSH_REGISTER_ZERO_REQUIRED = NO
PREEXISTING_FLUSH_REGISTER_POLICY = Gate A le/reporta valores atuais como diagnostico; se !=0, nao prosseguir automaticamente, sem overwrite, sem restore; exigir review separado humano+Astra
```

## F — WPR2 absent — REBAIXADO PARA DIAGNÓSTICO

`wpr2_range().is_none()` é exigido antes de `run_fwsec_frts` (EBUSY senão), não antes do flush. Flush é anterior a qualquer Falcon; WPR2 é estado de bring-up posterior.

```text
WPR2_ABSENCE_REQUIRED_FOR_FLUSH_PROGRAMMING = NO (nao e HW requirement para o register write)
WPR2_PREWRITE_POLICY = diagnostico em Gate A; se presente, nao prosseguir sem review separado (indica estado previo, precisa reset fora de escopo)
```

Evita leituras/dependências desnecessárias no gate crítico, mantém segurança por política explícita.

## G — Write order — MANTIDO COMO FIDELIDADE

```text
HILO_HARDWARE_ORDER_REQUIREMENT = NOT_PROVEN (UNKNOWN se latch em LO)
HILO_IMPLEMENTATION_POLICY = HI then LO por fidelidade a write_sysmem_flush_page_ga100
REVERSE_ORDER_MUTATION_MEANING = fidelity test, nao prova de invalidez HW
```

## H — Partial — MANTIDO UNKNOWN + ENDURECIDO

```text
PARTIAL_HILO_SEMANTICS = UNKNOWN
CAN_STOP_ABORT_BETWEEN_HI_LO = NO
```

PREPARE abortável; COMMIT (HI,LO,readback) não abortável no meio; stop espera drain; sem alloc/sleep/retry/restore entre HI e LO.

## I — Zero-fill — RECLASSIFICADO

Upstream só faz `CoherentHandle::alloc(PAGE_SIZE)`, sem sentinel. Zero é higiene local.

```text
ZERO_FILL_REQUIREMENT_STRENGTH = DEFENSIVE_LOCAL_POLICY
FLUSH_PAGE_INITIAL_CONTENT_POLICY = zero-fill antes de mapear, sem contador/sentinel/estrutura; documentar como política local, nao requisito HW
```

## J — Selector7 — RECONFIRMADO

```text
SELECTOR7_RUNTIME_DEPENDENCY = NO
REUSE_SELECTOR7_MAPPING = NO (motivo: lifetime/ownership, nao conveniencia)
Gate A/B MUST NOT chamar selector7 internamente nem depender de invocação prévia via userspace
```
