# docs/ga106/ — a nossa GA106 (RTX 3060)

Índice Fase 0 (documentos verificados):

- `device-ids.md` — Device IDs GA106 verificados + armadilhas não-GA106.
- `implementation-map.md` — mapa Nouveau `nvkm` (`nv176_chipset 0x176`, reuse GA102/GA100/TU102/legado).
- `initialization-map.md` — grafo de init (ctor vs lazy init, `boot0=0x176`, GSP-RM).
- `first-mmio-read.md` — candidatura do primeiro MMIO read (doc only): `NV_PMC_BOOT_0` (`BAR0+0x00`, `0x176xxxxx`) + reuse `common/` + riscos + gates.

Placeholder restante (preencher após bancada real, somente leitura):
- `bancada.md` — BDF, `lspci -nnv`, revisão, VBIOS version, subsistema, BARs (`resource*`).
- `bars.md` — tamanho/flags de cada BAR, o que é mapeável para leitura.
- `straps-bios.md` — straps, VBIOS dump **read-only** (se/quando documentado como seguro).
- Device IDs observados: 0x2503/0x2504 (12GB), 0x2487 (laptop), 0x24AA — confirmar na bancada.

Proibido nesta fase: qualquer escrita MMIO, `mmap(PROT_WRITE)`, `/dev/mem`,
unbind/bind, reset, clocks, firmware, power state. Ver `../SAFETY.md`.
