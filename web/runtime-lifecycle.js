export function isEmscriptenUnwind(error) {
  const message = String(error?.message ?? error ?? "").trim().toLowerCase();
  return message === "unwind";
}

const RUNTIME_GATE_STATES = new Set(["idle", "awaiting-frame", "pass", "fail"]);
const RUNTIME_GATE_EVENTS = new Set(["main-ready", "frame-sample", "failure"]);

export function advanceRuntimeFrameGate(state, event) {
  if (!RUNTIME_GATE_STATES.has(state)) {
    throw new TypeError(`Unknown runtime frame gate state: ${state}`);
  }
  if (!RUNTIME_GATE_EVENTS.has(event)) {
    throw new TypeError(`Unknown runtime frame gate event: ${event}`);
  }
  if (event === "failure" || state === "fail") return "fail";
  if (state === "idle" && event === "main-ready") return "awaiting-frame";
  if (state === "awaiting-frame" && event === "frame-sample") return "pass";
  return state;
}

function exitStatusFrom(error) {
  const status = Number(error?.status);
  return Number.isInteger(status) ? status : null;
}

export function callMainOnce(runtime, args = []) {
  if (!runtime || typeof runtime.callMain !== "function") {
    throw new TypeError("The WebAssembly runtime does not expose callMain().");
  }

  let status = 0;
  let unwound = false;
  try {
    const returnedStatus = runtime.callMain(args);
    if (returnedStatus !== undefined) {
      if (!Number.isInteger(returnedStatus)) {
        throw new TypeError(`callMain() returned an invalid status: ${returnedStatus}`);
      }
      status = returnedStatus;
    }
  } catch (error) {
    if (isEmscriptenUnwind(error)) {
      unwound = true;
    } else {
      const exitStatus = exitStatusFrom(error);
      if (exitStatus === null) throw error;
      status = exitStatus;
    }
  }

  return { status, unwound };
}
