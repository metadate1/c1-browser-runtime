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

Select the title screen or any complete level pair and press **Launch native build**. The retail disc contains 44 recognized level pairs. Partial sets can boot individual levels, but later transitions may stop when data is missing.

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

The browser platform port can compile and run the inherited native engine, but C1 itself is not yet a perfect reimplementation. The best public fork still documents broken demo playback, non-map title states, a few level-specific crashes, camera/texture differences, and no implemented PC memory-card backend. Those are engine-completion tasks rather than WebAssembly blockers.

“Fully functional” for this project means all of the following, which remain the continuing acceptance criteria:

1. Title, intro, map, password/load, options, completion, and ending flows work without hangs.
2. Every retail level and bonus stage is completable with correct transitions and bosses.
3. Music and SFX survive deaths, pauses, bonuses, and level changes.
4. Keyboard, standard controllers, and touch all cover the complete pad.
5. Save/password state persists in browser storage.
6. A clean end-to-end playthrough passes against original-game reference captures.

See `engine/doc/issues.md` for inherited known defects. The practical next phase is to fix those one by one using the legally owned disc image and original game behavior as the oracle.

## Legal boundary

Game assets remain user-supplied and local. The C1 repositories do not provide an express root license, so public distribution of this derivative needs permission from the relevant contributors as well as appropriate review for original-game rights. See `NOTICE.md` and `engine/UPSTREAM.md`.
