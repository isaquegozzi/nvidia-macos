# TG-KEXT2 runbook RETRY — MAC_GPU_PCI_READ_0_RETRY (futuro, NÃO executar)

Pré: NDRV0 + KEXT2-FIX 1.2.1 revisado (SHAs em fix `SHA256.txt`; binário novo).
Instalar como antes (cópia + entry, NDRV0 fallback intacto).
Medir (read-only): `sudo ga106ctl pci` 1× → snapshot DEVE ser vendor=10de
device=2504 (gate kernel rejeita outro com NoDevice → CLI exit≠0); 2ª leitura
para estabilidade + Command/MSE/BME; identidade == registry; disassembly
`configWrite* = NONE`.
PASS_RETRY: boot + pci SUCCESS + 10de:2504 + MSE/BME ok + zero resto +
AMD/WS ok + sem panic. Rollback: entry off/remoção via outro SO.
