# Browser flow checks

Build the WebAssembly runtime and start the harness with the complete retail
stream set:

```sh
make web
PORT=4174 node engine/tests/browser/server.mjs local-data/streams
```

The flow scenarios auto-start, remove only the C1 card/resume keys on the
harness origin, and keep `HARNESS RUNNING` until the complete route is proven.

- `http://127.0.0.1:4174/?scenario=title-attract-intro`
  waits at the main menu without input, requires attract mode to enter Intro
  (`0x38`), requires the retail delayed-dialogue schedule to remain staggered
  without any completed dialogue sample being re-keyed,
  rejects severe foreground audio-callback stalls, requires sustained rendered
  frames and camera motion, and requires Intro to return to Title (`0x19`).
- `http://127.0.0.1:4174/?scenario=level-complete`
  boots N. Sanity Beach, skips its entrance camera with retail Cross input,
  waits for stable gameplay, sends the retail Warp event, requires the route
  `0x09 -> 0x2D`, and requires sustained rendered Level Complete frames.

Both scenarios also reject texture failures, missing texture pages, GL errors,
and primitive-arena overflows. Read the machine-friendly result with
`window.__c1HarnessSnapshot()` or the JSON in `#metrics`.

The Intro audio assertions require the browser exports
`C1GetAudioDelayedVoiceCount` and `C1GetAudioCompletedSampleRekeyCount`. Their
pure state-machine regression checks can be run without a browser:

```sh
node engine/tests/browser/audio-regression_test.mjs
```
