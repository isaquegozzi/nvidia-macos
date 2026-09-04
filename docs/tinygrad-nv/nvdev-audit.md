> Status: Fase TG0 — SOMENTE análise estática. Nada executado contra hardware.
> Clone: `/tmp/opencode/tg0-tinygrad/tinygrad` (tinygrad/tinygrad, `--depth 1`).
> SHA: `e8c8ba1c7751142f7b20d85692afd2da5cef5e84` (`e8c8ba1c`, `2026-09-04`).
> Gap de referência: `docs/ga106/initialization-map.md` (Etapas 0–7, Nouveau corrigido pelo código).
> Sem código copiado: apenas símbolos, `arquivo:função:linha`, registradores e SHAs de firmware.

# NVDev audit — init, memory, submission, GSP

## 0. Veredito

**NVDev inicializa NVIDIA sem kernel driver NVIDIA? CONFIRMED.**

Prova mínima (tudo @ `e8c8ba1c`):

- `tinygrad/runtime/support/system.py:PCIDevice.__init__:168-170` faz `unbind` de qualquer driver ligado (`.../driver/unbind`) e aborta se ainda ligado (`:170` `raise ... Driver is bound`). Ou seja, exige GPU **sem** `nvidia.ko`/`nouveau`/`vfio-pci` mandatório.
- `tinygrad/runtime/support/system.py:PCIDevice.__init__:176-194` usa VFIO **opcional** (`getenv("VFIO",0)` + `System.vfio`); fallback `:194` é só `.../enable <- "1"` via sysfs. Nenhum `/dev/nvidia*` aqui.
- `tinygrad/runtime/ops_nv.py:PCIIface.__init__:559` recusa coexistência com driver: `if NVKIface.root is not None: raise ... would corrupt UVM memory`. `PCIIface` (`:556-561`) usa `dev_impl_t=NVDev`, e `rm_alloc/rm_control` (`:574-575`) delegam a `NVDev.gsp.rpc_*` (MMIO+RPC), não a ioctl `/dev/nvidiactl`.
- `tinygrad/runtime/support/nv/nvdev.py:NVDev.__init__:75-76` abre PCI por `pci_dev.map_bar(0, fmt='I')` (MMIO BAR0 via `PCIDevice.map_bar` → `mmap(resource0)` em `system.py:PCIDevice.map_bar:219-222`). Nenhum `FileIOInterface("/dev/nvidia*")` em `nvdev.py` ou `nv/ip.py`.
- Contraste: `NVKIface.__init__:388-390` abre `/dev/nvidiactl`, `/dev/nvidia-uvm` (`ops_nv.py:NVKIface:388-390`) — esse **sim** depende de `nvidia.ko`. `PCIIface` é o caminho oposto.

Mecanismo PCI usado pelo NVDev (resumo):

| Caminho | Transporte | Arquivo:função:linha @ `e8c8ba1c` |
|---|---|---|
| Linux local (default NVDev) | sysfs `config`/`resourceN` + `mmap` BAR; VFIO só se `VFIO=1` | `system.py:PCIDevice.__init__:160-196`, `read_config:206`, `write_config_flush:208-210`, `bar_fd:213`, `bar_info:216`, `map_bar:219`, `alloc_sysmem:198`, `reset:205`, `resize_bar:223` |
| Linux VFIO (opcional) | `vfio-pci` + `noiommu` + MSI eventfd | `system.py:PCIDevice.__init__:176-193`, `System.vfio:33-42` |
| Remoto genérico | socket TCP RPC (`PROBE/MAP_BAR/MMIO_READ/...`) | `system.py:RemotePCIDevice:334-414`, `RemoteCmd:311-312`, `RemoteMMIOInterface:314-332` |
| macOS TinyGPU | UNIX socket `tinygpu.sock` → TinyGPU.app (DriverKit só mapeia BARs) | `system.py:APLRemotePCIDevice:416-447`, `map_bar:412-413`, `alloc_sysmem:441-447`, `ensure_app:420-427` pin `c0d024f9` |
| Enumeração | sysfs `/sys/bus/pci/devices` (Linux) / IOKit `IOPCIDevice` (macOS) + predicado vendor/máscara/`base_class` | `system.py:System.pci_scan_bus:58-80`, `list_devices:82-85`, `pci_probe_device:87-90`; tabela `ops_nv.py:PCIIface.__init__:560` |

