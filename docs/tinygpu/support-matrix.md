# TinyGPU — support matrix (TG0, somente pesquisa/análise estática)

> Fase TG0. Nenhum hardware tocado, nenhum driver instalado, nenhum código executado contra GPU.
> Clones usados SOMENTE em `/tmp/opencode/tg0-tinygrad` (fora deste repo).
> Este repo recebe SÓ `.md`. `docs/README.md` não foi editado (índice com o coordenador).

## 1. Fontes pinadas (SHA + data)

| Fonte | SHA (clone `--depth 1`) | Data do commit clonado | Onde foi clonado |
|---|---|---|---|
| `tinygrad/tinygrad` | `e8c8ba1c7751142f7b20d85692afd2da5cef5e84` | `2026-09-04T14:06:04-07:00` (`Add regression coverage for assignment and callify (#17971)`) | `/tmp/opencode/tg0-tinygrad/tinygrad` |
| `tinygrad/tinygpu_releases` | `c0d024f9ff0e1dc8fdf217f255da7101d91e8323` | `2026-03-31T11:54:09+03:00` (`signed`) | `/tmp/opencode/tg0-tinygrad/tinygpu_releases` |

- Data do clone (UTC): `2026-09-04`.
- `tinygpu_releases` contém SOMENTE `TinyGPU.zip` (`1634625` bytes no clone).
  - `sha256(TinyGPU.zip)` do artefato clonado (informativo, pode variar por re-sign): `0c47285e2232643210555cf30ce08289b9e55da261c300e0c82e8448a359a21f`.
- Se `tinygpu_releases` não existisse, o fallback previsto era registrar + `websearch`. Não foi preciso: o repo existe (`https://github.com/tinygrad/tinygpu_releases`).

Todas as referências `arquivo:função/símbolo:linha` abaixo são relativas ao clone `tinygrad@e8c8ba1c` salvo indicação contrária.

## 2. Pin da TinyGPU.app — `APLRemotePCIDevice.ensure_app()`

| Item | Valor |
|---|---|
| Símbolo | `APLRemotePCIDevice.ensure_app()` |
| Arquivo | `tinygrad/tinygrad/runtime/support/system.py:416-427` (`class APLRemotePCIDevice:416`, `ensure_app:420`, pin `commit:421`, `fetch:426`, `install:427`) @ `e8c8ba1c` |
| `APP_PATH` | `/Applications/TinyGPU.app/Contents/MacOS/TinyGPU` (`system.py:417`) |
| Commit pinado (const no source) | `c0d024f9ff0e1dc8fdf217f255da7101d91e8323` (`system.py:421`: `commit = "c0d024f9..."`) |
| Nome do zip em cache | `TinyGPU_{commit}.zip` (`system.py:422`: `app_name = f"TinyGPU_{commit}.zip"`) |
| URL de download | `https://github.com/tinygrad/tinygpu_releases/raw/{commit}/TinyGPU.zip` (`system.py:426`: `fetch(f'https://github.com/tinygrad/tinygpu_releases/raw/{commit}/TinyGPU.zip', name=app_name)`) |
| Instalação | `ditto -xk {zip} /Applications` + `{APP_PATH} install` (`system.py:426-427`); o `install` dispara o prompt do DriverKit |
| Bootstrap | `extra/setup_tinygpu_osx.sh:1-10` chama `APLRemotePCIDevice.ensure_app()`; `docs/tinygpu.md:22` instrui `curl .../extra/setup_tinygpu_osx.sh \| sh` |
| Consistência do pin | O `commit` em `system.py:421` (`c0d024f9...`) é idêntico ao `HEAD` do clone `tinygpu_releases@c0d024f9` — pin CONFIRMED |
| Cache local | `tinygrad/helpers.py:_ensure_downloads_dir:453`, `fetch:464` — o zip vai para o dir de downloads; `ensure_app` retorna cedo se zip em cache E `APP_PATH` existe (`system.py:423`) |

## 3. Requisitos TinyGPU (support matrix)

Fonte primária: `docs/tinygpu.md:1-61` @ `e8c8ba1c`. Fonte secundária: `docs/runtime.md:7,77-81` @ `e8c8ba1c`.

| Requisito | Valor no source | Símbolo/arquivo:linha |
|---|---|---|
| macOS mínimo | `macOS (13.0+)` | `docs/tinygpu.md:7` |
| Transporte | `USB4/Thunderbolt port`; `Plug the supported GPU into your Mac over USB4/Thunderbolt` | `docs/tinygpu.md:8,15` |
| GPU AMD | `AMD RDNA3+` | `docs/tinygpu.md:9` |
| GPU NVIDIA | `NVIDIA Ampere+` | `docs/tinygpu.md:9`; `docs/runtime.md:7`: `Ampere/Ada/Blackwell series GPUs` |
| Variável de dispositivo | `DEV={AMD\|NV} python3 -m tinygrad.llm` | `docs/tinygpu.md:58` |
| Habilitação do driver | Prompt `would like to use a new driver extension` → `System Settings > General > Login Items & Extensions > Driver Extensions` toggle ON | `docs/tinygpu.md:29-31` |
| Compilador AMD (macOS) | `extra/setup_hipcomgr_osx.sh` | `docs/tinygpu.md:37-39` |
| Compilador NV (macOS) | `Docker Desktop` + `extra/setup_nvcc_osx.sh`; `export PATH="$HOME/.local/bin:$PATH"` (`nvcc`/`nvdisasm` via shim Docker `cuda-nvcc:12.8`) | `docs/tinygpu.md:43-53`, `extra/setup_nvcc_osx.sh:1-31` |
| Compiladores NV em código | `NVRTCCompiler`/`NVCCCompiler` (CUDA C) e `PTXCompiler`/`NVPTXCompiler` (PTX), com `osx_docker_cmd = docker run ... ghcr.io/tinygrad/cuda-arm64:v2.3` no macOS | `tinygrad/runtime/support/compiler_cuda.py:9,46-100` |

