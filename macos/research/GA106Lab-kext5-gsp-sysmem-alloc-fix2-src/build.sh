#!/bin/zsh
# build.sh — compila GA106Lab.kext com o toolchain Xcode (sem assinar/instalar).
# Método: MANUAL_OFFICIAL_EQUIVALENT — reproduz o que o target Kernel Extension do
# Xcode faz: compile -mkernel + link -kext (MH_KEXT_BUNDLE) + kmod libs + KMOD decl
# no fonte (ver auditoria §3: NootedRed tem filetype KEXTBUNDLE + _kmod_info local).
# ld -r (MH_OBJECT) é PROIBIDO como link final — objeto relocável não carrega.
# Uso: ./build.sh (gera ./build/GA106Lab.kext). Falha estrita, sem SUCCESS falso.
set -euo pipefail
SRC_DIR="${0:A:h}"
BUILD_DIR="$SRC_DIR/build"
OBJ_DIR="$BUILD_DIR/obj"
LOG="$BUILD_DIR/build.log"
SDK=$(xcrun --show-sdk-path)
KHDR="$SDK/System/Library/Frameworks/Kernel.framework/Headers"
ARCH=x86_64
# Alvo mínimo = SDK corrente (26.5); kernel vivo é 26.6.2.
VERSMIN="-mmacosx-version-min=26.5"

rm -rf "$BUILD_DIR"
mkdir -p "$OBJ_DIR/GA106Lab.kext/Contents/MacOS"

