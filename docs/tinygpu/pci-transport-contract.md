# TinyGPU — contrato de transporte PCI (TG0, análise estática)

> Fase TG0. SOMENTE análise estática. Nenhum hardware tocado, nenhum driver instalado, nada executado contra GPU.
> Clones usados SOMENTE em `/tmp/opencode/tg0-tinygrad` (fora deste repo) e cache em `/tmp/opencode/tg0-tinygpu`.
> Este repo recebe SÓ `.md`. `docs/README.md` não foi editado.
> Nenhum binário copiado para o repo.

## 0. Fontes pinadas

| Fonte | SHA | Arquivo principal |
|---|---|---|
| `tinygrad/tinygrad` | `e8c8ba1c7751142f7b20d85692afd2da5cef5e84` | `tinygrad/runtime/support/system.py:1-447` |
| `tinygrad/tinygpu_releases` | `c0d024f9ff0e1dc8fdf217f255da7101d91e8323` | `TinyGPU.zip` (`sha256 0c47285e2232643210555cf30ce08289b9e55da261c300e0c82e8448a359a21f`, `1634625` bytes) |
| Dext/app source (dentro do clone tinygrad) | mesmo `e8c8ba1c` | `extra/usbgpu/tbgpu/installer/Shared/server.c:1-285`, `extra/usbgpu/tbgpu/installer/TinyGPUDriverExtension/*`, `extra/remote/serve.py:1-101` |

Todas as referências `arquivo:linha` abaixo são relativas ao clone `tinygrad@e8c8ba1c` salvo indicação contrária.
`system.py` = `tinygrad/runtime/support/system.py`.

## 1. Classes auditadas (system.py)

| Símbolo | Definição | Papel |
|---|---|---|
| `_System` / `System` | `system.py:15`, instância `system.py:156` | `pci_scan_bus:58`, `list_devices:82-85`, `pci_probe_device:87-90`, `pci_setup_usb_bars:92-140` (só caminho USB Linux), `flock_acquire:142-154` |
| `PCIDevice` | `system.py:160-226` | PCI local Linux: `/sys/bus/pci` (`enable`, `config`, `resourceN`, `remove`, `driver/unbind`, VFIO opt). `alloc_sysmem:198`, `reset:205`, `read_config:206`, `write_config:207`, `bar_fd:213`, `bar_info:216`, `map_bar:219-222` (`mmap` + `MMIOInterface`), `resize_bar:223-226` |
| `USBPCIDevice(PCIDevice)` | `system.py:228-248` | Caminho Linux USB (NÃO é TinyGPU): `CustomASM24Controller` + `USBMMIOInterface`, `gpu_bus=4`, `mem_base=0x10000000`, `pref_mem_base=32<<30` (`system.py:235`). Contraste necessário para não confundir com APLRemote |
| `PCIAllocationMeta` | `system.py:250-251` | `mapping:VirtMapping; has_cpu_mapping:bool; hMemory:int` |
| `PCIIfaceBase` | `system.py:253-307` | Camada que `ops_nv.PCIIface` usa. `peer_group:255`, `is_local:256` (`not isinstance RemotePCIDevice`), `is_bar_small:257` (`bar_info(vram_bar)[1]==256<<20`), `__init__:259-265`, `alloc:267-281`, `free:283-286`, `p2p_paddrs:288-289`, `map:291-307` |
| `RemoteCmd` | `system.py:311-312` | Enum real do protocolo (ver §2). NÃO usar nomes hipotéticos: a lista real é `PROBE,MAP_BAR,MAP_SYSMEM_FD,CFG_READ,CFG_WRITE,RESET,MMIO_READ,MMIO_WRITE,MAP_SYSMEM,SYSMEM_READ,SYSMEM_WRITE,RESIZE_BAR,PING = range(13)` |
| `RemoteMMIOInterface(MMIOInterface)` | `system.py:314-332` | View remota: `__getitem__:319-324` → `_bulk_read`, `__setitem__:326-329` → `_bulk_write`, `view:331-332`. `rd_cmd/wr_cmd` default `MMIO_READ/MMIO_WRITE`; sysmem usa `SYSMEM_READ/SYSMEM_WRITE` (`system.py:404`) |
| `RemotePCIDevice(PCIDevice)` | `system.py:334-414` | PCI remoto genérico (TCP). `remote_sock:342`, `remote_list:360` (PROBE), `_recvall:369`, `_rpc:376`, `__init__:387`, `_bulk_read:394`, `_bulk_write:397-399`, `alloc_sysmem:401-404` (MAP_SYSMEM), `reset:406`, `read/write_config:407-408`, `bar_info:411`, `map_bar:412-413`, `resize_bar:414` |
| `APLRemotePCIDevice(RemotePCIDevice)` | `system.py:416-447` | Caminho macOS TinyGPU (unix socket). `APP_PATH:417`, `ensure_app:420-427`, `__init__:429-439`, `alloc_sysmem:441-447` (MAP_SYSMEM_FD, override) |

