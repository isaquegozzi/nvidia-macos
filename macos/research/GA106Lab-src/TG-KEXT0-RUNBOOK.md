# TG-KEXT0-RUNBOOK — teste vivo futuro (NÃO executar agora)

Pré-condição: NDRV0 estável + `GA106Lab.kext` revisado (fontes + bundle em
`GA106Lab-src/`, SHAs em `SHA256.txt`, build log preservado).

## Identidade da EFI experimental (fixo, verificado)

```text
EFI experimental : pendrive MACOS PENDR (/dev/disk6s1, MSDOS)
volume/partição  : disk6s1
OpenCore usado   : EFI/OC/OpenCore.efi (REL-108-2026-09-05 via NVRAM)
config NDRV0     : EFI/OC/config.plist
                   SHA256 dd431bdc7cebaee77bf45477d8c41135f472074259fcdf2991c91d60f4c9f74f
config futuro    : EFI/OC/config.GA106Lab-TEST-<timestamp> (cópia do NDRV0 + entry)
GA106Lab.kext    : bundle SHA256 registrado em SHA256.txt (NÃO copiar ainda)
```

Acesso por outro SO: JÁ PROVADO (usuário restaurou NDRV0 via outro SO em BARE e
CLASS0; arquivos `*.BARE-FAILED`/`*.CLASS0-FAILED` na EFI provam o fluxo).

## Instalação futura (manual, um passo por vez)

1. Copiar `GA106Lab.kext` → `EFI/OC/Kexts/` (outros discos/EFIs intocados).
2. Copiar NDRV0 → `config.GA106Lab-TEST-<ts>` e SÓ nele adicionar `Kernel→Add`
   (`GA106Lab.kext`/`Contents/MacOS/GA106Lab`/`Contents/Info.plist`, `Any`,
   `Enabled=true`); apontar o boot para esse config (ou renomear com backup).
   NDRV0 (`config.plist` + SHA acima) intacto como fallback imediato.
3. Reboot com `-v`, fotografar tela se parar.

## Medição (identificadores confiáveis — NÃO grep frágil `0x2504`)

```bash
log show --last boot --style compact | grep "\[GA106Lab\]"   # START_ENTER…START_SUCCESS
ioreg -l -w0 -c IOPCIDevice | grep -B4 -A10 "pci10de,2504"   # filho GA106Lab?
kextstat | grep -i ga106lab
# correlação: IORegistry path + registryEntryID do log vs nó pci10de,2504,
# provider parent do filho GA106Lab, system_profiler (AMD ok?)
```

Sucesso (`MAC_COMPUTE_KEXT_ATTACH_0`): boot + AMD + RTX + ignore-ndrv + sem fb +
loaded/START/provider-2504 + AMD/HDAU-NO + MMIO/PCI_CONFIG/DMA/GSP ZERO.
Falhas: NOT_LOADED / NO_MATCH / START_NOT_REACHED / WRONG_PROVIDER / START_FAILED /
PANIC / DESKTOP_REGRESSION / NDRV_REGRESSION.

## Rollback concreto por outro OS (sem depender do macOS alvo)

1. Iniciar Windows/Linux/recovery disponível.
2. Identificar o pendrive MACOS PENDR e montar a EFI (MSDOS).
3. Em `EFI/OC/`: apagar `config.GA106Lab-TEST-*` (ou `Enabled=false`) e confirmar
   `config.plist` = NDRV0 via `shasum -a 256` (`dd431bdc…` acima).
4. Opcional: remover `EFI/OC/Kexts/GA106Lab.kext/`.
5. Reiniciar pelo mesmo OpenCore (picker, mesmo volume Tahoe).
