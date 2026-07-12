import {
  advanceRuntimeFrameGate,
  callMainOnce,
  isEmscriptenUnwind,
} from "/dist/runtime-lifecycle.js";
import {
  createIntroAudioRegressionState,
  finishIntroAudioRegression,
  sampleIntroAudioRegression,
} from "/audio-regression.mjs";

const q = new URLSearchParams(location.search);
const scenarioName = q.get("scenario") || "";
const scenarioLids = {
  "title-attract-intro": 0x19,
  "level-complete": 0x09,
};
const lid = Number(q.get("lid") || scenarioLids[scenarioName] || 9);
const canvas = document.querySelector("#canvas");
const result = document.querySelector("#result");
const metrics = document.querySelector("#metrics");
const log = document.querySelector("#log");
const startButton = document.querySelector("#start");
const buttons = [...document.querySelectorAll("button:not(#start)")];
const lines = [];
const lifecycle = {
  status: "idle",
  phase: "idle",
  failurePhase: null,
  failure: null,
  mainStatus: null,
  mainUnwound: false,
  abortReason: null,
  globalErrors: [],
  telemetrySamples: 0,
  frameSamples: 0,
  firstFrameUs: null,
  lid,
  streamCount: 0,
  scenario: scenarioName || null,
  scenarioPhase: scenarioName ? "boot" : null,
  scenarioStorageReset: false,
  scenarioRoute: [],
  scenarioEvidence: {},
};
let module;
let runtimeFrameGate = "idle";
let lastTelemetry = {};
let frameFrozen = false;
let maxFrameMisses = 0;
let maxFrameFailures = 0;
let maxFrameMissingPages = 0;
let maxFrameGenerationMisses = 0;
let maxFrameCacheFailures = 0;
let maxFrameUploadBytes = 0;
let maxTextureTotalFailures = 0;
let maxGlErrors = 0;
let maxPrimitiveOverflows = 0;
let worstFailureFrame = null;
let scenarioStartedAt = 0;
let scenarioLastRouteKey = "";
let scenarioPadHeld = false;
const scenarioSkippedTitleStates = new Set();
const scenarioIntroCameras = new Set();

window.__c1HarnessResult = lifecycle;
window.__c1HarnessSnapshot = () => ({
  ...lifecycle,
  globalErrors: [...lifecycle.globalErrors],
  scenarioRoute: lifecycle.scenarioRoute.map((step) => ({ ...step })),
  scenarioEvidence: { ...lifecycle.scenarioEvidence },
});

function oneLine(value) {
  return String(value ?? "Unknown failure").replace(/\s+/g, " ").trim();
}

function appendLine(line) {
  lines.push(String(line));
  log.textContent = lines.slice(-20).join("\n");
}

function renderMetrics() {
  metrics.textContent = JSON.stringify({
    harnessStatus: lifecycle.status,
    harnessPass: lifecycle.status === "pass",
    harnessPhase: lifecycle.phase,
    harnessFailurePhase: lifecycle.failurePhase,
    harnessFailure: lifecycle.failure,
    harnessMainStatus: lifecycle.mainStatus,
    harnessMainUnwound: lifecycle.mainUnwound,
    harnessAbortReason: lifecycle.abortReason,
    harnessGlobalErrors: [...lifecycle.globalErrors],
    harnessTelemetrySamples: lifecycle.telemetrySamples,
    harnessFrameSamples: lifecycle.frameSamples,
    harnessFirstFrameUs: lifecycle.firstFrameUs,
    harnessScenario: lifecycle.scenario,
    harnessScenarioPhase: lifecycle.scenarioPhase,
    harnessScenarioStorageReset: lifecycle.scenarioStorageReset,
    harnessScenarioRoute: lifecycle.scenarioRoute,
    harnessScenarioEvidence: lifecycle.scenarioEvidence,
    ...lastTelemetry,
  });
}

