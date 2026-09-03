# Legal-review brief for C1 Browser Runtime source publication

This document gives engineering facts to a qualified lawyer. It does not give legal
advice.

## Planned action

The owner wants to make `github.com/metadate1/c1-browser-runtime` public as
source-available research code.

The planned source release would:

- keep the owner's rights reserved;
- publish only source and documents;
- exclude builds, hosted runtime files, game data, BIOS files, screenshots, videos,
  saves, and replays; and
- require users to select their own supported game data locally.

The current application has no game-file upload endpoint.

## Repository audit

The 2026-09-03 audit found one branch, no tags, nine commits, and 257 unique blobs. It
found no reachable disc image, ROM, BIOS, executable, extracted stream, save, replay,
recording, credential, or unexpected large blob.

The only binary blob is `engine/doc/memmap.xlsx`. This 42,813-byte spreadsheet came
with the C1 engine. It has six XML worksheets. Inspection found no macro, image, or
external link. Its SHA-256 is
`2f67862f880f1da2594b1fc90e9f812d4bb6e58b5a9416137a1252d070542aff`.

All nine commits use the GitHub author identity of Dominykas Norvilas. The repository
imported the upstream C1 work as a snapshot. As a result, this Git history does not show
the original source commits or authors.

## Source history for review

Most of `engine/` comes directly from:

- `wurlyfox/c1` at commit `256fdcef59f15a190290cc19db3fa9a707843b69`; and
- the `windows` branch of `mateusfavarin/c1` at commit
  `408d6409afadc1202230ac1183d4d7f40292b87c`.

At these revisions and during the 2026-09-03 audit, neither repository had a stated root
license. GitHub lists Wurly Fox, Mateus Favarin, and ManDude as contributors. Their
source remains subject to normal copyright even though it is public on GitHub.

The initial commit imported the engine as one snapshot. It did not preserve upstream
Git history. A blob comparison with the fixed Mateus revision found:

- 94 byte-identical engine files;
- no differences at paths that both trees contain;
- 10 additions specific to this repository or browser work; and
- 157 omitted upstream paths.

Most omitted paths contain desktop dependencies and Visual Studio files.

Later commits combine browser work with changes to engine rendering, audio, collision,
saves, and game behavior. There is no ownership record for each file.

This is a source port, not a clean-room rewrite. It contains declarations, addresses,
constants, state changes, and behavior from the original game and the C1 lineage.

## Existing controls

- Git and CI reject game data, captures, credentials, and generated builds.
- The public-release audit scans reachable history. It limits blob size, rejects unknown
  binary files, checks the imported spreadsheet hash, and looks for common credential
  formats.
- npm metadata sets `private: true` and `license: UNLICENSED`.
- Code contributions remain closed while the license issue is unresolved.
- The browser has no telemetry, analytics, or game-file upload endpoint.
- TinySoundFont and TinyMidiLoader keep their MIT and zlib notices.

These controls reduce accidental disclosure. They do not create copyright, trademark,
or distribution rights.

## Questions for counsel

1. Can the owner make this derived C1 snapshot and its later changes public without
   written permission from the source authors?
2. Which contributors or rights holders must give permission, and under which terms?
3. Does any source, structure, constant, document, or behavior require permission from
   the original game's rights holders?
4. Is the current use of game, console, developer, and publisher names acceptable with
   the no-affiliation notice?
5. What must change before the owner distributes a compiled Wasm bundle or runs a
   hosted service?
6. What contribution process is needed before the project accepts outside code?

## Requested decision

Useful decisions are:

1. approve public source access within the stated limits;
2. approve access after named files or claims change; or
3. keep the repository private until named permission or replacement work is complete.

Add an open-source license only if the project has authority to license every covered
file.
