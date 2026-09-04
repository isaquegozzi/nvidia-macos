# TinyGPU — gap PCIe interno vs tunneled (TG0, análise estática)

> Fase TG0. SOMENTE análise estática. Ponto exato de divergência + diagrama corrigido pelo source.
> Fontes: `tinygrad@e8c8ba1c` (`system.py`, `server.c`, `TinyGPUDriver*`, `Info.plist`), dext shipped `c0d024f9` (`binary-architecture.md` §3/§5), gate `docs/macos/driver-architecture-gate.md` §3.

## Gap exato (1 frase) — [SUPERADO — ver Correção TGM0 abaixo]

> Texto original preservado: O gap está no **matching DriverKit** (`IOPCITunnelCompatible` + filtro `built-in`/entitlement em `matchPropertyTable`), **antes** de `Start_Impl`/`Open` — o `IOPCIDevice` interno nunca instancia `TinyGPUDriver`, logo nenhum serviço `"tinygpu"` existe para o servidor abrir.
>
> Leitura corrigida: o gap **hipotetizado** está no matching DriverKit **antes** de `Start_Impl`/`Open`, mas a perna `IOPCITunnelCompatible`-como-filtro é `UNKNOWN` (a chave declara capacidade Thunderbolt do driver, não prova exclusão de não-tunneled — Apple canônica em Correção TGM0). A perna `built-in`/entitlement segue com mecanismo `CONFIRMED` e aplicabilidade `UNCERTAIN` (requer `ioreg`). Se o matching falhar por qualquer perna, o `IOPCIDevice` interno não instancia `TinyGPUDriver` e nenhum serviço `"tinygpu"` existe para o servidor abrir — mas qual perna falha (se alguma) está **em aberto** (ver Resultado A/B/C).

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
       │ tunnel ✔ (suporte declarado)                      │ tunnel ? (SUPERADO o "✘ filtra" — ver Correção TGM0:
       │                                                     │ Compatible = capacidade declarada; exclusão = UNKNOWN)
       │ vendor 10de/1002 ✔                                  │ vendor ✔ mas built-in? → ✘ se presente
       │ built-in ausente ✔                                  │ (sem ...builtin no dext shipped)
       ▼                                                     ▼
  TinyGPUDriver::Start_Impl RUNS                    TinyGPUDriver::Start_Impl EM ABERTO (SUPERADO o "NUNCA RUNS"
    (TinyGPUDriver.cpp:34-70: Open,                     determinístico — ver Resultado A/B/C; depende de qual
     Command IOS|BM|MEM, SetName                       perna do matching falha, se alguma)
     "tinygpu", RegisterService)
       │                                                     ╳? (sem serviço "tinygpu" SOMENTE se o match falhar —
       ▼                                                     ver Resultado A/B/C)
  IOServiceGetMatchingService("tinygpu") HIT         IOServiceGetMatchingService("tinygpu") HIT/MISS EM ABERTO
    (server.c:98 open_tinygpu)                         → open_tinygpu retorna NULL SOMENTE se sem serviço
       │                                               → handle_client envia "Driver not available.
       ▼                                                Check: System Report > PCI ..." (server.c:195)
  IOServiceOpen → g_conn ✔                                SOMENTE nesse caso; Python nunca chegaria a BAR/CFG/DMA
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

## Correção TGM0 (2026-09-04) — semântica `IOPCITunnelCompatible`

Bug TG0: este doc tratou `IOPCITunnelCompatible=true` como filtro que excluiria o `IOPCIDevice` interno do matching ("tunnel ✘ filtra", "`Start_Impl` NUNCA RUNS" determinístico). Correção perante a documentação Apple atual (fonte canônica):

- `IOPCITunnelCompatible` = a personality **DECLARA suportar PCI tunneled/Thunderbolt** (capacidade do driver): "To indicate that your PCIe driver supports Thunderbolt, include the `IOPCITunnelCompatible` key [...]" (`developer.apple.com/documentation/pcidriverkit/creating-custom-pcie-drivers-for-thunderbolt-devices`); "This opt-in key protects consumers against older drivers that have not yet been updated to support Thunderbolt technology." (Thunderbolt Device Driver Programming Guide, arquivo Apple). A recíproca — presença excluiria provedores não-tunneled — **não** é documentada.
- A presença real de túnel é indicada por **`IOPCITunnelled` no IORegistry** (propriedade do dispositivo/caminho): "drivers can determine if a Thunderbolt device is connected by recursively searching over parents in the I/O Registry for the key `IOPCITunnelled`" (mesmo guia). Nenhuma checagem de `IOPCITunnelled` foi encontrada no código/server TG0 (`TinyGPUDriver*.cpp/.iig`, `server.c`; strings do dext com zero `Thunderbolt/USB4/tunnel` — `binary-architecture.md` §7).

## Resultado A/B/C — todos em aberto (Correção TGM0)

| Resultado | Enunciado | Veredito | O que falta |
|---|---|---|---|
| A — tunneled casa | GPU USB4/TB instancia `TinyGPUDriver` (`Start_Impl` roda, serviço `"tinygpu"` existe) | `LIKELY` (em aberto até teste vivo) | Esperado pela declaração `Compatible` + requisito USB4/TB documentado (`docs/tinygpu.md:8,15`), mas nunca observado em TG0 (sem hardware). |
| B — interna vs `IOPCITunnelCompatible` | Nó interno não-tunneled é excluído do matching pela presença de `IOPCITunnelCompatible=true` | `UNKNOWN` (SUPERADO o "✘ filtra" determinístico) | Apple não documenta a exclusão; sem evidência de checagem de `IOPCITunnelled` no driver/server. Só `ioreg` + teste vivo resolvem. |
| C — interna vs `built-in` | Nó interno é excluído por `built-in` sem isenção `...builtin` | `UNCERTAIN` (mecanismo `CONFIRMED`, aplicabilidade aberta) | `matchPropertyTable` checa `built-in` (gate §3.1); dext shipped sem `...builtin`; mas `built-in` no nó `10de:2504` depende de ACPI/OpenCore — requer `ioreg` (verificação abaixo). |

## Verificação obrigatória antes de qualquer conclusão viva (não executada em TG0)

No Tahoe alvo, com a GPU interna presente: `ioreg -l -w0` no nó `10de:2504` por `built-in`, `IOPCITunnelled`, `class-code`, `IOServiceDEXTEntitlements`, `AAPL,slot-name` (`driver-architecture-gate.md` §3.2, `ADR-0001` consequências); `systemextensionsctl list` para estado do dext; `log show --predicate tinygpu` para `Start_Impl`/`NewUserClient`.
Em TG0 tudo acima é `REQUIRES_MACOS_INSPECTION` — [SUPERADO "o gap aqui é provado no nível de source/binário" — ver Correção TGM0/Resultado A/B/C] o que o source/binário prova é a presença declarativa (`IOPCIClassMatch`, `IOPCITunnelCompatible`, allowlist vendor, ausência de `...builtin`); qual perna excluiria a interna (se alguma) está em aberto até `ioreg` vivo.