function renderResult() {
  result.dataset.status = lifecycle.status;
  const scenarioLabel = lifecycle.scenario ? ` scenario=${lifecycle.scenario}` : "";
  if (lifecycle.status === "pass") {
    result.textContent = `HARNESS PASS${scenarioLabel} lid=${lid} streams=${lifecycle.streamCount} mainStatus=${lifecycle.mainStatus}`;
  } else if (lifecycle.status === "fail") {
    result.textContent = `HARNESS FAIL phase=${lifecycle.failurePhase} reason=${lifecycle.failure}`;
  } else if (lifecycle.status === "running") {
    result.textContent = `HARNESS RUNNING phase=${lifecycle.phase}`;
  } else {
    result.textContent = "HARNESS IDLE";
  }
  renderMetrics();
}

function setPhase(phase) {
  if (lifecycle.status === "fail") return;
  lifecycle.status = "running";
  lifecycle.phase = phase;
  renderResult();
}

function markFailure(reason, phase = lifecycle.phase) {
  runtimeFrameGate = advanceRuntimeFrameGate(runtimeFrameGate, "failure");
  if (lifecycle.status !== "fail") {
    lifecycle.failure = oneLine(reason);
    lifecycle.failurePhase = phase;
    lifecycle.phase = phase;
    lifecycle.status = "fail";
    appendLine(`HARNESS FAIL phase=${phase}: ${lifecycle.failure}`);
  }
  startButton.disabled = true;
  buttons.forEach((button) => { button.disabled = true; });
  renderResult();
}

function markPass(phase = "runtime") {
  if (lifecycle.status === "fail" || runtimeFrameGate !== "pass") return;
  lifecycle.status = "pass";
  lifecycle.phase = phase;
  lifecycle.scenarioPhase = lifecycle.scenario ? "complete" : lifecycle.scenarioPhase;
  appendLine(`HARNESS PASS${lifecycle.scenario ? ` scenario=${lifecycle.scenario}` : ""} lid=${lid} streams=${lifecycle.streamCount}`);
  renderResult();
}

function setScenarioPhase(phase) {
  if (!lifecycle.scenario || lifecycle.status === "fail") return;
  lifecycle.scenarioPhase = phase;
  lifecycle.phase = `scenario:${phase}`;
  lifecycle.status = "running";
  renderResult();
}

function pulseScenarioPad(bit) {
  if (scenarioPadHeld || !module) return false;
  scenarioPadHeld = true;
  module._C1SetVirtualPad(bit);
  setTimeout(() => {
    module?._C1SetVirtualPad(0);
    scenarioPadHeld = false;
  }, 120);
  return true;
}

function recordScenarioRoute(telemetry) {
  if (!lifecycle.scenario) return;
  const key = `${telemetry.lid}:${telemetry.loadedTitleState}:${telemetry.titleTransition}`;
  if (key === scenarioLastRouteKey) return;
  scenarioLastRouteKey = key;
  lifecycle.scenarioRoute.push({
    atMs: Date.now() - scenarioStartedAt,
    lid: telemetry.lid,
    titleState: telemetry.titleState,
    loadedTitleState: telemetry.loadedTitleState,
    titleTransition: telemetry.titleTransition,
  });
  if (lifecycle.scenarioRoute.length > 64) lifecycle.scenarioRoute.shift();
}

function runtimeDiagnosticFailure() {
  if (maxFrameFailures) return `texture failures reached ${maxFrameFailures}`;
  if (maxFrameMissingPages) return `missing texture pages reached ${maxFrameMissingPages}`;
  if (maxFrameCacheFailures) return `texture cache failures reached ${maxFrameCacheFailures}`;
  if (maxTextureTotalFailures) return `total texture failures reached ${maxTextureTotalFailures}`;
  if (maxGlErrors) return `GL errors reached ${maxGlErrors}`;
  if (maxPrimitiveOverflows) return `primitive overflows reached ${maxPrimitiveOverflows}`;
  return null;
}

function finishScenarioPass() {
  const diagnostic = runtimeDiagnosticFailure();
  if (diagnostic) {
    markFailure(diagnostic, `scenario:${lifecycle.scenarioPhase}`);
    return;
  }
  markPass("scenario:complete");
}

