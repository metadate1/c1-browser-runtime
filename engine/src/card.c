#include "card.h"

#include <string.h>

#include "globals.h"
#include "level.h"

#ifdef C1_BROWSER
#include <emscripten.h>
#endif

#define C1_CARD_SLOT_COUNT 15
#define C1_CARD_PAYLOAD_SIZE 128

enum c1_card_storage_result {
  C1_CARD_STORAGE_ERROR = -1,
  C1_CARD_STORAGE_CORRUPT = -2,
  C1_CARD_STORAGE_EMPTY = 0,
  C1_CARD_STORAGE_OK = 1
};

enum c1_card_storage_operation {
  C1_CARD_STORAGE_READ = 0,
  C1_CARD_STORAGE_WRITE = 1,
  C1_CARD_STORAGE_FORMAT = 2
};

enum c1_card_payload_offset {
  C1_CARD_PROGRESS_OFFSET = 0,
  C1_CARD_LEVEL_COUNT_OFFSET = 4,
  C1_CARD_INITIAL_LIVES_OFFSET = 8,
  C1_CARD_UNKNOWN_6190C_OFFSET = 12,
  C1_CARD_MONO_OFFSET = 16,
  C1_CARD_SFX_VOLUME_OFFSET = 20,
  C1_CARD_MUSIC_VOLUME_OFFSET = 24,
  C1_CARD_ITEM_POOL_1_OFFSET = 28,
  C1_CARD_ITEM_POOL_2_OFFSET = 32,
  C1_CARD_CHECKSUM_OFFSET = 124
};

typedef struct c1_card_payload {
  uint8_t bytes[C1_CARD_PAYLOAD_SIZE];
} c1_card_payload;

typedef struct c1_card_snapshot {
  int slot_map[C1_CARD_SLOT_COUNT];
  uint8_t slot_valid[C1_CARD_SLOT_COUNT];
  uint32_t partinfos[C1_CARD_SLOT_COUNT];
  int part_count;
  int has_corrupt_slot;
} c1_card_snapshot;

_Static_assert(sizeof(c1_card_payload) == C1_CARD_PAYLOAD_SIZE,
  "Crash 1 card payload must be exactly 128 bytes");

static int card_slot_map[C1_CARD_SLOT_COUNT];
static uint8_t card_slot_valid[C1_CARD_SLOT_COUNT];
static int card_current_slot = -1;
static c1_card_snapshot card_staged_snapshot;
static int card_scan_active;
static unsigned int card_scan_ticks;
static c1_card_payload browser_resume_last_payload;
static unsigned int browser_resume_ticks;
static int browser_resume_enabled;
static int browser_resume_result;
static c1_card_payload browser_resume_title_payload;
static int browser_resume_title_restore_pending;
static int browser_resume_title_protection;

#ifdef C1_BROWSER

/*
 * Browser saves deliberately use one small, versioned JSON record. The game
 * payload itself remains an opaque base64-encoded 128-byte retail structure.
 * Returning CORRUPT separately lets C expose the retail CHECK_NEEDED flag.
 */
