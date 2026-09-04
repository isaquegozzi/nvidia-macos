# TinyGPU — arquitetura do binário (TG0, análise estática em Linux)

> Fase TG0. SOMENTE análise estática em Linux. Nenhum hardware tocado, nenhum driver instalado, nada executado.
> Zip pinado `c0d024f9` baixado SOMENTE para análise e extraído em `/tmp` (`/tmp/tg0-tinygpu-extract`), cache em `/tmp/opencode/tg0-tinygpu`. NADA copiado para o repo (só `.md`).
> Ferramentas Linux usadas: `unzip`, `file`, `llvm-objdump`, `strings`, `python3 plistlib`. O que exigir macOS está marcado `REQUIRES_MACOS_INSPECTION`.

## 0. Artefato e hashes

| Artefato | sha256 | bytes |
|---|---|---|
| `TinyGPU.zip` (`tinygpu_releases@c0d024f9`, `1634625` bytes) | `0c47285e2232643210555cf30ce08289b9e55da261c300e0c82e8448a359a21f` | `1634625` |
| `TinyGPU.app/Contents/MacOS/TinyGPU` | `3ed8bbd9ec8e14e7cf0047fbe7fb6169242394e3e5e75f5428d3edcb70254409` | `328000` |
| `.../org.tinygrad.tinygpu.driver2.dext/org.tinygrad.tinygpu.driver2` | `236035427b9b182ad5f9eb3c16d4a3e5804f84bb864a933d6c7aa8e9c6f3f198` | `230976` |

Clones: `tinygrad@e8c8ba1c7751142f7b20d85692afd2da5cef5e84`, `tinygpu_releases@c0d024f9ff0e1dc8fdf217f255da7101d91e8323` (ver `support-matrix.md` §1).
Download direto da URL pinada `https://github.com/tinygrad/tinygpu_releases/raw/c0d024f9.../TinyGPU.zip` confere o mesmo sha256 do clone.

## 1. Estrutura do `.app` (extração Linux `unzip -o -q ... -d /tmp/tg0-tinygpu-extract`)

```
TinyGPU.app/Contents/
  PkgInfo                          ("APPL????")
  Info.plist                       (XML, app installer — §2)
  embedded.provisionprofile        (CMS+plist, 12804 B — §5)
  CodeResources                    (opaco CMS/ticket, NÃO é plist — ver §6)
  _CodeSignature/CodeResources     (plist XML com hashes — §6)
  MacOS/TinyGPU                    (Mach-O universal, 328000 B — §4)
  Resources/Assets.car, tiny_icon.icns
  Library/SystemExtensions/org.tinygrad.tinygpu.driver2.dext/
    Info.plist                     (BINÁRIO bplist, DriverKit — §3)
    org.tinygrad.tinygpu.driver2   (Mach-O universal, 230976 B — §4)
    embedded.provisionprofile      (CMS+plist, 13000 B — §5)
    _CodeSignature/CodeResources   (plist XML — §6)
```

`__MACOSX/` no zip é AppleDouble (`._Info.plist`, `._PkgInfo`, etc.) — metadado, ignorar.
O `.dext` NÃO tem `Resources/`, `MacOS/` ou `PkgInfo`; é bundle DriverKit mínimo (`Info.plist` + executável + provision + assinatura).

## 2. `Info.plist` do app (XML, legível em Linux)

`TinyGPU.app/Contents/Info.plist`: `CFBundleIdentifier=org.tinygrad.tinygpu.installer`, `CFBundleExecutable=TinyGPU`, `CFBundleName=TinyGPU`, `CFBundlePackageType=APPL`, `CFBundleShortVersionString=1.0.0`, `CFBundleVersion=1`, `LSApplicationCategoryType=public.app-category.utilities`, `LSMinimumSystemVersion=12.1`, `DTPlatformName=macosx`, `DTSDKName=macosx26.2`, `DTPlatformVersion=26.2`, `DTPlatformBuild=25C57`, `DTSDKBuild=25C57`, `DTCompiler=com.apple.compilers.llvm.clang.1_0`, `DTXcode=2620`, `DTXcodeBuild=17C52`, `BuildMachineOSBuild=25D2128`.
NENHUMA chave `IOPCI*` no app — matching PCI vive só no dext (§3). Consistente com `docs/tinygpu.md:29-31` (app só pede ativação).

