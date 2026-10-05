# P79 Command-Submission — Evidence Binding (model claim → upstream line)

- Entry 8B `{GET 31:2, OPERAND 31:0, GET_HI 7:0, LEVEL b9, LENGTH 30:10, SYNC b31,
  OPCODE 7:0=NOP/ILLEGAL/GP_CRC/PB_CRC}` → `clc56f.h` "GPFIFO entry format" (~265-283).
- `entries = length/8`, `limit2 = ilog2(length/8)`, inst 0x048/0x04c →
  `r535-fifo.c:91-92` (`gpFifoOffset/gpFifoEntries`), `ga100.c:72-78` (ramfc_write).
- Control `Put@0x40 RW / Get@0x44 RO / Reference@0x48 RO / PutHi / TopLevelGet(Hi) /
  GetHi / GPGet@0x88 RO / GPPut@0x8c` → `clc56f.h` `Nvc56fControl` (40-66).
- Kick = doorbell `(runl<<16)|chid` → `ga100.c` (`ga100_chan_doorbell_handle`),
  `tu102.c` (P77-pinned); P79 models it as ordering token, never MMIO.
- Method header ADDRESS 11:0 / SUBCHANNEL 15:13 / COUNT 28:16 / SEC_OP 31:29 with
  INC=1/NONINC=3/IMMD=4/ONE_INC=5/RESERVED6=6/END_SEGMENT=7 + TERT_OP + per-class
  opcode values → `clc56f.h` DMA method formats; same family in `cla06f.h`.
- 8 subchannels → `NVC56F_NUMBER_OF_SUBCHANNELS`; allowlist addresses (SET_OBJECT 0x00
  … CLEAR_FAULTED 0x84 incl. SEMA A-D, WFI 0x78, SET_REFERENCE 0x50) → `clc56f.h:68-252`.
- Semaphore ops ACQUIRE/RELEASE/ACQ_GEQ/ACQ_AND/REDUCTION + WFI/FORMAT/SIZE/TIMESTAMP →
  `clc56f.h:76-107,214`; firmware events `SEMAPHORE_SCHEDULE_CALLBACK 0x100b` /
  `TIMED_SEMAPHORE_RELEASE 0x1018` → `nvrm-msgfn.h`; Reference counter → control @0x48.
- Error pinning → `chan.h` (`error`), `chan.c` (`nvkm_chan_error`), P77 Errored;
  notifier types → `r570`/`nvos.h` (P77-pinned).
- GSP boundary: RM owns alloc/BIND/SCHEDULE + tokens 186/187 (`nvrm-rpcfn.h:202-203`);
  queues execute locally (no RPC for append/kick/consume in vendored set).
- Channel ops bind/unbind/start/stop/preempt → `chan.h` (`nvkm_chan_func`).
