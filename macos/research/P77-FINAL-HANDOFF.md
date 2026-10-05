> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# P77-FINAL-HANDOFF — M10 Channel/Runlist (APPROVED_AND_CLOSED)

Date: 2026-09-09 (America/Sao_Paulo)
Canonical root: `~/Mac/Documents/nvidia-macos-BARE0`
Prompt: PROMPT-77F-MUSE-AFTER-GEMINI (APPROVE path)
Candidate: 1.7.3 TG-KEXT7-SYSMEM-FLUSH-PREWRITE-ASTRA-FIX3 (FROZEN, offline, read-only)

## 1. Gemini verdict received

Source: user-pasted P77 review result (`P77_REVIEW_STATUS = COMPLETE`):

```text
VERDICT: APPROVE
```

No `NEED_EVIDENCE`, no `REJECT`. No material finding open. Two informative notes only
(engine tokens opaque; ManagementOverhead overflow defense-in-depth) — both already
honestly recorded in `P77-M10-CHANNEL-EVIDENCE/OPEN-QUESTIONS.md`.

P78 NOT started (per 77F APPROVE path §5).

## 2. Candidate 1.7.3 re-verification (2026-09-09, read-only)

Command:

```sh
cd GA106Lab-kext7-sysmem-flush-prewrite-fix3-src
shasum -a 256 build/obj/GA106Lab.kext/Contents/MacOS/GA106Lab \
  build/obj/GA106Lab.kext/Contents/Info.plist \
  build/obj/ga106ctl MANIFEST-P76-1.7.3.txt
shasum -a 256 -c <(grep -v "^MANIFEST" MANIFEST-P76-1.7.3.txt)
```

Result (identical to `P77-GEMINI-REVIEW/REVIEW-INDEX.md` §3 and `P77-STATUS.md`):

```text
KEXT_SHA     = 39b25bb9a3c469a74e1c42bf64417027b280cab8bbe870462c508e47cce08c51
PLIST_SHA    = fa32254e0a7e266e5f22431fd65a3a3d81cff8122739021ecc278257ed6673b0
CLI_SHA      = 9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
MANIFEST_SHA = f42487ce0d5a083c56189e63a53983716c6a35db9ca2ee8e83a3abdf0a091893
CANDIDATE_UNCHANGED = YES
```

Full manifest: 17/17 OK (KEXT + PLIST + CLI + 14 source/build files).
No write to candidate tree. No git mutation (BARE0 has no `.git`; provenance is
content-hash, not VCS).

## 3. What was approved (scoped M10 model)

- `P77-M10-CHANNEL-EVIDENCE/`: CHANNEL-ALLOC PROVEN (class `0xc56f`,
  `NV_CHANNELGPFIFO_ALLOCATION_PARAMETERS` via `r535_chan_alloc`, BIND `0xa06f0104` +
  SCHEDULE `0xa06f0103` order); FIFO-LIFETIME PROVEN
  (Uninit→RpcReady→Allocated→Bound→Scheduled→Errored→Free, non-reentrancy,
  use-after-free guards); GSP-RPC-CHANNEL PROVEN (`GSP_RM_ALLOC=103`, legacy
  `ALLOC_CHANNEL_DMA=6` recognized-but-not-path, framing M9-compatible, sequencing
  after `GspInitDone 0x1001` + `GetGspStaticInfo 65`); HEAP CONSTANTS PROVEN
  (BASE_RM_TU10X 8 MiB, SIZE_PER_GB 96 KiB, CLIENT_ALLOC 96 MiB / 2048 ch,
  OS_LIBOS3_BAREMETAL 22 MiB, window [88 MiB, 280 MiB]).
- RUNLIST: abstraction PROVEN (`r535_runl.block/allow` empty no-op, scheduling via
  firmware BIND+SCHEDULE); entry-bytes deliberately NOT modeled (firmware-private).
- Model: `SHADOW-P77-M10-CHANNEL-SIM/` (`channel_sim.hpp` `e7554cfa…5a074`,
  `test-channel.cpp` `776593d7…fee3a`, harness `7539a276…ccf89`).