EM_JS(int, C1BrowserCardStorage,
  (int operation, int slot, uint8_t *buffer, int length), {
    const key = "c1.virtual-memory-card.v1";
    const schema = "c1-virtual-memory-card";
    const version = 1;
    const slotCount = 15;

    const emptyCard = () => ({
      schema,
      version,
      slots: Array(slotCount).fill(null),
      updatedAt: Date.now()
    });

    const loadCard = (allowMissing) => {
      const raw = globalThis.localStorage.getItem(key);
      if (raw === null) {
        return allowMissing ? emptyCard() : null;
      }

      const card = JSON.parse(raw);
      if (!card || card.schema !== schema || card.version !== version ||
          !Array.isArray(card.slots)) {
        throw new Error("unsupported virtual memory-card record");
      }
      return card;
    };

    try {
      if (!globalThis.localStorage) {
        return -1;
      }

      if (operation === 2) {
        globalThis.localStorage.setItem(key, JSON.stringify(emptyCard()));
        return 1;
      }

      if (slot < 0 || slot >= slotCount || length !== 128) {
        return -1;
      }

      if (operation === 0) {
        const card = loadCard(false);
        if (card === null || slot >= card.slots.length || card.slots[slot] === null) {
          return 0;
        }

        const record = card.slots[slot];
        if (!record || typeof record.payload !== "string") {
          return -2;
        }

        let binary;
        try {
          binary = atob(record.payload);
        } catch (error) {
          return -2;
        }
        if (binary.length !== length) {
          return -2;
        }
        for (let i = 0; i < length; ++i) {
          HEAPU8[buffer + i] = binary.charCodeAt(i);
        }
        return 1;
      }

      if (operation === 1) {
        const card = loadCard(true);
        while (card.slots.length < slotCount) {
          card.slots.push(null);
        }
        if (card.slots.length > slotCount) {
          card.slots.length = slotCount;
        }

        let binary = "";
        for (let i = 0; i < length; ++i) {
          binary += String.fromCharCode(HEAPU8[buffer + i]);
        }
        card.slots[slot] = {
          payload: btoa(binary),
          updatedAt: Date.now()
        };
        card.updatedAt = Date.now();
        globalThis.localStorage.setItem(key, JSON.stringify(card));
        return 1;
      }
    } catch (error) {
      console.error("C1 virtual memory-card storage failed", error);
      return -1;
    }

    return -1;
  });

EM_JS(int, C1BrowserResumeStorage,
  (int operation, uint8_t *buffer, int length), {
    const key = "c1.browser-resume.v1";
    const schema = "c1-browser-resume";
    const version = 1;

    const quarantine = (raw) => {
      try {
        globalThis.localStorage.setItem(
          `${key}.invalid.${Date.now()}`, raw);
      } catch (error) {
        console.warn("C1 could not preserve an invalid resume record", error);
      }
      globalThis.localStorage.removeItem(key);
    };

    try {
      if (!globalThis.localStorage) return -1;
      if (operation === 2) {
        const raw = globalThis.localStorage.getItem(key);
        if (raw !== null) quarantine(raw);
        return 1;
      }
      if (length !== 128) return -1;
      if (operation === 0) {
        const raw = globalThis.localStorage.getItem(key);
        if (raw === null) return 0;
        let record;
        try { record = JSON.parse(raw); }
        catch (error) { quarantine(raw); return -2; }
        if (record && record.schema === schema && record.version > version) {
          console.warn("C1 resume record is from a newer version");
          return -1;
        }
        if (!record || record.schema !== schema || record.version !== version ||
            typeof record.payload !== "string") {
          quarantine(raw);
          return -2;
        }
        let binary;
        try { binary = atob(record.payload); }
        catch (error) { quarantine(raw); return -2; }
        if (binary.length !== length) { quarantine(raw); return -2; }
        for (let i = 0; i < length; ++i) {
          HEAPU8[buffer + i] = binary.charCodeAt(i);
        }
        return 1;
      }
      if (operation === 1) {
        let binary = "";
        for (let i = 0; i < length; ++i) {
          binary += String.fromCharCode(HEAPU8[buffer + i]);
        }
        globalThis.localStorage.setItem(key, JSON.stringify({
          schema,
          version,
          payload: btoa(binary),
          updatedAt: Date.now()
        }));
        return 1;
      }
    } catch (error) {
      console.error("C1 browser resume storage failed", error);
      return -1;
    }
    return -1;
  });

static int CardStorageRead(int slot, c1_card_payload *payload) {
  return C1BrowserCardStorage(
    C1_CARD_STORAGE_READ, slot, payload->bytes, C1_CARD_PAYLOAD_SIZE);
}

static int CardStorageWrite(int slot, const c1_card_payload *payload) {
  return C1BrowserCardStorage(
    C1_CARD_STORAGE_WRITE, slot, (uint8_t*)payload->bytes, C1_CARD_PAYLOAD_SIZE);
}

static int CardStorageFormat(void) {
  return C1BrowserCardStorage(C1_CARD_STORAGE_FORMAT, 0, NULL, 0);
}

