> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# BARE-0 failure analysis — boot 20:14:36 D27E9E45 (SOMENTE LEITURA, 2026-09-05)

> Boot BARE (sem `-wegnoegpu`, sem `disable-gpu`) NÃO chegou ao desktop. Config HIDDEN
> restaurada e boot atual saudável. Nada alterado nesta fase. Repo NTFS read-only →
> draft em `~/Documents/nvidia-macos-BARE0/bare0-failure-analysis.md`; destino final
> `docs/macos/hardware/bare0-failure-analysis.md`.

## Linha do tempo provada (Persist store)

```text
20:05:52  config BARE gravada (SHA 3675d8f4)
20:10     shutdown limpo do boot saudável (cause 5; shutdown_stall 20:10:32)
20:14:36  BARE boot start (UUID D27E9E45), boot-args SEM -wegnoegpu (confirmado no log)
20:14:37  Found + Probing 10de:2504 (030000) e 10de:228e — SEM removing child
20:14:38  childPublished VGA(1:0:0) + HDAU(1:0:1) → IOPCIDevice CRIADO
20:14:40  loginwindow spawnado e running; DumpPanic: sem panic anterior, sem corefile
20:14:42  childPublished GFX0(1:0:0) — VGA renomeado p/ GFX0 (stack gráfico assumiu)
20:14:43  GPUWrangler registra RTX (DG,pubArmed→published) e AMD (driversStarted+accel+3 fbs)
20:14:45  WindowServer display WillReconfigure + DidReconfigure (displays reconfigurados)
20:15:09  displaypolicyd[141] exit(1) após 29s; respawn [413] re-registra ambas as GPUs
20:15:16  WindowServer re-init + MTLCompilerService p/ WindowServer + loginwindow agents
20:15:44  ÚLTIMO evento Persist (secd spawn); silêncio total até o boot 23:28
23:28     boot saudável restaurado (HIDDEN de volta), shutdown anterior cause 5 (limpo)
```

Zero `.panic`, zero NVDA/GeForce touches, zero erros BAR. Morte = freeze duro ~70 s após
loginwindow, em userspace com WindowServer + compilação Metal em voo.

## Observação visual humana (boot BARE, incorporada 2026-09-06)

```text
DISPLAY_OBSERVATION:
- Monitor A (AMD/verbose): texto verbose continuou por várias linhas e congelou
  numa linha não registrada (sem foto da última linha)
- Monitor B (RTX/HDMI?): permaneceu no splash/logo do firmware da placa-mãe,
  nunca assumido pelo macOS — nenhuma GUI visível tomou aquela saída
```

Leitura honesta: compatível com falha de handoff gráfico/WindowServer (o WindowServer
reconfigurou displays às 20:14:45 no log, mas a GUI nunca apareceu de forma visível;
o verbose congelado indica kernel/userspace parados, não panic com tela de erro).
NÃO prova sozinho a causa — freeze com verbose parado também ocorreria num deadlock
fora do stack gráfico. Peso: corrobora H2 sem fechar o caso; H2 segue não-CONFIRMED.

## Classificações pedidas

- `PANIC_REPORT = NOT_FOUND` (nenhum `.panic`; DumpPanic do BARE: sem corefile)
- OpenCore log em arquivo: NOT_FOUND (EFI sem `opencore-*.txt`; `Misc→Debug`: AppleDebug
  false, ApplePanic false, Target 3, DisplayLevel 0x80000002)
- `RTX_BARE_ENUMERATION = GRAPHICS_STACK_PROBE_STARTED` (Found → IOPCIDevice → GFX0 →
  GPUWrangler published como DG discreta; além de CREATED e PROBE_STARTED)
- Estágio máximo: `STAGE_5 (userspace/WindowServer)`, `CONFIDENCE = HIGH` (loginwindow +
  WindowServer + reconfigure + MTLCompiler provados no log)
- Áudio `10de:228e`: publicado como HDAU às 20:14:38, nenhum erro/attach depois —
  falha NÃO distinguível pelo áudio, mas nenhum sinal contra ele; GPU levou toda a
  atenção do stack gráfico
- `PCI_RESOURCE_FAILURE = NOT_OBSERVED` (só falsos positivos de grep: XPC BARRIER,
  `bar2` de APFS, `signalBars`; nenhuma queixa de bridge window/BAR1 16G/Above4G)
- `GRAPHICS_STACK_INTERACTION = CONFIRMED` (GFX0 rename + GPUWrangler DG published +
  displaypolicyd exit(1)+respawn + WindowServer reconfigure + MTLCompiler)
- `AMD_INIT_FAILURE = NOT_OBSERVED` (AMD driversStarted, accel Vega10, fb0/1/2, PMRD
  tags, reconfigure postado; NootedRed silencioso e saudável)

## H1–H5

| Hipótese | Veredito | Evidência |
|---|---|---|
| H1 PCI/resource allocation | UNLIKELY | Sem erros BAR/bridge; publish limpo; freeze 90 s depois em userspace, não no PCI config |
| H2 graphics-class probing | STRONGLY_LIKELY (não CONFIRMED) | RTX driverless publicada como DG → displaypolicyd exit(1) → WindowServer/Metal engajam → freeze sem panic + verbose parado + splash da placa-mãe nunca assumido. Último subsistema ativo tocando estado GPU; observação visual corrobora handoff gráfico falhado |
| H3 dual-GPU/NootedRed | UNLIKELY | AMD 100% saudável (driversStarted/accel/fbs/reconfigure); sem erros AMD |
| H4 áudio/multifunction 228e | UNLIKELY | HDAU quieto, sem attach/erros; atenção do stack foi toda na função .0 |
| H5 outro (ex.: PM/wait em device DG sem driver) | POSSIBLE | Resíduo honesto: hang sem rastro pode ser policy-wait; indistinguível de H2 sem instrumentação |

## Causa mais provável (H2) e confiança

A RTX exposta como GPU discreta publicada sem driver entrou nas decisões de política do
stack gráfico (GPUWrangler/displaypolicyd/AGDP/WindowServer/Metal) e o boot travou em
userspace sem panic. `CONFIDENCE = MEDIUM-HIGH` — cadeia de logs forte + observação
visual compatível (verbose parado, splash nunca assumido), mas hang duro não deixa
smoking gun e a última linha do verbose não foi registrada; H1/H3/H4 desfavorecidas
por ausência de sinais. NÃO CONFIRMED.

## UM próximo experimento (não executar): C (classe diagnóstica FF0000)

**C — expor o PCI visível evitando o matching gráfico**: spoof de `class-code` da RTX
(030000→FF0000, Unassigned class — NÃO 028000, que é network e atrairia outro stack)
via DeviceProperties no mesmo caminho PCI, mantendo todo o resto do BARE
(sem `-wegnoegpu`, sem `disable-gpu`, sem drivers, sem TinyGPU).
Se o boot passar e o PCI ficar legível, H2→CONFIRMED (ou VERY_HIGH_CONFIDENCE) e H1→FALSE, e ganhamos o lab BARE
(config legível + BARs mapeáveis; class-code spoof não esconde BARs). Se continuar
morrendo, H1 volta à mesa. Reversível (apagar 1 property), não toca AMD, protocolo de
observação do plano BARE (foto da tela + mesmos logs). D (dext TinyGPU) fica depois:
instalar driver antes de entender o hang é pular a fila e carrega o RISK do display.
