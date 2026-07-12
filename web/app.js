import createC1Module from "./c1.mjs";
import { extractCrashStreamsFromDisc } from "./disc-image.js";
import { callMainOnce, isEmscriptenUnwind } from "./runtime-lifecycle.js";

const LEVELS = [
  [0x03, "Cortex Power"], [0x04, "Cave"],
  [0x05, "Generator Room"], [0x06, "Heavy Machinery"], [0x07, "Toxic Waste"],
  [0x08, "Pinstripe"], [0x09, "N. Sanity Beach"], [0x0a, "Papu Papu"],
  [0x0c, "Jungle Rollers"], [0x0e, "Boulders"], [0x0f, "Upstream"], [0x11, "Hog Wild"],
  [0x12, "The Great Gate"], [0x13, "Boulder Dash"], [0x14, "Road to Nowhere"],
  [0x15, "Rolling Stones"], [0x16, "The High Road"], [0x17, "Ripper Roo"],
  [0x18, "Up the Creek"], [0x19, "Title / Island Map"], [0x1a, "Native Fortress"],
  [0x1b, "Dr. N. Brio"], [0x1c, "Temple Ruins"], [0x1d, "Jaws of Darkness"],
  [0x1e, "Whole Hog"], [0x1f, "Dr. Neo Cortex"], [0x20, "The Lost City"],
  [0x21, "Koala Kong"], [0x22, "Stormy Ascent"], [0x23, "Sunset Vista"],
  [0x24, "Tawna Bonus 1"], [0x25, "Brio Bonus"], [0x26, "Bonus"], [0x28, "Lights Out"],
  [0x29, "The Lab"], [0x2a, "Fumbling in the Dark"], [0x2c, "The Great Hall"],
  [0x2d, "Level Complete"], [0x2e, "Slippery Climb"], [0x33, "Tawna Bonus 2"],
  [0x34, "Cortex Bonus"], [0x37, "Castle Machinery"], [0x38, "Intro"], [0x39, "Ending"],
];

// The retail 0x04 pair is an index-only asset archive. Keep importing and
// mounting it for levels that reference those assets, but never boot it as a
// standalone level because it has no LDAT metadata.
const PLAYABLE_LEVELS = LEVELS.filter(([lid]) => lid !== 0x04);

const shell = document.querySelector(".shell");
const canvas = document.querySelector("#canvas");
const screen = document.querySelector("#screen");
const statusNode = document.querySelector("#runtimeStatus");
const dropzone = document.querySelector("#dropzone");
const folderInput = document.querySelector("#folderInput");
const fileInput = document.querySelector("#fileInput");
const chooseFolderButton = document.querySelector("#chooseFolderButton");
const recognizedFilesNode = document.querySelector("#recognizedFiles");
const pairCountNode = document.querySelector("#pairCount");
const requiredCountNode = document.querySelector("#requiredCount");
const assetSizeNode = document.querySelector("#assetSize");
const assetMessage = document.querySelector("#assetMessage");
const bootLevel = document.querySelector("#bootLevel");
const launchButton = document.querySelector("#launchButton");
const clearButton = document.querySelector("#clearButton");
const loadProgress = document.querySelector("#loadProgress");
const progressBar = document.querySelector("#progressBar");
const progressLabel = document.querySelector("#progressLabel");
const fullscreenButton = document.querySelector("#fullscreenButton");
const muteButton = document.querySelector("#muteButton");
const runtimeLog = document.querySelector("#runtimeLog");

const streamFiles = new Map();
const logLines = ["C1 loader ready."];
let module;
let mountedAssets;
let launching = false;
let importingDisc = false;
let runtimeLocked = false;
let runtimeFailure;
let muted = false;
let virtualPadState = 0;
const activePointers = new Map();

window.__consoleErrors = [];
window.addEventListener("error", (event) => {
  const message = String(event.error?.stack || event.error || event.message);
  window.__consoleErrors.push(message);
  appendLog(`Browser error: ${message}`, "err");
});
window.addEventListener("unhandledrejection", (event) => {
  const message = String(event.reason?.stack || event.reason || "Unhandled promise rejection");
  if (!isEmscriptenUnwind(event.reason)) {
    window.__consoleErrors.push(message);
    appendLog(`Unhandled rejection: ${message}`, "err");
  }
});

