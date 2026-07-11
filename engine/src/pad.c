#include "pad.h"
#include "globals.h"
#include "gool.h"
#include "pbak.h"

pad pads[2];
int pad_count;
int pad_lock;

extern pbak_header *cur_pbak_header;
extern pbak_frame *cur_pbak_frame;

#include "pc/pad.h"
#include "pc/gfx/gl.h"
extern gl_context context;

//----- (80016718) --------------------------------------------------------
void PadInit(int count) {
  pad *pad;
  int i;

  pad_count = count;
  for (i=0;i<pad_count;i++) {
    pad = &pads[i];
    pad->tapped = 0;
    pad->held = 0;
    pad->held_prev = 0;
    pad->held_prev2 = 0;
  }
}

static inline void PadUpdatePbak(pad *pad) {
  uint32_t held, arg;
  int count, finished;

  if (pad_lock == 2) {
    count = cur_pbak_header ? cur_pbak_header->frame_count : 0;
    if (!cur_pbak_header || count <= 0 || !cur_pbak_frame) {
      pad_lock = 0;
      pbak_state = 0;
      return;
    }
    if (cur_pbak_frame == &cur_pbak_header->frames[0]) {
      context.ticks_per_frame = cur_pbak_header->ticks_per_frame;
    }
    held = pad->held;
    pad->held = cur_pbak_frame->held;
    finished = cur_pbak_frame + 1 >= &cur_pbak_header->frames[count];
    if (!finished)
      ++cur_pbak_frame;
    if (held & 0x9F0 || finished) { /* X, [], /\, O, start, or select pressed? */
      pbak_state = 0;
      if (island_cam_rot_x) {
        arg = 0;
        GoolSendEvent(0, caption_obj, 0xE00, 1, &arg);
        pad_lock = 3;
      }
      else
        pad_lock = 0;
    }
  }
  else if (pad_lock == 3)
    pad->held = 0;
}

//----- (800167A4) --------------------------------------------------------
void PadUpdate() {
  pad *pad;
  uint32_t held;
  int i;

  for (i=0;i<pad_count;i++) {
    pad = &pads[i];
    pad->held_prev2 = pad->held_prev;
    pad->tapped_prev = pad->tapped;
    pad->held_prev = pad->held;
    held = SwPadRead(i);
    if (i == 0) { held >>= 16; }
    else        { held &= 0xFFFF; }
    pad->held = held;
    if (pad->held & 0x1000) /* up is held? */
      pad->held &= 0xBFFF; /* ignore holding down */
    if (pad->held & 0x8000) /* left is held? */
      pad->held &= 0xDFFF; /* ignore holding right */
    if (i == 0)
      PadUpdatePbak(pad);
    pad->tapped = (~pad->held_prev & pad->held) & 0xF9FF; /* select bits for buttons not held in previous frame (except L3 & R3) */
  }
}
