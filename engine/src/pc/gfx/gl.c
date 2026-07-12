/*
 * opengl pc gfx backend
 */
#ifdef _WIN32
#include <Windows.h>
#define GLEW_STATIC
#include <GL/glew.h>
#else
#include <GL/gl.h>
#endif
#include "gl.h"
#include "pcgfx.h"
#include "globals.h"
#include "level.h"
#include "pbak.h"
#include "audio.h"
#include "pc/gfx/tex.h"
#include "pc/time.h"

#ifdef CFLAGS_GUI
#include "ext/gui.h"
#define CIMGUI_USE_OPENGL2
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"
#endif

int GLCreateTexture(dim2 dim, uint8_t *buf) {
  GLuint texid;

  glGenTextures(1, &texid);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texid);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dim.w, dim.h, 0,
    GL_RGBA, GL_UNSIGNED_BYTE, buf);
  glBindTexture(GL_TEXTURE_2D, 0);
  return texid;
}

void GLDeleteTexture(GLuint texid) {
  glDeleteTextures(1, &texid);
}

void GLUpdateTexture(GLuint texid, rect2 rect, uint8_t *buf) {
  glBindTexture(GL_TEXTURE_2D, texid);
  glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x, rect.y, rect.w, rect.h,
    GL_RGBA, GL_UNSIGNED_BYTE, buf);
  glBindTexture(GL_TEXTURE_2D, 0);
}

gl_context context = { 0 };
gl_callbacks callbacks = { 0 };
rect2 screen = { .x = -256, .y = -120, .w = 512, .h = 240 };
prim_struct prim_links[2048];
int image_texid = 0;
static dim2 image_tex_dim;
static uint32_t image_tex_hash;
static uint8_t *image_tex_pixels;
static size_t image_tex_size;
static void *trimem = 0;
static size_t trimem_capacity = 0;
static uint32_t last_primitive_bytes;
static uint32_t max_primitive_bytes;
static uint32_t primitive_overflow_count;
static uint32_t last_converted_triangle_count;
typedef struct {
  uint32_t area2;
  uint32_t index;
  int32_t x[3];
  int32_t y[3];
  int texid;
  int flags;
  int type;
  uint32_t outside_count;
} gl_triangle_frame_diag;
static gl_triangle_frame_diag triangle_frame_diag;
static uint32_t last_gl_error;
static uint32_t total_gl_error_count;
static int render_frame = 1;
static int direct_frame_written;

#define GL_PRIMITIVE_CAPACITY 0x800000u

extern ns_struct ns;
extern entry *cur_zone;
extern int paused;
extern int draw_count;
extern int rcnt_stopped;
extern uint32_t ticks_elapsed;
extern rgb8 vram_fill_color, next_vram_fill_color;
extern pbak_frame *cur_pbak_frame;

#ifdef CFLAGS_GUI
void GLInitGui() {
  GuiInit();
  ImGui_ImplOpenGL2_Init();
}

void GLKillGui() {
  ImGui_ImplOpenGL2_Shutdown();
}
#endif

int textures_inited=0;

/* gl */
int GLInit(gl_callbacks *_callbacks) {
  rect2 window;
  int i;

  callbacks = *_callbacks;
  last_primitive_bytes = 0;
  max_primitive_bytes = 0;
  primitive_overflow_count = 0;
  last_converted_triangle_count = 0;
  memset(&triangle_frame_diag, 0, sizeof(triangle_frame_diag));
  triangle_frame_diag.index = UINT32_MAX;
  last_gl_error = GL_NO_ERROR;
  total_gl_error_count = 0;
  render_frame = 1;
  direct_frame_written = 0;
#ifdef _WIN32
  glewInit();
#endif
#ifdef CFLAGS_GUI
  GLInitGui();
#endif
  TexturesInit(GLCreateTexture, GLDeleteTexture, GLUpdateTexture);
  textures_inited=1;
  screen.x = -256;
  screen.y = -120;
  screen.w = 512;
  screen.h = 240;
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  return SUCCESS;
}

