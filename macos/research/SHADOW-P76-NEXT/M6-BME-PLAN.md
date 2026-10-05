# SHADOW — M6 BME policy model PLAN (draft, NOT implemented, awaiting M5 verdict)
Spec: G0004 §3 + protocol M6. Isolated dir SHADOW-P76-NEXT/bme-sim/, zero production wiring.
1. Mock PCI config space: 256B array, Command @0x04 (MSE bit1, BME bit2). Baseline 0x0003.
2. API model: setBusLeadEnable(bool) -> Command 0x0003<->0x0007; setBusMasterEnable marked
   deprecated (documents macOS 10.0+ KPI note; model rejects it => InvalidArg).
3. Policy: require BME_OFF during Gate A/B phases; enable allowed only with explicit
   DmaPhase token (future M7); disable returns to 0x0003; double-enable => AlreadyOn (no-op PASS).
4. Fail-closed: any unknown Command bits on entry => Error + no transition; enable during
   "flush-registered-unverified" mock state => Denied.
5. Timing test: enable BEFORE flush-commit vs AFTER verified-commit matrix.
6. Mutations (-D): SKIP_BME_CHECK, ALLOW_BME_DURING_GATEA, ACCEPT_DEPRECATED_API,
   DOUBLE_ENABLE_FLIP, IGNORE_UNKNOWN_BITS. All must be CAUGHT.
7. No config-write outside mock array (static assert: only MockPci::writeConfig symbol).