Dependência de `nvidia.ko`? **Não para NVDev/PCIIface.** Dependência é de `linux-firmware` (via `fetch_fw`, ver §5) + VBIOS da placa (para ucode Falcon), não de driver kernel NVIDIA.

Nº etapas cobertas (das 10 pedidas): **10/10 genérico Ampere em `tinygrad@e8c8ba1c`; 0/10 com quirk/SHA `ga106`-específico** (usa `ga102` por arquitetura — ver `ga106-gap-analysis.md`).

## 1. GPU init — `nvdev.py:NVDev` + `nv/ip.py`

Ordem real em `nvdev.py:NVDev.__init__:75-86`:

```
map_bar(0) MMIO → _early_ip_init → _early_mmu_init → is_booting=False
→ flcn.init_sw + gsp.init_sw → flcn.init_hw + gsp.init_hw
```

Depois `ops_nv.py:NVDevice.__init__:590-641` continua: `rm_alloc` device/subdevice/vaspace, `setup_usermode/vm`, channel_group/ctxshare, GPFIFOs, `cmdq`, `_query_gpu_info`, `_setup_gpfifos`.

| Item | Arquivo:função:linha @ `e8c8ba1c` | O que faz | Regs / FW |
|---|---|---|---|
| BAR0 MMIO | `nvdev.py:NVDev.__init__:76`, `133` (`map_bar(1)`+`map_bar(0)` de novo no `_early_mmu_init`) | `pci_dev.map_bar(0, fmt='I')` → `self.mmio`; `wreg:92-94` / `rreg:95` indexam `mmio[addr//4]` | BAR0 PRI |
| Tabelas reg | `nvdev.py:NVDev.include:161-163`, `_early_ip_init:101-103` | `include("nv_ref","")`, `("dev_fb","tu102")`, `("dev_gc6_island","ga102")` → `NVReg(base,off,fields)` de `nv_regs` | `nv_ref`, `dev_fb`, `dev_gc6_island` |
| WPR2 + reset | `nvdev.py:_early_ip_init:105-109` | Se `NV_PFB_PRI_MMU_WPR2_ADDR_HI != 0`: desliga `PCI_COMMAND_MASTER`, `pci_dev.reset()`, `sleep(0.1)` | `dev_fb.py:NV_PFB_PRI_MMU_WPR2_ADDR_HI:0x1FA828`; `pci.py:PCI_COMMAND/PCI_COMMAND_MASTER` |
| Bus master | `nvdev.py:_early_ip_init:111` | `write_config_flush(PCI_COMMAND, read\|MASTER)` | config-space |
| BOOT0 / chip detect | `nvdev.py:_early_ip_init:112-116` | `chip_id=NV_PMC_BOOT_0.read()`; `chip_details=NV_PMC_BOOT_42.read_bitfields()`; `chip_name={0x17:GA1,0x19:AD1,0x1b:GB2}+impl`; `fw_name={GB2:gb202,AD1:ad102,GA1:ga102}`; `mmu_ver,fmc_boot=(3,True)` se `arch>=0x1a` senão `(2,False)` | `nv_ref.py:NV_PMC_BOOT_0:0x00`, `NV_PMC_BOOT_42`; GA106 esperado `arch 0x17→GA1/ga102/mmu_ver 2` |
| Escolha Falcon | `nvdev.py:_early_ip_init:118-119` | `NV_FLCN_COT` se `fmc_boot` (Blackwell) senão `NV_FLCN` (Ampere/Ada); `gsp=NV_GSP` | `ip.py:NV_FLCN:93`, `NV_FLCN_COT:285`, `NV_GSP:346` |
| Espera reset | `nvdev.py:_early_ip_init:121` | `flcn.wait_for_reset()` | `NV_FLCN:94-96` (`PGC6_AON_SECURE_SCRATCH_GROUP_05` + `..._00==0xff`); `NV_FLCN_COT:286-288` (`NV_THERM_I2CS_SCRATCH==0xff`, inclui `dev_therm/gb202`) |
| Tamanho VRAM | `nvdev.py:_early_mmu_init:131` | `vram_size=NV_PGC6_AON_SECURE_SCRATCH_GROUP_42.read()<<20` | `dev_gc6_island.py:0x1183a4` (= `0x1183a4` do Nouveau `ga102_fb`) |
| `large_bar` | `nvdev.py:_early_mmu_init:133-134` | `vram=map_bar(1)`; `large_bar=vram.nbytes>=vram_size` decide VRAM vs sysmem em `_alloc_boot_mem` | BAR1 (`ops_nv.py:PCIIface:561` `vram_bar=1`) |
| PMC/FB/MMU includes | `nvdev.py:_early_mmu_init:124-129` | `include("dev_vm","tu102")`, `("dev_mmu","gh100" se v3 senão "tu102")`; `pte_t/pde_t/dual_pde_t=NV_MMU_VER{2,3}_*` | `dev_vm`, `dev_mmu` |
| Falcon `init_sw` | `ip.py:NV_FLCN.init_sw:98-108` | inclui `dev_gsp/ga102`, `dev_falcon_v4/ga102`, `dev_riscv_pri/ga102`, `dev_fbif_v4/ga102`, `dev_falcon_second_pri/ga102`, `dev_sec_pri/ga102`, `dev_bus/tu102`; `prep_ucode()` + `prep_booter()` | VBIOS + `booter_load-570.144.bin` |
| Falcon `init_hw` | `ip.py:NV_FLCN.init_hw:186-210` | `reset(falcon)` → `execute_hs(FRTS)` → `assert WPR2_HI!=0` (`:194`) → `reset(riscv)` → mailbox0/1=`libos_args` → `reset(sec2)` → `execute_hs(booter, mailbox=wpr_meta)` → `assert mbx==0` → `FALCON_OS=0` → `assert RISCV active==1` | `NV_PGSP_FALCON_MAILBOX0/1`, `NV_PFALCON_*/NV_PRISCV_*`, `NV_PGSP_FALCON_ENGINE/NV_PSEC_FALCON_ENGINE` |
| COT/FSP (não-GA106) | `ip.py:NV_FLCN_COT.init_sw:290-300`, `init_fmc_image:302-309`, `init_hw:311-326`, `kfsp_send_msg:328-344` | `fmc_boot_args` + `fmc-570.144.bin` (ELF `image/hash/signature/publickey`) → `NVDM_PAYLOAD_COT` via `NV_PFSP_*` | `dev_fsp_pri/gh100`, `dev_therm/gb202`; só `fmc_boot=True` (GB2) |
| Boot stages completos | `nvdev.py:75-86` + `ip.py:98-108,186-210,347-362,510-520` + `ops_nv.py:590-641` | IP-sw (aloca+prefill RPC) → IP-hw (Falcon→GSP-RM→golden image) → `NVDevice` (vaspace/channel/GPFIFO/cmdq) | ver §5 GSP |

