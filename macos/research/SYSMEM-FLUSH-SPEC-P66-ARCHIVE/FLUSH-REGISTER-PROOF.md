# P66 FLUSH-REGISTER-PROOF — GA106 (reconfirmação offline a partir de snapshots P65)

Status: SPEC / PROOF ONLY. Sem implementação, sem live, sem KEXT/EFI alterados.
Fontes: `~/Documents/nvidia-macos-BARE0/GSP-NEXTSTEP-P65/UPSTREAM-EVIDENCE/` (snapshots locais, ver SHA manifest P65).

## 1. Claim reconfirmado

```text
0x00100c10 = NV_PFB_NISO_FLUSH_SYSMEM_ADDR (LO)
0x00100c40 = NV_PFB_NISO_FLUSH_SYSMEM_ADDR_HI (HI)
FLUSH_REGISTER_PROOF_RECONFIRMED = YES
```

## 2. Evidência primária (snapshot, não só PRIMARY-SOURCES.md)

1. `nova-core-fb-regs.rs` (lido do snapshot):
   - `NV_PFB_NISO_FLUSH_SYSMEM_ADDR(u32) @ 0x00100c10 { 31:0 adr_39_08; }`
   - `NV_PFB_NISO_FLUSH_SYSMEM_ADDR_HI(u32) @ 0x00100c40 { 23:0 adr_63_40; }`
   - Comentário no próprio register: "Low/High bits of the physical system memory address used by the GPU to perform sysmembar operations (see crate::fb::SysmemFlush)".
   - Também define WPR2 em 0x001fa824/0x001fa828 — prova que 0x100c10/0x100c40 NÃO são WPR2.
   - Define variantes GB10x/Hopper em outros offsets (0xe50/0xe54, 0x8a1d58, 0x100a34) — prova que 0x100c10/0x100c40 são o par Ampere/NISO, não Hopper/GB20x.

2. `nova-core-fb.rs` struct `SysmemFlush`:
   - Doc: "A system memory page is required for `sysmembar`... It is required for falcons to be reset as the reset handshake writes into system memory. To ensure visibility + prevent timeouts, falcon must perform sysmembar."
   - Doc: "must be registered as early as possible during driver initialization, and before any falcon is reset."
   - `register()`: `CoherentHandle::alloc(dev, PAGE_SIZE)` + `hal.write_sysmem_flush_page(bar, dma_address())`.
   - `Drop`: `if read()==dma_address { write(0) } else { warn }` — prova readback válido e ownership.

3. `nova-core-fb-hal-ga100.rs`:
   - `FLUSH_SYSMEM_ADDR_SHIFT_HI = 40` (const local), usa `FLUSH_SYSMEM_ADDR_SHIFT = 8` de tu102.
   - `write_sysmem_flush_page_ga100`: escreve HI com `(addr>>40)` (Bounded shr+cast), DEPOIS LO com `(addr>>8) as u32` (comentário CAST proposital para strip upper bits).
   - `read_sysmem_flush_page_ga100`: `(LO.adr_39_08 << 8) | (HI.adr_63_40 << 40)`.
   - Ambos escritos incondicionalmente — HI+LO obrigatórios neste caminho.

4. `nova-core-fb-hal-ga102.rs`:
   - `Ga102::write/read` delegam 100% para `ga100::read/write_ga100`. Sem lógica própria.
   - GA106 = Ampere não-GA100 → usa `GA102_HAL` (confirmado por `fb/hal.rs` + `falcon_hal`/`fb_hal` dispatch + P65 GSP-BOOT-SEQUENCE.md). Logo GA106 usa caminho HI+LO.
   - Contraste: `nova-core-fb-hal-tu102.rs` `write_sysmem_flush_page_gm107` escreve SÓ LO (`addr>>8`, try_from u32). É caminho legado ≤40-bit, NÃO aplicável a GA106. Nouveau `gf100.c` (só LO + WARN_ON addr>DMA_BIT_MASK(40)) é corroborador legado, não prova de "HI desnecessário".

5. `nova-core-gpu.rs` `Gpu::new` (ordem):
   - `dma_set_mask_and_coherent` → `wait_gfw_boot_completion` → `SysmemFlush::register` → `GspResources { gsp_falcon: Falcon::new + clear_swgen0, sec2_falcon: Falcon::new, ... gsp.boot }`.
   - Comentário: "Initialize this early because `gsp_resources` depends on it."
   - Struct order: `gsp_resources` declarado ANTES de `sysmem_flush` para que Drop do GSP ainda tenha página ativa.
   - Logo: após GFW_READY, os PRIMEIROS MMIO writes são HI+LO do flush. `clear_swgen0_intr` (IRQSCLR) vem depois, dentro de `Falcon::new` do GSP.

## 3. Itens confirmados (§2 do prompt)

- LO encoding: `(dma>>8) as u32`, campo 31:0 = addr[39:8]. Largura 32-bit.
- HI encoding: `(dma>>40)` em campo 23:0 = addr[63:40]. Largura 32-bit reg, 24 bits usados.
- Write order no upstream: HI depois LO? Não — HI PRIMEIRO, LO DEPOIS (ordem textual em `write_ga100`).
- Register width: ambos u32 MMIO.
- Required alignment: low 8 bits ignorados → alinhamento 256 B mínimo; PAGE_SIZE (4096) satisfaz.
- Address bits representados: 63:8 (26+32? na prática 39:8 + 63:40). Bits 7:0 não representáveis.
- GA106 applicability: SIM, via GA102_HAL → ga100 path. LO-only (gm107/Nouveau) REJEITADO para GA106.

## 4. O que NÃO afirmar

- Não alegar latch/commit semântico só pela ordem do código (ver HILO-COMMIT-ATOMICITY.md).
- Não alegar endereço fixo — valor deriva do DMA address da página alocada.
- Não alegar que LO/HI são comando/dados — são endereço.
- Não alegar que flush só precisa antes de DMA grande — precisa antes de QUALQUER reset Falcon.

## 5. Veredito

```text
FLUSH_REGISTER_PROOF_RECONFIRMED = YES
```
