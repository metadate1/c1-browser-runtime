# Third-party notices

This list does not change the license of any file. The notice next to each dependency
is the controlling notice.

## Included source

`engine/src/third_party/tinysoundfont/` contains files from
[`schellingb/TinySoundFont`](https://github.com/schellingb/TinySoundFont) commit
`fbc913531b85f5707f49115110bb86b1cd583885`:

| File | License |
| --- | --- |
| `tsf.h` | MIT |
| `tml.h` | zlib |
| `LICENSE` | Upstream MIT license text |

[`engine/src/third_party/tinysoundfont/UPSTREAM.md`](engine/src/third_party/tinysoundfont/UPSTREAM.md)
records the exact file hashes and import details.

## Engine source without a stated license

The imported C1 engine is not a permissively licensed dependency. Its two source
repositories have no stated root license. Read [engine/UPSTREAM.md](engine/UPSTREAM.md)
and [RIGHTS_AND_LICENSES.md](RIGHTS_AND_LICENSES.md).

## Build dependencies

The build downloads Emscripten and uses its SDL integration. The setup script fixes the
`emsdk` bootstrap repository at commit
`ca38f487f28b7c3c16f8f70cd0e012099ac4b7e2`, which is tag `6.0.2`. It installs
Emscripten `6.0.2`. These tools and their generated files are not committed here.

Before you distribute a compiled Wasm bundle, collect and include every license and
notice that the exact Emscripten, SDL, and linked-library versions require. The current
source-publication plan does not include generated builds.

`package.json` has no npm runtime or development dependencies.
