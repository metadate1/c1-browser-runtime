#include <assert.h>
#include <string.h>

#include "solid.h"

extern zone_query cur_zone_query;
extern uint32_t *wall_bitmap;

static int run_query(int type, int level, int node_z, int collider_z1,
  int collider_z2) {
  zone_query query;
  zone_query_result_rect *rect;
  zone_query_result *node;
  bound nodes_bound = { 0 };
  bound collider = {
    .p1 = { .x = 0, .y = 100, .z = collider_z1 },
    .p2 = { .x = 300, .y = 500, .z = collider_z2 }
  };

  memset(&query, 0, sizeof(query));
  rect = &query.result_rect;
  rect->w = rect->h = rect->d = 1;
  rect->max_depth_x = rect->max_depth_y = rect->max_depth_z = 2;
  node = &query.results[2];
  node->level = level;
  /* A real node also carries subtype bits; keep one set so type 0 at level 0
   * is not mistaken for the all-zero rectangle marker. */
  node->node = type | 8;
  node->x = 0;
  node->y = 10;
  node->z = node_z;
  query.results[3].value = -1;
  query.result_count = 3;

  return FindCeilY(0, &query, &nodes_bound, &collider, 2, 1, -999);
}

static void TestCeilSkipsWrongTypeAndAdvances(void) {
  zone_query query;
  zone_query_result_rect *rect;
  zone_query_result *node;
  bound nodes_bound = { 0 };
  bound collider = {
    .p1 = { .x = 0, .y = 0, .z = 0 },
    .p2 = { .x = 300, .y = 500, .z = 300 }
  };

  memset(&query, 0, sizeof(query));
  rect = &query.result_rect;
  rect->w = rect->h = rect->d = 1;
  rect->max_depth_x = rect->max_depth_y = rect->max_depth_z = 2;
  node = &query.results[2];
  node->node = 2 | 8; /* not one of the two requested types */
  node->y = 10;
  node = &query.results[3];
  node->node = 0 | 8;
  node->y = 20;
  query.results[4].value = -1;
  query.result_count = 4;

  assert(FindCeilY(0, &query, &nodes_bound, &collider, 2, 1, -999) == 320);
}

static void TestReplotWallRecords(void) {
  uint32_t bitmap[32];
  zone_query_result_rect *rect;
  zone_query_result *node;
  vec trans = { 0 };
  int changed;

  memset(&cur_zone_query, 0, sizeof(cur_zone_query));
  rect = (zone_query_result_rect*)&cur_zone_query.results[0];
  rect->w = rect->h = rect->d = 16;
  node = &cur_zone_query.results[2];
  node->node = 1 | 8; /* type 2 after retail's +1 conversion */
  rect = (zone_query_result_rect*)&cur_zone_query.results[3];
  rect->w = rect->h = rect->d = 16;
  node = &cur_zone_query.results[5];
  node->node = 1 | 8;
  node->x = -16;
  cur_zone_query.result_count = 6;
  wall_bitmap = bitmap;

  memset(bitmap, 0xFF, sizeof(bitmap));
  assert(SolidReplotWalls(0, 0, &trans, 0) == 2);
  changed = 0;
  for (int i=0;i<32;i++) changed |= bitmap[i] != 0xFFFFFFFF;
  assert(changed);

  memset(bitmap, 0, sizeof(bitmap));
  assert(SolidReplotWalls(1, 0, &trans, 0) == 2);
  changed = 0;
  for (int i=0;i<32;i++) changed |= bitmap[i] != 0;
  assert(changed);
}

static void TestReplotHeaderDoesNotCountAsWork(void) {
  uint32_t bitmap[32];
  zone_query_result_rect *rect;
  vec trans = { 0 };

  memset(&cur_zone_query, 0, sizeof(cur_zone_query));
  rect = (zone_query_result_rect*)&cur_zone_query.results[0];
  rect->w = rect->h = rect->d = 16;
  cur_zone_query.result_count = 2;
  wall_bitmap = bitmap;

  memset(bitmap, 0xFF, sizeof(bitmap));
  assert(SolidReplotWalls(0, 0, &trans, 0) == 0);
  for (int i=0;i<32;i++) assert(bitmap[i] == 0xFFFFFFFF);
}

static void TestReplotCountsOnlyPlottedNodes(void) {
  uint32_t bitmap[32];
  zone_query_result_rect *rect;
  zone_query_result *node;
  vec trans = { 0 };
  int changed = 0;

  memset(&cur_zone_query, 0, sizeof(cur_zone_query));
  rect = (zone_query_result_rect*)&cur_zone_query.results[0];
  rect->w = rect->h = rect->d = 16;

  node = &cur_zone_query.results[2];
  node->node = 0 | 8; /* type 1: rejected by the flags == 0 type filter */

  node = &cur_zone_query.results[3];
  node->node = 1 | 8; /* type 2, but too far above the object to plot */
  node->y = 10000;

  node = &cur_zone_query.results[4];
  node->node = 1 | 8; /* type 2 in range: the sole plotted record */
  cur_zone_query.result_count = 5;
  wall_bitmap = bitmap;

  memset(bitmap, 0xFF, sizeof(bitmap));
  assert(SolidReplotWalls(0, 0, &trans, 0) == 1);
  for (int i=0;i<32;i++) changed |= bitmap[i] != 0xFFFFFFFF;
  assert(changed);
}

int main(void) {
  TestReplotWallRecords();
  TestReplotHeaderDoesNotCountAsWork();
  TestReplotCountsOnlyPlottedNodes();
  TestCeilSkipsWrongTypeAndAdvances();
  /* Retail accepts either requested octree type, not an impossible
   * simultaneous match of both. */
  assert(run_query(0, 0, 0, 0, 300) == 160);
  assert(run_query(1, 0, 0, 0, 300) == 160);
  assert(run_query(2, 0, 0, 0, 300) == -999);

  /* A node ending inside the collider overlaps it in Z. The old p2-vs-p2
   * comparison incorrectly rejected this partial overlap. */
  assert(run_query(0, 2, 10, 200, 300) == 160);
  return 0;
}