Seleção macOS: `System.list_devices:84-85` retorna `APLRemotePCIDevice` se `OSX`, senão `PCIDevice`. `REMOTE` env alterna para `RemotePCIDevice` TCP (`system.py:84`).
`PCIIface` NV: `tinygrad/runtime/ops_nv.py:556-562` (`vendor=0x10de`, `devices=((0xff00,(0x2200,...,0x2500,...)),)`, `base_class=0x03`, `vram_bar=1`).

## 2. `RemoteCmd` real — IDs, chamador Python, fio, direção, fds

`system.py:312`: `PROBE,MAP_BAR,MAP_SYSMEM_FD,CFG_READ,CFG_WRITE,RESET,MMIO_READ,MMIO_WRITE,MAP_SYSMEM,SYSMEM_READ,SYSMEM_WRITE,RESIZE_BAR,PING = range(13)`.
`server.c:19-33` confirma `0-11` com os mesmos nomes (`CMD_PROBE=0 ... CMD_RESIZE_BAR=11`); `PING=12` só existe no Python + `extra/remote/serve.py:15-16` + `extra/remote/bench.py:21-25`.
Fio (`system.py:376-385`, `server.c:35-36`): request `struct '<BIIQQQ'` = `cmd:u8, dev_id:u32, bar:u32, arg0:u64, arg1:u64, arg2:u64` (33 bytes) + `payload` opt; resposta `struct '<BQQ'` = `status:u8, resp0:u64, resp1:u64` (17 bytes) + `readout` opt + `fd` opt via `SCM_RIGHTS`.
Erro: `status!=0` → `RuntimeError(recvall(resp1).decode())` (`system.py:382-383`); `server.c:84-88` / `serve.py:6-7` (`resp_err`).