O que NVDev **não** faz (vs `initialization-map.md` Etapas 2–3/6): sem `nouveau_drm_device_init` (CLI/TTM/BIOS/display/KMS), sem `devinit_post/top_parse/fb_mem_unlock` explícito, sem LTC/GR nativo/reclock — tudo delegado ao GSP-RM após `GSP_INIT_DONE`.

## 2. Memory — VRAM / page tables / VA / sysmem / BAR1 / mappings

| Item | Arquivo:função:linha @ `e8c8ba1c` | Detalhe |
|---|---|---|
| VA global | `nvdev.py:NVMemoryManager:69-72` | `va_allocator=TLSFAllocator((1<<44), base=0x1000000000)`; `on_range_mapped:72` escreve `NV_VIRTUAL_FUNCTION_PRIV_MMU_INVALIDATE (1<<0\|1<<1\|1<<6\|1<<31)` |
| PTE/PDE | `nvdev.py:NVPageTableEntry:33-67` | `view(paddr,0x1000,fmt='Q')`; `set_entry:38` (`pte: valid/address_sys/aperture/kind=6/pcf|vol`; `pde/dual_pde: is_pte=False/aperture/address/no_ats`); `is_page:58`, `supports_huge_page:59`, `valid:61`, `address:65` |
| Gerente MM | `nvdev.py:_early_mmu_init:143-147` | `bits,shifts=(56,[12,21,29,38,47,56])` se v3 senão `(48,[12,21,29,38,47])`; `NVMemoryManager(vram_size-64MB, boot 2MB, pt_t, va_bits/shifts, va_base=0, palloc_ranges=[512MB,2MB,4KB], reserve_ptable=not large_bar)` |
| Boot alloc | `nvdev.py:_alloc_boot_mem:149-159` | `sysmem=True ou (None e not large_bar)` → `pci_dev.alloc_sysmem` (mlock+`pagemap` em `system.py:198-203`); senão `mm.palloc` VRAM + `vram.view` + `sysaddr=bar1_base+paddr+...`; `view[:size]=data` se dado |
| `alloc`/`free`/`map` | `system.py:PCIIfaceBase.alloc:267-281`, `free:283-286`, `map:291-307` | sysmem se `host ou (cpu_access se small-bar senão uncached&&cpu_access)`; senão `mm.valloc` + `map_bar(vram_bar, off=paddr)` se `cpu_access`; `p2p_paddrs:288-289` = `bar1_base+paddr` como `SYS` |
| BAR1 janela | `ip.py:NV_GSP.init_hw:516-517` | `NV_PBUS_BAR1_BLOCK.write(mode=0,target=0,ptr=0)` (+ `NV_VIRTUAL_FUNCTION_PRIV_FUNC_BAR1_BLOCK_LOW_ADDR` se `fmc_boot`) |
| Page dir | `ip.py:NV_GSP.rpc_set_page_directory:594-599` | `NV0080_CTRL_DMA_SET_PAGE_DIRECTORY (physAddress=pdir, numEntries=pte_cnt[0], flags=0x8, chId=0, pasid=0xffffffff)` via `SET_PAGE_DIRECTORY` |
| UVM (só NVK) | `ops_nv.py:NVKIface.alloc:478-516`, `map:545-549` | NVOS02/NVOS32 + `UVM_CREATE_EXTERNAL_RANGE/MAP_EXTERNAL_ALLOCATION`; **não** usado pelo caminho NVDev |

