#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "ns.h"

#define NSD_DISK_PAGE_COUNT_OFFSET      0x400u
#define NSD_DISK_PAGE_TABLE_SIZE_OFFSET 0x404u
#define NSD_DISK_HAS_LOADING_IMAGE_OFFSET 0x40Cu
#define NSD_DISK_LOADING_IMAGE_WIDTH_OFFSET 0x410u
#define NSD_DISK_LOADING_IMAGE_HEIGHT_OFFSET 0x414u
#define NSD_DISK_PAGE_TABLE_OFFSET      0x520u
#define NSD_DISK_PTE_SIZE               8u

static void put_u32(uint8_t *data, size_t offset, uint32_t value) {
  memcpy(data + offset, &value, sizeof(value));
}

static void test_index_only_archive_is_not_playable(void) {
  uint8_t cave_metadata[2160];

  memset(cave_metadata, 0, sizeof(cave_metadata));
  put_u32(cave_metadata, NSD_DISK_PAGE_COUNT_OFFSET, 29);
  put_u32(cave_metadata, NSD_DISK_PAGE_TABLE_SIZE_OFFSET, 141);
  assert(!NSLevelMetadataValid(cave_metadata, sizeof(cave_metadata), LID_CAVE));
}

static void test_short_ldat_is_playable(void) {
  const uint32_t pte_count = 1;
  const size_t ldat_offset = NSD_DISK_PAGE_TABLE_OFFSET
                           + pte_count * NSD_DISK_PTE_SIZE;
  const size_t metadata_size = ldat_offset + offsetof(nsd_ldat, image_data);
  uint8_t *metadata = calloc(1, metadata_size);

  assert(metadata);
  put_u32(metadata, NSD_DISK_PAGE_COUNT_OFFSET, 1);
  put_u32(metadata, NSD_DISK_PAGE_TABLE_SIZE_OFFSET, pte_count);
  put_u32(metadata, NSD_DISK_PAGE_TABLE_OFFSET, 1);
  put_u32(metadata, ldat_offset + offsetof(nsd_ldat, magic), MAGIC_LDAT);
  put_u32(metadata, ldat_offset + offsetof(nsd_ldat, lid), LID_BONUS);
  assert(NSLevelMetadataValid(metadata, metadata_size, LID_BONUS));
  assert(!NSLevelMetadataValid(metadata, metadata_size, LID_CAVE));
  assert(!NSLevelMetadataValid(metadata, metadata_size - 1, LID_BONUS));
  free(metadata);
}

static void test_loading_image_requires_complete_sane_data(void) {
  const uint32_t pte_count = 1;
  const size_t ldat_offset = NSD_DISK_PAGE_TABLE_OFFSET
                           + pte_count * NSD_DISK_PTE_SIZE;
  const size_t metadata_size = ldat_offset + sizeof(nsd_ldat);
  uint8_t *metadata = calloc(1, metadata_size);

  assert(metadata);
  put_u32(metadata, NSD_DISK_PAGE_COUNT_OFFSET, 1);
  put_u32(metadata, NSD_DISK_PAGE_TABLE_SIZE_OFFSET, pte_count);
  put_u32(metadata, NSD_DISK_PAGE_TABLE_OFFSET, 1);
  put_u32(metadata, NSD_DISK_HAS_LOADING_IMAGE_OFFSET, 0x100);
  put_u32(metadata, NSD_DISK_LOADING_IMAGE_WIDTH_OFFSET, 432);
  put_u32(metadata, NSD_DISK_LOADING_IMAGE_HEIGHT_OFFSET, 144);
  put_u32(metadata, ldat_offset + offsetof(nsd_ldat, magic), MAGIC_LDAT);
  put_u32(metadata, ldat_offset + offsetof(nsd_ldat, lid), LID_TITLE);
  assert(NSLevelMetadataValid(metadata, metadata_size, LID_TITLE));
  assert(!NSLevelMetadataValid(metadata, metadata_size - 1, LID_TITLE));

  put_u32(metadata, NSD_DISK_LOADING_IMAGE_WIDTH_OFFSET, 513);
  assert(!NSLevelMetadataValid(metadata, metadata_size, LID_TITLE));
  put_u32(metadata, NSD_DISK_LOADING_IMAGE_WIDTH_OFFSET, 432);
  put_u32(metadata, NSD_DISK_LOADING_IMAGE_HEIGHT_OFFSET, 0);
  assert(!NSLevelMetadataValid(metadata, metadata_size, LID_TITLE));
  free(metadata);
}

