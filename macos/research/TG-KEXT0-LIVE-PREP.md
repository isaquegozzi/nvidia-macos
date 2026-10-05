> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# TG-KEXT0-LIVE-PREP — plano do primeiro live test (DOCUMENTO, sem executar)

> 2026-09-06, NDRV0 saudável. Artefato auditado (BUNDLE-FIX). NADA instalado/
> copiado/editado/reiniciado. Destino final: `docs/macos/TG-KEXT0-LIVE-PREP.md`.

## 1. Artefato

```text
BUNDLE_PATH  = ~/Documents/nvidia-macos-BARE0/GA106Lab-src/build/obj/GA106Lab.kext
BINARY_PATH  = .../Contents/MacOS/GA106Lab
BUNDLE_SHA256= (dir, ver SHA256.txt)  BINARY_SHA256 = 9542e8c06320… (= registrado)
CFBundleIdentifier = org.nvidia-macos.GA106Lab / CFBundleExecutable = GA106Lab
KEXTBUNDLE x86_64, _kmod_info local, personality RTX-only. Sem rebuild nesta fase.
```

## 2. Variável única

`EXPERIMENT_VARIABLE = GA106Lab.kext presence + Kernel→Add enabled entry`.
Intocados: boot-args, DeviceProperties, ignore-ndrv, AMD/NootedRed, ACPI, SMBIOS,
BAR/ReBAR, HDAU, class-code.

## 3–4. Entry + ordem (só documento)

```xml
<dict>
  <key>Arch</key><string>Any</string>  <!-- x86_64-only aqui; Any = padrão OC usado pelos 12 atuais -->
  <key>BundlePath</key><string>GA106Lab.kext</string>
  <key>Comment</key><string>GA106Lab KEXT0 attach-only RTX 10de:2504</string>
  <key>Enabled</key><true/>
  <key>ExecutablePath</key><string>Contents/MacOS/GA106Lab</string>
  <key>MaxKernel</key><string></string>
  <key>MinKernel</key><string></string>
  <key>PlistPath</key><string>Contents/Info.plist</string>
</dict>
```

`KERNEL_ADD_ORDER = APPEND_AT_END (índice 12, após UTBDefault)`. Razão: sem
dependência de Lilu/NootedRed (sem API deles, categoria própria, matching por evento
no publish PCI); anexar no fim não reordena a sequência existente. Min/MaxKernel
vazios = correto (sem necessidade documentada; igual aos 12 atuais).

## 5–6. Deps + assinatura

`EXTERNAL_KEXT_DEPENDENCIES = NONE` (só kpi+iokit+IOPCIFamily; `nm` = símbolos kernel).
`SIGNING_STATE = UNSIGNED`. `OPENCORE_LOAD_EXPECTATION = LIKELY` (injeção OC não
verifica assinatura — mesmo caminho dos 12 kexts vivos). Distinção: policy runtime
`kextload` recusaria unsigned (irrelevante — via é OC); AuxKC não se aplica a injetado;
signing DriverKit irrelevante p/ KEXT.

## 7–10. Baseline, nomes, diff

```text
NDRV0_CONFIG_PATH = /Volumes/MACOS PENDR/EFI/OC/config.plist
NDRV0_CONFIG_SHA256 = dd431bdc7ceb…41135f472074259fcdf2991c91d60f4c9f74f
EFI_VOLUME = MACOS PENDR (disk6s1) / EFI_OC_PATH = EFI/OC
Futuro config = config.TG-KEXT0-EXPERIMENT-<ts> (NÃO criar agora)
Destino futuro = EFI/OC/Kexts/GA106Lab.kext (NÃO copiar; pós-cópia: 3 SHAs byte-identical)
Diff aceito: SÓ a entrada §3 (comparação semântica se plist reformatar).
```

## 11–12. PASS + falhas

PASS = boot AMD ok + RTX + ignore-ndrv runtime + sem fb/.Display_boot +
loaded/START_ENTER/PROVIDER_CONFIRMED/START_SUCCESS + provider 2504 (entryID/path) +
AMD/HDAU NO + MMIO/PCI_CONFIG/DMA/interrupts ZERO + sem panic/regressão.
Códigos: KEXT_INJECTION_FAILED / LINK_OR_DEPENDENCY_FAILED / PERSONALITY_NOT_MATCHED /
START_NOT_REACHED / START_FAILED / WRONG_PROVIDER_MATCHED / KERNEL_PANIC /
EARLY_REBOOT / DESKTOP_REGRESSION / NDRV_REGRESSION / PASS.

## 13–15. Logs + provas

`[GA106Lab]`, bundle id, `pci10de,2504` (NÃO `0x2504`), IOPCIDevice, IONDRVFramebuffer,
GPUWrangler, WindowServer, displaypolicyd, NootedRed, panic. Alvos: START_ENTER,
PROVIDER_CONFIRMED, START_SUCCESS + correlação provider.
`KEXT_LOADED_PROOF` = `kextstat|grep ga106lab` + filho no `ioreg` + linhas `[GA106Lab]`
(três fontes; nenhuma sozinha basta).
`PROVIDER_IDENTITY_PROOF` = `pci10de,2504` + path + entryID do log + parent provider
+ `[GA106Lab]` (quatro amarras).

## 16–17. Recovery + rollback

panic/freeze/reboot: NÃO repetir → outro OS → EFI correta (disk6s1) →
`config.plist` NDRV0 (`dd431bdc…`) OU entry `Enabled=false` → validar plist/hash →
mesmo OpenCore. Sem depender do macOS alvo.
`ROLLBACK_VARIABLES` = (1) entry Enabled=false/removida, (2) kext removido se
desejado. Sem tocar ignore-ndrv/NootedRed/Lilu/AMD/boot-args.

## 18. Riscos

`OWN_CODE_RISK = LOW` (só registry+log auditados; vtable≠chamadas).
`FRAMEWORK_MATCHING_RISK = LOW` (attach em categoria própria; publish limpo ×4 boots;
residual: arbitragem nunca é zero).
`RECOVERY_RISK = LOW` (fluxo provado 2×, EFI identificada, SHAs).
`OVERALL_LIVE_TEST_RISK = LOW` (read-only no nosso código ≠ zero absoluto: kernel é
anel privilegiado; pânico por bug próprio sempre possível — mitigado por minimalismo).

## 19. Gate

Entry ✓ / ordem ✓ / baseline SHA ✓ / destino ✓ / hash-verify ✓ / diff rule ✓ /
success-failure ✓ / logs ✓ / provider proof ✓ / rollback ✓ / recovery ✓ →
`LIVE_TEST_PLAN_READY = YES`.
