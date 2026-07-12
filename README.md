# C1 browser runtime

This workspace compiles the most complete public C1 engine lineage to WebAssembly, loads legally owned Crash Bandicoot disc data locally, and runs it in a browser with keyboard, gamepad, touch, fullscreen, SFX, and software-synthesized music support.

It is a real source port, not a bundled ROM or emulator. No copyrighted game data is included or uploaded.

## Run it

Requirements: macOS or Linux, Git, Node.js 20+, and roughly 2 GB free for the Emscripten SDK and build cache.

```bash
npm run setup
npm run build
npm run dev
```

Open [http://127.0.0.1:4173](http://127.0.0.1:4173). You can select either:

- a raw NTSC-U `.bin` disc image; the browser extracts the `S0`–`S3` streams locally, or
- the extracted `.NSD`/`.NSF` files from the disc’s `S0`–`S3` directories.

Select the title screen or any complete playable pair and press **Launch native build**. The retail disc contains 44 recognized stream pairs: 43 playable boot targets and the `0x04` Cave pair, which is an index-only asset archive used by other levels. Cave stays imported and mounted but is not offered as a standalone boot target. Partial sets can boot individual levels, but later transitions may stop when data is missing.

If you prefer extracting once instead of selecting the BIN on each page load, run:

```bash
npm run extract -- "/path/to/Crash Bandicoot (USA).bin" ./local-data/streams
```

Then choose the generated folder in the loader. `local-data/` and all recognized disc/stream formats are ignored by Git.

## Controls

| PlayStation input | Keyboard | Standard gamepad |
|---|---:|---:|
| D-pad | Arrow keys | D-pad |
| Cross / jump | `Z` | A / Cross |
| Square / spin | `X` | X / Square |
| Circle | `C` | B / Circle |
| Triangle | `V` | Y / Triangle |
| L1 / R1 | `A` / `S` | LB / RB |
| L2 / R2 | `Q` / `W` | LT / RT |
| Start / Select | `Enter` / `Space` | Start / Back |

Touch controls appear automatically on coarse-pointer devices.

## Architecture

- `engine/` — C1 C engine, based on `mateusfavarin/c1` commit `408d6409` and the original `wurlyfox/c1` work.
- `engine/src/pc/` — SDL2 platform layer, custom SFX mixer, TinySoundFont music, and fixed-function renderer.
- `web/` — browser boot UI, local disc/stream importer, touch controls, and wasm host.
- `scripts/` — pinned Emscripten setup, local server, and optional disc extraction tooling.
- `dist/` — generated browser build; intentionally ignored by Git.

The engine runs in wasm32 because C1 stores many pointers and tagged references in 32-bit fields. The browser main loop is cooperative at 30 Hz, SDL targets a canvas/WebGL context, and game files are mounted into Emscripten’s in-memory `/streams` filesystem before `main` starts.

## Current compatibility truth

The browser port is operational, but C1 is still an incomplete reimplementation of the retail executable. Current browser smoke tests cover full-disc import (88 streams / 44 recognized pairs, with 43 playable boot targets), publisher screens, the main menu, password/load/options states, direct level boot, keyboard input, audio, and returning between menus. Deterministic flow checks also cover the title's idle Intro sequence and N. Sanity Beach's transition into the missed-box Level Complete tally. The options screen renders all four entries and its volume, mono/stereo, and exit controls work.

The Cortex laboratory Intro is the first title-screen attract sequence in the retail flow: leave the main menu idle for about 30 seconds to play it. Pressing Start promptly goes directly to the world map.

Browser persistence has two separate records. The retail 15-slot virtual memory card remains under `c1.virtual-memory-card.v1`, preserving its 128-byte payload and card-state handshake. A checksummed automatic resume snapshot lives under `c1.browser-resume.v1`; it captures progression and options about once per second and when the page is hidden or closed, then restores before the title flow starts. Malformed resume data is quarantined instead of overwriting a valid manual card.

Both records use origin-scoped `localStorage`, not cookies. Always open the same URL (`http://127.0.0.1:4173`) because `localhost`, another port, or another protocol has separate browser storage. Clearing this site’s data removes both manual and automatic progress. The supplied disc image and extracted streams are never stored in browser persistence, so they must be selected again after a refresh.

This is not yet a claim of retail parity. A complete playthrough has not certified every level, boss, bonus room, death/checkpoint path, demo, ending, or long sequence of level transitions. Inherited camera, texture-cache, audio-mixing, and level-specific behavior can still differ from the PlayStation release.

“Fully functional” for this project means all of the following, which remain the continuing acceptance criteria:

1. Title, intro, map, password/load, options, completion, and ending flows work without hangs.
2. Every retail level and bonus stage is completable with correct transitions and bosses.
3. Music and SFX survive deaths, pauses, bonuses, and level changes.
4. Keyboard, standard controllers, and touch all cover the complete pad.
5. Save/password state persists in browser storage.
6. A clean end-to-end playthrough passes against original-game reference captures.

[`engine/doc/issues.md`](engine/doc/issues.md) is the historical upstream issue list, not a statement that every item still reproduces. Remaining parity work should be validated against the legally owned disc and original-game behavior, with regressions recorded as reproducible cases.

## Legal boundary

Game assets remain user-supplied and local. The C1 repositories do not provide an express root license, so this repository must remain private unless distribution permission is resolved with the relevant contributors and the original-game rights are reviewed. See `NOTICE.md` and `engine/UPSTREAM.md`.
