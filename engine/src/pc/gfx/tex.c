/*
 * texture cache, for paletted texture support
 */
#include "tex.h"
#include "ns.h"
#include "gfx.h"

extern quad28_t uv_map[600];

uint32_t pixel_5551_8888(uint16_t p, int mode) {
  int r, g, b, a;

  r = (p >> 0) & 0x1F;
  g = (p >> 5) & 0x1F;
  b = (p >> 10) & 0x1F;
  a = (p >> 15);

  r = ((r*510)+31)/62;
  g = ((g*510)+31)/62;
  b = ((b*510)+31)/62;

  switch (mode) {
  case 0: a = a == 1 ? 0x7F : (r|g|b) == 0 ? 0 : 0xFF; break;
  case 1: a = a == 1 ? 0xFF : (r|g|b) == 0 ? 0 : 0xFF; break;
  case 2: a = a == 1 ? 0xFF : (r|g|b) == 0 ? 0 : 0xFF; break;
  case 3: a = a == 1 ? 0xFF : (r|g|b) == 0 ? 0 : 0xFF; break;
  }
  return (a << 24) | (b << 16) | (g << 8) | r;
}

#define TEX_TPAGE_FIRST        8
#define TEX_TPAGE_COUNT        8
#define TEX_TPAGE_MEMSIZE      TEX_TPAGE_COUNT*((1024+512+256)*128)*2*sizeof(uint32_t)
#define TEX_CACHE_BUCKETSIZE   0x200
#define TEX_CACHE_TABLE_SIZE   0x4000
#define TEX_OWNED_MAX_COUNT    2048
#define TEX_OWNED_MAX_BYTES    (32u*1024u*1024u)
#define TEX_ATLAS_MAX_ROWS     16

typedef struct _tex_cache_entry {
  int valid;
  texinfo texinfo;
  int texid;
  fvec uvs[4];
  uint32_t hash;
} tex_cache_entry;

typedef struct {
  uint8_t *data;
  rect2 rect;
  int invalid;
  int tpage_id;
  int texid;
  uint16_t row_x[TEX_ATLAS_MAX_ROWS];
  uint16_t row_y[TEX_ATLAS_MAX_ROWS];
  uint16_t row_h[TEX_ATLAS_MAX_ROWS];
  uint16_t packed_height;
  uint8_t row_count;
} tex_atlas;

typedef struct {
  eid_t eids[TEX_TPAGE_COUNT];
  tex_cache_entry table[TEX_TPAGE_COUNT][TEX_CACHE_TABLE_SIZE];
  tex_atlas atlases[TEX_TPAGE_COUNT*6];
  uint8_t data[TEX_TPAGE_MEMSIZE];
  uint32_t owned_count;
  size_t owned_bytes;
  int owned_limit_warned;
  tex_create_callback_t create;
  tex_delete_callback_t delete;
  tex_subimage_callback_t subimage;
  /*
  int global_count;
  eid_t global_eids[TEX_TPAGE_GLOBAL_COUNT];
  tpage *global_tpages[TEX_TPAGE_GLOBAL_COUNT];
  */
} tex_cache;

tex_cache cache;

extern page_struct texture_pages[16];

static int TextureAlphaClass(const texinfo *info) {
  return info->colinfo.semi_trans == 0 ? 0 : 1;
}

static int TextureKeyEquals(const texinfo *a, const texinfo *b) {
  if (a->rgninfo.uv_idx != b->rgninfo.uv_idx
   || a->rgninfo.color_mode != b->rgninfo.color_mode
   || a->rgninfo.segment != b->rgninfo.segment
   || a->rgninfo.offs_x != b->rgninfo.offs_x
   || a->rgninfo.offs_y != b->rgninfo.offs_y
   || TextureAlphaClass(a) != TextureAlphaClass(b)) {
    return 0;
  }
  if (a->rgninfo.color_mode < 2) {
    return a->colinfo.clut_x == b->colinfo.clut_x
        && a->rgninfo.clut_y == b->rgninfo.clut_y;
  }
  return 1;
}

