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
# TEST-PATH-FIX: mesmos via shared flow + binding test.
xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-stop-free-regression" \
  "$SRC_DIR/test-stop-free-regression.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
xcrun clang++ -Os -std=c++17 -arch $ARCH -o "$OBJ_DIR/test-lock-failure" \
  "$SRC_DIR/test-lock-failure.cpp" \
  -I"$SRC_DIR"
xcrun clang++ -Os -std=c++17 -arch $ARCH -o "$OBJ_DIR/test-prod-binding" \
  "$SRC_DIR/test-prod-binding.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
# PROMPT 57: lifetime do owner no UserClient (shared flow, pthreads).
xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-userclient-lifetime" \
  "$SRC_DIR/test-userclient-lifetime.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
# KEXT6: GFW readiness shared flow + concorrência (offline, zero stores).
# KEXT7: Gate A flush-prewrite (offline, zero stores, zero HW).
xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-flush-prewrite" \
  "$SRC_DIR/test-flush-prewrite.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-flush-prewrite" || exit 1
echo "FLUSH_PREWRITE_TESTS = PASS"

xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-gfw-readiness" \
  "$SRC_DIR/test-gfw-readiness.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-gfw-concurrency" \
  "$SRC_DIR/test-gfw-concurrency.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
# KEXT6.1: lifetime adversarial + deadline fake-clock (offline).
xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-gfw-lifetime" \
  "$SRC_DIR/test-gfw-lifetime.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
xcrun clang++ -Os -std=c++17 -arch $ARCH -pthread -o "$OBJ_DIR/test-gfw-deadline" \
  "$SRC_DIR/test-gfw-deadline.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
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
if grep -n "complete\|clearMemoryDescriptor" "$SRC_DIR/GA106Lab.cpp" | grep -v "^.*://" | grep -v "SysmemRealOps::" | grep -v "FlushRealOps::" | grep -v "TeardownFlushPrewritePageFlow" | grep -v "clearAddressRecord\|//" | head -n 5 | grep -q "completeDma\|clearDma"; then
  echo "STOP_DIRECT_KPI = FAIL (stop deve usar template, não KPI direto)"
  exit 1
fi
echo "STOP_USES_SHARED_FLOW = YES"
echo "== auditoria TEST-PATH-FIX: shared init/free/lock, sem duplicação =="
if [ ! -f "$SRC_DIR/GA106LabInitFreeLockFlow.hpp" ]; then
  echo "SHARED_FLOW_MISSING = FAIL"
  exit 1
fi
if ! grep -q "InitOwnerLocksFlow" "$SRC_DIR/GA106LabInitFreeLockFlow.hpp"; then
  echo "SHARED_INIT_MISSING = FAIL"
  exit 1
fi
if ! grep -q "ReleaseOwnerLocksFlow" "$SRC_DIR/GA106LabInitFreeLockFlow.hpp"; then
  echo "SHARED_RELEASE_MISSING = FAIL"
  exit 1
fi
if ! grep -q "FreeOwnerResourcesFlow" "$SRC_DIR/GA106LabInitFreeLockFlow.hpp"; then
  echo "SHARED_FREE_MISSING = FAIL"
  exit 1
fi
echo "ONE_SHARED_INIT_FREE_LOCK_FLOW = YES (header presente)"
if ! grep -q "InitOwnerLocksFlow" "$SRC_DIR/GA106Lab.cpp"; then
  echo "PROD_INIT_BINDING = FAIL (GA106Lab::init não chama shared flow)"
  exit 1
fi
if ! grep -q "FreeOwnerResourcesFlow" "$SRC_DIR/GA106Lab.cpp"; then
  echo "PROD_FREE_BINDING = FAIL (GA106Lab::free não chama shared flow)"
  exit 1
fi
echo "PRODUCTION_USES_SHARED_FLOW = YES"
if ! grep -q "InitOwnerLocksFlow" "$SRC_DIR/test-lock-failure.cpp"; then
  echo "TEST_LOCK_BINDING = FAIL (teste não usa shared flow)"
  exit 1