| ID | Nome real | Chamador Python (lado cliente) | Args fio (`arg0,arg1,arg2,bar,payload`) | Resposta fio | Direção | FDs | Implementação de referência |
|---|---|---|---|---|---|---|---|
| 0 | `PROBE` | `RemotePCIDevice.remote_list:360-366` (só TCP `REMOTE=*`; APL NÃO usa) | `arg0=base_class or 0, arg1=len(payload), arg2=vendor, payload=array('I',(mask,dev)...)` | `resp0=data_len, resp1=num_devs` + `recvall(data_len).decode().split('\n')` (`"pcibus:idx"`) | C→S header+payload; S→C header+texto | não | `serve.py:18-29`; `server.c` NÃO implementa (não precisa p/ APL) |
| 1 | `MAP_BAR` | `RemotePCIDevice.bar_info:411` (`functools.cache`) | `bar=bar_idx`, sem args | `resp0=addr/size0, resp1=size` (`serve.py:40`: `resp(*pci_dev.bar_info(bar))`; `server.c:121-127`: `resp0=mapped VA no servidor, resp1=size`) | C→S header; S→C header | não | `serve.py:38-40`; `server.c:121-127` (`IOConnectMapMemory64` + cache `g_bars[6]`) |
| 2 | `MAP_SYSMEM_FD` | `APLRemotePCIDevice.alloc_sysmem:442` (override APL; TCP NÃO usa) | `arg0=size, arg1=int(contiguous)` | `resp0=mapped_size(alloc_sz page-align, mín 0x4000), resp1=idx` + `fd shm` via `recvmsg SCM_RIGHTS` (`system.py:442`) | C→S header; S→C header+fd | SIM (`SCM_RIGHTS`, `shm_open /tinygpu_%d`) | `server.c:129-165` (`shm_open+mmap+IOConnectCallStructMethod sel=3 PrepareDMA`, `memcpy paddrs→shm`); `serve.py` NÃO implementa (é APL-only) |
| 3 | `CFG_READ` | `RemotePCIDevice.read_config:407` | `arg0=offset, arg1=size` | `resp0=value` | C→S header; S→C header | não | `serve.py:41-42`; `server.c:216-220` (`dext_rpc sel=0 ReadCfg`) → `TinyGPUDriver::CfgRead:159-177` → `IOPCIDevice::ConfigurationRead8/16/32` |
| 4 | `CFG_WRITE` | `RemotePCIDevice.write_config:408` | `arg0=offset, arg1=size, arg2=value` | header `resp()` vazio | C→S header; S→C header | não | `serve.py:43-45` (`write_config(arg0,arg2,arg1)`); `server.c:222-226` (`dext_rpc sel=1`) → `CfgWrite:179-186` → `ConfigurationWrite8/16/32` |
| 5 | `RESET` | `RemotePCIDevice.reset:406` | sem args (`dev_id` só) | header vazio | C→S header; S→C header | não | `serve.py:49-51`; `server.c:231-233` (`dext_rpc sel=2`) → `ResetDevice:188-193` (`Reset(FunctionReset→HotReset)`) |
| 6 | `MMIO_READ` | `RemoteMMIOInterface.__getitem__:322` via `_bulk_read:394-396` | `bar=idx, arg0=offset, arg1=size` | `resp0=size` + `readout(size)` (`_rpc readout_size=size`) | C→S header; S→C header+dados | não | `serve.py:52-55` (fast `I` + `sendmsg`); `server.c:235-241` (`validate_bar` + `mmio_copy` + `send`) |
| 7 | `MMIO_WRITE` | `RemoteMMIOInterface.__setitem__:329` via `_bulk_write:397-399` | `bar=idx, arg0=offset, arg1=len(data)` + `data` anexada ao `sendall` | NENHUMA resposta (one-way). Servidor só `recvall(arg1)` e escreve | C→S header+dados; S→C nada | não | `serve.py:56-60` (sem `sendall`); `server.c:243-247` (sem resposta). Erro vira `ConnectionError` (`serve.py:82`) pois não há canal de erro |
| 8 | `MAP_SYSMEM` | `RemotePCIDevice.alloc_sysmem:402` (só TCP; APL usa `MAP_SYSMEM_FD`) | `arg0=size, arg1=int(contiguous)` | `resp0=paddrs_len, resp1=handle` + `recvall(paddrs_len)` → `struct '<NQ'` paddrs | C→S header; S→C header+dados | não | `serve.py:61-65` (`pci_dev.alloc_sysmem` + `struct.pack`); `server.c` NÃO implementa |
| 9 | `SYSMEM_READ` | `RemoteMMIOInterface` com `rd_cmd=SYSMEM_READ` (`system.py:404`) | `bar=handle, arg0=offset, arg1=size` | `resp0=size` + readout | C→S header; S→C header+dados | não | `serve.py:66-67`; `server.c` NÃO implementa (APL lê sysmem via `mmap` local do fd, sem RPC) |
| 10 | `SYSMEM_WRITE` | idem `wr_cmd=SYSMEM_WRITE` | `bar=handle, arg0=offset, arg1=len` + data | nenhuma (one-way, como MMIO_WRITE) | C→S header+dados | não | `serve.py:68-69`; `server.c` NÃO implementa |
| 11 | `RESIZE_BAR` | `RemotePCIDevice.resize_bar:414` (chamado via `PCIIfaceBase.__init__:263` com `suppress(Exception)`) | `bar=bar_idx` | header vazio | C→S header; S→C header | não | `serve.py:46-48`; `server.c:228-229` **noop** (`break` sem erro). No dext não há resize (BAR já exposto pelo `IOPCIDevice`) |
| 12 | `PING` | `extra/remote/bench.py:21-25` (latência; não usado pelo bring-up) | sem args | header vazio | C→S header; S→C header | não | `serve.py:15-16`; `server.c` NÃO implementa (cai em `default: resp.status=1`) |

