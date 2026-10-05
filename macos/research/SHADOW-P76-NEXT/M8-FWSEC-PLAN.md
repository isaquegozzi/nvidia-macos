# SHADOW — M8 FWSEC/FRTS/WPR2 sequencing PLAN (draft, NOT implemented, awaiting M7 verdict)
Spec: G0004 section 4 tail + protocol M8. Isolated dir SHADOW-P76-NEXT/fwsec-sim/.
1. Image inventory reusing M7 Inventory: FRTS (from VBIOS ROM region mock, 256B aligned),
   WPR2 meta struct {radix/sig/boot ranges + DMA addrs}, Radix L0/L1/L2, signatures blob,
   bootloader image, LibOS region descriptors.
2. Boot state machine: Unloaded -> FrtsStaged -> Wpr2Valid -> TablesBuilt -> BootloaderReady
   -> Booting -> Done | Failed(reason). Guards: each step requires prior terminal states of
   M7 transfers (all Done) + meta self-consistency (ranges inside staged images, no overlap).
3. Failure/rollback: chunk Timeout at any stage -> Failed(stage, bytesDone), staged buffers
   retained (pinned, no reuse), no auto-retry; fresh Sequencer object for retry (reboot model).
4. Evidence map file: Nova gsp/boot.rs + falcon.rs + fb.rs items used (function names +
   field semantics), OpenRM kernel_gsp.c items, Nouveau items; each mapped to a model
   element or marked NOT-MODELED with reason. No code copied (semantics only).
5. Tests: happy full sequence; meta-corrupt matrix (bad range, overlap, wrong align);
   timeout at each stage; double-boot Busy; 5k deterministic fuzz over image sizes/faults.
6. Mutations (-D): SKIP_META_CHECK, ALLOW_OVERLAP_RANGES, RETRY_AFTER_FAIL,
   SKIP_STAGE_ORDER, IGNORE_TIMEOUT. All must be CAUGHT.
