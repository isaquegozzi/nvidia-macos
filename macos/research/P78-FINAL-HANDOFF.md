> Registro histórico importado em 2026-10-05. Descreve a fase indicada no próprio documento; não comprova o estado atual da máquina. Consulte [o estado consolidado](../../docs/macos/STATUS-ATUAL.md).

# P78-FINAL-HANDOFF — VM/MMU (APPROVED_AND_CLOSED)

Date: 2026-09-09 (America/Sao_Paulo)
Canonical root: `~/Mac/Documents/nvidia-macos-BARE0`
Prompt: PROMPT-79 preamble (P78 APPROVE path)
Candidate: 1.7.3 TG-KEXT7-SYSMEM-FLUSH-PREWRITE-ASTRA-FIX3 (FROZEN, offline, read-only)

## 1. Gemini verdict received

Source: P79 prompt canonical-state declaration (no material findings):

```text
GEMINI_VERDICT = APPROVE
```

No `NEED_EVIDENCE`, no `REJECT`. P78 review package
(`P78-GEMINI-REVIEW/` + `Conversations/P78-REVIEW-START-HERE.md`) frozen unchanged.

## 2. Candidate 1.7.3 re-verification (2026-09-09, read-only)

```text
KEXT_SHA     = 39b25bb9a3c469a74e1c42bf64417027b280cab8bbe870462c508e47cce08c51
PLIST_SHA    = fa32254e0a7e266e5f22431fd65a3a3d81cff8122739021ecc278257ed6673b0
CLI_SHA      = 9ab351f5fab72ffe7daa5fe1822579e5abd66dcd0f23a1a25b39a4bdc34fe840
MANIFEST_SHA = f42487ce0d5a083c56189e63a53983716c6a35db9ca2ee8e83a3abdf0a091893
CANDIDATE_UNCHANGED = YES
```

Full manifest: 17/17 OK. No write to candidate tree.

## 3. What was approved (scoped VM/MMU model)

- VA space 47-bit + managed areas + server-reserved hole; PDE/PTE gp100-line layout
  (VALID/aperture/VOL/PRIV/RO/atomic/kind/comptag); 3-layer aperture map; page sizes
  4K/64K/2M/512M leaf + dir-only upper; permissions; 12 guard-level fault classes;
  mapping lifecycle with flush-token ordering; GSP VM RPC (VASpace alloc, reserved-PDE
  copy, page-directory bind).
- Model: `SHADOW-P78-VM-MMU-SIM/` (238/238 PASS, ASAN+UBSAN clean, 10/10 mutants caught,
  `clang --analyze` clean, 48-combo deterministic oracle).
- Open gaps carried: HW fault-packet wire bytes, kind meanings, comptag allocation,
  BAR2_PDB policy, COPY_PDES aperture label, external VASpace VA policy, GH100+ excluded
  (see `P78-VM-MMU-EVIDENCE/OPEN-QUESTIONS.md`).

## 4. Safety / live counters

```text
LIVE_ACTIONS = 0 / EFI_ACTIONS = 0 / KEXT_LIVE_ACTIONS = 0 / MMIO_WRITES = 0
PCI_WRITES = 0 / BME = 0 / GPU_DMA = 0 / GSP_LIVE = 0 / VRAM_WRITE = 0
COMMAND_SUBMISSION_REAL = 0
```

Only read-only `shasum`/`cat`/`ls` plus file creation for this handoff record.

## 5. Final report

```text
P78_FINAL_STATUS = APPROVED_AND_CLOSED
GEMINI_VERDICT = APPROVE
CANDIDATE_1_7_3_UNCHANGED = YES
NEXT = P79_OFFLINE_COMMAND_SUBMISSION_MODEL
```

STOP P78. P79 authorized offline only.
