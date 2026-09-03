# Browser flow checks

Build the WebAssembly runtime. Then start the test harness with the complete retail
stream set:

```sh
make web
PORT=4174 node engine/tests/browser/server.mjs local-data/streams
```

Each test starts by itself. It removes only the C1 card and resume keys from the test
origin. `HARNESS RUNNING` stays visible until the test proves the complete route.

- `http://127.0.0.1:4174/?scenario=title-attract-intro` waits at the main menu. It
  requires the idle flow to enter Intro (`0x38`). It checks that retail delayed speech
  stays staggered and that a completed speech sample does not get a new key. It rejects
  long foreground audio-callback stalls. It also requires continuous rendered frames,
  camera movement, and a return to Title (`0x19`).
- `http://127.0.0.1:4174/?scenario=level-complete` starts N. Sanity Beach. It skips the
  entrance camera with retail Cross input, waits for stable game play, and sends the
  retail Warp event. It requires the route `0x09 -> 0x2D` and continuous rendered Level
  Complete frames.

Both tests reject texture failures, missing texture pages, GL errors, and primitive
arena overflows. Read the machine result from `window.__c1HarnessSnapshot()` or from the
JSON in `#metrics`.

The Intro audio checks need these browser exports:

- `C1GetAudioDelayedVoiceCount`
- `C1GetAudioCompletedSampleRekeyCount`

You can run their pure state-machine checks without a browser:

```sh
node engine/tests/browser/audio-regression_test.mjs
```
