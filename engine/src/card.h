#ifndef C1_CARD_H
#define C1_CARD_H

#include "common.h"

enum c1_card_flag {
  C1_CARD_FLAG_PENDING      = 0x01,
  C1_CARD_FLAG_ERROR        = 0x02,
  C1_CARD_FLAG_CHECK_NEEDED = 0x04,
  C1_CARD_FLAG_CHECKING     = 0x08,
  C1_CARD_FLAG_NEW_DEVICE   = 0x10,
  C1_CARD_FLAG_6            = 0x20
};

enum c1_card_operation {
  C1_CARD_OP_CLEAR_FLAG_6    = 2,
  C1_CARD_OP_SAVE_SELECTED   = 3,
  C1_CARD_OP_LOAD_SELECTED   = 4,
  C1_CARD_OP_FORMAT          = 5,
  C1_CARD_OP_SAVE_CURRENT    = 6,
  C1_CARD_OP_PROBE_NAME      = 7,
  C1_CARD_OP_PROBE_PRESENT   = 8,
  C1_CARD_OP_FORGET_CURRENT  = 9,
  C1_CARD_OP_RESCAN          = 10
};

/*
 * Execute one of the memory-card operations used by the retail GOOL code.
 * Storage access is synchronous, but rescans expose the same short flag
 * sequence as the retail asynchronous card driver so CardC can observe it.
 * Accepted operations return zero; rejected operations and storage failures
 * return one.
 */
int CardControl(int op, int part_idx);

/* Advance an in-progress rescan once per game frame. */
void CardUpdate(void);

/* Restore and maintain the browser's separate automatic resume snapshot. */
int CardBrowserResumeLoad(void);
void CardBrowserResumeBeforeTitleReset(void);
void CardBrowserResumeAfterTitleReset(void);
void CardBrowserResumeUpdate(void);
int CardBrowserResumeFlush(void);

#endif /* C1_CARD_H */