EMSCRIPTEN_KEEPALIVE int C1CardControl(int operation, int part_idx) {
  return CardControl(operation, part_idx);
}

EMSCRIPTEN_KEEPALIVE int C1GetCardPartCount(void) {
  return card_part_count_ro;
}

EMSCRIPTEN_KEEPALIVE int C1GetCardFlags(void) {
  return card_flags_ro;
}

#else

/* Desktop builds keep a process-local card when no platform backend exists. */
static c1_card_payload card_memory_slots[C1_CARD_SLOT_COUNT];
static uint8_t card_memory_slot_used[C1_CARD_SLOT_COUNT];

static int CardStorageRead(int slot, c1_card_payload *payload) {
  if (slot < 0 || slot >= C1_CARD_SLOT_COUNT || !payload) {
    return C1_CARD_STORAGE_ERROR;
  }
  if (!card_memory_slot_used[slot]) {
    return C1_CARD_STORAGE_EMPTY;
  }
  *payload = card_memory_slots[slot];
  return C1_CARD_STORAGE_OK;
}

static int CardStorageWrite(int slot, const c1_card_payload *payload) {
  if (slot < 0 || slot >= C1_CARD_SLOT_COUNT || !payload) {
    return C1_CARD_STORAGE_ERROR;
  }
  card_memory_slots[slot] = *payload;
  card_memory_slot_used[slot] = 1;
  return C1_CARD_STORAGE_OK;
}

static int CardStorageFormat(void) {
  memset(card_memory_slots, 0, sizeof(card_memory_slots));
  memset(card_memory_slot_used, 0, sizeof(card_memory_slot_used));
  return C1_CARD_STORAGE_OK;
}

#endif

static uint32_t CardReadU32(const c1_card_payload *payload, int offset) {
  const uint8_t *p = &payload->bytes[offset];
  return (uint32_t)p[0]
       | ((uint32_t)p[1] << 8)
       | ((uint32_t)p[2] << 16)
       | ((uint32_t)p[3] << 24);
}

static void CardWriteU32(c1_card_payload *payload, int offset, uint32_t value) {
  uint8_t *p = &payload->bytes[offset];
  p[0] = (uint8_t)value;
  p[1] = (uint8_t)(value >> 8);
  p[2] = (uint8_t)(value >> 16);
  p[3] = (uint8_t)(value >> 24);
}

static uint32_t CardRotateLeft3(uint32_t value) {
  return (value << 3) | (value >> 29);
}

static uint32_t CardChecksum(const c1_card_payload *payload) {
  uint32_t checksum = 0x12345678;
  int i;

  for (i = 0; i < C1_CARD_PAYLOAD_SIZE; ++i) {
    uint8_t byte = payload->bytes[i];
    if (i >= C1_CARD_CHECKSUM_OFFSET && i < C1_CARD_CHECKSUM_OFFSET + 4) {
      byte = 0;
    }
    checksum = CardRotateLeft3(checksum + byte);
  }
  return checksum;
}

static int CardPayloadValid(const c1_card_payload *payload) {
  return CardReadU32(payload, C1_CARD_CHECKSUM_OFFSET) == CardChecksum(payload);
}

static uint32_t CardCurrentProgress(void) {
  return ((uint32_t)key_count << 10)
       | ((uint32_t)gem_count << 5)
       | ((uint32_t)level_count & 0x1F);
}