static void test_page_and_table_bounds(void) {
  const uint32_t pte_count = 1;
  const size_t ldat_offset = NSD_DISK_PAGE_TABLE_OFFSET
                           + pte_count * NSD_DISK_PTE_SIZE;
  const size_t metadata_size = ldat_offset + offsetof(nsd_ldat, image_data);
  uint8_t *metadata = calloc(1, metadata_size);

  assert(metadata);
  put_u32(metadata, NSD_DISK_PAGE_COUNT_OFFSET, 1);
  put_u32(metadata, NSD_DISK_PAGE_TABLE_SIZE_OFFSET, pte_count);
  put_u32(metadata, NSD_DISK_PAGE_TABLE_OFFSET, 1);
  put_u32(metadata, ldat_offset + offsetof(nsd_ldat, magic), MAGIC_LDAT);
  put_u32(metadata, ldat_offset + offsetof(nsd_ldat, lid), LID_TITLE);
  assert(NSLevelMetadataValid(metadata, metadata_size, LID_TITLE));

  put_u32(metadata, NSD_DISK_PAGE_COUNT_OFFSET, NS_PHYSICAL_PAGE_COUNT + 1);
  assert(!NSLevelMetadataValid(metadata, metadata_size, LID_TITLE));
  put_u32(metadata, NSD_DISK_PAGE_COUNT_OFFSET, 1);

  /* Bucket offsets are indices, so one-past-the-table is already invalid. */
  put_u32(metadata, (256 - 1) * sizeof(uint32_t), pte_count);
  assert(!NSLevelMetadataValid(metadata, metadata_size, LID_TITLE));
  put_u32(metadata, (256 - 1) * sizeof(uint32_t), 0);

  /* Odd PTE values are unresolved pgids; index 1 is outside page_count 1. */
  put_u32(metadata, NSD_DISK_PAGE_TABLE_OFFSET, 3);
  assert(!NSLevelMetadataValid(metadata, metadata_size, LID_TITLE));

  /* Resolved/tagged runtime forms are never valid in an on-disk page table. */
  put_u32(metadata, NSD_DISK_PAGE_TABLE_OFFSET, 0x80000002u);
  assert(!NSLevelMetadataValid(metadata, metadata_size, LID_TITLE));
  free(metadata);
}

static void test_page_data_size(void) {
  const size_t one_page = PAGE_SIZE;
  const size_t max_pages = (size_t)NS_PHYSICAL_PAGE_COUNT * PAGE_SIZE;

  assert(NSLevelPageDataSizeValid(one_page, 1));
  assert(NSLevelPageDataSizeValid(max_pages, NS_PHYSICAL_PAGE_COUNT));
  assert(!NSLevelPageDataSizeValid(one_page - 1, 1));
  assert(!NSLevelPageDataSizeValid(one_page + 1, 1));
  assert(!NSLevelPageDataSizeValid(0, 0));
  assert(!NSLevelPageDataSizeValid(max_pages + PAGE_SIZE,
    NS_PHYSICAL_PAGE_COUNT + 1));
}

int main(void) {
  test_index_only_archive_is_not_playable();
  test_short_ldat_is_playable();
  test_loading_image_requires_complete_sane_data();
  test_page_and_table_bounds();
  test_page_data_size();
  return 0;
}
