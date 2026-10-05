# TG-KEXT1-RUNBOOK (futuro, NÃO executar)

Pré: NDRV0 + KEXT1 1.1.0 revisado (SHAs em kext1 `SHA256.txt`).
Instalar como o KEXT0 (cópia + entry `Kernel→Add`, NDRV0 fallback intacto).
Medir pós-boot (read-only): `sudo ga106ctl version|identity|status` +
correlação entryID/path + AMD/WS/displaypolicyd + ausência de fb/accel RTX.
PASS (`MAC_GPU_USERCLIENT_0`): boot+provider+AMD ok; 3 comandos SUCCESS com
valores do registry; close/reopen ok; 2º cliente simultâneo rejeitado limpo;
sem panic/regressão/atividade PCI-MMIO-DMA-GSP.
Rollback: entry off/remoção via outro SO (mesmo fluxo KEXT0).