static void TextureRegion(const texinfo *info, quad28 *quad, rect2 *rect,
  pnt2 *minimum, int *vertex_count) {
  int i, count, min_x, min_y, max_x, max_y, ppi;

  *quad = *(quad28*)&uv_map[info->rgninfo.uv_idx];
  count = quad->p[3].x == 99 && quad->p[3].y == 99 ? 3 : 4;
  min_x = max_x = quad->p[0].x;
  min_y = max_y = quad->p[0].y;
  for (i = 1; i < count; i++) {
    if (quad->p[i].x < min_x) min_x = quad->p[i].x;
    if (quad->p[i].x > max_x) max_x = quad->p[i].x;
    if (quad->p[i].y < min_y) min_y = quad->p[i].y;
    if (quad->p[i].y > max_y) max_y = quad->p[i].y;
  }
  ppi = 2 << (2 - info->rgninfo.color_mode);
  rect->x = ((info->rgninfo.segment * 32) + info->rgninfo.offs_x) * ppi + min_x;
  rect->y = info->rgninfo.offs_y * 4 + min_y;
  rect->w = max_x - min_x + 1;
  rect->h = max_y - min_y + 1;
  if (minimum) {
    minimum->x = min_x;
    minimum->y = min_y;
  }
  *vertex_count = count;
}

static int TextureImageKeyEquals(const texinfo *a, const rect2 *a_rect,
  const texinfo *b, const rect2 *b_rect) {
  if (a->rgninfo.color_mode != b->rgninfo.color_mode
   || TextureAlphaClass(a) != TextureAlphaClass(b)
   || a_rect->x != b_rect->x || a_rect->y != b_rect->y
   || a_rect->w != b_rect->w || a_rect->h != b_rect->h) {
    return 0;
  }
  return a->rgninfo.color_mode == 2
      || (a->colinfo.clut_x == b->colinfo.clut_x
       && a->rgninfo.clut_y == b->rgninfo.clut_y);
}

static tex_cache_entry *TextureFindImage(tex_cache_entry *table, uint32_t hash,
  const texinfo *info, const rect2 *rect) {
  tex_cache_entry *entry;
  quad28 quad;
  rect2 other_rect;
  uint32_t i;
  int vertex_count;

  for (i = hash; i < hash + TEX_CACHE_BUCKETSIZE; i++) {
    entry = &table[i % TEX_CACHE_TABLE_SIZE];
    if (!entry->valid) return 0;
    if (entry->hash != hash) continue;
    TextureRegion(&entry->texinfo, &quad, &other_rect, 0, &vertex_count);
    if (TextureImageKeyEquals(info, rect, &entry->texinfo, &other_rect))
      return entry;
  }
  return 0;
}

static void TextureCacheClearPage(int idx) {
  tex_cache_entry *entry;
  rect2 rect;
  quad28 quad;
  int i, vertex_count;

  for (i = 0; i < TEX_CACHE_TABLE_SIZE; i++) {
    entry = &cache.table[idx][i];
    if (entry->valid == 2) {
      TextureRegion(&entry->texinfo, &quad, &rect, 0, &vertex_count);
      (*cache.delete)(entry->texid);
      if (cache.owned_count > 0) cache.owned_count--;
      if (cache.owned_bytes >= (size_t)rect.w * rect.h * sizeof(uint32_t))
        cache.owned_bytes -= (size_t)rect.w * rect.h * sizeof(uint32_t);
      else
        cache.owned_bytes = 0;
    }
  }
  memset(cache.table[idx], 0, sizeof(cache.table[idx]));
}

static void TextureEnsurePage(int idx) {
  tex_atlas *atlas;
  page_struct *page = &texture_pages[TEX_TPAGE_FIRST + idx];
  int i;

  if (cache.eids[idx] == page->eid) return;
  TextureCacheClearPage(idx);
  for (i = 0; i < 6; i++) {
    atlas = &cache.atlases[idx * 6 + i];
    memset(atlas->data, 0, (size_t)atlas->rect.w * atlas->rect.h * sizeof(uint32_t));
    atlas->invalid = 1;
    atlas->tpage_id = idx;
    atlas->row_count = 0;
    atlas->packed_height = 0;
  }
  cache.eids[idx] = page->eid;
}

void TexturesInit(
  tex_create_callback_t create,
  tex_delete_callback_t delete,
  tex_subimage_callback_t subimage) {
  tex_atlas *atlas;
  uint32_t w;
  uint8_t *data;
  int idx, i, j;

  cache.create = create;
  cache.delete = delete;
  cache.subimage = subimage;
  memset(cache.table, 0, sizeof(cache.table));
  memset(cache.eids, 0, sizeof(cache.eids));
  memset(cache.data, 0, sizeof(cache.data));
  cache.owned_count = 0;
  cache.owned_bytes = 0;
  cache.owned_limit_warned = 0;
  data = cache.data;
  for (i=0;i<TEX_TPAGE_COUNT;i++) {
    for (j=0;j<6;j++) {
      idx = (i*6)+j;
      w = 1 << (10 - (j%3));
      atlas = &cache.atlases[idx];
      atlas->data = data;
      atlas->rect.w = w;
      atlas->rect.h = 128;
      atlas->invalid = 0;
      atlas->tpage_id = i;
      atlas->row_count = 0;
      atlas->packed_height = 0;
      atlas->texid = (*cache.create)(atlas->rect.dim, 0);
      data += w*128*sizeof(uint32_t);
    }
  }
  // cache.global_count = 0;
}