static void CardCreatePayload(c1_card_payload *payload) {
  memset(payload, 0, sizeof(*payload));
  CardWriteU32(payload, C1_CARD_PROGRESS_OFFSET, CardCurrentProgress());
  CardWriteU32(payload, C1_CARD_LEVEL_COUNT_OFFSET, (uint32_t)level_count);
  CardWriteU32(payload, C1_CARD_INITIAL_LIVES_OFFSET, (uint32_t)init_life_count);
  CardWriteU32(payload, C1_CARD_UNKNOWN_6190C_OFFSET, dword_8006190C);
  CardWriteU32(payload, C1_CARD_MONO_OFFSET, (uint32_t)mono);
  CardWriteU32(payload, C1_CARD_SFX_VOLUME_OFFSET, sfx_vol);
  CardWriteU32(payload, C1_CARD_MUSIC_VOLUME_OFFSET, mus_vol);
  CardWriteU32(payload, C1_CARD_ITEM_POOL_1_OFFSET, item_pool1);
  CardWriteU32(payload, C1_CARD_ITEM_POOL_2_OFFSET, item_pool2);
  CardWriteU32(payload, C1_CARD_CHECKSUM_OFFSET, CardChecksum(payload));
}

static void CardRestorePayload(const c1_card_payload *payload) {
  uint32_t progress = CardReadU32(payload, C1_CARD_PROGRESS_OFFSET);

  init_life_count = (int32_t)CardReadU32(payload, C1_CARD_INITIAL_LIVES_OFFSET);
  LevelResetGlobals(1);
  level_count = (int32_t)CardReadU32(payload, C1_CARD_LEVEL_COUNT_OFFSET);
  dword_8006190C = CardReadU32(payload, C1_CARD_UNKNOWN_6190C_OFFSET);
  mono = (int32_t)CardReadU32(payload, C1_CARD_MONO_OFFSET);
  sfx_vol = CardReadU32(payload, C1_CARD_SFX_VOLUME_OFFSET);
  mus_vol = CardReadU32(payload, C1_CARD_MUSIC_VOLUME_OFFSET);
  item_pool1 = CardReadU32(payload, C1_CARD_ITEM_POOL_1_OFFSET);
  item_pool2 = CardReadU32(payload, C1_CARD_ITEM_POOL_2_OFFSET);
  gem_count = (int)((progress >> 5) & 0x1F);
  key_count = (int)(progress >> 10);
  levels_unlocked = level_count;
  cur_map_level = level_count;
}

int CardBrowserResumeLoad(void) {
#ifdef C1_BROWSER
  c1_card_payload payload;
#endif

  browser_resume_ticks = 0;
  browser_resume_title_restore_pending = 0;
#ifdef C1_BROWSER
  browser_resume_title_protection = 1;
  browser_resume_result = C1BrowserResumeStorage(
    C1_CARD_STORAGE_READ, payload.bytes, C1_CARD_PAYLOAD_SIZE);
  if (browser_resume_result == C1_CARD_STORAGE_OK) {
    if (!CardPayloadValid(&payload)) {
      browser_resume_result = C1_CARD_STORAGE_CORRUPT;
      C1BrowserResumeStorage(C1_CARD_STORAGE_FORMAT, NULL, 0);
      CardCreatePayload(&browser_resume_last_payload);
      browser_resume_enabled = 1;
      return browser_resume_result;
    }
    CardRestorePayload(&payload);
    browser_resume_last_payload = payload;
    browser_resume_enabled = 1;
    return browser_resume_result;
  }
  if (browser_resume_result == C1_CARD_STORAGE_EMPTY) {
    CardCreatePayload(&browser_resume_last_payload);
    browser_resume_enabled = 1;
    return browser_resume_result;
  }
  if (browser_resume_result == C1_CARD_STORAGE_CORRUPT) {
    CardCreatePayload(&browser_resume_last_payload);
    browser_resume_enabled = 1;
    return browser_resume_result;
  }
  browser_resume_enabled = 0;
  return browser_resume_result;
#else
  browser_resume_title_protection = 0;
  CardCreatePayload(&browser_resume_last_payload);
  browser_resume_enabled = 0;
  browser_resume_result = C1_CARD_STORAGE_EMPTY;
  return browser_resume_result;
#endif
}

void CardBrowserResumeBeforeTitleReset(void) {
  if (!browser_resume_title_protection)
    return;
  CardCreatePayload(&browser_resume_title_payload);
  browser_resume_title_restore_pending = 1;
  if (browser_resume_enabled)
    CardBrowserResumeFlush();
}

