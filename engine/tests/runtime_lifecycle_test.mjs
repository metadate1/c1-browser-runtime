import assert from "node:assert/strict";

import {
  advanceRuntimeFrameGate,
  callMainOnce,
  isEmscriptenUnwind,
} from "../../web/runtime-lifecycle.js";

assert.deepEqual(callMainOnce({ callMain: () => undefined }), { status: 0, unwound: false });
assert.deepEqual(callMainOnce({ callMain: () => 0 }), { status: 0, unwound: false });
assert.deepEqual(callMainOnce({ callMain: () => 7 }), { status: 7, unwound: false });
assert.deepEqual(callMainOnce({ callMain: () => { throw "unwind"; } }), { status: 0, unwound: true });
assert.deepEqual(callMainOnce({ callMain: () => { throw new Error("unwind"); } }), { status: 0, unwound: true });

const exitStatus = new Error("Program terminated with exit(3)");
exitStatus.status = 3;
assert.deepEqual(callMainOnce({ callMain: () => { throw exitStatus; } }), { status: 3, unwound: false });

assert.equal(isEmscriptenUnwind("unwind"), true);
assert.equal(isEmscriptenUnwind("cannot unwind runtime"), false);
assert.throws(() => callMainOnce({ callMain: () => { throw new Error("cannot unwind runtime"); } }), /cannot unwind runtime/);
assert.throws(() => callMainOnce({ callMain: () => 1.5 }), /invalid status/);
assert.throws(() => callMainOnce({}), /does not expose callMain/);

assert.equal(advanceRuntimeFrameGate("idle", "frame-sample"), "idle");
assert.equal(advanceRuntimeFrameGate("idle", "main-ready"), "awaiting-frame");
assert.equal(advanceRuntimeFrameGate("awaiting-frame", "frame-sample"), "pass");
assert.equal(advanceRuntimeFrameGate("pass", "failure"), "fail");
assert.equal(advanceRuntimeFrameGate("awaiting-frame", "failure"), "fail");
assert.equal(advanceRuntimeFrameGate("fail", "frame-sample"), "fail");
assert.throws(() => advanceRuntimeFrameGate("unknown", "main-ready"), /Unknown runtime frame gate state/);
assert.throws(() => advanceRuntimeFrameGate("idle", "unknown"), /Unknown runtime frame gate event/);