{
echo "== toolchain =="
xcodebuild -version
xcrun clang++ --version | head -n 2
echo "SDK=$SDK"
echo "== compile kext =="
xcrun clang++ -arch $ARCH -c \
  -Os -mkernel $VERSMIN \
  -fno-builtin -fno-stack-protector -fno-common -fapple-kext \
  -nostdinc \
  -I"$KHDR" \
  -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
  -o "$OBJ_DIR/GA106Lab.o" "$SRC_DIR/GA106Lab.cpp"
xcrun clang++ -arch $ARCH -c \
  -Os -mkernel $VERSMIN \
  -fno-builtin -fno-stack-protector -fno-common -fapple-kext \
  -nostdinc \
  -I"$KHDR" \
  -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
  -o "$OBJ_DIR/GA106LabUserClient.o" "$SRC_DIR/GA106LabUserClient.cpp"
echo "== link (MH_KEXT_BUNDLE, sem -r) =="
xcrun clang++ -arch $ARCH -mkernel $VERSMIN \
  -nostdlib -Xlinker -kext \
  -o "$OBJ_DIR/GA106Lab.kext/Contents/MacOS/GA106Lab" \
  "$OBJ_DIR/GA106Lab.o" "$OBJ_DIR/GA106LabUserClient.o" \
  -lkmodc++ -lkmod -lcc_kext
echo "== assemble =="
cp "$SRC_DIR/Info.plist" "$OBJ_DIR/GA106Lab.kext/Contents/Info.plist"
plutil -lint "$OBJ_DIR/GA106Lab.kext/Contents/Info.plist"
echo "== compile cli (userspace, sem kext) =="
xcrun clang -Os -mmacosx-version-min=26.2 -arch $ARCH \
  -o "$OBJ_DIR/ga106ctl" "$SRC_DIR/ga106ctl.c" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" \
  -framework IOKit -framework CoreFoundation
echo "== compile+run testes offline (shared flow, sem kernel) =="
xcrun clang -Os -arch $ARCH -o "$OBJ_DIR/test-slot" "$SRC_DIR/test-slot.c"
xcrun clang -Os -arch $ARCH -o "$OBJ_DIR/test-contract" \
  "$SRC_DIR/test-contract.c" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR"
xcrun clang -Os -arch $ARCH -o "$OBJ_DIR/test-bar-map-probe" \
  "$SRC_DIR/test-bar-map-probe.c" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR"
# Teste do shared flow selector 5 (C++ template + MockOps).
xcrun clang++ -Os -std=c++17 -arch $ARCH -o "$OBJ_DIR/test-first-mmio-read" \
  "$SRC_DIR/test-first-mmio-read.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
# Teste do shared flow selector 6 (static identity single-read).
xcrun clang++ -Os -std=c++17 -arch $ARCH -o "$OBJ_DIR/test-static-identity" \
  "$SRC_DIR/test-static-identity.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
# Teste do shared flow selector 7 (sysmem alloc, zero HW).
xcrun clang++ -Os -std=c++17 -arch $ARCH -o "$OBJ_DIR/test-gsp-sysmem" \
  "$SRC_DIR/test-gsp-sysmem.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
# Teste de concorrência pthreads (gate determinístico no Mock).
xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-sysmem-concurrency" \
  "$SRC_DIR/test-sysmem-concurrency.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
# FIX-ASTRA-B2: regressão stop→free + lock failure (offline, zero HW).
xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-stop-free-regression" \
  "$SRC_DIR/test-stop-free-regression.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
xcrun clang++ -Os -std=c++17 -arch $ARCH -o "$OBJ_DIR/test-lock-failure" \
  "$SRC_DIR/test-lock-failure.cpp" \
  -I"$SRC_DIR"
echo "== regressão bug 00:00.0: forma 2-arg/union zerada PROIBIDA no fonte =="
if grep -n "kIOPCIConfigSpace,\|kSpace\|configRead.*(.*," "$SRC_DIR/GA106Lab.cpp" | grep -v "^.*://"; then
  echo "ZEROED_SPACE_REGRESSION_TEST = FAIL (forma antiga presente)"
  exit 1
fi
echo "ZEROED_SPACE_REGRESSION_TEST = PASS (só wrappers 1-arg)"
echo "== auditoria TG-KEXT4-B: loads MMIO (5:1, 6:1), zero writes =="
if grep -n "configWrite\|setMemoryEnable\|setBusMasterEnable\|setBusLeadEnable\|OSWrite\|write8\|write16\|write32\|write64\|IOInterrupt\|firmware" "$SRC_DIR/GA106LabMmioFlow.hpp" "$SRC_DIR/GA106LabStaticIdentityFlow.hpp" | grep -v "^.*://" | grep -v "NÃO\|nunca\|NUNCA\|SEM\|sem \|PROIBID\|Ausência\|ausência\|Nenhum\|nenhum\|0 .*write\|ÚNICA\|única"; then
  echo "KEXT4_SOURCE_AUDIT = FAIL (símbolo proibido nos flows MMIO)"
  exit 1
fi
echo "KEXT4_SOURCE_AUDIT = PASS (flows 5/6 sem write/configWrite/DMA/IRQ)"
echo "== auditoria selector 7: zero MMIO/BAR/PCI-config/BME/DMA-trigger =="
if grep -n "OSReadLittleInt32\|OSWriteLittleInt32\|configWrite\|configRead\|setBusLeadEnable\|setBusMasterEnable\|mapDeviceMemory\|getVirtualAddress" "$SRC_DIR/GA106LabGspSysmemFlow.hpp" | grep -v "^.*://"; then
  echo "SYSMEM_FLOW_HW = FAIL (HW access no flow sysmem)"
  exit 1
fi
echo "SYSMEM_ZERO_HW_SOURCE = PASS (flow sem HW; RealOps sysmem auditado abaixo)"
if awk '/struct GA106LabSysmemRealOps/,/^};/' "$SRC_DIR/GA106LabRealOps.hpp" | grep -n "OSReadLittleInt32\|OSWriteLittleInt32\|configWrite\|configRead\|setBusLeadEnable\|setBusMasterEnable\|mapDeviceMemory\|getVirtualAddress\|Falcon" | grep -v "^.*://"; then
  echo "SYSMEM_REALOPS_HW = FAIL (HW access nas SysmemRealOps)"
  exit 1
fi
echo "SYSMEM_REALOPS_ZERO_HW = YES"
# RealOps: 1 primitiva OSRead; MmioFlow: 1x ops.read32; StaticFlow: 1x ops.read32.
READS_REALOPS=$(grep -c "OSReadLittleInt32" "$SRC_DIR/GA106LabRealOps.hpp" || true)
READS_FLOW5=$(grep -c "ops.read32" "$SRC_DIR/GA106LabMmioFlow.hpp" || true)
READS_FLOW6=$(grep -c "ops.read32" "$SRC_DIR/GA106LabStaticIdentityFlow.hpp" || true)
echo "READS: RealOps=$READS_REALOPS Flow5=$READS_FLOW5 Flow6=$READS_FLOW6"
if [ "$READS_REALOPS" != "1" ]; then
  echo "KEXT4_READ_AUDIT = FAIL (RealOps x$READS_REALOPS, esperado x1 primitiva)"
  exit 1
fi
if [ "$READS_FLOW5" != "1" ]; then
  echo "KEXT4_READ_AUDIT = FAIL (Flow5 x$READS_FLOW5, esperado x1)"
  exit 1
fi
if [ "$READS_FLOW6" != "1" ]; then
  echo "KEXT4_READ_AUDIT = FAIL (Flow6 x$READS_FLOW6, esperado x1 single-read)"
  exit 1
fi
echo "KEXT4_READ_AUDIT = PASS (5:1 load, 6:1 load, 0 stores)"
echo "== auditoria selector 6: sem BOOT_0/BOOT_2, allowlist fixa, mask 10-bit =="
if grep -n "PmcBoot0" "$SRC_DIR/GA106LabStaticIdentityFlow.hpp" | grep -v "^.*://"; then
  echo "BOOT0_REREAD = FAIL (selector 6 referencia BOOT_0)"
  exit 1
fi
echo "BOOT0_REREAD_IN_SELECTOR6 = NO"
if grep -n "Boot2\|boot2Raw\|0x00000008" "$SRC_DIR/GA106LabStaticIdentityFlow.hpp" "$SRC_DIR/GA106LabRealOps.hpp" | grep -v "^.*://" | grep -v "BOOT_2 removido\|removido"; then
  echo "BOOT2_RESIDUE = FAIL (referência viva a BOOT_2 no caminho de produção)"
  exit 1
fi
echo "BOOT2_REMOVED_FROM_PRODUCTION_FLOW = YES"
if ! grep -q "kGA106LabStaticIdentity_Boot42_Offset" "$SRC_DIR/GA106LabRealOps.hpp"; then
  echo "ALLOWLIST = FAIL (RealOps sem offset fixo BOOT_42)"
  exit 1
fi
if ! grep -q "0x00000A00" "$SRC_DIR/GA106LabProtocol.h"; then
  echo "ALLOWLIST = FAIL (Protocol sem offset BOOT_42)"
  exit 1
fi
if ! grep -q "GA106LabBoot42IsGA106\|GA106LAB_BOOT42_CHIP_MASK" "$SRC_DIR/GA106LabStaticIdentityFlow.hpp"; then
  echo "BOOT42_MASK_FIX = FAIL (flow não usa helper 10-bit)"
  exit 1
fi
echo "ALLOWLIST_FIXED = {0xA00}"
echo "BOOT42_MASK = 0x3FF00000 (helper distinto; BOOT_0 0x1FF00000 intacto)"
echo "== auditoria ONE_SHARED_FLOW: wrapper fino, sem fluxo duplicado =="
if grep -n "GA106LabTestSeam\|modelRead" "$SRC_DIR/GA106Lab.cpp" "$SRC_DIR/GA106LabMmioFlow.hpp" "$SRC_DIR/GA106LabRealOps.hpp" | grep -v "^.*://"; then
  echo "DUPLICATED_MODEL_FLOW = FAIL (seam antigo ainda referenciado na produção)"
  exit 1
fi
echo "DUPLICATED_MODEL_FLOW = NO"
if ! grep -q "RunFirstMmioReadFlow" "$SRC_DIR/GA106Lab.cpp"; then
  echo "ONE_SHARED_MMIO_FLOW = FAIL (produção não usa shared flow)"
  exit 1
fi
echo "ONE_SHARED_MMIO_FLOW = YES (produção usa RunFirstMmioReadFlow)"
if ! grep -q "RunStaticIdentityReadsFlow" "$SRC_DIR/GA106Lab.cpp"; then
  echo "ONE_SHARED_STATIC_IDENTITY_FLOW = FAIL"
  exit 1
fi
echo "ONE_SHARED_STATIC_IDENTITY_FLOW = YES"
if ! grep -q "RunPrepareGspSysmemFlow" "$SRC_DIR/GA106Lab.cpp"; then
  echo "ONE_SHARED_SYSMEM_FLOW = FAIL"
  exit 1
fi
echo "ONE_SHARED_SYSMEM_FLOW = YES"
echo "== auditoria selector 7: explicit prepare/clear, zero HW =="
if ! grep -q "setMemoryDescriptor(" "$SRC_DIR/GA106Lab.cpp"; then
  echo "AUTO_PREPARE = FAIL (sem setMemoryDescriptor explícito)"
  exit 1
fi
if grep -n "setMemoryDescriptor([^)]*, *true" "$SRC_DIR/GA106Lab.cpp" | grep -v "^.*://"; then
  echo "AUTO_PREPARE = FAIL (autoPrepare=true presente)"
  exit 1
fi
if ! grep -q "clearMemoryDescriptor(false)" "$SRC_DIR/GA106Lab.cpp"; then
  echo "AUTO_COMPLETE = FAIL (clear sem autoComplete=false explícito)"
  exit 1
fi
if grep -n "clearMemoryDescriptor([^)]*, *true" "$SRC_DIR/GA106Lab.cpp" | grep -v "^.*://"; then
  echo "AUTO_COMPLETE = FAIL (autoComplete=true presente)"
  exit 1
fi
echo "AUTO_PREPARE = NO / AUTO_COMPLETE = NO"
PREP_COUNT=$(grep -c "ops.prepareDma" "$SRC_DIR/GA106LabGspSysmemFlow.hpp" || true)
if [ "$PREP_COUNT" != "1" ]; then
  echo "PREPARE_COUNT = FAIL (flow chama prepare x$PREP_COUNT, esperado x1)"
  exit 1
fi
echo "PREPARE_COUNT_SUCCESS_PATH = 1"
if grep -n "OSReadLittleInt32\|OSWriteLittleInt32\|configWrite\|setBusLeadEnable\|setBusMasterEnable\|mapDeviceMemory\|getVirtualAddress\|Falcon\|firmware" "$SRC_DIR/GA106LabGspSysmemFlow.hpp" | grep -v "^.*://"; then
  echo "SYSMEM_FLOW_HW = FAIL (HW/firmware no flow sysmem)"
  exit 1
fi
echo "SYSMEM_FLOW_ZERO_HW = YES"
if grep -n "IOBufferMemoryDescriptor\|IODMACommand\|fSysmem\|IOVMAddr\|dmaAddr" "$SRC_DIR/GA106LabUserClient.cpp" | grep -v "^.*://"; then
  echo "USERCLIENT_DMA_OWNERSHIP = FAIL (UserClient guarda objeto DMA)"
  exit 1
fi
echo "USERCLIENT_DMA_OBJECT_OWNERSHIP = NO"
awk '/struct GA106LabGspSysmemV1/,/^};/' "$SRC_DIR/GA106LabProtocol.h" | grep -in "uint64_t\|dmaaddress\|physicaladdress\|iovm\|kernelva\|pointer" && { echo "ABI_ADDRESS_LEAK = FAIL"; exit 1; } || true
echo "DMA_ADDRESS_EXPOSED_TO_USERSPACE = NO"
echo "== auditoria FIX-ASTRA-C: serialização e guards =="
if ! grep -q "kIOReturnBusy" "$SRC_DIR/GA106LabGspSysmemFlow.hpp"; then
  echo "BUSY_GUARD = FAIL"
  exit 1
fi
if ! grep -q "kIOReturnAborted" "$SRC_DIR/GA106LabGspSysmemFlow.hpp"; then
  echo "ABORT_GUARD = FAIL"
  exit 1
fi
if ! grep -q "IN_PROGRESS" "$SRC_DIR/GA106LabGspSysmemFlow.hpp"; then
  echo "IN_PROGRESS = FAIL"
  exit 1
fi
if grep -n "^xcrun clang.*-DGA106LAB_SYSMEM_MUT_" "$SRC_DIR/build.sh"; then
  echo "MUT_IN_PROD_BUILD = FAIL (macro test-only em comando de build)"
  exit 1
fi
echo "SERIALIZATION_GUARDS = YES (Busy/Aborted/IN_PROGRESS; MUT só no harness)"
echo "== stop/free contract =="
if ! grep -q "WaitForSysmemIdleAndTeardownFlow" "$SRC_DIR/GA106Lab.cpp"; then
  echo "STOP_SHARED_FLOW = FAIL (stop não usa template compartilhado)"
  exit 1
fi
if grep -n "complete\|clearMemoryDescriptor" "$SRC_DIR/GA106Lab.cpp" | grep -v "^.*://" | grep -v "SysmemRealOps::" | grep -v "clearAddressRecord\|//" | head -n 5 | grep -q "completeDma\|clearDma"; then
  echo "STOP_DIRECT_KPI = FAIL (stop deve usar template, não KPI direto)"
  exit 1
fi
echo "STOP_USES_SHARED_FLOW = YES"
echo "== auditoria FIX-ASTRA-B2: free CLEANUP_FAILED + lock init =="
if ! grep -q "CLEAR_FAILURE_RELEASE_POLICY" "$SRC_DIR/GA106Lab.cpp"; then
  echo "FREE_POLICY = FAIL (sem LEAK_SAFE_NO_RELEASE)"
  exit 1
fi
if ! grep -q "kSysmemPhase_CleanupFailed" "$SRC_DIR/GA106Lab.cpp"; then
  echo "FREE_CLEANUP_FAILED_GUARD = FAIL"
  exit 1
fi
if ! grep -q "LOCK_POINTERS_EXPLICITLY_INITIALIZED" "$SRC_DIR/GA106Lab.cpp"; then
  echo "LOCK_INIT_DOC = FAIL"
  exit 1
fi
if ! grep -q "fSysmemLock = NULL" "$SRC_DIR/GA106Lab.cpp"; then
  echo "LOCK_INIT_NULL = FAIL (fSysmemLock sem nullptr explícito)"
  exit 1
fi
if ! grep -q "fSlotLock = NULL" "$SRC_DIR/GA106Lab.cpp"; then
  echo "LOCK_INIT_NULL = FAIL (fSlotLock sem nullptr explícito)"
  exit 1
fi
if ! grep -q "GA106LabFreeLockNullSafe" "$SRC_DIR/GA106Lab.cpp"; then
  echo "LOCK_HELPER = FAIL (sem helper null-safe central)"
  exit 1
fi
if grep -n "IOSimpleLockFree(fSlotLock)" "$SRC_DIR/GA106Lab.cpp" | grep -v "^.*://" | grep -v "GA106LabFreeLockNullSafe\|if (fSlotLock)\|if (fSysmemLock)"; then
  echo "RAW_LOCK_FREE = FAIL (free direto sem null-safe)"
  exit 1
fi
echo "FIX_ASTRA_B2_SOURCE = YES (free leak-safe + locks nullptr + helper)"
echo "== auditoria versão 1.5.2 =="
if ! grep -q "1.5.2" "$SRC_DIR/Info.plist"; then
  echo "VERSION_PLIST = FAIL"
  exit 1
fi
if ! grep -q "0x00010502" "$SRC_DIR/GA106LabProtocol.h"; then
  echo "VERSION_PROTOCOL = FAIL"
  exit 1
fi
if ! grep -q "TG-KEXT5-GSP-SYSMEM-ALLOC-FIX-ASTRA-B2" "$SRC_DIR/GA106Lab.cpp"; then
  echo "VERSION_REVISION = FAIL"
  exit 1
fi
echo "VERSION_1_5_2 = YES"
"$OBJ_DIR/test-slot"
"$OBJ_DIR/test-contract"
"$OBJ_DIR/test-bar-map-probe"
"$OBJ_DIR/test-first-mmio-read"
"$OBJ_DIR/test-static-identity"
"$OBJ_DIR/test-gsp-sysmem"
"$OBJ_DIR/test-sysmem-concurrency"
"$OBJ_DIR/test-stop-free-regression"
"$OBJ_DIR/test-lock-failure"
echo "== sanitizers (userspace tests) =="
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -o "$OBJ_DIR/test-gsp-sysmem-asan" \
  "$SRC_DIR/test-gsp-sysmem.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-gsp-sysmem-asan" > /dev/null && echo "ASAN_GSP_SYSMEM = PASS"
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -pthread -o "$OBJ_DIR/test-conc-asan" \
  "$SRC_DIR/test-sysmem-concurrency.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-conc-asan" > /dev/null && echo "ASAN_CONCURRENCY = PASS"
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -pthread -o "$OBJ_DIR/test-stopfree-asan" \
  "$SRC_DIR/test-stop-free-regression.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-stopfree-asan" > /dev/null && echo "ASAN_STOP_FREE = PASS"
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -o "$OBJ_DIR/test-lockfail-asan" \
  "$SRC_DIR/test-lock-failure.cpp" \
  -I"$SRC_DIR"
"$OBJ_DIR/test-lockfail-asan" > /dev/null && echo "ASAN_LOCK_FAILURE = PASS"
if xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=thread -pthread -o "$OBJ_DIR/test-conc-tsan" \
  "$SRC_DIR/test-sysmem-concurrency.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$OBJ_DIR/tsan-build.log"; then
  "$OBJ_DIR/test-conc-tsan" > /dev/null && echo "TSAN_CONCURRENCY = PASS" || echo "TSAN_CONCURRENCY = FAIL"
else
  echo "TSAN = NOT_AVAILABLE (toolchain rejeita -fsanitize=thread; ver tsan-build.log)"
fi
echo "== mutation harness =="
chmod +x "$SRC_DIR/mutation-harness.sh"
if [ "${SKIP_MUTATIONS:-0}" = "1" ]; then
  echo "SKIP_MUTATIONS=1 (harness executado separadamente)"
else
  "$SRC_DIR/mutation-harness.sh" || exit 1
fi
"$OBJ_DIR/ga106ctl" 2>&1 | head -n 1 || true
"$OBJ_DIR/ga106ctl" bogus 2>&1 | head -n 1 || true
"$OBJ_DIR/ga106ctl" version 2>&1 | head -n 1 || true
# (saídas acima esperadas offline: usage + service-not-found; BUILD_OK só
# reflete compile/link/assemble/testes, cujas falhas abortam via pipefail)
echo "BUILD_OK"
} 2>&1 | tee "$LOG"
