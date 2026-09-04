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
- `json_flat.{hpp,cpp}` — parser JSON manual mínimo (sem dependências) que
  achata `baseline.json` em mapa ordenado para o `--compare` determinístico.
- `baseline_compare.{hpp,cpp}` — diff por classes
  IDENTITY/STATIC/SEMI_STATIC/DYNAMIC; `--strict` só barra em IDENTITY/STATIC.
- `lab_status.{hpp,cpp}` — `lab-status` read-only (ownership, iGPU `1002:1638`,
  veredito) + bloco `lab_readiness` reutilizado pelo `baseline`.
- `main.cpp` — comando `info`, secoes GPU/Architecture/PCI/System/DRM/VRAM.

Tabela arquitetura/chip (fonte: banco pci-ids, `pci.ids`, fabricante 10de):
GA106 = `0x2501/2503/2504/2505/2509/2544` + mobile `0x2520/2521` → Ampere/GA106;
`0x2484/2486/2487/2488/2489/249c/24a0` → Ampere/GA104 (not GA106);
demais `10de` → `NVIDIA GPU`/`unknown`. Sem dado → `"unknown"`, sem chute.

## Build e uso

```sh
./scripts/build.sh            # configura, compila e roda ctest
./build/linux/ga106-lab/ga106-lab info
./build/linux/ga106-lab/ga106-lab baseline --stdout
./build/linux/ga106-lab/ga106-lab baseline --out-dir /tmp/ga106-baseline
./build/linux/ga106-lab/ga106-lab baseline --compare /tmp/ga106-old/baseline.json /tmp/ga106-new/baseline.json [--strict]
```

## baseline — snapshot reproduzível (somente leitura)

`ga106-lab baseline [--out-dir DIR] [--stdout]` agrega `pci_discovery` /
`drm_discovery` (mesmas fontes do `info`, sem quebrar nada) + leituras
`O_RDONLY` de sysfs/proc + saída de ferramentas de inspeção via `popen`
(`lspci -vv`, `nvidia-smi`, `modinfo`, `vulkaninfo --summary` com timeout,
`journalctl -k`). Nenhum estado do dispositivo é alterado; campos ausentes
viram `"unknown"` e ferramentas ausentes nunca fazem o verbo falhar.

Saída padrão: `artifacts/baselines/<timestamp>/baseline.txt` (humano, em
seções) + `baseline.json` (mesmos campos, serializado à mão sem dependência
externa), com `<timestamp>` em UTC `%Y%m%d-%H%M%S`. Com `--out-dir DIR`, os
dois arquivos vão direto para `DIR/`. Com `--stdout`, o `baseline.txt`
também é impresso. `artifacts/` não é versionado (ver `.gitignore`).

O snapshot é reproduzível: reexecute o mesmo verbo no mesmo host e compare
`baseline.txt`/`baseline.json` para detectar deriva (troca de driver,
link PCIe, conectores, VRAM em uso, holders via `/proc` fd, logs filtrados).
Seções: Meta, Identity (BDF/vendor/device/subsys/rev/arch/chip/driver/UUID
via `/proc/driver/nvidia/gpus/*/information`), PCI (BARs + `lspci -vv` +
`current_link_*`/`max_link_*` + `resizable_bar*` + IOMMU), Driver (versão
`/proc/driver/nvidia/version` + `modinfo` + flavor open/proprietary via
`"Open"` + params `/sys/module/nvidia*/parameters` + userspace + GPU
Firmware exposto), DRM (nós + conectores `status`/`modes` + driver DRM),
Memory (VRAM total/used via `nvidia-smi` + abertura BAR1 + resizable —
nota no relatório: `BAR1 size != VRAM size não implica erro`), GSP (só
campos expostos, sem interagir), Vulkan (excerto opcional), Logs
(`journalctl -k` filtrado por `NVRM|nvidia|nvidia-drm|GSP|PCIe|IOMMU|Xid|GPU`,
máx 100 linhas), In-use (`nvidia-smi` compute-apps + holders `/dev/dri` via
`/proc/*/fd`, só leitura, nenhuma ação sobre processos).

## baseline --compare — diff determinístico (somente leitura)

`ga106-lab baseline --compare OLD.json NEW.json [--strict]` achata os dois
`baseline.json` com parser manual mínimo próprio (sem dependência externa,
ver `src/json_flat.{hpp,cpp}`) em mapa ordenado — comparação determinística
— e classifica cada chave em `IDENTITY` (BDF, vendor/device/subsys/rev,
UUID), `STATIC` (layout de BARs, `max_link_*`, resizable, `bar1_aperture`),
`SEMI_STATIC` (versão de driver, firmware GSP exposto, VBIOS, kernel e demais
configs) ou `DYNAMIC` (link atual, VRAM em uso, conectores, processos/holders,
logs, vulkan, `lab_readiness`; inclui `temp`/`clocks` se um dia coletados).
Saída em seções com `unchanged` ou diffs `a → b`; `meta.timestamp_utc` é
ignorado (sempre difere). Diferenças `DYNAMIC` nunca são erro crítico.
`--strict` retorna exit != 0 **apenas** se `IDENTITY` ou `STATIC` mudar, com
mensagem explicando (modo normal sempre retorna 0 em compare bem-sucedido).

