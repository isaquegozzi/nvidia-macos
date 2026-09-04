# TinyGPU — protocolo remoto no fio (TG0, análise estática)

> Fase TG0. SOMENTE análise estática. Complemento fio-a-fio de `pci-transport-contract.md` (contrato) e `apl-remote-pci.md` (caminho).
> Fontes: `tinygrad@e8c8ba1c`: `tinygrad/runtime/support/system.py:376-414`, `extra/remote/serve.py:1-101`, `extra/usbgpu/tbgpu/installer/Shared/server.c:1-285`, `extra/remote/bench.py:1-30`.

## 1. Enquadramento

Duas implementações de servidor falam o mesmo header, com cobertura diferente:

| Servidor | Transporte | Cobre os 13 `RemoteCmd`? | Uso |
|---|---|---|---|
| `extra/remote/serve.py` | TCP `0.0.0.0:6667` (`serve.py:86-101`) | SIM (todos, incl. `PROBE/MAP_SYSMEM/SYSMEM_RW/PING`) | Referência genérica `REMOTE=host:port` (Linux→Linux). Abre `PCIDevice` real via `System.list_devices` (`serve.py:25,32-36`) |
| `server.c` (dentro da TinyGPU.app) | UNIX socket (`run_server:259`) | NÃO — só `MAP_BAR/MAP_SYSMEM_FD/CFG_READ/CFG_WRITE/RESET/MMIO_READ/MMIO_WRITE/RESIZE_BAR(noop)` | Caminho APL/macOS. Suficiente porque o cliente APL nunca emite os outros 5 IDs |

O cliente é sempre `RemotePCIDevice._rpc` (`system.py:376-385`) + `_bulk_read/_bulk_write` (`system.py:394-399`) + `RemoteMMIOInterface` (`system.py:314-332`).

## 2. Formato do header (ambos os lados)

```c
// server.c:35-36
typedef struct { uint8_t cmd; uint32_t dev_id, bar; uint64_t arg0, arg1, arg2; } __attribute__((packed)) request_t;   // 33 bytes '<BIIQQQ'
typedef struct { uint8_t status; uint64_t resp0, resp1; } __attribute__((packed)) response_t;                          // 17 bytes '<BQQ'
```

```python
# system.py:377,381
sock.sendall(struct.pack('<BIIQQQ', cmd, dev_id, bar, *(*args, 0, 0, 0)[:3]) + payload)
msg, fd = recvmsg(17) if has_fd else (_recvall(sock, 17), None)  # 17 bytes '<BQQ'
if resp[0] != 0: raise RuntimeError(_recvall(sock, resp[1]).decode() if resp[1] > 0 else 'unknown error')
return (resp[1], resp[2]) + ((_recvall(sock, readout_size) if readout_size > 0 else None),) + (fd,)
```

- `cmd` = `RemoteCmd` 0-12 (`system.py:312`, `server.c:19-33`).
- Args não usados são zero-preenchidos (`(*args,0,0,0)[:3]`).
- `payload` (só `PROBE` e escritas bulk) é concatenado após os 33 bytes, num único `sendall` (`system.py:377,399`).
- Resposta de erro: `status=1, resp0=len(msg)` + `msg` em seguida (`serve.py:7`, `server.c:84-88`).
- FD (`só MAP_SYSMEM_FD`): `sendmsg` com `SCM_RIGHTS` (`server.c:69-82`); cliente `recvmsg(17, CMSG_LEN(4))` + `struct.unpack('<i', anc[0][2][:4])` (`system.py:379-380`).

## 3. Leituras vs escritas bulk (assimetria crítica)

- Leitura (`_bulk_read:394-396`): `send header` → `recv header(17B)` → `recv readout(size)`. Contabiliza `_bulk_recv += size`. Servidor: `send_response(resp)+send(dados)` (`server.c:239-240`, `serve.py:54-55,67` com `sendmsg` fast-path `I` quando `off%4==0 and size==4`).
- Escrita (`_bulk_write:397-399`): `send header+dados` em UM `sendall`, **sem `recv`**. Contabiliza `_bulk_sent`. Servidor: `recvall(arg1)` → escreve → `continue` SEM resposta (`server.c:243-247`, `serve.py:56-60,68-69`). Por isso `serve.py:82` transforma exceção em escrita em `ConnectionError` (não há canal p/ erro) e o cliente nunca saberá de falha de escrita além de disconnect.
- Estatística: `_rpc_count`, `_bulk_sent/_bulk_recv`, `_start_time`, `atexit _print_stats` com `DEBUG>=1` (`system.py:348-355`).

