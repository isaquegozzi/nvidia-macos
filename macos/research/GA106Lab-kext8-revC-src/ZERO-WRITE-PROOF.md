# ZERO-WRITE-PROOF — selector9 caminho normal (P68 §14)

Producao: MMIO stores = 0 (nenhum OSWrite/write_reg; so OSRead via fluxos),
PCI config writes = 0 (so configRead), BME API = 0 (nm -u sem
setBusLeadEnable/setBusMasterEnable), DMA trigger = 0 (prepare/genSegments
apenas obtem IOVA; nenhum TRANSCFG/CMD/doorbell/Falcon/firmware/GSP),
selector7 interno = 0 (pagina dedicada fFlush*, nunca prepareGspSysmem),
unregister MMIO = 0 (nenhum write 0; teardown so complete/clear/releases).

Provas: `nm -u` sem OSWrite/configWrite/BME; grep sem 0x100c10/0x100c40 stores,
sem setBusLeadEnable(true), sem Falcon/DMA/firmware em verify; testes mock
com contadores (mmioWrite/configWrite/bme/dmaTrigger = 0); build.sh auditorias
STOP_USES_SHARED_FLOW + BINARY_ZERO_STORES_PCI_BME. Mutacoes de escrita devem
ser test-only isoladas (nenhuma criada na producao neste prompt).