## 4. Interfaces NV (`DEV=NV`)

Fonte: `docs/runtime.md:77-81` + `tinygrad/runtime/ops_nv.py:585-586` (`NVDevice.ifaces = [NVKIface, PCIIface, MOCKIface]`) @ `e8c8ba1c`.

| Interface (`DEV=NV:<IFACE>`) | Símbolo | Arquivo:linha | Papel | macOS + TinyGPU? |
|---|---|---|---|---|
| `NVK` | `NVKIface` | `tinygrad/runtime/ops_nv.py:370-554` | Usa o driver NVIDIA (`/dev/nvidiactl`, `/dev/nvidia-uvm`, `rm_alloc`/`rm_control`/`uvm`) | NÃO — requer driver NVIDIA (Linux). Não é o caminho TinyGPU |
| `PCI` | `PCIIface` (+ `PCIIfaceBase`, `NVDev`) | `tinygrad/runtime/ops_nv.py:556-581`; `tinygrad/runtime/support/system.py:253-307`; `tinygrad/runtime/support/nv/nvdev.py:74-163` | Driver userspace direto: mapeia BARs, boota FLCN/GSP via MMIO, `rpc_rm_alloc/control` via GSP | SIM — é o caminho TinyGPU no macOS (BARs via DriverKit, ver §5) |
| `MOCK` | `MOCKIface` | `tinygrad/runtime/ops_nv.py:583` (`class MOCKIface(NVKIface): count = 1`) | Mock para testes, sem hardware | Teste apenas |

Seleção: `tinygrad/device.py:_select_iface:370-377` filtra `NVDevice.ifaces` por `DEV.target(...).interface` (`NVK`/`PCI`/`MOCK`); nunca cai em `MOCK` por fallback.
No macOS, `System.list_devices:82-85` (`tinygrad/runtime/support/system.py:85`) retorna `APLRemotePCIDevice` em vez de `PCIDevice` — ou seja, `PCIIface` no macOS fala com a TinyGPU.app via socket UNIX (`APLRemotePCIDevice:429-439`), não via `/sys/bus/pci`.

## 5. Linha RTX 3060 / GA106 / `10de:2504` — arquitetura suportada vs SKU verificado

> Nível de confiança: `CONFIRMED` = afirmado no source; `LIKELY` = implicado por constante/código sem afirmação direta; `UNCERTAIN` = insuficiente; `UNKNOWN` = nenhuma evidência no clone. Nada inventado.

| Afirmação | Nível | Prova (arquivo:linha@SHA) |
|---|---|---|
| Arquitetura Ampere é suportada pelo backend NV | CONFIRMED | `docs/tinygpu.md:9` (`NVIDIA Ampere+`) + `docs/runtime.md:7` (`Ampere/Ada/Blackwell series GPUs`) @ `e8c8ba1c` |
| Família PCI `0x2500` (máscara `0xff00`) é aceita pela enumeração `PCIIface` | CONFIRMED | `tinygrad/runtime/ops_nv.py:560`: `devices=((0xff00, (0x2200,...,0x2500,...)),)`; match em `tinygrad/runtime/support/system.py:80`: `(device & mask) in devlist` @ `e8c8ba1c`. Ver cálculo em `docs/tinygrad-nv/ops-nv-audit.md` |
| `0x2504 & 0xff00 == 0x2500` ⇒ `10de:2504` passa no filtro de família | CONFIRMED (aritmética + código) | Cálculo bit a bit em `docs/tinygrad-nv/ops-nv-audit.md`; resposta `YES` restrita à camada de enumeração PCI |
| SKU `10de:2504` (RTX 3060 12 GB) explicitamente listado/testado no tinygrad | UNKNOWN | `grep 2504\|GA106\|RTX.?3060` no clone `e8c8ba1c` retorna ZERO menções em docs/código (só `0x2500` familiar em `ops_nv.py:560` e `IMPLEMENTATION_GA106` no autogen, ver abaixo). Nenhum `support-matrix`/quirk por SKU |
| `GA106` como implementação de chip suportada pelo bring-up GSP | UNCERTAIN | `tinygrad/runtime/autogen/nv_570.py:23415` (idem `nv_580.py:24463`, `nv_610.py:25044`): `NV2080_CTRL_MC_ARCH_INFO_IMPLEMENTATION_GA106 = 6` — é constante de header, NÃO afirmação de suporte/bring-up. `nvdev.py:114-116` deriva `chip_name`/`fw_name` de `NV_PMC_BOOT_0/42` (`GA1`→`ga102`, `AD1`→`ad102`, `GB2`→`gb202`); `ip.py:172-173,426-428` só tem `sha` de firmware para `ga102/ad102/gb202` — nenhum `sha`/`fw_name` `ga106`-específico no clone |
| RTX 3060 / GA106 funciona fim-a-fim via TinyGPU.app | UNKNOWN | Passar no filtro PCI ≠ bring-up GSP + GPFIFO + compute validados. Sem SKU verificado no source, sem inventar: tratar como NÃO verificado até teste real (fora do escopo TG0) |

Leitura correta: **arquitetura (Ampere) suportada = CONFIRMED; SKU `10de:2504`/RTX 3060 verificado = UNKNOWN; `GA106` bring-up específico = UNCERTAIN.** O `YES` da auditoria responde SOMENTE "o `PCIIface` reconhece `10de:2504` na enumeração?" — não é certificado de funcionamento fim-a-fim.
