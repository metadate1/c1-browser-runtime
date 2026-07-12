#include "common.h"
#include "globals.h"
#include "ns.h"
#include "pad.h"
#include "gfx.h"
#include "misc.h"
#include "gool.h"
#include "level.h"
#include "slst.h"
#include "solid.h"
#include "pbak.h"
#include "audio.h"
#include "midi.h"
#include "title.h"
#include "card.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/heap.h>
extern lid_t cur_lid;
extern title_struct *title;

EMSCRIPTEN_KEEPALIVE int C1GetTitleState(void) {
  return title_state;
}

EMSCRIPTEN_KEEPALIVE int C1GetLoadedTitleState(void) {
  return title ? title->state : -1;
}

EMSCRIPTEN_KEEPALIVE int C1GetCurrentLid(void) {
  return cur_lid;
}

EMSCRIPTEN_KEEPALIVE int C1GetTitleTransitionState(void) {
  return title ? title->transition_state : -1;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetHeapSize(void) {
  return (uint32_t)emscripten_get_heap_size();
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetHeapAllocatedEnd(void) {
  return (uint32_t)*emscripten_get_sbrk_ptr();
}

#endif

#include "pc/init.h"
#include "pc/time.h"
#include "pc/gfx/gl.h"
#include "pc/gfx/soft.h" // for ext only
#include "pc/gfx/tex.h"

/* .data */
const ns_subsystem subsys[21] = {
  { .name = "NONE", .init =       GLSetupPrims, .init2 =                  0, .on_load =          0, .unused = 0, .kill =             GLKill },
  { .name = "SVTX", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "TGEO", .init =                  0, .init2 =                  0, .on_load = TgeoOnLoad, .unused = 0, .kill =                  0 },
  { .name = "WGEO", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "SLST", .init =           SlstInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =           SlstKill },
  { .name = "TPAG", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "LDAT", .init =                  0, .init2 =           LdatInit, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "ZDAT", .init =                  0, .init2 =                  0, .on_load = ZdatOnLoad, .unused = 0, .kill =                  0 },
  { .name = "CPAT", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "BINF", .init =           BinfInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =           BinfKill },
  { .name = "OPAT", .init = GoolInitAllocTable, .init2 =        GoolInitLid, .on_load =          0, .unused = 0, .kill = GoolKillAllocTable },
  { .name = "GOOL", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "ADIO", .init =          AudioInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =          AudioKill },
  { .name = "MIDI", .init =           MidiInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =           MidiKill },
  { .name = "INST", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "IMAG", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "LINK", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "MDAT", .init =          TitleInit, .init2 = TitleLoadNextState, .on_load = MdatOnLoad, .unused = 0, .kill =          TitleKill },
  { .name = "IPAL", .init =                  0, .init2 =                  0, .on_load =          0, .unused = 0, .kill =                  0 },
  { .name = "PBAK", .init =           PbakInit, .init2 =                  0, .on_load =          0, .unused = 0, .kill =           PbakKill }
};
/* .sdata */
int wgeom_disabled = 0;         /* 800563FC; gp[0x0]  */
int paused = 0;                 /* 80056400; gp[0x1]  */
int pause_status = 0;           /* 8005640C; gp[0x4]  */
int use_cd = 1;                 /* 80056410; gp[0x5]  */
int done = 0;                   /* 80056428; gp[0xB]  */
/* .sbss */
uint32_t pause_stamp;           /* 800565B8; gp[0x6F] */
uint32_t pause_draw_stamp;      /* 800565BC; gp[0x70] */

extern ns_struct ns;
extern pad pads[2];
extern lid_t cur_lid, next_lid;
extern entry *cur_zone;
extern gool_object *crash;
extern eid_t crash_eid;
extern gool_handle handles[8];
extern int bonus_return;
extern level_state savestate;
extern rgb8 vram_fill_color, next_vram_fill_color;
extern gool_vectors cam;
extern int32_t dcam_rot_y1, dcam_angvel2;

#ifdef CFLAGS_DRAW_EXTENSIONS
extern zone_query cur_zone_query;
extern uint32_t *wall_bitmap;
extern gool_bound object_bounds[96];
extern int object_bound_count;
int draw_octrees = 0;
int draw_wallmap = 0;
int draw_objbounds = 0;
#endif

extern gl_context context;

void CoreLoop(lid_t lid);
void CoreFrame(void);

#ifdef __EMSCRIPTEN__
static double browser_next_frame_ms;
static uint32_t browser_last_frame_us;
static uint32_t browser_max_frame_us;
static int browser_debug_frame_paused;

EMSCRIPTEN_KEEPALIVE uint32_t C1GetLastFrameUs(void) {
  return browser_last_frame_us;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetMaxFrameUs(void) {
  return browser_max_frame_us;
}

EMSCRIPTEN_KEEPALIVE void C1SetDebugFramePaused(int paused) {
  browser_debug_frame_paused = paused != 0;
}

EMSCRIPTEN_KEEPALIVE int C1DebugCrashEvent(int event) {
  uint32_t arg = 0x6400;
  if (!crash) return 0;
  return GoolSendEvent(0, crash, event, 1, &arg);
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetGameState(void) { return game_state; }
EMSCRIPTEN_KEEPALIVE int C1GetLifeCount(void) { return life_count; }
EMSCRIPTEN_KEEPALIVE int C1GetDeathCount(void) { return death_count; }
EMSCRIPTEN_KEEPALIVE int C1GetRespawnCount(void) { return respawn_count; }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetDisplayFlags(void) { return cur_display_flags; }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetNextDisplayFlags(void) { return next_display_flags; }
EMSCRIPTEN_KEEPALIVE int C1GetFadeCounter(void) { return fade_counter; }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetVramFillRgb(void) {
  return ((uint32_t)vram_fill_color.r << 16)
       | ((uint32_t)vram_fill_color.g << 8)
       | vram_fill_color.b;
}
EMSCRIPTEN_KEEPALIVE uint32_t C1GetNextVramFillRgb(void) {
  return ((uint32_t)next_vram_fill_color.r << 16)
       | ((uint32_t)next_vram_fill_color.g << 8)
       | next_vram_fill_color.b;
}
EMSCRIPTEN_KEEPALIVE int C1GetCamX(void) { return cam.trans.x; }
EMSCRIPTEN_KEEPALIVE int C1GetCamY(void) { return cam.trans.y; }
EMSCRIPTEN_KEEPALIVE int C1GetCamZ(void) { return cam.trans.z; }
EMSCRIPTEN_KEEPALIVE int C1GetCamRotX(void) { return cam.rot.x; }
EMSCRIPTEN_KEEPALIVE int C1GetCamRotY(void) { return cam.rot.y; }
EMSCRIPTEN_KEEPALIVE int C1GetDeathCamOrbit(void) { return dcam_rot_y1; }
EMSCRIPTEN_KEEPALIVE int C1GetDeathCamFlipVelocity(void) { return dcam_angvel2; }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFrameRequests(void) { return TextureFrameRequestCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFrameHits(void) { return TextureFrameHitCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFrameMisses(void) { return TextureFrameMissCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFrameFailures(void) { return TextureFrameFailureCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFrameMissingPages(void) { return TextureFrameMissingPageCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFrameGenerationMisses(void) { return TextureFrameGenerationMissCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFrameCacheFailures(void) { return TextureFrameCacheFailureCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFramePageChanges(void) { return TextureFramePageChangeCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureTotalMisses(void) { return TextureTotalMissCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureTotalFailures(void) { return TextureTotalFailureCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureTotalPageChanges(void) { return TextureTotalPageChangeCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureFrameUploadBytes(void) { return TextureFrameUploadBytes(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureTotalUploadBytes(void) { return TextureTotalUploadBytes(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetPrimitiveBytes(void) { return GLPrimitiveBytes(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetMaxPrimitiveBytes(void) { return GLMaxPrimitiveBytes(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetPrimitiveOverflowCount(void) { return GLPrimitiveOverflowCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetWorldPrimitiveCount(void) { return SwWorldPrimitiveCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetConvertedTriangleCount(void) { return GLConvertedTriangleCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetLargestTriangleArea2(void) { return GLFrameLargestTriangleArea2(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetLargestTriangleIndex(void) { return GLFrameLargestTriangleIndex(); }
EMSCRIPTEN_KEEPALIVE int32_t C1GetLargestTriangleX(int vertex) { return GLFrameLargestTriangleX(vertex); }
EMSCRIPTEN_KEEPALIVE int32_t C1GetLargestTriangleY(int vertex) { return GLFrameLargestTriangleY(vertex); }
EMSCRIPTEN_KEEPALIVE int C1GetLargestTriangleTexid(void) { return GLFrameLargestTriangleTexid(); }
EMSCRIPTEN_KEEPALIVE int C1GetLargestTriangleFlags(void) { return GLFrameLargestTriangleFlags(); }
EMSCRIPTEN_KEEPALIVE int C1GetLargestTriangleType(void) { return GLFrameLargestTriangleType(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetOutsideTriangleCount(void) { return GLFrameOutsideTriangleCount(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetVisiblePolyCount(void) {
  return cur_poly_ids ? (uint32_t)cur_poly_ids->len : 0;
}
EMSCRIPTEN_KEEPALIVE uint32_t C1GetGlError(void) { return GLLastError(); }
EMSCRIPTEN_KEEPALIVE uint32_t C1GetGlErrorCount(void) { return GLTotalErrorCount(); }
EMSCRIPTEN_KEEPALIVE int C1GetDrawSkipCounter(void) { return ns.draw_skip_counter; }

static void CoreBrowserFrame(void) {
  const double frame_ms = 1000.0 / 30.0;
  double start, now = emscripten_get_now();

  if (browser_debug_frame_paused) {
    return;
  }
  if (browser_next_frame_ms == 0.0)
    browser_next_frame_ms = now;
  if (now + 0.25 < browser_next_frame_ms) {
    return;
  }
  start = now;
  CoreFrame();
  browser_last_frame_us = (uint32_t)((emscripten_get_now() - start) * 1000.0);
  if (browser_last_frame_us > browser_max_frame_us)
    browser_max_frame_us = browser_last_frame_us;
  if (done) return;
  browser_next_frame_ms += frame_ms;
  if (now - browser_next_frame_ms > frame_ms * 2.0)
    browser_next_frame_ms = now + frame_ms;
}
#endif

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureOwnedCount(void) {
  return TextureOwnedCount();
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetTextureOwnedBytes(void) {
  return TextureOwnedBytes();
}
#endif

//----- (80011D88) --------------------------------------------------------
int main(int argc, char **argv) {
  lid_t boot_lid = LID_BOOTLEVEL;
  if (argc > 1) {
    char *end = 0;
    long parsed = strtol(argv[1], &end, 0);
    if (end != argv[1] && parsed > 0 && parsed <= LID_GAMEWIN)
      boot_lid = (lid_t)parsed;
  }
  use_cd = 0;
  if (init() != SUCCESS)
    return 1;
  CoreLoop(boot_lid);
#ifndef __EMSCRIPTEN__
  _kill();
#endif
  return done ? 1 : 0;
}

//----- (80011DD0) --------------------------------------------------------
void CoreObjectsCreate() {
  PadUpdate();
  pause_obj = 0;
  if (cur_lid == LID_TITLE) { /* title level? */
    NSOpen(&ns.ldat->exec_map[4], 0, 1);
    NSOpen(&ns.ldat->exec_map[52], 0, 1);
  }
  else if (cur_lid == LID_LEVELEND) { /* level completion screen? */
    NSOpen(&ns.ldat->exec_map[29], 0, 1);
    NSOpen(&ns.ldat->exec_map[30], 0, 1);
    NSOpen(&ns.ldat->exec_map[3], 0, 1);
  }
  else if (cur_lid != LID_INTRO && cur_lid != LID_GAMEWIN) { /* not intro or ending? */
    life_hud = GoolObjectCreate(&handles[1], 4, 0, 0, 0, 0);
    fruit_hud = GoolObjectCreate(&handles[1], 4, 1, 0, 0, 0);
    pickup_hud = GoolObjectCreate(&handles[1], 4, 5, 0, 0, 0);
    NSOpen(&ns.ldat->exec_map[0], 0, 1);
    NSOpen(&ns.ldat->exec_map[5], 0, 1);
    NSOpen(&ns.ldat->exec_map[29], 0, 1);
    if (cur_lid != LID_THEGREATHALL) /* not the great hall? */
      NSOpen(&ns.ldat->exec_map[34], 0, 1); /* load boxes code */
    NSOpen(&ns.ldat->exec_map[3], 0, 1);
    NSOpen(&ns.ldat->exec_map[4], 0, 1);
  }
  LevelInitMisc(1);
}

//----- (80011FC4) --------------------------------------------------------
void CoreFrame(void) {
  lid_t lid;
  int is_pause_lid, can_pause;
  zone_header *header;
  void *ot;
  uint32_t arg;
  int bonus_return2 = 0;

#ifdef __EMSCRIPTEN__
  if (done) {
    CardBrowserResumeFlush();
    cur_display_flags = 0;
    NSKill(&ns);
    _kill();
    emscripten_cancel_main_loop();
    return;
  }
#endif
  if (!ns.ldat) {
    done = 1;
    return;
  }

  CardUpdate();
  CardBrowserResumeUpdate();

    lid = ns.ldat->lid;
    is_pause_lid = lid != LID_TITLE && lid != LID_LEVELEND && lid != LID_INTRO;
    can_pause = (pbak_state == 0) && ((is_pause_lid && title_pause_state != -1) || title_pause_state > 0);
    if ((pads[0].tapped & 0x800) && can_pause) {
      paused = 1 - paused;
      if (!paused) {
        if (pause_obj) { /* pause screen object exists? */
          arg = 0;
          GoolSendEvent(0, pause_obj, 0xC00, 1, &arg); /* send resume/kill? event to pause screen object */
          pause_obj = 0;
          pause_status = -1;
          SetTicksElapsed(pause_stamp);
          context.draw_stamp = pause_draw_stamp;
        }
      }
      else if (!pause_obj) {
        pause_obj = GoolObjectCreate(&handles[7], 4, 4, 0, 0, 0);
        if (ISERRORCODE(pause_obj)) {
          pause_status = 0;
          paused = 0;
          pause_obj = 0;
        }
        else {
          pause_status = 1;
          pause_stamp = GetTicksElapsed();
          pause_draw_stamp = context.draw_stamp;
        }
      }
    }
    else { pause_status = 0; }
    if (crash && crash_eid != EID_NONE) /* crash exists and there is a pbak entry to play? */
      PbakPlay(&crash_eid);
    if (next_lid == -1 && lid != LID_TITLE
      && (game_state == GAME_STATE_GAMEOVER
       || game_state == GAME_STATE_CONTINUE
       || game_state == 0x400))
      next_lid = LID_TITLE;
    if (next_lid != -1) {
      GoolSendToColliders(0, GOOL_EVENT_LEVEL_END, 0, 0, 0);
      if (next_lid == -2) {
        lid = savestate.lid;
        bonus_return = 1; /* i.e. loading nsf and there is a savestate to load */
        bonus_return2 = 1; /* LdatInit clears bonus_return so we need a persistent variant */
      }
      else {
        lid = next_lid;
        bonus_return = 0;
        bonus_return2 = 0;
      }
      ns.draw_skip_counter = 2;
      NSKill(&ns);
      paused = 0;
      if (lid == LID_TITLE) {
        respawn_count = 0;
        death_count = 0;
        cortex_count = 0;
        brio_count = 0;
        tawna_count = 0;
        checkpoint_id = -1;
      }
      NSInit(&ns, lid);
      if (done || !ns.ldat)
        return;
      CoreObjectsCreate();
      if (bonus_return2) {
        next_lid = -2;
        LevelSpawnObjects();
        next_lid = -1;
        LevelRestart(&savestate);
      }
      bonus_return = 0;
    }
#ifdef PSX
    NSUpdate(-1);
#endif
    LevelSpawnObjects();
    if (!paused) {
      header = (zone_header*)cur_zone->items[0];
      if (header->gfx.flags & (ZONE_FLAG_DARK2 | ZONE_FLAG_LIGHTNING))
        ShaderParamsUpdate(0);
      /* if (!globals->paused) { ??? */
      if (header->gfx.flags & ZONE_FLAG_RIPPLE)
        ShaderParamsUpdateRipple(0);
      /* if (!globals->paused) ???*/
      CamUpdate();
    }
    /*
     * CamUpdate can cross a zone boundary and replace physical texture pages.
     * Snapshot page generations only after that work, immediately before any
     * world or object primitive asks the texture cache for UVs.
     */
    TexturesBeginFrame();
    GfxUpdateMatrices();
    ot = context.ot;
    header = (zone_header*)cur_zone->items[0];
    if ((cur_display_flags & GOOL_FLAG_DISPLAY_WORLDS) && header->world_count && !wgeom_disabled) {
      if (header->gfx.flags & ZONE_FLAG_DARK2)
        GfxTransformWorldsDark2(ot);
      else if ((header->gfx.flags & ZONE_FLAG_FOG_LIGHTNING) == ZONE_FLAG_FOG_LIGHTNING)
        GfxTransformWorldsDark(ot);
      else if (header->gfx.flags & ZONE_FLAG_FOG)
        GfxTransformWorldsFog(ot);
      else if (header->gfx.flags & ZONE_FLAG_RIPPLE)
        GfxTransformWorldsRipple(ot);
      else if (header->gfx.flags & ZONE_FLAG_LIGHTNING)
        GfxTransformWorldsLightning(ot);
      else
        GfxTransformWorlds(ot);
    }
    GoolUpdateObjects(!paused);
#if defined(CFLAGS_DRAW_EXTENSIONS) && !defined(PSX)
    if (pads[0].tapped & 4)
      draw_octrees = !draw_octrees;
    if (draw_octrees)
      SwTransformZoneQuery(&cur_zone_query, ot, GLGetPrimsTail());
    if (pads[0].tapped & 8)
      draw_wallmap = !draw_wallmap;
    if (draw_wallmap)
      SwDrawWallMap(wall_bitmap, ot, GLGetPrimsTail());
    if (pads[0].tapped & 1)
      draw_objbounds = !draw_objbounds;
    if (draw_objbounds)
      SwTransformObjectBounds(object_bounds, object_bound_count, ot, GLGetPrimsTail());
#endif
    GLBeginFrame();
    GLClear();
    if (ns.ldat->lid == LID_TITLE) { TitleUpdate(ot); }
    GLUpdate();
}

void CoreLoop(lid_t lid) {
  LevelInitGlobals();
  CardBrowserResumeLoad();
  NSInit(&ns, lid);
  if (done || !ns.ldat)
    return;
  CoreObjectsCreate();
#ifdef GOD_MODE
  levels_unlocked = 99;
  item_pool2 = (1 << 10) | (1 << 20);
#endif /* GOD_MODE */
#ifdef __EMSCRIPTEN__
  browser_next_frame_ms = 0.0;
  browser_debug_frame_paused = 0;
  browser_last_frame_us = browser_max_frame_us = 0;
  emscripten_set_main_loop(CoreBrowserFrame, 0, 1);
#else
  do {
    CoreFrame();
  } while (!done);
  cur_display_flags = 0;
  NSKill(&ns);
#endif
}
