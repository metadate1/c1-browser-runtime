#include <assert.h>
#include <stdint.h>

#include "pc/gfx/gl.h"

int main(void) {
  /* Untextured modes encode their blend factor in vertex alpha. */
  assert(GLPrimitiveVertexAlpha(-1, 0) == 0x7FFFFFFFu);
  assert(GLPrimitiveVertexAlpha(-1, 1) == 0);

  /* Opaque/subtractive untextured primitives must not inherit that alpha. */
  assert(GLPrimitiveVertexAlpha(-1, 2) == UINT32_MAX);
  assert(GLPrimitiveVertexAlpha(-1, 3) == UINT32_MAX);

  /* Texture alpha selects STP texels for every textured blend mode. */
  assert(GLPrimitiveVertexAlpha(7, 0) == UINT32_MAX);
  assert(GLPrimitiveVertexAlpha(7, 1) == UINT32_MAX);
  assert(GLPrimitiveVertexAlpha(7, 2) == UINT32_MAX);
  assert(GLPrimitiveVertexAlpha(7, 3) == UINT32_MAX);
  return 0;
}