int GLKill() {
  if (context.prims_head) {
    free(context.prims_head);
    context.prims_head = 0;
    context.prims_tail = 0;
  }
  if (trimem) {
    free(trimem);
    trimem = 0;
    trimem_capacity = 0;
  }
  if (textures_inited) {
    TexturesKill();
    textures_inited=0;
  }
  if (image_texid) {
    GLDeleteTexture(image_texid);
    image_texid = 0;
  }
  free(image_tex_pixels);
  image_tex_pixels = 0;
  image_tex_size = 0;
  memset(&image_tex_dim, 0, sizeof(image_tex_dim));
  image_tex_hash = 0;
#ifdef CFLAGS_GUI
  // GLKillGui(); /* removed for now */
#endif
  return SUCCESS;
}

int GLSetupPrims() {
  if (!textures_inited) {
    TexturesInit(GLCreateTexture, GLDeleteTexture, GLUpdateTexture);
    textures_inited=1;
  }
  if (!context.prims_head) // not in orig
    context.prims_head = calloc(1, GL_PRIMITIVE_CAPACITY);
  if (!context.prims_head)
    return ERROR_MALLOC_FAILED;
  GLResetPrims(&context);
  return SUCCESS;
}

void GLResetPrims(gl_context *gc) {
  gc->prims_tail = gc->prims_head;
  GLResetOT(gc->ot, 2048);
}

void GLDrawRect(int x, int y, int w, int h, int r, int g, int b) {
  quad q;
  int i;

  if (!render_frame) return;
  q.p[0].x =   x; q.p[0].y =   y;
  q.p[1].x = x+w; q.p[1].y =   y;
  q.p[3].x =   x; q.p[2].y = y+h;
  q.p[2].x = x+w; q.p[3].y = y+h;
  for (i=0;i<4;i++) {
    q.p[i].x += screen.x;
    q.p[i].y += screen.y;
  }
  glBegin(GL_QUADS);
  for (i=0;i<4;i++) {
    glColor4ub(r, g, b, 255);
    glVertex3i(q.p[i].x, -q.p[i].y, -1);
  }
  glEnd();
  glColor4ub(255, 255, 255, 255);
}

void GLDrawOverlay(int brightness) {
  quad q;
  int i, adj[16];

  if (!render_frame) return;
  adj[0] = 12; adj[1] = 25; adj[2] = 38; adj[3] = 51;
  adj[4] = 64; adj[5] = 77; adj[6] = 91; adj[7] =105;
  adj[8] =120; adj[9] =135; adj[10]=151; adj[11]=167;
  adj[12]=185; adj[13]=203; adj[14]=225; adj[15]=255;
  if (brightness) {
    brightness = limit(brightness>>4,1,16)-1;
    brightness = adj[brightness];
    q.p[0].x = -256; q.p[0].y = -120;
    q.p[1].x =  256; q.p[1].y = -120;
    q.p[3].x = -256; q.p[2].y =  120;
    q.p[2].x =  256; q.p[3].y =  120;
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glBegin(GL_QUADS);
    for (i=0;i<4;i++) {
      glColor4ub(0, 0, 0, brightness);
      glVertex3i(q.p[i].x, -q.p[i].y, -1);
    }
    glEnd();
    glDisable(GL_BLEND);
    glColor4ub(255, 255, 255, 255);
  }
}

static uint32_t GLImageHash(dim2 *dim, const uint8_t *buf) {
  size_t i, size = (size_t)dim->w * dim->h * sizeof(uint32_t);
  uint32_t hash = 2166136261u ^ (uint32_t)dim->w ^ ((uint32_t)dim->h << 16);

  for (i = 0; i < size; i++) {
    hash ^= buf[i];
    hash *= 16777619u;
  }
  return hash;
}

