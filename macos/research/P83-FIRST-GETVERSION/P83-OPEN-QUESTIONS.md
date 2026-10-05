# P83 — Open Questions (GetVersion)

```text
SESSION_ID: V4-P83-GETVERSION / STATE: MUSE_WORK / LIVE_EXECUTION_AUTHORIZED = NO
```

| ID | Pergunta | Status |
|---|---|---|
| Q-P83-01 | Binário-em-RAM == source auditado? (Q10 herdado) | PARCIAL (offline): source↔manifest fechado via `MANIFEST-P80-1.8.0.txt:1-3` + `P80-FINAL-HANDOFF.md:15-21` (32/32 OK por referência externa); BINÁRIO-EM-RAM segue BLOCKED até método aprovado — declarado, não fingido |
| Q-P83-02 | Caller userspace mínimo | PINADA (source+path+hash FULL): `~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl version` (`ga106ctl.c:54-74,380-386`; open `ga106ctl.c:40`, selector-0 `ga106ctl.c:63-65`, close incondicional `ga106ctl.c:66`) — ver `P83-PRIVILEGE.md` item 4; gate FULL: `shasum -a 256` == `9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840` |
| Q-P83-03 | Linha `sudo` do selector-0 byte-for-byte | PINADA (forma instanciada): `sudo ~/Mac/Documents/nvidia-macos-BARE0/GA106Lab-kext8-production-architecture-src/build/obj/ga106ctl version` (DIRECT: sem `sudo`; sem args extras/pipes) — ver `P83-PRIVILEGE.md` item 4; sem match FULL do hash, caminho B para nos 2 snapshots |
| Q-P83-04 | Timeout de 5s por passo é adequado para trap de UC sob carga? (P80: handlers pinados são bounded; GetVersion é o menor caminho) | ABERTA (reviewer) |
| Q-P83-05 | Snapshot pós deve incluir `dmesg`/spindump? (fora do escopo read-only atual; risco de superfície) | ABERTA (default: NÃO) |
| Q-P83-06 | Re-uso deste pacote para selectors 1..9 após PASS do selector 0, ou nova auditoria por selector? | ABERTA (default: nova auditoria) |

```text
live = 0 / nada bloqueia o review (perguntas acompanham o pacote, não o APPROVE automático)
```
