# M10 channel/runlist evidence-sourcing survey — MODEL DEFERRED (no fabrication)

Surveyor: Muse, 2026-09-09 02:53 -0300. Read-only survey; no model built, no code written.

## Verdict
A host-only channel/runlist descriptor model is DEFERRED: local evidence is
insufficient to ground executable semantics. Building one now would be fabrication.
This file records exactly what exists, what is missing, and what would unblock M10.

## What exists locally (all in GSP-NEXTSTEP-P65/)
1. `UPSTREAM-EVIDENCE/nova-core-gsp-fw.rs:276,315` — `MsgFunction::AllocChannelDma`
   (= `NV_VGPU_MSG_FUNCTION_ALLOC_CHANNEL_DMA`): proves the function code exists and
   round-trips through TryFrom. Nothing about its payload layout.
2. Same file ~:82 — `client_alloc_size()`: per-channel heap reservation
   (`GSP_FW_HEAP_PARAM_CLIENT_ALLOC_SIZE` aligned to `GSP_HEAP_ALIGNMENT`). Constant
   VALUES not present locally (bindings only referenced by name).
3. `FIRST-RPC.md` — sequencing constraint: ALL RM allocs (hence any channel alloc)
   come strictly after first boot (`GspInitDone` + `GetGspStaticInfo`). Channel work
   is therefore post-boot scope, correctly ordered after M7/M8/M9.

## What is missing (all required before any model)
- Channel descriptor / alloc payload layout (no struct locally).
- Runlist format and semantics: zero hits — `grep -ril runlist UPSTREAM-EVIDENCE/`
  returns nothing; no FIFO/channel-scheduling source vendored.
- Heap constant values (`GSP_FW_HEAP_PARAM_*`, `GSP_HEAP_ALIGNMENT`).
- Any sequencer/RPC example carrying a channel payload.

## Unblock criteria for M10
Vendor (semantics only, no copying) ONE of: Nova `driver/gpu/nova-core/fifo` or
equivalent channel/runlist source; or OpenRM `nv04_fifo`/` equichannel` alloc path
with payload layouts; then model {descriptor validation, runlist slot rules,
error paths} with tests + mutations per house practice. Until then: no M10 model.

## Deliberately NOT done
No invented offsets, no guessed state machines, no placeholder "model" that tests
would merely restate. The M4–M9 bar (grounded semantics + caught mutants) applies.
