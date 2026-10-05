# UPSTREAM-EVIDENCE.md — GFW_BOOT primary evidence (KEXT6.1, ASTRA-B-FIX §10)

Snapshots byte-exactos (não editados) de trechos de fontes primárias.
Recuperados em 2026-09-08 via raw.githubusercontent.com (torvalds/linux,
branch master). Nenhum blog/issue usado como substituto.

## 1. nova-regs-pgc6-gfw.rs

```text
project = torvalds/linux, drivers/gpu/nova-core/regs.rs (linhas 118-165)
upstream URL = https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/nova-core/regs.rs
last commit touching file = 849044a381323b141d81f6bcba628cbeb0bcff31
commit date = 2026-07-30T04:01:13Z (via GitHub API)
retrieval date = 2026-09-08
SHA-256 (excerpt file) = fa4419225898e350ee8bd17904c6ef066fc31e35d4e0224471803baf9760ccbb
exact relevant symbols/lines =
  NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_PRIV_LEVEL_MASK @ 0x00118128,
    field 0:0 read_protection_level0 ("Set after FWSEC lowers its protection level")
  NV_PGC6_AON_SECURE_SCRATCH_GROUP_05[1] @ 0x00118234
    ("OpenRM defines this as a register array ... only uses its first element")
  NV_PGC6_AON_SECURE_SCRATCH_GROUP_05_0_GFW_BOOT => GROUP_05[0],
    field 7:0 progress ("0xff means completed")
  completed() = (progress() == 0xff)
proves = REG_118128_IDENTITY + REG_118234_IDENTITY + COMPLETION 0xff
```

## 2. nova-hal-tu102-gfw.rs

```text
project = torvalds/linux, drivers/gpu/nova-core/gpu/hal/tu102.rs (linhas 1-100)
upstream URL = https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/nova-core/gpu/hal/tu102.rs
last commit touching file = 99676aed1fec109d62822e21a06760eb098dc5f4
commit date = 2026-06-03T20:10:51Z (via GitHub API)
retrieval date = 2026-09-08
SHA-256 (excerpt file) = 308271f9f4fbd0b36acb85a54f9c453bfa3c06263e86c3e83a0ad3c419bd1dbb
exact relevant symbols/lines =
  wait_gfw_boot_completion(): gate && completed() short-circuit,
  Delta::from_millis(1), Delta::from_secs(4),
  doc: gate must be checked before status; TIMEOUT arbitrarily large
proves = READ_ORDER + POLL 1ms/4s + gate-means-access (não readiness)
```

## 3. nova-hal-dispatch.rs

```text
project = torvalds/linux, drivers/gpu/nova-core/gpu/hal.rs (arquivo integral, 39 linhas)
upstream URL = https://raw.githubusercontent.com/torvalds/linux/master/drivers/gpu/nova-core/gpu/hal.rs
last commit touching file = 99676aed1fec109d62822e21a06760eb098dc5f4
commit date = 2026-06-03T20:10:51Z (via GitHub API)
retrieval date = 2026-09-08
SHA-256 (excerpt file) = e3dac99970be9418295bd799bfd1cf5e4a4cfe3fc9f4684e7ab19d18eb53354b
exact relevant symbols/lines =
  gpu_hal(): Turing | Ampere | Ada => tu102::TU102_HAL
proves = GA106_APPLICABILITY (GA106 é Ampere => mesmo caminho TU102;
  consistente com open-gpu-kernel-modules sem kernel_gsp_ga106.c,
  cf. repo docs/nvidia-open/GA106-kernel-map.md:75)
```

Resultados:

```text
REGISTER_SEMANTICS_PRIMARY_EVIDENCE = PASS
GA106_APPLICABILITY_PRIMARY_EVIDENCE = PASS
```