## 3. Submission — channels / GPFIFO / doorbells / pushbuffers / fences / compute+DMA

| Item | Arquivo:função:linha @ `e8c8ba1c` | Detalhe |
|---|---|---|
| Canal/grupo | `ops_nv.py:NVDevice.__init__:610-621` | `channel_group=rm_alloc(KEPLER_CHANNEL_GROUP_A, engine GRAPHICS)`; `ctxshare=rm_alloc(FERMI_CONTEXT_SHARE_A, hVASpace, ASYNC)`; `gpfifo_area=alloc(0x300000, contiguous, cpu_access, force_devmem, WRITECOMBINED)`; `GPFIFO_SCHEDULE(bEnable=1)` |
| GPFIFO alloc | `ops_nv.py:NVDevice._new_gpu_fifo:642-666` | notifier 48 MB `uncached`; `NV_CHANNELGPFIFO_ALLOCATION_PARAMETERS(gpFifoOffset/Entries, hContextShare, hObjectError, hUserdMemory/userdOffset, engineType)`; `rm_alloc(gpfifo_class)`; compute aloca `compute_class`+`GT200_DEBUGGER` (`:651-653`), DMA aloca `dma_class` (`:654`); `BIND`+`GPFIFO_SCHEDULE` se `channel_group==nvdevice` (`:657-659`); token=`GET_WORK_SUBMIT_TOKEN` (`:661-662`); `GPFifo(ring=view(offset,entries*8,'Q'), gpput=view(offset+entries*8+GPPut,'I'), token)` |
| Pushbuffer | `ops_nv.py:NVCommandQueue.nvm:86`, `bind:105-112`, `_submit_to_gpfifo:114-126` | `nvm` empacota `(typ<<28\|len<<16\|subch<<13\|mthd>>2)`; `bind` copia `_q` p/ `hw_page`; `_submit` escreve `ring[put%entries]=(cmdq_addr//4<<2\|len<<42\|1<<41)`, `gpput[0]=(put+1)%entries`, `memory_barrier`, **doorbell** `gpu_mmio[0x90//4]=token`, `put+=1` |
| Doorbell token | `ops_nv.py:_submit_to_gpfifo:125`, `GPFifo:363-368` | `token` vem do RM; `ip.py:NV_GSP.rpc_rm_control:589-591` OR-runlist (`<<16`) e bit `1<<30` se `GB2`; usermode `gpu_mmio` em `PCIIface.setup_usermode:570` (`map_bar(0, off=0xbb0000, size=0x10000)`) vs `NVKIface:445-446` (`_gpu_map_to_cpu(usermode,0x10000)`) |
| Fences/signals | `ops_nv.py:NVComputeQueue.signal:159-178`, `wait:97-101`, `NVCopyQueue.signal:207-210`, `hcq.py:HCQSignal` | `wait`=`NVC56F_SEM ACQ_CIRC_GEQ`; `signal`=release via QMD (`release{i}_enable/address/payload`) ou `NVC56F_SEM RELEASE+WFI+TIMESTAMP` + `NON_STALL_INTERRUPT`; copy=`NVC6B5_SET_SEMAPHORE+LAUNCH_DMA(flush+release_four_word)`; `NVDevice._setup_gpfifos:681-694` + `synchronize()` amarram `timeline_signal/timeline_value` |
| Compute queue | `ops_nv.py:NVComputeQueue:128-192` | `setup:88-95` (`SETOBJECT` compute, janelas local/shared); `exec:135-157` (QMD em `args_state.buf`, `bind_sints` grid/local, `SEND_PCAS_A(qmd>>8)`, `SEND_SIGNALING_PCAS2_B(9)` ou QMD dependente); `memory_barrier:129-133` (`INVALIDATE_SHADER_CACHES`); `write:180`, `poll_bit:186` |
| DMA queue | `ops_nv.py:NVCopyQueue:194-212` | `copy:199-205` (`NVC6B5_OFFSET_IN_UPPER` src/dst 64b, `LINE_LENGTH_IN`, `LAUNCH_DMA(non_pipelined,pitch,pitch)` em fatias `1<<31`); `_submit:212` → `dma_gpfifo` |
| QMD | `ops_nv.py:QMD:44-76`, `NVProgram:292-309` | `ver5/sz0x60` se `compute_class>=BLACKWELL_COMPUTE_A` senão `ver3/sz0x40` (`:48`); `program_address`, `register_count`, `shared_memory_size`, `sass_version`, `constant_buffer_*`, `max_threads:316` (`65536//round_up(regs*32,256)//4*4*32`) |
| Video (extra) | `ops_nv.py:NVVideoQueue:214-239`, `NVDevice._ensure_has_vid_hw:710-728` | `decode_hevc_chunk` (`NVC9B0_*`), `vid_gpfifo` (offset `0x200000`, 2048 entries, `engineType 19`); `viddec_class` só AD/GB (ver §5) |