Notas de fio que o `.app` precisa respeitar exatamente:
- Header é sempre 33 bytes `'<BIIQQQ'`; resposta sempre 17 bytes `'<BQQ'` (`system.py:377`, `server.c:35-36`). `dev_id` APL é sempre `0` (`APLRemotePCIDevice.__init__:439` fixa `pcibus="usb4"` → `dev_id=0` em `RemotePCIDevice.__init__:388`).
- `has_fd=True` só em `MAP_SYSMEM_FD`: cliente faz `recvmsg(17, CMSG_LEN(4))` (`system.py:379`); servidor faz `sendmsg` com `SCM_RIGHTS` (`server.c:69-82,211-214`). Sem isso o `mmap(fd)` do Python falha.
- Leituras bulk usam `readout_size` (`system.py:385`); escritas bulk NÃO esperam resposta — o servidor NÃO deve enviar header após `MMIO_WRITE` (`server.c:243-247` `continue` sem `send_response`), senão o stream dessincroniza.
- `bar_info` remoto: o cliente APL ignora `resp0` (VA do servidor) e usa só `resp1=size` (`system.py:412-413`, `is_bar_small:257`). O servidor pode retornar VA local sem significado remoto (`server.c:124-125`).

## 3. Tabela operação × chamada × implementação × necessária p/ GA106

`PCIIface` NV (`ops_nv.py:556-562`): `vram_bar=1`, `base_class=0x03`. `PCIIfaceBase.__init__:261-265` faz `pci_probe_device` → `resize_bar(vram_bar)` (suprimido) → `dev_impl_t=NVDev`.
Leitura: **necessária-p/GA106 = o bring-up `PCIIface→NVDev` chama isso no macOS/TinyGPU?** (`YES` = no caminho `alloc/map/cfg/BAR/sysmem/reset`; `NO` = só TCP/bench).

| Operação (API que o NVDev chama) | Chamada Python | `RemoteCmd` | Implementação que o `.app` precisa ter | Necessária p/ GA106 via TinyGPU? |
|---|---|---|---|---|
| Descobrir VRAM BAR size / `is_bar_small` | `pci_dev.bar_info(vram_bar)` (`system.py:257,411`) | `MAP_BAR (1)` | `server.c:map_bar` → `IOConnectMapMemory64(bar)` + cache `g_bars`; retorna `(addr_local,size)`; cliente usa `size` | YES (decide `alloc` sysmem vs devmem, `p2p_paddrs`, `map`) |
| Mapear BAR p/ CPU (`cpu_access`) | `pci_dev.map_bar(bar,off,size,fmt)` (`system.py:412-413`) → `RemoteMMIOInterface.view` | `MAP_BAR (1)` + depois `MMIO_READ/WRITE (6/7)` | `MAP_BAR` uma vez por BAR; R/W via memória já mapeada no servidor (`g_bars[bar].addr+off`, `mmio_copy` 32-bit volatile `server.c:63-67`, `validate_bar` `server.c:167-169`, `BULK_BUF_SIZE=64<<20`) | YES (`PCIIfaceBase.alloc:280` com `cpu_access`, `setup_usermode: ops_nv` mapeia BAR0) |
| Ler/escrever BAR (registradores, doorbells, GSP) | `RemoteMMIOInterface[i]/[o:s]` (`system.py:319-329`) | `MMIO_READ (6)` / `MMIO_WRITE (7)` | Leitura: header+`send`; escrita: `recvall`+`mmio_copy`, sem resposta | YES (todo MMIO do `NVDev` passa aqui) |
| Ler config space (VID/DID, command, caps, REBAR) | `pci_dev.read_config(off,size)` (`system.py:407`) | `CFG_READ (3)` | `dext_rpc sel=0` → `CfgRead` → `ConfigurationRead8/16/32` (`TinyGPUDriver.cpp:159-177`) | YES (enumeração já feita no IOKit, mas `NVDev` lê config no bring-up) |
| Escrever config space (command MSE/BME, BARs) | `pci_dev.write_config(off,val,size)` (`system.py:408`) | `CFG_WRITE (4)` | `sel=1` → `CfgWrite` → `ConfigurationWrite8/16/32` (`TinyGPUDriver.cpp:179-186`). Nota: dext já seta `IOSpace|BusMaster|MemorySpace` no `Start_Impl:57-60` | YES |
| Resize BAR1 | `pci_dev.resize_bar(1)` (`system.py:414`, via `PCIIfaceBase.__init__:263`) | `RESIZE_BAR (11)` | noop com sucesso (`server.c:228-229`); `serve.py:46-48` chama `resize_bar` local no TCP | YES (chamado, mas tolera noop; falha é suprimida no `__init__`) |
| Alocar sysmem DMA + obter paddrs | `pci_dev.alloc_sysmem(size,vaddr,contiguous)` (`system.py:441-447` override APL) | `MAP_SYSMEM_FD (2)` + fd | `server.c:map_sysmem_fd:129-165`: `shm_open /tinygpu_%d`, `ftruncate+mmap`, `IOConnectCallStructMethod(sel=3 PrepareDMA, in=shm, out=paddr_buf[8192])`, `memcpy`, `sendmsg+fd`; `resp0=alloc_sz,resp1=idx`. Python `mmap(fd)` + parse `(paddr,len)` até `(0,0)` + expande p/ 4K (`system.py:446-447`) | YES (`PCIIfaceBase.alloc:273-277` sysmem path, `map:295-296` CPU buffers) |
| Reset função | `pci_dev.reset()` (`system.py:406`) | `RESET (5)` | `sel=2` → `ResetDevice` (`TinyGPUDriver.cpp:188-193`: `FunctionReset→HotReset`) | Opcional no bring-up (exposto, raramente chamado; não é pré-requisito do `NVDev.__init__`) |
| Listar dispositivos remotos | `RemotePCIDevice.remote_list` (`system.py:360-366`) | `PROBE (0)` | N/A p/ APL (APL fixa `"usb4"`, sem probe). Só `serve.py:18-29` TCP | NO (APL não faz probe; `System.list_devices:85` usa `pci_scan_bus` IOKit local) |
| Alocar sysmem TCP + R/W sysmem remota | `RemotePCIDevice.alloc_sysmem` + views (`system.py:401-404`) | `MAP_SYSMEM (8)`, `SYSMEM_READ (9)`, `SYSMEM_WRITE (10)` | Só `serve.py:61-69` TCP. APL usa `mmap` local do fd, nunca esses RPCs | NO (APL substitui por `MAP_SYSMEM_FD+mmap`) |
| Liveness/bench | `bench.py` | `PING (12)` | Só `serve.py:15-16` | NO |

