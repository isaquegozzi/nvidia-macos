# ADR-0002: Reuse TinyGPU para GA106 interna (Hackintosh Tahoe Intel) — Fase TG0, somente .md

> Status: **PROPOSED (Fase TG0, documental)** — 2026-09-04.
> Supersede: `docs/macos/ADR-0001-driver-entrypoint.md` (KEXT-first para lab PCI) — ver banner no topo daquele arquivo.
> Escopo: somente decisão de estratégia de reuse (A/B/C/D/E). Nenhum código autorizado. Sem hardware tocado.
> Entradas (não re-auditadas, vindas dos auditores TG0): `docs/tinygpu/support-matrix.md`, `docs/tinygpu/why-usb4.md`, `docs/tinygpu/internal-pcie-gap.md`, `docs/tinygrad-nv/ga106-gap-analysis.md`, `docs/tinygrad-nv/nvdev-audit.md`, `docs/tinygpu/binary-architecture.md`, `docs/tinygpu/pci-transport-contract.md`, `docs/tinygpu/apl-remote-pci.md`, `docs/tinygpu/remote-protocol.md`, `docs/macos/driver-architecture-gate.md`.
> Se algum arquivo acima faltar no checkout, o veredito correspondente é UNKNOWN (nada inventado).

## 0. Decisão (uma)

**Estratégia C — TinyGPU-compatible transport.**

Manter o stack tinygrad NVDev→GSP→GPFIFO→compute **sem reescrever** e manter o protocolo de transporte TinyGPU **byte-compatível** (`RemoteCmd` 1–7,11 sobre socket UNIX, semântica `MAP_SYSMEM_FD+mmap`), trocando **somente** o que bloqueia a GPU interna: matching do dext (`IOPCITunnelCompatible`, categoria/entitlements) num fork de dext+servidor derivado do source shipped (`extra/usbgpu/tbgpu/installer/*@e8c8ba1c`), para que `APLRemotePCIDevice`/`PCIIface`/`NVDev` operem contra `10de:2504` interna como se fosse tunneled.

Frase justificadora: **o gap é só matching (C+D CONFIRMED antes de `Start`), o bring-up é 10/10 genérico Ampere sem `nvidia.ko`, então o menor delta que desbloqueia a interna é um dext fork wire-compatível, não um driver novo.**

## 1. Opções — evidência por opção (veredito + porquê, com fonte)

### A — KEXT IOKit custom (lab PCI read-only, ponteiro direto)

- Veredito: **REJEITADA como estratégia principal (mantida como fallback de lab).**
- A favor: era a decisão do ADR-0001; carregabilidade Hackintosh Tahoe por injeção OpenCore com precedente Lilu 1.7.1/WEG 1.7.1d7 + `kextstat` (gate §2.4, LIKELY); sem check `built-in`/entitlement Apple (gate §3.1 CONFIRMED mecanismo, UNCERTAIN aplicabilidade); ponteiro direto `mapDeviceMemoryWithRegister` (gate §2.2 CONFIRMED doc legado).
- Contra: reuse do stack TinyGPU ≈ 0% no transporte (descarta `server.c` + `TinyGPUDriver*.cpp/.iig` + `APLRemotePCIDevice`); reimplementa do zero `MapBar/CfgRead/Write/Reset/PrepareDMA` + validação `off+sz`, `BULK 64MB`, `shm/SCM_RIGHTS` (contract §2–§4); paga panic=reboot + AuxKC+reboot por iteração (gate §2.3 CONFIRMED WWDC19); Tahoe é o último major Intel (gate §2.1 CONFIRMED) — beco sem saída além-Tahoe; não aproxima GSP/compute (gate §4: PCI lab ≠ driver gráfico).
- Conclusão: útil se o dext custom for bloqueado por signing/`built-in`, mas como estratégia principal perde para C em reuse e longevidade.

### B — PCIDriverKit custom (dext novo, protocolo próprio)

