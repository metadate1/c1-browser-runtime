#!/usr/bin/env bash
set -euo pipefail

EMSDK_DIR="${EMSDK_DIR:-$HOME/.cache/emsdk}"
EMSDK_VERSION="${EMSDK_VERSION:-6.0.2}"
EMSDK_REPOSITORY_REVISION="${EMSDK_REPOSITORY_REVISION:-ca38f487f28b7c3c16f8f70cd0e012099ac4b7e2}"

if [[ ! -d "$EMSDK_DIR/.git" ]]; then
  mkdir -p "$(dirname "$EMSDK_DIR")"
  git clone --filter=blob:none --no-checkout https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi

git -C "$EMSDK_DIR" fetch --depth 1 origin "$EMSDK_REPOSITORY_REVISION"
git -C "$EMSDK_DIR" checkout --detach "$EMSDK_REPOSITORY_REVISION"

"$EMSDK_DIR/emsdk" install "$EMSDK_VERSION"
"$EMSDK_DIR/emsdk" activate "$EMSDK_VERSION"

echo "Emscripten $EMSDK_VERSION is ready at $EMSDK_DIR"
