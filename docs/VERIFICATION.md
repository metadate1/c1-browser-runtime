# Verification record

This is a dated engineering log, not a statement of legal permission or complete retail parity.
All game data and generated outputs named below remained local and ignored.

## 2026-09-03 public-source preparation

The preparation tree was checked on Ubuntu 24.04 with:

- Node.js `22.16.0`;
- GCC `13.3.0`;
- `emsdk` commit `ca38f487f28b7c3c16f8f70cd0e012099ac4b7e2`;
- Emscripten `6.0.2` (`7a2d97d627ff4945eae28847ce0387ac52b92c09`); and
- Google Chrome `151.0.7922.137` under Xvfb with SwiftShader.

The following checks passed:

```bash
bash -n scripts/check-public-release.sh scripts/setup-emsdk.sh
node --check <each tracked .js and .mjs file>
bash scripts/check-public-release.sh
npm test
npm run setup
npm run build
git diff --check
```

The native suite compiled and executed every C and JavaScript test target in `make test`. The C
compiler still reports inherited warnings that the current Makefile does not promote to errors;
no test executable or sanitizer check failed.

The Wasm build produced these ignored artifacts:

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `dist/c1.mjs` | 386,704 | `12f47f6aa1b4b506f1fd121dff1174b74c12906433043e07f9bee8dc2f8c52e4` |
| `dist/c1.wasm` | 703,871 | `958616b3b55eafb66547371746091d4417811d2896ee90d4160a03c17a303df4` |
| `dist/c1.wasm.map` | 422,318 | `138343a489bf6d8e4092365e9e3f5e067ef915cc82d4d703e9fe095995ac2bfb` |

### Owned-data browser smoke

The existing `level-complete` browser scenario ran against the complete ignored local stream set:

- 88 streams mounted;
- N. Sanity Beach (`0x09`) reached Level Complete (`0x2d`);
- 69 telemetry and rendered-frame samples observed;
- ten sustained rendered Level Complete samples observed;
- no harness failure, abort, global browser error, texture failure, missing texture page, WebGL
  error, or primitive overflow; and
- final harness status `pass`.

The test uses the harness's existing debug completion event after proving stable gameplay. It is a
bounded transition/rendering smoke, not a normal-input level completion or complete-game proof.
The owned streams and the generated `dist/` directory were not committed.

### Repository boundary

Before the preparation commit, the complete reachable repository contained one branch tip, no
tags, seven commits, and 220 unique blobs. No secret signature or prohibited game-data path was
found. The sole binary was the pinned imported spreadsheet documented in
[LEGAL_REVIEW_BRIEF.md](LEGAL_REVIEW_BRIEF.md).
