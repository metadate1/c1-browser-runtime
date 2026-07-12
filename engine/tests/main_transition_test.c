#include <assert.h>

#include "main_transition.h"

int main(void) {
  core_level_transition transition;

  transition = CoreResolveLevelTransition(
    LID_LEVELEND, LID_TITLE, LID_NSANITYBEACH);
  assert(transition.lid == LID_LEVELEND);
  assert(transition.bonus_return == 0);

  transition = CoreResolveLevelTransition(
    LID_TITLE, LID_LEVELEND, LID_NSANITYBEACH);
  assert(transition.lid == LID_TITLE);
  assert(transition.bonus_return == 0);

  transition = CoreResolveLevelTransition(
    LID_LEVELEND, -2, LID_NSANITYBEACH);
  assert(transition.lid == LID_NSANITYBEACH);
  assert(transition.bonus_return == 1);

  return 0;
}
