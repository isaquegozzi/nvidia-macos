# TG-KEXT2-PCI-READ-RETRY result — FIRST FIXED SNAPSHOT (2026-09-07 19:41:20 -0300)

Single call only. No repeat.

## Raw

TIMESTAMP=2026-09-07 19:41:20 -0300
BIN=GA106Lab-kext2-pci-address-fix-src/build/obj/ga106ctl (550d916b...)
EXIT=0
STDERR=(empty)

```text
RAW vendor=10de device=2504 class=030000 rev=a1
RAW command=0003 status=0010
RAW bar0=fb000000
RAW bar1=4000000c
RAW bar2=00000004
RAW bar3=5000000c
RAW bar4=00000004
RAW bar5=00001001
RAW subsys=10de:2504 rom=fc000000 capPtr=60 intLine=ff intPin=01
DECODED identity: vendor=10de device=2504 class=030000
DECODED mse=on bme=off
```

## Veredito

```text
PCI_ADDRESS_FIX_LIVE = PASS
MAC_GPU_PCI_READ_0_RETRY = PASS_STAGE1
PCI_CONFIG_READ_FOUNDATION_COMPLETE = NOT_YET (estabilidade + Command before-after em fase separada)
```

Identity matches IORegistry GFX0@0 (vendor de10/device 0425/class 00000300/rev a1/subsys 10de:2504, BDF 01:00.0, entry 4294967865).
Previous bug (00:00.0 / 1022:1630 via zeroed space) NOT reproduced.
Audit offline: configWrite NONE / setMemoryEnable NONE / setBusMasterEnable NONE.
Post: AMD HEALTHY / WindowServer HEALTHY (195) / displaypolicyd HEALTHY (143) / PANIC NO / GPU_RESTART NO.

## Files

timestamp.txt / stdout.txt / stderr.txt / exitcode.txt / preflight.txt / posthealth.txt / result.md
```

(End of file)