void CardBrowserResumeAfterTitleReset(void) {
  if (!browser_resume_title_restore_pending)
    return;
  CardRestorePayload(&browser_resume_title_payload);
  browser_resume_title_restore_pending = 0;
}

int CardBrowserResumeFlush(void) {
  c1_card_payload payload;

  if (!browser_resume_enabled) return browser_resume_result;
  CardCreatePayload(&payload);
  if (memcmp(&payload, &browser_resume_last_payload, sizeof(payload)) == 0)
    return C1_CARD_STORAGE_OK;
#ifdef C1_BROWSER
  browser_resume_result = C1BrowserResumeStorage(
    C1_CARD_STORAGE_WRITE, payload.bytes, C1_CARD_PAYLOAD_SIZE);
  if (browser_resume_result != C1_CARD_STORAGE_OK) {
    browser_resume_enabled = 0;
    return browser_resume_result;
  }
#else
  browser_resume_result = C1_CARD_STORAGE_OK;
#endif
  browser_resume_last_payload = payload;
  return browser_resume_result;
}

void CardBrowserResumeUpdate(void) {
  if (!browser_resume_enabled) return;
  if (++browser_resume_ticks < 30) return;
  browser_resume_ticks = 0;
  CardBrowserResumeFlush();
}

#ifdef C1_BROWSER
EMSCRIPTEN_KEEPALIVE int C1FlushBrowserResume(void) {
  return CardBrowserResumeFlush();
}

EMSCRIPTEN_KEEPALIVE int C1GetBrowserResumeResult(void) {
  return browser_resume_result;
}

EMSCRIPTEN_KEEPALIVE int C1GetLevelCount(void) {
  return level_count;
}

EMSCRIPTEN_KEEPALIVE int C1GetKeyCount(void) {
  return key_count;
}

EMSCRIPTEN_KEEPALIVE int C1GetGemCount(void) {
  return gem_count;
}

EMSCRIPTEN_KEEPALIVE int C1GetSfxVolume(void) {
  return (int)sfx_vol;
}

EMSCRIPTEN_KEEPALIVE int C1GetMusicVolume(void) {
  return (int)mus_vol;
}

EMSCRIPTEN_KEEPALIVE int C1GetMono(void) {
  return mono;
}
#endif

static void CardCancelScan(void) {
  card_scan_active = 0;
  card_scan_ticks = 0;
}

static void CardSetFailure(int check_needed) {
  uint32_t flags = card_flags_ro & C1_CARD_FLAG_NEW_DEVICE;

  CardCancelScan();
  card_flags_ro = flags | C1_CARD_FLAG_ERROR;
  if (check_needed) {
    card_flags_ro |= C1_CARD_FLAG_CHECK_NEEDED;
  }
}

static void CardSetOperationSuccess(int clear_new_device) {
  uint32_t flags = clear_new_device
    ? 0
    : card_flags_ro & C1_CARD_FLAG_NEW_DEVICE;

  CardCancelScan();
  card_flags_ro = flags;
}

static void CardClearPublishedMetadata(void) {
  memset(card_partinfos, 0, sizeof(card_partinfos));
  memset(card_slot_map, -1, sizeof(card_slot_map));
  memset(card_slot_valid, 0, sizeof(card_slot_valid));
  card_part_count_ro = 0;
}

static void CardSnapshotAddDamagedSlot(c1_card_snapshot *snapshot, int slot) {
  int part_idx = snapshot->part_count++;

  snapshot->has_corrupt_slot = 1;
  snapshot->slot_map[part_idx] = slot;
  /* Retail marks a damaged one-image part as complete, but not loadable. */
  snapshot->partinfos[part_idx] = 1u | (1u << 1);
}

