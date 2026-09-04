# docs/ — índice

Fase 0 é 80% documentação. Código sem documento correspondente é suspeito.

| Diretório           | Pergunta que responde                                              | Status Fase 0 |
|---------------------|--------------------------------------------------------------------|---------------|
| `ampere/`           | O que a arquitetura Ampere garante para todo chip?                 | placeholder   |
| `ga106/`            | O que a nossa GA106 específica tem/é?                              | placeholder   |
| `nouveau/`          | O que o Nouveau já sabe fazer com GA106/GSP?                       | placeholder   |
| `nvk/`              | O que o NVK (Vulkan aberto) revela sobre init?                     | placeholder   |
| `nvidia-open/`      | O que o RM open (`610.57.04`) implementa para GA106?               | mapeado       |
| `nvidia-open-gpu-doc/` | O que a documentação pública de HW garante para Ampere/GA106?   | mapeado       |
| `research/`         | Decisões, becos sem saída, próximos passos                        | placeholder   |

Regra: toda observação de hardware vira nota datada aqui **antes** de virar
código em `linux/`. Ver `SAFETY.md` antes de propor qualquer escrita.

## Documentos Fase 0 (2026-09-04)

- `nouveau/ampere-discovery.md` — discovery Nouveau (probe PCI, engines GA10x, GSP-RM, BAR/VM, display).
- `nvk/ampere-discovery.md` — discovery NVK (nv_device_info, classes, queues, memória, WSI).
- `ga106/device-ids.md` — Device IDs GA106 verificados + armadilhas não-GA106.
- `ampere/architecture-overview.md` — overview Ampere (dies, blocos, RTX 3060, firmwares GA106).
- `nvidia-open/GA106-kernel-map.md` — mapa GA106 no `open-gpu-kernel-modules` tag `610.57.04` (SHA `e4a5faa…`): identidade HAL, class-list (`AMPERE_B`), engines GR/FIFO/MMU/CE/GSP/SEC2/BIF/DISP/vídeo; driver-instalado vs source + lacunas UNKNOWN.
- `nvidia-open-gpu-doc/ampere-ga106-index.md` — índice `open-gpu-doc` commit `9fdf5c4…` aplicável a Ampere/GA106 (3D `clc797.h`, compute, CE, FIFO, MMU, display, vídeo, manuais `ga100/ga102`, BIT/DCB/Falcon-GSP); confirmação `AMPERE_B` + lacunas UNKNOWN.
