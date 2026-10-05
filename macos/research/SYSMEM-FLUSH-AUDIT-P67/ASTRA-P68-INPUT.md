# P67 ASTRA-P68-INPUT — pacote conciso para futura auditoria (Astra indisponível agora)

## P65 conclusions (primárias)

- Primeiro write pós-`GFW_BOOT==0xFF` = SYSMEM_FLUSH HI 0x100c40 + LO 0x100c10 (NV_PFB_NISO_FLUSH_SYSMEM_ADDR_HI/ADDR), página Coherent PAGE_SIZE, `LO=(dma>>8)`, `HI=(dma>>40)`, ordem código HI→LO, readback `(LO<<8)|(HI<<40)`, GA106 via GA102→ga100 path; LO-only legado rejeitado. Fontes: fb-regs.rs, fb.rs SysmemFlush, fb-hal-ga100.rs/ga102.rs, gpu.rs ordem.
- `BME_REQUIRED_BEFORE_CPU_MMIO=NO`, `BEFORE_FIRST_GPU_DMA=YES`; MSE ON/BME OFF (0003) permite CPU MMIO.
- Sequencer pós-flush é data-driven (GSP decide RegWrite/Modify via cmdq) — nenhum "segundo write fixo" offline além do flush.

## P66 original (onde estava)

- 10 docs em `.../SYSMEM-FLUSH-SPEC-P66/`; dedicated page (não reuso selector7); Gate A selector9 + Gate B PROGRAM_SYSMEM_FLUSH_PAGE; 2 writes; BME OFF preferido; GPU DMA NO; commit PREPARE/COMMIT/CLEANUP; rollback NOT_AUTHORIZED; testes/mutações spec.

## P67 corrections (o que mudou e por quê)

1. Rollback: `UNREGISTER_00_IS_MMIO_WRITE=YES`; `TEARDOWN_WRITES=0`; `AUTOMATIC_UNREGISTER=NO` — Drop do Nova é teardown de driver, não parte do Gate B.
2. Lifetime: `CAN_RELEASE_AFTER_GATE_B=NO`; owner provider pinned; IOVA estável até unload/teardown dedicado.
3. Reads fixados: `WRITES=2, READS=2` (HI,LO + HI read,LO read), sem ambiguidade.
4. BME determinístico: Gate A exige `BME==OFF`; Gate B com BME OFF.
5. HI/LO!=0 e WPR2: rebaixados para diagnóstico (não requisito HW); política: sem auto-prosseguir, sem overwrite/restore, review separado.
6. Ordem: `NOT_PROVEN` HW; política HI→LO por fidelidade; reverso = fidelity test.
7. Partial UNKNOWN; `CAN_STOP_ABORT=NO`; pós-commit FAIL sem mais writes; risco residual não-atômico declarado.
8. Zero-fill = política local defensiva, não HW.
9. Selector7 dependency NO; Gate A/B nunca chamam selector7.
10. Test/mutação corrigidos: rollback-to-zero must FAIL; early-release must FAIL.

## Open questions (para Astra/P68)

- Ordem de readback HI-LO vs LO-HI tem requisito? (P67 fixa HI,LO por simetria; confirmar.)
- Layout ABI 48B exato (padding/ sheer size) — travar na implementação.
- Teardown dedicado futuro com MMIO (se algum dia): gate separado ou reboot-only permanente?
- DMA mask efetivo GA106 no macOS para range check (complementa representabilidade 8..63).

## Exact risk boundary

Permitido neste P67: só docs em P67 workspace. NADA de: KEXT/EFI/stage/reboot/load/selectors live/MMIO live/PCI write/BME/DMA/Falcon/GSP/firmware, nem implementação de Gates. Próximo passo exige: selector8 live PASS → autorização → Gate A → review → Astra → autorização → Gate B → stop.