void GLDrawImage(dim2 *dim, uint8_t *buf, pnt2 *loc) {
  quad q;
  rect2 rect;
  pnt2 _loc;
  uint8_t *pixels;
  size_t size;
  uint32_t hash;
  int i;

  /* NSInit draws loading images synchronously before the normal render gate. */
  direct_frame_written = 1;

  rect.x=0;rect.y=0;
  rect.dim = *dim;
  size = (size_t)dim->w * dim->h * sizeof(uint32_t);
  hash = GLImageHash(dim, buf);
  if (!image_texid || image_tex_dim.w != dim->w || image_tex_dim.h != dim->h) {
    if (image_texid) GLDeleteTexture(image_texid);
    image_texid = GLCreateTexture(rect.dim, buf);
    image_tex_dim = *dim;
    image_tex_hash = hash;
    pixels = realloc(image_tex_pixels, size);
    if (pixels) {
      image_tex_pixels = pixels;
      image_tex_size = size;
      memcpy(image_tex_pixels, buf, size);
    }
    else {
      free(image_tex_pixels);
      image_tex_pixels = 0;
      image_tex_size = 0;
    }
  }
  else if (image_tex_size != size || !image_tex_pixels
        || image_tex_hash != hash || memcmp(image_tex_pixels, buf, size) != 0) {
    GLUpdateTexture(image_texid, rect, buf);
    image_tex_hash = hash;
    if (image_tex_size != size || !image_tex_pixels) {
      pixels = realloc(image_tex_pixels, size);
      if (pixels) {
        image_tex_pixels = pixels;
        image_tex_size = size;
      }
    }
    if (image_tex_pixels && image_tex_size == size)
      memcpy(image_tex_pixels, buf, size);
  }
  q.p[0].x =      0; q.p[0].y =      0;
  q.p[1].x = dim->w; q.p[1].y =      0;
  q.p[3].x =      0; q.p[2].y = dim->h;
  q.p[2].x = dim->w; q.p[3].y = dim->h;
  if (!loc) {
    loc = &_loc;
    loc->x = (int32_t)-dim->w/2; /* centered */
    loc->y = (int32_t)-dim->h/2;
  }
  for (i=0;i<4;i++) {
    q.p[i].x += loc->x;
    q.p[i].y += loc->y;
  }
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, image_texid);
  glEnable(GL_TEXTURE_2D);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
  glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0f);
  glDisable(GL_BLEND);
  /* Emscripten's fixed-function bridge leaves attribute arrays enabled after
   * glEnd. Supply the constant color once per vertex so this batch cannot read
   * stale color-array data from an earlier primitive batch. */
  glBegin(GL_QUADS);
  for (i=0;i<4;i++) {
    glColor4ub(255, 255, 255, 255);
    glTexCoord2f((i%3)?1.0:0, (i/2)?1.0:0);
    glVertex3i(q.p[i].x, -q.p[i].y, -1);
  }
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  glColor4ub(255, 255, 255, 255);
}

static void GLDrawPrimitiveBatch(poly3i *poly, int count) {
  int i, ii;

  glBegin(GL_TRIANGLES);
  for (i=0;i<count;i++,poly++) {
    for (ii=0;ii<3;ii++) {
      glColor4ui(poly->colors[ii].r, poly->colors[ii].g,
        poly->colors[ii].b,
        GLPrimitiveVertexAlpha(poly->texid, poly->flags));
      if (poly->texid != -1)
        glTexCoord2f(poly->uvs[ii].x, poly->uvs[ii].y);
      glVertex3i(poly->verts[ii].x, poly->verts[ii].y, -1);
    }
  }
  glEnd();
}

