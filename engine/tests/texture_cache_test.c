#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "ns.h"
#include "pc/gfx/tex.h"

page_struct texture_pages[16];
quad28_t uv_map[600];

static int next_texture_id = 1;
static int upload_count;
static rect2 last_upload;

static int TestCreateTexture(dim2 dim, uint8_t *data) {
  (void)dim;
  (void)data;
  return next_texture_id++;
}

static void TestDeleteTexture(GLuint texture) { (void)texture; }

static void TestUploadTexture(GLuint texture, rect2 rect, uint8_t *data) {
  (void)texture;
  assert(data != 0);
  upload_count++;
  last_upload = rect;
}

int main(void) {
  tpage first_page, second_page;
  tpage *active_page;
  texinfo info;
  fvec uvs[4];
  dim2 indexed_dim = { .w = 2, .h = 1 };
  uint8_t indexed_pixels[2] = { 0, 1 };
  uint16_t indexed_palette[256] = { 0 };
  uint32_t converted_pixels[2] = { 0 };
  uint16_t *pixels;
  int texture;

  assert(pixel_5551_8888(0x0000, 0) == 0);
  assert((pixel_5551_8888(0x801F, 0) >> 24) == 0x7F);
  assert((pixel_5551_8888(0x001F, 1) >> 24) == 0xFF);
  assert((pixel_5551_8888(0x801F, 1) >> 24) == 0);
  assert((pixel_5551_8888(0x001F, 2) >> 24) == 0xFF);
  assert((pixel_5551_8888(0x801F, 2) >> 24) == 0x7F);
  assert((pixel_5551_8888(0x801F, 3) & 0xFF) == 255);
  assert((pixel_5551_8888(0x801F, 3) >> 24) == 0xFF);

  /* Loading screens use one byte per palette index.  Keep this conversion
   * covered so their 16-bit CLUT cannot regress to the old blank buffer. */
  indexed_palette[0] = 0x001F;
  indexed_palette[1] = 0x03E0;
  TextureCopy(indexed_pixels, (uint8_t*)converted_pixels,
    &indexed_dim, &indexed_dim, 0, 0, indexed_palette, 2, 1, 3);
  assert(converted_pixels[0] == pixel_5551_8888(indexed_palette[0], 3));
  assert(converted_pixels[1] == pixel_5551_8888(indexed_palette[1], 3));

  memset(texture_pages, 0, sizeof(texture_pages));
  memset(&first_page, 0, sizeof(first_page));
  memset(&second_page, 0, sizeof(second_page));
  memset(&info, 0, sizeof(info));
  memset(uv_map, 0, sizeof(uv_map));
  for (int i = 0; i < 16; i++) texture_pages[i].eid = EID_NONE;

  pixels = (uint16_t*)first_page.data;
  for (int i = 0; i < PAGE_SIZE / (int)sizeof(*pixels); i++) pixels[i] = 0xFFFF;
  pixels = (uint16_t*)second_page.data;
  for (int i = 0; i < PAGE_SIZE / (int)sizeof(*pixels); i++) pixels[i] = 0x83E0;
  /* The page header aliases the raw texture bytes. Populate the synthetic
   * image first, then restore the header fields used by page lookup. */
  first_page.eid = 0x101;
  second_page.eid = 0x103;
  /* TPAGE metadata shares the first words of data; restore its EID after
   * populating the synthetic pixel payload. */
  first_page.eid = 0x101;
  second_page.eid = 0x103;
  texture_pages[8].eid = first_page.eid;
  texture_pages[8].page = (page*)&first_page;

  uv_map[0][0].x = 0; uv_map[0][0].y = 0;
  uv_map[0][1].x = 3; uv_map[0][1].y = 0;
  uv_map[0][2].x = 3; uv_map[0][2].y = 3;
  uv_map[0][3].x = 0; uv_map[0][3].y = 3;
  info.tpage = first_page.eid;
  info.colinfo.type = 1;
  info.colinfo.semi_trans = 3;
  info.rgninfo.color_mode = 2;

  TexturesInit(TestCreateTexture, TestDeleteTexture, TestUploadTexture);
  TexturesBeginFrame();
  texture = TextureLoad(&info, &uvs);
  assert(texture > 0);
  assert(TextureFrameMissCount() == 1);
  assert(TextureFrameFailureCount() == 0);
  TexturesUpdate();
  assert(upload_count == 1);
  assert(last_upload.x == 0 && last_upload.y == 0);
  assert(last_upload.w == 6 && last_upload.h == 6);
  assert(TextureFrameUploadBytes() == 6 * 6 * sizeof(uint32_t));

  assert(TextureLoad(&info, &uvs) == texture);
  assert(TextureFrameHitCount() == 1);
  assert(TextureFrameRequestCount() == TextureFrameHitCount()
       + TextureFrameMissCount());
  TexturesUpdate();
  assert(upload_count == 1);

  /* A replacement later in the same frame must not invalidate queued UVs. */
  texture_pages[8].eid = second_page.eid;
  texture_pages[8].page = (page*)&second_page;
  assert(TextureLoad(&info, &uvs) == texture);
  assert(TextureFrameGenerationMissCount() == 0);

  /* The old generation retires at the next frame boundary. */
  TexturesBeginFrame();
  assert(TextureLoad(&info, &uvs) == -1);
  assert(TextureFrameFailureCount() == 1);
  assert(TextureFrameMissingPageCount() == 1);

  /* Repeated map/level transitions reuse the same physical texture slots.
   * Every newly active generation must rebuild cleanly, including when an EID
   * returns to a slot it occupied on an earlier visit. */
  for (int cycle = 0; cycle < 8; cycle++) {
    active_page = cycle & 1 ? &first_page : &second_page;
    texture_pages[8].eid = active_page->eid;
    texture_pages[8].page = (page*)active_page;
    info.tpage = active_page->eid;
    TexturesBeginFrame();
    assert(TextureLoad(&info, &uvs) > 0);
    assert(TextureFrameFailureCount() == 0);
    assert(TextureFrameCacheFailureCount() == 0);
    TexturesUpdate();
  }
  assert(TextureTotalPageChangeCount() >= 8);
  TexturesKill();
  return 0;
}
