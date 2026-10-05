# P82.1 — Executability Verdict (respostas obrigatórias + Caso A/B)

```text
SESSION_ID: P82-1-AUDIT-RELAY-V3
MILESTONE: P82_1_EXECUTABILITY_AUDIT_C (OFFLINE ONLY, só leitura de source)
STATE: MUSE_WORK / ACTOR: MUSE_BUILD
P82_LIVE_EXECUTION_AUTHORIZED = NO (live=0; nada executado, nenhum source modificado)
```

## 1. Respostas obrigatórias (com prova file:symbol/line/hash)

```text
C_EXECUTABLE_ON_CURRENT_1_5_3 = NO
CURRENT_1_5_3_HAS_GFW_READINESS_SELECTOR = NO
CURRENT_1_5_3_SELECTOR_NUMBER = NONE (Count = 8, válidos 0-7; 8 rejeitado com kIOReturnBadArgument)
CURRENT_1_5_3_EXACT_PATH = GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/
CURRENT_1_5_3_HASH = GA106LabProtocol.h:2e39a274a744a299dea6a16ca1b55e39776c7bdc131c56c2f588b7150732b0c6; GA106LabUserClient.cpp:aed3d33b715c046c7601eed004920823ebb7bbc13baa0d95c090e9db871a6a81; GA106Lab.cpp:bec65dcd02bae6dc5ead613eadce42e155497609ff7e11e551b7113a5728f5f7; Info.plist(CFBundleVersion 1.5.3):4e9d6335a90021a34367e2538069ab6a776f1c09e3ab7793833c587e10c25aee
C_DESIGN_REFERENCES_1_6_2_OR_1_7_X = YES
STAGE_OR_LOAD_REQUIRED_FOR_C_AS_DESIGNED = YES
```

Provas linha-a-linha:

- `GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/GA106LabProtocol.h:23-31` —
  enum termina em `kGA106LabSelector_PrepareGspSysmem = 7`,
  `kGA106LabSelector_Count = 8`. Não há `ReadGfwBootReadiness`.
- `GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/GA106LabProtocol.h:20` —
  `GA106LAB_DRIVER_VERSION_U32 0x00010503u /* 1.5.3 */`;
  `Info.plist:17-20` — `CFBundleVersion 1.5.3`.
- Ausência total: `grep -rn "Gfw|gfw|118128|118234"` na árvore 1.5.3 = zero
  ocorrências (nenhum símbolo, offset ou comentário GFW).
- Barreira de dispatch: `GA106Lab-kext5-gsp-sysmem-alloc-testpath-fix-src/GA106LabUserClient.cpp:404`
  `if (selector >= kGA106LabSelector_Count) return kIOReturnBadArgument;`
  e tabela `sMethods[]` (`:312-324`) com exatamente 8 entradas (0–7).
  Invocar selector 8 no residente = `BadArgument`, não "read-only silencioso".
- Nascimento do selector 8: `GA106Lab-kext6-gsp-gfw-readiness-src/GA106LabProtocol.h`
  (`0x00010600`, Count = 9) — primeira aparição em 1.6.0; forma estável em
  `GA106Lab-kext6-gsp-gfw-readiness-userclient-fix-src/GA106LabProtocol.h:31-32`
  (`ReadGfwBootReadiness = 8`, Count = 9, `0x00010602`, `Info.plist 1.6.2`).
- O próprio P82 admite a geração: `P82-CANDIDATE-COMPARISON.md` (opção C) —
  "selector8-classe … pacote 1.6.2/1.7.x nunca foi live"; `CURRENT-STATE.md` —
  "1.6.2 selector8 GFW readiness offline PASS, nunca live" e
  "selector8 GFW readiness OFFLINE candidate / NOT live".
- Residente = 1.5.3: `CURRENT-STATE.md` (`1.5.3 … START_SUCCESS`, selector 7 1x
  live; EFI `1.5.3 Enabled = true`).

## 2. Decisão obrigatória: CASO B

```text
C_CURRENT_DESIGN = NO-GO / NOT_EXECUTABLE_IN_CURRENT_LIVE_STATE
STAGE_OR_LOAD_REQUIRED_FOR_C_AS_DESIGNED = YES
```

(Caso A exigiria `C_EXECUTABLE_ON_CURRENT_1_5_3 = YES` — refutado acima. Não há
semântica equivalente em 1.5.3: nenhum selector 0–7 lê `0x118128/0x118234` ou
reporta `gateReady/progress`; os leitores MMIO do 1.5.3 são BOOT_0 @0x0 e
BOOT_42 @0xA00 — registradores e semântica distintos.)

