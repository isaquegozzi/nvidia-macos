# P77 — Open Questions (honest remainder; no fabrication)

1. **GSP-RM runlist entry bytes** — firmware-private. No public header describes the
   in-memory runlist entry for the GSP path. The host programs scheduling via
   `GSP_RM_ALLOC` + `NVA06F BIND/SCHEDULE`; entries themselves are invisible.
   Status: deliberately NOT modeled. Closes only if NVIDIA publishes the format or
   Nova gains a `fifo/` GSP-runlist implementation.
2. **`NV2080_ENGINE_TYPE_*` numeric encodings** — `cl2080.h` was rate-limited during
   fetch. The xlat TABLES (names → opaque) are proven in `r535/r570_fifo_xlat`, but the
   u32 wire values are not pinned here. Model treats engine as a validated token set
   `{GR0,COPY0-7(+8-19 r570),NVDEC0-7,NVENC0-3,NVJPG0-7,SW,SEC2,OFA(0-1)}` without
   hardcoding numbers. Closes by vendoring `cl2080.h` + `nvrm/engine.h`.
3. **`inst_size` / `userd_size` byte counts** — `gf100_chan_inst` / `gv100_chan_userd`
   externs proven, but their `.size` initializers live in other Nouveau TUs not pinned
   in this round. Model takes them as positive opaque parameters (inst 4K-aligned per
   `chan.c:409`). Closes by vendoring `nvkm/engine/fifo/gf100.c` + `gv100.c` (or equivalents).
4. **`GSP_FW_HEAP_PARAM_SIZE_PER_GB_FB` vs `SIZE_PER_GB` naming** — Nova uses the `_FB`
   suffix, OpenRM `main` uses the shorter name with identical semantics (96 KiB/GB).
   Value proven; symbol rename noted as drift, not a second constant.
5. **GPFIFO entry / pushbuffer emission** — `NVA06F_DMA_*` formats (`cla06f.h`) describe
   pushbuffer methods, but emission is command-submission scope (post-M10, pre-IOGPU).
   Explicitly excluded per P77 §5/§10.
6. ** fault method buffer contents / GR ctx contents** — sizes/placement proven
   (`mthdbuf_size` ctrl, `PROMOTE_CTX` struct), contents are engine-private. Not modeled.
7. **Multi-runlist / per-runlist ChID spaces** — `gsp_fw_heap.h` comment notes GSP-RM
   currently uses a single ChID space (per-runlist RAM disabled by default); 2048-entry
   sizing assumes this. Model assumes single ChID space; per-runlist spaces would
   invalidate the 2048 bound and need re-derivation.

None of the above blocks the scoped channel-alloc/FIFO-lifetime/heap/RPC model delivered
in `SHADOW-P77-M10-CHANNEL-SIM/` — each is excluded by construction and listed here with
its closer.