void GLDrawPrims(void *data, int count) {
  poly3i *poly;
  int i, texid, flags, group_type, group_flags, group_texid, group_count;

  if (!render_frame) return;

  texid = -1;
  flags = 3;
  poly = (poly3i*)data;
  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_ALPHA_TEST);
  glEnable(GL_BLEND);
  for (i=0;i<count;) {
    if (poly->prim.type == 3)
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (poly->flags != flags) {
      if (poly->flags == 2) { glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); }
      else if (flags == 2)  { glBlendEquation(GL_FUNC_ADD); }
      switch (poly->flags) {
      case 0:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
      case 1:
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        break;
      case 2:
        glBlendFunc(GL_ONE, GL_ONE);
        break;
      case 3:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
      }
      flags = poly->flags;
    }
    if (poly->texid != texid) {
      if (poly->texid == -1) { glDisable(GL_TEXTURE_2D); }
      else {
        if (texid == -1) { glEnable(GL_TEXTURE_2D); }
        glBindTexture(GL_TEXTURE_2D, poly->texid);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
        glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0f);
      }
      texid = poly->texid;
    }
    group_type = poly->prim.type;
    group_flags = poly->flags;
    group_texid = poly->texid;
    group_count = 1;
    while (i + group_count < count
        && poly[group_count].prim.type == group_type
        && poly[group_count].flags == group_flags
        && poly[group_count].texid == group_texid)
      group_count++;

    if (group_flags == 2 && group_texid != -1) {
      int group_idx;

      /* PS1 textured semi-transparency applies B-F only to STP texels.
       * Draw ordinary texels opaquely, then subtract the STP mask. Zero
       * texels enter the second pass but subtract black, leaving B unchanged.
       * Keep the two passes adjacent per primitive so overlapping polygons
       * retain the ordering-table order. */
      for (group_idx = 0; group_idx < group_count; group_idx++) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.75f);
        glDisable(GL_BLEND);
        GLDrawPrimitiveBatch(&poly[group_idx], 1);
        glEnable(GL_BLEND);
        glAlphaFunc(GL_LESS, 0.75f);
        GLDrawPrimitiveBatch(&poly[group_idx], 1);
      }
      glDisable(GL_ALPHA_TEST);
    }
    else {
      GLDrawPrimitiveBatch(poly, group_count);
    }
    i += group_count;
    poly += group_count;
  }
  glDisable(GL_BLEND);
  glDisable(GL_ALPHA_TEST);
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
  glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0f);
  glColor4ub(255, 255, 255, 255);
}

void **GLGetPrimsTail() {
  return &context.prims_tail;
}

void *GLReservePrimitive(void **tail, size_t size) {
  uint8_t *head = (uint8_t*)context.prims_head;
  uint8_t *current = (uint8_t*)*tail;
  uintptr_t head_address = (uintptr_t)head;
  uintptr_t current_address = (uintptr_t)current;
  size_t used;

  if (!head || current_address < head_address || size > GL_PRIMITIVE_CAPACITY
   || current_address - head_address > GL_PRIMITIVE_CAPACITY - size) {
    primitive_overflow_count++;
    return 0;
  }
  *tail = current + size;
  used = (size_t)((uintptr_t)*tail - head_address);
  if (used > max_primitive_bytes) max_primitive_bytes = (uint32_t)used;
  return current;
}

uint32_t GLPrimitiveBytes(void) {
  return last_primitive_bytes;
}

uint32_t GLMaxPrimitiveBytes(void) { return max_primitive_bytes; }
uint32_t GLPrimitiveOverflowCount(void) { return primitive_overflow_count; }
uint32_t GLConvertedTriangleCount(void) { return last_converted_triangle_count; }
uint32_t GLFrameLargestTriangleArea2(void) { return triangle_frame_diag.area2; }
uint32_t GLFrameLargestTriangleIndex(void) { return triangle_frame_diag.index; }
int32_t GLFrameLargestTriangleX(int vertex) {
  return vertex >= 0 && vertex < 3 ? triangle_frame_diag.x[vertex] : 0;
}
int32_t GLFrameLargestTriangleY(int vertex) {
  return vertex >= 0 && vertex < 3 ? triangle_frame_diag.y[vertex] : 0;
}
int GLFrameLargestTriangleTexid(void) { return triangle_frame_diag.texid; }
int GLFrameLargestTriangleFlags(void) { return triangle_frame_diag.flags; }
int GLFrameLargestTriangleType(void) { return triangle_frame_diag.type; }
uint32_t GLFrameOutsideTriangleCount(void) { return triangle_frame_diag.outside_count; }
uint32_t GLLastError(void) { return last_gl_error; }
uint32_t GLTotalErrorCount(void) { return total_gl_error_count; }

