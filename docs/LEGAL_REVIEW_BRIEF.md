# Legal-review brief for C1 Browser Runtime source publication

This is a factual engineering summary for qualified counsel. It does not give a legal opinion.

## Proposed action

The owner wants to make `github.com/metadate1/c1-browser-runtime` publicly readable as
source-available research code.

The proposed source release would:

- keep project-owned rights reserved;
- publish source and documentation only;
- exclude compiled releases, hosted runtime files, game data, BIOS files, screenshots, videos,
  saves, and replays; and
- require users to select their own supported game data locally.

The current application has no game-file upload endpoint.

## Repository audit

The 2026-09-03 engineering audit found one branch, no tags, seven commits, and 220 unique blobs.
It found no reachable disc image, ROM, BIOS, executable, extracted stream, save, replay,
recording, credential, or large unexpected blob.

The sole binary blob is `engine/doc/memmap.xlsx`, a 42,813-byte spreadsheet imported with the C1
engine. Inspection found six XML worksheets and no macro, image, or external-link payload. Its
SHA-256 is `2f67862f880f1da2594b1fc90e9f812d4bb6e58b5a9416137a1252d070542aff`.

All seven repository commits use Dominykas Norvilas's GitHub author identity. The upstream C1
history was imported as a snapshot, so its original commits and authors are not represented in
this repository's Git history.

## Source history that needs review

Most of `engine/` is directly derived from:

- `wurlyfox/c1` at `256fdcef59f15a190290cc19db3fa9a707843b69`; and
- the `windows` branch of `mateusfavarin/c1` at
  `408d6409afadc1202230ac1183d4d7f40292b87c`.

At the recorded revisions and the 2026-09-03 audit, neither repository had an express root
license. GitHub's contributor records name Wurly Fox, Mateus Favarin, and ManDude. Default
copyright therefore remains relevant even though the source is publicly visible upstream.

The initial commit imported the engine as one snapshot rather than preserving upstream Git
history. A blob-level comparison with the pinned Mateus revision found 94 byte-identical engine
files, no differing files at shared paths, 10 browser-specific or repository-specific additions,
and 157 omitted upstream paths. The omitted paths are mainly bundled desktop dependencies and
Visual Studio files.

Later commits mix browser-platform work with changes to engine rendering, audio, collision, saves,
and gameplay behavior. There is no file-by-file ownership ledger.

The project is a source port, not a clean-room rewrite. It contains format declarations,
addresses, constants, state transitions, and behavior derived from the original game and the C1
lineage.

## Existing safeguards

- Game data, captures, credentials, and generated builds are ignored and rejected by CI.
- The public-release audit scans reachable history, limits blob size, rejects unapproved binary
  files, pins the imported spreadsheet hash, and scans text for common credential forms.
- npm metadata keeps `private: true` and uses `UNLICENSED`.
- Code contributions remain closed while licensing is unresolved.
- The browser has no telemetry, analytics, or game-file upload endpoint.
- TinySoundFont and TinyMidiLoader retain their upstream MIT and zlib notices.

These safeguards reduce accidental disclosure. They do not create copyright, trademark, or
distribution permission.

## Questions to answer

1. Can this directly derived C1 snapshot and its later modifications be made publicly readable
   without written permission from the upstream authors?
2. Which upstream contributors or rights holders would need to grant permission, and under what
   terms?
3. Does any source, data structure, constant, documentation, or behavior require permission from
   the original game's rights holders?
4. Is the current nominative use of game, console, developer, and publisher names acceptable with
   the no-affiliation disclaimer?
5. What changes would be required before distributing a compiled Wasm bundle or operating a hosted
   runtime?
6. What contributor process would be needed before accepting outside code?

## Requested decision

Useful outcomes are:

1. approve public source-available visibility under the disclosed boundary;
2. approve visibility after named files or claims are changed; or
3. keep the repository private until named permission or replacement work is complete.

An open-source license should be added only if the project has authority to grant it over every
covered file.
