# TG-KEXT5-GSP-SYSMEM-ALLOC-LIVE-RUNBOOK (NÃO EXECUTAR sem auditoria Astra)

Pré-condição: `ASTRA-TG-KEXT5-GSP-SYSMEM-ALLOC-AUDIT` = PASS + staging
desabilitado + activate + reboot + primeiro boot 1.5.0 validado SEM selector 7.

Futura execução (somente se auditada), exatamente UMA vez:

```text
sudo ./ga106ctl prepare-gsp-sysmem
```

Comportamento contratado (implementação 1.5.0 desta árvore):

```text
1 selector 7 call
buffer 4096 alignment 4096, 1 segmento DMA, ADDRESS_READY
BME stays OFF
0 MMIO / 0 PCI writes / 0 DMA triggers / 0 firmware
mapping persiste no owner (sem complete/clear/release)
```

Segunda chamada esperada: ALREADY_PREPARED sem realocar.

Proibido: pci/bar-map-probe/first-mmio-read/static-identity de comparação,
qualquer MMIO/PCI write, BME, DMA trigger, firmware, GSP/Falcon.

Evidências a salvar: command+stdout+stderr+exit, ABI (48B, sem endereços),
health AMD/WindowServer/displaypolicyd, regressões. STOP após.
