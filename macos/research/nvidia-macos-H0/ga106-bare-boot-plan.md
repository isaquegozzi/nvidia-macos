# GA106 BARE boot plan — UM boot experimental reversível (H0, NÃO EXECUTADO)

> Fase H0, SOMENTE LEITURA. Este documento é plano doc-only, criado em 2026-09-05 no
> macOS Tahoe 26.6.2 (25G83) MacPro7,1. **NADA aqui foi aplicado**: nenhuma EFI alterada,
> nenhum NVRAM/boot-arg tocado, nenhum reboot, nenhum TinyGPU instalado.
> Repo `nvidia-macos` em `/Volumes/Downloads` = NTFS read-only → este draft vive em
> `~/Documents/nvidia-macos-H0/ga106-bare-boot-plan.md`; copiar para
> `docs/macos/hardware/ga106-bare-boot-plan.md` quando o repo estiver gravável.

## 0. Objetivo (UM boot)

```text
AMD 1002:1638 → continua mantendo o desktop (sem mudar framebuffer/config)
RTX 10de:2504 → permanece presente no IORegistry como IOPCIDevice exposto
                 → sem TinyGPU, sem driver nosso, sem escrita, só leitura pós-boot
```

Isolar UMA variável: `RTX hidden → RTX visible`. Nada mais muda.

## 1. Estado atual confirmado (H0, evidência §8)

- Effective `boot-args` = `-v debug=0x100 keepsyms=1 -amfipassbeta -wegnoegpu`
  (origem: `EFI/OC/config.plist` NVRAM Add `7C436110-...`, GUID Apple; NVRAM runtime idêntico).
- `DeviceProperties → Add` contém `PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0)` =
  `00:01.1 → 01:00.0` = RTX, com `disable-gpu = true`.
- Ambos os gatilhos são consumidos por **Lilu 1.7.3** (único kext com as strings
  `-wegnoegpu` + `disable-gpu` + `failed to terminate external gpu`; WEG ausente em
  EFI/config/runtime → hipótese A FALSE, B CONFIRMED).
- ACPI: 4 SSDTs (`EC`, `PLUG`, `USB-Reset`, `USBX`, 112–277 B), nenhum caminho GPU
  (`GPP0/PEG/PEGP/GFX0/D004` ausentes) → hipótese ACPI FALSE.
- Log: `Found 10de:2504` + `Found 10de:228e` às 15:58:23.105 → `bridge 0:1:1 removing
  child` ambos às 15:58:24.023 (~0,9 s depois). Emissor: `IOPCIFamily` (kernel);
  gatilho: patch Lilu (silencioso no sucesso; sem linhas Lilu/NootedRed no window).

## 2. Mudança mínima proposta (TESTE, não aplicar agora)

Ideal (UMA variável de cada vez — testar nesta ordem, um boot por etapa):

```text
ANTES (atual):
  boot-args = ... -wegnoegpu ...
  DeviceProperties Add PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0) = { disable-gpu = true }

TESTE 1 (mínimo absoluto — só boot-arg):
  boot-args = -v debug=0x100 keepsyms=1 -amfipassbeta [sem -wegnoegpu]
  DeviceProperties = inalterado (disable-gpu=true permanece)
  → Se RTX continuar HIDDEN: prova que disable-gpu sozinho basta (Lilu lê a property).
  → Se RTX virar BARE: prova que o boot-arg era o gatilho dominante.

TESTE 2 (só se TESTE 1 ainda HIDDEN — só DeviceProperty):
  boot-args = sem -wegnoegpu (mantém resultado do TESTE 1)
  DeviceProperties Add PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0) = removido OU disable-gpu=false
  → Isola a property como segunda variável.
```

Só escolher TESTE 1 se ANTES provar (já provado neste H0, §§5–6) que não existe
`disable-external-gpu`, SSDT de disable, nem segunda property escondendo a placa —
existe `disable-gpu=true`, logo o TESTE 1 pode não bastar; o plano prevê os dois passos
separados para não confundir as variáveis. Nunca remover os dois de uma vez no primeiro boot.