## 3. `Info.plist` do dext (bplist — decodificado via `plistlib`) — MATCHING

```python
IOKitPersonalities = {"TinyGPUDriver": {
  "CFBundleIdentifier": "org.tinygrad.tinygpu.driver2",
  "IOClass": "IOUserService",
  "IOMatchCategory": "TinyGPUDriver",
  "IOPCIClassMatch": "0x03000000",          # <-- único critério PCI no plist
  "IOPCITunnelCompatible": True,            # <-- [SUPERADO "thunderbolt-only no matching" — ver §9 Correção TGM0: declara suporte Thunderbolt, não prova exclusão]
  "IOProviderClass": "IOPCIDevice",
  "IOResourceMatch": "IOKit",
  "IOUserClass": "TinyGPUDriver",
  "IOUserServerName": "org.tinygrad.tinygpu.Driver",
  "TinyGPUDriverUserClientProperties": {"IOClass": "IOUserUserClient", "IOUserClass": "TinyGPUDriverUserClient"}}}
CFBundlePackageType=DEXT, CFBundleVersion=3, CFBundleShortVersionString=1.0.0,
OSBundleUsageDescription="TinyGPU Driver", OSMMinimumDriverKitVersion=21.0,
DTPlatformName=driverkit, DTSDKName=driverkit25.2, DTPlatformVersion=25.2
```

Idêntico ao source `extra/usbgpu/tbgpu/installer/TinyGPUDriverExtension/Info.plist:5-35@e8c8ba1c`.

**Veredito matching (resposta direta):**
- Classe: `IOPCIClassMatch=0x03000000` (display controller, `0x03____`; máscara exata sem `&` → match de classe inteira `0x030000`). `base_class=0x03` do `PCIIface` (`ops_nv.py`) é consistente.
- IDs: **NENHUM `IOPCIMatch` / `IOPCIPrimaryMatch` / `IOPCISecondaryMatch` no `Info.plist`** (ausência verificada por decode completo). Filtro por vendor vive no **entitlement** (§5), não no plist.
- Thunderbolt-only? [SUPERADO o "SIM no nível de matching" — ver §9 Correção TGM0] Leitura corrigida: `IOPCITunnelCompatible=True` + `IOProviderClass=IOPCIDevice` prova (como fato, `CONFIRMED`) que a personality **declara suportar Thunderbolt**; NÃO prova que internas seriam excluídas ("Sem essa chave, internas casariam pela classe; com ela, só tunneled" superado — a recíproca não é documentada pela Apple e não há checagem de `IOPCITunnelled` no driver/server). Detalhe e gap em `why-usb4.md` / `internal-pcie-gap.md` (ambos com Correção TGM0).
- Outros: `IOMatchCategory=TinyGPUDriver` (arbitragem), `IOUserServerName=org.tinygrad.tinygpu.Driver`, UserClient `TinyGPUDriverUserClient` via `IOUserUserClient`.

## 4. Identifiers, archs, plataformas