```sh
./build/linux/ga106-lab/ga106-lab baseline --compare /tmp/ga106-old/baseline.json /tmp/ga106-new/baseline.json
./build/linux/ga106-lab/ga106-lab baseline --compare /tmp/ga106-old/baseline.json /tmp/ga106-new/baseline.json --strict; echo "exit=$?"
./build/linux/ga106-lab/ga106-lab lab-status
```

Exemplo real (dois snapshots da bancada, segundos de diferença):

```
[ga106-lab] baseline compare
  old: /tmp/ga106-snap-old/baseline.json
  new: /tmp/ga106-snap-new/baseline.json
  mode: normal
IDENTITY (0 diferenca(s)):
  unchanged
STATIC (0 diferenca(s)):
  unchanged
SEMI_STATIC (0 diferenca(s)):
  unchanged
DYNAMIC (1 diferenca(s)):
  pci.link_current_speed: 2.5 GT/s PCIe → 5.0 GT/s PCIe
Summary: unchanged=159 identity=0 static=0 semi_static=0 dynamic=1
Note: meta.timestamp_utc ignorado (sempre difere entre snapshots)
Note: diferencas DYNAMIC nunca sao erro critico
Result: differences found (nao critico em modo normal; use --strict para barrar em IDENTITY/STATIC)
```

(O link PCIe varia em runtime por economia de energia — exatamente o tipo de
ruído que a classe `DYNAMIC` isola do gating `--strict`.)

## lab-status — ownership read-only + veredito (somente leitura)

`ga106-lab lab-status` responde "quem usa a RTX agora?" sem tocar em nada.
Alvo fixo `0000:01:00.0` (RTX 3060 `10de:2504` GA106); fontes sempre
read-only: driver via readlink, `boot_vga`, `fb` via
`/sys/class/graphics/fb*/name` (+ hint `dmesg | grep fbcon`, só leitura),
nós DRM + `DRM primary` (heurística via holders `/proc`; `unknown` se não
detectável — master DRM não é exposto pelo sysfs), processos GNOME via scan
`/proc` dos comms (+ `nvidia-smi` compute-apps opcional), conectores ativos
via `/sys/class/drm/card*-*/status`+`enabled`. A alternativa é a Cezanne
`1002:1638` (driver + card DRM + conectores); se ausente → `NOT PRESENT`.

Veredito fail-closed: `BLOCKED` por padrão; `READY_FOR_NEXT_PHASE` somente se
**todas** valerem — iGPU presente E sessão gráfica na iGPU E RTX sem conector
ativo usado E sem clientes desktop no alvo. Outra GPU sozinha nunca implica
`READY`. Exit sempre 0 em coleta bem-sucedida (relato, não gate).

### Modelo dual: independência do desktop vs prontidão MMIO (Fase 0b)

Dois conceitos separados, ambos somente leitura, impressos no bloco
`Readiness split`:

- `Desktop independence: READY` quando **todas** valerem — render dominante
  do compositor na AMD (`alt_render_fds > target_render_fds`, mínimo
  `alt>=1`) E fb owner AMD/amdgpu E monitor ativo na AMD (`alt_active>=1`)
  E RTX com 0 conectores ativos E `boot_vga` do alvo=`0`.
- `MMIO readiness: BLOCKED` **sempre** nesta fase, com razões explícitas:
  ownership PCI com driver NVIDIA vinculado (unbind proibido) + SSH não
  testado externamente + teste de boot sem NVIDIA pendente. O hook
  `mmio_release_conditions_met()` centraliza as condições futuras e hoje
  retorna sempre `false` — nenhum caminho afrouxa isso agora.
- `PCI ownership: <driver> (...)` resume driver vinculado + `boot_vga`
  alvo/alt a partir de campos já coletados (nunca opera hardware).

**Por que FDs residuais não bloqueiam o desktop.** Estado observado
pós-reboot: `boot_vga` AMD=`1`, fb só `amdgpudrmfb`, RTX com 0 conectores,
`glxinfo` na AMD, render dominante AMD (ex.: 20 fds `renderD129` vs 2 fds
`renderD128`) — mas o `gnome-shell` mantém ~2 fds `renderD128` + `card1` (+
`/dev/nvidia*`). Justificativa registrada no código: isso é compatível com
a enumeração multi-GPU do Mutter, que lista todos os nós DRM; só a
dominância `renderD*` (GL/EGL real) conta como sinal. Nada além do
observável é afirmado (sem alegar mecanismo interno). O `Verdict` legado e
o `desktop_gpu` seguem fail-closed de propósito (qualquer toque no alvo
bloqueia) — o sinal novo vive em `desktop_independence` +
`desktop_render_note`.

Exemplo real pós-reboot (trecho do split, bancada dual AMD boot VGA + RTX
sem conectores):