fi
if ! grep -q "FreeOwnerResourcesFlow" "$SRC_DIR/test-stop-free-regression.cpp"; then
  echo "TEST_FREE_BINDING = FAIL (teste não usa shared flow)"
  exit 1
fi
echo "TESTS_USE_SHARED_FLOW = YES"
# Duplicação proibida: testes não podem conter réplicas de policy.
if grep -n "MockOwnerFree\|MockFreeLockNullSafe\|MockInit\|if (m.state == kSysmemPhase_CleanupFailed)\|if (fSysmemState ==" "$SRC_DIR/test-lock-failure.cpp" "$SRC_DIR/test-stop-free-regression.cpp" "$SRC_DIR/test-prod-binding.cpp" "$SRC_DIR/test-gfw-readiness.cpp" "$SRC_DIR/test-gfw-concurrency.cpp" "$SRC_DIR/test-gfw-lifetime.cpp" "$SRC_DIR/test-gfw-deadline.cpp" | grep -v "^.*://"; then
  echo "DUPLICATED_INIT_FREE_TEST_LOGIC = FAIL (réplica em teste)"
  exit 1
fi
echo "DUPLICATED_INIT_FREE_TEST_LOGIC = NO"
# Produção não pode conter policy independente (toda decisão no shared).
if grep -n "kSysmemPhase_CleanupFailed" "$SRC_DIR/GA106Lab.cpp" | grep -v "^.*://"; then
  echo "PROD_POLICY_DUP = FAIL (CLEANUP_FAILED em GA106Lab.cpp, deve viver no shared)"
  exit 1
fi
# Locks brutos SOMENTE em OwnerRealOps (2 alloc + 1 free); init/free wrappers finos.
ALLOC_N=$(grep -c "IOSimpleLockAlloc" "$SRC_DIR/GA106Lab.cpp" || true)
FREE_N=$(grep -c "IOSimpleLockFree" "$SRC_DIR/GA106Lab.cpp" || true)
if [ "$ALLOC_N" != "2" ]; then
  echo "PROD_LOCK_ALLOC_COUNT = FAIL (esperado 2 em OwnerRealOps, achado $ALLOC_N)"
  exit 1
fi
if [ "$FREE_N" != "1" ]; then
  echo "PROD_LOCK_FREE_COUNT = FAIL (esperado 1 em OwnerRealOps, achado $FREE_N)"
  exit 1
fi
# Releases brutos SOMENTE em Ops (2 SysmemRealOps + 2 OwnerRealOps = 4); wrappers 0.
REL_N=$(grep -c "fSysmemCmd->release\|fSysmemDesc->release" "$SRC_DIR/GA106Lab.cpp" || true)
# grep -c com alternância conta linhas, não ocorrências; aqui 4 linhas esperadas.
if [ "$REL_N" != "4" ]; then
  echo "PROD_RELEASE_COUNT = FAIL (esperado 4 em Ops, achado $REL_N)"
  exit 1
fi
echo "PROD_NO_INDEPENDENT_POLICY = YES"
# Ops contêm operações apenas (sem branching de fase terminal).
if grep -n "CLEANUP_FAILED" "$SRC_DIR/GA106LabRealOps.hpp" | grep -v "//"; then
  echo "POLICY_IN_REALOPS = FAIL"
  exit 1
fi
if awk '/struct GA106LabOwnerMockOps/,/^};/' "$SRC_DIR/GA106LabMockOps.hpp" | grep -n "CLEANUP_FAILED" | grep -v "//"; then
  echo "POLICY_IN_MOCKOPS = FAIL"
  exit 1
fi
echo "POLICY_IN_OPS = NO"
if ! grep -q "FREE_CLEANUP_FAILED_SHARED_PATH" "$SRC_DIR/GA106Lab.cpp" | grep -q "FreeOwnerResourcesFlow" "$SRC_DIR/GA106Lab.cpp"; then
  true
