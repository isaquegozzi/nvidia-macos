# P66 FIRST-WRITE-GATE-B-SPEC — semantic fixed command (§11–§14 + §22)

## 1. Nome

```text
FIRST_WRITE_GATE_B_NAME = PROGRAM_SYSMEM_FLUSH_PAGE
(alternativa aceitável: REGISTER_GSP_SYSMEM_FLUSH_PAGE; fixar em implementação futura com justificativa)
```

## 2. Semântica fixa (sem implementação neste prompt)

Operação kernel-only, zero-input:

1. Usa dedicated coherent page já validada em Gate A (ou aloca+valida de novo se Gate A não deixou pinned — preferir revalidar tudo).
2. Deriva IOVA internamente (nunca de userspace).
3. Encoda HI/LO internamente + roundtrip check.
4. `write HI @0x100c40` (u32, valor encodado).
5. `write LO @0x100c10` (u32, valor encodado), back-to-back, sem sleep/wait entre eles.
6. Opcional immediate readback: `read HI, read LO`, recompor, comparar com IOVA.
7. Retorna SÓ status semântico (abaixo). Página permanece pinned.

Userspace NÃO fornece: address, offset, value, width, BAR, BDF, timeout, HI/LO, ordem.

## 3. Write/read counts (§12)

```text
EXPECTED_MMIO_WRITE_COUNT = 2 (HI+LO, exatamente; sem retry)
EXPECTED_MMIO_READ_COUNT = 0 ou 2 (0 sem readback, 2 com readback HI+LO; reads não contam como writes)
```

Nenhum retry automático. Mismatch de readback → FAIL + teardown dedicado (não reescrever).

## 4. BME (§13)

```text
FIRST_WRITE_WITH_BME_OFF = YES (preferido)
```

- CPU MMIO write não requer BME (P65 BME-TIMING + Nova `enable_device_mem` vs `set_master` separados; BME é para DMA iniciado pelo device, não para store da CPU no BAR).
- Isolar primeiro store de capacidade DMA: manter BME OFF prova que Gate B não depende nem habilita DMA.
- Se fonte futura exigir BME antes deste write → marcar CONFLITO e parar para Astra, não ligar BME por conta própria.
- Gate B NUNCA liga BME; BME permanece como gate separado futuro.

## 5. NO GPU DMA (§14)

```text
FIRST_WRITE_GATE_GPU_DMA = NO
```

Gate B executa: 0 Falcon reset, 0 FBDMA (FBIF_TRANSCFG/DMATRFBASE/BASE1/MOFFS/FBOFFS/CMD), 0 GSP boot, 0 firmware load, 0 doorbell, 0 interrupt enable, 0 DMA trigger — mesmo com IOVA mapeado. Auditoria futura (estática + binary): grep de símbolos (`Falcon::`, `dma_load`, `boot(`, `doorbell`, `setBusLeadEnable`, `write_reg` além dos 2 fixos) deve retornar vazio exceto os 2 stores autorizados.

## 6. ABI (§22)

```text
FIRST_WRITE_ABI = v1 48B semântico:
  u32 abiVersion, u32 status, u8 phase(=B), u8 gfwReady, u8 mseEnabled, u8 bmeEnabled,
  u8 pageReady, u8 segmentCount(=1), u16 alignment(=4096), u8 encodingReady,
  u8 writeCount(=2), u8 readbackMatch(bool), u8 reserved[] zero
```

NÃO expõe: dmaAddress, hiEncodedValue, loEncodedValue, MMIO addresses, kernel pointers. Se futura implementação alegar necessidade → parar e justificar para Astra em vez de expor por padrão.

## 7. Pré/pós-condições

- Pré: Gate A PASS na mesma boot-sessão + revalidação imediata (GFW ainda FF, WPR2 ainda ausente, página ainda pinned, stop não pendente).
- Pós: `read()==dma` (se readback), só 0x100c10/0x100c40 alterados (prova por diff read-only de 0x100c00–0x100c80 + WPR2 + Command inalterados), página pinned vitalícia.
- Timeout: alloc limitada monotônica; writes são stores únicos sem poll; sem espera de firmware.