| Item | App | Dext |
|---|---|---|
| `CFBundleIdentifier` | `org.tinygrad.tinygpu.installer` | `org.tinygrad.tinygpu.driver2` |
| Executável | `Contents/MacOS/TinyGPU` | `org.tinygrad.tinygpu.driver2` (nome = bundle ID, exigência DriverKit) |
| Serviço IOKit | — (abre `"tinygpu"` via `IOServiceNameMatching("tinygpu")`, `server.c:98`) | nome de serviço `tinygpu` (`SetName("tinygpu")`, `TinyGPUDriver.cpp:62`), `IOUserServerName org.tinygrad.tinygpu.Driver` |
| UserClient | — (usa `IOServiceOpen`) | `TinyGPUDriverUserClient` (`IOUserClass`), selectors `TinyGPURPC {ReadCfg=0,WriteCfg=1,Reset=2,PrepareDMA=3}` (`TinyGPUDriverUserClient.iig:6-12@e8c8ba1c`) |
| Team | `9YG3G8543N` (`tinygrad, Corp.`) ambos (provision + codesig `subject.OU`) | idem |
| Archs (`llvm-objdump --macho --universal-headers`) | universal 2 slices: `x86_64` (`off 16384,size 137248`) + `arm64` (`off 163840,size 164160`) | universal 2 slices: `x86_64` (`off 16384,size 100416`) + `arm64` (`off 131072,size 99904`) |
| `file` | `Mach-O universal binary with 2 architectures: [x86_64: Mach-O 64-bit x86_64 executable] [arm64: Mach-O 64-bit arm64 executable]` | idem (ambos `PIE,NOUNDEFS,DYLDLINK,TWOLEVEL,BINDS_TO_WEAK`) |
| `LC_UUID` | `4B4BDECD-0EA0-3B7B-A25B-AD1263E29CD2` | `7418361E-3D37-39C2-9B4D-5970B475465C` |
| `LC_BUILD_VERSION` | `platform macos, sdk 26.2, minos 12.1, tool ld 1230.1` | `platform driverkit, sdk 25.2, minos 21.0, tool ld 1230.1` |
| `LC_CODE_SIGNATURE` | `dataoff 116944, datasize 20304` | `dataoff 80272, datasize 20144` |

## 5. Entitlements extraíveis em Linux (sem macOS)

Duas fontes independentes, ambas decodificadas em Linux e **convergentes**:

(a) Blob de assinatura embutido no Mach-O (`LC_CODE_SIGNATURE`, parse do offset acima, plist XML dentro do SuperBlob):
- App: `com.apple.security.app-sandbox=false`, `com.apple.security.files.user-selected.read-only=true`, `com.apple.developer.system-extension.install=true`, `com.apple.developer.driverkit.userclient-access=[org.tinygrad.tinygpu.driver2]`. (`subject.OU=9YG3G8543N`, `org.tinygrad.tinygpu.installer` no `identifier` do blob.)
- Dext: `com.apple.application-identifier=9YG3G8543N.org.tinygrad.tinygpu.driver2`, `com.apple.developer.driverkit=true`, `com.apple.developer.driverkit.transport.pci=[{IOPCIPrimaryMatch: 0x000010de&0x0000FFFF}, {IOPCIPrimaryMatch: 0x00001002&0x0000FFFF}]`.

(b) `embedded.provisionprofile` (CMS, plist após os primeiros 62 bytes; `CreationDate 2026-03-31T07:0xZ`, `Expiration 2044-03-26`, `TeamName tinygrad, Corp.`):
- App (`Name installer_release_0431`, `UUID 433b80a7-25d5-44ad-96a3-857c0b6bfe90`): `application-identifier 9YG3G8543N.org.tinygrad.tinygpu.installer`, `system-extension.install true`, `driverkit.userclient-access [org.tinygrad.tinygpu.driver2]`, `team-identifier 9YG3G8543N`, `keychain-access-groups [9YG3G8543N.*]`.
- Dext (`Name driver_release_0431`, `UUID 80f84d4e-6ba4-4260-a9c1-a4a4e25e107d`): `application-identifier 9YG3G8543N.org.tinygrad.tinygpu.driver2`, `driverkit true`, `transport.pci` com os **mesmos dois `IOPCIPrimaryMatch`** acima, `team-identifier`, `keychain-access-groups`.

Leitura `IOPCIPrimaryMatch`: formato `0xDDDDVVVV & mask`; `0x000010de&0x0000FFFF` = vendor `10de` (NVIDIA) qualquer device; `0x00001002&0x0000FFFF` = vendor `1002` (AMD) qualquer device. É allowlist por vendor, não por SKU (logo `10de:2504` passa no entitlement; ver GA106 em `support-matrix.md` §5).
Variantes no source NÃO shipped: `TinyGPUDriver.Release.entitlements` (= shipped, ambos vendors), `TinyGPUDriver.NV.Release.entitlements` (só `10de`), `TinyGPUDriver.entitlements` (wildcard `0xFFFFFFFF&0x00000000` dev), `TinyGPUDriver.NoSIP.entitlements` (wildcard + `allow-any-userclient-access`), `macOS.entitlements` (app sandbox false + install + userclient-access) — todas `@e8c8ba1c`.

