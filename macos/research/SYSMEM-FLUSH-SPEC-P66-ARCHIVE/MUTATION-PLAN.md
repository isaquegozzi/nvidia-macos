# P66 MUTATION-PLAN — requirements futuros (§21, sem implementar mutations agora)

Futura implementação deve demonstrar que tests/audits PEGAM cada mutação abaixo (kill rate esperado 100% para lista crítica).

## 1. Lista crítica

- M1 HI/LO reversed (LO antes de HI): teste de ordem + auditoria textual devem FALHAR (desvio de upstream; strength=implementation, mas ainda desvio).
- M2 LO-only (remove HI store): teste exact-count (1 vs 2) + roundtrip >40bit devem FALHAR.
- M3 HI-only (remove LO store): idem M2.
- M4 wrong shifts (`>>4`, `>>12`, `>>32`, `>>40` sem máscara, `as u16`): roundtrip deve FALHAR.
- M5 address exposed (IOVA/HI/LO em ABI ou log): scan de ABI + grep de `dma_address` em UserClient out deve FALHAR.
- M6 arbitrary write API (offset/value de userspace): fuzz + auditoria de assinatura devem FALHAR.
- M7 extra MMIO store (3º write_reg): contador mockado deve FALHAR.
- M8 PCI config write (`setBusLeadEnable`, Command write): grep estático + contador PCI devem FALHAR.
- M9 BME enable em Gate B: grep + estado BME mockado devem FALHAR.
- M10 DMA trigger (`dma_load`, TRANSCFG/DMATRFBASE/CMD, doorbell): grep + contador DMA devem FALHAR.
- M11 Falcon reset / firmware action (`Falcon::new/reset/boot`, `gsp.boot`, FWSEC/Booter): grep deve FALHAR.
- M12 selector7 call interna: stub de selector7 que registra chamada deve FALHAR se chamado.
- M13 readback removido mas `readbackMatch=1` forjado: teste de mismatch injetado deve FALHAR.
- M14 retry automático (loop em torno de writes): contador + auditoria de loop devem FALHAR.

## 2. Requisitos do harness futuro

- Cada mutação é um diff mínimo aplicado ao código candidato, com teste correspondente que PASSA no código correto e FAILS no mutante.
- Dois falso-positivos conhecidos de 1.6.1 (store idêntico; sel7 não compilava) não devem se repetir: mutações devem compilar e alterar comportamento observável.
- Relatório de mutação com kill/survived por ID, sem esconder sobreviventes.

```text
MUTATION_PLAN_READY = YES (spec pronta; execução pendente)
```
