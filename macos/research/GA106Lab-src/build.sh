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
echo "== compile =="
xcrun clang++ -arch $ARCH -c \
  -Os -mkernel $VERSMIN \
  -fno-builtin -fno-stack-protector -fno-common -fapple-kext \
  -nostdinc \
  -I"$KHDR" \
  -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT \
  -o "$OBJ_DIR/GA106Lab.o" "$SRC_DIR/GA106Lab.cpp"
echo "== link (MH_KEXT_BUNDLE, sem -r) =="
xcrun clang++ -arch $ARCH -mkernel $VERSMIN \
  -nostdlib -Xlinker -kext \
  -o "$OBJ_DIR/GA106Lab.kext/Contents/MacOS/GA106Lab" "$OBJ_DIR/GA106Lab.o" \
  -lkmodc++ -lkmod -lcc_kext
echo "== assemble =="
cp "$SRC_DIR/Info.plist" "$OBJ_DIR/GA106Lab.kext/Contents/Info.plist"
plutil -lint "$OBJ_DIR/GA106Lab.kext/Contents/Info.plist"
echo "BUILD_OK"
} 2>&1 | tee "$LOG"
