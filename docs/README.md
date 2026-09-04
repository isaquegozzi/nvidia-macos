# docs/ — índice

Fase 0 é 80% documentação. Código sem documento correspondente é suspeito.

| Diretório           | Pergunta que responde                                              | Status Fase 0 |
|---------------------|--------------------------------------------------------------------|---------------|
| `ampere/`           | O que a arquitetura Ampere garante para todo chip?                 | placeholder   |
| `ga106/`            | O que a nossa GA106 específica tem/é?                              | placeholder   |
| `nouveau/`          | O que o Nouveau já sabe fazer com GA106/GSP?                       | placeholder   |
| `nvk/`              | O que o NVK (Vulkan aberto) revela sobre init?                     | placeholder   |
| `nvidia-open/`      | O que o RM open (`610.57.04`) implementa para GA106?               | mapeado       |
| `nvidia-open-gpu-doc/` | O que a documentação pública de HW garante para Ampere/GA106?   | mapeado       |
| `research/`         | Decisões, becos sem saída, próximos passos                        | placeholder   |
| `lab/`              | A bancada está pronta para isolar a RTX sem perder o display?      | mapeado       |
| `macos/`            | Qual entrypoint de driver (PCIDriverKit vs KEXT) para o lab PCI?   | mapeado       |

Regra: toda observação de hardware vira nota datada aqui **antes** de virar
código em `linux/`. Ver `SAFETY.md` antes de propor qualquer escrita.
Ver `SAFETY_MMIO_PREREQUISITES.md` (gate: enquanto `BLOCKED`, Fase 0 segue somente leitura).

## Documentos Fase 0 (2026-09-04)

- `nouveau/ampere-discovery.md` — discovery Nouveau (probe PCI, engines GA10x, GSP-RM, BAR/VM, display).
- `nvk/ampere-discovery.md` — discovery NVK (nv_device_info, classes, queues, memória, WSI).
- `ga106/device-ids.md` — Device IDs GA106 verificados + armadilhas não-GA106.
- `ampere/architecture-overview.md` — overview Ampere (dies, blocos, RTX 3060, firmwares GA106).
- `nvidia-open/GA106-kernel-map.md` — mapa GA106 no `open-gpu-kernel-modules` tag `610.57.04` (SHA `e4a5faa…`): identidade HAL, class-list (`AMPERE_B`), engines GR/FIFO/MMU/CE/GSP/SEC2/BIF/DISP/vídeo; driver-instalado vs source + lacunas UNKNOWN.
- `nvidia-open-gpu-doc/ampere-ga106-index.md` — índice `open-gpu-doc` commit `9fdf5c4…` aplicável a Ampere/GA106 (3D `clc797.h`, compute, CE, FIFO, MMU, display, vídeo, manuais `ga100/ga102`, BIT/DCB/Falcon-GSP); confirmação `AMPERE_B` + lacunas UNKNOWN.
- `SAFETY_MMIO_PREREQUISITES.md` — gate MMIO (11 itens, 0/11 em 2026-09-04 → `BLOCKED`, inclui acesso remoto SSH).
- `lab/igpu-readiness.md` — investigação Cezanne somente leitura (2026-09-04): `1002:1638` ausente, `1002:1637` presente, boot VGA RTX `01:00.0`, sem `amdgpu`/DRM AMD; verdict `LIKELY` (firmware).
- `lab/dual-gpu-setup.md` — alvo Vega=display/RTX=target, critérios de pronto + categorias genéricas de opções de firmware (nomes variam por fabricante).
- `lab/recovery-plan.md` — Machine A/B, SSH, coleta de logs, escada de reinício; sem watchdog.
- `lab/pcie-topology.md` — topologia PCIe read-only (2026-09-04): GPU max Gen4 x16, root-port `00:01.1` max Gen3 x16, current Gen1 x16 (era Gen2 no baseline) por ASPM; tabela canônica GT/s→Gen (`5.0`=Gen2).
- `lab/nvidia-free-boot-test.md` — plano REVERSÍVEL DE UM ÚNICO BOOT (NÃO APLICADO, 2026-09-04): bootloader Limine 12.7.0, initramfs mkinitcpio com `MODULES+=(nvidia ...)` via chwd → exige `module_blacklist=` (`modprobe.blacklist` só não basta; `rd.driver.blacklist` é só-dracut); edição `E` no menu, sem persistir.
- `lab/ssh-readiness.md` — prontidão SSH (somente leitura, 2026-09-04, nada habilitado): `sshd` instalado mas `inactive`+`disabled`, sem `:22`, sem `authorized_keys`, `ufw` ativo com regras UNKNOWN, alvo LAN `192.168.5.12`; gaps + comandos locais/remotos exatos.
- `macos/driver-architecture-gate.md` — comparativo PCIDriverKit (A) vs IOKit KEXT (B) com confiança por afirmação (2026-09-04): IOPCIDevice, matching, config/BAR, SystemExtensions, signing, entitlements, restrição `built-in` CONFIRMED (código + DTS) com aplicabilidade à RTX UNCERTAIN, produção `0x10DE` como bloqueador prático, Tahoe/KEXT/Lilu-WEG, futuro gráfico.
- `macos/ADR-0001-driver-entrypoint.md` — decisão `IOKit KEXT first` (PCIDriverKit diferido), baseada em carregabilidade real, entitlements, PCI interno, MMIO, futuro, debugging, Tahoe e objetivo gráfico.
- `ga106/first-mmio-read.md` — candidatura do primeiro MMIO read (doc only, sem execução): veredito reuse `common/` (limpo, só comentários sysfs), `NV_PMC_BOOT_0` confirmado (`0x00000000`, 32-bit, `R--4R`, `0x176xxxxx`, rev `a1` ⇒ `0x176000A1` aprox.) com pins tag `610.57.04` (SHA `e4a5faa…`) + `open-gpu-doc@9fdf5c4…` + Nouveau `46741e4f`/`v6.6`/`986c24e0`; riscos + gates (boot sem NVIDIA + SSH + desktop independente, `BLOCKED` 3/11).
- `macos/common-reuse.md` — reuse `common/` no macOS (DriverKit C++ vs IOKit C++ vs helper userspace): `NvidiaDeviceInfo`/`NvidiaPciDevice`/`NvidiaPciBar` reutilizáveis sem deps Linux (só comentários sysfs em `pci_device.hpp:3`/`pci_bar.hpp:11`); ressalva tabela `0x2487`/`0x24AA` vs `device-ids.md`.
