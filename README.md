# nvidia-macos — Fase 0: Linux GPU Research Lab

Repositório experimental de pesquisa, **sem nenhum objetivo de driver funcional nesta fase**.

## Objetivo de longo prazo

Estudar a viabilidade de suporte à **NVIDIA GeForce RTX 3060 (GA106, arquitetura Ampere)**
no **macOS Tahoe**, partindo exclusivamente de observação e documentação no Linux.

Não há promessa de driver funcional, não há prazo, não há atalho. Se um dia existir
código para macOS, ele será derivado de dados coletados e revisados aqui.

## Regra incremental (obrigatória)

Todo progresso segue esta ordem, sem pular etapas:

```
identificação → observação → documentação → análise → prototipação (somente leitura)
→ validação → [fases futuras: escrita controlada → bring-up mínimo → macOS]
```

1. **Identificação:** saber exatamente qual hardware/PCI/BAR/revisão está na bancada.
2. **Observação:** coletar apenas dados passivos (lspci, sysfs, dmesg, Nouveau/NVK logs).
3. **Documentação:** registrar tudo em `docs/` antes de escrever qualquer código novo.
4. **Análise:** comparar Ampere/GA106/Nouveau/NVK e marcar lacunas.
5. **Prototipação (somente leitura nesta fase):** ferramentas em `linux/` que nunca escrevem no hardware.
6. **Validação:** build + revisão + novo documento. Só então a próxima etapa é discutida.

Nenhuma etapa de escrita no hardware existe na Fase 0. Qualquer proposta de escrita
exige antes `docs/SAFETY.md` preenchido + revisão explícita.

## Fase 0 — escopo fechado

* `common/` — tipos portáveis C++20, sem dependência de OS (compila em Linux e macOS).
* `linux/ga106-lab/` — **SOMENTE LEITURA**. Observação passiva da GA106 no Linux.
* `linux/tools/` — utilitários de leitura/inspeção (lspci/sysfs wrappers, dumps read-only).
* `docs/{ampere,ga106,nouveau,nvk,research}/` — notas de arquitetura e observações.
* `macos/` — **placeholder intencional**. Nenhum código de driver nesta fase.
* `scripts/`, `tests/` — build reproduzível e smoke tests da lib comum.

## ga106-lab é SOMENTE LEITURA nesta fase — proibições

Na Fase 0, `linux/ga106-lab/` e `linux/tools/` **NÃO PODEM**:

- [ ] fazer escrita MMIO (nenhum store em BAR, nenhum `*(volatile *) = x`);
- [ ] alterar clocks, voltagem, fan curve ou power limit;
- [ ] emitir reset de GPU / FLR / PCI reset;
- [ ] submeter command buffer (nenhum PUSH/doorbell/fifo);
- [ ] carregar, substituir ou tocar em firmware (GSP, PMU, SEC2, NVDEC, etc.);
- [ ] alterar power state (D0/D3, runtime PM, ASPM, PCIe link);
- [ ] fazer unbind/bind de driver (`driver/unbind`, `driver_override`, vfio-pci bind);
- [ ] abrir `/dev/mem`, `/dev/kmem`, `O_RDWR` em recurso PCI, ou `mmap(PROT_WRITE)` em BAR;
- [ ] contornar IOMMU, Secure Boot ou qualquer trava de segurança.

Leituras permitidas: `lspci -nnv`, `lspci -xxxx` (somente visual), sysfs `config`/`resource*`
abertos `O_RDONLY`, `mmap(PROT_READ)` documentado, dmesg/journalctl, logs Nouveau/NVK.

Qualquer violação invalida a Fase 0 e exige revert + post-mortem em `docs/research/`.

## Estrutura

```
nvidia-macos/
├── common/            # lib estática portável (C++20, sem deps de OS)
│   ├── include/nvidia/
│   └── src/
├── linux/
│   ├── ga106-lab/     # SOMENTE LEITURA (Fase 0)
│   └── tools/         # helpers de inspeção read-only
├── macos/             # placeholder futuro — sem código nesta fase
├── docs/
│   ├── SAFETY.md      # checklist obrigatório antes de qualquer escrita MMIO futura
│   ├── ampere/ ga106/ nouveau/ nvk/ research/
├── scripts/           # build.sh e helpers
├── tests/             # smoke tests da lib comum
└── CMakeLists.txt
```

## Build rápido

```bash
./scripts/build.sh
cmake --build build
ctest --test-dir build --output-on-failure
```

Requer: CMake ≥ 3.20, compilador C++20 (GCC 11+ / Clang 13+).

## Status

Fase 0 — scaffolding. Nenhum acesso a hardware implementado ainda.
Ver `docs/SAFETY.md` antes de propor qualquer coisa além de leitura.