- Veredito: **REJEITADA como desnecessariamente divergente (C é B com compatibilidade de fio).**
- A favor: resolve o mesmo bloqueio que C (remove `IOPCITunnelCompatible=True`, ajusta `IOPCIClassMatch`/`IOMatchCategory`/entitlements); debugging userspace sem parar o kernel (gate §1.4 CONFIRMED WWDC19/debugging doc); DMA mediado `PrepareForDMA(maxAddressBits=40)` já provado suficiente para `MAP_SYSMEM_FD` (binary-architecture §7, apl-remote-pci §4).
- Contra: protocolo próprio quebra compatibilidade com `RemotePCIDevice._rpc` (`system.py:376-385`: header 33B `<BIIQQQ` + resposta 17B `<BQQ` + `SCM_RIGHTS` só em cmd 2) e com `RemoteMMIOInterface`/`_bulk_read/_bulk_write` (remote-protocol §2–§3, incl. escrita sem resposta); exigiria reescrever/forkar `system.py:311-447` + `serve.py:1-101`/`server.c:1-285` + `CopyClientMemoryForType` selectors 0–3 (`TinyGPUDriverUserClient.iig:6-12`); todo desvio do fio documentado em contract §2 (13 cmds) vira risco de dessincronização de stream.
- Conclusão: todo benefício de B é herdado por C; todo custo extra de B é evitável mantendo o fio idêntico. Escolher B seria pagar divergência sem ganhar desbloqueio adicional.

### C — TinyGPU-compatible transport (ESCOLHIDA)

- Veredito: **ACEITA.**
- Prova de que o bloqueio é só matching: `IOPCIClassMatch=0x03000000` (`Info.plist:15-16@e8c8ba1c`, idêntico no binário shipped — binary-architecture §3) casa com `base_class=0x03` do `PCIIface` (`ops_nv.py:556-562`); allowlist `transport.pci=[10de any, 1002 any]` (`IOPCIPrimaryMatch 0x000010de&0x0000FFFF`, entitlements extraídos do blob + provision — binary-architecture §5) casa com `vendor=0x10de` e com `0x2504&0xFF00=0x2500 ∈ devlist` (`ops_nv.py:560`, `system.py:80` — support-matrix §5, ops-nv-audit §2–§3, YES restrito a enumeração); o que filtra a interna é `IOPCITunnelCompatible=True` nas duas pontas (source + shipped — binary-architecture §3, why-usb4 D CONFIRMED) + risco `built-in` sem isenção `...builtin` no dext shipped (why-usb4 B LIKELY, gate §3.1 CONFIRMED mecanismo); diagrama prova que a interna nunca chega a `Start_Impl/Open/RegisterService("tinygpu")`, logo `open_tinygpu` → MISS → `"Driver not available..."` (`server.c:98,195`) — internal-pcie-gap (gap antes de `Start`, não em BAR/MMIO/DMA/GSP/socket/ASM).
- Prova de que o transporte restante já basta: APL emite só 8/13 cmds — `MAP_BAR(1)/MAP_SYSMEM_FD(2)/CFG_READ(3)/CFG_WRITE(4)/RESET(5)/MMIO_READ(6)/MMIO_WRITE(7)/RESIZE_BAR(11 noop)` (contract §2–§3, apl-remote-pci §3, remote-protocol §5–§6); `MAP_BAR` via `IOConnectMapMemory64` + cache `g_bars[6]`, `CFG` via selectors 0/1 → `ConfigurationRead/Write8/16/32`, `RESET` sel 2 → `FunctionReset→HotReset`, `MMIO` via `mmio_copy` volatile 32-bit + `validate_bar` + `BULK 64MB`, `MAP_SYSMEM_FD` via `shm_open /tinygpu_%d + PrepareDMA sel 3 (≥4097B, seg≤32, maxAddressBits=40)` + `SCM_RIGHTS`, `RESIZE_BAR` noop OK tolerado por `PCIIfaceBase.__init__:263 suppress` (apl-remote-pci §4, contract §4, binary-architecture §7); `PROBE(0)/MAP_SYSMEM(8)/SYSMEM_RW(9,10)/PING(12)` nunca são emitidos no caminho APL (server.c retorna `status=1`, irrelevante — remote-protocol §6).
- Prova de que o bring-up acima do transporte já existe sem `nvidia.ko`: `PCIDevice.__init__:168-194` exige GPU sem driver + VFIO opcional + fallback `enable` sysfs; `PCIIface:559` recusa coexistência com `NVKIface.root`; `NVDev.__init__:75-76` abre PCI por `map_bar(0)` MMIO, sem `/dev/nvidia*` (contraste `NVKIface:388-390` que sim abre) — nvdev-audit §0 CONFIRMED; 10/10 etapas Ampere genéricas cobertas (`nvdev.py:74-163`, `ip.py:19-661`, `ops_nv.py:363-728`, `system.py:58-90,160-307`) — ga106-gap-analysis §26-30.
- Delta mínimo de C: fork de `TinyGPUDriverExtension/Info.plist` (remover/condicionar `IOPCITunnelCompatible`, revisar `IOMatchCategory`/`IOProbeScore` se necessário) + entitlements/provision para a interna (dev wildcard `0xFFFFFFFF&0x00000000` + `dk=0x8001` em lab descartável, SIP off — gate §1.4; produção `0x10DE` LIKELY negada — gate §3.3, fora de escopo TG0) + verificação `ioreg` (`built-in`, `IOPCITunnelled`, `class-code`, `IOServiceDEXTEntitlements`, `AAPL,slot-name` — internal-pcie-gap verificação, ADR-0001 consequências). Lógica `TinyGPUDriver.cpp:34-193` + `TinyGPUDriverUserClient.cpp:99-191` + `server.c:1-285` reutilizada verbatim salvo matching.
- Por que não D: D exige zero mudança mas herda o filtro tunneled — a interna jamais instancia o driver (internal-pcie-gap). C é D + o menor patch que remove o filtro, preservando todo o resto.

