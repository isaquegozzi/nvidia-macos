# TG-KEXT2-RUNBOOK (futuro MAC_GPU_PCI_READ_0, NÃO executar)

Pré: NDRV0 + KEXT2 1.2.0 revisado (SHAs em kext2 `SHA256.txt`).
Instalar como KEXT0/KEXT1 (cópia + entry, NDRV0 fallback intacto).
Medir (read-only): `sudo ga106ctl pci` + estabilidade repetida + identidade ==
registry + Command estável + MSE/BME inalterados + disassembly `configWrite*=NONE`.
PASS: boot + `pci` SUCCESS + identidade/revisão + MSE/BME ok + zero
map/MMIO/DMA/irq/reset/GSP + AMD/WS ok + sem panic.
Rollback: entry off/remoção via outro SO.
