> Documento histórico da Fase 0. A pasta macos agora contém as fontes importadas do laboratório.

# macos/ — placeholder intencional (Fase 0)

Não há código de driver macOS nesta fase. Este diretório existe apenas para
marcar o destino de longo prazo e impedir que código prematuro apareça aqui.

## Por que vazio?

A regra incremental exige antes:

1. identificação + observação no Linux (`linux/ga106-lab/`, somente leitura),
2. documentação em `docs/` (Ampere → GA106 → Nouveau → NVK),
3. análise e validação,
4. só então — em fase futura — prototipação de bring-up mínimo.

Pular para `macos/` agora seria tentar escrever um driver sem saber o que o
hardware espera. O custo de um erro no macOS (kernel panic, SPI/flash, T2/AMFI)
é ordens de magnitude maior que no Linux lab.

## Quando este diretório ganha conteúdo?

Apenas quando todas as condições forem verdadeiras:

- [ ] `docs/ga106/` descreve BARs, BIOS/VBIOS e strap da bancada real;
- [ ] `docs/nouveau/` + `docs/nvk/` descrevem init mínimo observado (GSP/RM);
- [ ] `docs/SAFETY.md` tem checklist de escrita MMIO aprovado em revisão;
- [ ] existe decisão registrada em `docs/research/` autorizando a Fase N (escrita).

Até lá: **não criar `.kext`, DriverKit `.dext`, IOKit, PCIDriverKit ou qualquer
código Mach-O aqui**. Propostas de macOS entram primeiro como documento em
`docs/research/`.

## Pesquisa documental macOS (2026-09-04)

Gate comparativo + ADR de entrypoint (somente docs, sem código):

- `../docs/macos/driver-architecture-gate.md` — PCIDriverKit (A) vs IOKit
  KEXT (B) com confiança por afirmação; restrição `built-in` CONFIRMED com
  aplicabilidade à RTX UNCERTAIN; produção `0x10DE` como bloqueador prático.
- `../docs/macos/ADR-0001-driver-entrypoint.md` — decisão: **IOKit KEXT first**
  (PCIDriverKit diferido) para o lab PCI read-only no Hackintosh Tahoe.
