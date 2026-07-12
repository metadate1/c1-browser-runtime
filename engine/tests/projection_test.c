#include <assert.h>

#include "pc/gfx/soft.h"

int main(void) {
  mat16 identity = { .m = { { 0x1000, 0, 0 },
                            { 0, 0x1000, 0 },
                            { 0, 0, 0x1000 } } };
  mat16 rolling_stones_camera = {
    .m = { { 4029, 0, -737 },
           { -370, 3545, -2019 },
           { 637, 2052, 3487 } }
  };
  tgeo_header pill_header = {
    .scale_x = 7200,
    .scale_y = 7200,
    .scale_z = 7200,
  };
  gool_vectors pill_vectors = {
    .scale = { .x = 0x1000, .y = 0x1000, .z = 0x1000 },
  };
  sw_transform_struct object_params = { 0 };
  sw_transform_struct sprite_params = { 0 };
  gool_vectors sprite_vectors = {
    .scale = { .x = 0x1000, .y = 0x0FFF, .z = 0x1000 },
  };
  gool_vectors camera_vectors = { 0 };
  vec zero = { 0 };
  vec2 screen = { 0 };
  vec input, output;

  /* Pi10V in Rolling Stones uses a 7200 TGEO scale.  With the 0E_lZ
   * camera, the pre-aspect Y row is {-651, 6231, -3550}.  Retail
   * 0x80039BD4 computes -((coefficient * 5) >> 3); dividing first moves
   * visible peg vertices one screen pixel downward. */
  SwCalcObjectRotMatrix(&rolling_stones_camera, &pill_header, &pill_vectors,
    &object_params);
  assert(object_params.m_rot.m[1][0] == 407);
  assert(object_params.m_rot.m[1][1] == -3894);
  assert(object_params.m_rot.m[1][2] == 2219);
  input = (vec){ .x = -48, .y = 136, .z = -24 };
  zero = (vec){ .x = -1129, .y = -2456, .z = 6160 };
  assert(SwRotTransPers(&input, &output, &zero, &object_params.m_rot,
    &screen, 800));
  assert(output.x == -159 && output.y == -343 && output.z == 6089);

  /* Sprite matrices use the same retail multiply-before-shift order.  A
   * coefficient with nonzero low bits distinguishes it from the old
   * divide-first path (-2555), which caused one-pixel sprite jitter. */
  assert(SwCalcSpriteRotMatrix(&sprite_vectors, &camera_vectors, 1, 0,
    &identity, 1000, 12000, &sprite_params));
  assert(sprite_params.m_rot.m[1][1] == -2559);

  /* Normal projection retains the port's fixed-point rounding. */
  zero = (vec){ 0 };
  input = (vec){ .x = -333, .y = 125, .z = 1000 };
  assert(SwRotTransPers(&input, &output, &zero, &identity, &screen, 460));
  assert(output.x == -154 && output.y == 57 && output.z == 1000);

  /* At H/2 and behind the camera, the GTE quotient saturates instead of
   * dividing by zero or producing an unbounded screen triangle. Divide
   * overflow also sets the GTE summary flag, so object callers can reject it. */
  input = (vec){ .x = 100, .y = -100, .z = 230 };
  assert(!SwRotTransPers(&input, &output, &zero, &identity, &screen, 460));
  assert(output.x == 199 && output.y == -200 && output.z == 230);
  input.z = 231;
  assert(SwRotTransPers(&input, &output, &zero, &identity, &screen, 460));
  assert(output.x == 199 && output.y == -200 && output.z == 231);
  input.z = -100;
  assert(!SwRotTransPers(&input, &output, &zero, &identity, &screen, 460));
  assert(output.x == 199 && output.y == -200 && output.z == 0);

  input = (vec){ .x = 0x7FFF, .y = -0x8000, .z = 1 };
  assert(!SwRotTransPers(&input, &output, &zero, &identity, &screen, 460));
  assert(output.x == 0x3FF && output.y == -0x400);

  /* Translation MACs are evaluated in 64-bit before shifting. This covers
   * negative translations and values whose 12-bit fixed-point product would
   * overflow (and left-shift a negative signed integer) in int32_t. */
  input = (vec){ .x = 100, .y = -200, .z = 300 };
  zero = (vec){ .x = -1234567, .y = 1234567, .z = -400000 };
  assert(!SwRotTrans(&input, &output, &zero, &identity));
  assert(output.x == -1234467);
  assert(output.y == 1234367);
  assert(output.z == -399700);
  return 0;
}
