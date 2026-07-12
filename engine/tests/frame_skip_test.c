#include <assert.h>

#include "pc/gfx/gl.h"

int main(void) {
  assert(GLShouldRenderFrame(0));
  assert(GLShouldRenderFrame(1));
  assert(!GLShouldRenderFrame(2));
  assert(!GLShouldRenderFrame(3));
  assert(!GLShouldRenderFrame(-1));
  return 0;
}
