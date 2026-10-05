# TG-KEXT2-PCI-READ-STABILITY result (2026-09-07 20:32)

2 calls only (A + sleep 2 + B). No third read.

## Snapshots
A exit 0 + B exit 0, identical (diff empty):
```text
RAW vendor=10de device=2504 class=030000 rev=a1
RAW command=0003 status=0010
BAR0 fb000000 BAR1 4000000c BAR2 00000004 BAR3 5000000c BAR4 00000004 BAR5 00001001
subsys 10de:2504 rom fc000000 capPtr 60 intLine 18 intPin 01
mse on bme off
```
Note: intLine 18 vs 19:41 first-retry ff — OBSERVE_ONLY field, system-routed,
A==B stable here. No FAIL.

## Gate
```text
MAC_GPU_PCI_READ_0 = PASS
PCI_CONFIG_READ_FOUNDATION_COMPLETE = YES
IDENTITY_STABLE YES / COMMAND UNCHANGED YES / MSE/BME UNCHANGED YES /
BAR_RAW_STABLE YES / SUBSYSTEM/ROM/CAP/INTPIN STABLE YES /
STATUS 0010==0010 (unchanged, observe) / INTLINE 18==18 (unchanged, observe)
WRITE NONE / MAPPING/MMIO/DMA/INT/RESET/GSP/FW NO /
AMD/WS/DP HEALTHY / PANIC NO / GPU_RESTART NO
```

## Files
preflight.txt / snapshot-A.txt / snapshot-B.txt / comparison.txt /
command-analysis.txt / bar-analysis.txt / graphics-health-after.txt /
regression-check.txt / result.md
```

(End of file)