## 4. `PROBE` (só TCP)

Cliente (`system.py:360-366`): `payload = array('I', chain((mask,dev)...)).tobytes()` (8 bytes por par `(mask,dev)` LE); `q(r)`: `remote_sock(host,port)` (`TCP_NODELAY`, `REMOTE_TIMEOUT=3`, `system.py:342-347`), `_rpc(sock,0,PROBE, base_class or 0, len(payload), vendor, payload=payload)`, `recvall(data_len).decode().split('\n')` → `[(sock, f"remote:{host}:{port}:{d}")]`.
Servidor (`serve.py:18-29`): `recv(arg1)` payload, reconstrói `filter_devices {mask:[devs]}`, `System.list_devices(vendor, tuple, base_class)`, registra em `discovered_devices`, responde `resp(len(data),len(devs))+data` onde `data="pcibus:idx\n..."`. Abertura lazy por `dev_id` (`serve.py:32-36`: `cl("SV", pcibus)`).
`server.c` NÃO implementa `PROBE` (APL fixa `"usb4"/dev_id=0, sem descoberta).

## 5. `MAP_SYSMEM` vs `MAP_SYSMEM_FD` (não confundir)

| | `MAP_SYSMEM (8)` — TCP | `MAP_SYSMEM_FD (2)` — APL |
|---|---|---|
| Emissor | `RemotePCIDevice.alloc_sysmem:402` | `APLRemotePCIDevice.alloc_sysmem:442` (override) |
| Resposta | `(paddrs_len,handle)+paddrs pack '<NQ'` (`serve.py:61-65`) | `(alloc_sz,idx)+fd SCM_RIGHTS` (`server.c:129-165`) |
| View resultante | `RemoteMMIOInterface(handle, rd/sy SYSMEM_RW)` (`system.py:404`) — todo acesso posterior é RPC `SYSMEM_READ/WRITE` | `MMIOInterface(mmap(fd), size)` LOCAL (`system.py:443`) — acessos posteriores NÃO tocam o socket; paddrs lidos do início do `mmap` como `Q` pares até `(0,0)` (`system.py:446`) |
| Servidor oposto | `server.c` não implementa | `serve.py` não implementa |

## 6. Tabela de divergência serve.py × server.c (por cmd)

| Cmd | `serve.py` (TCP ref) | `server.c` (APL ship) | Cliente que emite |
|---|---|---|---|
| `PROBE 0` | `18-29` full | ausente → `default status=1` | só TCP `remote_list` |
| `MAP_BAR 1` | `38-40` lazy `map_bar` + `bar_info` | `121-127` `IOConnectMapMemory64` + cache | ambos |
| `MAP_SYSMEM_FD 2` | ausente (`unknown command`) | `129-165` shm+PrepareDMA+fd | só APL |
| `CFG_READ 3` | `41-42` | `216-220` sel 0 | ambos |
| `CFG_WRITE 4` | `43-45` (`write_config(arg0,arg2,arg1)`) | `222-226` sel 1 | ambos |
| `RESET 5` | `49-51` | `231-233` sel 2 | ambos (raro) |
| `MMIO_READ 6` | `52-55` | `235-241` | ambos (via view) |
| `MMIO_WRITE 7` | `56-60` sem resposta | `243-247` sem resposta | ambos (via view) |
| `MAP_SYSMEM 8` | `61-65` | ausente | só TCP |
| `SYSMEM_READ 9` | `66-67` | ausente (APL usa mmap) | só TCP |
| `SYSMEM_WRITE 10` | `68-69` sem resposta | ausente | só TCP |
| `RESIZE_BAR 11` | `46-48` real | `228-229` noop OK | ambos (`PCIIfaceBase.__init__:263` suprime erro) |
| `PING 12` | `15-16` | ausente | só `bench.py` |

`RESIZE_BAR` noop no APL é correto: BARs já expostos pelo `IOPCIDevice`; `PCIIfaceBase` ignora falha (`contextlib.suppress(Exception)`).
`PING` no APL cairia em `default: status=1` — irrelevante pois nada no bring-up o emite.