static int CardBuildSnapshot(c1_card_snapshot *snapshot) {
  c1_card_payload payload;
  int slot;

  memset(snapshot, 0, sizeof(*snapshot));
  memset(snapshot->slot_map, -1, sizeof(snapshot->slot_map));

  for (slot = 0; slot < C1_CARD_SLOT_COUNT; ++slot) {
    int result = CardStorageRead(slot, &payload);
    uint32_t progress;

    if (result == C1_CARD_STORAGE_EMPTY) {
      continue;
    }
    if (result == C1_CARD_STORAGE_CORRUPT) {
      CardSnapshotAddDamagedSlot(snapshot, slot);
      continue;
    }
    if (result != C1_CARD_STORAGE_OK) {
      return 1;
    }
    if (!CardPayloadValid(&payload)) {
      CardSnapshotAddDamagedSlot(snapshot, slot);
      continue;
    }

    progress = CardReadU32(&payload, C1_CARD_PROGRESS_OFFSET);
    snapshot->slot_valid[slot] = 1;
    snapshot->slot_map[snapshot->part_count] = slot;
    snapshot->partinfos[snapshot->part_count] =
      1u | 8u | (progress << 5) | 0x20000u;
    ++snapshot->part_count;
  }

  return 0;
}

static void CardPublishSnapshot(const c1_card_snapshot *snapshot) {
  /* Part count is the retail GOOL readiness marker, so publish it last. */
  memcpy(card_partinfos, snapshot->partinfos, sizeof(card_partinfos));
  memcpy(card_slot_map, snapshot->slot_map, sizeof(card_slot_map));
  memcpy(card_slot_valid, snapshot->slot_valid, sizeof(card_slot_valid));
  if (card_current_slot < 0 || card_current_slot >= C1_CARD_SLOT_COUNT ||
      !card_slot_valid[card_current_slot]) {
    card_current_slot = -1;
  }
  card_part_count_ro = snapshot->part_count;
}

static int CardRefreshMetadataImmediate(int clear_new_device) {
  c1_card_snapshot snapshot;

  if (CardBuildSnapshot(&snapshot) != 0) {
    CardClearPublishedMetadata();
    card_current_slot = -1;
    CardSetFailure(0);
    return 1;
  }

  CardPublishSnapshot(&snapshot);
  CardSetOperationSuccess(clear_new_device);
  return 0;
}

static void CardFinishRescan(void) {
  uint32_t flags;

  if (!card_scan_active) {
    return;
  }

  flags = card_flags_ro & C1_CARD_FLAG_NEW_DEVICE;
  CardPublishSnapshot(&card_staged_snapshot);
  CardCancelScan();
  card_flags_ro = flags;
}

void CardUpdate(void) {
  if (!card_scan_active) {
    return;
  }

  /* Keep CHECKING visible through one complete GOOL update. */
  if (card_flags_ro & C1_CARD_FLAG_CHECKING) {
    if (card_scan_ticks++ == 0) {
      return;
    }
    card_flags_ro &= ~C1_CARD_FLAG_CHECKING;
  }

  if (!(card_flags_ro & C1_CARD_FLAG_6)) {
    CardFinishRescan();
  }
}

static int CardResolveReadSlot(int part_idx) {
  if (part_idx < 0 || part_idx >= card_part_count_ro ||
      part_idx >= C1_CARD_SLOT_COUNT) {
    return -1;
  }
  return card_slot_map[part_idx];
}

static int CardResolveWriteSlot(int part_idx) {
  int slot;

  if (part_idx < 0 || part_idx >= C1_CARD_SLOT_COUNT) {
    return -1;
  }
  if (part_idx >= 0 && part_idx < card_part_count_ro &&
      part_idx < C1_CARD_SLOT_COUNT) {
    return card_slot_map[part_idx];
  }
  for (slot = 0; slot < C1_CARD_SLOT_COUNT; ++slot) {
    if (!card_slot_valid[slot]) {
      return slot;
    }
  }
  return -1;
}

static int CardSaveSlot(int slot) {
  c1_card_payload payload;

  if (slot < 0 || slot >= C1_CARD_SLOT_COUNT) {
    CardSetFailure(0);
    return 1;
  }

  card_flags_ro |= C1_CARD_FLAG_PENDING;
  CardCreatePayload(&payload);
  if (CardStorageWrite(slot, &payload) != C1_CARD_STORAGE_OK) {
    CardSetFailure(0);
    return 1;
  }

  card_current_slot = slot;
  return CardRefreshMetadataImmediate(1);
}

