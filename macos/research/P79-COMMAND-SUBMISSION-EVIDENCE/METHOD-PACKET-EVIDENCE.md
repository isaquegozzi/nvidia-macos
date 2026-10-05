# P79 — Method-Packet Evidence (Ampere DMA method family)

## Verdict: PROVEN (header fields + opcodes + method addresses; LE word order by header convention)

## 1. Header word (32-bit, `clc56f.h` DMA method formats; same family in `cla06f.h`)

| Bits | Field | Notes |
|---|---|---|
| 11:0 | ADDRESS | method address (12-bit; `*_ADDRESS_OLD 12:2` legacy view) |
| 15:13 | SUBCHANNEL | 0..7 (`NVC56F_NUMBER_OF_SUBCHANNELS = 8`) |
| 17:16 | TERT_OP | GRP0 INC/SET_SUB_DEV_MASK/STORE/USE_MASK; GRP2 NON_INC selector |
| 28:16 | COUNT / IMMD_DATA | data-word count (13-bit) or immediate value |
| 31:29 | SEC_OP | packet class (see §2) |

`NVC56F_DMA_METHOD_COUNT_OLD 28:18` documents the older narrower count; current is 28:16.

## 2. Packet classes (SEC_OP)

```text
0 = GRP0_USE_TERT (tertiary-op packet) | 1 = INC_METHOD (count words, address++)
2 = GRP2_USE_TERT | 3 = NON_INC_METHOD (count words, same address)
4 = IMMD_DATA_METHOD (data in header, no payload) | 5 = ONE_INC (single increment)
6 = RESERVED6 (never emitted; detection-only) | 7 = END_PB_SEGMENT
```

Per-class opcode values repeat the class (`INCR_OPCODE_VALUE=1`, `NONINCR=3`, `IMMD=4`,
`ONEINCR=5`). `DMA_NOP = 0x00000000`. `SET_SUBDEVICE_MASK` uses bits 15:4 + opcode 31:16.

## 3. Known method addresses (class 0xc56f)

`SET_OBJECT 0x00` (+NVCLASS 15:0, ENGINE 20:16, SW engine 0x1f), `ILLEGAL 0x04`,
`NOP 0x08`, `SEMAPHOREA-D 0x10-0x1C` (see FENCE doc), `NON_STALL_INTERRUPT 0x20`,
`FB_FLUSH 0x24` (deprecated → SYS_MEMBAR), `MEM_OP_A-D 0x28+`, `SEM_EXECUTE*`,
`WFI 0x78`, `YIELD 0x80`, `SET_REFERENCE 0x50`, `CLEAR_FAULTED 0x84`.
Model rule: ADDRESS beyond the documented set ⇒ `UnknownMethod` fault (never executed).

## 4. Payload rules (modeled)

- INC/NONINC/ONE_INC: exactly COUNT payload words follow; COUNT=0 ⇒ empty packet fault
  (upstream analogue: zero-length emission is meaningless; model rejects).
- COUNT must not overrun the pushbuffer window (`packet overflow` fault).
- IMMD/END_SEGMENT/TERT: no payload words; extra words ⇒ overflow fault.
- RESERVED6 ⇒ `ReservedEncoding` fault on encode; detection path covered by decode test.
- Words are 32-bit little-endian units as numbered by the header bitfields (driver writes
  via 32-bit stores, e.g. `nvkm_wo32`; big-endian hosts out of scope — see OPEN-QUESTIONS).