## 6. Signing metadata (extraível) e `CodeResources`

- Cert: `Developer ID Application: tinygrad, Corp. (9YG3G8543N)` (`DeveloperCertificates` no provision, `subject.OU` no blob). Cadeia `Apple Root CA - G3 → Apple Certification Authority → Developer ID Certification Authority G2` (blobs no provision, datas `2026-01-18→2031-01-19`).
- `Contents/_CodeSignature/CodeResources` (app, plist XML): hashes `files` (`Assets.car`, `icns`) + `files2` hash2 de cada item (`dext Info.plist/binary/provision`, `Assets`, `provision` app). `Contents/CodeResources` (app root) é **opaco CMS** (ticket, não plist) — decode completo `REQUIRES_MACOS_INSPECTION`.
- `dext/_CodeSignature/CodeResources` (plist XML): `files Info.plist/provision` + `files2` hash2 do provision.
- Build provenance (strings, não metadado assinado): `/Users/nimelehin/Develop/ML/tinygrad/extra/usbgpu/tbgpu/installer/...` + objetos `TinyGPUDriver-*.o`, `TinyGPUDriverUserClient-*.o`, `TinyGPUDriver.iig.cpp` para `x86_64` e `arm64` — confirma build Xcode local, não reproduzível aqui.

## 7. Strings / frameworks / class names / user-client names (Linux `strings -a`, `llvm-objdump --macho --dylibs-used`)

App (`2898` strings; Swift+ObjC): dylibs `CoreAudio, IOKit, Foundation, libobjc, libSystem, SwiftUI, SystemExtensions, libc++, libswift* (Core,Foundation,Dispatch,IOKit,XPC,Metal,OSLog,ObjectiveC,QuartzCore...)`.
Símbolos Swift `TtC7TinyGPU11AppDelegate`, `TinyGPUApp`, `TinyGPUCLIRunner/TinyGPUCLIExit`, `OSSystemExtensionRequestDelegate` (`requestNeedsUserApproval/didFinishWithResult/didFailWithError/actionForReplacingExtension`), `systemextensionsctl`, CLI `install/uninstall/server <path>/status/help`, mensagens `Installing/Uninstalling TinyGPU driver extension...`, `Move TinyGPU to /Applications first.`, `TinyGPU - Remote PCI Device Server`, `failed to connect to tinygpu driver`, `If previously disabled: System Settings > General > Login Items & Extensions > Driver Extensions > Toggle TinyGPU ON`, `Missing entitlements. Rebuild with proper signing.`, socket `tinygpu` + `/tinygpu_%d` (servidor). **Zero** ocorrências de `ASM/2464/Thunderbolt/USB4/tunnel` no texto do binário app — tunnel vive só no plist (§3).