function runTitleAttractIntroScenario(telemetry) {
  const elapsed = Date.now() - scenarioStartedAt;
  const evidence = lifecycle.scenarioEvidence;
  if (elapsed > 120000) {
    markFailure(`timed out; route=${JSON.stringify(lifecycle.scenarioRoute)}`, "scenario:title-attract-intro");
    return;
  }

  if (telemetry.lid === 0x19
   && [10, 7, 8].includes(telemetry.loadedTitleState)
   && telemetry.titleTransition === 3
   && !scenarioSkippedTitleStates.has(telemetry.loadedTitleState)) {
    scenarioSkippedTitleStates.add(telemetry.loadedTitleState);
    pulseScenarioPad(0x40);
    setScenarioPhase(`skip-title-${telemetry.loadedTitleState}`);
    return;
  }

  if (!evidence.mainMenuReached
   && telemetry.lid === 0x19
   && telemetry.loadedTitleState === 5
   && telemetry.titleTransition === 3) {
    evidence.mainMenuReached = true;
    evidence.mainMenuReachedAtMs = elapsed;
    setScenarioPhase("await-idle-attract-intro");
    return;
  }

  if (!evidence.sawIntro && telemetry.lid === 0x38) {
    evidence.sawIntro = true;
    evidence.introSamples = 0;
    evidence.introTriangleSamples = 0;
    evidence.introStartedAtMs = elapsed;
    evidence.introAudio = createIntroAudioRegressionState({
      atMs: 0,
      delayedVoices: telemetry.delayedVoices,
      audioCallbacks: telemetry.audioCallbacks,
      audioMaxGapUs: telemetry.audioMaxGapUs,
      activeSfx: telemetry.activeSfx,
      completedSampleRekeys: telemetry.completedSampleRekeys,
      visible: document.visibilityState === "visible",
    });
    setScenarioPhase("intro-rendering");
  }

  if (evidence.mainMenuReached && !evidence.sawIntro
   && telemetry.lid === 0x19
   && telemetry.loadedTitleState === 15) {
    markFailure("idle title flow entered map state 15 before intro lid 56", "scenario:await-idle-attract-intro");
    return;
  }

  if (evidence.sawIntro && telemetry.lid === 0x38) {
    const introAtMs = elapsed - evidence.introStartedAtMs;
    const audioFailure = sampleIntroAudioRegression(evidence.introAudio, {
      atMs: introAtMs,
      delayedVoices: telemetry.delayedVoices,
      audioCallbacks: telemetry.audioCallbacks,
      audioMaxGapUs: telemetry.audioMaxGapUs,
      activeSfx: telemetry.activeSfx,
      completedSampleRekeys: telemetry.completedSampleRekeys,
      visible: document.visibilityState === "visible",
    });
    if (audioFailure) {
      markFailure(audioFailure, "scenario:intro-audio");
      return;
    }
    evidence.introSamples++;
    if (telemetry.convertedTriangles > 0) evidence.introTriangleSamples++;
    scenarioIntroCameras.add(telemetry.camera.join(","));
    evidence.introDistinctCameras = scenarioIntroCameras.size;
    if (evidence.introSamples >= 50
     && evidence.introTriangleSamples >= 5
     && evidence.introDistinctCameras >= 2
     && (evidence.introSkipAttempts || 0) < 5
     && (!evidence.lastIntroSkipAt || Date.now() - evidence.lastIntroSkipAt >= 2000)) {
      if (pulseScenarioPad(0x40)) {
        evidence.introSkipAttempts = (evidence.introSkipAttempts || 0) + 1;
        evidence.lastIntroSkipAt = Date.now();
        setScenarioPhase("await-title-after-intro");
      }
    }
    return;
  }

  if (evidence.sawIntro
   && telemetry.lid === 0x19
   && telemetry.loadedTitleState !== -1
   && telemetry.titleTransition === 3) {
    if ((evidence.introTriangleSamples || 0) < 5 || (evidence.introDistinctCameras || 0) < 2) {
      markFailure("intro transitioned without sustained rendered/camera evidence", "scenario:await-title-after-intro");
      return;
    }
    const audioFailure = finishIntroAudioRegression(evidence.introAudio);
    if (audioFailure) {
      markFailure(audioFailure, "scenario:intro-audio");
      return;
    }
    finishScenarioPass();
  }
}