function flushBrowserResume() {
  module?._C1FlushBrowserResume?.();
}

window.addEventListener("pagehide", flushBrowserResume);
document.addEventListener("visibilitychange", () => {
  if (document.visibilityState === "hidden") flushBrowserResume();
});

requiredCountNode.textContent = String(LEVELS.length);

function setRuntimeState(state, label) {
  shell.dataset.runtimeState = state;
  statusNode.textContent = label;
}

function appendLog(message, kind = "out") {
  const line = `${kind === "err" ? "!" : ">"} ${String(message)}`;
  logLines.push(line);
  if (logLines.length > 90) logLines.splice(1, logLines.length - 90);
  runtimeLog.textContent = logLines.join("\n");
  runtimeLog.scrollTop = runtimeLog.scrollHeight;
}

function normalizeStreamName(name) {
  const base = name.split(/[\\/]/).pop().toLowerCase();
  const match = /^s([0-9a-f]{7})\.(nsd|nsf)$/.exec(base);
  return match ? `s${match[1]}.${match[2]}` : null;
}

function levelStem(lid) {
  return `s${lid.toString(16).padStart(7, "0")}`;
}

function hasLevelPair(lid) {
  const stem = levelStem(lid);
  return streamFiles.has(`${stem}.nsd`) && streamFiles.has(`${stem}.nsf`);
}

function formatBytes(bytes) {
  if (!bytes) return "0 B";
  const units = ["B", "KiB", "MiB", "GiB"];
  const index = Math.min(Math.floor(Math.log(bytes) / Math.log(1024)), units.length - 1);
  const value = bytes / (1024 ** index);
  return `${value.toFixed(index === 0 || value >= 100 ? 0 : 1)} ${units[index]}`;
}

function updateControlAvailability(playableLevels = PLAYABLE_LEVELS.filter(([lid]) => hasLevelPair(lid))) {
  const importsDisabled = runtimeLocked || launching || importingDisc;
  chooseFolderButton.disabled = importsDisabled;
  folderInput.disabled = importsDisabled;
  fileInput.disabled = importsDisabled;
  dropzone.setAttribute("aria-disabled", String(importsDisabled));
  dropzone.tabIndex = importsDisabled ? -1 : 0;
  bootLevel.disabled = playableLevels.length === 0 || importsDisabled;
  launchButton.disabled = playableLevels.length === 0 || importsDisabled;
  clearButton.disabled = streamFiles.size === 0 || importsDisabled;
}

function refreshAssets() {
  const previousBoot = Number(bootLevel.value);
  const completePairs = LEVELS.filter(([lid]) => hasLevelPair(lid));
  const playableLevels = PLAYABLE_LEVELS.filter(([lid]) => hasLevelPair(lid));
  const totalBytes = [...streamFiles.values()].reduce((sum, file) => sum + file.size, 0);
  recognizedFilesNode.textContent = `${streamFiles.size} file${streamFiles.size === 1 ? "" : "s"}`;
  pairCountNode.textContent = String(completePairs.length);
  assetSizeNode.textContent = formatBytes(totalBytes);

  bootLevel.replaceChildren();
  for (const [lid, name] of playableLevels) {
    const option = document.createElement("option");
    option.value = String(lid);
    option.textContent = `0x${lid.toString(16).padStart(2, "0").toUpperCase()} — ${name}`;
    bootLevel.append(option);
  }

  updateControlAvailability(playableLevels);

  if (playableLevels.length) {
    const fallback = playableLevels.some(([lid]) => lid === 0x19) ? 0x19 : playableLevels[0][0];
    bootLevel.value = String(playableLevels.some(([lid]) => lid === previousBoot) ? previousBoot : fallback);
  } else {
    const option = document.createElement("option");
    option.textContent = "Select game data first";
    bootLevel.append(option);
  }

  assetMessage.className = "asset-message";
  if (!streamFiles.size) {
    assetMessage.textContent = "No game files selected yet.";
  } else if (!playableLevels.length) {
    assetMessage.classList.add("is-warning");
    assetMessage.textContent = "Files were recognized, but no playable level has both its NSD and NSF file.";
  } else if (completePairs.length === LEVELS.length) {
    assetMessage.classList.add("is-ready");
    assetMessage.textContent = "Full stream set recognized: 43 playable pairs plus the Cave asset archive.";
  } else {
    assetMessage.classList.add("is-warning");
    assetMessage.textContent = `${playableLevels.length} bootable level pair${playableLevels.length === 1 ? "" : "s"}. Missing pairs can stop later level transitions.`;
  }
}

