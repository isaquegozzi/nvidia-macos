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
  -framework IOKit -framework CoreFoundation
echo "== compile+run testes offline (modelo+harness, sem kernel) =="
xcrun clang -Os -arch $ARCH -o "$OBJ_DIR/test-slot" "$SRC_DIR/test-slot.c"
xcrun clang -Os -arch $ARCH -o "$OBJ_DIR/test-contract" \
  "$SRC_DIR/test-contract.c" "$SRC_DIR/ga106ctl-validate.c" \
  -I"$SRC_DIR"
echo "== regressão bug 00:00.0: forma 2-arg/union zerada PROIBIDA no fonte =="
if grep -n "kIOPCIConfigSpace,\|kSpace\|configRead.*(.*," "$SRC_DIR/GA106Lab.cpp" | grep -v "^.*://"; then
  echo "ZEROED_SPACE_REGRESSION_TEST = FAIL (forma antiga presente)"
  exit 1
fi
echo "ZEROED_SPACE_REGRESSION_TEST = PASS (só wrappers 1-arg)"
"$OBJ_DIR/test-slot"
"$OBJ_DIR/test-contract"
"$OBJ_DIR/ga106ctl" 2>&1 | head -n 1 || true
"$OBJ_DIR/ga106ctl" bogus 2>&1 | head -n 1 || true
"$OBJ_DIR/ga106ctl" version 2>&1 | head -n 1 || true
# (saídas acima esperadas offline: usage + service-not-found; BUILD_OK só
# reflete compile/link/assemble/testes, cujas falhas abortam via pipefail)
echo "BUILD_OK"
} 2>&1 | tee "$LOG"