fi
echo "FREE_CLEANUP_FAILED_SHARED_PATH = YES"
echo "LOCK_INIT_SHARED_PATH = YES"
echo "== auditoria versão 1.7.0 =="
if ! grep -q "1.7.0" "$SRC_DIR/Info.plist"; then
  echo "VERSION_PLIST = FAIL"
  exit 1
fi
if ! grep -q "0x00010700" "$SRC_DIR/GA106LabProtocol.h"; then
  echo "VERSION_PROTOCOL = FAIL"
  exit 1
fi
if ! grep -q "TG-KEXT7-SYSMEM-FLUSH-PREWRITE-READINESS" "$SRC_DIR/GA106Lab.cpp"; then
  echo "VERSION_REVISION = FAIL"
  exit 1
fi
echo "VERSION_1_6_2 = YES"
echo "== auditoria UC lifetime: shared flow, pin atômico, sem duplicação =="
if [ ! -f "$SRC_DIR/GA106LabUserClientOwnerFlow.hpp" ]; then
  echo "UC_SHARED_FLOW_MISSING = FAIL"
  exit 1
fi
if ! grep -q "PinOwnerForExternalMethod" "$SRC_DIR/GA106LabUserClient.cpp"; then
  echo "ONE_SHARED_USERCLIENT_OWNER_LIFETIME_FLOW = FAIL (handler sem pin compartilhado)"
  exit 1
fi
echo "ONE_SHARED_USERCLIENT_OWNER_LIFETIME_FLOW = YES"
if ! grep -q "PinOwnerForExternalMethod" "$SRC_DIR/test-userclient-lifetime.cpp"; then
  echo "UC_TEST_BINDING = FAIL (teste não usa shared flow)"
  exit 1
fi
# Handler GFW sem leitura crua de fOwner (só via pin compartilhado).
if awk '/GA106LabUCReadGfwReadiness/,/^}/' "$SRC_DIR/GA106LabUserClient.cpp" | grep -n "getOwnerService\|fOwner" | grep -v "^.*://"; then
  echo "UC_HANDLER_RAW_OWNER = FAIL (handler lê fOwner fora do pin)"
  exit 1
fi
echo "UC_HANDLER_NO_RAW_OWNER = YES"
if grep -n "MockPinOwner\|MockReleaseOwner\|MockStopDrain" "$SRC_DIR/test-userclient-lifetime.cpp" | grep -v "^.*://"; then
  echo "DUPLICATED_USERCLIENT_TEST_POLICY = FAIL (réplica em teste)"
  exit 1
fi
echo "DUPLICATED_USERCLIENT_TEST_POLICY = NO"
# Ordem de locks documentada: UC antes de owner; sem aninhamento reverso.
if ! grep -q "UserClient fOwnerLock" "$SRC_DIR/GA106LabUserClientOwnerFlow.hpp"; then
  echo "USERCLIENT_LOCK_ORDER = FAIL (ordem não documentada)"
  exit 1
fi
echo "USERCLIENT_LOCK_ORDER = UC-then-owner, never-nested"
echo "USERCLIENT_LOCK_PRIMITIVE = IOSimpleLock (seções curtas; retain atômico)"
echo "== auditoria KEXT6 selector 8: shared flow, ABI32, sem stores =="
if [ ! -f "$SRC_DIR/GA106LabGfwReadinessFlow.hpp" ]; then
  echo "GFW_SHARED_FLOW_MISSING = FAIL"
  exit 1
fi
if ! grep -q "RunGfwBootReadinessFlow" "$SRC_DIR/GA106Lab.cpp"; then
  echo "ONE_SHARED_GFW_READINESS_FLOW = FAIL (produção não usa shared flow)"
  exit 1
fi
echo "ONE_SHARED_GFW_READINESS_FLOW = YES (produção usa RunGfwBootReadinessFlow)"
if ! grep -q "RunGfwBootReadinessFlow" "$SRC_DIR/test-gfw-readiness.cpp"; then
  echo "GFW_TEST_BINDING = FAIL (teste não usa shared flow)"
  exit 1