void TexturesKill() {
  tex_atlas *atlas;
  int i;

  for (i=0;i<TEX_TPAGE_COUNT;i++)
    TextureCacheClearPage(i);
  for (i=0;i<TEX_TPAGE_COUNT*6;i++) {
    atlas = &cache.atlases[i];
    (*cache.delete)(atlas->texid);
  }
}

void TexturesUpdate() {
  tex_atlas *atlas;
  rect2 rect;
  int i;

  for (i=0;i<TEX_TPAGE_COUNT*6;i++) {
    atlas = &cache.atlases[i];
    if (atlas->invalid) {
      rect.x=0;rect.y=0;rect.dim=atlas->rect.dim;
      (*cache.subimage)(atlas->texid, rect, atlas->data);
      atlas->invalid = 0;
    }
  }
}

void TexturesBeginFrame(void) {
  int i;

  for (i = 0; i < TEX_TPAGE_COUNT; i++)
    TextureEnsurePage(i);
}


/**
 * copies a rect of pixels inside a texture into a 32-bit color texture
 *
 * src        - source texture pixel buffer
 * sdim       - dimensions of source texture, or 0 for default
 * srect      - subrect of pixels to copy, or 0 for entire texture
 * dst        - destination texture pixel buffer
 * ddim       - dimensions of destination texture, or 0 if same as source texture
 * dloc       - location in destination at which to copy pixels, or 0 for (0,0)
 * clut       - generic; see below
 * clut_type  - see below
 * color_mode - color mode of the subrect of pixels in the source texture
 * semi_trans - semi-trans mode of the subrect of pixels in the source texture
 *
 * clut is generic and can be one of the following based on clut_type:
 *
 *   clut_type = 0 => no clut; clut = 0
 *   clut_type = 1 => clut is a pnt2* with the 2 dimensional location
 *                    of the clut in the source texture
 *   clut_type = 2 => clut is a direct pointer to clut data
 */
void TextureCopy(uint8_t *src, uint8_t *dst, dim2 *sdim, dim2 *ddim,
  rect2 *srect, pnt2 *dloc,
  void *clut, int clut_type, int color_mode, int semi_trans) {
  uint32_t *data, palette[256], *dst32;
  uint16_t *clut_data, *src16;
  pnt2 pnt, *clut_loc;
  dim2 dim;
  rect2 rect;
  int i, idx, si, di;
  uint32_t x, y;

  src16 = (uint16_t*)src;
  dst32 = (uint32_t*)dst;
  if (!sdim) { /* use the defaults if no source dimensions specified */
    sdim = &dim;
    sdim->w = 1024 >> color_mode;
    sdim->h = 128;
  }
  if (!ddim) { ddim = sdim; }
  if (!srect) {
    srect = &rect;
    srect->x = 0; srect->y = 0;
    srect->dim = *sdim;
  }
  if (!dloc) {
    dloc = &pnt;
    dloc->x = 0; dloc->y = 0;
  }
  if (clut_type == 0) {
    clut = 0;
  }
  else if (clut_type == 1) {
    clut_loc = (pnt2*)clut;
    idx = (clut_loc->x*16)+(clut_loc->y*(sdim->w>>(2-color_mode)));
    clut_data = &src16[idx];
  }
  else if (clut_type == 2) {
    clut_data = (uint16_t*)clut;
  }
  switch (color_mode) {
  case 0:
    for (i=0;i<16;i++)
      palette[i] = pixel_5551_8888(clut_data[i], semi_trans);
    for (y=0;y<srect->h;y++) {
      for (x=0;x<srect->w;x++) {
        si = (srect->x+x)+(srect->y+y)*sdim->w;
        di = (dloc->x+x)+(dloc->y+y)*ddim->w;
        if (si%2) { idx = (src[si/2] >> 4) & 0xF; }
        else      { idx = (src[si/2] >> 0) & 0xF; }
        dst32[di] = palette[idx];
      }
    }
    break;
  case 1:
    for (i=0;i<256;i++)
      palette[i] = pixel_5551_8888(clut_data[i], semi_trans);
    for (y=0;y<srect->h;y++) {
      for (x=0;x<srect->w;x++) {
        si = (srect->x+x)+((srect->y+y)*sdim->w);
        di = (dloc->x+x)+((dloc->y+y)*ddim->w);
        dst32[di] = palette[src[si]];
      }
    }
    break;
  case 2:
    for (y=0;y<srect->h;y++) {
      for (x=0;x<srect->w;x++) {
        si = (srect->x+x)+(srect->y+y)*sdim->w;
        di = (dloc->x+x)+(dloc->y+y)*ddim->w;
        dst32[di] = pixel_5551_8888(src16[si], semi_trans);
      }
    }
    break;
  }
}