## 4. O que o `.app` precisa implementar (checklist mínimo p/ GA106)

1. `MAP_BAR`: `CopyClientMemoryForType(type=bar)` → `TinyGPUDriver::MapBar` (`TinyGPUDriver.cpp:95-103`: `GetBARInfo+_CopyDeviceMemoryWithIndex`) → `IOConnectMapMemory64` no servidor, cache, retorna size. Sem isso `bar_info/map_bar` falham e `PCIIfaceBase` não aloca.
2. `CFG_READ/WRITE`: selectors `0/1` (`TinyGPUDriverUserClient.iig:6-12`, `TinyGPUDriverUserClient.cpp:105-128`) → `CfgRead/CfgWrite`. Sem isso config space inacessível.
3. `MMIO_READ/WRITE`: acesso à memória mapeada do item 1 com cópia volatile 32-bit (`server.c:63-67`) + validação `off+sz<=size, sz<=64MB`. Escrita sem resposta.
4. `MAP_SYSMEM_FD`: `PrepareDMA` selector `3` (`TinyGPUDriverUserClient.cpp:132-162`: exige buffers ≥4097 bytes, `SetupDMA: maxAddressBits=40`, `PrepareForDMA`, escreve `[addr,len,...,0,0]` no output) + transporte `shm` + `SCM_RIGHTS`. Sem isso `alloc(host/cpu_access)` falha.
5. `RESET` (sel `2`) e `RESIZE_BAR` noop com sucesso. `PROBE/MAP_SYSMEM/SYSMEM_RW/PING` NÃO são exigidos do `.app` no caminho APL.

Divergência conhecida (contrato vs binário-fonte): `server.c` no clone NÃO implementa `PROBE/MAP_SYSMEM/SYSMEM_READ/SYSMEM_WRITE/PING` (caem em `default: status=1`); `serve.py` implementa todos os 13 para TCP. Isso é consistente porque o caminho APL nunca emite esses IDs (ver tabela). Detalhes do servidor APL em `docs/tinygpu/apl-remote-pci.md`; fio byte-a-byte em `docs/tinygpu/remote-protocol.md`.
