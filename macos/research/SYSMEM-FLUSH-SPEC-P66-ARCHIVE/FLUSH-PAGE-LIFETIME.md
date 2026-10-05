# P66 FLUSH-PAGE-LIFETIME — dedicated page vs selector7 + IODMA model + conteúdo + lifetime

## 1. Decisão (§6)

```text
FLUSH_PAGE_MODEL = B (dedicated flush page)
REUSE_SELECTOR7_MAPPING = NO

DEDICATED_FLUSH_PAGE_SIZE = 4096
DEDICATED_FLUSH_PAGE_ALIGNMENT = 4096
DEDICATED_FLUSH_PAGE_DIRECTION = coherent, kIODirectionInOut-equivalente (não streaming; GPU escreve handshake, host mantém coerência)
DEDICATED_FLUSH_PAGE_SEGMENT_REQUIREMENT = exatamente 1 segmento de 4096
```

### Por que NÃO reutilizar selector7 (opção A rejeitada)

Selector7 existente (`PREPARE_GSP_SYSMEM`, 1.5.3 live 1× ADDRESS_READY, mapping LEFT PREPARED):
- Lifetime: selector7 foi desenhado como foundation one-shot, deixado preparado; seu teardown/reuso não foi especificado para flush vitalício (flush deve viver até teardown dedicado pós-GSP, ver `SysmemFlush` Drop ordering em gpu.rs).
- Coherency: ambos exigiriam Coherent, mas compartilhar implicaria alias de ownership (quem desregistra? flush Drop checa `read()==minha_pagina`, reusar quebraria essa invariante).
- Direction: selector7 é genérico sysmem; flush tem semântica sysmembar específica (GPU-initiated barrier). Acoplar confunde auditoria.
- Reuse safety: reuso violaria one-variable-at-a-time (misturaria foundation já validada live com primeiro write high-risk no mesmo objeto).
- Teardown: flush precisa `write(0,0)` só se ainda dono; selector7 não tem esse contrato.
- Future DMA: GSP boot precisa de vários outros buffers (FWSEC, radix3, wpr_meta, libos, cmdq); fixar flush no mesmo objeto impede evolução independente.
- Clarity: auditor (Astra) precisa de objeto com nome/propósito único.

Opção C (outro objeto específico que não seja página Coherent dedicada) rejeitada: upstream exige `CoherentHandle::alloc(PAGE_SIZE)` — página sysmem coerente, sem estrutura especial.

## 2. IODMACommand / IOMMU model (§7, spec apenas)

```text
FLUSH_DMA_MAPPING_MODEL = kernel-owned IOBufferMemoryDescriptor(Coherent) + IODMACommand(V2, mapper físico/IOMMU) → IOVA device-visible de 1 segmento; sem exposição a userspace
```

- Descriptor type: `IOBufferMemoryDescriptor` com `kIOMemoryPhysicallyContiguous` + coerente (equivalente a `CoherentHandle::alloc(PAGE_SIZE)`; no 1.5.3 já usado para selector7 — reutilizar o *padrão*, não o *objeto*).
- Prepare: `descriptor->prepare()` antes de criar DMA command; `complete()` só no teardown dedicado.
- DMA command lifecycle: `IODMACommand::withSpecification(kIODMACommandOutputHost64, 64-bit, ...)`; `setMemoryDescriptor`; `genIOVMSegments`/`genSegment` para extrair IOVA; `clearMemoryDescriptor` no teardown.
- Mapper selection: mapper do provider PCI (IOMapper/DMAR); nunca endereço físico CPU (`never use physical address`).
- Segment extraction: exigir `segmentCount==1`, `segmentLength==4096`; qualquer outro → abortar sem write.
- Address width: 64-bit; validar contra DMA mask do chipset + roundtrip encode (ver ENCODING.md).
- Error cleanup: em qualquer falha antes do COMMIT, `complete` + release descriptor + command, sem MMIO. Depois do COMMIT, página permanece pinned (ver §5 COMMIT).
- Nenhum endereço (IOVA, kernel VA, físico) exposto via ABI; só semântica (pageReady, segmentCount, alignment, encodingReady).

## 3. Conteúdo da página (§8)

```text
FLUSH_PAGE_INITIAL_CONTENT = ZERO-FILLED, sem sentinel/contador/estrutura
```

Upstream (`SysmemFlush::register`) apenas aloca página coerente, não escreve nada nela antes de registrar. É barreira HW (sysmembar drena writes PCIe pendentes), não mailbox de dados. Zero-fill é higiene (evita leak + determinismo), não requisito HW documentado. Não inventar sentinel.

## 4. Lifetime / stop (resumo; detalhe em HILO-COMMIT-ATOMICITY.md)

- Owner: provider/UserClient com pin + retain (lições 1.6.2): reserve operação semântica antes de tocar provider, retain provider, own map/mapping/descriptor/command localmente.
- Página + mapping vivos desde PREPARE (Gate A alloc) até teardown dedicado pós-GSP ou reboot. `close()` não libera; `stop()` drena active-calls e espera COMMIT crítico terminar (sem novo MMIO após stop retornar).
- Drop/unregister futuro: `write(0,0)` SOMENTE se `read()==minha_pagina` (igual Nova Drop); senão warn + não tocar (outra página assumiu — bug a investigar, nunca "corrigir" com write cego).