async function acceptFiles(files) {
  if (runtimeLocked) {
    appendLog("Game data is locked for this runtime. Reload the page to choose different files.", "err");
    return;
  }
  const candidates = [...files];
  const discImages = candidates.filter((file) => /\.(?:bin|iso)$/i.test(file.name));
  let accepted = 0;

  if (discImages.length) {
    const disc = discImages[0];
    importingDisc = true;
    refreshAssets();
    setRuntimeState("loading", "Reading local disc image");
    loadProgress.hidden = false;
    progressBar.style.width = "2%";
    progressLabel.textContent = "Detecting disc image…";
    appendLog(`Scanning ${disc.name} without uploading it.`);
    try {
      const extracted = await extractCrashStreamsFromDisc(disc, async (progress) => {
        progressLabel.textContent = progress.message;
        if (progress.total > 0 && progress.loaded > 0) {
          progressBar.style.width = `${Math.max(3, (progress.loaded / progress.total) * 100)}%`;
        }
        await new Promise((resolve) => requestAnimationFrame(resolve));
      });
      for (const [name, file] of extracted) streamFiles.set(name, file);
      accepted += extracted.size;
      progressBar.style.width = "100%";
      appendLog(`Prepared ${extracted.size} stream files from ${disc.name}.`);
      setRuntimeState("idle", "Disc data ready");
    } catch (error) {
      const message = String(error?.message || error);
      setRuntimeState("error", "Disc image could not be read");
      assetMessage.className = "asset-message is-warning";
      assetMessage.textContent = message;
      appendLog(message, "err");
      window.__consoleErrors.push(message);
    } finally {
      importingDisc = false;
      setTimeout(() => {
        if (!launching) loadProgress.hidden = true;
      }, 500);
    }
  }

  for (const file of candidates) {
    const normalized = normalizeStreamName(file.name);
    if (!normalized) continue;
    streamFiles.set(normalized, file);
    accepted += 1;
  }
  if (accepted > 0) {
    appendLog(`Accepted ${accepted} stream file${accepted === 1 ? "" : "s"}.`);
  }
  refreshAssets();
}

function readDirectoryEntries(reader) {
  return new Promise((resolve, reject) => {
    const entries = [];
    const next = () => reader.readEntries((batch) => {
      if (!batch.length) resolve(entries);
      else { entries.push(...batch); next(); }
    }, reject);
    next();
  });
}

async function filesFromEntry(entry) {
  if (entry.isFile) {
    return [await new Promise((resolve, reject) => entry.file(resolve, reject))];
  }
  if (!entry.isDirectory) return [];
  const children = await readDirectoryEntries(entry.createReader());
  const nested = await Promise.all(children.map(filesFromEntry));
  return nested.flat();
}

async function filesFromDrop(dataTransfer) {
  const entries = [...(dataTransfer.items || [])]
    .map((item) => item.webkitGetAsEntry?.())
    .filter(Boolean);
  if (entries.length) {
    const nested = await Promise.all(entries.map(filesFromEntry));
    return nested.flat();
  }
  return [...dataTransfer.files];
}

