> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# TG-KEXT2-PCI-READ-STABILITY-RUNBOOK (2026-09-07)

Runbook técnico — NÃO EXECUTADO. Preparação apenas.

Baseline aprovado:
- KEXT GA106Lab 1.2.1, Revision TG-KEXT2-PCI-ADDR-FIX
- Config SHA 65f1d63be5c12ba4c9520a540189d86611bc682df3df218897790192438d56e7
- FIRST_BOOT PASS, BOOT_BASELINE_STABLE YES
- MAC_GPU_PCI_READ_0_RETRY PASS_STAGE1, PCI_ADDRESS_FIX_LIVE PASS
- Single call 19:41:20: vendor 10de device 2504 class 030000 rev a1,
  Command 0x0003 Status 0x0010, MSE ON BME OFF,
  BAR0 fb000000 BAR1 4000000c BAR2 00000004 BAR3 5000000c BAR4 00000004 BAR5 00001001,
  Subsys 10de:2504 ROM fc000000 CapPtr 0x60 IntLine ff IntPin 01, exit 0
- Provider GFX0@0 01:00.0 10de:2504 entry 4294967865 (0x100000239)
- Pós-call: AMD/WS/displaypolicyd HEALTHY, PANIC NO

Proibido nesta fase futura além do definido: CODE/BUILD/EFI/REBOOT/BAR-write-probe/
BAR-mapping/MMIO/DMA/interrupts/reset/GSP/firmware/VRAM/command-submission.

## 1. Sequência futura exata (máx 2 chamadas, sem terceira automática)

### Snapshot A — pré-flight (read-only, sem tocar dispositivo)
```text
KEXT2_FIX_1_2_1_LOADED = YES (kmutil, 1.2.1 único)
GA106LabState = START_SUCCESS
GA106LabRevision = TG-KEXT2-PCI-ADDR-FIX
provider registry = 10de:2504 / 01:00.0 / rev a1 / class 030000
AMD_GRAPHICS = HEALTHY
WINDOWSERVER = HEALTHY
DISPLAYPOLICYD = HEALTHY
BIN = GA106Lab-kext2-pci-address-fix-src/build/obj/ga106ctl (SHA 550d916b...)
```
Salvar em `preflight.txt`.

Executar exatamente uma vez:
```bash
sudo ~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext2-pci-address-fix-src/build/obj/ga106ctl pci > snapshot-A.txt 2> snapshot-A.stderr.txt; echo $? > snapshot-A.exit.txt
```
Capturar timestamp, stdout, stderr, exit code integralmente em `snapshot-A.txt`.

Se `vendor/device != 10de:2504` ou exit != 0:
```text
STOP
FAIL
NÃO executar snapshot B
MAC_GPU_PCI_READ_0 = FAIL
```

### Intervalo
```text
DELAY = 2 segundos (fixo, `sleep 2`)
```
Sem nenhuma ação no dispositivo durante o intervalo. Sem `configWrite`, sem
probe, sem mapping, sem MMIO. Apenas aguardar.

### Snapshot B
Executar exatamente mais uma vez (somente se A == SUCCESS + 10de:2504):
```bash
sudo ~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext2-pci-address-fix-src/build/obj/ga106ctl pci > snapshot-B.txt 2> snapshot-B.stderr.txt; echo $? > snapshot-B.exit.txt
```
```text
FUTURE_CALL_COUNT_MAX = 2
TERCEIRA_TENTATIVA_AUTOMÁTICA = NÃO
```

## 2. Comparação obrigatória A vs B
Comparar campo a campo (raw como fonte de verdade):
```text
Vendor / Device / Class / Revision
Command / MSE / BME
BAR0 / BAR1 / BAR2 / BAR3 / BAR4 / BAR5
Subsystem Vendor / Subsystem Device
Expansion ROM
Capabilities Pointer
Interrupt Pin (+ Interrupt Line como OBSERVE_ONLY, ver §3)
```
Gerar `comparison.txt` (diff linha a linha + tabela), `command-analysis.txt`,
`bar-analysis.txt`. Esperado:
```text
IDENTITY_STABLE = YES
COMMAND_REGISTER_UNCHANGED = YES
MSE_UNCHANGED = YES
BME_UNCHANGED = YES
BAR_RAW_STABLE = YES
```

## 3. Campos com tratamento especial
```text
STRICT_STABLE = Vendor, Device, Class, Revision, Command, MSE, BME,
  BAR0-5 raw, Subsystem Vendor/Device, Expansion ROM, Capabilities Pointer, Interrupt Pin
OBSERVE_ONLY = Status, Interrupt Line
```
Justificativa PCI:
- `Status (0x0010 base)`: contém bits W1C / live (ex.: Signaled Target Abort,
  Master Abort, DEVSEL timing pode refletir atividade do barramento/host).
  Não exigir igualdade rígida; registrar A/B e observar. FAIL somente se
  indicar erro novo associado a regressão do sistema.
- `Interrupt Line (0xff base)`: programável por software/firmware (valor de
  roteamento PIC/APIC, 0xff = no-connect comum). Pode mudar legitimamente por
  atividade do sistema sem escrita nossa. Registrar A/B, OBSERVE_ONLY.
  `Interrupt Pin (0x01 = INTA#)` é fixo por hardware/single-function; STRICT.