## 4. GSP — fw loading / cmd queues / RPC / RM alloc / class discovery / Ampere classes

| Item | Arquivo:função:linha @ `e8c8ba1c` | Detalhe |
|---|---|---|
| FW `booter_load` (Falcon) | `ip.py:NV_FLCN.prep_booter:171-184` | `fetch_fw(nvidia/{fw}/gsp, booter_load-570.144.bin, sha)`; `sha={ga102:4497e3ef..., ad102:8b293e19...}` (`:172-173`); patch `sig_prod` em `data` (`:178-181`); `_alloc_boot_mem(sysmem=False)`; `os_data/code/off/sz` de `hs_load_header_v2` |
| FW ucode (VBIOS) | `ip.py:NV_FLCN.prep_ucode:110-169` | Lê VBIOS de `mmio[0x300000:0x400000]`; PCI ROM dir → BIT header (`0x1b0`, `Signature 0x424954`) → `FALCON_DATA v2` → `FWSEC_PROD` → `DESC_V3` → `FRTS` (`frts_offset=vram-2MB`); `FWSECLIC_FRTS_CMD` patch `init_cmd 0x15` + `PKC sig[-0x180:]` |
| FW `gsp` (RM) | `ip.py:NV_GSP.init_gsp_image:401-423` | `fetch_fw(nvidia/ga102/gsp, gsp-570.144.bin, a8c3ebee...)` (`:402` — **fixo `ga102` mesmo em AD/GB**); ELF `.fwimage` + `.fwsignature_{chip[:4].lower()}x`; radix3 (`npages/offsets`) + `_alloc_boot_mem`; `signature→gsp_signature_bar1` |
| FW `bootloader` (RM) | `ip.py:NV_GSP.init_boot_binary_image:425-432` | `fetch_fw(nvidia/{fw}/gsp, bootloader-570.144.bin, sha)`; `sha={ga102:82428f53..., ad102:65ab2e6b..., gb202:d40b48e4...}` (`:426-428`); `RM_RISCV_UCODE_DESC(monitorCode/Data/Manifest)`; `_alloc_boot_mem` → `booter_bar1` |
| FW `fmc` (só GB2) | `ip.py:NV_FLCN_COT.init_fmc_image:302-309` | `fetch_fw(nvidia/{fw}/gsp, fmc-570.144.bin, cb59a35c...)` (`:303-304`); ELF `image/hash/signature/publickey`; `_alloc_boot_mem` → `fmc_booter_bar1` |
| Fonte FW | `helpers.py:fetch_fw:504-509` | `/lib/firmware/{path}/{name}.zst` (zstd, py≥3.14) se SHA bate, senão `fetch(https://gitlab.com/kernel-firmware/linux-firmware/-/raw/0a6871b1.../{path}/{name})` |
| WPR meta | `ip.py:NV_GSP.init_wpr_meta:434-455` | `GspFwWprMeta(sizeOfBootloader/Radix3/Signature, sysmemAddrs, bootloader offsets, REV/MAGIC)`; `fmc_boot`→ layout heap (`vga 0x20000, pmu 0x1820000, nonWpr 0x220000, gspHeap 0x8700000, frts 0x100000`); senão layout VRAM (`vga 1MB no topo, frts, bootBin, gspFw, heap 0x8100000, nonWpr 1MB`); `assert frts_offset==m.frtsOffset` (`:453`); `_alloc_boot_mem` → `wpr_meta_sysmem` |
| Filas RPC | `ip.py:NV_GSP.init_rm_args:364-387`, `NVRpcQueue:19-91` | `queues _alloc_boot_mem(pt+2*queue, sysmem=True)` (`queue 0x40000`); PTEs=sysmem addrs; `GSP_ARGUMENTS_CACHED(BDmemStack, queue_args)` → `rm_args_sysmem`; `msgqTxHeader(ver0,size,entryOff0x1000,msgSize0x1000,msgCount,writePtr0,flags1)`; `NVRpcQueue` (`tx_view`, `seq`, `queue_mv`); `send_rpc:57` fragmenta `msgSize*16-hdrs`; `_send_rpc_record:38` (`SIGNATURE_VALID/PENDING/ver3<<24`) + `GSP_MSG_QUEUE_ELEMENT(checkSum elemCount seqNum)` + `NV_PGSP_QUEUE_HEAD[0]=0` (`:55`) |
| `libos` args | `ip.py:init_libos_args:389-399` | logbufs 2 MB + `libos 0x1000`; `LibosMemoryRegionInitArgument(CONTIGUOUS,SYSMEM,0x10000,LOG{INIT,INTR,RM,MNOC,KRNL})` + `RMARGS(rm_args_sysmem)`; `libos_args_sysmem` vai p/ mailbox Falcon (`init_hw:198-200`) e `run_cpu_seq:653-654` |
| System info | `ip.py:rpc_set_gsp_system_info:601-609` | `GspSystemInfo(gpuPhysAddr=bar0, gpuPhysFbAddr=bar1, gpuPhysInstAddr=bar3, pciMirror=[0x88000,0x92000][fmc_boot], BDF=bdf_as_int(devfmt), bIsPassthru=1, DeviceID=config(VENDOR_ID,4), SubDeviceID, RevisionID, maxUserVa=0x7ffffffff000)`; `bdf_as_int:602` retorna `0x000` se `usb/remote` |
| Registry | `ip.py:rpc_set_registry_table:616-627` | `PACKED_REGISTRY_TABLE{RMForcePcieConfigSave=1, RMSecBusResetEnable=1}` via `SET_REGISTRY` (prefill em `init_sw:355`) |
| `init_hw`/golden | `ip.py:init_hw:510-520`, `init_golden_image:468-508` | `stat_q=NVRpcQueue(stat, cmd)`; `wait GSP_INIT_DONE (:514)`; `BAR1_BLOCK` (`:516-517`); `priv_root=0xc1e00004`; golden: `rm_alloc(ROOT/DEVICE/VASPACE)` → `GET_DEVICE_INFO_TABLE→runlists` → `COPY_SERVER_RESERVED_PDES` (512 MB) → `rm_alloc(GPFIFO 32 entries, cid3, VASPACE, userd)` → `GET_CONTEXT_BUFFERS_INFO` → `promote_ctx(grctx 0,1,2,3-6,9,10,11)` → `rm_alloc(compute)+rm_alloc(dma)` |
| `promote_ctx` | `ip.py:promote_ctx:457-466` | `NV2080_CTRL_GPU_PROMOTE_CTX(entryCount, engineType, hChanClient, hObject)`; `valloc(contiguous)` por buf se não dado; `gpuVirtAddr/gpuPhysAddr/size/physAttr/bNonmapped`; `PROMOTE_CTX` via `rm_control` |
| RM alloc/control | `ip.py:rpc_rm_alloc:538-569`, `rpc_rm_control:571-592`, `rpc_alloc_memory:526-536` | `GSP_RM_ALLOC(hClient/hParent/hObject=next(handle_gen 0xcf000000)/hClass/paramsSize)` + `GSP_RM_CONTROL(...)`; `ALLOC_MEMORY` exige páginas 4 KB (`:527`); `rm_control` injeta `PERMISSIONS_INIT` p/ `POWER_REQUEST_FEATURES` (`:572-575`), PMA bufs p/ `ALLOC_PMA_STREAM` (`:576-580`), e OR `workSubmitToken` (`:589-591`); `SET_PAGE_DIRECTORY:594-599`, `UNLOADING_GUEST_DRIVER:611-614` |
| CPU seq (GSP→CPU) | `ip.py:run_cpu_seq:629-660`, `read_resp:62-85` | `read_resp` trata `GSP_RUN_CPU_SEQUENCER` (`:70`) + `OS_ERROR_LOG` (`:71-72`, seta `is_err_state` `:74`); `run_cpu_seq` ops `0x0` wreg, `0x1` modify, `0x2` poll, `0x3` delay, `0x4` save, `0x5` reset+`disable_ctx_req`, `0x6` start_cpu, `0x7` wait_halted, `0x8` resume RISC-V + mailbox + `wait BOOT_STAGE_3_HANDOFF` (`:657`) + `assert SEC2 mailbox==0` |
| Class discovery | `ip.py:init_sw:357-362` vs `ops_nv.py:NVKIface.setup_usermode:434-443` | **NVDev/GSP: hardcoded, sem query.** default `gpfifo=AMPERE_CHANNEL_GPFIFO_A(0xc56f)`, `compute=AMPERE_COMPUTE_B(0xc7c0)`, `dma=AMPERE_DMA_COPY_B(0xc7b5)` (`nv_570.py:24729,24838,24825`); `AD→ADA_COMPUTE_A`, `GB→BLACKWELL_*`; `viddec={AD:NVC9B0,GB:NVCFB0}` (`:358`). **NVK: query real** `GET_CLASSLIST` + `next(c in nvclasses)` (`ops_nv.py:435-443`: `HOPPER/TURING_USERMODE`, `BLACKWELL/AMPERE_GPFIFO`, `BLACKWELL/ADA/AMPERE_COMPUTE`, `BLACKWELL/AMPERE_DMA`) |
| Ampere classes | `ip.py:357`, `ops_nv.py:440-442` | GA106 usa o default Ampere (sem branch `AD`/`GB`): `AMPERE_CHANNEL_GPFIFO_A / AMPERE_COMPUTE_B / AMPERE_DMA_COPY_B` |