function runLevelCompleteScenario(telemetry) {
  const elapsed = Date.now() - scenarioStartedAt;
  const evidence = lifecycle.scenarioEvidence;
  if (elapsed > 45000) {
    markFailure(`timed out; route=${JSON.stringify(lifecycle.scenarioRoute)}`, "scenario:level-complete");
    return;
  }

  if (!evidence.entranceSkipSent
   && telemetry.lid === 0x09
   && telemetry.gameState === 0
   && elapsed >= 1000) {
    evidence.entranceSkipSent = pulseScenarioPad(0x40);
    if (evidence.entranceSkipSent) setScenarioPhase("skip-level-entrance");
    return;
  }

  if (telemetry.lid === 0x09 && telemetry.gameState === 0x100)
    evidence.playingSamples = (evidence.playingSamples || 0) + 1;
  else if (!evidence.completionEventAccepted)
    evidence.playingSamples = 0;

  if (!evidence.completionEventAccepted
   && telemetry.lid === 0x09
   && telemetry.gameState === 0x100
   && evidence.playingSamples >= 10
   && telemetry.convertedTriangles > 0
   && (!evidence.lastAttemptAt || Date.now() - evidence.lastAttemptAt >= 500)) {
    evidence.lastAttemptAt = Date.now();
    evidence.completionEventAttempts = (evidence.completionEventAttempts || 0) + 1;
    const eventResult = module._C1DebugCrashEvent(0x1600);
    evidence.completionEventResult = eventResult;
    if (eventResult !== 0) {
      evidence.completionEventAccepted = true;
      setScenarioPhase("await-level-complete");
    }
    return;
  }

  if (evidence.completionEventAccepted && telemetry.lid !== 0x09 && telemetry.lid !== 0x2d) {
    markFailure(`completion event transitioned to unexpected lid ${telemetry.lid}`, "scenario:await-level-complete");
    return;
  }

  if (telemetry.lid === 0x2d) {
    if (!evidence.sawLevelComplete) {
      evidence.sawLevelComplete = true;
      evidence.levelCompleteSamples = 0;
      evidence.levelCompleteRenderedSamples = 0;
      setScenarioPhase("level-complete-rendering");
    }
    evidence.levelCompleteSamples++;
    if (telemetry.convertedTriangles > 0 && telemetry.worldPrimitives > 0)
      evidence.levelCompleteRenderedSamples++;
    if (evidence.levelCompleteSamples >= 10 && evidence.levelCompleteRenderedSamples >= 5)
      finishScenarioPass();
  }
}

function runScenario(telemetry) {
  if (!lifecycle.scenario || lifecycle.status === "fail" || lifecycle.status === "pass") return;
  recordScenarioRoute(telemetry);
  if (lifecycle.scenario === "title-attract-intro") runTitleAttractIntroScenario(telemetry);
  else if (lifecycle.scenario === "level-complete") runLevelCompleteScenario(telemetry);
}

function recordGlobalFailure(kind, value) {
  if (isEmscriptenUnwind(value)) return;
  const message = `${kind}: ${oneLine(value)}`;
  lifecycle.globalErrors.push(message);
  markFailure(message, "global");
}

window.addEventListener("error", (event) => {
  recordGlobalFailure("error", event.error?.message || event.message || event.error);
});
window.addEventListener("unhandledrejection", (event) => {
  recordGlobalFailure("unhandledrejection", event.reason?.message || event.reason);
});

