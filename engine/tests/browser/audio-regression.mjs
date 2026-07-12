const INITIAL_SCHEDULE_DEADLINE_MS = 800;
const DELAY_RETENTION_CHECK_MS = 1200;
const CALLBACK_STALL_LIMIT_MS = 750;
const CALLBACK_GAP_LIMIT_US = 250000;
const MIN_INITIAL_DELAYED_VOICES = 4;
const MIN_RETAINED_DELAYED_VOICES = 3;

export function createIntroAudioRegressionState(sample) {
  const delayedVoiceApi = Number.isInteger(sample.delayedVoices);
  const state = {
    delayedVoiceApi,
    delayedVoiceRoute: [],
    maxDelayedVoices: 0,
    maxEarlyActiveSfx: 0,
    scheduleVerified: false,
    retentionVerified: false,
    completedSampleRekeyApi: Number.isInteger(sample.completedSampleRekeys),
    completedSampleRekeyBaseline: sample.completedSampleRekeys,
    maxCompletedSampleRekeys: sample.completedSampleRekeys,
    callbacksObserved: sample.audioCallbacks > 0,
    callbackResets: 0,
    lastCallbackCount: sample.audioCallbacks,
    lastCallbackProgressAtMs: sample.atMs,
    maxCallbackStallMs: 0,
    callbackGapBaselineUs: sample.audioMaxGapUs,
    maxReportedCallbackGapUs: sample.audioMaxGapUs,
    alwaysVisible: sample.visible,
  };
  recordDelayedVoiceSample(state, sample);
  return state;
}

function recordDelayedVoiceSample(state, sample) {
  if (!Number.isInteger(sample.delayedVoices)) return;
  state.maxDelayedVoices = Math.max(state.maxDelayedVoices, sample.delayedVoices);
  const last = state.delayedVoiceRoute.at(-1);
  if (!last || last.count !== sample.delayedVoices) {
    state.delayedVoiceRoute.push({
      atMs: sample.atMs,
      count: sample.delayedVoices,
    });
  }
}

export function sampleIntroAudioRegression(state, sample) {
  state.alwaysVisible &&= sample.visible;
  state.maxReportedCallbackGapUs = Math.max(
    state.maxReportedCallbackGapUs,
    sample.audioMaxGapUs,
  );
  if (sample.atMs <= DELAY_RETENTION_CHECK_MS)
    state.maxEarlyActiveSfx = Math.max(state.maxEarlyActiveSfx, sample.activeSfx);
  if (Number.isInteger(sample.completedSampleRekeys)) {
    state.maxCompletedSampleRekeys = Math.max(
      state.maxCompletedSampleRekeys,
      sample.completedSampleRekeys,
    );
    if (sample.completedSampleRekeys > state.completedSampleRekeyBaseline)
      return `Intro completed-sample rekeys reached ${sample.completedSampleRekeys}`;
  }
  recordDelayedVoiceSample(state, sample);

  if (sample.audioCallbacks !== state.lastCallbackCount) {
    if (sample.audioCallbacks < state.lastCallbackCount) state.callbackResets++;
    state.callbacksObserved ||= sample.audioCallbacks > 0;
    state.lastCallbackCount = sample.audioCallbacks;
    state.lastCallbackProgressAtMs = sample.atMs;
  } else if (!sample.visible) {
    // Browser throttling while hidden is not an engine audio regression. Re-arm
    // the cadence check once callbacks advance after the page becomes visible.
    state.lastCallbackProgressAtMs = sample.atMs;
  } else if (state.callbacksObserved) {
    const stallMs = sample.atMs - state.lastCallbackProgressAtMs;
    state.maxCallbackStallMs = Math.max(state.maxCallbackStallMs, stallMs);
    if (stallMs > CALLBACK_STALL_LIMIT_MS)
      return `audio callback count did not advance for ${stallMs} ms`;
  }

  if (state.alwaysVisible
   && sample.audioMaxGapUs > state.callbackGapBaselineUs
   && sample.audioMaxGapUs > CALLBACK_GAP_LIMIT_US) {
    return `audio callback gap reached ${sample.audioMaxGapUs} us`;
  }

  if (!state.scheduleVerified && sample.atMs >= INITIAL_SCHEDULE_DEADLINE_MS) {
    if (!state.delayedVoiceApi)
      return "missing C1GetAudioDelayedVoiceCount export";
    if (state.maxDelayedVoices < MIN_INITIAL_DELAYED_VOICES) {
      return `intro scheduled only ${state.maxDelayedVoices} delayed dialogue voices; expected at least ${MIN_INITIAL_DELAYED_VOICES}`;
    }
    state.scheduleVerified = true;
  }

  if (!state.retentionVerified && sample.atMs >= DELAY_RETENTION_CHECK_MS) {
    if (sample.delayedVoices < MIN_RETAINED_DELAYED_VOICES) {
      return `only ${sample.delayedVoices} intro dialogue voices remained delayed after ${sample.atMs} ms; expected at least ${MIN_RETAINED_DELAYED_VOICES}`;
    }
    state.retentionVerified = true;
  }

  return null;
}

export function finishIntroAudioRegression(state) {
  if (!state) return "intro audio evidence was not recorded";
  if (!state.delayedVoiceApi) return "missing C1GetAudioDelayedVoiceCount export";
  if (!state.scheduleVerified) return "intro delayed-dialogue schedule was not verified";
  if (!state.retentionVerified) return "intro delayed-dialogue retention was not verified";
  if (!state.completedSampleRekeyApi)
    return "missing C1GetAudioCompletedSampleRekeyCount export";
  if (!state.callbacksObserved) return "browser audio callbacks never started during Intro";
  return null;
}