### D — Reuse TinyGPU.app direto (sem fork)

- Veredito: **REJEITADA para GPU interna (válida só para eGPU USB4/TB).**
- A favor: reuse 100% sem trabalho; caminho APL completo já provado para tunneled (contract + apl-remote-pci + binary-architecture); `ensure_app` pin `c0d024f9` + `ditto/install` + toggle DriverKit (`system.py:420-427`, `docs/tinygpu.md:29-31`).
- Contra (bloqueador): `IOPCITunnelCompatible=True` + ausência de `...builtin` no shipped provam enforcement tunneled-only no matching (why-usb4 C+D CONFIRMED); interna nunca instancia `TinyGPUDriver`, nenhum serviço `"tinygpu"` existe, `server.c:195` falha determinística (internal-pcie-gap). `E=FALSE` para ASM (bridge específico não é requisito do `.app` — why-usb4 E) não salva a interna; `A+B LIKELY` como co-causas não abrem exceção sem `ioreg`/teste vivo. D confunde "arquitetura Ampere suportada (CONFIRMED)" com "SKU 10de:2504 verificado (UNKNOWN)" e "bring-up GA106-específico (UNCERTAIN)" (support-matrix §5).
- Conclusão: D é o teste de controle (se um dia houver eGPU TB disponível), não a estratégia para slot x16 interno.

### E — NEED_MORE_EVIDENCE (indecisão)

- Veredito: **REJEITADA como decisão (lacunas viram follow-ups, não veto).**
- Lacunas reais: `ioreg` vivo no Tahoe alvo (`built-in`/`IOPCITunnelled`/entitlements — internal-pcie-gap verificação, gate §3.2 UNCERTAIN); teste wildcard dev + `dk=0x8001` em ambiente descartável (ADR-0001 consequências); `BOOT_0 ≈ 0x176000A1` + `BOOT_42 arch 0x17/impl 6` + `sm_86`/`_query_gpu_info` + golden-ctx `promote_ctx` + `GSP_INIT_DONE` só em HW (ga106-gap-analysis leitura do gap; nvdev-audit §6 UNCERTAIN; MMIO BLOCKED).
- Por que não veta C: nenhuma lacuna inverte a ordem — mesmo com `built-in` ausente, o filtro `tunnel` sozinho já exige fork; mesmo com `built-in` presente, o fork de lab (`dk=0x8001`/SIP off) continua o menor delta; o risco FW/ctx (`gsp/bootloader/booter_load ga102-570.144` vs GA106, `ip.py:172-173,402,426-428`, sem SHA ga106) afeta igualmente A/B/C/D e só se resolve em HW. Evidência faltante vira verificação obrigatória antes de codificar, não indecisão estratégica.

## 2. Reuse% por subsistema — metodologia + tabela

