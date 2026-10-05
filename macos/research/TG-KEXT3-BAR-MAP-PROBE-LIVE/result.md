# TG-KEXT3-BAR-MAP-PROBE-LIVE result — FIRST LIVE (2026-09-07 20:56)

Single call only (1/1). No retry, no pci, no second probe.

## Raw (exit 0, stderr empty)

```text
BAR semantic=0 resourceIndex=0
BAR physBase=0x00000000fb000000 resourceLength=0x0000000001000000 mappedLength=0x0000000001000000
BAR mapOptions=0x00001101 mapState=1 validMask=0x0000006f status=4
```

## Gate

```text
MAC_GPU_BAR_MAP_0 = PASS
BAR_MAPPING_FOUNDATION_COMPLETE = YES (MMIO ainda NO)
```

Decode: semantic BAR0 / idx 0 runtime-validated / phys 0xFB000000 /
len 16MiB sane / mapLen==resLen / opts 0x1101 ⊇ 0x1100 (RO+Inhibit;
bit0 = kIOMapAnywhere normalizado pelo framework — REQUIRED_BITS policy
confirmada) / state released / mask 0x6f (todos menos RES_FLAGS, por desenho) /
status 4 MAP_RELEASED + RELEASE_CONFIRMED. Command/MSE/BME unchanged:
sucesso do caminho interno exige before==after (código retorna Error senão).
Zero MMIO/deref/write/configWrite/DMA/IRQ/reset/GSP. AMD/WS/dp HEALTHY,
panic NO. Segunda chamada PROIBIDA.

## Files

preflight.txt / timestamp.txt / bar-map-probe-first.txt (.stderr/exit) /
abi-analysis.txt / resource-analysis.txt / map-policy-analysis.txt /
lifecycle-analysis.txt / graphics-health-after.txt / regression-check.txt /
result.md
```

(End of file)