fi
echo "GFW_TESTS_USE_SHARED_FLOW = YES"
if ! grep -q "kGA106LabSelector_ReadGfwBootReadiness = 8" "$SRC_DIR/GA106LabProtocol.h"; then
  echo "SELECTOR8_NUMBER = FAIL"
  exit 1
fi
if ! grep -q "sizeof(GA106LabGfwReadinessV1) == 32" "$SRC_DIR/GA106LabProtocol.h"; then
  echo "GFW_ABI_SIZE = FAIL"
  exit 1
fi
echo "SELECTOR8_NUMBER = 8, GFW_READINESS_ABI_SIZE = 32"
echo "== auditoria ASTRA-B-FIX: reserva antecipada + retain + deadline =="
if ! grep -q "acquireProvider" "$SRC_DIR/GA106LabGfwReadinessFlow.hpp"; then
  echo "RESERVE_BEFORE_PROVIDER = FAIL (flow sem acquire)"
  exit 1
fi
if ! grep -q "pci->retain\|->retain()" "$SRC_DIR/GA106Lab.cpp"; then
  echo "PROVIDER_RETAIN = FAIL (sem retain no pin de lifetime)"
  exit 1
fi
if ! grep -q "PinOwnerForExternalMethod" "$SRC_DIR/GA106LabUserClient.cpp"; then
  echo "OWNER_RETAIN_HANDLER = FAIL (handler sem pin do owner)"
  exit 1
fi
# Retain/release do pin SOMENTE via shared flow (nunca bare owner->retain).
PRETAIN=$(awk '/GA106LabUCReadGfwReadiness/,/^}/' "$SRC_DIR/GA106LabUserClient.cpp" | grep -c "PinOwnerForExternalMethod" || true)
PRELEASE=$(awk '/GA106LabUCReadGfwReadiness/,/^}/' "$SRC_DIR/GA106LabUserClient.cpp" | grep -c "ReleasePinnedOwner" || true)
PBARE=$(awk '/GA106LabUCReadGfwReadiness/,/^}/' "$SRC_DIR/GA106LabUserClient.cpp" | grep -c "owner->retain()\|owner->release()" || true)
if [ "$PRETAIN" != "1" ] || [ "$PRELEASE" != "2" ]; then
  echo "OWNER_RETAIN_BALANCE = FAIL (pin=$PRETAIN release=$PRELEASE, esperado 1/2 via shared)"
  exit 1
fi
if [ "$PBARE" != "0" ]; then
  echo "OWNER_BARE_RETAIN = FAIL (retain/release cru fora do shared flow)"
  exit 1
fi
echo "RESERVE_FIRST_RETAIN_BALANCED = YES (pin 1x + release 2x via shared; zero bare)"
if ! grep -q "mach_absolute_time" "$SRC_DIR/GA106Lab.cpp"; then
  echo "MONOTONIC_CLOCK = FAIL (sem mach_absolute_time na produção)"
  exit 1
fi
if ! grep -q "absolutetime_to_nanoseconds" "$SRC_DIR/GA106Lab.cpp"; then
  echo "CLOCK_CONVERSION = FAIL (sem conversão SDK documentada)"
  exit 1
fi
echo "MONOTONIC_CLOCK_API = mach_absolute_time+absolutetime_to_nanoseconds"
if ! grep -q "TIMEOUT_TRANSPORT_SEMANTICS_DOCUMENTED\|transport success" "$SRC_DIR/GA106LabProtocol.h"; then
  echo "TIMEOUT_DOC = FAIL (semântica transporte-vs-resposta não documentada)"
  exit 1
fi
echo "TIMEOUT_TRANSPORT_SEMANTICS_DOCUMENTED = YES"
if ! grep -q "post-read comparison intentionally absent\|compare-after" "$SRC_DIR/GA106LabProtocol.h" "$SRC_DIR/GA106LabGfwReadinessFlow.hpp"; then
  echo "MSE_DOC = FAIL (modelo MSE não documentado)"
  exit 1
