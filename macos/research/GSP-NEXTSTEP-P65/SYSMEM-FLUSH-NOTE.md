# P65 SYSMEM FLUSH — GA106 (prova nova, sem reutilizar numeros antigos)

## Conclusao

```text
SYSMEM_FLUSH_REQUIRED_FOR_GA106 = YES
SYSMEM_FLUSH_LO = 0x00100c10 (NV_PFB_NISO_FLUSH_SYSMEM_ADDR, adr_39_08)
SYSMEM_FLUSH_HI = 0x00100c40 (NV_PFB_NISO_FLUSH_SYSMEM_ADDR_HI, adr_63_40)
SYSMEM_FLUSH_SEMANTICS = pagina sysmem coerente registrada p/ operacao sysmembar (barreira HW
  iniciada pela GPU que drena writes PCIe pendentes p/ sysmem); sem ela, handshake de reset
  Falcon (que escreve na sysmem ao reconhecer reset) pode nao ficar visivel ao host => timeout.
  Deve ser registrada o mais cedo possivel, ANTES de qualquer reset Falcon.
```

## Prova primaria (Nova-core)

1. `nova-core-fb-regs.rs` (fb/regs.rs):
   - `NV_PFB_NISO_FLUSH_SYSMEM_ADDR(u32) @ 0x00100c10 { 31:0 adr_39_08; }`
   - `NV_PFB_NISO_FLUSH_SYSMEM_ADDR_HI(u32) @ 0x00100c40 { 23:0 adr_63_40; }`
   - Comentario: "Low/High bits of the physical system memory address used by the GPU to perform
     sysmembar operations (see crate::fb::SysmemFlush)".
2. `nova-core-fb.rs` (fb.rs), struct `SysmemFlush`:
   - "A system memory page is required for `sysmembar`... It is required for falcons to be reset
     as the reset operation involves a reset handshake. When the falcon acknowledges a reset, it
     writes into system memory. To ensure this write is visible to the host and prevent driver
     timeouts, the falcon must perform a sysmembar operation to flush its writes."
   - "must be registered as early as possible during driver initialization, and before any falcon
     is reset."
   - `register()`: `CoherentHandle::alloc(dev, PAGE_SIZE)` + `hal.write_sysmem_flush_page(bar, dma_address)`.
3. `nova-core-fb-hal-ga102.rs` (fb/hal/ga102.rs): Ga102 delega para ga100.
   `nova-core-fb-hal-ga100.rs` (fb/hal/ga100.rs):
   - `FLUSH_SYSMEM_ADDR_SHIFT_HI = 40`; write = HI com `(addr>>40)` cast, DEPOIS LO com `(addr>>8) as u32`.
   - read = `(LO.adr_39_08 << 8) | (HI.adr_63_40 << 40)`.
   - GA106 (Ampere nao-GA100) usa `ga102::GA102_HAL` (falcon_hal() + fb_hal()), logo usa este caminho
     de 2 registradores. Ordem de escrita no codigo: HI primeiro, LO depois.
   - Contraste: `nova-core-fb-hal-tu102.rs` (gm107): `FLUSH_SYSMEM_ADDR_SHIFT = 8`, apenas LO
     (`write_sysmem_flush_page_gm107`); suficiente so ate 40 bits. GA106 nao usa este caminho.
4. `nova-core-gpu.rs` (gpu.rs), `Gpu::new`:
   - Ordem: `dma_set_mask_and_coherent` -> `wait_gfw_boot_completion` -> `SysmemFlush::register`
     -> `GspResources` (Falcon::new + clear_swgen0 + gsp.boot).
   - `sysmem_flush` declarado DEPOIS de `gsp_resources` no struct para que o Drop do GSP ainda tenha
     a pagina ativa ("Must be kept declared *after* gsp_resources, as the latter's PinnedDrop
     implementation requires the sysmem flush page to be in place").
   - Logo: apos GFW_READY, os PRIMEIROS MMIO writes sao os do flush page (HI+LO).

## Formato do valor (GA106)

- LO recebe `(dma_addr >> 8) as u32` (bits 39:8; low 8 bits ignorados pelo HW).
- HI recebe `(dma_addr >> 40)` (bits 63:40, campo 23:0; na pratica ate ~48 bits de DMA mask).
- Ambos sao PECAS DE ENDERECO da mesma pagina fisica/DMA, nao comando/dados.
- Ambos sao obrigatorios no caminho GA100/GA102 (write_sysmem_flush_page_ga100 escreve os dois
  incondicionalmente). Nouveau legado (gf100, <=40bit) escreve so LO — NAO copiar para GA106.
- Leitura de confirmacao: `read_sysmem_flush_page() == dma_address` (usado no Drop para decidir
  desregistrar com 0).

## Subsistema consumidor

- Falcoes (GSP e SEC2) via sysmembar durante reset handshake; indiretamente todo o boot GSP
  (FWSEC-FRTS, Booter, sequencer CoreReset).
- Nao e "flush de cache da CPU" nem TLB; e barreira de writes GPU->sysmem via PCIe.

## Corroboracao Nouveau (nao-primaria)

- `nvkm/subdev/fb/gf100.c:gf100_fb_sysmem_flush_page_init`: `WARN_ON(addr > DMA_BIT_MASK(40))`
  + `nvkm_wr32(dev, 0x100c10, addr >> 8)`. Confere LO/shift. `ga102.c` reusa este ponteiro, mas o
  Nova (mais novo e com HI) e a prova normativa para GA106 64-bit.
- Nao usar Nouveau como prova de "HI desnecessario" em GA106.

## O que NAO afirmar

- Nao alegar valor fixo (ex. endereco) — o valor deriva do DMA address da pagina alocada.
- Nao alegar que LO/HI sao comando/dados — sao endereco.
- Nao alegar que flush so e preciso antes de DMA grande — e preciso antes de QUALQUER reset Falcon.