Objetivo: não gerar falso FAIL por semântica legítima.

## 4. Command decoding (raw = verdade)
Para cada snapshot registrar:
```text
COMMAND_A = (ex. 0x0003)
COMMAND_B =
MSE_A = (bit1: 0x0002)
MSE_B =
BME_A = (bit2: 0x0004)
BME_B =
```
Baseline atual conhecido (19:41:20): `Command 0x0003, MSE ON, BME OFF`, mas o
runbook NÃO hardcoda `0x0003` como condição universal. Critério principal:
```text
Command A == Command B
MSE A == MSE B
BME A == BME B
```
Decodificação: `MSE = (Command & 0x0002) != 0`, `BME = (Command & 0x0004) != 0`.
Qualquer mudança => STOP/FAIL (possível escrita externa ou efeito colateral).

## 5. BAR raw
Comparar somente valores raw hex tal como impressos (`fb000000`, `4000000c`, …).
```text
PROIBIDO: BAR size probe, escrever 0xffffffff, mapear BAR, acessar MMIO.
CRITÉRIO: BAR_RAW_STABLE = YES se os 6 raws idênticos A==B.
```
Mudança sem explicação clara => STOP/FAIL. Mudança com explicação (ex.:
rebalance PCIe pelo SO entre boots — não esperado intra-boot com 2s) deve ser
documentada e ainda assim FAIL para esta fundação, sem retry automático.

## 6. Ausência de escrita (referência, sem reauditoria total)
Evidências já aprovadas referenciadas:
```text
configWrite* = NONE
setMemoryEnable = NONE
setBusMasterEnable = NONE
OWN_CODE_PCI_WRITE = NONE
```
Fase futura executa somente `ga106ctl pci` 2x, sem outros comandos que
controlem o dispositivo. Verificação pós-fase: confirmar no fonte 1.2.1 que
nenhum caminho de `pci` chama `configWrite*`/`setMemoryEnable`/`setBusMasterEnable`
(auditoria offline já concluída; citar hashes bin `ffe35ba1…`/plist `4a974360…`).

## 7. Health check após B (obrigatório)
```text
AMD_GRAPHICS_AFTER = HEALTHY (1002:1638, acelerador active, Metal, display online)
WINDOWSERVER_AFTER = HEALTHY (pid vivo, sem crash)
DISPLAYPOLICYD_AFTER = HEALTHY (sem loop)
PANIC = NO
GPU_RESTART = NO
SPONTANEOUS_REBOOT = NO
WINDOWSERVER_CRASH = NO
```
Separar logs antigos (stale: panic 2026-09-06-003847, gpuRestart 2026-09-05,
shutdown_stall 19:33 pré-boot) de eventos do boot atual. Salvar
`graphics-health-after.txt` + `regression-check.txt`.

## 8. STOP conditions (imediato, sem correção live, sem 3ª leitura)
```text
- snapshot A != 10de:2504 ou erro/exit != 0
- snapshot B != 10de:2504 ou erro/exit != 0
- Command A != Command B
- MSE A != MSE B
- BME A != BME B
- BAR raw divergente sem explicação clara
- Status indica erro novo com regressão associada
- panic / GPU restart / spontaneous reboot / WindowServer crash / degradação grave do desktop
```
Em qualquer STOP: `MAC_GPU_PCI_READ_0 = FAIL`, preservar tudo, não retry.

## 9. Evidências futuras
Diretório: `~/Documents/nvidia-macos-BARE0/TG-KEXT2-PCI-READ-STABILITY/`
```text
preflight.txt
snapshot-A.txt (+ .stderr/.exit/timestamp)
snapshot-B.txt (+ .stderr/.exit/timestamp)
comparison.txt
command-analysis.txt
bar-analysis.txt
graphics-health-after.txt
regression-check.txt
result.md
```

## 10. Gate futuro
```text
MAC_GPU_PCI_READ_0 = PASS
PCI_CONFIG_READ_FOUNDATION_COMPLETE = YES
```
somente se:
```text
A SUCCESS + B SUCCESS
A vendor/device 10de:2504 + B vendor/device 10de:2504
IDENTITY_STABLE YES
COMMAND_REGISTER_UNCHANGED YES
MSE_UNCHANGED YES
BME_UNCHANGED YES
BAR_RAW_STABLE YES
OWN_CODE_PCI_WRITE NONE
BAR_MAPPING NO / MMIO NO / DMA NO / INTERRUPTS NO / RESET NO / GSP NO
AMD_GRAPHICS_AFTER HEALTHY / WINDOWSERVER_AFTER HEALTHY / PANIC NO / GPU_RESTART NO
```
Caso contrário PARTIAL/FAIL com causa exata.

## 11. Após PASS
```text
PCI config read foundation → COMPLETE → próxima fase = BAR mapping ARCHITECTURE/DESIGN
BAR_MAPPING_AUTHORIZED = NO
MMIO_AUTHORIZED = NO
```
Próxima fase desenha o BAR mapping; não implementa automaticamente.

(End of runbook — NÃO EXECUTADO)
