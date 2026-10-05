#!/bin/zsh
# isolate-mutations.sh — roda mutações suspeitas isoladas, cada binário com
# timeout rígido (watchdog portátil; macOS não tem `timeout`).
# Uso: ./isolate-mutations.sh MUT1 [MUT2 ...]
# HANG (kill pelo watchdog, rc=137/143) conta como CAUGHT somente quando a
# mutação remove sincronização/reserva deliberadamente (NO_INPROGRESS,
# SECOND_ALLOC); nas demais, HANG = bug do harness/teste (HARNESS-FAIL).
set -uo pipefail
SRC_DIR="${0:A:h}"
ISO_DIR="$SRC_DIR/build/isolate"
mkdir -p "$ISO_DIR"
ARCH=x86_64
TIMEOUT_SECS=90

run_timeout() {
    secs=$1; out=$2; shift 2
    "$@" > "$out" 2>&1 & pid=$!
    (sleep "$secs"; kill -9 $pid 2>/dev/null) & wd=$!
    wait $pid 2>/dev/null; rc=$?
    kill $wd 2>/dev/null; wait $wd 2>/dev/null || true
    return $rc
}

if [ "$#" -eq 0 ]; then
    echo "usage: $0 MUT [...]"
    exit 2
fi

OVERALL=0
for M in "$@"; do
    echo "===== $M ====="
    xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M \
      -o "$ISO_DIR/test-gsp-$M" \
      "$SRC_DIR/test-gsp-sysmem.cpp" "$SRC_DIR/ga106ctl-validate.c" \
      -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2> "$ISO_DIR/$M-build.log" || { echo "$M: BUILD_FAIL"; OVERALL=1; continue; }
    xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M -pthread \
      -o "$ISO_DIR/test-conc-$M" \
      "$SRC_DIR/test-sysmem-concurrency.cpp" \
      -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$ISO_DIR/$M-build.log" || { echo "$M: BUILD_FAIL"; OVERALL=1; continue; }
    xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M -pthread \
      -o "$ISO_DIR/test-stopfree-$M" \
      "$SRC_DIR/test-stop-free-regression.cpp" \
      -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$ISO_DIR/$M-build.log" || { echo "$M: BUILD_FAIL"; OVERALL=1; continue; }
    xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M \
      -o "$ISO_DIR/test-lockfail-$M" \
      "$SRC_DIR/test-lock-failure.cpp" \
      -I"$SRC_DIR" 2>> "$ISO_DIR/$M-build.log" || { echo "$M: BUILD_FAIL"; OVERALL=1; continue; }
    xcrun clang++ -Os -std=c++17 -arch $ARCH -D$M \
      -o "$ISO_DIR/test-binding-$M" \
      "$SRC_DIR/test-prod-binding.cpp" \
      -I"$SRC_DIR" -framework IOKit -framework CoreFoundation 2>> "$ISO_DIR/$M-build.log" || { echo "$M: BUILD_FAIL"; OVERALL=1; continue; }
    R1=0; R2=0; R3=0; R4=0; R5=0
    run_timeout $TIMEOUT_SECS "$ISO_DIR/$M-run1.log" "$ISO_DIR/test-gsp-$M" || R1=$?
    run_timeout $TIMEOUT_SECS "$ISO_DIR/$M-run2.log" "$ISO_DIR/test-conc-$M" || R2=$?
    run_timeout $TIMEOUT_SECS "$ISO_DIR/$M-run3.log" "$ISO_DIR/test-stopfree-$M" || R3=$?
    run_timeout $TIMEOUT_SECS "$ISO_DIR/$M-run4.log" "$ISO_DIR/test-lockfail-$M" || R4=$?
    run_timeout $TIMEOUT_SECS "$ISO_DIR/$M-run5.log" "$ISO_DIR/test-binding-$M" || R5=$?
    S1="exit=$R1"; S2="exit=$R2"; S3="exit=$R3"; S4="exit=$R4"; S5="exit=$R5"
    [ "$R1" = "137" ] && S1="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R1" = "143" ] && S1="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R2" = "137" ] && S2="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R2" = "143" ] && S2="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R3" = "137" ] && S3="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R3" = "143" ] && S3="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R4" = "137" ] && S4="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R4" = "143" ] && S4="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R5" = "137" ] && S5="HANG(killed@ ${TIMEOUT_SECS}s)"
    [ "$R5" = "143" ] && S5="HANG(killed@ ${TIMEOUT_SECS}s)"
    echo "$M run1: $S1"
    echo "$M run2: $S2"
    echo "$M run3: $S3"
    echo "$M run4: $S4"
    echo "$M run5: $S5"
    tail -n 3 "$ISO_DIR/$M-run1.log" 2>/dev/null | head -n 3
    case "$M" in
      *NO_INPROGRESS|*SECOND_ALLOC)
        if [ "$R1" != "0" ] || [ "$R2" != "0" ] || [ "$R3" != "0" ] || [ "$R4" != "0" ] || [ "$R5" != "0" ]; then echo "$M: CAUGHT (incl. HANG-esperado se houver)"; else echo "$M: ESCAPED"; OVERALL=1; fi ;;
      *)
        if { [ "$R1" = "137" ] || [ "$R1" = "143" ] || [ "$R2" = "137" ] || [ "$R2" = "143" ] || [ "$R3" = "137" ] || [ "$R3" = "143" ] || [ "$R4" = "137" ] || [ "$R4" = "143" ] || [ "$R5" = "137" ] || [ "$R5" = "143" ]; }; then
          echo "$M: HARNESS-FAIL (hang inesperado — bug, não PASS)"; OVERALL=1
        elif [ "$R1" != "0" ] || [ "$R2" != "0" ] || [ "$R3" != "0" ] || [ "$R4" != "0" ] || [ "$R5" != "0" ]; then echo "$M: CAUGHT"; else echo "$M: ESCAPED"; OVERALL=1; fi ;;
    esac
done
echo "ISOLATE_DONE rc=$OVERALL"
exit $OVERALL
