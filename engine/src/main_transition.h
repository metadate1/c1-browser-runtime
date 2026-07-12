#ifndef C1_MAIN_TRANSITION_H
#define C1_MAIN_TRANSITION_H

#include "ns.h"

typedef struct {
  lid_t lid;
  int bonus_return;
} core_level_transition;

/*
 * The retail loop keeps the requested level in a saved register while it
 * broadcasts GOOL_EVENT_LEVEL_END.  Event cleanup may write next_lid, but it
 * must not replace an ordinary transition target.  A post-event -2 is the
 * sole override: it means return to the level held by the bonus savestate.
 */
static inline core_level_transition CoreResolveLevelTransition(
  lid_t requested_lid, lid_t next_lid_after_event, lid_t saved_lid) {
  core_level_transition transition;

  if (next_lid_after_event == -2) {
    transition.lid = saved_lid;
    transition.bonus_return = 1;
  }
  else {
    transition.lid = requested_lid;
    transition.bonus_return = 0;
  }
  return transition;
}

#endif /* C1_MAIN_TRANSITION_H */
