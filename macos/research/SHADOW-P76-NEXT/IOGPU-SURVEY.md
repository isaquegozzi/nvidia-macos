# IOGPU/IOAccelerator integration survey — RESEARCH DEFERRED (no fabrication)

Surveyor: Muse, 2026-09-09 03:08 -0300. Read-only survey; nothing designed, no code.

## Verdict
Integration research is DEFERRED: it is a post-live-display activity with no local
grounding available tonight. Designing interfaces now would be fabrication.

## What exists locally
1. Roadmap position only: `GA106Lab-kext7-sysmem-flush-prewrite-fix3-src/README.md:26`
   places IOGPU/IOAccelerator → MTLDevice as the FINAL stage after GPU bring-up →
   memory/queues → rendering. Every predecessor stage is itself pre-live.
2. Protocol backlog item 13 (`Tools/P76-Overnight-Relay/P76-RELAY-PROTOCOL.md:118`;
   also `Conversations/P76-PROTOCOL.md` backlog list).
3. No IOGPUFamily/IOAccelerator headers, integration code, or upstream evidence
   vendored anywhere in the workspace (only roadmap mentions found by grep).

## Unblock criteria (in order)
1. Live display output working through the current bring-up sequence (all prior
   stages are pre-live; nothing renders yet).
2. Apple IOGPUFamily + IOAccelerator headers/requirements sourced for the target
   macOS version, plus an integration design (family matching, command buffers,
   surface sharing) reviewed before any code.
Until then: no IOGPU model, no stubs, no placeholder interfaces.

## Deliberately NOT done
No invented family-matching tables, no guessed method signatures, no stub kext
code. The M4–M9 bar (grounded semantics + caught mutants) applies here too.
