EMSDK_DIR ?= $(HOME)/.cache/emsdk
EMCC ?= $(EMSDK_DIR)/upstream/emscripten/emcc.py
EMSDK_PYTHON ?= $(firstword $(wildcard $(EMSDK_DIR)/python/*_64bit/bin/python3))
DIST_DIR := dist
OPT ?= -O2

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
	-sEXPORTED_FUNCTIONS=_main,_C1SetVirtualPad,_C1SetAudioPaused,_C1GetAudioCallbackCount,_C1GetAudioPeak,_C1GetAudioClipCount,_C1GetAudioDeadlineMissCount,_C1GetAudioMaxGapUs,_C1GetAudioMaxCallbackUs,_C1GetAudioMusicPeak,_C1GetAudioSfxPeak,_C1GetAudioMusicRms,_C1GetAudioSfxRms,_C1GetAudioActiveSfx,_C1GetSampleCacheHits,_C1GetSampleCacheMisses,_C1GetSampleCacheBytes,_C1GetTextureOwnedCount,_C1GetTextureOwnedBytes,_C1GetLastFrameUs,_C1GetMaxFrameUs,_C1GetHeapSize,_C1GetHeapAllocatedEnd,_C1CardControl,_C1GetCardPartCount,_C1GetCardFlags,_C1FlushBrowserResume,_C1GetBrowserResumeResult,_C1GetLevelCount,_C1GetKeyCount,_C1GetGemCount,_C1GetSfxVolume,_C1GetMusicVolume,_C1GetMono,_C1GetTitleState,_C1GetLoadedTitleState,_C1GetCurrentLid,_C1GetTitleTransitionState \
	-sEXPORTED_RUNTIME_METHODS=FS,callMain

.PHONY: all web clean serve setup

all: web

web:
	@test -f "$(EMCC)" || (echo "Emscripten is missing. Run: npm run setup" && exit 1)
	@test -x "$(EMSDK_PYTHON)" || (echo "Emscripten's Python runtime is missing. Run: npm run setup" && exit 1)
	rm -rf "$(DIST_DIR)"
	mkdir -p "$(DIST_DIR)"
	cp -R web/. "$(DIST_DIR)/"
	"$(EMSDK_PYTHON)" "$(EMCC)" $(C1_SOURCES) $(CFLAGS) $(EMFLAGS) -o "$(DIST_DIR)/c1.mjs"

clean:
	rm -rf "$(DIST_DIR)"

serve:
	node scripts/serve.mjs

setup:
	./scripts/setup-emsdk.sh
