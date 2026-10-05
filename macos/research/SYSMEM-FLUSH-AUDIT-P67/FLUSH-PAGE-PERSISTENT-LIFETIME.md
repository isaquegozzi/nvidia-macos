# P67 FLUSH-PAGE-PERSISTENT-LIFETIME — owner e persistência (corrige P66)

Fontes: `nova-core-fb.rs` (SysmemFlush doc/register/Drop) + `nova-core-gpu.rs` (ordem + field order).

## Vereditos

```text
FLUSH_PAGE_REQUIRED_LIFETIME = desde SysmemFlush::register (pos-GFW, pre-qualquer Falcon reset) ate driver unload / teardown dedicado futuro; persiste durante FWSEC-FRTS, GSP reset/boot, Booter via SEC2, RPCs e sequencer; ate reboot/reset para known-good se teardown dedicado nao existir
FLUSH_PAGE_MAPPING_REQUIRED_LIFETIME = igual; allocated+prepared+DMA-mapped+IOVA stable continuamente; sem complete/release apos Gate B
FLUSH_PAGE_OWNER = provider/service dono do objeto tipo SysmemFlush (CoherentHandle + BAR + chipset/device), pinned+retained, reserve antes de provider access (licoes 1.6.2)
CAN_RELEASE_AFTER_GATE_B = NO
```

## Distinções

- selector call lifetime (microssegundos da syscall) ≠ KEXT/service lifetime ≠ GPU bring-up lifetime. Flush pertence às duas últimas, não à primeira. Gate B retorna mas objeto permanece.
- `close()` não libera; `stop()` drena active-calls e espera COMMIT, mas não completa/desmapeia a página.
- IOVA mapping early-release é bug crítico futuro (mutation `mapping released too early` deve FAIL): sysmembar posterior usaria IOVA inválida durante handshake Falcon → timeout.
- Desprogramar registradores (write 0) é separado de liberar página; ambos NÃO autorizados em Gate B (`TEARDOWN_WRITES=0`, `ROLLBACK=NOT_AUTHORIZED`).
- Conteúdo: `ZERO_FILL_REQUIREMENT_STRENGTH=DEFENSIVE_LOCAL_POLICY`; `FLUSH_PAGE_INITIAL_CONTENT_POLICY=zero-fill antes de mapear, sem sentinel` (P66 corrigido: não é requisito HW).
