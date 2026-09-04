# SAFETY — Pré-requisitos MMIO (gate antes de qualquer escrita)

> Fase 0 = **somente leitura**. Este documento é o **gate**: enquanto qualquer item
> estiver `BLOCKED`, nenhuma escrita MMIO/BAR, `mmap(PROT_WRITE)`, `/dev/mem`,
> `O_RDWR` em recurso PCI, unbind/bind, reset, clock, firmware ou power-state pode
> ser proposta — nem "rapidinho". Ver também `docs/SAFETY.md` (checklist por proposta).
> Data da avaliação: 2026-09-04 (rev. dual-GPU 2026-09-04: iGPU habilitada,
> `lab-status` = PARTIALLY_READY). Nenhum item foi marcado como pronto sem evidência.

## Checklist (estado atual — quase tudo BLOCKED)

| # | Pré-requisito | Estado | Evidência / falta |
|---|---|---|---|
| 1 | BDF exato + revisão + VBIOS da target anotados (`docs/ga106/`) | [x] `READY` | RTX `01:00.0 [10de:2504] rev a1`, VBIOS `94.06.25.00.bd`, UUID `GPU-8d1a…` em baseline (`identity`) + `ga106-lab info` (2026-09-04) |
| 2 | BARs (índice, tamanho, endereço físico via `resource*`, só leitura) mapeados | [x] `READY` | BAR0 16M/BAR1 16G/BAR3 32M/IO/ROM em baseline (`pci`) + `lspci -vv` cruzado (2026-09-04) |
| 3 | Registradores alvo (nome/offset/máscara, valor lido antes, fonte doc/código) | [ ] `BLOCKED` | nenhum registrador proposto; sem fonte pinada |
| 4 | Implementação de referência linkada (arquivo:linha + commit) + contexto seguro | [ ] `BLOCKED` | sem referência selecionada |
| 5 | Análise de riscos + engines afetados + janela de irreversibilidade | [ ] `BLOCKED` | sem proposta, sem análise |
| 6 | Dono atual do recurso (driver bound, IOMMU, Secure Boot, PM, Xorg/Wayland, concorrência) | [x] `READY` | `lab-status` 2026-09-04: driver `nvidia`, IOMMU 10, boot_vga=1, fb nvidia-drmdrmfb primary, holders GNOME/Xwayland listados; SB/PM seguem UNKNOWN (não bloqueiam leitura) |
| 7 | Detecção de hang + recuperação sem power-cycle avaliada | [ ] `BLOCKED` | `docs/lab/recovery-plan.md` criado, mas **não exercitado** |
| 8 | Plano de reversão (valor de restore + ordem + critério de abort) | [ ] `BLOCKED` | sem proposta |
| 9 | Revisor designado + aprovação datada | [ ] `BLOCKED` | sem revisor |
| 10 | **Acesso remoto (SSH Machine B → Machine A) testado antes da sessão** | [ ] `BLOCKED` | Reconfirmado 2026-09-04 (somente leitura): `sshd` binário presente mas `disabled+inactive`, nada em `:22`; teste real **não executado** |
| 11 | Display isolado da target (iGPU = display, RTX = target, ver `docs/lab/dual-gpu-setup.md`) | [ ] `BLOCKED` | Progresso 2026-09-04: iGPU `1002:1638` PRESENTE (`amdgpu`, card0/renderD129, HDMI-A-2 ativo) — `lab-status` = `PARTIALLY_READY`; porém desktop (gnome-shell/Xwayland, glxinfo NVIDIA, boot_vga=1, DP-1 ativo) ainda na RTX |

## Regra de desbloqueio

- Cada caixa só pode ser marcada `[x]` com link para evidência datada (log, doc, hash de commit).
- Marcar sem evidência é violação de revisão — o commit/PR deve ser rejeitado.
- Este arquivo é avaliado em todo commit que toque `linux/`, `scripts/` com escrita,
  ou qualquer proposta de `mmap`/`unbind`/`/dev/mem`.

## Estado global (2026-09-04, rev. dual-GPU)

**`BLOCKED` — 3/11 pronto (1, 2, 6). Fase 0 permanece somente leitura.**
`lab-status` = `PARTIALLY_READY`: iGPU funcional com monitor, mas GNOME ainda na RTX.
Próximo desbloqueio esperado: 11 (migrar desktop p/ iGPU) + 10 (SSH),
ambos operacionais, antes de qualquer item de escrita ser sequer proposto.
