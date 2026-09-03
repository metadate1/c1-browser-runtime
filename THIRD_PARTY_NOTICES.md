# Third-party notices

This inventory does not relicense any file. The notices beside each dependency remain
authoritative.

## Vendored source

`engine/src/third_party/tinysoundfont/` contains files pinned from
[`schellingb/TinySoundFont`](https://github.com/schellingb/TinySoundFont) commit
`fbc913531b85f5707f49115110bb86b1cd583885`:

| File | License |
| --- | --- |
| `tsf.h` | MIT |
| `tml.h` | zlib |
| `LICENSE` | Upstream MIT license text |

Exact hashes and import details are recorded in
[`engine/src/third_party/tinysoundfont/UPSTREAM.md`](engine/src/third_party/tinysoundfont/UPSTREAM.md).

## Engine source with no express license

The imported C1 engine is not listed as a permissively licensed dependency. Its two upstream
repositories have no express root license. See [engine/UPSTREAM.md](engine/UPSTREAM.md) and
[RIGHTS_AND_LICENSES.md](RIGHTS_AND_LICENSES.md).

## Build-time dependencies

The source build downloads Emscripten and uses Emscripten's SDL integration. The setup script pins
the `emsdk` bootstrap repository to commit
`ca38f487f28b7c3c16f8f70cd0e012099ac4b7e2` (tag `6.0.2`) and installs Emscripten `6.0.2`.
Those tools and their generated components are not committed to this repository.

Before distributing a compiled Wasm bundle, collect and ship every license text and notice
required by the exact Emscripten, SDL, and linked-library versions. The current public-source plan
does not include generated builds.

There are no npm runtime or development dependencies in `package.json`.