/**
 * retrieves the corresponding texture page [struct] index for a tpag eid
 */
static int TexturePageIdx(entry_ref *tpag) {
  eid_t eid;
  int i;

  if (tpag->is_eid) { eid = tpag->eid; }
  else { eid = tpag->en->eid; }
  for (i=TEX_TPAGE_FIRST;i<16;i++) {
    //if (i<cache.global_count && cache.global_eids[i] == eid) { break; }
    if (texture_pages[i].eid == eid) { break; }
  }
  return i < 16 ? i - TEX_TPAGE_FIRST : -1;
}

/**
 * retrieves the corresponding texture page for a tpag eid
 */
static tpage *TexturePage(entry_ref *tpag) {
  eid_t eid;
  int i, idx;

  eid = tpag->is_eid ? tpag->eid : tpag->en->eid;
  idx = TexturePageIdx((entry_ref*)&eid);
  if (idx == -1) { return 0; }
  //if (idx < cache.global_count) { return cache.global_tpages[idx]; }
  return (tpage*)texture_pages[TEX_TPAGE_FIRST + idx].page;
}

/**
 * retrieves the corresponding texture atlas for a texture page
 */
static tex_atlas *TextureAtlas(tpage *tpage, int color_mode, int semi_trans) {
  tex_atlas *atlas;
  int i, idx;

  i = TexturePageIdx((entry_ref*)&tpage->eid);
  idx = i*6 + (color_mode + (semi_trans == 0 ? 3 : 0));
  atlas = &cache.atlases[idx];
  if (atlas->tpage_id == -1)
    atlas->tpage_id = i;
  return atlas;
}

static int TextureAtlasReserve(tex_atlas *atlas, uint32_t width,
  uint32_t height, pnt2 *inner) {
  uint32_t outer_width = width + 2;
  uint32_t outer_height = height + 2;
  int best = -1;
  int i;

  if (outer_width > atlas->rect.w || outer_height > atlas->rect.h)
    return 0;
  for (i = 0; i < atlas->row_count; i++) {
    if (outer_height <= atlas->row_h[i]
     && atlas->row_x[i] + outer_width <= atlas->rect.w
     && (best == -1 || atlas->row_h[i] < atlas->row_h[best])) {
      best = i;
    }
  }
  if (best == -1) {
    if (atlas->row_count >= TEX_ATLAS_MAX_ROWS
     || atlas->packed_height + outer_height > atlas->rect.h) {
      return 0;
    }
    best = atlas->row_count++;
    atlas->row_x[best] = 0;
    atlas->row_y[best] = atlas->packed_height;
    atlas->row_h[best] = outer_height;
    atlas->packed_height += outer_height;
  }
  inner->x = atlas->row_x[best] + 1;
  inner->y = atlas->row_y[best] + 1;
  atlas->row_x[best] += outer_width;
  return 1;
}

static void TextureAtlasWrite(tex_atlas *atlas, tpage *tpage, rect2 rect,
  pnt2 inner, pnt2 clut, int color_mode, int semi_trans) {
  uint32_t *pixels = (uint32_t*)atlas->data;
  uint32_t stride = atlas->rect.w;
  uint32_t y;

  TextureCopy((uint8_t*)tpage, atlas->data, 0, &atlas->rect.dim, &rect,
    &inner, &clut, 1, color_mode, semi_trans);
  for (y = 0; y < rect.h; y++) {
    uint32_t row = (inner.y + y) * stride;
    pixels[row + inner.x - 1] = pixels[row + inner.x];
    pixels[row + inner.x + rect.w] = pixels[row + inner.x + rect.w - 1];
  }
  memcpy(&pixels[(inner.y - 1) * stride + inner.x - 1],
    &pixels[inner.y * stride + inner.x - 1],
    (rect.w + 2) * sizeof(uint32_t));
  memcpy(&pixels[(inner.y + rect.h) * stride + inner.x - 1],
    &pixels[(inner.y + rect.h - 1) * stride + inner.x - 1],
    (rect.w + 2) * sizeof(uint32_t));
  atlas->invalid = 1;
}