async function launch() {
  if (runtimeLocked || launching || bootLevel.disabled) return;
  runtimeLocked = true;
  launching = true;
  refreshAssets();
  setRuntimeState("loading", "Loading WebAssembly runtime");
  loadProgress.hidden = false;
  progressBar.style.width = "2%";
  progressLabel.textContent = "Loading WebAssembly runtime…";
  appendLog("Creating wasm32 runtime.");
  let abortReason;

  try {
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
      locateFile: (path) => `./${path}`,
      print: (message) => appendLog(message),
      printErr: (message) => appendLog(message, "err"),
      onAbort: (reason) => {
        abortReason = `Engine aborted: ${reason}`;
        failRuntime(abortReason);
      },
    });

    try { module.FS.mkdir("/streams"); } catch (error) {
      if (!String(error).includes("File exists")) throw error;
    }

    const selectedBoot = Number(bootLevel.value);
    const entries = [...streamFiles.entries()].sort(([a], [b]) => a.localeCompare(b));
    const totalBytes = entries.reduce((sum, [, file]) => sum + file.size, 0);
    const completePairs = LEVELS.filter(([lid]) => hasLevelPair(lid)).length;
    let loadedBytes = 0;

    for (const [name, file] of entries) {
      progressLabel.textContent = `Mounting ${name} · ${formatBytes(loadedBytes)} / ${formatBytes(totalBytes)}`;
      const contents = new Uint8Array(await file.arrayBuffer());
      module.FS.writeFile(`/streams/${name}`, contents, { canOwn: true });
      loadedBytes += file.size;
      progressBar.style.width = `${Math.max(4, (loadedBytes / totalBytes) * 94)}%`;
      await new Promise((resolve) => requestAnimationFrame(resolve));
    }

    mountedAssets = { files: entries.length, completePairs, totalBytes };
    streamFiles.clear();
    entries.length = 0;
    folderInput.value = "";
    fileInput.value = "";
    appendLog("Released the source disc references after mounting game data.");
    progressBar.style.width = "100%";
    progressLabel.textContent = "Starting C1…";
    appendLog(`Starting at level 0x${selectedBoot.toString(16)}.`);
    const mainResult = callMainOnce(module, [String(selectedBoot)]);
    if (abortReason) throw new Error(abortReason);
    if (mainResult.status !== 0) {
      throw new Error(`Engine exited with status ${mainResult.status}.`);
    }
    if (runtimeFailure) throw new Error(runtimeFailure);

    setRuntimeState("running", "Native engine running");
    progressLabel.textContent = "C1 is running.";
    fullscreenButton.disabled = false;
    muteButton.disabled = false;
    canvas.focus();
  } catch (error) {
    failRuntime(error?.stack || error);
  } finally {
    launching = false;
    updateControlAvailability();
  }
}

function failRuntime(reason) {
  const message = String(reason || "Unknown runtime failure");
  const firstFailure = !runtimeLocked || runtimeFailure === undefined;
  if (runtimeLocked && runtimeFailure === undefined) runtimeFailure = message;
  setRuntimeState("error", "Runtime error");
  if (firstFailure) appendLog(message, "err");
  progressLabel.textContent = "Runtime failed. Reload this page before trying again.";
  assetMessage.className = "asset-message is-warning";
  assetMessage.textContent = runtimeLocked
    ? "The engine stopped. Reload this page before starting another runtime."
    : "The selected files could not be read. You can correct the selection and try again.";
  if (firstFailure) window.__consoleErrors.push(message);
  updateControlAvailability();
}

chooseFolderButton.addEventListener("click", (event) => {
  event.stopPropagation();
  if (runtimeLocked) return;
  folderInput.click();
});

dropzone.addEventListener("click", (event) => {
  if (runtimeLocked || event.target === chooseFolderButton) return;
  fileInput.click();
});

dropzone.addEventListener("keydown", (event) => {
  if (runtimeLocked) return;
  if (event.key === "Enter" || event.key === " ") {
    event.preventDefault();
    folderInput.click();
  }
});

folderInput.addEventListener("change", () => void acceptFiles(folderInput.files));
fileInput.addEventListener("change", () => void acceptFiles(fileInput.files));

for (const type of ["dragenter", "dragover"]) {
  dropzone.addEventListener(type, (event) => {
    event.preventDefault();
    dropzone.classList.add("is-dragging");
  });
}

for (const type of ["dragleave", "drop"]) {
  dropzone.addEventListener(type, (event) => {
    event.preventDefault();
    dropzone.classList.remove("is-dragging");
  });
}

dropzone.addEventListener("drop", async (event) => {
  try { await acceptFiles(await filesFromDrop(event.dataTransfer)); }
  catch (error) { failRuntime(`Could not read dropped folder: ${error}`); }
});

clearButton.addEventListener("click", () => {
  if (runtimeLocked) return;
  streamFiles.clear();
  folderInput.value = "";
  fileInput.value = "";
  appendLog("Cleared selected files.");
  refreshAssets();
});

launchButton.addEventListener("click", launch);

fullscreenButton.addEventListener("click", async () => {
  try {
    if (document.fullscreenElement) await document.exitFullscreen();
    else await screen.requestFullscreen();
  } catch (error) {
    appendLog(`Fullscreen unavailable: ${error}`, "err");
  }
});