Metodologia (aplicada igual a todos os subsistemas): `% = (etapas NVDev reutilizáveis sem reescrever) / (etapas NVDev totais do subsistema no caminho `PCIIface→NVDev→GSP→GPFIFO`)`, contado sobre as 10 etapas de `ga106-gap-analysis` (PCI detect, BAR mapping, BOOT0, GSP boot, MMU, VRAM, FIFO, GPFIFO, compute, DMA) + 2 camadas fora do NVDev (compiler, PCI transport). Numerador = etapas com implementação genérica Ampere em `tinygrad@e8c8ba1c` reusável verbatim via transporte C; denominador = etapas do subsistema. Fonte citada por linha. `0/10 com quirk/SHA ga106-específico` (só `IMPLEMENTATION_GA106` constante em `nv_570.py:23415`/`nv_580.py:24463`/`nv_610.py:25044`) não reduz o numerador — reduz a **confiança de validação**, registrada em §4 (PROVADO vs NÃO PROVADO). Compiler e PCI transport usam denominador próprio (artefatos listados).

| Subsistema | Etapas NVDev contadas (fonte) | Reuse% (C) | O que é reusado sem reescrever | Ressalva (não reduz %, reduz confiança) |
|---|---|---|---|---|
| identification (PCI detect + BOOT0/42) | 2/2 = **100%** (`ops_nv.py:PCIIface:560` + `system.py:pci_scan_bus:58-80, pci_probe_device:87-90`; `nvdev.py:_early_ip_init:112-116` `NV_PMC_BOOT_0/42` → `GA1/ga102/mmu_ver 2`) | 100% | filtro `(0x2504&0xFF00)=0x2500 ∈ devlist` YES (só enumeração); `chip_id`/`fw_name`/`mmu_ver` genéricos | GA106 cai em `GA1/ga102` por `arch 0x17`; sem quirk `0x2504`/SHA ga106 (`ip.py:172-173,426-428`); SKU verificado UNKNOWN (support-matrix §5) |
| GSP (Falcon boot + RM boot + RPC) | 5/5 genérico = **100%** (`ip.py:NV_FLCN.prep_ucode:110-169`, `prep_booter:171-184`, `init_hw:186-210`; `NV_GSP.init_gsp_image:401-423`, `init_boot_binary_image:425-432`, `init_wpr_meta:434-455`, `init_hw:510-520`, `run_cpu_seq:629-660`) | 100% | VBIOS/BIT/FWSEC/FRTS, `booter_load-570.144.bin`, `gsp-570.144.bin` (fixo `ga102`), `bootloader-570.144.bin`, WPR2/mailbox/`GSP_INIT_DONE` | FW `ga102`, não `ga106`; fecha **se** RM aceitar GA106 como `GA1` — UNCERTAIN sem HW (gap-analysis leitura do gap §2) |
| memory (BAR mapping + VRAM alloc) | 2/2 = **100%** (`nvdev.py:NVDev:76,133` `map_bar(0/1)`; `_early_mmu_init:131-147` `vram_size=SCRATCH_GROUP_42<<20 = 0x1183a4` Nouveau; `_alloc_boot_mem:149-159`; `PCIIfaceBase.alloc:267-281`) | 100% | `large_bar` (BAR1), `palloc [512MB,2MB,4KB]`, sysmem-vs-VRAM | tamanho real só em HW; `resize_bar` vira noop no APL (tolerado — contract §2 ID 11) |
| MMU (page tables + VA + BAR1 janela) | 1/1 = **100%** (`nvdev.py:_early_mmu_init:124-147` `dev_vm/tu102`, `dev_mmu/tu102\|gh100`, `pte/pde/dual_pde VER2/3`, VA `48b v2/56b v3`; `NVMemoryManager:69-72` `1<<44@0x1000000000` + INVALIDATE; `ip.py:rpc_set_page_directory:594-599`, `BAR1_BLOCK:516-517`) | 100% | `mmu_ver 2` para GA106, `palloc_ranges`, `reserve_ptable=!large_bar` | faults nativos delegados ao RM sob GSP (gap-analysis) |
| FIFO (FIFO + GPFIFO + doorbell) | 2/2 = **100%** (`ip.py:init_golden_image:468-508` runlists/`COPY_SERVER_RESERVED_PDES`/`GPFIFO 32 entries`/`GET_CONTEXT_BUFFERS_INFO`/`promote_ctx:457-466`; `ops_nv.py:NVDevice:610-621`, `_new_gpu_fifo:642-666`, `_submit_to_gpfifo:114-126` doorbell `gpu_mmio[0x90//4]=token`, `setup_usermode:570` BAR0 `0xbb0000`) | 100% | `KEPLER_CHANNEL_GROUP_A`, `FERMI_CONTEXT_SHARE_A`, `GPFIFO_SCHEDULE`, token RM | sem `ga102_fifo` nativo — só RM; suficiente se GSP subir, sem fallback (gap-analysis) |
| compute (compute + DMA + QMD) | 2/2 = **100%** (`ops_nv.py:NVComputeQueue:128-192` QMD v3 `0x40`/`SEND_PCAS_A`/signal, `NVCopyQueue:194-212` `NVC6B5_*` fatias `1<<31`, `NVProgram:248-339` `arch sm_XX`, `QMD:44-76`; classes `AMPERE_CHANNEL_GPFIFO_A 0xc56f`/`AMPERE_COMPUTE_B 0xc7c0`/`AMPERE_DMA_COPY_B 0xc7b5` — `ip.py:357`, `nv_570.py:24729,24838,24825`) | 100% | default Ampere (sem branch `AD`/`GB`); `sm_86` derivado em HW via `_query_gpu_info:627-679` | `sm_86`/golden-ctx/`SEND_PCAS_A` só validáveis em HW; `viddec` só AD/GB (fora de escopo compute) |
| compiler (CUDA C + PTX + NAK) | 3/3 caminhos = **100%** (`compiler_cuda.py:9,46-100` `NVRTC/NVCC/PTX/NVPTX`, `osx_docker_cmd ghcr.io/tinygrad/cuda-arm64:v2.3`; `extra/setup_nvcc_osx.sh:1-31` `cuda-nvcc:12.8` + shims `~/.local/bin`; `docs/tinygpu.md:43-53`) | 100% | nenhum reescrever de renderer (`CUDARenderer/PTXRenderer/NVCCRenderer/NAKRenderer` — stack-map §1–§2); no macOS só `Docker Desktop` + `PATH` | sem Docker+`PATH`, `Tensor→PTX/cubin` não fecha mesmo com PCI OK (stack-map correção §4) |
| PCI transport (socket + dext + sysmem) | 8/8 cmds APL = **100%** protocolo; dext source **~95%** (só matching muda) (`system.py:RemoteCmd:311-312` 0–12, APL usa 1–7,11; `RemotePCIDevice:334-414`, `APLRemotePCIDevice:416-447`; `server.c:1-285`; `TinyGPUDriver.cpp:34-193` + `UserClient:99-191` + `.iig:6-12` selectors 0–3) | 100% / ~95% | header 33B/17B, `SCM_RIGHTS` só cmd 2, escrita sem resposta, `shm /tinygpu_%d`, `PrepareDMA ≥4097B/seg≤32/maxAddressBits=40`, `BULK 64MB`, `MAX_SYSMEM 128` (contract §2, remote-protocol, apl-remote-pci §4) | delta = `Info.plist:15-18` (`IOPCITunnelCompatible`, categoria/score) + entitlements/provision (vendor OK, tunnel/builtin NOK) + `ioreg` gate; `PROBE/MAP_SYSMEM/SYSMEM_RW/PING` fora do caminho APL |

