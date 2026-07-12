#ifndef _MISC_H_
#define _MISC_H_

#include "common.h"

/* Advance a signed lighting parameter without stepping past its target.
 * Both increasing and decreasing ramps are used by Lights Out/Fumbling. */
static inline int32_t ShaderStepToward(int32_t current, int32_t target,
  int32_t *step) {
  int32_t next = current + *step;

  if ((target > current && next >= target)
   || (target < current && next <= target)) {
    *step = 0;
    return target;
  }
  return next;
}

extern void ShaderParamsUpdateRipple(int init);
extern void ShaderParamsUpdate(int init);

#endif /* _MISC_H_ */
