# linux/ga106-lab/ — SOMENTE LEITURA (Fase 0)

Laboratório de observação passiva da GA106 no Linux. **Nenhum binário aqui pode
escrever no hardware.** Esta é a regra que protege a bancada.

## Permitido

- `lspci -nnv`, `lspci -xxxx` (inspeção visual, sem escrita).
- Leitura de sysfs com `O_RDONLY`: `config`, `resource*`, `vendor`, `device`, `driver` (readlink).
- `mmap` com `PROT_READ` documentado, quando justificado em `docs/`.
- `dmesg` / `journalctl` / logs Nouveau / NVK.

## Proibido (Fase 0) — reafirmação do README raiz

Sem escrita MMIO, sem clocks/voltagem/fan, sem reset/FLR, sem command buffer
(PUSH/doorbell/FIFO), sem firmware (GSP/PMU/SEC2/NVDEC), sem power state
(D0/D3/PM/ASPM/link), sem unbind/bind/`driver_override`, sem `/dev/mem`,
sem `O_RDWR` em recurso PCI, sem `mmap(PROT_WRITE)` em BAR.

## Estado atual

Diretório intencionalmente sem código-fonte nesta etapa de scaffolding.
O primeiro tool read-only será proposto via `docs/research/` + revisão,
e só então implementado aqui com asserts/guardas que impedem escrita
(ex. `static_assert`, `O_RDONLY`, `PROT_READ`, ausência de `iopl/ioperm`).

## Layout futuro (quando autorizado)

```
ga106-lab/
├── README.md          # este arquivo (regras read-only)
├── CMakeLists.txt     # tools read-only, linkadas em nvidia_common
└── src/
    ├── pci_scan.cpp   # enumera BDFs via sysfs O_RDONLY (proposta futura)
    └── bar_view.cpp   # imprime BARs sem mapear p/ escrita (proposta futura)
```
