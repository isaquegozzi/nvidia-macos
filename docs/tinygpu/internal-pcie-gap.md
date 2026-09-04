# TinyGPU — gap PCIe interno vs tunneled (TG0, análise estática)

> Fase TG0. SOMENTE análise estática. Ponto exato de divergência + diagrama corrigido pelo source.
> Fontes: `tinygrad@e8c8ba1c` (`system.py`, `server.c`, `TinyGPUDriver*`, `Info.plist`), dext shipped `c0d024f9` (`binary-architecture.md` §3/§5), gate `docs/macos/driver-architecture-gate.md` §3.

## Gap exato (1 frase)

O gap está no **matching DriverKit** (`IOPCITunnelCompatible` + filtro `built-in`/entitlement em `matchPropertyTable`), **antes** de `Start_Impl`/`Open` — o `IOPCIDevice` interno nunca instancia `TinyGPUDriver`, logo nenhum serviço `"tinygpu"` existe para o servidor abrir.

## Diagrama corrigido pelo source

```
                        MATCHING DriverKit (aqui diverge)
                        ───────────────────
                        Info.plist dext (source Info.plist:15-18 + binário §3):
                          IOProviderClass=IOPCIDevice
                          IOPCIClassMatch=0x03000000
                          IOPCITunnelCompatible=True
                        Entitlements dext (provision shipped §5):
                          transport.pci = [10de any, 1002 any], SEM ...builtin
                        Enforcement Apple (gate §3.1):
                          matchPropertyTable: built-in sem isenção → isMatch=false

GPU tunneled (USB4/TB, ex. eGPU)                    GPU interna (slot x16, ex. GA106 10de:2504)
  IOPCIDevice: class 0x03____,                        IOPCIDevice: class 0x03____,
    IOPCITunnelled=YES, built-in=AUSENTE                 IOPCITunnelled=AUSENTE, built-in=?(ver ioreg)
       │                                                     │
       │ class 0x03000000 ✔                                  │ class 0x03000000 ✔ (passaria isolado)
       │ tunnel ✔                                            │ tunnel ✘ (IOPCITunnelCompatible filtra)
       │ vendor 10de/1002 ✔                                  │ vendor ✔ mas built-in? → ✘ se presente
       │ built-in ausente ✔                                  │ (sem ...builtin no dext shipped)
       ▼                                                     ▼
  TinyGPUDriver::Start_Impl RUNS                    TinyGPUDriver::Start_Impl NUNCA RUNS
    (TinyGPUDriver.cpp:34-70: Open,                     (sem instância, sem Open,
     Command IOS|BM|MEM, SetName                       sem RegisterService)
     "tinygpu", RegisterService)
       │                                                     ╳ (sem serviço "tinygpu")
       ▼                                                     ▼
  IOServiceGetMatchingService("tinygpu") HIT         IOServiceGetMatchingService("tinygpu") MISS
    (server.c:98 open_tinygpu)                         → open_tinygpu retorna NULL
       │                                               → handle_client envia "Driver not available.
       ▼                                                Check: System Report > PCI ..." (server.c:195)
  IOServiceOpen → g_conn ✔                                e fecha; Python nunca chega a BAR/CFG/DMA
       │
       ▼
  MAP_BAR/CFG/MMIO/MAP_SYSMEM_FD/RESET ✔
    (server.c:121-247 → dext sel 0-3 + CopyClientMemory)
       │
       ▼
  Python APLRemotePCIDevice ("usb4"/0) ✔
    bar_info/size, MMIO views, sysmem mmap(fd), cfg
       │
       ▼
  PCIIface (vram_bar=1, base_class=0x03) → NVDev bring-up
```

## O que NÃO é o gap (correções comuns)

- NÃO é filtro Python `0x2500`: `0x2504&0xff00==0x2500` passa (`ops_nv.py:560`, `system.py:80`; `support-matrix.md` §5). Enumeração ≠ matching DriverKit.
- NÃO é BAR/MMIO/DMA/GSP: esses são downstream idênticos após o match (`MapBar/CfgRead/PrepareDMA` em `TinyGPUDriver.cpp:95-193` só executam SE o match ocorreu).
- NÃO é bridge ASM2464: ASM é o transporte Linux `USBPCIDevice` (`usb.py:135`), não o matching APL; zero strings ASM no `.app`/dext (ver `binary-architecture.md` §7, `why-usb4.md` E=FALSE).
- NÃO é socket/RPC: `tinygpu.sock` + header 33B/17B + `SCM_RIGHTS` funcionam igual para qualquer GPU uma vez que o serviço exista (`apl-remote-pci.md` §2, `remote-protocol.md` §2).

## Verificação obrigatória antes de qualquer conclusão viva (não executada em TG0)

No Tahoe alvo, com a GPU interna presente: `ioreg -l -w0` no nó `10de:2504` por `built-in`, `IOPCITunnelled`, `class-code`, `IOServiceDEXTEntitlements`, `AAPL,slot-name` (`driver-architecture-gate.md` §3.2, `ADR-0001` consequências); `systemextensionsctl list` para estado do dext; `log show --predicate tinygpu` para `Start_Impl`/`NewUserClient`.
Em TG0 tudo acima é `REQUIRES_MACOS_INSPECTION` — o gap aqui é provado no nível de source/binário, não no nível de ioreg vivo.