static int CardLoadSlot(int slot) {
  c1_card_payload payload;
  int result;

  if (slot < 0 || slot >= C1_CARD_SLOT_COUNT) {
    CardSetFailure(0);
    return 1;
  }

  card_flags_ro |= C1_CARD_FLAG_PENDING;
  result = CardStorageRead(slot, &payload);
  if (result != C1_CARD_STORAGE_OK || !CardPayloadValid(&payload)) {
    CardSetFailure(result == C1_CARD_STORAGE_CORRUPT ||
                   result == C1_CARD_STORAGE_OK);
    return 1;
  }

  CardRestorePayload(&payload);
  card_current_slot = slot;
  CardSetOperationSuccess(0);
  return 0;
}

int CardControl(int op, int part_idx) {
  int slot;

  switch (op) {
  case C1_CARD_OP_CLEAR_FLAG_6:
    /* Retail op 2 clears only this handshake latch. */
    card_flags_ro &= ~C1_CARD_FLAG_6;
    if (card_scan_active && !(card_flags_ro & C1_CARD_FLAG_CHECKING)) {
      CardFinishRescan();
    }
    return 0;

  case C1_CARD_OP_SAVE_SELECTED:
    if (card_flags_ro & (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_CHECK_NEEDED |
                         C1_CARD_FLAG_CHECKING)) {
      card_flags_ro |= C1_CARD_FLAG_ERROR;
      return 1;
    }
    return CardSaveSlot(CardResolveWriteSlot(part_idx));

  case C1_CARD_OP_LOAD_SELECTED:
    if (card_flags_ro & (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_CHECK_NEEDED |
                         C1_CARD_FLAG_CHECKING)) {
      card_flags_ro |= C1_CARD_FLAG_ERROR;
      return 1;
    }
    return CardLoadSlot(CardResolveReadSlot(part_idx));

  case C1_CARD_OP_FORMAT:
    if (card_flags_ro & (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_CHECKING)) {
      card_flags_ro |= C1_CARD_FLAG_ERROR;
      return 1;
    }
    card_flags_ro |= C1_CARD_FLAG_PENDING;
    if (CardStorageFormat() != C1_CARD_STORAGE_OK) {
      CardSetFailure(0);
      return 1;
    }
    card_current_slot = -1;
    CardCancelScan();
    CardClearPublishedMetadata();
    card_flags_ro = (card_flags_ro & C1_CARD_FLAG_NEW_DEVICE)
                  | C1_CARD_FLAG_CHECK_NEEDED;
    return 0;

  case C1_CARD_OP_SAVE_CURRENT:
    if (card_flags_ro & (C1_CARD_FLAG_PENDING | C1_CARD_FLAG_CHECK_NEEDED |
                         C1_CARD_FLAG_CHECKING)) {
      card_flags_ro |= C1_CARD_FLAG_ERROR;
      return 1;
    }
    slot = card_current_slot;
    if (slot < 0) {
      CardSetFailure(0);
      return 1;
    }
    return CardSaveSlot(slot);

  case C1_CARD_OP_PROBE_NAME:
    /* The browser card has no PSX directory name to invalidate. */
    return 1;

  case C1_CARD_OP_PROBE_PRESENT:
    return 0;

  case C1_CARD_OP_FORGET_CURRENT:
    card_current_slot = -1;
    return 0;

  case C1_CARD_OP_RESCAN:
    CardCancelScan();
    CardClearPublishedMetadata();
    if (CardBuildSnapshot(&card_staged_snapshot) != 0) {
      card_current_slot = -1;
      CardSetFailure(1);
      return 1;
    }
    card_scan_active = 1;
    card_scan_ticks = 0;
    card_flags_ro = (card_flags_ro & C1_CARD_FLAG_NEW_DEVICE)
                  | C1_CARD_FLAG_PENDING | C1_CARD_FLAG_CHECKING
                  | C1_CARD_FLAG_6;
    return 0;

  default:
    return 1;
  }
}