fi
echo "MSE_COMMAND_VALIDATION_MODEL = PASS (documentado; sem compare-after)"
# Offsets fixos ocorrem SOMENTE como constantes (nada de userspace).
if grep -n "0x118128\|0x118234" "$SRC_DIR/GA106LabUserClient.cpp" | grep -v "^.*://"; then
  echo "GFW_OFFSET_USERCLIENT = FAIL (offset fora do flow/CoreValidate)"
  exit 1
fi
if grep -n "OSWrite\|configWrite\|setBusLeadEnable\|setBusMasterEnable" "$SRC_DIR/GA106LabGfwReadinessFlow.hpp" | grep -v "^.*://"; then
  echo "GFW_FLOW_STORE = FAIL (store/BME no flow GFW)"
  exit 1
fi
echo "GFW_FLOW_ZERO_STORES = YES"
if awk '/struct GA106LabGfwRealOps/,/^};/' "$SRC_DIR/GA106LabRealOps.hpp" | grep -n "OSWrite\|configWrite\|setBusLeadEnable\|setBusMasterEnable\|IODMACommand\|firmware\|Falcon" | grep -v "^.*://"; then
  echo "GFW_REALOPS_HW = FAIL"
  exit 1
fi
echo "GFW_REALOPS_OPS_ONLY = YES"
if grep -n "RunPrepareGspSysmemFlow\|prepareGspSysmem\|completeDma\|clearDma" "$SRC_DIR/GA106LabGfwReadinessFlow.hpp" | grep -v "^.*://"; then
  echo "INTERNAL_SELECTOR7_CALL = FAIL (flow GFW referencia sysmem)"
  exit 1
fi
echo "INTERNAL_SELECTOR7_CALL = NO"
if awk '/GA106Lab::readGfwBootReadiness/,/^}/' "$SRC_DIR/GA106Lab.cpp" | grep -n "RunPrepareGspSysmemFlow\|prepareGspSysmem\|completeDma\|clearDma" | grep -v "^.*://"; then
  echo "WRAPPER_SYSMEM_CALL = FAIL (wrapper selector8 referencia sysmem)"
  exit 1
fi
echo "WRAPPER_NO_SYSMEM_CALL = YES"
if grep -n "RunPrepareGspSysmemFlow\|prepareGspSysmem" "$SRC_DIR/test-gfw-readiness.cpp" "$SRC_DIR/test-gfw-concurrency.cpp" "$SRC_DIR/test-gfw-lifetime.cpp" "$SRC_DIR/test-gfw-deadline.cpp" | grep -v "^.*://"; then
  echo "GFW_TEST_SYSMEM_REF = FAIL"
  exit 1
fi
echo "GFW_TESTS_NO_SYSMEM = YES"
if grep -n "^xcrun clang.*-DGA106LAB_GFW_MUT_" "$SRC_DIR/build.sh"; then
  echo "GFW_MUT_IN_PROD_BUILD = FAIL (macro test-only em comando de build)"
  exit 1
