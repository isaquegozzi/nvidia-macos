# P81.2 Offline Rehearsal Harness (Relay V3-P81.2, headless)

```text
HARNESS_IS_NOT_LIVE_EVIDENCE = YES
GPU_STATE = UNKNOWN
LIVE_EXECUTION_AUTHORIZED = NO
SESSION_ID = P81-2-HARNESS-RELAY-V3
```

## What this harness proves

SOMENTE o pipeline, OFFLINE, contra fixtures sintéticas MOCK:

- runbook order (`IDENT-1..4, EVENT-1/2, FOTO-meta, INDEX, CLOSE`)
- bounds (timeout <= 60s, retry <= 1, destino >= 5GB, clock gate G-M0244-01-R1)
- SAVE / INDEX / SHA256 / CLOSE / FAIL / ABORT / BLOCKED / recovery shape
- fail-closed semantics nos cenários S01–S20 + mutations (kill-rate 100%)

## What this harness does NOT prove (não é evidência live)

macOS live, RTX/GA106Lab state, KEXT safety, panic-rate, MMIO/BME/DMA/GSP/VRAM,
command/Metal, destino real, clock real. `live=0`. Nenhum comando de host é
executado — ioreg/kextstat/kmutil/kextcache/nvram/diskutil/log/system_profiler/
sntp/mount/reboot/shutdown/sudo aparecem SOMENTE como documentação ou fixture;
o motor usa `hashlib` stdlib e nunca `subprocess`.

## Layout

```text
README.md, run_rehearsal.py, fixtures/, expected/, results/, tests/
P81_2-THREAT-MODEL.md, P81_2-TEST-MATRIX.md, P81_2-PASS-FAIL-ABORT.md, P81_2-LIMITATIONS.md
```

## Run

```text
python3 run_rehearsal.py            # roda S01-S20 + mutations, escreve results/
python3 run_rehearsal.py --self-test  # exit!=0 se mismatch ou kill<100%
python3 -m unittest discover -s tests -v
```

Passa SOMENTE se S01=PASS, S02–S05/S11–S15/S18/S20=FAIL(-3), S06–S10/S16/S17=ABORT,
S19=BLOCKED, e `MUTATION_KILL_RATE = 100%`. Senão `P81_2 = REJECT`.
