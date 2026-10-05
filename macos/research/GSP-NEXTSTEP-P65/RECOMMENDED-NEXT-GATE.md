# P65 RECOMMENDED NEXT GATE + SPEC CONDICIONAL (§14/§15/§16)

## Classificacao (§13)

- A. another read-only validation — blast radius zero; nao produz progresso de boot sozinha, mas e
  pre-requisito imediato (reler flush regs, WPR2 range, Command, fuse/HWCFG2) e pos-condicao de cada passo.
- B. first fixed MMIO write — SYSMEM_FLUSH HI+LO — RECOMENDADO como proximo gate de progresso real.
- C. BME enable — requerido antes do primeiro FBDMA, mas DEPOIS do flush na ordem Nova (probe ja o
  teria ligado no Linux; no macOS ainda OFF). Nao e o primeiro write; e PCI config, nao MMIO.
- D. first GPU DMA — muito cedo; exige B+C+flush+firmwares+WPR2. Very high.
- E. firmware staging — very high; exige D.

```text
RECOMMENDED_NEXT_GATE = B (first fixed MMIO write: SYSMEM_FLUSH_PAGE_REGISTER HI+LO)
NEXT_GATE_RISK = high (first MMIO write; ver §17)
ASTRA_REVIEW_REQUIRED_BEFORE_IMPLEMENTATION = YES (Astra indisponivel; nenhuma implementacao/live neste prompt)
```

Criterios atendidos: semantica fixa (2 regs + shifts), fonte primaria total, sucesso/falha mensuravel
(readback == dma_address; reset handshake passa a ter sysmembar), zero offset/value arbitrario,
uma variavel por vez (so flush; sem BME/DMA/firmware no mesmo experimento).

Pre-requisito read-only (A) antes de B: confirmar LO/HI atuais (esperado 0 se nunca registrado),
WPR2 ausente (`HI==0`), Command `0003`, BOOT0/BOOT42 estaveis. Apos B: readback + preservar pagina
vitalicia (Drop so no unload/teardown dedicado).

## §14 — SPEC do proximo gate (WRITE; spec apenas, sem codigo, sem live)

```text
semantic selector name = PREPARE_GSP_SYSMEM_FLUSH_PAGE (nome semantico; sem numero final aqui)
fixed registers = NV_PFB_NISO_FLUSH_SYSMEM_ADDR_HI @0x00100c40 (23:0 adr_63_40) + NV_PFB_NISO_FLUSH_SYSMEM_ADDR @0x00100c10 (31:0 adr_39_08)
fixed value derivation = DMA address (device-visible, IOVA/fisico coerente) de UMA pagina sysmem PAGE_SIZE recem-alocada via API DMA coerente do kernel (equivalente a CoherentHandle::alloc(PAGE_SIZE)); LO=(addr>>8) as u32; HI=(addr>>40) (cast limitado ao campo); ordem: HI primeiro, LO depois (igual ga100 HAL); nenhum byte de userspace entra no valor
input ABI = nenhum input de userspace (chamada sem argumentos; pagina e vitalicia no kernel; IOVA/endereco NUNCA expostos)
output ABI = status (OK/ERR) + flush_registered (bool) + readback_match (bool: read(HI,LO)==dma_address) — sem enderecos
preconditions = GFW_BOOT==0xff provado; BAR0 mapeado + MSE ON; BME ainda OFF permitido; WPR2 ausente (HI_WPR2==0); nenhum reset/load/boot Falcon executado apos este ponto sem flush; pagina anterior (se houver) preservada; Astra final review COMPLETO (pendente)
postconditions = read_sysmem_flush_page()==dma_address da pagina; pagina mantida viva (pinned) ate teardown dedicado; nenhum outro reg alterado (prova por diff de leitura antes/depois de regiao 0x100c00-0x100c80 + WPR2 + Command inalterado exceto o esperado=nenhum)
timeout = alocacao limitada (ex. monotona, sem loop infinito); writes sao stores unicos (sem poll); readback imediato; sem espera de firmware
readback = SIM, semanticamente valido (read HI/LO e recompor (LO<<8)|(HI<<40) == dma_address); mismatch => FAIL + teardown (desregistrar com 0 apenas se read==nossa pagina, igual Drop do Nova)
zero arbitrary exposure = sem offset/valor/largura/timeout controlados por userspace; sem seletor generico de escrita
stop/lifetime rules = pagina pinned ate driver stop/unload dedicado que desregistra (write 0,0) apenas se ainda dono; close nao libera; reboot/EFI inalterados
expected failure modes = alloc fail (ENOMEM); addr overflow do campo (EINVAL); readback mismatch (EIO); WPR2 presente (EBUSY => abortar, GPU precisa reset); GFW!=0xff (EAGAIN); qualquer falha => sem retry automatico, sem avancar para BME/DMA
```

Nao autorizado: live, MMIO, BME, DMA, firmware, GSP (todos NO no relatorio).

## §15 — SE BME FOSSE o proximo gate (deferido; documentado, nao implementado)

- API exata: `IOPCIDevice::setBusLeadEnable(true)` (atual; `setBusMasterEnable` deprecated :574).
  Header: `.../Kernel.framework/.../IOKit/pci/IOPCIDevice.h:1095` (lead) / `:574` (deprecated) / `:563` (MSE).
- Esperado Command: antes `0003` (MSE ON, BME OFF) -> depois `0007` (MSE+BME). Nada mais muda.
- Proibido apos BME (mesmo com BME ON): qualquer MMIO write alem do flush, qualquer DMA/GSP/firmware,
  qualquer interrupt/MSI, qualquer reset alem do ja especificado — BME so arma o DMA futuro, nao o autoriza.
- Rollback: `setBusLeadEnable(false)` => volta a `0003`; verificar por leitura read-only de Command.

## §16 — SE DMA FOSSE o proximo gate (deferido; documentado, nao implementado)

- Proposito one-shot: FWSEC-FRTS load no GSP via FBDMA (primeiro DMA real) — nao Booter, nao Radix3.
- Fonte/destino fixos: fonte = imagem FWSEC do VBIOS em CoherentBox 256B-aligned; destino =
  IMEM secure + DMEM do GSP; TRANSCFG[0]={CoherentSysmem,Physical}; descritores DMATRFBASE/BASE1/
  MOFFS/FBOFFS/CMD Size256B em loop; BOOTVEC+brom; mbox0=Some(0).
- Lifetime: todas as fontes DMA vivas ate halt+mailbox; flush page viva; WPR meta ainda nao existe aqui.
- IOVA: apenas `dma_address()` do kernel; nada de userspace.
- Pre-requisito BME: ON + dma mask + flush registrado + WPR2 ausente.
- Conclusao/abort: poll DMATRFCMD idle 2s/bloco + halt 2s + mbox0==0 + FRTS_ERR==0 + WPR2 criado;
  timeout/erro => abortar, sem retry, sem avancar para Booter/RM.

## §17 — RISK GATES

```text
GFW readiness read = low/read-only (ja validado 1.6.2 offline; live futuro = read-only)
first MMIO write (flush) = high (primeiro store persistente; Astra obrigatorio)
BME = high (PCI config persistente; altera capacidade DMA; Astra obrigatorio)
first GPU DMA = very high (FBDMA; corrupcao IMEM/DMEM/WPR possivel; Astra obrigatorio)
firmware/GSP execution = very high (RM executa; sequencer data-driven; Astra obrigatorio)
Astra final review obrigatorio antes de implementar B, C, D ou E. Astra indisponivel neste prompt.
```