Resumo: **10/10 etapas NVDev genéricas Ampere reutilizáveis sem reescrever (100%); 0/10 com especialização ga106 (validação pendente em HW).** Transporte APL 8/8 cmds reutilizável; dext ~95% (só matching/entitlements mudam); compiler 3/3 caminhos reutilizável via Docker.

## 3. MAC-COMPUTE-0 — plausibilidade

**MAC-COMPUTE-0 = MEDIUM.**

Porquê: o desbloqueio de transporte tem delta conhecido e mínimo (fork de matching + `ioreg`/wildcard-dev de lab) e o bring-up acima dele já existe 10/10 genérico sem `nvidia.ko` (nvdev-audit CONFIRMED); mas a prova fim-a-fim falta em três pontos que só HW resolve — (1) GA106 aceita como `GA1/ga102` pelo RM `570.144` (`GSP_INIT_DONE`/`promote_ctx`/`SEND_PCAS_A`, sem SHA ga106 — gap-analysis §2, nvdev-audit §6 UNCERTAIN), (2) `BOOT_0 ≈ 0x176000A1` + `BOOT_42` + `sm_86` nunca lidos nesta bancada macOS (MMIO BLOCKED), (3) `built-in`/`IOPCITunnelled`/DMA-IOVA reais nunca inspecionados no Tahoe alvo (internal-pcie-gap verificação, gate §3.2 UNCERTAIN). MEDIUM = "caminho fechado no papel, risco residual concentrado em FW/ctx + matching vivo". Não é HIGH (exigiria SKU verificado + GSP log em HW), não é LOW/BLOCKED (o bloqueio D é contornável pelo fork C; E/FALSE exclui bridge como requisito).

