#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cam.h"
#include "globals.h"
#include "level.h"
#include "pad.h"
#include "formats/svtx.h"
#include "formats/tgeo.h"
#include "formats/zdat.h"

gool_globals globals;
ns_struct ns;
level_state savestate;
gool_vectors cam;
int frames_elapsed;
entry *cur_zone;
zone_path *cur_path;
int32_t cur_progress;
pad pads[2];
gool_object *crash;

static eid_t svtx_eid = 0x101;
static eid_t tgeo_eid = 0x103;
static struct { entry base; uint8_t *items[1]; } svtx_entry;
static struct { entry base; uint8_t *items[1]; } tgeo_entry;
static vec observed_scale;

entry *NSLookup(void *ref) {
  eid_t eid = *(eid_t*)ref;
  if (eid == svtx_eid) return &svtx_entry.base;
  if (eid == tgeo_eid) return &tgeo_entry.base;
  return 0;
}

int32_t msqrt(int32_t value) { return value > 0 ? 100 : 0; }
int32_t matan2(int32_t y, int32_t x) { (void)y; (void)x; return 0; }
int16_t GoolAngDiff(int16_t a, int16_t b) { (void)a; (void)b; return 200; }
int16_t GoolObjectRotate(int16_t a, int16_t b, int32_t speed, gool_object *obj) {
  (void)b;
  (void)obj;
  return a + speed;
}
int32_t GoolSeek(int32_t a, int32_t b, int32_t delta) {
  if (a < b) return a + delta < b ? a + delta : b;
  return a - delta > b ? a - delta : b;
}
void GoolTransform(vec *in, vec *trans, ang *rot, vec *scale, vec *out) {
  (void)rot;
  if (scale) {
    observed_scale = *scale;
    out->x = trans->x + (int32_t)(((int64_t)in->x * scale->x) >> 12);
    out->y = trans->y + (int32_t)(((int64_t)in->y * scale->y) >> 12);
    out->z = trans->z + (int32_t)(((int64_t)in->z * scale->z) >> 12);
  }
  else {
    out->x = trans->x + in->x;
    out->y = trans->y + in->y;
    out->z = trans->z + in->z;
  }
}

extern int32_t dcam_accel, dcam_angvel2, dcam_rot_y1;
extern int CamSelectIslandNeighbor(zone_path *path, int state);

static void TestIslandNeighborSelection(void) {
  zone_path path;

  memset(&path, 0, sizeof(path));
  path.neighbor_path_count = 3;
  path.neighbor_paths[0].goal = 6;
  path.neighbor_paths[1].goal = 1;
  path.neighbor_paths[2].goal = 5;

  /* An exact goal supersedes an earlier direction-compatible candidate. */
  assert(CamSelectIslandNeighbor(&path, 5) == 2);

  /* With no exact/direction match, retail takes the first goal without bit 4. */
  path.neighbor_paths[0].goal = 6;
  path.neighbor_paths[1].goal = 2;
  path.neighbor_paths[2].goal = 7;
  assert(CamSelectIslandNeighbor(&path, 1) == 1);

  path.neighbor_path_count = 2;
  path.neighbor_paths[0].goal = 4;
  path.neighbor_paths[1].goal = 4;
  assert(CamSelectIslandNeighbor(&path, 2) == -1);
}

int main(void) {
  gool_object object;
  gool_anim animation;
  uint8_t frame_bytes[sizeof(svtx_frame) + sizeof(svtx_vertex)];
  svtx_frame *frame = (svtx_frame*)frame_bytes;
  tgeo_header header;
  int count = 0;

  TestIslandNeighborSelection();

  memset(&globals, 0, sizeof(globals));
  memset(&object, 0, sizeof(object));
  memset(&animation, 0, sizeof(animation));
  memset(frame_bytes, 0, sizeof(frame_bytes));
  memset(&header, 0, sizeof(header));
  memset(&svtx_entry, 0, sizeof(svtx_entry));
  memset(&tgeo_entry, 0, sizeof(tgeo_entry));

  animation.eid = svtx_eid;
  object.process.anim_seq = &animation;
  object.process.vectors.scale.x = 0x2000;
  object.process.vectors.scale.y = 0x3000;
  object.process.vectors.scale.z = 0x4000;
  object.process.vectors.trans.x = 1000;
  object.process.vectors.trans.y = 2000;
  object.process.vectors.trans.z = 3000;
  frame->tgeo = tgeo_eid;
  frame->vertices[0].x = 129;
  frame->vertices[0].y = 129;
  frame->vertices[0].z = 129;
  header.scale_x = 0x1000;
  header.scale_y = 0x1800;
  header.scale_z = 0x0800;
  svtx_entry.items[0] = (uint8_t*)frame;
  tgeo_entry.items[0] = (uint8_t*)&header;

  cam_spin_obj = &object;
  cam_spin_obj_vert = 0;
  dcam_flip_speed = 100;
  dcam_zoom_speed = 1000;
  cur_display_flags = GOOL_FLAG_SPIN_ACCEL;
  cam.trans.x = 4000;
  cam.trans.y = 5000;
  cam.trans.z = 6000;

  assert(CamDeath(&count) == SUCCESS);
  assert(count == 1);
  assert(observed_scale.x == 0x2000);
  assert(observed_scale.y == 0x4800);
  assert(observed_scale.z == 0x2000);
  assert(dcam_accel == 22);
  assert(dcam_rot_y1 == 22);
  assert(dcam_angvel2 == 100);
  assert(abs(cam.trans.x) < 10000000);
  assert(abs(cam.trans.y) < 10000000);
  assert(abs(cam.trans.z) < 10000000);
  return 0;
}
