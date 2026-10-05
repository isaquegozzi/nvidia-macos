#!/usr/bin/env bash
# Executa apenas modelos e contratos portaveis. Nao acessa GPU, IOKit ou EFI.
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL_ROOT="$PROJECT_ROOT/macos/research/GA106Lab-kext8-production-architecture-src"
TEST_BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/ga106-model-tests.XXXXXX")"
trap 'rm -rf "$TEST_BUILD_DIR"' EXIT
CXX_BIN="${CXX:-c++}"

TEST_SOURCES=(
  "$MODEL_ROOT/test-production-phase.cpp"
  "$MODEL_ROOT/test-channel-generation-contract.cpp"
  "$MODEL_ROOT/test-seq-fuzz-contract.cpp"
  "$MODEL_ROOT/test-completion-chain-contract.cpp"
  "$MODEL_ROOT/test-contract-fuzz.cpp"
  "$PROJECT_ROOT/macos/research/SHADOW-P78-VM-MMU-SIM/test-vm.cpp"
  "$PROJECT_ROOT/macos/research/SHADOW-P79-COMMAND-SUBMISSION-SIM/test-submission.cpp"
)

for TEST_SOURCE in "${TEST_SOURCES[@]}"; do
  TEST_NAME="$(basename "$TEST_SOURCE" .cpp)"
  printf '\n%s\n' "== $TEST_NAME =="
  "$CXX_BIN" -std=c++17 -O2 -Wall -Wextra -I"$(dirname "$TEST_SOURCE")" \
    -I"$PROJECT_ROOT/macos/research/SHADOW-P78-VM-MMU-SIM" \
    -I"$PROJECT_ROOT/macos/research/SHADOW-P79-COMMAND-SUBMISSION-SIM" \
    "$TEST_SOURCE" -o "$TEST_BUILD_DIR/$TEST_NAME"
  "$TEST_BUILD_DIR/$TEST_NAME"
done

printf '\n%s\n' "PASS: ${#TEST_SOURCES[@]} suites de modelos offline; nenhum acesso a hardware."
