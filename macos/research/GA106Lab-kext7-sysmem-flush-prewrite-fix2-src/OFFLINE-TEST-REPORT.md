# OFFLINE-TEST-REPORT — 1.7.0 Gate A (atualizar com build.log final)

Build KEXT 1.7.0 + CLI offline: PASS (kext bundle x86_64, _kmod_info, revisao).
Unitarios: slot/contract/bar-map/mmio/static/sysmem/concurrency/stop/lock/
binding/uclifetime/gfw-readiness/concurrency/lifetime/deadline ALL PASS;
test-flush-prewrite 6 grupos PASS (contrato 48B, encoding, falhas ABI,
wrong-device, AlreadyReady, pagina via template + alloc/multi fail).
ASAN: PASS (incl. flush); TSAN: PASS (incl. flush).
Mutacoes sysmem 16/16 CAUGHT (primeiro harness); harnesses GFW/executable:
ver build.log (herdados 1.6.2, sem alteracao por este prompt).
CLI: usage/bogus apenas; BUILD_SCRIPT_LIVE_CALLS = 0; nenhum teste abre servico.
