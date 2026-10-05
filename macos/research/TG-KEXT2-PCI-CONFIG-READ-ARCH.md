> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# TG-KEXT2-PCI-CONFIG-READ-ARCH — leitura PCI config read-only (ESPECIFICAÇÃO)

> DESENHO apenas (2026-09-07). Sem código/build/EFI/reboot/hardware-access.
> Primeira fase com hardware real: SOMENTE leitura de config space da função
> RTX 01:00.0. Destino final: `docs/macos/TG-KEXT2-PCI-CONFIG-READ-ARCH.md`.

## 1. Menor privilégio

Só `configRead8/16/32`. PROIBIDO: configWrite*, MSE/BME, BAR map/MMIO, DMA,
interrupts/MSI, reset/FLR, GSP/firmware, VRAM, submit/compute/accel.
Qualquer necessidade dessas = ARCH_REJECT.

## 2–3. Allowlist fixa (sem offset arbitrário; 1 snapshot, §10)

Offsets `kIOPCIConfig*` (IOPCIDevice.h; `kIOPCIConfigSpace = 0`):

| Campo | Offset | Width | Classe |
|---|---|---|---|
| Vendor/Device | 0x00/0x02 | 16 | READ_SAFE |
| Command/Status | 0x04/0x06 | 16 | READ_SAFE (R/W1C do Status só limpa em ESCRITA; leitura não-destrutiva) |
| Rev/ProgIf/Sub/Base | 0x08–0x0B | 8 | READ_SAFE |
| CLS/Latency/Header/BIST | 0x0C–0x0F | 8 | READ_SAFE (BIST só dispara em escrita do bit 7) |
| BAR0–5 raw | 0x10–0x24 | 32 | READ_SAFE (valor bruto; write-probe `0xffffffff` PROIBIDO) |
| Subsys VID/DID | 0x2C/0x2E | 16 | READ_SAFE |
| Expansion ROM raw | 0x30 | 32 | READ_SAFE |
| Capabilities Ptr | 0x34 | 8 | READ_SAFE (só ponteiro — opção A) |
| IntLine/IntPin | 0x3C/0x3D | 8 | READ_SAFE |
| Min_Gnt/Max_Lat… | — | — | SKIP_FOR_NOW (sem necessidade) |

## 4–5. Command/Status e BARs

Leitura apenas; MSE/BME/SERR/Parity/INTx observados, nunca tocados.
BAR: `configRead32` do valor atual + interpretação passiva de bits;
SEM write-probe, SEM sizing destrutivo, SEM map.

## 6–8. Fronteira

PCI genérico (sem offsets MMIO NVIDIA/Nouveau). Arquitetura:
`ga106ctl → UC → GA106Lab::copyPciSnapshot() → configRead* (allowlist fixa,
sem offset/width/valor de fora)`. UC SEM `IOPCIDevice*` (mantido).

## 9–10. ABI + selector único

`selector 3 = GET_PCI_SNAPSHOT` (0/1/2 intactos). Struct 84 bytes, align 4:

```c
/* offsets: size@0 version@4 status@8 validMask@12 vendor@16 device@18
   command@20 statusReg@22 rev@24 progIf@25 sub@26 base@27 cls@28 lat@29
   hdr@30 bist@31 barRaw[6]@32 subsysVID@56 subsysID@58 romRaw@60
   capPtr@64 intLine@65 intPin@66 r0@67 reserved[4]@68 — sizeof 84 */
```

`status`: 0 = ok; `validMask` bit por grupo (identity/cmd/header/bars/subsys/
cap/irq); `0xffff/0xffffffff` = inválido (master-abort), nunca mascarado.
`static_assert` de size + offsets-chave nos dois lados.

## 11–13. Policy, cliente, concorrência

ROOT_ONLY + single-client preservados. Sem lock extra: single-client serializa
NOSSAS chamadas; config space é serializado pela IOPCIFamily; spin lock durante
configRead seria incorreto (pode bloquear). Sem travar nada além do slot.

## 14–16. Side effects, RTC, capabilities, bounds

`PCI_CONFIG_READ_SIDE_EFFECT_RISK = LOW`: leituras PCI config não têm efeitos
(hardware read puro; framework sem hooks de escrita; R/W1C e BIST só reagem a
write). RTC/sticky em 0x00–0xFF: só Status tem bits R/W1C — leitura segura,
documentada. Capabilities: opção A (só o ponteiro). Bounds: 0x00–0xFF.

## 17–20. ga106ctl pci + correlações + MSE/BME

`sudo ga106ctl pci` imprime raw + interpretação separada (raw ≠ certeza).
Live futuro: identidade == IORegistry; comparar com baseline Linux (diferença de
alocação de firmware ≠ falha); Command repetido estável + MSE/BME inalterados
(`COMMAND_REGISTER_UNCHANGED = YES`, sem escrever); disassembly prova
`configWrite* = NONE`.

## 21–24. Metal, fases, gate

PCI identity/recursos → BAR discovery → map → MMIO → bring-up → filas →
rendering → IOGPU/Accelerator → Metal (Metal NÃO começa aqui).
Fases: KEXT2-A read-only → KEXT2-B validação → KEXT3 map → KEXT4 MMIO
(KEXT2+ sem detalhe).

```text
TG_KEXT2_PCI_READ_ARCH_READY = YES (sem offsets arbitrários, allowlist fixa,
sem writes/probe/map/MMIO/DMA/irq/reset/GSP, root+single preservados, ABI
versionada, erros definidos, side-effects auditados, PASS + Metal definidos)
```

UM próximo passo (sem executar): instanciar KEXT2 (`copyPciSnapshot` + selector 3
+ `ga106ctl pci`) em árvore nova, PASS/KEXT1 intactos.