/**
* creates a new texture for the given texinfo
* this may require creating a parent atlas for the texture
*
* returns the id of the parent atlas
* and the uv coordinates of the texture within that atlas
*/
static int TextureNew(texinfo *texinfo, fvec (*uvs)[4]) {
  tex_atlas *atlas;
  tex_cache_entry *table, *entry = 0, *image_entry;
  tpage *tpage;
  quad28 quad, image_quad;
  rect2 rect;
  rect2 image_rect;
  pnt2 clut, minimum, image_minimum, origin, packed_location;
  uint32_t hash;
  size_t texture_bytes;
  uint8_t *pixels;
  float base_u, base_v;
  int page_idx, vertex_count, image_vertex_count, texid, owned, packed;
  uint32_t i;

  TextureRegion(texinfo, &quad, &rect, &minimum, &vertex_count);
  if (rect.x < 0 || rect.y < 0 || !rect.w || !rect.h
   || rect.x + rect.w > (uint32_t)(1024 >> texinfo->rgninfo.color_mode)
   || rect.y + rect.h > 128) {
    return -1;
  }
  clut.x = texinfo->colinfo.clut_x;
  clut.y = texinfo->rgninfo.clut_y;
  tpage = TexturePage((entry_ref*)&texinfo->tpage);
  if (tpage == 0) { return -1; }
  page_idx = TexturePageIdx((entry_ref*)&texinfo->tpage);
  if (page_idx == -1) { return -1; }
  atlas = TextureAtlas(tpage, texinfo->rgninfo.color_mode, texinfo->colinfo.semi_trans);
  hash = texinfo->rgninfo.color_mode<<12|texinfo->rgninfo.segment<<10
        |texinfo->rgninfo.offs_x<<5|texinfo->rgninfo.offs_y;
  table = cache.table[page_idx];
  for (i=hash;i<hash+TEX_CACHE_BUCKETSIZE;i++) {
    if (!table[i%TEX_CACHE_TABLE_SIZE].valid) {
      entry = &table[i%TEX_CACHE_TABLE_SIZE];
      break;
    }
  }
  if (!entry) return -1;

  image_entry = TextureFindImage(table, hash, texinfo, &rect);
  if (image_entry) {
    TextureRegion(&image_entry->texinfo, &image_quad, &image_rect,
      &image_minimum, &image_vertex_count);
    owned = image_entry->valid == 2 || image_entry->valid == 3;
    if (owned) {
      for (i = 0; i < (uint32_t)vertex_count; i++) {
        (*uvs)[i].x = (quad.p[i].x - minimum.x + 0.5f) / rect.w;
        (*uvs)[i].y = (quad.p[i].y - minimum.y + 0.5f) / rect.h;
      }
    }
    else {
      base_u = image_entry->uvs[0].x
             - (image_quad.p[0].x - image_minimum.x + 0.5f) / atlas->rect.w;
      base_v = image_entry->uvs[0].y
             - (image_quad.p[0].y - image_minimum.y + 0.5f) / atlas->rect.h;
      for (i = 0; i < (uint32_t)vertex_count; i++) {
        (*uvs)[i].x = base_u
                     + (quad.p[i].x - minimum.x + 0.5f) / atlas->rect.w;
        (*uvs)[i].y = base_v
                     + (quad.p[i].y - minimum.y + 0.5f) / atlas->rect.h;
      }
    }
    if (vertex_count == 3) (*uvs)[3] = (*uvs)[0];
    entry->valid = owned ? 3 : 1;
    entry->hash = hash;
    entry->texinfo = *texinfo;
    entry->texid = image_entry->texid;
    for (i = 0; i < 4; i++) entry->uvs[i] = (*uvs)[i];
    return entry->texid;
  }

  packed = TextureAtlasReserve(atlas, rect.w, rect.h, &packed_location);
  texture_bytes = (size_t)rect.w * rect.h * sizeof(uint32_t);
  owned = !packed
       && cache.owned_count < TEX_OWNED_MAX_COUNT
       && texture_bytes <= TEX_OWNED_MAX_BYTES - cache.owned_bytes;
  if (packed) {
    TextureAtlasWrite(atlas, tpage, rect, packed_location, clut,
      texinfo->rgninfo.color_mode, texinfo->colinfo.semi_trans);
    texid = atlas->texid;
    for (i = 0; i < (uint32_t)vertex_count; i++) {
      (*uvs)[i].x = (packed_location.x + quad.p[i].x - minimum.x + 0.5f)
                 / atlas->rect.w;
      (*uvs)[i].y = (packed_location.y + quad.p[i].y - minimum.y + 0.5f)
                 / atlas->rect.h;
    }
  }
  else if (owned) {
    pixels = malloc(texture_bytes);
    if (pixels) {
      origin.x = origin.y = 0;
      TextureCopy((uint8_t*)tpage, pixels, 0, &rect.dim, &rect, &origin,
        &clut, 1, texinfo->rgninfo.color_mode, texinfo->colinfo.semi_trans);
      texid = (*cache.create)(rect.dim, pixels);
      free(pixels);
      cache.owned_count++;
      cache.owned_bytes += texture_bytes;
      for (i = 0; i < (uint32_t)vertex_count; i++) {
        (*uvs)[i].x = (quad.p[i].x - minimum.x + 0.5f) / rect.w;
        (*uvs)[i].y = (quad.p[i].y - minimum.y + 0.5f) / rect.h;
      }
    }
    else {
      owned = 0;
    }
  }
  if (!packed && !owned) {
    if (!cache.owned_limit_warned) {
      printf("C1: packed texture budget reached; omitting additional texture regions\n");
      cache.owned_limit_warned = 1;
    }
    return -1;
  }
  if (vertex_count == 3) (*uvs)[3] = (*uvs)[0];

  entry->valid = owned ? 2 : 1;
  entry->hash = hash;
  entry->texinfo = *texinfo;
  entry->texid = texid;
  for (i = 0; i < 4; i++) entry->uvs[i] = (*uvs)[i];
  return texid;
}