void GLResetOT(void *ot, int len) {
  prim_struct *link;
  int i;

  link = (prim_struct*)ot;
  for (i=0;i<len;i++,link++) {
    link->type = 0;
    link->next = i + 1 < len ? link + 1 : 0;
  }
}

void GLAddPrim(void *prim, int idx) {
  prim_struct *ps, *it, *next;

  ps = (prim_struct*)prim;
  if (!context.ot[idx]) { return; }

  it = context.ot[idx];
  next = PRIM_NEXT(it);
  while (next && next->type) {
    it = next;
    next = PRIM_NEXT(it);
  }
  PRIM_SETNEXT(it,ps);
  PRIM_SETNEXT(ps,next);
}

static uint32_t GLTriangleArea2(const poly3i *tri) {
  int64_t ax = (int64_t)tri->verts[1].x - tri->verts[0].x;
  int64_t ay = (int64_t)tri->verts[1].y - tri->verts[0].y;
  int64_t bx = (int64_t)tri->verts[2].x - tri->verts[0].x;
  int64_t by = (int64_t)tri->verts[2].y - tri->verts[0].y;
  uint64_t p = (uint64_t)(ax < 0 ? -ax : ax)
             * (uint64_t)(by < 0 ? -by : by);
  uint64_t q = (uint64_t)(ay < 0 ? -ay : ay)
             * (uint64_t)(bx < 0 ? -bx : bx);
  uint64_t magnitude;
  int p_negative = (ax < 0) != (by < 0);
  int q_negative = (ay < 0) != (bx < 0);

  if (p_negative != q_negative) {
    if (p > UINT32_MAX || q > UINT32_MAX || p + q > UINT32_MAX)
      return UINT32_MAX;
    magnitude = p + q;
  }
  else {
    magnitude = p > q ? p - q : q - p;
  }
  return magnitude > UINT32_MAX ? UINT32_MAX : (uint32_t)magnitude;
}

/*
 * until another solution is found, for now it is necessary to convert all primitives
 * which are processed by glDrawArrays to triangles, in their order in the OT.
 *
 * alternatives are to
 * 1) a) iterate the OT and group contiguous primitives of the same type;
 *    b) perform a glDrawArrays call for each group
 * 2) manually generate tris instead of quads in sw gfx code
 */
