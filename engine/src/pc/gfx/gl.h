#ifndef _GL_H_
#define _GL_H_

#include "common.h"
#include "geom.h"
#include "pcgfx.h"

typedef struct {
  uint8_t keys[512];
  vec2 mouse;
  int click;
  rect2 window;
  rect2 screen;
} gl_input;

typedef struct {
  void (*pre_update)();
  void (*post_update)();
  int (*ext_supported)(const char*);
  void *(*proc_addr)(const char*);
  void (*input)(gl_input*);
} gl_callbacks;

typedef struct {
  void *prims_head;
  void *prims_tail;
  uint32_t draw_stamp;
  uint32_t sync_stamp;
  uint32_t ticks_per_frame;
  void *ot[2048];
  vec2 draw_clip;
} gl_context;

extern int GLInit(gl_callbacks * _callbacks);
extern int GLKill();
extern int GLSetupPrims();
extern void GLResetPrims(gl_context *gc);
extern void GLDrawRect(int x, int y, int w, int h, int r, int g, int b);
extern void GLDrawOverlay(int brightness);
extern void GLDrawImage(dim2 *dim, uint8_t *buf, pnt2 *loc);
extern void **GLGetPrimsTail();
extern void *GLReservePrimitive(void **tail, size_t size);
extern uint32_t GLPrimitiveBytes(void);
extern uint32_t GLMaxPrimitiveBytes(void);
extern uint32_t GLPrimitiveOverflowCount(void);
extern uint32_t GLConvertedTriangleCount(void);
extern uint32_t GLFrameLargestTriangleArea2(void);
extern uint32_t GLFrameLargestTriangleIndex(void);
extern int32_t GLFrameLargestTriangleX(int vertex);
extern int32_t GLFrameLargestTriangleY(int vertex);
extern int GLFrameLargestTriangleTexid(void);
extern int GLFrameLargestTriangleFlags(void);
extern int GLFrameLargestTriangleType(void);
extern uint32_t GLFrameOutsideTriangleCount(void);
extern uint32_t GLLastError(void);
extern uint32_t GLTotalErrorCount(void);
extern void GLResetOT(void *ot, int len);
/* Textured alpha comes from the texture itself.  Only untextured PS1 blend
 * modes 0 and 1 need a vertex alpha; every other primitive must explicitly
 * restore opaque alpha instead of inheriting fixed-function color state. */
static inline uint32_t GLPrimitiveVertexAlpha(int texid, int flags) {
  if (texid == -1 && flags == 0) return 0x7FFFFFFFu;
  if (texid == -1 && flags == 1) return 0;
  return UINT32_MAX;
}
/* Freeze the original post-decrement skip decision before framebuffer writes. */
static inline int GLShouldRenderFrame(int draw_skip_counter) {
  return draw_skip_counter == 0 || draw_skip_counter == 1;
}
extern void GLBeginFrame(void);
extern void GLClear();
extern void GLUpdate();
#ifdef CFLAGS_DRAW_EXTENSIONS
typedef unsigned int GLuint;
extern int GLCreateTexture(dim2 dim, uint8_t *buf);
extern void GLDeleteTexture(GLuint texid);
extern void GLUpdateTexture(GLuint texid, rect2 rect, uint8_t *buf);
#endif

#endif /* _GL_H_ */
