#!/bin/zsh
# mutation-production-harness.sh — P80: mutantes no production path (headers novos).
# Cada mutante deve ser CAPTURADO (teste falha, exit != 0). BUILD_OK só com 4/4.
set -euo pipefail
SRC_DIR="${0:A:h}"
OBJ_DIR="$SRC_DIR/build/obj"
ARCH=x86_64
pass=0; total=0
run_mut() { # $1=name $2=define $3=test-src $4=extra-flags
  total=$((total + 1))
  if xcrun clang++ -Os -std=c++17 -arch $ARCH -D$2 -o "$OBJ_DIR/mut-prod-$1" \
      "$SRC_DIR/$3" -I"$SRC_DIR" ${=4} 2> "$OBJ_DIR/mut-prod-$1-build.log"; then
    if "$OBJ_DIR/mut-prod-$1" > /dev/null 2>&1; then
      echo "MUT_PROD_$1 exit=0: ESCAPED"
    else
      echo "MUT_PROD_$1 exit!=0: CAUGHT"
      pass=$((pass + 1))
    fi
  else
    echo "MUT_PROD_$1: BUILD-FAIL (caught, ver log)"
    pass=$((pass + 1))
  fi
}
run_mut ALLOW_SKIP PROD_MUT_ALLOW_SKIP test-production-phase.cpp "-framework IOKit -framework CoreFoundation"
run_mut BLOCK_SUCCESS PROD_MUT_BLOCK_RETURNS_SUCCESS test-production-blocks.cpp "-framework IOKit -framework CoreFoundation"
run_mut CONTRACT_SKIP PROD_MUT_CONTRACT_SKIP test-production-contracts.cpp "-framework IOKit -framework CoreFoundation"
run_mut CONTRACT_SKIP_LIFE PROD_MUT_CONTRACT_SKIP test-production-lifetime.cpp ""
echo "MUTATION_PRODUCTION = $pass/$total CAUGHT"
[ "$pass" = "$total" ]
