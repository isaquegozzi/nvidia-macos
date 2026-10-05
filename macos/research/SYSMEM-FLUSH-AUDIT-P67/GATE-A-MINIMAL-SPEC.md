# P67 GATE-A-MINIMAL-SPEC — VERIFY_SYSMEM_FLUSH_PRECONDITIONS (read-only, zero-write)

## Nome/número/ABI

```text
GATE_A_REQUIRED_CHECKS = provider alive; GA106 exact (10de:2504/a1/nv176); BAR0 mapped+readable e len>=0x100c44; MSE ON; GFW Completed (0xFF via selector8); BME==OFF; dedicated coherent page allocated/prepared/mapped; exactly 1 segment 4096; IOVA 256-aligned; IOVA representable bits 8..63; encoding roundtrip decode(encode)==IOVA e HI>>24==0; operation/lifetime healthy (retain possivel, stop nao pendente)
GATE_A_DIAGNOSTIC_ONLY_CHECKS = current HI/LO values; WPR2 state (0x1fa828 HI); BOOT0/BOOT42 estabilidade
SELECTOR_NUMBER = 9 candidato (nao fixar se conflitar)
ABI = v1 48B semantica, zero-input; sem IOVA/VA/fisico/HI/LO/MMIO addrs/ponteiros
```

REQUIRED protege Gate B; DIAGNOSTIC informa review humano mas nao e requisito HW. Se diagnostico inesperado (HI/LO!=0 ou WPR2 presente), Gate B nao prossegue automaticamente — exige review separado (politica E/F), sem overwrite/restore.

## Zero-write proof (design)

```text
GATE_A_MMIO_WRITES = 0
GATE_A_PCI_WRITES = 0
GATE_A_BME_CHANGES = 0
GATE_A_GPU_DMA = 0
+ Falcon reset = 0, GSP = 0, firmware = 0
```

- IODMA mapping (alloc/prepare/genSegments) NAO e GPU DMA: nenhum FBDMA programado, nenhum doorbell, nenhum `dma_load`, nenhum Falcon tocado. Mapping apenas obtem IOVA device-visible para validacao offline de encoding.
- Nenhum `write_reg`, nenhum `setBusLeadEnable/setBusMasterEnable`, nenhum Command write.
- Auditoria futura: contagem estatica + mock (0 stores) + mutation extra-write deve FAIL.
- Sem chamada interna a selector7; valida dedicated page própria (`SELECTOR7_RUNTIME_DEPENDENCY=NO`).
- Falha em qualquer REQUIRED → status semantico, sem retry, sem avançar.
