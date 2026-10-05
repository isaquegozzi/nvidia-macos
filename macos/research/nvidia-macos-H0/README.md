# H0 — Hidden GPU Root Cause + Reversible BARE Boot Plan (índice de entregas, 2026-09-05)

Fase H0 executada no macOS Tahoe 26.6.2 (25G83) MacPro7,1, SOMENTE LEITURA.
Nada alterado: EFI/config/NVRAM/SIP/AMFI/OC/kexts/ACPI intactos; sem reboot; sem TinyGPU.
Repo `nvidia-macos` em `/Volumes/Downloads` = NTFS read-only → drafts nesta pasta;
copiar para o repo quando gravável (caminhos de destino indicados por arquivo).

## Arquivos

- `ga106-bare-boot-plan.md` → destino `docs/macos/hardware/ga106-bare-boot-plan.md`
  (plano do UM boot BARE: objetivo, mudança mínima em 2 testes, aplicação manual,
  recovery sucesso/falha A/B/C, comandos read-only pós-boot com gabarito
  RTX_PRESENT/AMD_DESKTOP/RTX_DRIVER/BUILT_IN/TUNNELLED, fora de escopo).
- `typo-fix-wegnoegpu.patch.md` → aplicar diff em `docs/macos/TGM0-HANDOFF.md` §6 (3 linhas).
- `tgm0-update-hidden-note.md` → acrescentar a `docs/macos/hardware/ga106-ioreg.md` §2
  (HIDDEN != hardware unsupported).
- `artifacts-h0/` (evidências sanitizadas — sem serial/MLB/UUID/ROM/MAC):
  - `oc-config-plist-SANITIZED.txt` — `plutil -p config.plist` integral (986 linhas)
  - `log-pci-removal.txt` — 6 linhas IOPCIFamily (Found/Probing/removing child)
  - `lilu-wegnoegpu-strings.txt` — strings `-wegnoegpu` + `disable-gpu` no binário Lilu
  - `nvram-boot-args.txt`, `oc-version.txt` (`REL-108-2026-09-05`), `kmutil-loaded-relevant.txt`
- Crus com PII (NÃO commitar, NÃO copiar para docs): EFI lida in-place (desmontada após),
  `/tmp/macos-h0-config.txt` (com serial — apagar ao final da sessão).

## Para finalizar no repo gravável

```sh
cp ~/Documents/nvidia-macos-H0/ga106-bare-boot-plan.md docs/macos/hardware/ga106-bare-boot-plan.md
# aplicar typo-fix diff em docs/macos/TGM0-HANDOFF.md (3 linhas §6)
# acrescentar tgm0-update-hidden-note em docs/macos/hardware/ga106-ioreg.md §2
git add docs/macos/hardware/ga106-bare-boot-plan.md docs/macos/TGM0-HANDOFF.md docs/macos/hardware/ga106-ioreg.md
git commit -m "docs(macos): H0 root cause Lilu + plano BARE reversível; fix typo -wegnoegpu"
```

## Próximo milestone (UM, não executado)

Executar o plano BARE (TESTE 1: remover só `-wegnoegpu`; se ainda HIDDEN, TESTE 2:
`disable-gpu`), coletar `ga106-ioreg-bare.md`, e só então reavaliar A/B/C.