muteButton.addEventListener("click", () => {
  if (!module?._C1SetAudioPaused) return;
  muted = !muted;
  module._C1SetAudioPaused(muted ? 1 : 0);
  muteButton.setAttribute("aria-pressed", String(muted));
  muteButton.textContent = muted ? "Sound off" : "Sound on";
});

function syncVirtualPad() {
  virtualPadState = 0;
  for (const bit of activePointers.values()) virtualPadState |= bit;
  module?._C1SetVirtualPad?.(virtualPadState);
}

function resumeBrowserAudio() {
  module?.SDL2?.audioContext?.resume?.();
}

window.addEventListener("pointerdown", resumeBrowserAudio);
window.addEventListener("keydown", resumeBrowserAudio);

for (const button of document.querySelectorAll("[data-pad-bit]")) {
  const press = (event) => {
    event.preventDefault();
    button.setPointerCapture?.(event.pointerId);
    activePointers.set(event.pointerId, Number(button.dataset.padBit));
    button.classList.add("is-active");
    syncVirtualPad();
  };
  const release = (event) => {
    activePointers.delete(event.pointerId);
    button.classList.remove("is-active");
    syncVirtualPad();
  };
  button.addEventListener("pointerdown", press);
  button.addEventListener("pointerup", release);
  button.addEventListener("pointercancel", release);
  button.addEventListener("lostpointercapture", release);
}

window.addEventListener("blur", () => {
  activePointers.clear();
  document.querySelectorAll(".pad-button.is-active").forEach((button) => button.classList.remove("is-active"));
  syncVirtualPad();
});

document.addEventListener("keydown", (event) => {
  if (shell.dataset.runtimeState === "running" && ["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight", " "].includes(event.key)) {
    event.preventDefault();
  }
});

window.__c1Debug = {
  get module() { return module; },
  get recognizedFiles() { return mountedAssets?.files ?? streamFiles.size; },
  get completePairs() { return mountedAssets?.completePairs ?? LEVELS.filter(([lid]) => hasLevelPair(lid)).length; },
  get audio() {
    return module ? {
      callbacks: module._C1GetAudioCallbackCount?.() ?? 0,
      peak: module._C1GetAudioPeak?.() ?? 0,
      clips: module._C1GetAudioClipCount?.() ?? 0,
      deadlineMisses: module._C1GetAudioDeadlineMissCount?.() ?? 0,
      maxGapUs: module._C1GetAudioMaxGapUs?.() ?? 0,
      maxCallbackUs: module._C1GetAudioMaxCallbackUs?.() ?? 0,
      musicPeak: module._C1GetAudioMusicPeak?.() ?? 0,
      sfxPeak: module._C1GetAudioSfxPeak?.() ?? 0,
      musicRms: module._C1GetAudioMusicRms?.() ?? 0,
      sfxRms: module._C1GetAudioSfxRms?.() ?? 0,
      activeSfx: module._C1GetAudioActiveSfx?.() ?? 0,
      sampleCacheHits: module._C1GetSampleCacheHits?.() ?? 0,
      sampleCacheMisses: module._C1GetSampleCacheMisses?.() ?? 0,
      sampleCacheBytes: module._C1GetSampleCacheBytes?.() ?? 0,
    } : null;
  },
  get card() {
    return module ? {
      parts: module._C1GetCardPartCount?.() ?? 0,
      flags: module._C1GetCardFlags?.() ?? 0,
    } : null;
  },
  get game() {
    return module ? {
      lid: module._C1GetCurrentLid?.() ?? -1,
      titleState: module._C1GetTitleState?.() ?? -1,
      loadedTitleState: module._C1GetLoadedTitleState?.() ?? -1,
      transitionState: module._C1GetTitleTransitionState?.() ?? -1,
      resumeResult: module._C1GetBrowserResumeResult?.() ?? -1,
      levelCount: module._C1GetLevelCount?.() ?? 0,
      keyCount: module._C1GetKeyCount?.() ?? 0,
      gemCount: module._C1GetGemCount?.() ?? 0,
      sfxVolume: module._C1GetSfxVolume?.() ?? 0,
      musicVolume: module._C1GetMusicVolume?.() ?? 0,
      mono: module._C1GetMono?.() ?? 0,
    } : null;
  },
};

refreshAssets();