## 5. Dependências (imports que provam ausência de `nvidia.ko`)

- `nvdev.py:2-8`: `nv_regs`, `helpers(getenv,DEBUG,getbits,round_up)`, `autogen.pci`, `support.memory(TLSFAllocator,MemoryManager,AddrSpace)`, `support.nv.ip(NV_FLCN,NV_FLCN_COT,NV_GSP)`, `support.system(PCIDevice)`, `support.hcq(MMIOInterface)`. Zero `nvidiactl/uvm`.
- `ip.py:4-8`: `autogen(nv,nv_570,pci)`, `helpers(lo32,hi32,DEBUG,round_up,round_down,fetch_fw,wait_cond,ceildiv)`, `support.system(System)`, `support.hcq(MMIOInterface)`, `support.elf(elf_loader)`.
- `ops_nv.py:PCIIface:556-581` + `system.py:PCIIfaceBase:253-307` + `PCIDevice:160-226`: sysfs+mmap. `NVKIface:370-554`: `/dev/nvidia*` + NVOS/UVM ioctls (para contraste).
- `helpers.py:fetch_fw:504-509`: pin `linux-firmware@0a6871b1`.

## 6. Lacunas honestas (UNCERTAIN)

- Nenhum `ga106` em `nvdev.py:114-116` / `ip.py:172-173,426-428` — GA106 pega `fw ga102` por `arch 0x17`. `grep 2504|GA106` no clone só acha `IMPLEMENTATION_GA106` em `nv_570.py:23415`/`nv_580.py:24463`/`nv_610.py:25044` (constante de header, não suporte).
- `init_gsp_image:402` busca `nvidia/ga102/gsp/gsp-570.144.bin` fixo; `booter_load`/`bootloader` variam por `fw_name` (ga102/ad102/gb202) — sem SHA `ga106`.
- `sm_86`/`arch` de GA106 só via `NVDevice._query_gpu_info:668-679` (`GR_INFO num_gpcs/tpc/sm/warps/sm_version→arch:631`) em hardware — fora do TG0.
- Display/modeset, `DEVINIT POST`, runpm, faults nativos: dono é o RM sob GSP; NVDev não os reimplementa.

## 7. Referências

- `tinygrad/runtime/support/nv/nvdev.py:1-163` (NVDev, NVMemoryManager, NVPageTableEntry, NVReg).
- `tinygrad/runtime/support/nv/ip.py:1-661` (NV_FLCN, NV_FLCN_COT, NV_GSP, NVRpcQueue).
- `tinygrad/runtime/ops_nv.py:27-848` (NVKIface:370, PCIIface:556, NVDevice:585, GPFifo:363, QMD:44, queues:78-239).
- `tinygrad/runtime/support/system.py:58-90,160-307,311-447` (scan, PCIDevice, PCIIfaceBase, Remote).
- `tinygrad/runtime/autogen/nv_regs/{nv_ref,dev_fb,dev_gc6_island,dev_gsp,dev_bus,dev_vm,dev_mmu}.py`.
- `tinygrad/helpers.py:fetch_fw:504-509`.
- Tudo @ `e8c8ba1c7751142f7b20d85692afd2da5cef5e84`.
