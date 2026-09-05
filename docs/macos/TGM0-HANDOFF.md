# HANDOFF TGM0 — contexto para sessão macOS (opencode no Tahoe)

> Gerado no Linux em 2026-09-04. Leia este arquivo primeiro, depois execute a
> tarefa em "MISSÃO IMEDIATA". Tudo abaixo é o resumo fiel da conversa/projeto.

## 1. Premissa do projeto

Projeto experimental de longo prazo `nvidia-macos`: estudar a viabilidade de
controlar uma **NVIDIA RTX 3060 (GA106)** no **macOS Tahoe (Hackintosh)** para
compute (meta intermediária MAC-COMPUTE-0). Metal/aceleração de desktop estão
FORA de escopo (informado ao usuário com honestidade: backend Metal é Apple
proprietary; teto realista = laboratório PCI/GSP/compute, nunca Metal).

Regra permanente: incrementos pequenos, somente leitura até gates liberarem,
`docs/SAFETY.md` + `docs/SAFETY_MMIO_PREREQUISITES.md` (gate: `BLOCKED` 3/11).

## 2. Hardware

- Ryzen 5 5600G; iGPU **AMD Cezanne `1002:1638`** (`08:00.0`, rev c9) + áudio `1002:1637`
- **RTX 3060 LHR `10de:2504`** (`01:00.0`, rev a1, subsys `10de:2504`, GA106 — NÃO confundir com `0x2487`, que é GA104!)
- VBIOS RTX `94.06.25.00.bd`, UUID `GPU-8d1a1165-3fc5-8092-bc90-9783284b66f1`, VRAM 12 GB
- BARs RTX: BAR0 16M @fb000000, BAR1 16G, BAR3 32M, IO f000/128, ROM 512K disabled
- Driver Linux RTX: NVIDIA Open `610.57.04`; IOMMU grupo 10; link Gen1/Gen2 idle (teto Gen3 do root-port Cezanne; `5.0 GT/s` = **Gen2**, não Gen3!)
- Bootloader Linux: **Limine 12.7.0**; initramfs mkinitcpio com `MODULES+=(nvidia…)` (single-boot sem NVIDIA exige `module_blacklist=`, doc em `docs/lab/nvidia-free-boot-test.md` — NÃO aplicado)
- SSH: instalado, `disabled+inactive`, sem `:22` (bloqueador manual pendente)

## 3. Estado dual-GPU (Linux, verificado pós-reboot)

`lab-status`: `Desktop independence: READY` (render AMD 20 vs 2 fds, fb
`amdgpudrmfb`, boot_vga AMD=1/NVIDIA=0, RTX 0 conectores, glxinfo AMD);
`MMIO readiness: BLOCKED` (ownership NVIDIA + SSH + boot-test). Veredito
legado fail-closed: `PARTIALLY_READY` (gnome-shell mantém ~2 fds renderD128 +
enumeração — documentado como multi-GPU Mutter, não-bloqueante p/ desktop).

## 4. Fases e artefatos principais

- **Fase 0**: `ga106-lab info` (C++20, `common/` portátil + `linux/`, CMake, ctest); docs Nouveau/NVK/GA106.
- **Fase 0.5**: `baseline` (txt+json em `artifacts/baselines/`, gitignored) + `implementation-map` + `initialization-map` + `three-stack-comparison`. Correção: símbolo é `nv176_chipset` (`case 0x176`), commit `46741e4f` — NÃO `ga106_chipset`.
- **Fase 0.75**: `baseline --compare` (IDENTITY/STATIC/SEMI_STATIC/DYNAMIC, `--strict`) + `lab-status` + `docs/lab/*` + `SAFETY_MMIO_PREREQUISITES.md`.
- **Dual-GPU**: PARTIALLY_READY (iGPU habilitada, desktop ainda NVIDIA na época).
- **Pós-reboot físico** (monitor só na placa-mãe, IGD primário): desktop migrou p/ AMD; `lab-status` com `Desktop render` dominante.
- **Pré-TG0**: split readiness; bootloader/SSH docs; `driver-architecture-gate` + **ADR-0001 = IOKit KEXT first**; `common/` limpo p/ macOS; `first-mmio-read` (`NV_PMC_BOOT_0` @0x0, esperado GA106 rev a1 ≈ `0x176000A1`, doc-only).
- **TG0 (TinyGPU audit)**: tinygrad `e8c8ba1c`, tinygpu_releases `c0d024f9` (zip sha `0c47285e…`); **PCIIface↔`10de:2504` = YES** (`0x2504&0xFF00=0x2500`); NVDev init sem `nvidia.ko` = CONFIRMED, 10/10 Ampere genérico, 0/10 GA106-específico; dext = class-match `0x03000000` + entitlements `10DE`/`1002` any-device; RemoteCmd 0–12; **ADR-0002 = C (TinyGPU-compatible transport)**, ADR-0001 SUPERSEDED; MAC-COMPUTE-0 = MEDIUM→LOW.
- **TGM0 (correção)**: `IOPCITunnelCompatible` = suporte declarado, NÃO prova Thunderbolt-only (túnel real = `IOPCITunnelled`); `fix(tinygpu)` commit `77fd0b5`; personality re-extraída; **risco AMD = RISK (não instalar DEXT)**; reuso direto = UNLIKELY; `tinygpu-match-check.py` (`--self-test` PASS); runbook ioreg PENDENTE (era impossível no Linux).
- **IDs**: `common/include/nvidia/chip.hpp` (`NvidiaChip`, `chip_from_device_id`), `fix(common)` `5911c75`, teste `chip_id` no ctest. `0x2487`=GA104!

## 5. MISSÃO IMEDIATA (executar AGORA no macOS Tahoe)

Pré-condição: desktop Tahoe rodando na AMD (verifique: About This Mac / display ativo na saída da placa-mãe; aborte experimentos se o desktop depender da RTX).

**SOMENTE LEITURA. PROIBIDO**: instalar/ativar TinyGPU.app ou qualquer dext; carregar KEXT próprio; escrever PCI config; mapear BAR; MMIO; alterar OpenCore/SIP/AMFI/NVRAM; detach de driver; modificar qualquer coisa.

1. Leia e execute `docs/macos/hardware/ga106-ioreg-runbook.md` (comandos `ioreg`/`system_profiler` exatos).
2. Registre a RTX `10de:2504`: IORegistry path, provider, vendor/device/class/subsys/revision, **`built-in`** (YES/NO/ABSENT/UNKNOWN), **`IOPCITunnelled`** (idem), driver anexado atual (hierarquia provider, sem detach).
3. Salve snapshot sanitizado em `artifacts/macos/` (gitignored — confira `.gitignore`).
4. Crie `docs/macos/hardware/ga106-ioreg.md` com os campos + respostas explícitas built-in/tunnelled + current provider.
5. Rode `python3 scripts/tinygpu-match-check.py` com um snapshot JSON da RTX (formato no `--help`/cabeçalho do script) e anexe o resultado ao doc.
6. Atualize `docs/tinygpu/internal-pcie-gap.md` (addendum: Resultado A/B/C decidido ou ainda em aberto e por quê) e recalcule MAC-COMPUTE-0.
7. Commits pequenos (`docs(macos): ...`), sem push salvo se o usuário pedir; termine com relatório + próximo milestone (UM, sem implementar driver).