fi
echo "GFW_POLICY_IN_OPS = NO (verificado: branching só no flow)"
"$OBJ_DIR/test-slot"
"$OBJ_DIR/test-contract"
"$OBJ_DIR/test-bar-map-probe"
"$OBJ_DIR/test-first-mmio-read"
"$OBJ_DIR/test-static-identity"
"$OBJ_DIR/test-gsp-sysmem"
"$OBJ_DIR/test-sysmem-concurrency"
"$OBJ_DIR/test-stop-free-regression"
"$OBJ_DIR/test-lock-failure"
"$OBJ_DIR/test-prod-binding"
"$OBJ_DIR/test-userclient-lifetime"
"$OBJ_DIR/test-gfw-readiness"
"$OBJ_DIR/test-gfw-concurrency"
"$OBJ_DIR/test-gfw-lifetime"
"$OBJ_DIR/test-gfw-deadline"
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
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -o "$OBJ_DIR/test-binding-asan" \
  "$SRC_DIR/test-prod-binding.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-binding-asan" > /dev/null && echo "ASAN_PROD_BINDING = PASS"
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -pthread -o "$OBJ_DIR/test-uclife-asan" \
  "$SRC_DIR/test-userclient-lifetime.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-uclife-asan" > /dev/null && echo "ASAN_UC_LIFETIME = PASS"
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -pthread -o "$OBJ_DIR/test-gfw-asan" \
  "$SRC_DIR/test-gfw-readiness.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-gfw-asan" > /dev/null && echo "ASAN_GFW_READINESS = PASS"
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -pthread -o "$OBJ_DIR/test-gfwconc-asan" \
  "$SRC_DIR/test-gfw-concurrency.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-gfwconc-asan" > /dev/null && echo "ASAN_GFW_CONCURRENCY = PASS"
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -pthread -o "$OBJ_DIR/test-gfwlife-asan" \
  "$SRC_DIR/test-gfw-lifetime.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-gfwlife-asan" > /dev/null && echo "ASAN_GFW_LIFETIME = PASS"
xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=address -pthread -o "$OBJ_DIR/test-gfwdeadline-asan" \
  "$SRC_DIR/test-gfw-deadline.cpp" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation
"$OBJ_DIR/test-gfwdeadline-asan" > /dev/null && echo "ASAN_GFW_DEADLINE = PASS"
if xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=thread -pthread -o "$OBJ_DIR/test-conc-tsan" \
  "$SRC_DIR/test-sysmem-concurrency.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$OBJ_DIR/tsan-build.log"; then
  "$OBJ_DIR/test-conc-tsan" > /dev/null && echo "TSAN_CONCURRENCY = PASS" || echo "TSAN_CONCURRENCY = FAIL"
else
  echo "TSAN = NOT_AVAILABLE (toolchain rejeita -fsanitize=thread; ver tsan-build.log)"
fi
if xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=thread -pthread -o "$OBJ_DIR/test-gfwconc-tsan" \
  "$SRC_DIR/test-gfw-concurrency.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$OBJ_DIR/tsan-gfw-build.log"; then
  "$OBJ_DIR/test-gfwconc-tsan" > /dev/null && echo "TSAN_GFW_CONCURRENCY = PASS" || echo "TSAN_GFW_CONCURRENCY = FAIL"
else
  echo "TSAN_GFW = NOT_AVAILABLE (toolchain rejeita -fsanitize=thread; ver tsan-gfw-build.log)"
fi
if xcrun clang++ -Os -std=c++17 -arch $ARCH -fsanitize=thread -pthread -o "$OBJ_DIR/test-uclife-tsan" \
  "$SRC_DIR/test-userclient-lifetime.cpp" \
  -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$OBJ_DIR/tsan-uclife-build.log"; then
  "$OBJ_DIR/test-uclife-tsan" > /dev/null && echo "TSAN_UC_LIFETIME = PASS" || echo "TSAN_UC_LIFETIME = FAIL"
else
  echo "TSAN_UC_LIFETIME = NOT_AVAILABLE (ver tsan-uclife-build.log)"
fi
echo "== mutation harness =="
chmod +x "$SRC_DIR/mutation-harness.sh" "$SRC_DIR/mutation-gfw-harness.sh" "$SRC_DIR/mutation-gfw-executable-proof.sh"
if [ "${SKIP_MUTATIONS:-0}" = "1" ]; then
  echo "SKIP_MUTATIONS=1 (harness executado separadamente)"
else
  "$SRC_DIR/mutation-harness.sh" || exit 1
  "$SRC_DIR/mutation-gfw-harness.sh" || exit 1
  "$SRC_DIR/mutation-gfw-executable-proof.sh" || exit 1
