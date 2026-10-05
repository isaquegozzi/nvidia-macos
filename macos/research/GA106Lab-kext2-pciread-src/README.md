# GA106Lab.kext KEXT2 (1.2.0) — PCI config read-only, ATTACH_ONLY

Long-term project objective: macOS graphics acceleration via Metal on GA106
(PCI config read → resource discovery → BAR mapping futuro → controlled MMIO →
GPU bring-up → memory/queues → rendering → IOGPU/IOAccelerator → MTLDevice).
KEXT2 is a transport foundation, not a compute-specific interface (protocolo v1
neutro + selector 3 snapshot; sem APIs tinygrad-specific).

Árvore nova (`GA106Lab-kext2-pciread-src/`); 1.0.0/1.0.1/1.1.0 intactas. Staging
fora do repo (NTFS read-only).

Long-term project objective: macOS graphics acceleration via Metal on GA106.
KEXT1 is a transport foundation, not a compute-specific interface (protocolo v1
neutro; sem APIs tinygrad-specific).

Árvore nova (`GA106Lab-kext1-fix-src/`); kext1 auditada intacta. Staging fora
do repo (NTFS read-only).

Long-term project objective: macOS graphics acceleration via Metal on GA106.
KEXT1 is a transport foundation, not a compute-specific interface (protocolo v1
neutro; sem APIs tinygrad-specific).

Árvore nova (`GA106Lab-kext1-src/`); PASS 1.0.0 e REV1 1.0.1 intactas. Staging fora
do repo (NTFS read-only): destino final `macos/GA106Lab/`.

Cópia de trabalho derivada de `../GA106Lab-src/` (artefato PASS 1.0.0 intacto lá).
Diferenças REV1: `CFBundleVersion/Short 1.0.1`; `start()` publica no PRÓPRIO serviço
`GA106LabState` (START_ENTER→PROVIDER_CAST_FAILED|START_SUCCESS), `GA106LabRevision`,
`GA106LabProviderVendor/Device` (hex), `GA106LabProviderEntryID` (OSNumber 64);
`os_log` REJEITADO (KEXT_OS_LOG_SUPPORT=UNKNOWN — header só documenta
`os_log_driverKit`; Lilu usa logger próprio; sem kpi.unsupported por telemetria);
IOLog mantido (inócuo). Matching/personality/NDRV0 inalterados.
Spec normativa: `../TG-KEXT0-spec.md`. Fase BUNDLE-FIX (sem instalar/carregar).

## Método de link (MANUAL_OFFICIAL_EQUIVALENT)

`ld -r` (MH_OBJECT) PROIBIDO como final — objeto relocável não carrega (auditoria
Astra confirmada). Equivalente ao target Kernel Extension do Xcode: compile
`-mkernel` + link `-Xlinker -kext` (MH_KEXT_BUNDLE) + `-lkmodc++ -lkmod -lcc_kext` +
decl `KMOD_EXPLICIT_DECL` no fonte (prova: NootedRed tem filetype KEXTBUNDLE +
`_kmod_info` local). Lifecycle de módulo (start/stop triviais KERN_SUCCESS) é só
infraestrutura de registro; lógica RTX vive SÓ em `GA106Lab::start(provider)`.

## Arquivos

- `Info.plist` — personality RTX-only (`0x250410de`), `IOProbeScore 0` explícito
  (default; documenta sem-prioridade), categoria `GA106Lab`, deps §4 da spec.
- `GA106Lab.hpp` — `GA106Lab : IOService`, `start`/`stop` apenas.
- `GA106Lab.cpp` — Start: super → cast → `copyProperty` (vendor/device u16-LE
  decodificado em CPU + class-code/built-in bytes crus + entryID + path
  `gIOServicePlane`) → `IOLog[GA106Lab]` → true. Sem probe(), sem Open/map/config/
  DMA/GSP. `UNAVAILABLE_WITH_ZERO_PCI_ACCESS` onde dado faltar (nunca PCI access).
- `build.sh` — build reproduzível com toolchain Xcode (sem assinatura/instalação).

## Auditoria estática (proibidas — verificar com grep antes de qualquer teste vivo)

open, close, configRead*, configWrite*, mapDeviceMemory*, setMemoryEnable,
setBusMasterEnable, reset, DMA, interrupt, BAR, GSP, firmware —
zero CHAMADAS próprias (símbolos de vtable herdada em `nm -u` são referências,
não chamadas — desassembly de `start()` mostra só super/cast/copyProperty/
getBytes/IOLog/entryID/path/release/snprintf). APIs usadas: `super::start/stop`,
`OSDynamicCast`, `copyProperty`, `getBytesNoCopy/getLength` (com null-check),
`getRegistryEntryID`, `getPath`, `IOLog`, `snprintf`, `OSSafeReleaseNULL`,
+ start/stop triviais de módulo (infra).
`OWN_CODE_HARDWARE_MUTATION_EXPECTED = NO`.
`FRAMEWORK_MATCHING_HARDWARE_MUTATION = UNLIKELY` (neste provider/estado: publish
limpo em 3 boots + `IOPCIResourced=Yes`; sem garantia para próximo boot — linguagem
exigida pela auditoria: não atribuir ao nosso código ação do framework).
