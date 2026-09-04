# SAFETY — Pré-requisitos MMIO (gate antes de qualquer escrita)

> Fase 0 = **somente leitura**. Este documento é o **gate**: enquanto qualquer item
> estiver `BLOCKED`, nenhuma escrita MMIO/BAR, `mmap(PROT_WRITE)`, `/dev/mem`,
> `O_RDWR` em recurso PCI, unbind/bind, reset, clock, firmware ou power-state pode
> ser proposta — nem "rapidinho". Ver também `docs/SAFETY.md` (checklist por proposta).
> Data da avaliação: 2026-09-04. Nenhum item foi marcado como pronto sem evidência.

## Checklist (estado atual — quase tudo BLOCKED)

| # | Pré-requisito | Estado | Evidência / falta |
|---|---|---|---|
| 1 | BDF exato + revisão + VBIOS da target anotados (`docs/ga106/`) | [ ] `BLOCKED` | RTX `01:00.0 [10de:2504] rev a1` conhecida via `lspci`, mas VBIOS **não** coletado de forma read-only |
| 2 | BARs (índice, tamanho, endereço físico via `resource*`, só leitura) mapeados | [ ] `BLOCKED` | `lspci -v`/`resource*` ainda não anexados a doc datado |
| 3 | Registradores alvo (nome/offset/máscara, valor lido antes, fonte doc/código) | [ ] `BLOCKED` | nenhum registrador proposto; sem fonte pinada |
| 4 | Implementação de referência linkada (arquivo:linha + commit) + contexto seguro | [ ] `BLOCKED` | sem referência selecionada |
| 5 | Análise de riscos + engines afetados + janela de irreversibilidade | [ ] `BLOCKED` | sem proposta, sem análise |
| 6 | Dono atual do recurso (driver bound, IOMMU, Secure Boot, PM, Xorg/Wayland, concorrência) | [ ] `BLOCKED` | parcial: `nvidia` bound em `01:00.0`, mas IOMMU/SB/PM/concorrência **não** verificados |
| 7 | Detecção de hang + recuperação sem power-cycle avaliada | [ ] `BLOCKED` | `docs/lab/recovery-plan.md` criado, mas **não exercitado** |
| 8 | Plano de reversão (valor de restore + ordem + critério de abort) | [ ] `BLOCKED` | sem proposta |
| 9 | Revisor designado + aprovação datada | [ ] `BLOCKED` | sem revisor |
| 10 | **Acesso remoto (SSH Machine B → Machine A) testado antes da sessão** | [ ] `BLOCKED` | `recovery-plan.md` §2 define o teste; teste real **não executado/não logado** |
| 11 | Display isolado da target (iGPU = display, RTX = target, ver `docs/lab/dual-gpu-setup.md`) | [ ] `BLOCKED` | `igpu-readiness.md` prova iGPU ausente (verdict `LIKELY`); setup desejado **não** atingido |

## Regra de desbloqueio

- Cada caixa só pode ser marcada `[x]` com link para evidência datada (log, doc, hash de commit).
- Marcar sem evidência é violação de revisão — o commit/PR deve ser rejeitado.
- Este arquivo é avaliado em todo commit que toque `linux/`, `scripts/` com escrita,
  ou qualquer proposta de `mmap`/`unbind`/`/dev/mem`.

## Estado global (2026-09-04)

**`BLOCKED` — 0/11 pronto. Fase 0 permanece somente leitura.**
Próximo desbloqueio esperado: 10 (SSH) + 11 (dual-GPU), ambos somente leitura/operacionais,
antes de qualquer item de escrita ser sequer proposto.
