# P83 — Privilégio (UserClient root-only; sem senha no executor)

```text
SESSION_ID: V4-P83-GETVERSION / LIVE_EXECUTION_AUTHORIZED = NO
REGRA: executor headless NUNCA pede, recebe, digita ou armazena senha/sudo.
```

## Fato

Abertura de `IOUserClient` de driver de GPU é historicamente root-only: `IOServiceOpen`
contra o serviço GA106Lab exige privilégio. O executor opera sem privilégio e sem TTY
para `sudo`; portanto há dois caminhos mutuamente exclusivos, decididos no momento
pós-APPROVE por UMA checagem não-interativa.

## Caminho A — DIRECT (tentar primeiro)

1. Executar open/selector-0/close SEM `sudo` (plano `P83-INVOCATION-PLAN.md` T+01..T+03).
2. Se `T+01 open kr == 0` → segue DIRECT até T+05. Fim.
3. Se `open` retorna `kIOReturnNotPrivileged`/`kIOReturnNotPermitted` → ABORT do caminho A,
   registrar `kr`, e publicar marcador (sem nova tentativa):

```text
READY_FOR_HUMAN_GETVERSION (DIRECT negado por privilégio; sem senha pedida ou usada)
```

## Caminho B — HUMAN_SUDO (somente se A negado; executor PARA no marcador)

Pré-condição: review APPROVE + caminho A negado com `kr` registrado. O executor:

1. NÃO executa `sudo`, NÃO pede senha, NÃO lê stdin/TTY.
2. Imprime o comando exato byte-for-byte abaixo para o humano colar, executa NADA,
   e para em:

```text
READY_FOR_HUMAN_GETVERSION
```

3. Comandos exatos (snapshots read-only que emolduram a chamada; o humano cola na ordem):

```text
sudo /usr/sbin/kextstat -l | /usr/bin/grep GA106Lab
sudo /bin/ls -lT /Library/Logs/DiagnosticReports | /usr/bin/head -n 20
```

4. Linha do selector-0 — CALLER PINADO (source 1.8.0; path instanciado gate-close 2026-09-09):
   - Caller mínimo: `ga106ctl`, subcomando exato `version` (`argc == 2`, `argv[1] == "version"`).
   - `APPROVED_GA106CTL_ABS_PATH` (instanciado F1, sem placeholder): `~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl`.
   - `NEW_CLI_SHA` (FULL 64-hex, F2): `9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840` — gate exige igualdade FULL de `shasum -a 256` do binário acima contra este valor (sem prefixo, sem `…`); binding source↔hash em `MANIFEST-P80-1.8.0.txt:5` / `P80-FINAL-HANDOFF.md:17`.
   - Source (re-checado byte-for-byte F6, 2026-09-09): `openService` (`ga106ctl.c:20-52`, `IOServiceOpen` em `ga106ctl.c:40`) →
     `cmdVersion` (`ga106ctl.c:54-74`, `IOConnectCallMethod(conn, kGA106LabSelector_GetProtocolVersion,
     NULL,0,NULL,0,out,&outCount,NULL,NULL)` em `ga106ctl.c:63-65` com `outCount = 2` em
     `ga106ctl.c:56-57`) → `IOServiceClose(conn)` INCONDICIONAL em `ga106ctl.c:66`
     (antes do check de `kr` em `ga106ctl.c:67`) → print em `ga106ctl.c:71-72`.
   - Sem loop/retry/outro selector neste `argv`: `argc != 2` → `usage` (`ga106ctl.c:382-383`, exit 2 definido em `ga106ctl.c:377`);
     `argv[1] == "version"` → `cmdVersion()` (`ga106ctl.c:385-386`); qualquer outro `argv[1]` cai em
     outro ramo ou `usage` (`ga106ctl.c:388-420`). Chamada única, retorno 1 em falha (`ga106ctl.c:60-70`).
   - `CALLER_PINNED = YES` (nível source + path + hash FULL). Formas byte-for-byte:
```text
~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl version
sudo ~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl version
```
   - DIRECT usa a 1ª forma (sem `sudo`); HUMAN_SUDO usa a 2ª (só após DIRECT negado).
     Sem args extras, sem pipes, sem outro subcomando. Stdout esperado:
     `protocol: 1` + `driver: 0x00010800`.
   - CONDIÇÃO DURA (INSTANCIADA): APPROVE exige `shasum -a 256` do binário acima com igualdade FULL 64-hex contra `NEW_CLI_SHA` ANTES de qualquer passo live; sem match FULL, NENHUM APPROVE pode sair e o caminho B NÃO prossegue além dos 2 snapshots do item 3.

## Anti-regras

- Sem `echo <senha> | sudo`, sem `SUDO_ASKPASS`, sem TTY, sem cache de credencial.
- Sem adivinhar senha, sem retry com `sudo` pelo executor, sem escalar para outro selector.
- `LIVE_EXECUTION_AUTHORIZED = NO` até APPROVE; este arquivo não autoriza nada.

```text
live = 0 / senha tocada = 0 / sudo executado pelo executor = 0
```
