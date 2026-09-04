#!/usr/bin/env bash
# build.sh — build reproduzível da Fase 0 (lib comum + smoke tests).
# Uso: ./scripts/build.sh [--clean] [--no-test]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT/build}"
GEN="${CMAKE_GENERATOR:-}"
CLEAN=0
RUN_TESTS=1

for arg in "$@"; do
  case "$arg" in
    --clean) CLEAN=1 ;;
    --no-test) RUN_TESTS=0 ;;
    -h|--help)
      echo "Uso: $0 [--clean] [--no-test]"
      exit 0
      ;;
    *) echo "arg desconhecido: $arg" >&2; exit 2 ;;
  esac
done

if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
  rm -rf "$BUILD_DIR"
fi

CONFIGURE_ARGS=(-S "$ROOT" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=RelWithDebInfo)
if [[ -n "$GEN" ]]; then
  CONFIGURE_ARGS+=(-G "$GEN")
fi

cmake "${CONFIGURE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel

if [[ "$RUN_TESTS" -eq 1 ]]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure
fi
