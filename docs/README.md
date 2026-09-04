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
| `macos/`            | Qual entrypoint de driver (PCIDriverKit vs KEXT) para o lab PCI?   | ADR-0002 (TG0) |
| `tinygpu/`          | Quanto do controle Ampere já existe no TinyGPU e o que falta p/ PCI interno? | mapeado (TG0) |
| `tinygrad-nv/`      | Quanto do stack NV (NVDev/GSP/mem/submit) é reutilizável?          | mapeado (TG0) |

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

## Documentos Fase TG0 — TinyGPU Architecture Audit (2026-09-04)

Fontes pinadas: `tinygrad/tinygrad@e8c8ba1c` (2026-09-04), `tinygpu_releases@c0d024f9` (TinyGPU.zip `0c47285e…`, 1634625 B).

- `tinygpu/support-matrix.md` — requisitos TinyGPU (macOS, Ampere+, USB4/TB, `DEV=NV`); RTX 3060/GA106/`2504`: arquitetura LIKELY suportada, SKU específico UNKNOWN.
- `tinygrad-nv/ops-nv-audit.md` — auditoria `ops_nv.py`; **`PCIIface reconhece 10de:2504? YES`** (`0x2504 & 0xFF00 = 0x2500 ∈ lista`, `ops_nv.py:560` + `system.py:80`).
- `tinygrad-nv/nv-stack-map.md` — caminho real Tensor→…→GA106 por camada (classe/arquivo/função/OS-/transport-/gen-specific).
- `tinygrad-nv/nvdev-audit.md` — NVDev por subsistema (init/mem/submit/GSP); **init sem `nvidia.ko`: CONFIRMED** (`PCIDevice` dá unbind e aborta se driver ligado; `NVKIface` é quem usa `/dev/nvidia*`).
- `tinygrad-nv/ga106-gap-analysis.md` — tabela 10 etapas × Nouveau × NVIDIA Open × NVDev × nós: **10/10 Ampere genérico no tinygrad, 0/10 GA106-específico** (cai em GA1/ga102, FW `ga102`).
- `tinygpu/pci-transport-contract.md` — contrato real extraído de `system.py` (tabela operação×chamada×implementação×necessária-p/GA106).
- `tinygpu/apl-remote-pci.md` — caminho `APLRemotePCIDevice`→TinyGPU.app→socket→dext→`IOPCIDevice`→GPU.
- `tinygpu/remote-protocol.md` — `RemoteCmd` 0–12 (APL emite 1–7,11; `server.c` não implementa 0,8,9,10,12).
- `tinygpu/binary-architecture.md` — estática do bundle (análise no Linux): dext `IOProviderClass=IOPCIDevice`, `IOPCIClassMatch=0x03000000`, `IOPCITunnelCompatible=True`, entitlements `0x10DE`/`0x1002` any-device, team `9YG3G8543N`; resto `REQUIRES_MACOS_INSPECTION`.
- `tinygpu/why-usb4.md` — hipóteses: C+D CONFIRMED, A+B LIKELY, E FALSE, F UNCERTAIN.
- `tinygpu/internal-pcie-gap.md` — gap exato: matching DriverKit antes de `Start_Impl` (interna nunca instancia `TinyGPUDriver`).
- `macos/ADR-0002-tinygpu-reuse.md` — decisão **C — TinyGPU-compatible transport** (ADR-0001 SUPERSEDED); MAC-COMPUTE-0 = MEDIUM.
- `tinygpu/project-impact.md` — artefatos antigos em STILL_REQUIRED/USEFUL_REFERENCE/REPLACED_BY_TINYGRAD/DEFERRED; Metal segue NÃO resolvido.

## Documentos Fase TGM0 — correção TunnelCompatible + inspeção macOS (2026-09-04, sessão Linux; ioreg pendente)

- `fix(tinygpu)` — `IOPCITunnelCompatible` = suporte declarado (Apple: "supports Thunderbolt"), NÃO prova Thunderbolt-only; túnel real = `IOPCITunnelled`. Hipóteses: C→LIKELY, D→UNKNOWN; gap A/B/C em aberto.
- `tinygpu/amd-igpu-match-risk.md` — veredito **RISK**: classe + entitlement `0x1002` casam a iGPU do desktop; **não instalar** a DEXT.
- `tinygpu/direct-reuse-assessment.md` — TinyGPU-original→RTX interna→NVDev = **UNLIKELY** + PROVADO vs NÃO PROVADO.
- Reuse corrigido (ADR-0002 §8): CODE_PATH_COVERAGE 100% vs HARDWARE_VALIDATION 0% (macOS tudo; GA106-Linux NVDev tudo); **MAC-COMPUTE-0 = LOW**.
- `macos/hardware/ga106-ioreg-runbook.md` — procedimento somente-leitura p/ Tahoe (snapshots em `artifacts/macos/`, gitignored); **PENDING_MACOS** (sessão atual é Linux).
- `scripts/tinygpu-match-check.py` — simulador estático (`--self-test`: ALL PASS).