static void GLConvertToTris(void *ot, poly3i **tris, int *count) {
  prim_struct *prim;
  poly3i tri;
  size_t size;
  uint8_t *src, *dst;
  int i, j, k;
  int ot_idx, idx;

  if (count)
    *count = 0;
  last_converted_triangle_count = 0;
  memset(&triangle_frame_diag, 0, sizeof(triangle_frame_diag));
  triangle_frame_diag.index = UINT32_MAX;
  context.prims_tail = context.prims_head;
  /* iterate ot and compute total size of converted tris */
  src = (uint8_t*)ot;
  prim = (prim_struct*)src;
  size = 0;
  while (prim) {
    if (prim->type == 1) { size += sizeof(poly3i); }
    else if (prim->type >= 2) { size += sizeof(poly3i)*2; }
    prim = (prim_struct*)PRIM_NEXT(prim);
  }
  if (size > trimem_capacity) {
    void *new_trimem = realloc(trimem, size);
    if (!new_trimem) {
      *tris = 0;
      return;
    }
    trimem = new_trimem;
    trimem_capacity = size;
  }
  *tris=trimem;
  prim = (prim_struct*)src;
  dst = (uint8_t*)*tris;
  ot_idx = 0;
  while (prim) {
    src = (uint8_t*)prim;
    if (prim->type == 1) {
      *(poly3i*)dst = *(poly3i*)src;
      for (i=0;i<3;i++)
        ((poly3i*)dst)->verts[i].z = -1;
      dst += sizeof(poly3i);
      ++(*count);
    }
    else if (prim->type >= 2) {
      int idxs[4] = {0,1,3,2};
      for (j=0;j<2;j++) {
        /* `next` and `type` share storage. Clear the link first so assigning
         * the converted primitive type is not immediately overwritten. */
        tri.prim.next = 0;
        tri.prim.type = prim->type == 2 ? 1 : 3;
        for (k=0;k<3;k++) {
          idx = idxs[((j*2)+k)%4];
          tri.verts[k] = ((poly4i*)src)->verts[idx];
          tri.verts[k].z = -1;
          tri.colors[k] = ((poly4i*)src)->colors[idx];
          tri.texid = ((poly4i*)src)->texid;
          if (prim->type == 3) {
            tri.texid = -1;
          }
          tri.flags = ((poly4i*)src)->flags;
          tri.uvs[k] = ((poly4i*)src)->uvs[idx];
        }
        *(poly3i*)dst = tri;
        dst += sizeof(poly3i);
        ++(*count);
      }
    }
    else if (prim->type == 0) { ot_idx++; }
    prim = (prim_struct*)PRIM_NEXT(prim);
  }
  dst = (uint8_t*)*tris;
  for (i=0;i<*count;i++) {
    poly3i *converted = (poly3i*)dst;
    uint32_t area2;
    int outside = 0;

    for (j=0;j<3;j++) {
      converted->verts[j].y = -converted->verts[j].y;
      converted->colors[j].a = ~0; /* for now */
      if (converted->verts[j].x < screen.x
       || converted->verts[j].x > screen.x + screen.w
       || converted->verts[j].y < screen.y
       || converted->verts[j].y > screen.y + screen.h)
        outside = 1;
    }
    if (outside) triangle_frame_diag.outside_count++;
    area2 = GLTriangleArea2(converted);
    if (triangle_frame_diag.index == UINT32_MAX
     || area2 > triangle_frame_diag.area2) {
      triangle_frame_diag.area2 = area2;
      triangle_frame_diag.index = (uint32_t)i;
      for (j=0;j<3;j++) {
        triangle_frame_diag.x[j] = converted->verts[j].x;
        triangle_frame_diag.y[j] = converted->verts[j].y;
      }
      triangle_frame_diag.texid = converted->texid;
      triangle_frame_diag.flags = converted->flags;
      triangle_frame_diag.type = converted->prim.type;
    }
    dst += sizeof(poly3i);
  }
  last_converted_triangle_count = (uint32_t)*count;
}

static void GLDraw(void *ot) {
  poly3i *tris;
  int count;

  GLConvertToTris(ot, &tris, &count);
  GLDrawPrims(tris, count);
}

int GLRoundTicks(int ticks) {
  if (ticks < 0)
    return 34;
  if (ticks < 19)
    return 17; /* 1/2 frame */
  if (ticks < 36)
    return 34; /* 1 frame */
  if (ticks < 53)
    return 51; /* 1 1/2 frames */
  return ticks;
}

void GLClear() {
  zone_header *header;
  rgb8 fill;
  int fh;

  if (!render_frame) return;

  glActiveTexture(GL_TEXTURE0);
  glDisable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  glDisable(GL_BLEND);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  glTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 1.0f);
  glTexEnvf(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1.0f);
  glColor4ub(255, 255, 255, 255);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glLoadIdentity();
  // glViewport(0, 0, screen.w, screen.h);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glFrustum(screen.x, screen.x+screen.w, screen.y, screen.y+screen.h,
            1.0, 100000.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  if (!(cur_display_flags & 0x80000)) {
    /* fill bg color(s) */
    header = (zone_header*)cur_zone->items[0];
    fh = header->gfx.vram_fill_height;
    if (cur_display_flags & 0x2000) {
      fill = vram_fill_color;
      GLDrawRect(0,  12+0, 512,     fh, fill.r, fill.g, fill.b); /* top color */
      fill = header->gfx.vram_fill;
      GLDrawRect(0, 12+fh, 512, 216-fh, fill.r, fill.g, fill.b); /* bottom color */
    }
    else {
      GLDrawRect(0,  12+0, 512, 216, 0, 0, 0);
    }
  }
}

