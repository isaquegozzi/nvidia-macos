# TG-KEXT4-STATIC-IDENTITY-READS-LIVE-RUNBOOK (NÃO EXECUTAR sem auditoria Astra)

Pré-condição: `ASTRA-TG-KEXT4-STATIC-IDENTITY-READS-AUDIT` = PASS + staging
desabilitado + activate + reboot + primeiro boot 1.4.1 validado SEM selector 6.

Futura execução (somente se auditada), exatamente UMA vez:

```text
sudo ./ga106ctl static-identity
```

Comportamento contratado (implementação 1.4.1 desta árvore):

```text
1 selector 6 call
1 MMIO read (BOOT_42 @0xA00; BOOT_2 removido)
0 writes / 0 retries / 0 polling
1 map RO+Inhibit / 1 release / sem persistência
Command/MSE/BME before == after (sem restaurar)
```

Proibido: segunda chamada, pci/bar-map-probe/first-mmio-read de comparação,
qualquer MMIO write, DMA/IRQ/reset/GSP/firmware/VRAM/command submission.

Evidências a salvar: command+stdout+stderr+exit, ABI (32B), chip 0x176 (10-bit),
health AMD/WindowServer/displaypolicyd, regressões. STOP após.