## 4. PROVADO vs NÃO PROVADO (nada além do source/binário + aritmética)

PROVADO (source/binário/aritmética em `tinygrad@e8c8ba1c` + `tinygpu_releases@c0d024f9`):
- `PCIIface` reconhece `10de:2504` na enumeração: `0x2504&0xFF00=0x2500 ∈ devlist` (`ops_nv.py:560`, `system.py:80`) — YES restrito a enumeração (support-matrix §5, ops-nv-audit §2–§3).
- `NVDev` inicializa sem `nvidia.ko`: exige GPU sem driver, `map_bar(0)` MMIO, `rm_alloc/control` via `NVDev.gsp`, nenhum `/dev/nvidia*` (nvdev-audit §0 CONFIRMED; contraste `NVKIface:388-390`).
- Dext matching shipped = `IOPCIClassMatch 0x03000000` + `IOPCITunnelCompatible=True` + entitlements `0x10DE/0x1002 any-device`, sem `...builtin` (binary-architecture §3/§5, idêntico ao source).
- `RemoteCmd` 0–12, APL usa 1–7,11; fio 33B/17B, `SCM_RIGHTS` só cmd 2, escrita sem resposta (contract §2, remote-protocol §2–§6).
- why-usb4 = C+D CONFIRMED, A+B LIKELY, E FALSE (para o `.app`), F UNCERTAIN (why-usb4).
- Gap = matching DriverKit antes de `Start`; interna nunca instancia `TinyGPUDriver`, sem serviço `"tinygpu"` (internal-pcie-gap + diagrama).
- 10/10 etapas Ampere genéricas em NVDev; 0/10 ga106-específicas (só `IMPLEMENTATION_GA106` constante — gap-analysis §26-30, nvdev-audit §6).
- `BOOT_0` GA106 ≈ `0x176000A1` (padrão `0x176xxxxx`, rev `a1`) — first-mmio-read §2 (derivado de `nv_ref.h@610.57.04` + Nouveau `base.c@986c24e0`).

NÃO PROVADO (exige `ioreg`/HW vivo, fora do TG0):
- Que `10de:2504`/GA106 completa GSP→GPFIFO→compute via transporte C (golden-ctx, `sm_86`, `GSP_INIT_DONE`, doorbell/token reais).
- Que a interna do Tahoe alvo tem/ausenta `built-in`/`IOPCITunnelled`, qual `memoryIndex`/BAR size real, qual IOVA/`paddrs` real do `PrepareDMA`, qual `Start_Impl` log `ven/dev` real.
- Que wildcard dev + `dk=0x8001`/SIP-off casa a interna forkada em lab descartável; que produção `0x10DE` seria concedida (fora de escopo — gate §3.3 LIKELY negada).
- Que FW `ga102-570.144` + `booter_load`/`bootloader` + `golden image` tratam GA106 sem quirk; que `BAR1`/`0x1183a4`/`0xb80f48/50`/`0xbb0000` comportam-se igual na interna macOS.
- Qualquer afirmação Metal/display/WindowServer (fora deste ADR — ver project-impact: Metal segue NÃO resolvido).

## 5. Consequências (doc-only, sem código)

- Follow-ups obrigatórios antes de qualquer código (não bloqueiam este ADR, bloqueiam implementação): `ioreg -l -w0` no nó `10de:2504` (`built-in`, `IOPCITunnelled`, `class-code`, `IOServiceDEXTEntitlements`, `AAPL,slot-name`); `systemextensionsctl list`; `log show --predicate tinygpu`; plano de fork wire-compatível (só `Info.plist` + entitlements) com matriz de teste descartável (SIP off, sem promessa de produção).
- ADR-0001 permanece como histórico (KEXT-first para lab PCI); a estratégia de compute macOS segue este ADR-0002 (C).
- Nenhuma escrita MMIO/firmware/power/display autorizada por este ADR (gate §4; SAFETY docs).

## 6. Referências (pins TG0)