- Tests: 139/139 PASS (`build-mutations/BASELINE-RESULT.txt`); ASAN+UBSAN 139/139 PASS;
  `clang --analyze` exit 0, 0 diagnostics; mutations 10/10 CAUGHT
  (`MUTATION-RESULT.txt`); deterministic oracle (exhaustive 2048-chid sweep + USERD /
  doorbell / heap oracles, no randomness).
- Source pinning: 22-25 upstream blobs via `git hash-object` exact match per
  `SOURCE-MANIFEST.md`; cross-generation contamination NONE (Ampere `0xc56f`+LIBOS3
  isolated; TU102 MMIO corroboration non-executable; Hopper/GH100 excluded).

## 4. Open limitations (still open after APPROVE)

Mandatory carry-forwards:

- bytes internos / formato privado da runlist NÃO provados (GSP firmware-private;
  host-only math `chid/runqueue [0,1]/USERD page+index/doorbell` apenas; sem entry bytes).
- nenhum teste live (todos host-only/offline; `kextstat` confirma apenas baseline viva
  1.5.3; candidato 1.7.3 segue 100% offline).
- nenhum command submission real (GPFIFO emission, pushbuffer, fault/GR contents fora
  de escopo por `P77 §5/§10`).

Full remainder from `OPEN-QUESTIONS.md` (closers included there):

1. GSP-RM runlist entry bytes firmware-private — closer: NVIDIA publish or Nova `fifo/` GSP-runlist.
2. `NV2080_ENGINE_TYPE_*` numerics not pinned (opaque validated tokens `Gr0..Ofa`) — closer: vendor `cl2080.h`.
3. `inst/userd` byte counts opaque params (4K-align on inst proven) — closer: vendor `gf100.c`+`gv100.c`.
4. `SIZE_PER_GB_FB` vs `SIZE_PER_GB` naming drift (value identical 96 KiB).
5. GPFIFO emission out of scope.
6. fault/GR contents not modeled.
7. single-ChID-space assumption (per heap comment; per-runlist spaces would invalidate 2048 bound).

## 5. Safety / live counters

```text
LIVE_ACTIONS = 0
EFI_ACTIONS = 0
KEXT_LIVE_ACTIONS = 0
MMIO_WRITES = 0
PCI_WRITES = 0
BME = 0 / GPU_DMA = 0 / GSP_LIVE = 0 / FIRMWARE_UPLOAD = 0
VRAM_WRITE = 0 / INTERRUPT_RESET = 0 / COMMAND_SUBMISSION_REAL = 0
```

HARD SAFETY honored (sudo/reboot/shutdown/EFI-write/KEXT load-unload/selector-live/
MMIO-write/PCI-config-write/BME/GPU-DMA/GSP-live/firmware-upload/VRAM-write/
interrupt-reset/command-submission-real — none executed). Only read-only `shasum`/`cat`/`ls`.

## 6. Canonical state update

- This file (`P77-FINAL-HANDOFF.md`) is the canonical P77 closure record.
- `P77-GEMINI-REVIEW/P77-STATUS.md` (`COMPLETE`) + `REVIEW-INDEX.md` unchanged (frozen review package).
- `P77-M10-CHANNEL-EVIDENCE/` + `SHADOW-P77-M10-CHANNEL-SIM/` unchanged (frozen evidence/model).
- `Conversations/P77-REVIEW-START-HERE.md` unchanged (no rereview needed on APPROVE).
- `CURRENT-STATE.md` left untouched (still at P72 sync, stale at 1.7.1) — superseded for
  P77 scope by this file; full CURRENT-STATE refresh deferred to P78 definition (not started).

## 7. Final report (77F APPROVE format)

```text
P77_FINAL_STATUS = APPROVED_AND_CLOSED
CANDIDATE_1_7_3_UNCHANGED = YES
GEMINI_VERDICT = APPROVE
OPEN_LIMITATIONS = runlist-private-bytes-unproven; no-live-tests; no-real-command-submission; + OPEN-QUESTIONS 1-7 (engine-numerics-opaque, inst-userd-sizes-opaque, SIZE_PER_GB-naming-drift, GPFIFO-emission-excluded, fault-GR-contents-excluded, single-ChID-space-assumption)
NEXT = WAIT_FOR_P78_DEFINITION
```

STOP. P78 definition awaited; no new milestone started.
