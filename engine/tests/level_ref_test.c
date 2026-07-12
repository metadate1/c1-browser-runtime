#include <assert.h>

#include "level.h"

int main(void) {
  assert(LdatZoneRefIncrement(LID_HOGWILD) == 2);
  assert(LdatZoneRefIncrement(LID_WHOLEHOG) == 2);
  assert(LdatZoneRefIncrement(LID_ROLLINGSTONES) == 1);
  assert(LdatZoneRefIncrement(LID_TITLE) == 1);
  return 0;
}
