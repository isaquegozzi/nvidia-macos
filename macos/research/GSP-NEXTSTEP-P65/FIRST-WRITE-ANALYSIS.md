# P65 FIRST REQUIRED WRITE — analise de candidatos

Criterio (prompt §4): required pela sequencia real GA106 + progresso mensuravel + alvo/semantica
fixos + nao arbitrario de userspace.

## Candidatos comparados

| Candidato | Alvo | Veredito |
|---|---|---|
| SYSMEM_FLUSH page register (HI 0x100c40 + LO 0x100c10) | NV_PFB_NISO_FLUSH_SYSMEM_ADDR*_HI/LO com DMA addr de pagina Coherent | VENCEDOR — primeiro write requerido |
| Falcon control (CPUCTL start, BOOTVEC, BROM, MAILBOX, OS) | varios por-falcon | Requerido, mas DEPOIS do flush (falcon reset exige flush antes) |
| DMA descriptor programming (FBIF_TRANSCFG, DMATRFBASE/BASE1/MOFFS/FBOFFS/CMD) | por-falcon FBDMA | Requerido, mas DEPOIS (dentro de FWSEC/Booter load) |
| Boot vector isolado | NV_PFALCON_FALCON_BOOTVEC | Subconjunto de dma_load, nao primeiro |
| Interrupt masking (IRQSCLR swgen0) | NV_PFALCON_FALCON_IRQSCLR | Write real cedo (`clear_swgen0_intr` apos Falcon::new), mas nao e o primeiro (flush vem antes em Gpu::new) e nao desbloqueia reset |
| Doorbell | cmdq doorbell | Posterior (RPC), nao pre-boot |
| SYSMEM_FLUSH isolado LO apenas | 0x100c10 so | REJEITADO p/ GA106 (caminho ga100 exige HI+LO; LO-only e legado Nouveau <=40bit) |

## Entrega

```text
FIRST_REQUIRED_WRITE_CANDIDATE = SYSMEM_FLUSH_PAGE_REGISTER (NV_PFB_NISO_FLUSH_SYSMEM_ADDR_HI @0x100c40 + NV_PFB_NISO_FLUSH_SYSMEM_ADDR @0x100c10, caminho GA100/GA102, valor = DMA address da pagina Coherent PAGE_SIZE, LO=(addr>>8), HI=(addr>>40))
WHY_FIRST = fb.rs exige registro "as early as possible, before any falcon is reset" porque o handshake de reset escreve na sysmem e precisa de sysmembar para ficar visivel ao host; Gpu::new ordena SysmemFlush::register imediatamente apos GFW wait e antes de GspResources (qualquer Falcon::new/reset/load/boot, clear_swgen0, FWSEC, Booter, sequencer). Sem ele, o primeiro reset Falcon ja pode dar timeout. E o unico write pos-GFW com semantica fixa, valor derivado (nao arbitrario) e readback valido (read==dma_address).
PRIMARY_SOURCE_PROOF = nova-core-fb.rs (SysmemFlush doc+register) + nova-core-fb-regs.rs (0x100c10/0x100c40) + nova-core-fb-hal-ga100.rs/ga102.rs (HI+LO, shifts 8/40, ordem HI->LO) + nova-core-gpu.rs (ordem Gpu::new) ; corroboracao Nouveau gf100.c (LO/shift) e ga102.c (reuso)
WRITE_DEEP_DIVE_REQUIRED = NO (para identificar o primeiro; deep-dive de sequencer dirigido por firmware continua necessario antes de QUALQUER write alem do flush, ver abaixo)
```

## Ressalva critica (sequencer dirigido por firmware)

`GspSequencer::run` executa `RegWrite/RegModify/RegPoll/...` vindos do GSP via cmdq em runtime.
Logo, apos o flush, NAO existe "segundo write fixo" enumeravel offline: a sequencia subsequente e
data-driven. Qualquer spec de write alem do flush exige capturar/interpretar a sequencia real ou
limitar-se a re-executar writes primitivos ja provados (reset/dma_load/boot) — nenhum write
arbitrario offset/value. Por isso §14 espec so cobre o flush como proximo gate; demais writes
permanecem PROIBIDOS neste prompt.