- `tinygrad@e8c8ba1c7751142f7b20d85692afd2da5cef5e84` (2026-09-04) + `tinygpu_releases@c0d024f9ff0e1dc8fdf217f255da7101d91e8323` (`TinyGPU.zip` 1634625 B, sha256 `0c47285e...`).
- `docs/tinygpu/support-matrix.md` §2/§5, `docs/tinygpu/why-usb4.md`, `docs/tinygpu/internal-pcie-gap.md`, `docs/tinygpu/binary-architecture.md` §3/§5/§7, `docs/tinygpu/pci-transport-contract.md` §2–§4, `docs/tinygpu/apl-remote-pci.md` §3–§4, `docs/tinygpu/remote-protocol.md` §5–§6.
- `docs/tinygrad-nv/ops-nv-audit.md` §2–§4, `docs/tinygrad-nv/nv-stack-map.md`, `docs/tinygrad-nv/nvdev-audit.md` §0–§6, `docs/tinygrad-nv/ga106-gap-analysis.md` §26-30 + leitura do gap.
- `docs/macos/driver-architecture-gate.md` §1–§5, `docs/ga106/first-mmio-read.md` §2, `docs/macos/common-reuse.md`.

## 7. Addendum TGM0 (2026-09-04) — correção `IOPCITunnelCompatible` (decisão NÃO reescrita)

Este addendum registra a correção do bug TG0 sem reescrever a decisão (reavaliação da estratégia fica para follow-up). Bug: `IOPCITunnelCompatible=true` foi usado como prova de "Thunderbolt-only" (matching excluiria a interna). Correção perante a documentação Apple atual (fonte canônica): a chave declara que a personality **suporta PCI tunneled/Thunderbolt** (capacidade do driver — "To indicate that your PCIe driver supports Thunderbolt, include the `IOPCITunnelCompatible` key [...]", `developer.apple.com/documentation/pcidriverkit/creating-custom-pcie-drivers-for-thunderbolt-devices`); a presença real de túnel é indicada por **`IOPCITunnelled` no IORegistry** (propriedade do dispositivo/caminho — Thunderbolt Device Driver Programming Guide, arquivo Apple). A recíproca (presença excluiria não-tunneled) não é documentada, e não há evidência TG0 de checagem de `IOPCITunnelled` no driver/server. Ver Correção TGM0 em `docs/tinygpu/why-usb4.md` (C→`LIKELY` p/ externo, D→`UNKNOWN` p/ filtro), `docs/tinygpu/internal-pcie-gap.md` (Resultado A/B/C em aberto), `docs/tinygpu/binary-architecture.md` §9.

Passagens deste ADR SUPERADAS como prova (texto original preservado acima, leitura corrigida aqui):

- §0 frase justificadora ("o gap é só matching (C+D CONFIRMED...)"): o `D CONFIRMED` como enforcement é superado (D=`UNKNOWN`); a justificativa passa a repousar em base mais fraca (requisito USB4/TB documentado `LIKELY` + risco `built-in` + `ioreg` pendente).
- §C "o que filtra a interna é `IOPCITunnelCompatible=True`... (why-usb4 D CONFIRMED)" e "diagrama prova que a interna nunca chega a `Start_Impl`": superado como determinístico — internal-pcie-gap Resultado A/B/C em aberto.
- §C "Delta mínimo" e §2 transporte ("remover/condicionar `IOPCITunnelCompatible`" como remoção de filtro certo): passa a hipótese de fork a validar, não remoção de bloqueio provado.
- §D "provam enforcement tunneled-only no matching (why-usb4 C+D CONFIRMED)" e "`server.c:195` falha determinística": superado como prova; D-para-interna deixa de ser "rejeitada por bloqueio provado" e passa a "rejeitada por caminho não demonstrado" (requisito USB4 documentado + ausência de teste vivo).
- §3 "o bloqueio D é contornável pelo fork C" e §4 "why-usb4 = C+D CONFIRMED" / "Gap = matching... nunca instancia": mesma superação; vereditos de reuse% e PROVADO/NÃO PROVADO fora do matching seguem válidos.

Status da decisão: **Estratégia C permanece ACEITA como PROPOSED documental**, mas sua premissa de bloqueio passa a `UNKNOWN`-dependente de `ioreg`/teste vivo. Follow-ups obrigatórios (§5) estendidos: além de `built-in`/`IOPCITunnelled` no `ioreg`, verificar empiricamente se o dext shipped casa (ou não) um nó não-tunneled — i.e., testar a perna B do Resultado A/B/C em vez de assumi-la.
