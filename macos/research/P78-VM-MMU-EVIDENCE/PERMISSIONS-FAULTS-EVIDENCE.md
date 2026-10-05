# P78 — Permissions + Faults Evidence

## Verdict: PROVEN (permission bits + guard-level fault classes; HW packet bytes NOT pinned)

## 1. Permissions

PTE level (`gp100_vmm_valid` + PTE writers):

```text
ro   → BIT(6)  (1 = read-only; 0 = writable).   v0 args {vol,ro,priv,kind}; vn default ro=0.
priv → BIT(5)  (1 = privileged).                vn default priv=0.
vol  → BIT(3)  (coherent/volatile).             vn default vol=(target==HOST).
atomic-disable → BIT(7) (1 = atomics off).      from PFN flags (!A).
kind → bits 63:56, range-checked (<kindn, !=kind_inv).
```

RM API level (`nvos-excerpt.h` `NVOS33_FLAGS_ACCESS` 1:0):

```text
READ_WRITE = 0 | READ_ONLY = 1 | WRITE_ONLY = 2
```

Model rule: PTE RO=1 + write access → `Permission` fault; PRIV=1 + unprivileged client →
`Permission`; unknown ACCESS value → `InvalidEncoding`. RM ACCESS maps to PTE RO
(RW→RO 0, RO→RO 1, WO→RO 0 + documented write-only caveat: RM may still return RW —
modeled as WO accepted but reported back as RW-capable, never as failure).

## 2. Fault handling (proven)

- `nvkm_fault_data {addr, inst, time, engine, valid, gpc, hub, access, client, reason}`
  (`nvkm-fault.h`); `tu102_fault_new` exists → Ampere fault subdevice present.
- `gp100_vmm_mthd`: `FAULT_REPLAY` → global replay invalidate `0x0b`;
  `FAULT_CANCEL` → targeted cancel invalidate `0x1b|hub<<20|gpc<<15|client<<9` after
  GR ctxsw pause/check/resume with inst translate `(inst>>12)|(aper<<28)|0x80000000`.
- `invalidate_pdb`: `0x100cb8/0x100cec = addr lo/hi`; `flush`: PAGE_ALL (+HUB_ONLY if BAR bound).
- `tu102_vmm_flush`: root `pd_addr>>8` to `0xb830a0`, `0` to `0xb830a4`, `0x80000000|type`
  to `0xb830b0`, poll ≤2000 ms. (Token-modeled: flush/invalidate are ordering tokens,
  not MMIO — model records sequence, never touches HW.)
- RM fault-buffer object lifecycle (`mmu_fault_buffer.c`): construct (class check →
  `INVALID_CLASS`; alloc replayable buffer), destruct, CPU map/unmap (whole buffer,
  no offsets), getMapAddrSpace (`INVALID_OBJECT` when absent).

## 3. Guard-level fault classes (modeled, each with upstream error path)

| Model fault | Upstream analogue |
|---|---|
| InvalidPte (VALID=0, non-sentinel) | `pfn_clear` VALID test; unmapped PTE reads as 0 |
| Permission (RO/PRIV/ACCESS) | `valid()` ro/priv/kind checks → `-EINVAL` |
| OutOfRange (VA ≥ limit /unmanaged) | ctor/get/map limit + mapref checks → `-EINVAL/-ENOENT` |
| ApertureMismatch (enc 1 / cross-layer leak) | `aper<0 → WARN_ON`; kind/aper validation |
| Alignment (VA/size/PA misaligned) | `IS_ALIGNED` checks → `-EINVAL` |
| Duplicate/Overlap (map over live VMA) | `!mapref \|\| memory → -EINVAL`; split/merge guards |
| UnmapMissing (no VMA / not mapped) | `node_search NULL → -ENOENT/-EINVAL` |
| Stale (use after unmap/free) | `mapped=false` + `memory=NULL` after unmap; dtor `WARN_ON(!list_empty)` |
| Wrap (addr+size wraps) | `addr+size<addr → -EINVAL` (ctor + pfn_map) |

## 4. NOT proven (honest gap)

HW fault-packet *wire bytes* (access/client/reason numeric encodings, timestamp format)
are not pinned to a struct in the vendored set — classification stops at the guard level
above. Closer: vendor `nvkm/subdev/fault/gv100.c`+`tu102.c` buffer parsing + OpenRM
`mmu_fault_buffer` packet structs.
