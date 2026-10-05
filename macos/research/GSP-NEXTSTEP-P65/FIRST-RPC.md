# P65 FIRST RPC (hipotese antiga rejeitada)

Hipotese antiga (NAO aceitar sem prova): `INIT -> INIT_DONE -> SET_PAGE_DIRECTORY -> RM_ALLOC...`

## Sequencia real (Nova-core, Ampere/GA106)

```text
FIRST_RPC_SEQUENCE =
  host->GSP: SetSystemInfo (GspSetSystemInfo; send_command_no_wait, sem reply esperado)
  host->GSP: SetRegistry (PackedRegistryTable; RMSecBusResetEnable=1, RMForcePcieConfigSave=1,
             RMDevidCheckIgnore=1 [+RMSetSriovMode se vGPU]; send_command_no_wait)
  host<->GSP: sequencer msgs (GspSequence via cmdq receive_msg 5s; comandos RegWrite/Modify/Poll/
             Delay/Store/Core*; CoreResume envolve ambos os falcoes)
  GSP->host: GspInitDone (MessageFromGsp; wait_gsp_init_done loop ate FUNCTION==GspInitDone)
  (so depois: GetGspStaticInfo e demais RM controls/allocs — fora do primeiro boot)
```

## Prova

- `gsp/boot.rs:boot`: `send_command_no_wait(SetSystemInfo)` + `send_command_no_wait(SetRegistry)` ANTES
  de `post_boot` (sequencer) e ANTES de `wait_gsp_init_done`. Nenhum `INIT`/`SET_PAGE_DIRECTORY`/
  `RM_ALLOC` neste caminho.
- `gsp/commands.rs`: `SetSystemInfo::FUNCTION=GspSetSystemInfo`, `SetRegistry::FUNCTION=SetRegistry`
  com entradas hardcoded acima; `GspInitDone::FUNCTION=GspInitDone` + `wait_gsp_init_done()` loop
  (ERANGE=>continue).
- `gsp/cmdq.rs`: transporte por filas compartilhadas com checksum + seq#, polling (sem IRQ).
- OpenRM `kernel_gsp.c` corrobora nomes `GspSetSystemInfo`/`SetRegistry` como "init RPC" preparadas em
  `_kgsp...` (linhas ~4463/~4637). Direcao: host->GSP as duas primeiras; GSP->host o INIT_DONE.
