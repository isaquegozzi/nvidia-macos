# docs/ — índice

Fase 0 é 80% documentação. Código sem documento correspondente é suspeito.

| Diretório   | Pergunta que responde                              | Status Fase 0 |
|-------------|----------------------------------------------------|---------------|
| `ampere/`   | O que a arquitetura Ampere garante para todo chip? | placeholder   |
| `ga106/`    | O que a nossa GA106 específica tem/é?              | placeholder   |
| `nouveau/`  | O que o Nouveau já sabe fazer com GA106/GSP?       | placeholder   |
| `nvk/`      | O que o NVK (Vulkan aberto) revela sobre init?     | placeholder   |
| `research/` | Decisões, becos sem saída, próximos passos        | placeholder   |

Regra: toda observação de hardware vira nota datada aqui **antes** de virar
código em `linux/`. Ver `SAFETY.md` antes de propor qualquer escrita.

## Documentos Fase 0 (2026-09-04)

- `nouveau/ampere-discovery.md` — discovery Nouveau (probe PCI, engines GA10x, GSP-RM, BAR/VM, display).
- `nvk/ampere-discovery.md` — discovery NVK (nv_device_info, classes, queues, memória, WSI).
- `ga106/device-ids.md` — Device IDs GA106 verificados + armadilhas não-GA106.
- `ampere/architecture-overview.md` — overview Ampere (dies, blocos, RTX 3060, firmwares GA106).
