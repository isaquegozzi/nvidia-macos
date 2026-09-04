# docs/ga106/ — a nossa GA106 (RTX 3060)

Placeholder Fase 0.

Escopo previsto (preencher após bancada real, somente leitura):
- `bancada.md` — BDF, `lspci -nnv`, revisão, VBIOS version, subsistema, BARs (`resource*`).
- `bars.md` — tamanho/flags de cada BAR, o que é mapeável para leitura.
- `straps-bios.md` — straps, VBIOS dump **read-only** (se/quando documentado como seguro).
- Device IDs observados: 0x2503/0x2504 (12GB), 0x2487 (laptop), 0x24AA — confirmar na bancada.

Proibido nesta fase: qualquer escrita MMIO, `mmap(PROT_WRITE)`, `/dev/mem`,
unbind/bind, reset, clocks, firmware, power state. Ver `../SAFETY.md`.