SEM alterar qualquer outro argumento (`-v debug keepsyms -amfipassbeta` intactos).
NÃO mudar SMBIOS, NootedRed, framebuffer AMD, ACPI, nem adicionar properties NVIDIA,
spoof, ou drivers NVIDIA antigos.

## 3. Como aplicar (quando autorizado, manual, com rollback pronto)

Pré-requisitos (todos verificados antes de tocar qualquer coisa):

1. Monitor SÓ na placa-mãe (AMD); confirmar `About This Mac > Displays` = AMD antes.
2. Backup read-only da EFI: `cp -R` (via Finder ou `ditto`) de
   `/Volumes/MACOS PENDR/EFI/OC/config.plist` para local seguro + anotar MD5.
   EFI montada read-only nesta fase (`diskutil mount readOnly`); remontar RW só no ato,
   com `sudo`, e só o tempo de editar um arquivo.
3. Ter à mão um segundo meio de boot (o próprio picker OC com `ShowPicker=true Timeout=5`
   + opção `ResetNvramEntry.efi` visível) e saber voltar (seção 4).
4. Fotografar/anotar os boot-args atuais (`nvram boot-args`) para restauração literal.

Aplicação TESTE 1 (exemplo via OpenCore Configurator ou `plutil`, UMA edição):

- Abrir cópia de `config.plist` → `NVRAM → Add → 7C436110-...` → `boot-args` →
  remover somente o token `-wegnoegpu` (manter espaços simples).
- Salvar, desmontar EFI, reboot MANUAL pelo picker (sem auto-reboot scripts).
- NÃO rodar `nvram` userspace, NÃO `systemextensionsctl developer on`, NÃO TinyGPU.

## 4. Plano de recuperação (voltar ao estado atual)

O estado atual (HIDDEN) é o fallback conhecido-bom (boota com desktop AMD).
Para cada caso, o retorno é a mesma ação: restaurar `boot-args` com `-wegnoegpu`
e/ou `disable-gpu=true`, reboot.

### sucesso

macOS inicia pela AMD e `ioreg → NVIDIA 10de:2504 presente` (BARE).

- Executar comandos §5, salvar snapshots, **não instalar nada**, reboot de volta?
  NÃO — permanecer no boot BARE o mínimo necessário para coletar, depois decidir em
  revisão se o próximo boot mantém BARE ou volta a HIDDEN. Rollback = re-adicionar
  `-wegnoegpu` (e manter `disable-gpu`) → reboot → confirmar HIDDEN de volta
  (`ioreg` sem `10de` + log `removing child`).

### falha A — inicia, mas GUI com problema (ex.: login roxo, sem HIDPI, glitch, display errado)

- Causa provável: macOS tenta algo com a RTX exposta (ex.: AGDP/framebuffer policy)
  sem conseguir. Ação: reboot → no picker OC, escolher mesmo volume com
  `boot-args` restaurado (se editou EFI, reeditar de outro OS/pendrive ou via
  Recovery/Terminal). Se GUI impedir edição no Tahoe, editar `config.plist` do
  Linux (Limine na `disk3s4`, CachyOS) ou de outro Mac — a EFI é MSDOS, legível em Linux.
- Critério de abort: 1 ocorrência reproduzível de GUI degradada = voltar a HIDDEN e
  documentar (foto + `log show --last boot` + `ioreg`).

### falha B — não chega à interface (trava na maçã/barra, tela preta com cursor, login ausente)

- Aguardar 10 min (NVMe/picker timeout). Forçar power-button 5 s → ligar → picker OC →
  boot com config restaurada (HIDDEN). Se o volume Tahoe não aparece, bootar Linux
  (Limine) e reeditar a EFI do pendrive por lá.
- Nunca deixar a máquina em loop: após 2 tentativas sem GUI, restaurar HIDDEN e parar.

### falha C — kernel panic / reboot loop

- Fotografar a panic screen (`-v keepsyms debug=0x100` já garantem texto).
- Boot seguinte: segurar a tecla do picker → `ResetNvramEntry.efi`? **NÃO** resetar NVRAM
  sem anotar antes (apagaria `boot-args` inteiro). Preferir: editar EFI por fora
  (Linux/pendrive secundário) restaurando `-wegnoegpu` + `disable-gpu=true`.
