# Verification record

This is a dated engineering log. It is not legal permission or proof of complete retail
parity. All named game data and generated files stayed local and ignored by Git.

## 2026-09-03 public-source preparation

The checks ran on Ubuntu 24.04 with:

- Node.js `22.16.0`;
- GCC `13.3.0`;
- `emsdk` commit `ca38f487f28b7c3c16f8f70cd0e012099ac4b7e2`;
- Emscripten `6.0.2` at commit
  `7a2d97d627ff4945eae28847ce0387ac52b92c09`; and
- Google Chrome `151.0.7922.137` under Xvfb with SwiftShader.

These checks passed:

```bash
bash -n scripts/check-public-release.sh scripts/setup-emsdk.sh
node --check <each tracked .js and .mjs file>
bash scripts/check-public-release.sh
npm test
npm run setup
npm run build
git diff --check
```

The native test command built and ran every C and JavaScript test target in `make test`.
The C compiler reported warnings inherited from the engine. The Makefile does not treat
these warnings as errors. All test programs and sanitizer checks passed.

The Wasm build made these ignored files:

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `dist/c1.mjs` | 386,704 | `12f47f6aa1b4b506f1fd121dff1174b74c12906433043e07f9bee8dc2f8c52e4` |
| `dist/c1.wasm` | 703,871 | `958616b3b55eafb66547371746091d4417811d2896ee90d4160a03c17a303df4` |
| `dist/c1.wasm.map` | 422,318 | `138343a489bf6d8e4092365e9e3f5e067ef915cc82d4d703e9fe095995ac2bfb` |

### Local-data browser smoke test

The existing `level-complete` browser test used the complete local stream set. It
reported:

- 88 mounted streams;
- N. Sanity Beach (`0x09`) changed to Level Complete (`0x2d`);
- 69 telemetry and rendered-frame samples;
- 10 continuous rendered Level Complete samples; and
- final harness status `pass`.

The harness reported no failure, abort, global browser error, texture failure, missing
texture page, WebGL error, or primitive overflow.

The test uses the harness's debug completion event after it proves stable game play. It
is a short transition and rendering test. It is not a normal-input level completion or
a complete-game test. The local streams and generated `dist/` directory were not
committed.

### Repository boundary

The current reachable repository has one branch tip, no tags, nine commits, and 257
unique blobs. The audit found no secret signature or prohibited game-data path. The only
binary is the imported spreadsheet listed in
[LEGAL_REVIEW_BRIEF.md](LEGAL_REVIEW_BRIEF.md).
