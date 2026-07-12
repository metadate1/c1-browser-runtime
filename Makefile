EMSDK_DIR ?= $(HOME)/.cache/emsdk
EMCC ?= $(EMSDK_DIR)/upstream/emscripten/emcc.py
EMSDK_PYTHON ?= $(firstword $(wildcard $(EMSDK_DIR)/python/*_64bit/bin/python3))
DIST_DIR := dist
OPT ?= -O2
HOST_UNAME := $(shell uname -s)
ifeq ($(HOST_UNAME),Darwin)
DEAD_CODE_FLAGS := -Wl,-dead_strip
else
DEAD_CODE_FLAGS := -Wl,--gc-sections
endif

C1_SOURCES := \
	engine/src/ns.c \
	engine/src/pad.c \
	engine/src/math.c \
	engine/src/audio.c \
	engine/src/card.c \
	engine/src/midi.c \
	engine/src/cam.c \
	engine/src/solid.c \
	engine/src/slst.c \
	engine/src/level.c \
	engine/src/misc.c \
	engine/src/gfx.c \
	engine/src/gool.c \
	engine/src/pbak.c \
	engine/src/title.c \
	engine/src/main.c \
	engine/src/pc/init.c \
	engine/src/pc/math.c \
	engine/src/pc/pad.c \
	engine/src/pc/time.c \
	engine/src/pc/gfx/tex.c \
	engine/src/pc/gfx/soft.c \
	engine/src/pc/gfx/gl.c \
	engine/src/pc/sound/util.c \
	engine/src/pc/sound/midi_tsf.c \
	engine/src/pc/sound/audio.c \
	engine/src/util/list.c \
	engine/src/util/tree.c

CFLAGS := \
	-Iengine/src \
	-std=gnu11 \
	$(OPT) \
	-gsource-map \
	-fwrapv \
	-fno-strict-aliasing \
	-DC1_BROWSER \
	-Wno-pointer-to-int-cast \
	-Wno-int-to-pointer-cast \
	-Wno-shift-op-parentheses

EMFLAGS := \
	-sUSE_SDL=2 \
	-sLEGACY_GL_EMULATION=1 \
	-sGL_FFP_ONLY=1 \
	-sGL_PREINITIALIZED_CONTEXT=1 \
	-sGL_UNSAFE_OPTS=0 \
	-sMIN_WEBGL_VERSION=1 \
	-sMAX_WEBGL_VERSION=2 \
	-sALLOW_MEMORY_GROWTH=0 \
	-sINITIAL_MEMORY=134217728 \
	-sABORTING_MALLOC=0 \
	-sSTACK_SIZE=8388608 \
	-sMODULARIZE=1 \
	-sEXPORT_ES6=1 \
	-sINVOKE_RUN=0 \
	-sEXIT_RUNTIME=0 \
	-sENVIRONMENT=web \
	-sASSERTIONS=2 \
	-sINCOMING_MODULE_JS_API=canvas,preinitializedWebGLContext,noInitialRun,locateFile,print,printErr,onAbort,GL_MAX_TEXTURE_IMAGE_UNITS \
	-sEXPORTED_FUNCTIONS=_main,_C1SetVirtualPad,_C1SetAudioPaused,_C1SetDebugFramePaused,_C1GetAudioCallbackCount,_C1GetAudioPeak,_C1GetAudioClipCount,_C1GetAudioDeadlineMissCount,_C1GetAudioMaxGapUs,_C1GetAudioMaxCallbackUs,_C1GetAudioMusicPeak,_C1GetAudioSfxPeak,_C1GetAudioMusicRms,_C1GetAudioSfxRms,_C1GetAudioActiveSfx,_C1GetAudioDelayedVoiceCount,_C1GetSampleCacheHits,_C1GetSampleCacheMisses,_C1GetSampleCacheBytes,_C1GetTextureOwnedCount,_C1GetTextureOwnedBytes,_C1GetTextureFrameRequests,_C1GetTextureFrameHits,_C1GetTextureFrameMisses,_C1GetTextureFrameFailures,_C1GetTextureFrameMissingPages,_C1GetTextureFrameGenerationMisses,_C1GetTextureFrameCacheFailures,_C1GetTextureFramePageChanges,_C1GetTextureTotalMisses,_C1GetTextureTotalFailures,_C1GetTextureTotalPageChanges,_C1GetTextureFrameUploadBytes,_C1GetTextureTotalUploadBytes,_C1GetPrimitiveBytes,_C1GetMaxPrimitiveBytes,_C1GetPrimitiveOverflowCount,_C1GetWorldPrimitiveCount,_C1GetConvertedTriangleCount,_C1GetLargestTriangleArea2,_C1GetLargestTriangleIndex,_C1GetLargestTriangleX,_C1GetLargestTriangleY,_C1GetLargestTriangleTexid,_C1GetLargestTriangleFlags,_C1GetLargestTriangleType,_C1GetOutsideTriangleCount,_C1GetVisiblePolyCount,_C1GetGlError,_C1GetGlErrorCount,_C1GetDrawSkipCounter,_C1DebugCrashEvent,_C1GetGameState,_C1GetLifeCount,_C1GetDeathCount,_C1GetRespawnCount,_C1GetDisplayFlags,_C1GetNextDisplayFlags,_C1GetFadeCounter,_C1GetVramFillRgb,_C1GetNextVramFillRgb,_C1GetCamX,_C1GetCamY,_C1GetCamZ,_C1GetCamRotX,_C1GetCamRotY,_C1GetDeathCamOrbit,_C1GetDeathCamFlipVelocity,_C1GetLastFrameUs,_C1GetMaxFrameUs,_C1GetHeapSize,_C1GetHeapAllocatedEnd,_C1CardControl,_C1GetCardPartCount,_C1GetCardFlags,_C1FlushBrowserResume,_C1GetBrowserResumeResult,_C1GetLevelCount,_C1GetKeyCount,_C1GetGemCount,_C1GetSfxVolume,_C1GetMusicVolume,_C1GetMono,_C1GetTitleState,_C1GetLoadedTitleState,_C1GetCurrentLid,_C1GetTitleTransitionState \
	-sEXPORTED_RUNTIME_METHODS=FS,callMain

.PHONY: all web test test-tree clean serve setup

all: web

web:
	@test -f "$(EMCC)" || (echo "Emscripten is missing. Run: npm run setup" && exit 1)
	@test -x "$(EMSDK_PYTHON)" || (echo "Emscripten's Python runtime is missing. Run: npm run setup" && exit 1)
	rm -rf "$(DIST_DIR)"
	mkdir -p "$(DIST_DIR)"
	cp -R web/. "$(DIST_DIR)/"
	"$(EMSDK_PYTHON)" "$(EMCC)" $(C1_SOURCES) $(CFLAGS) $(EMFLAGS) -o "$(DIST_DIR)/c1.mjs"

test: test-tree
	cc -std=gnu11 -Wall -Wextra -Iengine/src engine/tests/main_transition_test.c -o /tmp/c1-main-transition-test
	/tmp/c1-main-transition-test
	cc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections -Iengine/src engine/tests/audio_delay_test.c $(DEAD_CODE_FLAGS) -o /tmp/c1-audio-delay-test
	ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/c1-audio-delay-test
	cc -std=gnu11 -Wall -Wextra -Iengine/src engine/tests/card_test.c engine/src/card.c -o /tmp/c1-card-test
	/tmp/c1-card-test
	cc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -ffunction-sections -fdata-sections -Iengine/src engine/tests/ns_texture_lifecycle_test.c engine/src/ns.c $(DEAD_CODE_FLAGS) -o /tmp/c1-ns-texture-lifecycle-test
	/tmp/c1-ns-texture-lifecycle-test
	cc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function -ffunction-sections -fdata-sections -Iengine/src engine/tests/ns_metadata_test.c engine/src/ns.c $(DEAD_CODE_FLAGS) -o /tmp/c1-ns-metadata-test
	/tmp/c1-ns-metadata-test
	cc -std=gnu11 -Wall -Wextra -Iengine/src engine/tests/level_ref_test.c -o /tmp/c1-level-ref-test
	/tmp/c1-level-ref-test
	cc -std=gnu11 -Wall -Wextra -Iengine/src engine/tests/gool_null_gop_test.c -o /tmp/c1-gool-null-gop-test
	/tmp/c1-gool-null-gop-test
	cc -std=gnu11 -Wall -Wextra -ffunction-sections -fdata-sections -Iengine/src engine/tests/cam_death_test.c engine/src/cam.c $(DEAD_CODE_FLAGS) -o /tmp/c1-cam-death-test
	/tmp/c1-cam-death-test
	cc -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Iengine/src engine/tests/texture_cache_test.c engine/src/pc/gfx/tex.c -o /tmp/c1-texture-cache-test
	ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/c1-texture-cache-test
	cc -std=gnu11 -Wall -Wextra -Iengine/src engine/tests/frame_skip_test.c -o /tmp/c1-frame-skip-test
	/tmp/c1-frame-skip-test
	cc -std=gnu11 -Wall -Wextra -Iengine/src engine/tests/primitive_alpha_test.c -o /tmp/c1-primitive-alpha-test
	/tmp/c1-primitive-alpha-test
	cc -std=gnu11 -Wall -Wextra -Iengine/src engine/tests/shader_ramp_test.c -o /tmp/c1-shader-ramp-test
	/tmp/c1-shader-ramp-test
	cc -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-but-set-variable -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections -Iengine/src engine/tests/solid_ceil_test.c engine/src/solid.c $(DEAD_CODE_FLAGS) -o /tmp/c1-solid-ceil-test
	ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/c1-solid-ceil-test
	cc -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Iengine/src engine/tests/adpcm_test.c engine/src/pc/sound/util.c -o /tmp/c1-adpcm-test
	ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/c1-adpcm-test
	cc -std=gnu11 -Wall -Wextra -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-unused-function -Wno-shift-op-parentheses -fsanitize=undefined -fno-sanitize-recover=undefined -ffunction-sections -fdata-sections -Iengine/src engine/tests/projection_test.c engine/src/pc/gfx/soft.c engine/src/pc/math.c $(DEAD_CODE_FLAGS) -o /tmp/c1-projection-test
	/tmp/c1-projection-test
	node engine/tests/runtime_lifecycle_test.mjs
	node engine/tests/browser/audio-regression_test.mjs

test-tree:
	cc -std=gnu11 -Wall -Wextra -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-deprecated-declarations -Wno-pointer-to-int-cast -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer -Iengine/src engine/tests/tree_changes_test.c engine/src/util/tree.c engine/src/util/list.c -o /tmp/c1-tree-changes-test
	ASAN_OPTIONS=halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 /tmp/c1-tree-changes-test

clean:
	rm -rf "$(DIST_DIR)"
	rm -f /tmp/c1-main-transition-test /tmp/c1-audio-delay-test /tmp/c1-card-test /tmp/c1-level-ref-test /tmp/c1-gool-null-gop-test \
		/tmp/c1-ns-texture-lifecycle-test /tmp/c1-ns-metadata-test \
		/tmp/c1-cam-death-test /tmp/c1-texture-cache-test /tmp/c1-frame-skip-test \
		/tmp/c1-primitive-alpha-test \
		/tmp/c1-shader-ramp-test /tmp/c1-solid-ceil-test /tmp/c1-adpcm-test \
		/tmp/c1-projection-test /tmp/c1-tree-changes-test

serve:
	node scripts/serve.mjs

setup:
	./scripts/setup-emsdk.sh
