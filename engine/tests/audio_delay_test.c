#include <assert.h>
#include <stdint.h>
#include <string.h>

/* Include the implementation so the regression test can inspect the retail
 * voice bookkeeping while still exercising AudioControl and AudioUpdate. */
#include "../src/audio.c"

gool_globals globals;
ns_struct ns;
lid_t cur_lid;
entry *cur_zone;

int32_t ramp_rate;
uint32_t init_vol;
int32_t fade_vol;
int32_t fade_vol_step;
int max_midi_voices;
uint32_t seq2_vol;

static uint8_t low_level_keys[24];
static int note_on_count;
static int note_off_count;
static int last_note_on_voice;
static int last_note_off_voice;

void MidiResetSeqVol(int16_t val) { (void)val; }
void MidiUpdate(void *ref) { (void)ref; }
void SwMidiProcess(int ch, float amp[2], float freq, int len, int16_t *data) {
  (void)ch;
  (void)amp;
  (void)freq;
  (void)len;
  (void)data;
}
void SwAudioInit(void) {}
void SwAudioKill(void) {}
void SwSetMVol(int32_t vol) { (void)vol; }
void SwVoiceSetCallback(int voice_idx, svoice_callback_t callback) {
  (void)voice_idx;
  (void)callback;
}
void SwVoiceSetGain(int voice_idx, float gain) {
  (void)voice_idx;
  (void)gain;
}

void SwGetAllKeysStatus(uint8_t *status) {
  memcpy(status, low_level_keys, sizeof(low_level_keys));
}

void SwNoteOn(int voice_idx) {
  low_level_keys[voice_idx] = 1;
  note_on_count++;
  last_note_on_voice = voice_idx;
}

void SwNoteOff(int voice_idx) {
  low_level_keys[voice_idx] = 0;
  note_off_count++;
  last_note_off_voice = voice_idx;
}

void SwVoiceSetVolume(int voice_idx, uint32_t voll, uint32_t volr) {
  (void)voice_idx;
  (void)voll;
  (void)volr;
}

void SwVoiceSetPitch(int voice_idx, uint32_t pitch) {
  (void)voice_idx;
  (void)pitch;
}

int GoolTransform2(vec *in, vec *out, int flag) {
  (void)flag;
  *out = *in;
  return 0;
}

uint32_t SwSqrMagnitude2(int32_t a, int32_t b) {
  (void)a;
  (void)b;
  return 0;
}

uint32_t SwSqrMagnitude3(int32_t a, int32_t b, int32_t c) {
  (void)a;
  (void)b;
  (void)c;
  return 0;
}

int16_t mcos(uint32_t angle) {
  (void)angle;
  return 0;
}

int32_t matan2(int32_t y, int32_t x) {
  (void)y;
  (void)x;
  return 0;
}

static void assert_audio_init_restores_first_voice_template(void) {
  const int voice_idx = 16;
  const uint32_t poisoned_flags = 0xA5A5AFFFu;

  memset(voices, 0, sizeof(voices));
  memset(keys_status, 0, sizeof(keys_status));
  memset(low_level_keys, 0, sizeof(low_level_keys));
  memset(&voice_params, 0xA5, sizeof(voice_params));
  voice_params.flags = poisoned_flags;
  completed_sample_rekey_count = 99;

  assert(AudioInit() == SUCCESS);
  assert(completed_sample_rekey_count == 0);
  assert(voice_params.delay_counter == 1);
  assert((uint8_t)voice_params.sustain_counter == 128u);
  assert(voice_params.amplitude == 0x3FFF);
  assert(voice_params.pitch == 0x1000);
  assert(voice_params.obj == 0);
  assert(voice_params.case7val == 0);
  assert(voice_params.r_trans.x == 0);
  assert(voice_params.r_trans.y == 0);
  assert(voice_params.r_trans.z == 0);
  assert(voice_params.flags == ((poisoned_flags & 0xFFFFF000u) | 0x600u));

  /* The first voice created after AudioInit receives this template. Its
   * completed one-shot must consume the default count of one and be freed,
   * rather than wrapping an uninitialized zero to 255 and re-keying. */
  voices[voice_idx].params = voice_params;
  voices[voice_idx].params.flags |= 8;
  low_level_keys[voice_idx] = SW_KEY_STATUS_ON_ENV_OFF;
  note_on_count = note_off_count = 0;
  last_note_on_voice = last_note_off_voice = -1;

  AudioUpdate();
  assert(note_on_count == 0);
  assert(note_off_count == 1);
  assert(last_note_off_voice == voice_idx);
  assert(voices[voice_idx].params.delay_counter == 0);
  assert(!(voices[voice_idx].params.flags & 8));
}

static void reset_delayed_voice(int voice_idx, uint16_t delay) {
  generic arg;

  memset(voices, 0, sizeof(voices));
  memset(keys_status, 0, sizeof(keys_status));
  memset(low_level_keys, 0, sizeof(low_level_keys));
  memset(&voice_params, 0, sizeof(voice_params));
  voices[voice_idx].id = 1;
  voices[voice_idx].params.delay_counter = 1;
  voices[voice_idx].params.sustain_counter = INT8_MIN;
  voices[voice_idx].params.flags = 0x600;
  arg.u32 = delay;
  AudioControl(voices[voice_idx].id, 7, &arg, 0);
  voices[voice_idx].params.flags |= 8;
  note_on_count = 0;
  note_off_count = 0;
  last_note_on_voice = -1;
  last_note_off_voice = -1;
}

