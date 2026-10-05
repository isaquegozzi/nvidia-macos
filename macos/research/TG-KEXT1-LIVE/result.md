# TG-KEXT1-LIVE result — boot 958A4CCC (2026-09-07 17:43:26)

## Veredito

```text
MAC_GPU_USERCLIENT_0 = PASS
USERSPACE_KERNEL_TRANSPORT_COMPLETE = YES
```

 Transporte userspace↔kernel read-only provado vivo. Autorizações seguem todas NÃO:
 PCI_CONFIG / BAR_MMIO / DMA / GSP / COMPUTE / GRAPHICS_ACCEL.

## Prova (critério §15, todos atendidos)

 boot SUCCESS / KEXT1 1.1.0 YES / REV1 NO / PASS NO / nó PRESENT+ACTIVE /
 State START_SUCCESS + Revision TG-KEXT1 + vendor 10de + device 2504 +
 entryID 4294967865 = pai GFX0 (ENTRY_ID_MATCH YES) / AMD NO / HDAU NO /
 ignore-ndrv PRESENT / SEM framebuffer / AMD HEALTHY / WS HEALTHY /
 displaypolicyd 0 exits / version+identity+status SUCCESS / open-close-reopen
 SUCCESS / 2º simultâneo rejeitado ExclusiveAccess (-536870203) / non-root
 negado NotPrivileged (-536870207) sem selector / panic NO / sem regressões /
 PCI_CONFIG/MMIO/DMA/GSP ZERO (auditoria + desktop estável + NDRV intacto).
