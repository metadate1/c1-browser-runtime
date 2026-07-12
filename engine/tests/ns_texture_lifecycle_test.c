#include <assert.h>
#include <string.h>

#include "ns.h"

extern ns_struct ns;
extern page_struct texture_pages[16];

ns_subsystem subsys[21];
int use_cd;
int done;
entry *cur_zone;
eid_t insts[8];

int main(void) {
  static page raw_pages[2];
  page_struct *page_map[2] = { 0 };
  nsd_pte *pte_buckets[256] = { 0 };
  nsd_pte ptes[2];
  entry_ref ref_a, ref_b;
  page_struct *slot;
  page_struct *source_a, *source_b;
  tpage *tpage_a, *tpage_b;
  const eid_t eid_a = 0x00008001;
  const eid_t eid_b = 0x00008003;
  int i;

  memset(&ns, 0, sizeof(ns));
  memset(texture_pages, 0, sizeof(page_struct) * 16);
  memset(raw_pages, 0, sizeof(raw_pages));
  memset(ptes, 0, sizeof(ptes));
  memset(&ref_a, 0, sizeof(ref_a));
  memset(&ref_b, 0, sizeof(ref_b));

  slot = &texture_pages[15];
  source_a = &ns.physical_pages[0];
  source_b = &ns.physical_pages[1];
  tpage_a = (tpage*)&raw_pages[0];
  tpage_b = (tpage*)&raw_pages[1];
  ns.page_map = page_map;
  ns.pte_buckets = pte_buckets;
  ns.physical_page_count = 2;
  page_map[0] = source_a;
  page_map[1] = source_b;
  pte_buckets[1] = ptes;

  for (i = 0; i < 16; i++) {
    texture_pages[i].type = 2;
    texture_pages[i].state = 30;
  }
  slot->state = 1;

  raw_pages[0].magic = MAGIC_PAGE;
  raw_pages[0].type = 1;
  tpage_a->eid = eid_a;
  raw_pages[1].magic = MAGIC_PAGE;
  raw_pages[1].type = 1;
  tpage_b->eid = eid_b;

  source_a->type = 1;
  source_a->idx = 0;
  source_a->pgid = 1;
  source_a->page = &raw_pages[0];
  source_a->state = 4;
  source_b->type = 1;
  source_b->idx = 1;
  source_b->pgid = 3;
  source_b->page = &raw_pages[1];
  source_b->state = 4;

  ptes[0].pgid = 1;
  ptes[0].eid = eid_a;
  ptes[1].pgid = 3;
  ptes[1].eid = eid_b;
  ref_a.eid = eid_a;
  ref_b.eid = eid_b;

  /* Materialize A in slot 15, then explicitly retire it. */
  assert(NSOpen(&ref_a, 0, 1) != 0);
  assert(slot->state == 20);
  assert(slot->eid == eid_a);
  assert(page_map[0] == slot);
  NSTexturePageFree(15);
  assert(slot->state == 1);
  assert(ptes[0].pgid == 1);
  assert(page_map[0] == source_a);
  assert(source_a->state == 4);

  /* Reuse the same slot for B. */
  assert(NSOpen(&ref_b, 0, 1) != 0);
  assert(slot->state == 20);
  assert(slot->eid == eid_b);
  assert(page_map[1] == slot);

  /* Cached ref_a is now a PTE pointer.  Reopening it must evict B and
     rematerialize A from the retained physical page. */
  assert(NSOpen(&ref_a, 0, 1) != 0);
  assert(slot->state == 20);
  assert(slot->eid == eid_a);
  assert(((tpage*)slot->page)->eid == eid_a);
  assert(page_map[0] == slot);
  assert(ptes[0].entry == (entry*)slot->page);
  assert(ptes[1].pgid == 3);
  assert(page_map[1] == source_b);
  assert(source_b->state == 4);
  return 0;
}