uint32_t TextureOwnedCount(void) {
  return cache.owned_count;
}

uint32_t TextureOwnedBytes(void) {
  return (uint32_t)cache.owned_bytes;
}

/**
* returns the id of the atlas
* which contains the texture referenced by the given texinfo
* and the uv coordinates of the texture within that atlas
*/
static int TextureLookup(texinfo *texinfo, fvec(*uvs)[4]) {
  tex_cache_entry *table, *entry;
  uint32_t hash, i;
  int idx;

  idx = TexturePageIdx((entry_ref*)&texinfo->tpage);
  if (idx == -1) { return -1; }
  hash = texinfo->rgninfo.color_mode<<12|texinfo->rgninfo.segment<<10
        |texinfo->rgninfo.offs_x<<5|texinfo->rgninfo.offs_y;
  table = cache.table[idx];
  for (i = hash; i < hash + TEX_CACHE_BUCKETSIZE; i++) {
    entry = &table[i%TEX_CACHE_TABLE_SIZE];
    if (!entry->valid) { return -1; }
    if (hash == entry->hash && TextureKeyEquals(texinfo, &entry->texinfo)) {
      for (i = 0; i < 4; i++) { (*uvs)[i] = entry->uvs[i]; }
      return entry->texid;
    }
  }
  return -1;
}

/**
* creates a new texture for the given texinfo if one does not exist
*
* returns the id of the texture's parent atlas
* and the uv coordinates of the texture within that atlas
*/
int TextureLoad(texinfo *texinfo, fvec(*uvs)[4]) {
  int idx, texid;

  idx = TexturePageIdx((entry_ref*)&texinfo->tpage);
  if (idx == -1) return -1;
  /* A mid-frame page replacement is retired safely at the next frame start. */
  if (cache.eids[idx] != texture_pages[TEX_TPAGE_FIRST + idx].eid)
    return -1;
  texid = TextureLookup(texinfo, uvs);
  if (texid != -1) return texid;
  texid = TextureNew(texinfo, uvs);
  return texid;
}

/**
* loads a global texture page which persists between game states
*
* this page will be chosen over ones which are dynamically loaded/unloaded
* by the game
*/
/*
int TexturePageGlobal(tpage *tpage) {
  int idx;

  idx = cache.global_count++;
  cache.global_eids[idx] = tpage->eid;
  cache.global_tpages[idx] = tpage;
  return idx;
}
*/
