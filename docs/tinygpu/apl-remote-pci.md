# TinyGPU — APL remote PCI: ensure_app, socket, RPC e caminho até a GPU (TG0, análise estática)

> Fase TG0. SOMENTE análise estática. Nenhum hardware tocado, nenhum driver instalado, nada executado contra GPU.
> Clones SOMENTE em `/tmp/opencode/tg0-tinygrad` + cache `/tmp/opencode/tg0-tinygpu`. Repo recebe SÓ `.md`.

Fontes pinadas: `tinygrad@e8c8ba1c7751142f7b20d85692afd2da5cef5e84`, `tinygpu_releases@c0d024f9ff0e1dc8fdf217f255da7101d91e8323`.
Contrato de operações em `docs/tinygpu/pci-transport-contract.md`; fio byte-a-byte em `docs/tinygpu/remote-protocol.md`.
Arquitetura do binário em `docs/tinygpu/binary-architecture.md`.

## 1. `ensure_app` — pin, cache, install (system.py:416-427)

| Passo | Código real | Valor |
|---|---|---|
| `APP_PATH` | `system.py:417` | `/Applications/TinyGPU.app/Contents/MacOS/TinyGPU` |
| `commit` pinado | `system.py:421` | `c0d024f9ff0e1dc8fdf217f255da7101d91e8323` (idêntico ao HEAD do clone `tinygpu_releases`; ver `support-matrix.md` §2) |
| zip em cache | `system.py:422` + `helpers.py:_ensure_downloads_dir:453`, `fetch:464` | `TinyGPU_{commit}.zip` no dir de downloads; retorno cedo se zip em cache E `APP_PATH` existe (`system.py:423`) |
| kill anterior | `system.py:425` | `system("pkill -f TinyGPU")` com `suppress(RuntimeError)` |
| extração | `system.py:426` | `ditto -xk {fetch('https://github.com/tinygrad/tinygpu_releases/raw/{commit}/TinyGPU.zip')} /Applications` |
| install (prompt DriverKit) | `system.py:427` | `{APP_PATH} install` → `TinyGPUCLIRunner run(args=["TinyGPU","install"])` (`Shared/TinyGPUCLIRunner.swift:55-58`): `OSSystemExtensionRequest.activationRequest(forExtensionWithIdentifier: dextID)` + `OSSystemExtensionManager.shared.submitRequest` (`TinyGPUCLIRunner.swift:85-91`). Aprovação: `System Settings > General > Login Items & Extensions > Driver Extensions` (`docs/tinygpu.md:29-31`, `TinyGPUCLIRunner.swift:31-36`) |
| bootstrap doc | `extra/setup_tinygpu_osx.sh:1-10` | chama `APLRemotePCIDevice.ensure_app()`; `docs/tinygpu.md:22`: `curl .../extra/setup_tinygpu_osx.sh \| sh` |

CLI real do app (strings do binário + `TinyGPUCLIRunner.swift:47-83`): `status | install | uninstall | server <path> | help`.
`server` sem path → `Error: server requires socket path` (string no binário, `TinyGPUCLIRunner.swift:65`).
`Move TinyGPU to /Applications first.` e `failed to connect to tinygpu driver` são strings literais do binário (ver `binary-architecture.md`).

## 2. Socket UNIX — APL_REMOTE_SOCK, retry, spawn (system.py:429-439)

```python
sock_path, sock = getenv("APL_REMOTE_SOCK", temp("tinygpu.sock")), socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
for i in range(100):
  with suppress(ConnectionRefusedError, FileNotFoundError):
    sock.connect(sock_path); break
  if i == 0: subprocess.Popen([self.APP_PATH, "server", sock_path], stdout=DEVNULL, stderr=DEVNULL)
  time.sleep(0.05)
else: raise RuntimeError(f"Failed to connect to TinyGPU server at {sock_path}.")
super().__init__(devpref, "usb4", sock=sock)  # dev_id=0, peer_group=IP (aqui: unix socket)
```

- Default: `temp("tinygpu.sock")` = `$TMPDIR/tinygpu.sock` (`helpers.py:temp:162-163`, sem `append_user` → sem sufixo de usuário).
- Override: env `APL_REMOTE_SOCK` (`system.py:431`).
- Servidor: `run_server(sock_path)` (`server.c:259-285`): `socket AF_UNIX+SOCK_STREAM`, `bind`, `listen(1)`, `accept` em loop; **um cliente por vez** (`g_client_active`, `server.c:274-277`: segundo cliente é `rejected: client already connected` + `close`).
- Buffers: Python seta `SO_SNDBUF/SO_RCVBUF = 64<<20` (`system.py:390`); servidor seta `BULK_BUF_SIZE (64<<20)` (`server.c:186-188`). `TCP_NODELAY` só no TCP (`system.py:344`), não no unix socket.
- `pcibus` APL é sempre a string literal `"usb4"` (`system.py:439`) → `dev_id=0` (`system.py:388`: `int(pcibus.split(':')[-1]) if ':' in pcibus else 0` → `0`), `flock` em `f"{devpref}_usb4.lock"` (`system.py:392`).

## 3. RPC no caminho APL (subset — tabela completa no contract)

APL emite SOMENTE `MAP_BAR(1)`, `MAP_SYSMEM_FD(2)`, `CFG_READ(3)`, `CFG_WRITE(4)`, `RESET(5)`, `MMIO_READ(6)`, `MMIO_WRITE(7)`, `RESIZE_BAR(11)`.
NUNCA emite `PROBE(0)`, `MAP_SYSMEM(8)`, `SYSMEM_READ(9)`, `SYSMEM_WRITE(10)`, `PING(12)` (esses são TCP/bench; `server.c` retorna `status=1` para eles).
Sysmem APL lê/escreve via `mmap` local do fd recebido (`system.py:441-447`), não via RPC.