Dext (`944` strings; C++/DriverKit): dylibs SOMENTE `AudioDriverKit, DriverKit, PCIDriverKit, libc++` (prova de dext mínimo).
Classes: `TinyGPUDriver`, `TinyGPUDriverUserClient`, `TinyGPUDriverMetaClass*`, `IOUserService/IOUserClient/IOUserUserClient`, `IOPCIDevice`, `IOMemoryDescriptor/Map/DMACommand/BufferMemoryDescriptor`, `IOUserClientMethodArguments/Dispatch`.
Métodos: `Start_Impl/Stop_Impl/NewUserClient_Impl/MapBar/CfgRead/CfgWrite/ResetDevice/SetupDMA/CreateDMA/GetPCI`, `ExternalMethod/CopyClientMemoryForType_Impl/ensureDMACap`, `GetBARInfo/ConfigurationRead8-16-32/Write8-16-32/_CopyDeviceMemoryWithIndex/Open/Close/Reset`, `PrepareForDMA(maxAddressBits=40)/CreateMapping/GetAddress/GetLength`.
`os_log`: `tinygpu: init/on gpu detected/Open() failed/opened device ven=... dev=.../will register service/service started/failed to create NewUserClient/NewUserClient created/bar mapping %d idx=%d/rpc (%llu) in:%d,out:%d/read cfg off:%x sz:%d val:%x/wr cfg/reset/PrepareDMA .../CreateDMA size=.../cannot grow dma array/output map failed/PrepareDMA requires buffers >= 4097 bytes/...`.
Entitlement keys embutidas como strings: `com.apple.developer.driverkit[.transport.pci]`, `IOPCIPrimaryMatch`, `9YG3G8543N.org.tinygrad.tinygpu.driver2`.
**Zero** `ASM/2464/USB4/Thunderbolt` no dext — veredito `why-usb4` hipótese E.

## 8. `REQUIRES_MACOS_INSPECTION` (tudo que Linux NÃO pode afirmar)

- `codesign -dvv --deep --strict`, `codesign -d --entitlements -`, `spctl -a -vv`, `stapler validate`, CDHash vs `CodeResources` hash2, cadeia/notarização ao vivo.
- Estado vivo: `systemextensionsctl list` (`[activated enabled]` vs `waiting for user`), `ioreg -l` no `IOPCIDevice` (`IOPCITunnelled`, `built-in`, `class-code`, `IOServiceDEXTEntitlements`, `AAPL,slot-name`), `log show --predicate tinygpu`.
- Comportamento vivo: prompt `would like to use a new driver extension`, mapeamento BAR real (size/idx), `PrepareDMA` IOVA/paddrs reais, `ExternalMethod` selectors ao vivo, tráfego `tinygpu.sock` real, `CopyClientMemoryForType` types reais.
- Desassembly além de strings (Hopper/lldb, seletores, `CreateDMA` vs `PrepareDMA` paths) e execução dos Mach-O (não executáveis em Linux).
- Datas de build vs assinatura (XB/DT* são claims do plist, não prova sem `codesign`).

## 9. Correção TGM0 (2026-09-04) — semântica `IOPCITunnelCompatible`

Bug TG0: §3 leu `IOPCITunnelCompatible=True` como prova de "Thunderbolt-only no matching". Correção perante a documentação Apple atual (fonte canônica):

- A chave indica que o driver **suporta Thunderbolt** ("To indicate that your PCIe driver supports Thunderbolt, include the `IOPCITunnelCompatible` key [...]" — `developer.apple.com/documentation/pcidriverkit/creating-custom-pcie-drivers-for-thunderbolt-devices`); é opt-in que protege o caminho tunneled contra drivers antigos ("[...] the `IOPCIFamily` will not load the drivers for Thunderbolt connected PCI devices. This opt-in key protects consumers against older drivers [...]" — Thunderbolt Device Driver Programming Guide, arquivo Apple).
- A presença real de túnel é indicada por **`IOPCITunnelled` no IORegistry** (propriedade do dispositivo/caminho, verificável via `getProperty(kIOPCITunnelledKey, ..., kIORegistryIterateRecursively | kIORegistryIterateParents)` ou `ioreg` — mesmo guia Apple).

O que permanece `CONFIRMED` neste doc: o fato decodificado (plist do dext = source `Info.plist:5-35@e8c8ba1c`: `IOPCIClassMatch=0x03000000`, `IOPCITunnelCompatible=True`, sem `IOPCI*Match` por ID) e a ausência de `ASM/2464/USB4/Thunderbolt` nas strings do dext (§7 — que adicionalmente implica: nenhuma evidência TG0 de checagem de `IOPCITunnelled` no driver). O que passa a `UNKNOWN`: que a flag excluiria GPUs internas não-tunneled do matching. Numeração das seções anteriores preservada (citações §3/§5/§7 de outros docs continuam válidas como localização do fato, não da conclusão superada).
