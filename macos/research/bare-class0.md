> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# BARE-CLASS0 — experimento pronto, NÃO ativado, SEM reboot (2026-09-05/06)

> Partiu do BARE-FAILED (sem `-wegnoegpu`, sem `disable-gpu`) + 1 variável:
> `class-code` diagnóstico na RTX. Config vivo (`config.plist`) segue SAUDÁVEL —
> ativação é um `cp` manual do usuário no momento do reboot. Repo NTFS read-only →
> draft aqui; destino final `docs/macos/hardware/bare-class0.md`.

## Classe diagnóstica escolhida (NÃO 0x028000)

```text
DIAGNOSTIC_CLASS = 0xFF0000
base class  = 0xFF — Unassigned Class (Vendor specific)
subclass    = 0x00
prog-if     = 0x00
valor 24b   = 0xFF0000
```

Fontes: PCI-SIG Code & ID Assignment spec (base FF = Unassigned; 0x41–0xFE reservadas);
osdev PCI (tabela: `0xFF - Unassigned Class (Vendor specific)`); Linux `pci_ids.h`
(`PCI_CLASS_OTHERS 0xff`); precedente selvagem `Unassigned class [ff00]` (card reader
Realtek, sem driver especial — comportamento genérico). Rejeitado 0x028000 (network
atrairia outro stack) e FFFFFFFF (Dortania/OCLP usam `class-code=FFFFFFFF` + `#display`
para ESCONDER a GPU — all-ones = convenção "sem dispositivo" no scan PCI; risco de
voltar ao HIDDEN em vez de expor PCI genérico).

Por que deve funcionar: matching DriverKit/IOPCI é por classe (`IOPCIClassMatch
0x03000000` nas personalities display — gate TGM0 CONFIRMED); `compatible`
`pciclass,03xxxx` é construído da classe (visto vivo na AMD); com FF0000 nenhuma
personality display casa, mas o nó IOPCIDevice publica normalmente (como o card
reader ff00). GPUWrangler/AGDP/IOGraphics engajam por classe 0x03 (prova BARE: só
registraram a 10de:2504 porque era 030000). Confiança: LIKELY (mecanismo documentado +
precedentes; sem prova sem boot).

## Bytes exatos (NÃO adivinhados — derivados do vivo + computados por ferramenta)

Vivo AMD: classe 0x030000 ↔ ioreg `<00000300>` (bytes `00 00 03 00`; byte[2] = base).
Logo FF0000 → bytes `00 00 FF 00` → base64 via `printf|base64` = `AAD/AA==`
(round-trip `openssl|od` verificado; sanidade `00 00 03 00`→`AAADAA==` confere o padrão).

```xml
<key>PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0)</key>
<dict>
	<key>class-code</key>
	<data>AAD/AA==</data>
</dict>
```

SOMENTE no caminho RTX. vendor/device/subsys/model intocados; áudio 228e intocado;
sem rename `#display` (Dortania renomeia para esconder; nós queremos visível).

## Arquivos (EFI/OC no pendrive MACOS PENDR) + SHA256

- vivo `config.plist` = `dd431bdc…` (SAUDÁVEL, intocado)
- `config.HEALTHY-20260905-233655` = `dd431bdc…` (backup do atual)
- `config.plist.BARE-FAILED` = `3675d8f4…` (base, preservado)
- `config.BARE-CLASS0-20260905-233655` = `e7631b42…` (NOVO; diff vs BARE-FAILED = só o
  bloco acima; `plutil -lint` OK; `ocvalidate` NOT_AVAILABLE — Tools/ vazio)
- AMD byte-identical (NootedRed/layout-id/built-in/wegnoigpu + 1 único class-code no
  arquivo, o da RTX). ACPI/Kexts/SMBIOS/framebuffer/BARs/ReBAR intocados.

## Ativação manual (usuário) + rollback

```sh
# ATIVAR (no momento do reboot):
cp "/Volumes/MACOS PENDR/EFI/OC/config.BARE-CLASS0-20260905-233655" "/Volumes/MACOS PENDR/EFI/OC/config.plist"
# ROLLBACK (qualquer origem que leia MSDOS):
cp "/Volumes/MACOS PENDR/EFI/OC/config.HEALTHY-20260905-233655" "/Volumes/MACOS PENDR/EFI/OC/config.plist"
shasum -a 256 "/Volumes/MACOS PENDR/EFI/OC/config.plist"  # dd431bdc… = saudável de volta
```

## Previsões

- A (boot ok, RTX classe ff00, desktop AMD): H2→CONFIRMED/VERY_HIGH + lab PCI utilizável.
- B (trava igual): H2 perde força; revisitar H1/H4/H5. Não repetir boots.
- C (trava diferente): preservar logs + post-mortem.
Risco: baixo-médio — mesma janela do BARE anterior (freeze userspace), AMD isolada por
construção; rollback de 1 `cp`.

## Pós-boot (só leitura, sem TinyGPU)

RTX_PRESENT / RTX_CLASS_REPORTED (esperado ff00/`00 00 ff 00`) / RTX_DRIVER (esperado
NONE) / BUILT_IN / TUNNELLED / BARS (BAR0 16M/BAR1 16G/BAR3 32M via `ioreg -l`
`IODeviceMemory`) / visibilidade GPUWrangler (`(reg) gpu … 10de.2504` deve AUSENTAR) e
IOGraphics (sem GFX0/fb na RTX) / match estático TinyGPU (classe ff00 NÃO casa
`IOPCIClassMatch` — documentar como barreira conhecida do lab futuro).

## O que observar visualmente

Verbose no monitor AMD até login (esperado A) vs congela em texto (B/C — FOTOGRAFAR a
última linha desta vez); splash da placa-mãe no outro monitor deve continuar ignorado;
anotar minuto do freeze e qualquer tela de panic (improvável pelo histórico).
