# C1 Browser Runtime

C1 Browser Runtime builds the public C1 engine lineage as WebAssembly. It lets you use
your own *Crash Bandicoot* disc data in a browser. It supports keyboards, gamepads,
touch controls, full screen, sound effects, and software-made music.

This project is a source port. It is not a ROM bundle or a PlayStation emulator. The
repository has no proprietary game data. The browser does not upload the files that
you select.

> **Read this source and rights notice:** This repository is source-available research
> software. It is not open source. Much of `engine/` comes from C1 repositories that
> have no stated root license. Before you use or share this code, read
> [LICENSE.md](LICENSE.md), [RIGHTS_AND_LICENSES.md](RIGHTS_AND_LICENSES.md), and
> [NOTICE.md](NOTICE.md).

## Run the project

You need macOS or Linux, Git, Node.js 20 or later, and about 2 GB of free space. The
space is for the Emscripten SDK and the build cache.

```bash
npm run setup
npm run build
npm run dev
```

Open [http://127.0.0.1:4173](http://127.0.0.1:4173). Then select one of these inputs:

- a raw NTSC-U `.bin` disc image; the browser extracts the `S0` to `S3` streams in
  memory; or
- the `.NSD` and `.NSF` files that you extracted from the disc's `S0` to `S3`
  directories.

Select the title screen or a complete playable pair. Then select **Launch native
build**.

The retail disc has 44 known stream pairs. Of these, 43 are playable boot targets. The
`0x04` Cave pair is an index-only asset archive that other levels use. The runtime
imports and mounts Cave, but does not list it as a separate boot target. A partial set
can start one level. A later transition can stop if its data is missing.

You can extract the streams once and reuse them:

```bash
npm run extract -- "/path/to/Crash Bandicoot (USA).bin" ./local-data/streams
```

Then select the new folder in the loader. Git ignores `local-data/` and all known disc
and stream file types.

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

Touch controls appear on devices with a coarse pointer.

## Project layout

- `engine/` contains the C1 C engine. It is based on `mateusfavarin/c1` commit
  `408d6409` and earlier work in `wurlyfox/c1`.
- `engine/src/pc/` contains the SDL2 platform code, the sound-effects mixer,
  TinySoundFont music, and the fixed-function renderer.
- `web/` contains the browser start page, local file importer, touch controls, and
  Wasm host.
- `scripts/` contains the fixed Emscripten setup, local server, and optional disc
  extraction tools.
- `dist/` contains the generated browser build. Git ignores this directory.

The engine uses wasm32 because C1 stores many pointers and tagged references in 32-bit
fields. The browser runs a cooperative main loop at 30 Hz. SDL draws to a canvas with
WebGL. Before the engine starts, the browser mounts game files in Emscripten's in-memory
`/streams` file system.

## What works now

The browser port works, but C1 is still an incomplete version of the retail program.
Current browser smoke tests cover these areas:

- full-disc import: 88 streams, 44 known pairs, and 43 playable boot targets;
- publisher screens, the main menu, password, load, and options screens;
- direct level start, keyboard input, audio, and menu returns;
- the title screen's idle Intro sequence; and
- N. Sanity Beach changing to the missed-box Level Complete screen.

The options screen shows all four items. Its volume, mono/stereo, and exit controls
work.

The Cortex laboratory Intro is the first idle sequence from the title screen. Leave the
main menu idle for about 30 seconds to see it. Press Start soon after the menu appears
to go directly to the world map.

The browser keeps two separate progress records:

- `c1.virtual-memory-card.v1` is the retail 15-slot virtual memory card. It keeps each
  128-byte payload and the card-state handshake.
- `c1.browser-resume.v1` is a checksummed automatic resume record. It saves progression
  and options about once per second. It also saves when the page becomes hidden or
  closes. The runtime restores it before the title flow starts.

The runtime quarantines a malformed resume record. It does not use that record to
replace a valid manual card.

Both records use origin-specific `localStorage`, not cookies. Always use the same URL.
For example, `http://127.0.0.1:4173`, `localhost`, another port, and another protocol
each have separate storage. If you clear site data, you delete both records. The
browser never puts the disc image or extracted streams in persistent storage. You must
select them again after a page refresh.

This work does not yet prove retail parity. A complete test has not verified every
level, boss, bonus room, death and checkpoint route, demo, ending, or long transition
chain. Camera, texture-cache, audio-mixing, and level-specific behavior can still
differ from the PlayStation game.

For this project, **fully functional** means that all of these checks pass:

1. Title, Intro, map, password, load, options, completion, and ending flows do not hang.
2. Every retail level and bonus stage can finish with the correct transitions and bosses.
3. Music and sound effects continue through deaths, pauses, bonuses, and level changes.
4. Keyboard, standard gamepad, and touch controls provide the complete controller.
5. Save and password state remains in browser storage.
6. A clean end-to-end run matches captures from an original copy of the game.

[`engine/doc/issues.md`](engine/doc/issues.md) is the old upstream issue list. It does
not mean that every issue still occurs. Test remaining parity work against a legally
owned disc and original-game behavior. Record regressions as repeatable cases.

## Legal boundary

Users must supply their own game assets. The repository does not include a game disc,
BIOS, executable, extracted stream, retail image, audio file, screenshot, recording,
save, or replay.

The C1 source repositories have no stated root license. Public access would not
relicense their work or make this project open source. The current plan covers source
and documentation only. It does not approve a compiled release or a hosted playable
site.

Read these documents before you use or share the project:

- [License notice](LICENSE.md)
- [Rights and licenses](RIGHTS_AND_LICENSES.md)
- [Copyright and source notice](NOTICE.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)
- [Privacy](PRIVACY.md)
- [Security reports](SECURITY.md)
- [Contributing](CONTRIBUTING.md)
- [Documentation guide](docs/README.md)