static void assert_delayed_start(uint16_t delay) {
  const int voice_idx = 16;
  uint16_t frame;

  reset_delayed_voice(voice_idx, delay);
  assert(voices[voice_idx].params.case7val == delay);

  for (frame = 1; frame < delay; frame++) {
    AudioUpdate();
    assert(note_on_count == 0);
    assert(voices[voice_idx].params.flags & 8);
    assert(voices[voice_idx].params.flags & 0x10);
  }

  AudioUpdate();
  assert(note_on_count == 1);
  assert(completed_sample_rekey_count == 0);
  assert(last_note_on_voice == voice_idx);
  assert(low_level_keys[voice_idx] == 1);
  assert(voices[voice_idx].params.flags & 8);
  assert(!(voices[voice_idx].params.flags & 0x10));

  /* SwGetAllKeysStatus ran before the key-on above. The just-started voice
   * must survive that stale snapshot and remain allocated on the next tick. */
  AudioUpdate();
  assert(note_on_count == 1);
  assert(voices[voice_idx].params.flags & 8);
}

static void assert_active_voice_has_no_artificial_timeout(void) {
  const int voice_idx = 16;
  const int8_t allocation_priority = INT8_MIN;
  int frame;

  memset(voices, 0, sizeof(voices));
  memset(keys_status, 0, sizeof(keys_status));
  memset(low_level_keys, 0, sizeof(low_level_keys));
  voices[voice_idx].params.flags = 8;
  voices[voice_idx].params.delay_counter = 1;
  voices[voice_idx].params.sustain_counter = allocation_priority;
  low_level_keys[voice_idx] = SW_KEY_STATUS_ON;
  note_on_count = note_off_count = 0;

  for (frame = 0; frame < 256; frame++) {
    AudioUpdate();
    assert(voices[voice_idx].params.flags & 8);
    assert(voices[voice_idx].params.sustain_counter == allocation_priority);
    assert(low_level_keys[voice_idx] == SW_KEY_STATUS_ON);
  }
  assert(note_on_count == 0);
  assert(note_off_count == 0);
}

static void assert_completed_one_shot_frees_once(void) {
  const int voice_idx = 16;

  memset(voices, 0, sizeof(voices));
  memset(keys_status, 0, sizeof(keys_status));
  memset(low_level_keys, 0, sizeof(low_level_keys));
  voices[voice_idx].params.flags = 8;
  voices[voice_idx].params.delay_counter = 1;
  low_level_keys[voice_idx] = SW_KEY_STATUS_ON_ENV_OFF;
  note_on_count = note_off_count = 0;
  last_note_off_voice = -1;

  AudioUpdate();
  assert(note_on_count == 0);
  assert(note_off_count == 1);
  assert(last_note_off_voice == voice_idx);
  assert(voices[voice_idx].params.delay_counter == 0);
  assert(!(voices[voice_idx].params.flags & 8));

  AudioUpdate();
  assert(note_off_count == 1);
  assert(voices[voice_idx].params.delay_counter == 0);
}

static void assert_completed_sample_repeats(void) {
  const int voice_idx = 16;

  memset(voices, 0, sizeof(voices));
  memset(keys_status, 0, sizeof(keys_status));
  memset(low_level_keys, 0, sizeof(low_level_keys));
  voices[voice_idx].params.flags = 8;
  voices[voice_idx].params.delay_counter = 3;
  low_level_keys[voice_idx] = SW_KEY_STATUS_ON_ENV_OFF;
  note_on_count = note_off_count = 0;
  last_note_on_voice = last_note_off_voice = -1;

  AudioUpdate();
  assert(note_on_count == 1);
  assert(completed_sample_rekey_count == 1);
  assert(last_note_on_voice == voice_idx);
  assert(note_off_count == 0);
  assert(voices[voice_idx].params.delay_counter == 2);
  assert(voices[voice_idx].params.flags & 8);
  assert(low_level_keys[voice_idx] == SW_KEY_STATUS_ON);

  /* Normal active playback does not consume another repeat. */
  AudioUpdate();
  assert(note_on_count == 1);
  assert(completed_sample_rekey_count == 1);
  assert(voices[voice_idx].params.delay_counter == 2);

  low_level_keys[voice_idx] = SW_KEY_STATUS_ON_ENV_OFF;
  AudioUpdate();
  assert(note_on_count == 2);
  assert(completed_sample_rekey_count == 2);
  assert(voices[voice_idx].params.delay_counter == 1);
  assert(voices[voice_idx].params.flags & 8);

  low_level_keys[voice_idx] = SW_KEY_STATUS_ON_ENV_OFF;
  AudioUpdate();
  assert(note_on_count == 2);
  assert(completed_sample_rekey_count == 2);
  assert(note_off_count == 1);
  assert(last_note_off_voice == voice_idx);
  assert(voices[voice_idx].params.delay_counter == 0);
  assert(!(voices[voice_idx].params.flags & 8));
}

int main(void) {
  static const uint16_t intro_delays[] = { 15, 147, 450, 585, 816 };
  nsd_ldat ldat;
  size_t i;

  memset(&globals, 0, sizeof(globals));
  memset(&ns, 0, sizeof(ns));
  memset(&ldat, 0, sizeof(ldat));
  ldat.lid = LID_INTRO;
  ns.ldat = &ldat;
  cur_zone = 0;
  max_midi_voices = 16;
  fade_vol_step = 0;

  assert_audio_init_restores_first_voice_template();
  for (i = 0; i < sizeof(intro_delays) / sizeof(intro_delays[0]); i++)
    assert_delayed_start(intro_delays[i]);
  assert_active_voice_has_no_artificial_timeout();
  assert_completed_one_shot_frees_once();
  assert_completed_sample_repeats();
  return 0;
}
