#include <assert.h>

#include "gool.h"

int main(void) {
  uint32_t value = 0x12345678;

  assert(GoolPsxInputValue(&value) == value);
  assert(GoolPsxInputValue(0) == 3);
  return 0;
}
