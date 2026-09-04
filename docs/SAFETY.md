# Safety — checklist obrigatório antes de QUALQUER escrita MMIO

> Fase 0 = **somente leitura**. Este documento existe para que, quando (e se)
> uma fase futura propor escrita, não haja improviso.

Nenhuma escrita em BAR, nenhum `mmap(PROT_WRITE)`, nenhum `/dev/mem`, nenhum
`O_RDWR` em recurso PCI, nenhum unbind/bind, reset, clock, firmware ou power
state pode ser executado — nem testado "rapidinho" — sem este checklist
preenchido, revisado e linkado no commit/PR.

## Checklist (copiar para o PR/documento da proposta)

1. **O que será acessado**
   - [ ] BDF exato + revisão da placa + VBIOS version (`docs/ga106/`)
   - [ ] BAR índice, offset, tamanho e largura (8/16/32 bits)
   - [ ] Endereço físico + confirmação via `resource*` (somente leitura)
2. **Registradores**
   - [ ] Nome/offset de cada registrador + máscara de bits tocada
   - [ ] Valor lido antes, valor pretendido depois, bits preservados
   - [ ] Fonte: doc NVIDIA / Nouveau `nvhw/` / NVK / TRM — link + commit hash
   - [ ] Trecho de código open-source correspondente (arquivo:linha)
3. **Fonte e precedente**
   - [ ] Link para implementação de referência que faz a mesma escrita
   - [ ] Em que contexto ela é segura lá (ordem de init, locks, estado do engine)
   - [ ] O que aqui é diferente (kernel version, driver bound, GSP on/off)
4. **Riscos**
   - [ ] O que acontece se o valor estiver errado (hang PCIe? corrupção FB? brick?)
   - [ ] Engines afetados (FIFO, PGRAPH, NVDEC, PMU, SEC2, DISPLAY…)
   - [ ] Janela de irreversibilidade (ex. strap, fuse, flash, P-state travado)
5. **Quem controla o recurso agora**
   - [ ] Driver bound? (`nouveau` / `nvidia` / `vfio-pci` / nenhum)
   - [ ] IOMMU on/off, Secure Boot, runtime PM, Xorg/Wayland rodando?
   - [ ] Outro processo pode concorrer pelo mesmo registrador?
6. **Como recuperar de hang**
   - [ ] Como detectar o hang (timeout, dmesg pattern, `lspci` ainda responde?)
   - [ ] Recuperação sem power-cycle funciona? (remoção PCI? reboot?)
   - [ ] Pior caso: precisa de power-cycle físico? de reflash VBIOS?
   - [ ] Quem executa a recuperação e com que acesso físico à máquina?
7. **Plano de reversão**
   - [ ] Valor de restore exato + ordem de restore
   - [ ] Log de antes/depois anexado ao documento
   - [ ] Critério de abort (quando parar e reverter)

## Modelo mínimo de proposta (colar no doc)

```text
Alvo:      0000:01:00.0 (GA106 rev a1, VBIOS xx.xx.xx)
BAR:       BAR1, offset 0xxxxxx, 32-bit, máscara 0x000000ff
Antes:     0x........ (lido em <data>, via <ferramenta read-only>)
Depois:    0x........ (bits x..y alterados, resto preservado via RMW documentado)
Fonte:     nouveau/nvkm/engine/...c:<linha> @ <commit> + NVK ... @ <commit>
Risco:     <hang recuperável via reboot / ...>
Dono:      <driver bound>, IOMMU=<on/off>, PM=<...>
Recuperação: <passos 1..N>
Reversão:  <valor + ordem>
Revisor:   <nome> em <data> — aprovado / reprovado
```

Sem este bloco preenchido, o commit que introduz escrita **deve ser rejeitado
em revisão**, mesmo que "funcione na minha máquina".

## Proibições permanentes da Fase 0 (reafirmação)

Ver `README.md` raiz. Em resumo: sem escrita MMIO, sem clocks/voltagem/reset,
sem command buffer, sem firmware, sem power state, sem unbind, sem `/dev/mem`.
