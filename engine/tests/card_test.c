#include <assert.h>
#include <string.h>

#include "card.h"
#include "globals.h"

gool_globals globals;

static int reset_count;

void LevelResetGlobals(int flag) {
  if (flag) {
    ++reset_count;
  }
}

int main(void) {
  uint32_t progress;
  uint32_t expected_partinfo;

  memset(&globals, 0, sizeof(globals));
  assert(CardControl(C1_CARD_OP_PROBE_PRESENT, 0) == 0);
  assert(CardControl(C1_CARD_OP_PROBE_NAME, 0) == 1);
  assert(CardControl(C1_CARD_OP_RESCAN, 0) == 0);
  assert(card_part_count_ro == 0);
  assert(card_flags_ro == (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_CHECKING |
                           C1_CARD_FLAG_6));
  CardUpdate();
  assert(card_flags_ro == (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_CHECKING |
                           C1_CARD_FLAG_6));
  assert(CardControl(C1_CARD_OP_CLEAR_FLAG_6, 0) == 0);
  assert(card_flags_ro == (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_CHECKING));
  CardUpdate();
  assert(card_flags_ro == 0);

  level_count = 7;
  init_life_count = 4 << 8;
  dword_8006190C = 0x11223344;
  mono = 1;
  sfx_vol = 211;
  mus_vol = 199;
  item_pool1 = 0x10203040;
  item_pool2 = 0x50607080;
  gem_count = 3;
  key_count = 2;

  assert(CardControl(C1_CARD_OP_SAVE_SELECTED, 0) == 0);
  assert(card_flags_ro == 0);
  assert(card_part_count_ro == 1);
  progress = (2u << 10) | (3u << 5) | 7u;
  expected_partinfo = 1u | 8u | (progress << 5) | 0x20000u;
  assert(card_partinfos[0] == expected_partinfo);

  level_count = 1;
  init_life_count = 1;
  dword_8006190C = 0;
  mono = 0;
  sfx_vol = 0;
  mus_vol = 0;
  item_pool1 = 0;
  item_pool2 = 0;
  gem_count = 0;
  key_count = 0;

  assert(CardControl(C1_CARD_OP_LOAD_SELECTED, 0) == 0);
  assert(card_flags_ro == 0);
  assert(reset_count == 1);
  assert(level_count == 7);
  assert(levels_unlocked == 7);
  assert(cur_map_level == 7);
  assert(init_life_count == (4 << 8));
  assert(dword_8006190C == 0x11223344);
  assert(mono == 1);
  assert(sfx_vol == 211);
  assert(mus_vol == 199);
  assert(item_pool1 == 0x10203040);
  assert(item_pool2 == 0x50607080);
  assert(gem_count == 3);
  assert(key_count == 2);

  /* The save-current GOOL path calls op 2 after CHECKING clears. */
  assert(CardControl(C1_CARD_OP_RESCAN, 0) == 0);
  assert(card_part_count_ro == 0);
  CardUpdate();
  CardUpdate();
  assert(card_flags_ro == (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_6));
  assert(card_part_count_ro == 0);
  assert(CardControl(C1_CARD_OP_CLEAR_FLAG_6, 0) == 0);
  assert(card_flags_ro == 0);
  assert(card_part_count_ro == 1);

  assert(CardControl(C1_CARD_OP_FORGET_CURRENT, 0) == 0);
  level_count = 8;
  assert(CardControl(C1_CARD_OP_SAVE_CURRENT, 1) == 1);
  assert(card_flags_ro & C1_CARD_FLAG_ERROR);
  assert(CardControl(C1_CARD_OP_SAVE_SELECTED, 1) == 0);
  assert(card_flags_ro == 0);
  assert(card_part_count_ro == 2);

  level_count = 9;
  assert(CardControl(C1_CARD_OP_SAVE_CURRENT, 0) == 0);
  assert(card_flags_ro == 0);
  assert(card_part_count_ro == 2);

  assert(CardControl(C1_CARD_OP_FORMAT, 0) == 0);
  assert(card_part_count_ro == 0);
  assert(card_partinfos[0] == 0);
  assert(card_flags_ro == C1_CARD_FLAG_CHECK_NEEDED);
  assert(!(card_flags_ro & C1_CARD_FLAG_6));

  assert(CardControl(C1_CARD_OP_RESCAN, 0) == 0);
  CardUpdate();
  CardUpdate();
  assert(card_flags_ro == (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_6));
  assert(CardControl(C1_CARD_OP_CLEAR_FLAG_6, 0) == 0);
  assert(card_flags_ro == 0);
  assert(card_part_count_ro == 0);
  return 0;
}
