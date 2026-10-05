# OFFLINE-TEST-REPORT — 1.8.0 revE BME (gerado do build.log + harnesses; autoridade: build.log)

Build KEXT 1.8.0 revE + CLI offline: PASS (kext bundle x86_64, _kmod_info, revisão;
BUILD_OK; VERSION_CONSISTENCY_1_8_0; NO_NEW_SELECTORS Count=12 c/ EnableBusMaster=11).
Unitarios: slot/contract/bar-map/mmio/static/sysmem/concurrency/stop/lock/binding/
uclifetime/gfw-readiness/concurrency/lifetime/deadline/gateb-live-path/bme-enable ALL PASS;
test-bme-enable 9 grupos PASS (prestate/RMW/boot-reload/write-fail/readback/duplicate/
close/stop/validator/concorrência); test-gateb-live-path 9 grupos PASS.
ASAN: PASS (incl. bme-enable, gateb-live-path, flush); TSAN: PASS (incl. bme-enable
contenção x50, gateb-live-path, flush-verify).
Mutacoes: sysmem 16/16 + flush 6/6 + GFW 12/12 + production 4/4 + gateb 32/32 +
bme 16/16 CAUGHT (zero sobreviventes honestos; 2 exclusões documentadas no harness Gate B).
CLI: usage/bogus apenas; BUILD_SCRIPT_LIVE_CALLS = 0; nenhum teste abre servico
(OFFLINE_BUILD_STRICT = YES).
Auditorias: GATEB presence/intended, BME helper único (escopo writeBmeCommand),
latch one-way (GateB + BME), pin loop 11 handlers, zero-write surfaces, binário
(sem OSWrite/configWrite/BME-enable imports além do vtable configWrite16 único).
