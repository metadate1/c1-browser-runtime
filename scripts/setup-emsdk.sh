#!/usr/bin/env bash
set -euo pipefail

EMSDK_DIR="${EMSDK_DIR:-$HOME/.cache/emsdk}"
EMSDK_VERSION="${EMSDK_VERSION:-6.0.2}"

if [[ ! -x "$EMSDK_DIR/emsdk" ]]; then
  mkdir -p "$(dirname "$EMSDK_DIR")"
  git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi

"$EMSDK_DIR/emsdk" install "$EMSDK_VERSION"
"$EMSDK_DIR/emsdk" activate "$EMSDK_VERSION"

echo "Emscripten $EMSDK_VERSION is ready at $EMSDK_DIR"