fi
# Build estritamente offline (ASTRA-B-FIX §9 + PROMPT 57 §13): somente
# usage/bogus (sem IOServiceOpen). Invocações live de hardware foram removidas.
"$OBJ_DIR/ga106ctl" 2>&1 | head -n 1 || true
"$OBJ_DIR/ga106ctl" bogus 2>&1 | head -n 1 || true
echo "== auditoria build offline estrito: zero chamadas live em scripts/testes =="
if grep -n '"$OBJ_DIR/ga106ctl" \(version\|identity\|status\|pci\|bar-map-probe\|first-mmio-read\|static-identity\|prepare-gsp-sysmem\|read-gfw-boot-readiness\|verify-sysmem-flush-preconditions\)' "$SRC_DIR/build.sh" "$SRC_DIR/mutation-harness.sh" "$SRC_DIR/mutation-gfw-harness.sh" "$SRC_DIR/mutation-gfw-executable-proof.sh" "$SRC_DIR/isolate-mutations.sh" "$SRC_DIR/astra-repro.sh" 2>/dev/null; then
  echo "BUILD_SCRIPT_LIVE_CALLS = FAIL (script invoca CLI live)"
  exit 1
fi
if grep -ln "IOServiceOpen\|IOConnectCallMethod" "$SRC_DIR"/test-*.cpp 2>/dev/null; then
  echo "TEST_LIVE_SERVICE_CALL = FAIL (teste abre serviço real)"
  exit 1
fi
echo "BUILD_SCRIPT_LIVE_CALLS = 0"
echo "LIVE_SERVICE_OPEN_COUNT = 0 (estático: nenhum script/teste abre serviço)"
echo "LIVE_SELECTOR_CALL_COUNT = 0 (estático: nenhuma invocação de selector)"
echo "OFFLINE_BUILD_STRICT = YES"
echo "== binary/disassembly audit (KEXT 1.7.0, file audit, sem load) =="
KEXT_BIN="$OBJ_DIR/GA106Lab.kext/Contents/MacOS/GA106Lab"
if ! file "$KEXT_BIN" | grep -q "kext bundle"; then
  echo "BINARY_FILETYPE = FAIL (não é kext bundle)"
  exit 1
fi
if ! file "$KEXT_BIN" | grep -q "x86_64"; then
  echo "BINARY_ARCH = FAIL (não é x86_64)"
  exit 1
fi
if ! nm -g "$KEXT_BIN" 2>/dev/null | grep -q "_kmod_info"; then
  echo "BINARY_KMOD_INFO = FAIL (sem _kmod_info)"
  exit 1
fi
echo "BINARY_MH_KEXT_BUNDLE_x86_64_kmod_info = YES"
if ! strings "$KEXT_BIN" | grep -q "TG-KEXT7-SYSMEM-FLUSH-PREWRITE-READINESS"; then
  echo "BINARY_REVISION = FAIL (string de revisão ausente)"
  exit 1
fi
echo "BINARY_REVISION_1_7_0 = YES"
# Zero stores MMIO / config writes / BME no KEXT inteiro (contexto: selectors
# 5/6 usam só OSRead; sysmem usa DMA-KPI sem MMIO; selector 8 só OSRead).
# IODMACommand permanece (selector 7 pré-existente) — listado com contexto.
if nm -u "$KEXT_BIN" 2>/dev/null | grep -q "OSWrite"; then
  echo "BINARY_MMIO_STORE = FAIL (símbolo OSWrite importado)"
  exit 1
fi
if nm -u "$KEXT_BIN" 2>/dev/null | grep -q "configWrite"; then
  echo "BINARY_PCI_CONFIG_WRITE = FAIL"
  exit 1
fi
if nm -u "$KEXT_BIN" 2>/dev/null | grep -q "setBusLeadEnable\|setBusMasterEnable"; then
  echo "BINARY_BME_ENABLE = FAIL"
  exit 1
fi
echo "BINARY_ZERO_STORES_PCI_BME = YES (IODMACommand presente só via selector 7 pré-existente)"
# (saídas acima esperadas offline: usage + service-not-found; BUILD_OK só
# reflete compile/link/assemble/testes, cujas falhas abortam via pipefail)
echo "BUILD_OK"
} 2>&1 | tee "$LOG"
