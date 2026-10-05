> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# TG-KEXT1-ARCH — IOUserClient mínimo read-only (ESPECIFICAÇÃO, sem código)

> DESENHO apenas (2026-09-07, NDRV0+REV1 saudáveis). Sem código/build/EFI/reboot.
> Meta KEXT1: provar transporte userspace↔kernel read-only, não controle da GPU.
> Destino final: `docs/macos/TG-KEXT1-ARCH.md`.

## 1–2. Princípio e arquitetura

```text
ga106ctl → IOServiceGetMatchingService → IOServiceOpen → IOConnectCallMethod
→ GA106LabUserClient:IOUserClient → externalMethod(dispatch table)
→ lê CACHE do GA106Lab → struct versionada. IOPCIDevice nunca tocado pelo UC.
```

## 3. APIs Tahoe confirmadas (`TAHOE_IOUSERCLIENT_API_STATUS = CONFIRMED`)

Kernel (`Kernel.framework/Headers`, Xcode 26.5 SDK): `IOService::newUserClient`
(overloads clássicos presentes), `IOUserClient::initWithTask/clientClose/clientDied/
externalMethod(selector,args,dispatch,target,ref)`, `IOExternalMethodDispatch`
clássica `{function, checkScalarInputCount, checkStructureInputSize,
checkScalarOutputCount, checkStructureOutputSize}` (+ variante 2022 com
`allowAsync/checkEntitlement` — NÃO necessária), `clientHasPrivilege(token,
kIOClientPrivilegeAdministrator="root")` (estático, público).
Userspace (`IOKit.framework/Headers/IOKitLib.h`): `IOServiceOpen`,
`IOServiceGetMatchingService`, `IOConnectCallMethod`, `IOServiceClose`.

## 4–8. Protocolo v1 + ABI (x86_64 explícito, static_assert nos impl. futuros)

`GA106LAB_UC_PROTOCOL_VERSION = 1`. Selectors: `0 GET_PROTOCOL_VERSION`
(saída 2 scalars: protocol=1, driver=0x00010100), `1 GET_DEVICE_IDENTITY`,
`2 GET_SERVICE_STATE` (saída struct, entrada ZERO ambas).

```c
struct GA106LabIdentityV1 { // sizeof 32, align 8, sem padding oculto
    uint32_t size;             // @0  = 32
    uint32_t version;          // @4  = 1
    uint32_t vendor;           // @8  (0x10de)
    uint32_t device;           // @12 (0x2504)
    uint64_t providerEntryID;  // @16
    uint32_t state;            // @24 (1 = START_SUCCESS)
    uint32_t reserved;         // @28 = 0
};
struct GA106LabStatusV1 { // sizeof 32
    uint32_t size;             // 32
    uint32_t version;          // 1
    uint32_t state;            // 1 = START_SUCCESS
    uint32_t providerConfirmed;// 1
    uint32_t revision;         // 0x00010001 (1.0.1)
    uint32_t reserved[3];      // 0
};
```

## 9–10. Sem ponteiros kernel + acesso root-only

Retorno: só escalares/structs acima (entryID u64 é identificador opaco, não endereço;
sem `IOService*`/BAR/mapped addresses). `USERCLIENT_ACCESS_POLICY = A root-only`:
`newUserClient` nega sem `clientHasPrivilege(securityToken, "root")` — API
pública/estável, sem entitlement privado (rejeitadas: B local-admin, C entitlement
custom, D irrestrito).

## 11–12. Dispatch table clássica + input zero

`IOExternalMethodDispatch[3]`, `checkScalarInputCount=0`,
`checkStructureInputSize=0`, outputs validados por tamanho; sem override custom de
`externalMethod` (menos ambiguidade). `USER_CONTROLLED_KERNEL_INPUT = MINIMAL`
(só o selector). Outputs zerados antes de preencher, tamanho fixo, cópia pelo
mecanismo padrão; sem variable-length/user-pointers/shm/descriptors.

## 13–17. Lifecycle, ownership, concorrência, memória

`newUserClient` (checa root) → `initWithTask` (guarda owner) → `start`
(`fProvider = GA106Lab via getService + retain`; SEM `IOPCIDevice*` no UC →
`USERCLIENT_DIRECT_PCI_PROVIDER_REFERENCE = NO`) → `clientClose/clientDied/stop/free`
(release; fechar `ga106ctl` não toca GPU/provider). Cache (`vendor/device/entryID/
revision/state`) preenchido UMA vez no `start()` do GA106Lab; UC só lê (sem polling).
Concorrência: UM cliente por vez (`kIOReturnExclusiveAccess` no 2º) — simplifica
lifetime, sem workloop/gate. Mitigações: owner retido (sem UAF/close-race), tamanhos
checados pela dispatch table (sem mismatch), saídas zeradas (sem padding vazado),
`OSDynamicCast` + null (sem null-owner), selector fora da tabela → `kIOReturnBadArgument`
(sem wrong-selector), sem truncamento (u64 nativo).

## 18–19. ga106ctl (CLI direta, sem daemon)

`ga106ctl version|identity|status` → `protocol: 1, driver: TG-KEXT1,
vendor: 10de, device: 2504, providerEntryID: …, state: START_SUCCESS`.
Matching userspace por `IOServiceMatching("GA106Lab")`.

## 20–24. Critérios, proibições, fases, versão, plist

PASS futuro (`MAC_COMPUTE_USERCLIENT_0`): boot+provider+AMD ok; 3 comandos SUCCESS
com valores correlacionados ao registry; close/reopen ok; sem panic/regressão/atividade
PCI-MMIO-DMA-GSP. PROIBIDAS (ARCH_REJECT se aparecer): open/close/configRead*/Write*,
map*/getDeviceMemory*, MSE/BME, IOMemoryDescriptor-para-BAR, IOMap, DMA, interrupts/
MSI, workloop-p/ HW, firmware/GSP, `clientMemoryForType`/`IOConnectMapMemory`/rings.
Sem shm. Sem daemon/RPC TinyGPU (futuro: tinygrad→daemon→UC→GA106Lab).
Sequência: KEXT1-A transporte → KEXT1-B validação → KEXT2 cfg-read-only →
KEXT3 BAR discovery → KEXT4 MMIO controlado (KEXT2+ sem detalhe).
Versão `1.1.0`, mesmo bundle id; Info.plist SEM personality nova (UC nasce de
`newUserClient`, não de matching) e SEM tocar o matching PCI.

```text
TG_KEXT1_ARCH_READY = YES
```

UM próximo passo (sem executar): instanciar `GA106LabUserClient.{hpp,cpp}` +
`ga106ctl.c` + bump `1.1.0` exatamente por esta spec, em árvore nova
(`GA106Lab-kext1-src/`), PASS/REV1 intocados.
