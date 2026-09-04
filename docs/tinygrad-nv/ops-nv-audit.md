# Auditoria `tinygrad/runtime/ops_nv.py` + match `PCIIface` para `10de:2504` (TG0, estática)

> Clones SOMENTE em `/tmp/opencode/tg0-tinygrad`. Nada instalado, nada executado contra hardware.
> SHA base: `tinygrad@e8c8ba1c7751142f7b20d85692afd2da5cef5e84` (`2026-09-04T14:06:04-07:00`), `tinygpu_releases@c0d024f9ff0e1dc8fdf217f255da7101d91e8323`.
> Todas as linhas abaixo são desse SHA salvo indicação contrária.

## 0. Resposta explícita

**Does tinygrad `PCIIface` recognize `10de:2504`? YES.**

- Símbolo: `PCIIface.__init__` (tabela de famílias) + `System.pci_scan_bus` (predicado de match).
- Arquivo+linha+SHA: `tinygrad/tinygrad/runtime/ops_nv.py:560` + `tinygrad/tinygrad/runtime/support/system.py:80` @ `e8c8ba1c7751142f7b20d85692afd2da5cef5e84`.
- Escopo do YES: SOMENTE a camada de enumeração PCI (filtro vendor/máscara/família + `base_class`). NÃO afirma bring-up GSP/compute fim-a-fim do SKU (ver §4).

## 1. Mapa de símbolos `ops_nv.py` / `system.py` / `nvdev.py` / `nv/ip.py`