void GLBeginFrame(void) {
  render_frame = GLShouldRenderFrame(ns.draw_skip_counter);
}

void GLUpdate() {
  zone_header *header;
  rgb8 fill;
  int fh;
  int ticks_elapsed, elapsed_since;
  GLenum gl_error;
  gl_input gl_input;

  if (callbacks.pre_update)
    (*callbacks.pre_update)();
  if (callbacks.input) {
    (*callbacks.input)(&gl_input);
  }
#ifdef CFLAGS_GUI
  ImGui_ImplOpenGL2_NewFrame();
#endif
  cur_display_flags = next_display_flags;  /* copy display/animate flags */
  vram_fill_color = next_vram_fill_color;
  if (!paused && (cur_display_flags & 0x1000))
    draw_count++;
  AudioUpdate();
  ticks_elapsed = GetTicksElapsed();
  context.sync_stamp = ticks_elapsed;
  if (pbak_state == 0)
    ticks_cur_frame = ticks_elapsed - context.draw_stamp; /* time elapsed between current sync and previous draw  */
  else
    ticks_cur_frame = 17; /* default to 17 */
#ifndef C1_BROWSER
  while (ticks_elapsed - context.draw_stamp < 34) {
    ticks_elapsed = GetTicksElapsed();
  }
#endif
  if (pbak_state == 2)
    SetTicksElapsed(cur_pbak_frame->ticks_elapsed);
  ticks_elapsed = GetTicksElapsed();
  elapsed_since = ticks_elapsed - context.draw_stamp; /* time elapsed between draws */
  context.draw_stamp = ticks_elapsed;
  context.ticks_per_frame = GLRoundTicks(elapsed_since); /* record rounded tick count */
  TexturesUpdate();
  if (context.prims_head && context.prims_tail >= context.prims_head)
    last_primitive_bytes = (uint32_t)((uint8_t*)context.prims_tail
                                   - (uint8_t*)context.prims_head);
  else
    last_primitive_bytes = 0;
  if (ns.draw_skip_counter > 0)
    ns.draw_skip_counter--;
  if (render_frame && ns.draw_skip_counter == 0)
    GLDraw(context.ot);
  GLResetPrims(&context);
  if (fade_counter != 0) { /* brightness */
    if (fade_counter < -2) {
      fade_counter += fade_step;
      GLDrawOverlay(fade_counter + 256);
      if (fade_counter == 0 && !(cur_display_flags & 0x200000))
        fade_counter = -2;
    }
    else if (fade_counter < 0) {
      GLDrawOverlay(256);
      fade_counter = -1;
    }
    else {
      fade_counter -= fade_step;
      GLDrawOverlay(fade_counter);
    }
  }
  /* simulate draw area clipping behavior in orig impl */
  GLDrawRect(0, 0, 512, 12, 0, 0, 0);
  GLDrawRect(0, 228, 512, 12, 0, 0, 0);
#ifdef CFLAGS_GUI
  GuiUpdate();
  if (render_frame) {
    GuiDraw();
    ImGui_ImplOpenGL2_RenderDrawData(igGetDrawData());
  }
#endif
  last_gl_error = GL_NO_ERROR;
  while ((gl_error = glGetError()) != GL_NO_ERROR) {
    last_gl_error = gl_error;
    total_gl_error_count++;
  }
  if (callbacks.post_update && (render_frame || direct_frame_written))
    (*callbacks.post_update)();
  direct_frame_written = 0;
}
