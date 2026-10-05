# TGM0-macOS — entregas 2026-09-05 (Tahoe 25G83) + pendências por FS read-only

## O que foi executado (somente leitura, nada alterado)

- Runbook `docs/macos/hardware/ga106-ioreg-runbook.md` §§0–4: `ioreg`, `system_profiler`,
  `systemextensionsctl list` (leitura), `kextstat` (leitura), `log show` (leitura), `nvram -p` (leitura).
- Nenhum TinyGPU instalado/ativado, nenhum KEXT carregado, nenhum NVRAM/OpenCore/SIP/AMFI tocado.
- Pré-condição PASS: desktop AMD `1002:1638` como display principal.

## Achado central

- RTX `10de:2504` = **HIDDEN** (não BARE): `log` mostra `Found 0x10de:0x2504` + `0x10de:0x228e`
  → `bridge 0:1:1 removing child` ambos. `ioreg` vivo tem 0 nós `10de`, `SPPCI` só AMD,
  `GPP0@1,1` com filho vazio `D004@ff`. Causa: `boot-args -wegnoegpu` (literal com `e`).
- Logo A/B/C de `internal-pcie-gap.md` seguem **todos em aberto** (intestáveis sem nó BARE).
- `match-check`: HIDDEN→INDETERMINATE, AMD→LIKELY_NO_MATCH (sustenta RISK), BARE-hipotético→LIKELY_MATCH.
- `MAC-COMPUTE-0` permanece `LOW` (com bloqueador H0 explícito, não BLOCKED).

## Arquivos nesta pasta (fora do repo, repo é NTFS read-only)

- `artifacts-macos/`: 3 snapshots JSON + 4 saídas match-check + `log-pci-essencial.txt` + sysprof/ioreg sanitizados.
- `ga106-ioreg.md`: **draft pronto para `docs/macos/hardware/ga106-ioreg.md`** (copiar quando gravável).
- `internal-pcie-gap-addendum-20260905.md`: **addendum pronto para anexar a `docs/tinygpu/internal-pcie-gap.md`**.
- Crus completos (com PII, NÃO commitar): `/tmp/macos-tgm0/` (5.5 MB ioreg + logs).

## Por que não commitei (3 bloqueadores, nenhum por escolha)

1. `/Volumes/Downloads` = `ntfs, read-only` (`mkdir`/`touch` → `Read-only file system`).
   `artifacts/macos/` não pode nascer; `docs/` não pode ser escrito; `git commit` impossível.
2. `git` quebrado sem CLT (`xcode-select: No developer tools found`; `status`/`log` sem saída).
3. `python3` (`/usr/bin/python3`) exige CLT ausente → port Node fiel em `/tmp/macos-tgm0/match-check.js`
   (`--self-test ALL PASS`); `python3 scripts/tinygpu-match-check.py` original NÃO foi executado.

## Para finalizar quando o repo estiver gravável (Linux ou NTFS rw + CLT)

```sh
cp ~/Documents/nvidia-macos-TGM0/artifacts-macos/ga106-ioreg-*-SANITIZED-FIELDS.txt artifacts/macos/  # renomear p/ .txt datado
cp ~/Documents/nvidia-macos-TGM0/artifacts-macos/ga106-sysprof-*-SANITIZED.txt artifacts/macos/
cp ~/Documents/nvidia-macos-TGM0/ga106-ioreg.md docs/macos/hardware/ga106-ioreg.md
cat ~/Documents/nvidia-macos-TGM0/internal-pcie-gap-addendum-20260905.md >> docs/tinygpu/internal-pcie-gap.md
python3 scripts/tinygpu-match-check.py <snapshot>.json  # revalidar com o original
git status --short  # deve NÃO listar artifacts/macos/
git add docs/macos/hardware/ga106-ioreg.md docs/tinygpu/internal-pcie-gap.md
git commit -m "docs(macos): ga106 ioreg Tahoe 25G83 — RTX HIDDEN por -wegnoegpu, A/B/C em aberto"
```

## Próximo milestone (UM, sem implementar driver)

Tornar a RTX BARE sem perder o boot (ler `EFI/OC/config.plist` read-only → plano descartável
sem `-wegnoegpu` com recovery). Sem isso, nenhum teste TinyGPU/NVDev/MMIO é possível.
Detalhes em `ga106-ioreg.md` §7.
