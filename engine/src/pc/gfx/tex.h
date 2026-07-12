#ifndef _TEX_H_
#define _TEX_H_

#include "common.h"
#include "geom.h"
#include "pcgfx.h"

typedef unsigned int GLuint;
typedef int (*tex_create_callback_t)(dim2, uint8_t*);
typedef void (*tex_delete_callback_t)(GLuint);
typedef void (*tex_subimage_callback_t)(GLuint, rect2, uint8_t*);

extern uint32_t pixel_5551_8888(uint16_t pixel, int semi_trans_mode);
extern void TexturesInit(tex_create_callback_t create,
  tex_delete_callback_t delete, tex_subimage_callback_t subimage);
extern void TexturesKill();
extern void TexturesBeginFrame(void);
extern void TexturesUpdate();
extern int TextureId();
extern int TextureLoad(texinfo *texinfo, fvec(*uvs)[4]);
extern uint32_t TextureOwnedCount(void);
extern uint32_t TextureOwnedBytes(void);
extern uint32_t TextureFrameRequestCount(void);
extern uint32_t TextureFrameHitCount(void);
extern uint32_t TextureFrameMissCount(void);
extern uint32_t TextureFrameFailureCount(void);
extern uint32_t TextureFrameMissingPageCount(void);
extern uint32_t TextureFrameGenerationMissCount(void);
extern uint32_t TextureFrameCacheFailureCount(void);
extern uint32_t TextureFramePageChangeCount(void);
extern uint32_t TextureTotalMissCount(void);
extern uint32_t TextureTotalFailureCount(void);
extern uint32_t TextureTotalPageChangeCount(void);
extern uint32_t TextureFrameUploadBytes(void);
extern uint32_t TextureTotalUploadBytes(void);
// extern int TexturePageGlobal(tpage *tpage);
extern void TextureCopy(uint8_t *src, uint8_t *dst, dim2 *sdim, dim2 *ddim,
  rect2 *srect, pnt2 *dloc,
  void *clut, int clut_type, int color_mode, int semi_trans);
#endif /* _TEX_H_ */