Consequência imediata: **"read-only" NÃO dispensa o load.** Para executar C como
desenhada seria preciso stage + load (e futuro unload/teardown T3/T4) de binário
1.6.2/1.7.x/1.8.0 — classes `KEXT_STAGE` / `KEXT_LOAD` / `KEXT_UNLOAD` / `SELECTOR_LIVE`,
todas 26/26 NO (`P81-AUTHORIZATION-MATRIX.md`, `P82-AUTHORIZATION-MATRIX.md`),
`STAGING_1_7_0_AUTHORIZED = NO`. C-como-desenhada = NO-GO no estado live atual pela deriva do Caso B acima (enum + barreira + stage/load não autorizado) — deriva que dispensa Q10. (Q10 = BLOCKED citado só como contexto ortogonal; definição em `P82-FIRST-REAL-GPU-VALIDATION-DESIGN/P82-Q10-RESOLUTION.md`: sem método read-only para provar identidade do binário carregado; cf. VERDICT §3 ROUTE_1 e OPEN-QUESTIONS Q3.) NADA será implementado ou executado neste turno (live=0).

## 3. Rotas futuras (DESIGN-ONLY, comparação — NÃO implementar, NÃO executar)

### ROUTE_1 — outra validação GPU-específica já presente em 1.5.3, com evidência NOVA

Candidatos já residentes (Count = 8, sem stage/load novo):

- Selector 5 (`ReadFirstMmioRegister`, BOOT_0 @BAR0+0x0, 1 load, copyout 48B).
- Selector 6 (`ReadStaticIdentity`, BOOT_42 @BAR0+0xA00, 1 load, copyout 32B).

Por que ROUTE_1 é a única direção compatível com o estado live:

- Zero stage/load: o binário já residente contém o caminho; a barreira
  `selector >= Count` não se aplica (5, 6 < 8).
- Evidência NOVA real sobre a GPU: ambos fazem load MMIO em BAR0 (primeiro
  contato driver↔HW além de PCI config), respondendo "o caminho MMIO do
  residente funciona?" — pergunta distinta de presença-OS-level.
- Risco menor que C: 1 load único, sem polling (C = até 8000 loads + deadline
  4 s + mapa retido por até 4 s); sem gate/progress semântico de firmware.

O que ROUTE_1 ainda exigiria (não existe hoje — gaps duros, ver GATE-STATUS):

- Auditoria P-D4.0..P-D4.5 do caminho 5/6 **na árvore 1.5.3 exata**
  (hashes §1 acima), publicada + review + hash pinado — inclusive prova de
  ausência de clear-on-read em BOOT_0/BOOT_42 e análise locks/IRQ do caminho
  1.5.3 (que NÃO tem o pin atômico do 1.6.2+).
- P-D1 (Option 0 PASS non-target), P-D2 (reviewer + alternativa), P-D3
  (foto/watchdog/serial), P-D5 (D PASS + baseline HW), P-D6 (autorização
  dedicada) — todos ainda vermelhos.
- Q10 segue BLOCKED: ROUTE_1 provaria comportamento do caminho, nunca
  identidade do binário.

Veredito de design: ROUTE_1 = **única rota futura elegível sem stage/load**,
mas hoje = NO-GO (gates P-D1..P-D6 vermelhos + auditoria 1.5.3 inexistente).
Requer milestone de design próprio antes de qualquer janela.

### ROUTE_2 — milestone separado para primeiro stage/load controlado

Se a pergunta GFW-readiness (gate `0x118128` / progress `0x118234`) for
insubstituível, o preço é um milestone próprio de primeiro stage/load, com:

- Escolha de geração justificada em design: **preferir 1.8.0 se justificável**
  (arquitetura de produção: 13 hardware blocks fail-closed, `SELECTOR9-ABI`,
  `LIFETIME.md`, `GATE-A-IMPLEMENTATION.md`, phase/contracts —
  `GA106Lab-kext8-production-architecture-src/`), ou 1.6.2 mínima (menor delta
  sobre o residente: só +selector 8) se o delta menor vencer no review.
  1.7.x carrega o Gate A / flush-prewrite nunca-live e o histórico Astra
  (1.7.0 FAIL D, 1.7.1 reaudit pendente) — ônus a declarar.
- Pré-requisitos: Option 0 PASS + D PASS + review dedicado de stage/load +
  rollback ensaiado (EFI-BACKUP-PROMPT46/PROMPT50) + autorização humana
  dedicada de stage/load (forma a pinar, nunca herdada) + Q10 reaberto ou
  explicitamente aceito como BLOCKED com interpretação limitada.
- Ordem: stage/load SÓ depois de ROUTE_1 esgotada ou declarada insuficiente
  para a pergunta — nunca como primeiro passo.

Veredito de design: ROUTE_2 = **milestone separado, fora de P82**; P82 não o
autoriza nem o desenha além deste parágrafo. Nenhuma rota será implementada
ou executada aqui.

## 4. Frase de bloqueio (para o INDEX do reviewer)

C-como-desenhada exige binário que não está carregado; invocar selector 8 no
residente 1.5.3 retorna `kIOReturnBadArgument` — não é "read-only seguro", é
chamada inválida. O load necessário é a parte mais arriscada do experimento
(teardown T3/T4, Q10, rollback), e nenhuma auditoria de "zero writes" a dispensa.