renderResult();
if (scenarioName && !Object.hasOwn(scenarioLids, scenarioName))
  markFailure(`unknown scenario ${scenarioName}`, "scenario");

startButton.addEventListener("click", async (event) => {
  if (lifecycle.status === "fail") return;
  event.currentTarget.disabled = true;
  setPhase("module");
  try {
    if (scenarioName) {
      localStorage.removeItem("c1.virtual-memory-card.v1");
      localStorage.removeItem("c1.browser-resume.v1");
      lifecycle.scenarioStorageReset = true;
    }
    const { default: createC1Module } = await import("/dist/c1.mjs");
    const preservedWebGLContext = canvas.getContext("webgl", {
      alpha: false,
      antialias: false,
      depth: true,
      preserveDrawingBuffer: true,
      stencil: false,
    });
    if (!preservedWebGLContext) throw new Error("WebGL is unavailable.");
    module = await createC1Module({
      canvas,
      preinitializedWebGLContext: preservedWebGLContext,
      GL_MAX_TEXTURE_IMAGE_UNITS: 1,
      noInitialRun: true,
      locateFile: (path) => `/dist/${path}`,
      print: appendLine,
      printErr: appendLine,
      onAbort: (reason) => {
        lifecycle.abortReason = oneLine(reason);
        markFailure(`abort: ${lifecycle.abortReason}`, "runtime");
      },
    });
    if (lifecycle.status === "fail") return;
    try { module.FS.mkdir("/streams"); } catch {}
    setPhase("streams");
    let names = await (await fetch("/stream-manifest.json")).json();
    if (q.get("minimal") === "1") {
      const stem = `s${lid.toString(16).padStart(7, "0")}.`;
      names = names.filter((name) => name.startsWith(stem));
    }
    for (const name of names) {
      const response = await fetch(`/streams/${name}`);
      if (!response.ok) throw new Error(`${name}: HTTP ${response.status}`);
      const bytes = new Uint8Array(await response.arrayBuffer());
      module.FS.writeFile(`/streams/${name}`, bytes, { canOwn: true });
    }
    lifecycle.streamCount = names.length;
    if (lifecycle.status === "fail") return;
    setPhase("main");
    const mainResult = callMainOnce(module, [String(lid)]);
    lifecycle.mainStatus = mainResult.status;
    lifecycle.mainUnwound = mainResult.unwound;
    renderResult();
    if (mainResult.status !== 0) {
      throw new Error(`callMain exited with status ${mainResult.status}`);
    }
    if (lifecycle.abortReason || lifecycle.globalErrors.length || lifecycle.status === "fail") return;
    runtimeFrameGate = advanceRuntimeFrameGate(runtimeFrameGate, "main-ready");
    setPhase("awaiting-frame");
    if (scenarioName) {
      scenarioStartedAt = Date.now();
      setScenarioPhase("awaiting-first-frame");
    }
    canvas.focus();
  } catch (error) {
    markFailure(error?.message || error);
  }
}, { once: true });

document.querySelector("#death").addEventListener("click", () => module._C1DebugCrashEvent(0x900));
document.querySelector("#drown").addEventListener("click", () => module._C1DebugCrashEvent(0x2100));
document.querySelector("#forward").addEventListener("click", () => {
  module._C1SetVirtualPad(0x1000);
  setTimeout(() => module._C1SetVirtualPad(0), 8000);
});
document.querySelector("#mapSpin").addEventListener("click", () => {
  module._C1SetVirtualPad(0x2000);
  setTimeout(() => module._C1SetVirtualPad(0), 10000);
});
function pulsePad(bit) {
  module._C1SetVirtualPad(bit);
  setTimeout(() => module._C1SetVirtualPad(0), 120);
}
document.querySelector("#padDown").addEventListener("click", () => pulsePad(0x4000));
document.querySelector("#padCross").addEventListener("click", () => pulsePad(0x40));
document.querySelector("#padStart").addEventListener("click", () => pulsePad(0x800));
document.querySelector("#padSelect").addEventListener("click", () => pulsePad(0x100));
document.querySelector("#freeze").addEventListener("click", (event) => {
  frameFrozen = !frameFrozen;
  module._C1SetDebugFramePaused(frameFrozen ? 1 : 0);
  event.currentTarget.textContent = frameFrozen ? "Resume frame" : "Freeze frame";
});