- Se loop antes do picker: desplugar a RTX fisicamente (último recurso, com máquina
  desligada + PSU off) → boot AMD puro → restaurar EFI → replugar.
- Toda panic vira `docs/research/` post-mortem com foto + `log show` do boot anterior
  (se acessível) antes de qualquer nova tentativa.

## 5. O que verificar no primeiro boot BARE (somente leitura, sem TinyGPU)

Executar nesta ordem (todos read-only; nenhum pede `sudo` nem altera estado):

```sh
# 5a. Panorama + presença RTX (decide RTXPRESENT)
ioreg -l -w0 -c IOPCIDevice | grep -i -E "vendor-id|device-id|class-code" | grep -i -B2 -A2 "10de\|2504" | head -n 40
system_profiler SPPCIDataType -detailLevel mini
system_profiler SPDisplaysDataType -detailLevel mini

# 5b. Campos de decisão (runbook §2, ambos os nós + build)
ioreg -l -w0 -c IOPCIDevice | grep -i -E "built-in|IOPCITunnelled|vendor-id|device-id|class-code|IOServiceDEXTEntitlements|AAPL,slot-name" | head -n 60
ioreg -l -w0 -c IOPCIDevice | grep -B5 -A30 -i "0x2504" | head -n 80
ioreg -l -w0 -c IOPCIDevice | grep -B5 -A30 -i "0x1638" | head -n 40
ioreg -l -w0 -c IOPCIDevice | grep -i -E "IOPCITunnelled|IOPCITunnelCompatible|IOThunderbolt|AppleThunderbolt" | head -n 20
sw_vers; system_profiler SPHardwareDataType | head -n 15

# 5c. Current provider (sem detach) + dexts + kexts
ioreg -l -w0 -c IOPCIDevice | grep -i -E "CFBundleIdentifier|IOClass|IOMatchCategory|TinyGPUDriver|tinygpu|AMDRadeon|NVDA|GeForce" | head -n 30
systemextensionsctl list
kextstat | grep -i -E "nvidia|amd|radeon|whatevergreen|lilu|nootedred|IOGraphics|IOAccelerator" | head -n 20
kmutil showloaded 2>/dev/null | grep -i -E "Lilu|WhateverGreen|NootedRed|IOPCIFamily" | head -n 10

# 5d. Logs do boot (prova Found vs removing child)
log show --last boot --style compact | grep -E "15:5[0-9]" | grep -i -E "10de|2504|228e|removing child|terminate external" | head -n 20
log show --predicate 'eventMessage CONTAINS "tinygpu"' --last 1h | head -n 10
nvram boot-args
```

Registrar (valores literais, sem PII — sanitizar serial/UUID/MAC antes de colar em docs):

```text
RTX_PRESENT = YES / NO  (YES = nó IOPCIDevice 10de:2504 existe)
vendor = 10de (esperado) / outro
device = 2504 (esperado) / outro

AMD_DESKTOP = WORKING / DEGRADED / ABSENT

RTX_DRIVER = NONE (esperado: sem filho driver) / GENERIC / OTHER (anotar IOClass filho)

RTX_BUILT_IN = YES / NO / ABSENT / UNKNOWN

RTX_TUNNELLED = YES / NO / ABSENT / UNKNOWN
```

Critério de sucesso do boot BARE: `RTX_PRESENT=YES` + `AMD_DESKTOP=WORKING` +
`RTX_DRIVER=NONE` + nenhuma instalação/alteração feita. Qualquer outro resultado vira
`docs/macos/hardware/ga106-ioreg-bare.md` (novo) com os campos acima + snapshot sanitizado
em `artifacts/macos/` (gitignored) e reavaliação A/B/C.

## 6. Fora de escopo deste teste (explicitamente NÃO fazer)

SMBIOS, NootedRed, AMD framebuffer, ACPI, properties NVIDIA, spoof RTX, drivers NVIDIA
antigos, `systemextensionsctl developer on`, TinyGPU.app, KEXT próprio, `kmutil`,
MMIO/BAR, `unbind`, SIP/AMFI, NVRAM além do token único.
