import assert from "node:assert/strict";
import {
  createIntroAudioRegressionState,
  finishIntroAudioRegression,
  sampleIntroAudioRegression,
} from "./audio-regression.mjs";

function sample(overrides = {}) {
  return {
    atMs: 0,
    delayedVoices: 5,
    audioCallbacks: 1,
    audioMaxGapUs: 0,
    activeSfx: 0,
    visible: true,
    ...overrides,
  };
}

{
  const state = createIntroAudioRegressionState(sample());
  assert.equal(sampleIntroAudioRegression(state, sample({
    atMs: 500,
    delayedVoices: 4,
    audioCallbacks: 20,
    audioMaxGapUs: 43000,
    activeSfx: 1,
  })), null);
  assert.equal(sampleIntroAudioRegression(state, sample({
    atMs: 1200,
    delayedVoices: 4,
    audioCallbacks: 50,
    audioMaxGapUs: 43000,
    activeSfx: 1,
  })), null);
  assert.equal(finishIntroAudioRegression(state), null);
  assert.deepEqual(state.delayedVoiceRoute, [
    { atMs: 0, count: 5 },
    { atMs: 500, count: 4 },
  ]);
}

{
  const state = createIntroAudioRegressionState(sample({
    delayedVoices: 0,
    activeSfx: 4,
  }));
  const failure = sampleIntroAudioRegression(state, sample({
    atMs: 800,
    delayedVoices: 0,
    audioCallbacks: 35,
    audioMaxGapUs: 43000,
    activeSfx: 4,
  }));
  assert.match(failure, /scheduled only 0 delayed dialogue voices/);
}

{
  const state = createIntroAudioRegressionState(sample());
  assert.equal(sampleIntroAudioRegression(state, sample({
    atMs: 1200,
    delayedVoices: 4,
    audioCallbacks: 40,
    audioMaxGapUs: 43000,
  })), null);
  const failure = sampleIntroAudioRegression(state, sample({
    atMs: 2000,
    delayedVoices: 4,
    audioCallbacks: 40,
    audioMaxGapUs: 43000,
  }));
  assert.match(failure, /did not advance for 800 ms/);
}

{
  const state = createIntroAudioRegressionState(sample());
  const failure = sampleIntroAudioRegression(state, sample({
    atMs: 1200,
    delayedVoices: 4,
    audioCallbacks: 40,
    audioMaxGapUs: 300000,
  }));
  assert.match(failure, /callback gap reached 300000 us/);
}

{
  const state = createIntroAudioRegressionState(sample({ delayedVoices: null }));
  const failure = sampleIntroAudioRegression(state, sample({
    atMs: 800,
    delayedVoices: null,
    audioCallbacks: 35,
  }));
  assert.equal(failure, "missing C1GetAudioDelayedVoiceCount export");
}

console.log("browser intro audio regression tests passed");
