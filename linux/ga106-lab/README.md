# linux/ga106-lab/ — SOMENTE LEITURA (Fase 0)

Laboratório de observação passiva da GA106 no Linux. **Nenhum binário aqui pode
escrever no hardware.** Esta é a regra que protege a bancada.

## Permitido

- `lspci -nnv`, `lspci -xxxx` (inspeção visual, sem escrita).
- Leitura de sysfs com `O_RDONLY`: `config`, `resource*`, `vendor`, `device`, `driver` (readlink).
- `mmap` com `PROT_READ` documentado, quando justificado em `docs/`.
- `dmesg` / `journalctl` / logs Nouveau / NVK.

## Proibido (Fase 0) — reafirmação do README raiz

Sem escrita MMIO, sem clocks/voltagem/fan, sem reset/FLR, sem command buffer
(PUSH/doorbell/FIFO), sem firmware (GSP/PMU/SEC2/NVDEC), sem power state
(D0/D3/PM/ASPM/link), sem unbind/bind/`driver_override`, sem `/dev/mem`,
sem `O_RDWR` em recurso PCI, sem `mmap(PROT_WRITE)` em BAR.

## Estado atual

`ga106-lab info` implementado (Fase 0, somente leitura). Fontes em `src/`:

- `pci_discovery.{hpp,cpp}` — varre `/sys/bus/pci/devices` com abertura
  read-only, filtra vendor `0x10DE`, decodifica BARs via `.../resource`,
  driver via readlink, módulos via `/proc/modules`, IOMMU via readlink,
  NUMA via `numa_node`, VRAM via fallback opcional somente-leitura.
- `drm_discovery.{hpp,cpp}` — cruza `/sys/class/drm/card*` e `renderD*`
  com o BDF observado (symlink `device` + `MAJOR:MINOR` do arquivo `dev`).
- `main.cpp` — comando `info`, secoes GPU/Architecture/PCI/System/DRM/VRAM.

Tabela arquitetura/chip (fonte: banco pci-ids, `pci.ids`, fabricante 10de):
GA106 = `0x2501/2503/2504/2505/2509/2544` + mobile `0x2520/2521` → Ampere/GA106;
`0x2484/2486/2487/2488/2489/249c/24a0` → Ampere/GA104 (not GA106);
demais `10de` → `NVIDIA GPU`/`unknown`. Sem dado → `"unknown"`, sem chute.

## Build e uso

```sh
./scripts/build.sh            # configura, compila e roda ctest
./build/linux/ga106-lab/ga106-lab info
```

## Exemplo de saida (bancada GA106 real, 0000:01:00.0)

```
[ga106-lab] info for 0000:01:00.0
GPU:
  BDF: 0000:01:00.0
  Vendor: 0x10de
  Device: 0x2504
  Subsystem: 0x10de:0x2504
  Revision: 0xa1
  Class: 0x030000
  Modalias: pci:v000010DEd00002504sv000010DEsd00002504bc03sc00i00
Architecture:
  Arch: Ampere
  Chip: GA106
PCI:
  Driver: nvidia
  Modules: nvidia_drm, nvidia_uvm, nvidia_modeset, nvidia
  BAR0: start=0xfb000000 end=0xfbffffff size=0x1000000 (16M) flags=0x40200 (mem, 32-bit, non-prefetchable)
  BAR1: start=0x7800000000 end=0x7bffffffff size=0x400000000 (16G) flags=0x14220c (mem, 64-bit, prefetchable)
  BAR2: absent
  BAR3: start=0x7c00000000 end=0x7c01ffffff size=0x2000000 (32M) flags=0x14220c (mem, 64-bit, prefetchable)
  BAR4: absent
  BAR5: start=0xf000 end=0xf07f size=0x80 (128 B) flags=0x40101 (io)
  ROM: start=0xfc000000 end=0xfc07ffff size=0x80000 (512K) flags=0x46200
System:
  IOMMU group: 10
  NUMA node: -1
  Kernel driver: nvidia
DRM:
  card1: /dev/dri/card1 (226:1) slot=0000:01:00.0
  renderD128: /dev/dri/renderD128 (226:128) slot=0000:01:00.0
VRAM:
  Total: 12288 MiB
```

## Layout (quando autorizado)

```
ga106-lab/
├── README.md          # este arquivo (regras read-only)
├── CMakeLists.txt     # tools read-only, linkadas em nvidia_common
└── src/
    ├── pci_scan.cpp   # enumera BDFs via sysfs O_RDONLY (proposta futura)
    └── bar_view.cpp   # imprime BARs sem mapear p/ escrita (proposta futura)
```