| Símbolo | Arquivo:linha | Função/papel |
|---|---|---|
| `NVDevice` | `tinygrad/runtime/ops_nv.py:585-848` | `HCQCompiled`: seleciona iface (`_select_iface`), `rm_alloc` device/subdevice/vaspace, cria `compute_gpfifo`/`dma_gpfifo`, `cmdq`, interroga `sm_version`→`arch`, registra `CUDARenderer/PTXRenderer/NVCCRenderer/NAKRenderer` |
| `NVDevice.ifaces` | `tinygrad/runtime/ops_nv.py:586` | `[NVKIface, PCIIface, MOCKIface]` — ordem de tentativa via `DEV=NV:<IFACE>` |
| `NVDevice.is_nvd` | `tinygrad/runtime/ops_nv.py:588` | `isinstance(iface, PCIIface)` — distingue caminho userspace (GSP direto) do caminho driver |
| `NVKIface` | `tinygrad/runtime/ops_nv.py:370-554` | Interface via driver NVIDIA: `/dev/nvidiactl`, `/dev/nvidia-uvm`, `rm_alloc:416`, `rm_control:423`, `uvm:430`, `setup_usermode:434` (classes `HOPPER/TURING_USERMODE_A`, `GPFIFO/COMPUTE/DMA`), `alloc:478` (NVOS02/NVOS32), `map:545` |
| `PCIIface` | `tinygrad/runtime/ops_nv.py:556-581` | Interface userspace direta (única usada com TinyGPU): `vendor=0x10de`, tabela de famílias (§2), `base_class=0x03`, `vram_bar=1`, `dev_impl_t=NVDev`; `rm_alloc/rm_control` delegam a `NVDev.gsp` (`rpc_rm_alloc:574`, `rpc_rm_control:575`) |
| `PCIIfaceBase` | `tinygrad/runtime/support/system.py:253-307` | Base genérica PCI: `__init__:259` (`pci_probe_device` + `reserve_va` + `resize_bar` + `dev_impl_t(pci_dev)`), `alloc:267` (sysmem vs devmem), `free:283`, `map:291`, `p2p_paddrs:288` |
| `PCIDevice` | `tinygrad/runtime/support/system.py:160-226` | PCI Linux: `flock`, unbind driver, VFIO/noiommu, `config`, `bar_fd:213`, `bar_info:216`, `map_bar:219` (mmap `/sys/bus/pci/.../resourceN`), `alloc_sysmem:198`, `read_config:206` |
| `APLRemotePCIDevice` | `tinygrad/runtime/support/system.py:416-447` | PCI macOS/TinyGPU: `ensure_app:420` (pin §5), socket UNIX `tinygpu.sock` + `TinyGPU server` (`__init__:429-439`), `map_bar:412-413`/`bar_info:411` via RPC `MAP_BAR`, `alloc_sysmem:441` via `MAP_SYSMEM_FD` |
| `RemotePCIDevice` / `RemoteCmd` | `tinygrad/runtime/support/system.py:311-414` | Transporte RPC genérico (`PROBE/MAP_BAR/...`:312); `remote_list:360`, `_rpc:376`, `RemoteMMIOInterface:314` |
| `System.pci_scan_bus` | `tinygrad/runtime/support/system.py:58-80` | Enumera PCI (IOKit no macOS:60-71, sysfs no Linux:73-78) e aplica o predicado de match (`:80`) |
| `System.list_devices` | `tinygrad/runtime/support/system.py:82-85` | No macOS retorna `APLRemotePCIDevice`, senão `PCIDevice` (`:85`) |
| `System.pci_probe_device` | `tinygrad/runtime/support/system.py:87-90` | Filtra por `VISIBLE_DEVICES` e instancia `cl(device[:2], pcibus)` |
| `NVDev` | `tinygrad/runtime/support/nv/nvdev.py:74-163` | `__init__:75` (`map_bar(0)` MMIO, `_early_ip_init:97`, `_early_mmu_init:123`); `chip_id` (`NV_PMC_BOOT_0:112`), `chip_details` (`NV_PMC_BOOT_42:113`), `chip_name`/`fw_name`/`mmu_ver` (`:114-116`); `wreg/rreg:92-95`, `include:161` |
| `NVMemoryManager` | `tinygrad/runtime/support/nv/nvdev.py:69-72` | `MemoryManager` com `va_allocator = TLSFAllocator((1<<44), base=0x1000000000)` (`:70`); `on_range_mapped` invalida MMU (`:72`) |
| `NV_FLCN` / `NV_FLCN_COT` | `tinygrad/runtime/support/nv/ip.py:93-284` / `:285-344` | Boot Falcon: `NV_FLCN` (Ampere/Ada, `fmc_boot=False`) extrai ucode do VBIOS + `booter_load` (`prep_ucode:110`, `prep_booter:171`); `NV_FLCN_COT` (Blackwell/GB2, `fmc_boot=True`) via FSP/COT (`init_fmc_image:302`, `kfsp_send_msg:328`) |
| `NV_GSP` (`GSP`) | `tinygrad/runtime/support/nv/ip.py:346-661` | `init_sw:347` (filas RPC, `gpfifo/compute/dma/viddec_class:357-362`), `init_hw:510` (`GSP_INIT_DONE`, golden image), `rpc_rm_alloc:538`, `rpc_rm_control:571`, `rpc_set_page_directory:594`, `init_golden_image:468`, `promote_ctx:457` |
| `GPFifo` | `tinygrad/runtime/ops_nv.py:363-368` | Descritor (`ring/gpput/entries_count/token/put_value`); criado por `NVDevice._new_gpu_fifo:642-666` |
| `NVCommandQueue` | `tinygrad/runtime/ops_nv.py:78-127` | Base `HWQueue`: `nvm:86`, `setup:88`, `wait:97`, `bind:105`, `_submit_to_gpfifo:114` (anel + `gpput` + doorbell `gpu_mmio[0x90//4]`) |
| `NVComputeQueue` (compute) | `tinygrad/runtime/ops_nv.py:128-192` | `exec:135` (QMD + `SEND_PCAS_A`), `signal:159` (release via QMD ou `NVC56F_SEM`), `memory_barrier:129`, `write:180`, `poll_bit:186`, `_submit:192` → `compute_gpfifo` |
| `NVCopyQueue` (DMA) | `tinygrad/runtime/ops_nv.py:194-212` | `copy:199` (`NVC6B5_*_DMA`), `signal:207`, `_submit:212` → `dma_gpfifo` |
| `NVVideoQueue` | `tinygrad/runtime/ops_nv.py:214-239` | NVDEC (`decode_hevc_chunk:215`), `_submit:239` → `vid_gpfifo` |
| `QMD` | `tinygrad/runtime/ops_nv.py:44-76` | Queue Method Data: `ver 5/sz 0x60` se `compute_class >= BLACKWELL_COMPUTE_A` senão `ver 3/sz 0x40` (`:48`) |
| `NVProgram` | `tinygrad/runtime/ops_nv.py:248-339` | `elf_loader` + relocs CUDA, `lib_gpu` alloc, `cbuf_0`, `QMD` build (Blackwell `:292` vs Ampere/Ada `:297`), `max_threads:316` |
| `NVAllocator` | `tinygrad/runtime/ops_nv.py:341-360` | `_alloc:342` → `iface.alloc`, `_do_map:347` → `iface.map`, NVDEC `_encode_decode:349` |
| `NVSignal` | `tinygrad/runtime/ops_nv.py:27-30` | `HCQSignal` com `_sleep` cooperativo |

## 2. Tabela de famílias (source exato)

`tinygrad/tinygrad/runtime/ops_nv.py:556-561` @ `e8c8ba1c`:

```python
class PCIIface(PCIIfaceBase):
  def __init__(self, dev, dev_id):
    # PCIIface's MAP_FIXED mmap will overwrite UVM allocations made by NVKIface, so don't try PCIIface if kernel driver was already used.
    if NVKIface.root is not None: raise RuntimeError("Cannot use PCIIface after NVKIface has been initialized (would corrupt UVM memory)")
    super().__init__(dev, dev_id, vendor=0x10de, devices=((0xff00, (0x2200,0x2400,0x2500,0x2600,0x2700,0x2800,0x2b00,0x2c00,0x2d00,0x2f00)),),
      base_class=0x03, vram_bar=1, va_start=NVMemoryManager.va_allocator.base, va_size=NVMemoryManager.va_allocator.size, dev_impl_t=NVDev)
```

Predicado em `tinygrad/tinygrad/runtime/support/system.py:80` @ `e8c8ba1c`:

```python
return sorted([val for vndr, device, val in all_devs if vndr == vendor and any((device & mask) in devlist for mask, devlist in devices)])
```

Ou seja: `vendor == 0x10de` E `∃ (mask, devlist): (device_id & mask) ∈ devlist`, com `base_class == 0x03` pré-filtrado em `:70/:76`.

## 3. Cálculo bit a bit para `0x2504` (RTX 3060 / GA106)

```
device   = 0x2504 = 0010 0101 0000 0100 (9476)
mask     = 0xFF00 = 1111 1111 0000 0000 (65280)
                  & --------------------------------
masked   = 0x2500 = 0010 0101 0000 0000 (9472)
```

- `0x2504 & 0xFF00 = 0x2500` (o byte baixo `0x04` — revisão/SKU — é zerado pela máscara; compara-se só o byte alto `0x25` — família).
- `0x2500 ∈ (0x2200,0x2400,0x2500,0x2600,0x2700,0x2800,0x2b00,0x2c00,0x2d00,0x2f00)` → `True`.
- Portanto o predicado de `system.py:80` é satisfeito para `(vendor=0x10de, device=0x2504)` (assumindo `base_class 0x03`, i.e. display/VGA/3D, que é o caso da RTX 3060).
- Verificação executada localmente (aritmética Python pura, sem hardware): `(0x2504 & 0xFF00) in devlist == True`.

Conclusão: **YES — `10de:2504` faz match na tabela de famílias do `PCIIface`.**

## 4. Limites do YES (não inventar suporte)

1. O `YES` é SOMENTE enumeração PCI. Bring-up (`NVDev._early_ip_init`, FLCN/GSP, `init_golden_image`, `setup_usermode` com `AMPERE_COMPUTE_B`/`AMPERE_CHANNEL_GPFIFO_A`/`AMPERE_DMA_COPY_B` em `ops_nv.py:439-442` / `ip.py:357`) depende de `chip_id`/`BOOT_42`/firmware, não do `device_id` PCI.
2. `grep 2504|GA106|RTX 3060` no clone `e8c8ba1c` retorna zero em docs/código (único `GA106` é a constante de header `NV2080_CTRL_MC_ARCH_INFO_IMPLEMENTATION_GA106` em `nv_570.py:23415` / `nv_580.py:24463` / `nv_610.py:25044` — não é lista de suporte).
3. `nvdev.py:114-116` mapeia `architecture 0x17→GA1/ga102`, `0x19→AD1/ad102`, `0x1b→GB2/gb202`; `ip.py:172-173,426-428` só pineiam `sha` de firmware `ga102/ad102/gb202`. GA106 cai em `GA1/ga102` por arquitetura (esperado: GA106 é Ampere), mas não há `sha`/quirk `ga106`-específico no clone — por isso `support-matrix.md` marca SKU verificado como `UNKNOWN` e bring-up GA106-específico como `UNCERTAIN`.
4. No macOS, passar no filtro ainda exige a TinyGPU.app enumerar o dispositivo via DriverKit + socket (`APLRemotePCIDevice:429-439`, `RemoteCmd.PROBE`, `remote_list:360`) — fora do escopo TG0 testar.

## 5. Pin TinyGPU.app (para rastreabilidade do transporte macOS)

- `tinygrad/tinygrad/runtime/support/system.py:417`: `APP_PATH = "/Applications/TinyGPU.app/Contents/MacOS/TinyGPU"`.
- `tinygrad/tinygrad/runtime/support/system.py:420-427`: `ensure_app()` com `commit = "c0d024f9ff0e1dc8fdf217f255da7101d91e8323"` (`:421`), `fetch('https://github.com/tinygrad/tinygpu_releases/raw/{commit}/TinyGPU.zip')` (`:426`), `ditto -xk ... /Applications` + `{APP_PATH} install` (`:426-427`) @ `e8c8ba1c`.
- O pin bate com `tinygpu_releases@c0d024f9` (ver `docs/tinygpu/support-matrix.md`).