```
Readiness split (Fase 0, somente leitura):
  Desktop independence: READY
    - render dominante do compositor na AMD: sim (fds renderD* do desktop: alvo=2 alt=20; exige alt>=1 e alt>alvo)
    - fb owner AMD/amdgpu: sim (amdgpudrmfb)
    - monitor ativo na AMD: sim (1: card2-HDMI-A-2)
    - RTX com 0 conectores ativos: sim (0: nenhum)
    - boot_vga do alvo=0: sim (alvo=0 alt=1)
    - nota: fds residuais do compositor no alvo (...) NAO bloqueiam — compativeis com enumeracao multi-GPU do Mutter (...)
    - Desktop independence: READY (todas as condicoes valem; fds residuais de enumeracao nao bloqueiam)
  MMIO readiness: BLOCKED
    - PCI ownership: driver 'nvidia' ainda vinculado ao alvo 0000:01:00.0 (unbind proibido nesta fase)
    - SSH externo nao testado a partir de outra maquina (recuperacao remota nao verificada)
    - teste de boot sem NVIDIA pendente (inicializacao apenas com AMD nao verificada de ponta a ponta)
    - MMIO readiness: BLOCKED (fase atual nao autoriza liberacao; ver hook mmio_release_conditions_met)
  PCI ownership: nvidia (driver vinculado ao alvo 0000:01:00.0; boot_vga alvo=0 alt=1)
```

O `baseline` agrega o bloco `lab_readiness` com os mesmos sinais
(`alternative_gpu_present`, `desktop_gpu`, `target_gpu_used_by_desktop`,
`target_gpu_active_connectors`, `status` em
`BLOCKED`/`PARTIALLY_READY`/`READY_FOR_NEXT_PHASE`/`unknown`, mais
`desktop_independence` em `READY`/`BLOCKED`/`unknown` e `mmio_readiness`
sempre `BLOCKED` nesta fase; `null`/`unknown` quando não detectável, sem
inventar). No `--compare`, chaves `lab_readiness.*` são classe `DYNAMIC`
(comparação continua determinística: mapa ordenado, `meta.timestamp_utc`
ignorado).

Exemplo real (bancada atual: RTX é boot VGA, fb `nvidia-drmdrmfb`, 2
conectores ativos, GNOME rodando, sem `1002:1638`):

```
[ga106-lab] lab-status for 0000:01:00.0
Target GPU:
  BDF: 0000:01:00.0 (present)
  Vendor/Device: 0x10de / 0x2504 (esperado 10de:2504 GA106: match)
  Subsystem: 0x10de:0x2504
  Revision: 0xa1
Ownership (read-only):
  Kernel driver: nvidia (via driver readlink)
  boot_vga: 1
  fb owner: nvidia-drmdrmfb (via /sys/class/graphics/fb*/name)
  fbcon hint: unknown (dmesg indisponivel ou sem linhas fbcon)
  DRM nodes:
    card1 /dev/dri/card1 226:1 slot=0000:01:00.0
    renderD128 /dev/dri/renderD128 226:128 slot=0000:01:00.0
  DRM primary: card1 (via holder 2944 gnome-shell)
    note: heuristica read-only: unico card do alvo com fd do compositor; master DRM nao e exposto pelo sysfs
  Active connectors (status+enabled):
    card1-DP-1: status=connected enabled=enabled [ACTIVE]
    card1-DP-2: status=disconnected enabled=disabled
    card1-DP-3: status=disconnected enabled=disabled
    card1-HDMI-A-1: status=connected enabled=enabled [ACTIVE]
  ...
Alternative GPU (1002:1638 Cezanne):
  NOT PRESENT (nenhum 0x1002:0x1638 em /sys/bus/pci/devices)
Desktop GPU: 0000:01:00.0
Verdict: BLOCKED
  - alvo 0000:01:00.0 confere (10de:2504 GA106)
  - kernel driver do alvo: nvidia
  - boot_vga do alvo: 1
  - fb owner: nvidia-drmdrmfb
  - conectores ativos no alvo: 2 (card1-DP-1, card1-HDMI-A-1)
  - processos desktop visiveis (/proc): 10
  - holders desktop nos fds DRM do alvo: sim
  - alternativa 1002:1638 NOT PRESENT (nenhum 0x1002:0x1638 em /sys/bus/pci/devices)
  - sessao grafica na iGPU: nao
  - clientes desktop no alvo: sim
  - compute-apps nvidia-smi: 0 processo(s) desktop
  - veredito BLOCKED: sem iGPU alternativa (e presenca isolada de outra GPU jamais implicaria READY)
```

E no `baseline.json`:

```json
"lab_readiness": {
  "alternative_gpu_present": false,
  "desktop_gpu": "0000:01:00.0",
  "target_gpu_used_by_desktop": true,
  "target_gpu_active_connectors": 2,
  "status": "BLOCKED",
  "desktop_independence": "BLOCKED",
  "mmio_readiness": "BLOCKED"
}
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
