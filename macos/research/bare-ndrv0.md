> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# BARE-NDRV0 — preparado, NÃO ativado, SEM reboot (2026-09-06)

> Bloqueia SOMENTE o `IONDRVFramebuffer` na RTX via gate da própria Apple, mantendo
> PCI visível com classe original 030000. Parte do BARE original (NÃO do CLASS0).
> Vivo segue SAUDÁVEL — ativação é `cp` manual. Destino final:
> `docs/macos/hardware/bare-ndrv0.md`. D-minimal/KEXT-minimal SUSPENSOS (§13).

## Gate Apple (auditado, não copiado de revisão)

- `apple-oss-distributions/IOGraphics` (branch `main`, cópias em
  `apple-IONDRVFramebuffer.cpp` / `apple-IONDRVSupport.h` nesta pasta):
  - `IONDRVSupport/IOKit/ndrvsupport/IONDRVSupport.h:33`:
    `#define kIONDRVIgnoreKey "AAPL,iokit-ignore-ndrv"`
  - `IONDRVSupport/IONDRVFramebuffer.cpp:248-257` (`IONDRVFramebuffer::probe`):
    `if (0 != provider->getProperty(kIONDRVIgnoreKey)) { …; return (NULL); }`
- `SOURCE_GATE_EXISTS = CONFIRMED`. `TAHOE_EXACT_IMPLEMENTATION = LIKELY`
  (não UNKNOWN: o framebuffer `IONDRVFramebuffer` + `.Display_boot` foi observado
  vivo no CLASS0, logo a classe existe no Tahoe; byte-identidade com o source aberto
  não afirmada — Apple não publica o Tahoe exato).
- Semântica: teste de PRESENÇA (`0 !=` ponteiro). Qualquer valor não-nulo desarma.

## Tipo/valor (não-palpite)

```text
PROPERTY_NAME  = AAPL,iokit-ignore-ndrv
PROPERTY_TYPE  = Boolean
PROPERTY_VALUE = true  (<true/>)
```

Justificativa: consumidor testa só presença — conteúdo irrelevante, mas deve ser
não-nulo (nunca zero/vazio/false). Boolean `true` = padrão Dortania para presence-gates
(`disable-gpu | Boolean | True`) + precedente funcional NESTA máquina (o `disable-gpu`
Boolean que o Lilu consumiu). Sem precedente público de tipo para esta chave
específica — registrado honestamente; Data `<01>` funcionaria igual, Boolean foi o
escolhido pela trilha local.

## Mudança (1 lógica, na RTX `PciRoot(0x0)/Pci(0x1,0x1)/Pci(0x0,0x0)`)

```xml
<dict>
	<key>AAPL,iokit-ignore-ndrv</key>
	<true/>
</dict>
```

Base `config.plist.BARE-FAILED` (boot-args sem wegn, RTX sem disable-gpu/class-code).
Nada em vendor/device/subsys/model/ACPI/AMD/NootedRed/SMBIOS/fb-AMD/áudio-228e/BARs.

## Arquivos + SHA256

- vivo `config.plist` = `dd431bdc…` (saudável, intocado)
- `config.plist.BARE-FAILED` = `3675d8f4…` (base)
- `config.plist.CLASS0-FAILED` = `e7631b42…` (histórico, preservado)
- `config.BARE-NDRV0-20260906-003232` = `a2e3d4d1…` (NOVO; diff = só o bloco acima;
  `plutil -lint` OK; `ocvalidate` NOT_AVAILABLE; AMD byte-identical; zero class-code)

## Ativação / rollback (usuário, manual)

```sh
cp EFI/OC/config.BARE-NDRV0-20260906-003232 EFI/OC/config.plist   # ATIVAR
cp EFI/OC/config.HEALTHY-20260905-233655 EFI/OC/config.plist     # ROLLBACK
# (se o HEALTHY timestampado não existir mais, o vivo dd431bdc… já é o saudável)
```

## Previsões

- A (boot ok; RTX 030000 sem IONDRVFramebuffer; AMD ok): `NDRV_PATH_CAUSALITY =
  STRONGLY_SUPPORTED` (CONFIRMED só após logs/runtime) + novo lab base.
- B (trava igual): verificar se a property chegou (`ioreg` no provider) e se o
  framebuffer sumiu — gate não-aplicado = INCONCLUSIVE; sumiu mas travou = NDRV não
  suficiente.
- C (framebuffer permanece): falha de intervenção, não evidência contra hipótese.

## Pós-boot (só leitura, sem TinyGPU, sem DEXT)

```sh
ioreg -l -w0 -c IOPCIDevice | grep -B3 -A25 -i "0x2504" | head -n 60
ioreg -l -w0 | grep -i -E "iokit-ignore-ndrv" | head -n 5
ioreg -w0 | grep -i -E "IONDRVFramebuffer|Display_boot" | head -n 20
/usr/bin/log show --last boot --style compact | grep -E "GPUWrangler.*(10de|1002)|displaypolicyd.*exited|DidReconfigure" | head -n 20
system_profiler SPDisplaysDataType -detailLevel mini | head -n 25
```

Gabarito: `RTX_PRESENT=YES / AAPL,iokit-ignore-ndrv=PRESENT(no provider) /
IONDRVFramebuffer_ON_RTX=NO / .Display_boot_ON_RTX=NO / GPUWrangler-RTX=? /
AMD ok?`. Mesmo funcionando: NADA de TinyGPU nesta execução.

## Tela a observar

Verbose AMD até login (A) vs congela (B/C — FOTOGRAFAR última linha) + anotar minuto;
splash da outra saída deve seguir ignorado; qualquer panic visual é improvável.
