# P66 PREWRITE-GATE-A-SPEC — read-only gate antes do primeiro write (§9 + §10)

## 1. Nome e número

```text
PREWRITE_GATE_A_NAME = VERIFY_SYSMEM_FLUSH_PRECONDITIONS
PREWRITE_SELECTOR_NUMBER = 9 (candidato; não fixar se conflitar com 1.6.x numbering — selector8 já é READ_GFW_BOOT_READINESS)
PREWRITE_ABI = v1, 48B fixos, zero-input, só semântica (sem IOVA/VA/físico/BAR/offset)
```

## 2. Objetivo

Provar TODA pré-condição do write sem executar nenhum write (MMIO, PCI config, DMA trigger, firmware).

## 3. Requirements (todos read-only; qualquer FAIL → sem Gate B)

```text
PREWRITE_GATE_A_REQUIREMENTS =
  GFW_BOOT==0xFF (via selector8, sem releitura direta aqui se já provado na sessão; senão FAIL EAGAIN)
  + MSE ainda ON, BME estado conhecido (esperado OFF; se ON, registrar mas não alterar — Gate B prefere OFF)
  + BAR0 mapping válido e tamanho suficiente para 0x100c40 (checar comprimento, sem tocar regs além de leitura permitida)
  + HI/LO atuais legíveis SE semanticamente permitido (esperado 0 se nunca registrado; se !=0, FAIL EBUSY — já registrado, não reprogramar sem prova)
  + WPR2 ausente (HI_WPR2==0 via 0x1fa828 read-only; se presente → EBUSY, GPU precisa reset — fora de escopo)
  + dedicated flush page alloc+map OK (Coherent 4096, prepare, IODMACommand, 1 segmento 4096, alignment 4096)
  + IOVA cabe no encoding (aligned 256, roundtrip encode→decode == IOVA, HI>>24==0)
  + owner/provider lifetime healthy (retain possível, stop não pendente)
  + nenhum reset/load/boot Falcon executado nesta sessão após GFW (invariante de ordenação)
  + nenhuma chamada interna a selector7 (prova por auditoria de código; Gate A não chama selector7)
```

Leituras permitidas em A: GFW (via selector8), Command/MSE/BME (PCI config read), BAR0 len (sem map write), HI/LO (MMIO read — ainda é read; se política futura disser que até read de flush antes de programar é sensível, remover e marcar UNKNOWN), WPR2 HI (read), BOOT0/BOOT42 estabilidade (opcional, para detectar reset espúrio).

Nenhum `setBusLeadEnable`, nenhum `write_reg`, nenhum `dma_load`, nenhum doorbell.

## 4. ABI de saída (semântica apenas)

```text
struct PrewriteA_Out v1 (48B, packed, LE):
  u32 abiVersion (=1)
  u32 status (0 OK, EAGAIN/EIO/ENOMEM/EINVAL/EBUSY…)
  u8  phase (=A)
  u8  gfwReady (bool)
  u8  mseEnabled (bool)
  u8  bmeEnabled (bool)
  u8  pageReady (bool)
  u8  segmentCount (esperado 1)
  u16 alignment (esperado 4096)
  u8  encodingReady (bool, roundtrip OK)
  u8  writeCount (sempre 0 em A)
  u8  readbackMatch (n/a em A = 0)
  u8  reserved[rest zero]
```

PROIBIDO expor: raw IOVA, kernel VA, BAR VA, physical, HI/LO encoded values, MMIO addresses, kernel pointers, arbitrary register values/offsets.

## 5. Regras

- Zero-input (ou input fixo vazio); userspace não fornece address/offset/value/width/BAR/BDF/timeout.
- Sem retry automático; falha → retornar status, sem avançar.
- Auditoria futura deve provar: 0 MMIO stores, 0 PCI config writes, 0 BME changes, 0 DMA triggers em A (contagem estática + teste de mutation).
