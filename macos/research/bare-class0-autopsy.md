> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# BARE-CLASS0 autopsy — boot 23:41:43 UUID 946B04AB (SOMENTE LEITURA)

> BARE-CLASS0 (`-wegnoegpu` ausente + `disable-gpu` ausente + `class-code=FF0000` na RTX)
> também falhou: monitor principal PRETO, verbose parado (foto: IOVersatile/
> OSBundleLibraries + lfs/methodOpenKernelFD), sem desktop, sem panic visual.
> Config saudável restaurada. Nada alterado aqui. Destino final:
> `docs/macos/hardware/bare-class0-autopsy.md`.

## Etapa que diverge do HEALTHY (resposta direta)

HEALTHY: `Found 10de → removing child → boot continua` (Lilu remove no bridge).
BARE e CLASS0: `Found 10de → SEM removing → IOPCIDevice publicado → GPUWrangler
publica 2ª DG → displaypolicyd exit(1) em loop → freeze sem panic`.
Primeira divergência do HEALTHY = **ausência do `removing child`** (efeito pretendido);
primeira divergência patológica = **displaypolicyd exit(1) cíclico**, ausente no HEALTHY.

## Tabela comparativa

| stage | HEALTHY (23:48 2A4EE7FD) | BARE (20:14 D27E9E45) | BARE-CLASS0 (23:41 946B04AB) |
|---|---|---|---|
| PCI enumerate | Found 10de:2504+228e → `removing child` no bridge | Found, SEM removing | Found, SEM removing |
| RTX childPublished | nenhum (removida) | VGA→GFX0 (1:0:0) + HDAU | VGA,VGA (1:0:0, sem rename GFX0) + HDAU |
| class-code | n/a | 030000 (HW, sem spoof) | HW 030000 no scan + `ff0000` injetado (efeito no matching NÃO observável no log) |
| GPUWrangler | só AMD | AMD + RTX DG published | AMD + RTX DG published (repetido a cada respawn) |
| AMD accel | driversStarted+accel+3fb | driversStarted+accel+3fb | driversStarted+hasGPUC (accel/fb não recapturados; reconfigure OK) |
| loginwindow | desktop | running | running + handshake RB |
| WindowServer | desktop | running, re-init, MTLCompiler | running, Will/DidReconfigure |
| displaypolicyd | estável, só AMD | exit(1)×1 (29s) + respawn; morte 35 s depois | exit(1) em LOOP ×4 (29,5→24→23 s) até o freeze |
| Metal/MTLCompiler | sim | MTLCompilerService p/ WindowServer | AGDP só na AMD; MTLCompiler não capturado |
| last event | desktop | 20:15:44 secd spawn (+68 s) | 23:43:29 bluetoothd (+106 s) |
| panic | none | none | none |

## Itens 2–11 (resumo)

- 10de:2504 enumerada nos 3 boots; CLASS0 sem `removing child` (spoof não recriou remoção).
- Classe efetiva no IORegistry: NÃO observável no log (scan mostra HW 030000; property
  injetada não é logada). `compatible pciclass,ff00` nunca apareceu no log.
- GPUWrangler PUBLICOU a RTX (DG) 4×; IOGraphics: sem GFX0 rename (diferença vs BARE);
  AGDP só na AMD (`agdpclient 0` na RTX); displaypolicyd tocou a RTX a cada ciclo.
- WindowServer + loginwindow iniciaram; AMD/NootedRed normais (driversStarted, reconfigure).
- BAR/resources: zero erros (NOT_OBSERVED, igual ao BARE).
- 228e: HDAU publicado e quieto; sem AppleHDA; sem papel distinguível.
- IOVersatile: 150 linhas CLASS0 = 150 BARE; 225 no HEALTHY atual, 30 em 2 min do 23:28.
  Falha de dependência rotineira em TODOS os boots, cedo (+7 s) e longe do stall (+106 s).
- Foto congela em linhas de +7 s mas o sistema logou até +106 s: verbose parado ≠ morte;
  morte real é o silêncio Persist após 23:43:29 com tela preta + splash ignorado.

## Reavaliação H1–H5

| Hipótese | Veredito | Porquê |
|---|---|---|
| H1 PCI/resources | UNLIKELY | Dois boots com publish limpo, zero erros BAR, morte tardia em daemon userspace |
| H2 graphics-class policy | LIKELY (rebaixada de STRONGLY_LIKELY; NÃO CONFIRMED) | Classe-display como causa SUFICIENTE perde força: com spoof a política engajou igual. Resta forte como "policy-vs-2ª-DG" |
| H3 dual-GPU interaction | UNLIKELY (definição AMD-regressão) | AMD nunca regrediu; mas o discriminador real é "2 DGs publicadas" — ver H5 |
| H4 áudio 228e | UNLIKELY | Quieto nos 2 boots |
| H5 other (policy não tolera 2ª DG sem driver; exit(1) intencional?) | POSSIBLE→LIKELY-adjacent | exit(1) limpo em loop com lifetimes decrescentes é assinatura de desistência de policy, não de fault |

```text
CLASS_SPOOF_APPLIED = UNKNOWN (mudança no config real; efeito no IORegistry/matching sem evidência no log)
RTX_STILL_ENTERED_GRAPHICS_STACK = YES (GPUWrangler DG + ciclos displaypolicyd; só o rename GFX0 faltou)
IOVERSATILE_RELEVANCE = UNLIKELY (ubíquo, precoce, mesma magnitude nos 3 boots)
```

## UM próximo experimento (NÃO executar): D-minimal

**D — dext PCIDriverKit mínima 10de-only que casa, abre e estaciona a RTX** (sem app
TinyGPU, sem compute, sem escrita MMIO além de map read-only + log): a autópsia mostra
morte por policy-vs-DG-sem-driver (exit 1 em loop); um driver vinculado muda exatamente
essa variável e, se estabilizar, entrega o lab PCI direto. Reversível (uninstall +
restaura config), AMD protegida por match só-10de, sem tocar NVRAM/ACPI/SMBIOS.
Runner-up (diagnóstico puro): spoof vendor-id (não classe) para testar se a policy é
keyed em vendor NVIDIA — menor que D, mas não avança o lab.