## 4. Caminho completo APLRemote → app → dext → IOPCIDevice → GPU

```
Python (tinygrad, macOS)
  PCIIface (ops_nv.py:556, vram_bar=1, base_class=0x03)
    └─ PCIIfaceBase (system.py:253)
         └─ APLRemotePCIDevice (system.py:416, pcibus="usb4", dev_id=0)
              │  AF_UNIX SOCK_STREAM  ($TMPDIR/tinygpu.sock ou $APL_REMOTE_SOCK)
              │  header 33B '<BIIQQQ' + payload?  →  header 17B '<BQQ' + readout?/fd?
              ▼
TinyGPU.app  (Contents/MacOS/TinyGPU, Swift+server.c)
  TinyGPUCLIRunner "server <path>" (TinyGPUCLIRunner.swift:64-66)
    └─ run_server() (server.c:259): bind/listen/accept (1 cliente)
         └─ handle_client() (server.c:185): recv 33B → switch cmd
              ├─ MAP_BAR → map_bar() (server.c:121): IOConnectMapMemory64(conn, bar) cache g_bars[6]
              ├─ CFG_READ/WRITE → dext_rpc(sel 0/1) (server.c:113-119,216-226)
              ├─ RESET → dext_rpc(sel 2) (server.c:231-233)
              ├─ MMIO_READ/WRITE → mmio_copy() direto em g_bars[bar].addr+off (server.c:63-67,235-247)
              ├─ MAP_SYSMEM_FD → map_sysmem_fd() (server.c:129): shm_open /tinygpu_%d + mmap
              │     + IOConnectCallStructMethod(sel=3 PrepareDMA) → paddrs → SCM_RIGHTS fd
              └─ RESIZE_BAR → noop OK (server.c:228-229)
              │  IOServiceOpen("tinygpu") via open_tinygpu() (server.c:96-111)
              │  falha → "Driver not available. Check: System Report > PCI ..." (server.c:195)
              ▼
DriverExtension (org.tinygrad.tinygpu.driver2.dext)
  TinyGPUDriverUserClient (TinyGPUDriverUserClient.cpp:99-191)
    ExternalMethod selectors (TinyGPUDriverUserClient.iig:6-12): 0=ReadCfg 1=WriteCfg 2=Reset 3=PrepareDMA
    CopyClientMemoryForType (TinyGPUDriverUserClient.cpp:167-191): type<6 → MapBar(bar); senão → CreateDMA(size)
      └─ TinyGPUDriver (TinyGPUDriver.cpp)
           ├─ Start_Impl (TinyGPUDriver.cpp:34-70): Open(pci), log ven/dev, set Command IOSpace|BusMaster|MemorySpace, SetName("tinygpu"), RegisterService()
           ├─ MapBar (TinyGPUDriver.cpp:95-103): GetBARInfo + _CopyDeviceMemoryWithIndex
           ├─ CfgRead/CfgWrite (TinyGPUDriver.cpp:159-186): ConfigurationRead/Write8/16/32
           ├─ ResetDevice (TinyGPUDriver.cpp:188-193): Reset(FunctionReset→HotReset)
           ├─ SetupDMA/CreateDMA (TinyGPUDriver.cpp:121-157): IODMACommand::Create + PrepareForDMA(maxAddressBits=40)
           ▼
         IOPCIDevice (PCIDriverKit, provider do sistema) → GPU física (tunneled, ver why-usb4/internal-pcie-gap)
```

Detalhe `MAP_SYSMEM_FD` (único com fd):
`server.c:map_sysmem_fd` alinha `size→page` (mín `0x4000` p/ `IOMemoryDescriptor`), `shm_open+ ftruncate+mmap`, chama `PrepareDMA` (sel 3) com `in=shm` (`alloc_sz`) e `out=paddr_buf[8192]`, `memcpy(ptr,paddr_buf,out_sz)`, guarda em `g_sysmem[128]`, responde `resp0=alloc_sz,resp1=idx` + `sendmsg(fd)`.
Python (`system.py:441-447`): `mmap(fd, mapped_size)` → `MMIOInterface` + parse `view(fmt='Q')[0::2]/[1::2]` como pares `(paddr,len)` até terminador `(0,0)` (`takewhile p[1]!=0`), expande para páginas 4K.
Limites: `MAX_SYSMEM=128`, `shm_name /tinygpu_%d`, `paddr_buf 8192` → máx ~512 pares; `PrepareDMA` exige buffers ≥4097 bytes (`TinyGPUDriverUserClient.cpp:134-137`); segmentos máx 32 (`IOAddressSegment segments[32]`).

Lifecycle single-client: `handle_client` abre `g_conn` por conexão (`open_tinygpu`), `cleanup()` no disconnect (`server.c:171-183`: `UnmapMemory`, `munmap+close+shm_unlink`, `IOServiceClose`); `on_disconnect` (`server.c:92-94`): `kIOMessageServiceIsTerminated → _exit(0)`.

## 5. Nota GA106

No macOS, `PCIIface.__init__` fala com `APLRemotePCIDevice` em vez de `/sys/bus/pci` (`system.py:85`).
`devices=((0xff00,(0x2200,...,0x2500,...)),)` + `0x2504&0xff00==0x2500` passa no filtro de família (ver `support-matrix.md` §5 e `tinygrad-nv/ops-nv-audit.md`); isso responde SOMENTE enumeração — bring-up GSP/GPFIFO fim-a-fim é UNKNOWN em TG0.
`vram_bar=1` → todo `bar_info/map_bar` APL usa `bar=1`; `is_bar_small` decide `alloc` sysmem vs devmem (`system.py:257,267-281`).