setInterval(() => {
  const canSample = runtimeFrameGate === "awaiting-frame" || runtimeFrameGate === "pass";
  if (!module || !canSample || lifecycle.status === "fail") {
    renderMetrics();
    return;
  }
  try {
    const frameMisses = module._C1GetTextureFrameMisses();
    const frameFailures = module._C1GetTextureFrameFailures();
    const frameMissingPages = module._C1GetTextureFrameMissingPages();
    const frameGenerationMisses = module._C1GetTextureFrameGenerationMisses();
    const frameCacheFailures = module._C1GetTextureFrameCacheFailures();
    const frameUploadBytes = module._C1GetTextureFrameUploadBytes();
    maxFrameMisses = Math.max(maxFrameMisses, frameMisses);
    if (frameFailures > maxFrameFailures) {
      maxFrameFailures = frameFailures;
      worstFailureFrame = {
        failures: frameFailures,
        missingPages: frameMissingPages,
        generationMisses: frameGenerationMisses,
        cacheFailures: frameCacheFailures,
        fade: module._C1GetFadeCounter(),
        titleState: module._C1GetTitleState(),
        titleTransition: module._C1GetTitleTransitionState(),
        display: `0x${module._C1GetDisplayFlags().toString(16)}`,
      };
    }
    maxFrameMissingPages = Math.max(maxFrameMissingPages, frameMissingPages);
    maxFrameGenerationMisses = Math.max(maxFrameGenerationMisses, frameGenerationMisses);
    maxFrameCacheFailures = Math.max(maxFrameCacheFailures, frameCacheFailures);
    maxFrameUploadBytes = Math.max(maxFrameUploadBytes, frameUploadBytes);
    maxTextureTotalFailures = Math.max(maxTextureTotalFailures, module._C1GetTextureTotalFailures());
    maxGlErrors = Math.max(maxGlErrors, module._C1GetGlErrorCount());
    maxPrimitiveOverflows = Math.max(maxPrimitiveOverflows, module._C1GetPrimitiveOverflowCount());
    const frameUs = module._C1GetLastFrameUs();
    lastTelemetry = {
      lid: module._C1GetCurrentLid(),
      gameState: module._C1GetGameState(),
      titleState: module._C1GetTitleState(),
      loadedTitleState: module._C1GetLoadedTitleState(),
      titleTransition: module._C1GetTitleTransitionState(),
      life: module._C1GetLifeCount(),
      death: module._C1GetDeathCount(),
      respawn: module._C1GetRespawnCount(),
      display: `0x${module._C1GetDisplayFlags().toString(16)}`,
      nextDisplay: `0x${module._C1GetNextDisplayFlags().toString(16)}`,
      fade: module._C1GetFadeCounter(),
      fill: `0x${module._C1GetVramFillRgb().toString(16).padStart(6, "0")}`,
      nextFill: `0x${module._C1GetNextVramFillRgb().toString(16).padStart(6, "0")}`,
      camera: [module._C1GetCamX(), module._C1GetCamY(), module._C1GetCamZ()],
      cameraRotation: [module._C1GetCamRotX(), module._C1GetCamRotY()],
      deathCameraOrbit: module._C1GetDeathCamOrbit(),
      deathCameraFlipVelocity: module._C1GetDeathCamFlipVelocity(),
      textureRequests: module._C1GetTextureFrameRequests(),
      textureHits: module._C1GetTextureFrameHits(),
      textureMisses: frameMisses,
      textureFailures: frameFailures,
      textureMissingPages: frameMissingPages,
      textureGenerationMisses: frameGenerationMisses,
      textureCacheFailures: frameCacheFailures,
      textureTotalMisses: module._C1GetTextureTotalMisses(),
      textureTotalFailures: module._C1GetTextureTotalFailures(),
      textureOwnedCount: module._C1GetTextureOwnedCount(),
      textureOwnedBytes: module._C1GetTextureOwnedBytes(),
      pageChanges: module._C1GetTextureFramePageChanges(),
      totalPageChanges: module._C1GetTextureTotalPageChanges(),
      uploadBytes: frameUploadBytes,
      maxFrameUploadBytes,
      totalUploadBytes: module._C1GetTextureTotalUploadBytes(),
      primitiveBytes: module._C1GetPrimitiveBytes(),
      maxPrimitiveBytes: module._C1GetMaxPrimitiveBytes(),
      primitiveOverflows: module._C1GetPrimitiveOverflowCount(),
      worldPrimitives: module._C1GetWorldPrimitiveCount(),
      convertedTriangles: module._C1GetConvertedTriangleCount(),
      largestTriangle: {
        area2: module._C1GetLargestTriangleArea2() >>> 0,
        index: module._C1GetLargestTriangleIndex() >>> 0,
        vertices: [0, 1, 2].map((vertex) => [
          module._C1GetLargestTriangleX(vertex),
          module._C1GetLargestTriangleY(vertex),
        ]),
        texid: module._C1GetLargestTriangleTexid(),
        flags: module._C1GetLargestTriangleFlags(),
        type: module._C1GetLargestTriangleType(),
      },
      outsideTriangles: module._C1GetOutsideTriangleCount(),
      visiblePolys: module._C1GetVisiblePolyCount(),
      glError: "0x" + module._C1GetGlError().toString(16),
      glErrors: module._C1GetGlErrorCount(),
      drawSkip: module._C1GetDrawSkipCounter(),
      audioCallbacks: module._C1GetAudioCallbackCount(),
      audioPeak: module._C1GetAudioPeak(),
      audioClips: module._C1GetAudioClipCount(),
      audioDeadlineMisses: module._C1GetAudioDeadlineMissCount(),
      audioMaxGapUs: module._C1GetAudioMaxGapUs(),
      audioMaxCallbackUs: module._C1GetAudioMaxCallbackUs(),
      musicPeak: module._C1GetAudioMusicPeak(),
      sfxPeak: module._C1GetAudioSfxPeak(),
      activeSfx: module._C1GetAudioActiveSfx(),
      delayedVoices: module._C1GetAudioDelayedVoiceCount?.() ?? null,
      completedSampleRekeys: module._C1GetAudioCompletedSampleRekeyCount?.() ?? null,
      sampleCacheHits: module._C1GetSampleCacheHits(),
      sampleCacheMisses: module._C1GetSampleCacheMisses(),
      sampleCacheBytes: module._C1GetSampleCacheBytes(),
      maxFrameMisses,
      maxFrameFailures,
      maxFrameMissingPages,
      maxFrameGenerationMisses,
      maxFrameCacheFailures,
      maxTextureTotalFailures,
      maxGlErrors,
      maxPrimitiveOverflows,
      worstFailureFrame,
      frameUs,
      maxFrameUs: module._C1GetMaxFrameUs(),
      heapSize: module._C1GetHeapSize(),
      heapEnd: module._C1GetHeapAllocatedEnd(),
    };
    lifecycle.telemetrySamples++;
    if (frameUs > 0) {
      lifecycle.frameSamples++;
      if (lifecycle.firstFrameUs === null) lifecycle.firstFrameUs = frameUs;
      if (runtimeFrameGate === "awaiting-frame") {
        runtimeFrameGate = advanceRuntimeFrameGate(runtimeFrameGate, "frame-sample");
        buttons.forEach((button) => { button.disabled = false; });
        if (scenarioName) setScenarioPhase("running");
        else markPass();
      }
      runScenario(lastTelemetry);
    }
    renderMetrics();
  } catch (error) {
    markFailure(error?.message || error, "telemetry");
  }
}, 100);

if (scenarioName) queueMicrotask(() => startButton.click());
