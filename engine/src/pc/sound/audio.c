#include "audio.h"
#include "util.h"

#include <SDL2/SDL.h>
#include <limits.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define SVOICE_VOL_BASE 0x3FFF
#define SVOICE_PITCH_BASE 0x1000
#define AUDIO_MIX_DIVISOR 12
#define AUDIO_MAX_FRAMES 1024
#define SAMPLE_CACHE_ENTRY_COUNT 128
#define SAMPLE_CACHE_MAX_BYTES (8u*1024u*1024u)
typedef struct {
  int len;
  int loop_idx; /* negative index relative to sample end */
  int16_t data[];
} sample_t;

typedef struct sample_cache_entry {
  uint32_t eid;
  sample_t *sample;
  size_t bytes;
  uint32_t last_used;
  uint16_t refs;
  uint8_t used;
} sample_cache_entry;

typedef struct {
  int done;
  double t;
  double amp[2];
  double freq;
  sample_t *sample;
  sample_cache_entry *cache_entry;
} sampler_t;

typedef struct {
  int on;
  int16_t vol[2];
  int pitch;
  sampler_t sampler;
  svoice_callback_t callback;
  float gain;
} svoice_t;

const sampler_t def_sampler = {
  .amp = { 1.0, 1.0 }
};
const svoice_t def_svoice = {
  .vol = { SVOICE_VOL_BASE, SVOICE_VOL_BASE },
  .pitch = SVOICE_PITCH_BASE,
  .sampler = def_sampler,
  .gain = 1.0
};

double m_amp = 1.0;
svoice_t svoices[24];
static volatile uint32_t audio_callback_count;
static volatile int audio_last_peak;
static volatile uint32_t audio_clip_count;
static volatile uint32_t audio_deadline_miss_count;
static volatile uint32_t audio_max_gap_us;
static volatile uint32_t audio_max_callback_us;
static volatile int audio_music_peak;
static volatile int audio_sfx_peak;
static volatile int audio_music_rms;
static volatile int audio_sfx_rms;
static volatile int audio_active_sfx;
static sample_cache_entry sample_cache[SAMPLE_CACHE_ENTRY_COUNT];
static size_t sample_cache_bytes;
static uint32_t sample_cache_clock;
static volatile uint32_t sample_cache_hits;
static volatile uint32_t sample_cache_misses;
#ifdef __EMSCRIPTEN__
static double audio_last_callback_ms;
static double audio_expected_callback_ms;
static int audio_device_open;
#endif

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE uint32_t C1GetAudioCallbackCount(void) {
  return audio_callback_count;
}

EMSCRIPTEN_KEEPALIVE int C1GetAudioPeak(void) {
  return audio_last_peak;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetAudioClipCount(void) {
  return audio_clip_count;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetAudioDeadlineMissCount(void) {
  return audio_deadline_miss_count;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetAudioMaxGapUs(void) {
  return audio_max_gap_us;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetAudioMaxCallbackUs(void) {
  return audio_max_callback_us;
}

EMSCRIPTEN_KEEPALIVE int C1GetAudioMusicPeak(void) {
  return audio_music_peak;
}

EMSCRIPTEN_KEEPALIVE int C1GetAudioSfxPeak(void) {
  return audio_sfx_peak;
}

EMSCRIPTEN_KEEPALIVE int C1GetAudioMusicRms(void) {
  return audio_music_rms;
}

EMSCRIPTEN_KEEPALIVE int C1GetAudioSfxRms(void) {
  return audio_sfx_rms;
}

EMSCRIPTEN_KEEPALIVE int C1GetAudioActiveSfx(void) {
  return audio_active_sfx;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetSampleCacheHits(void) {
  return sample_cache_hits;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetSampleCacheMisses(void) {
  return sample_cache_misses;
}

EMSCRIPTEN_KEEPALIVE uint32_t C1GetSampleCacheBytes(void) {
  return (uint32_t)sample_cache_bytes;
}
#endif

static void SampleCacheRemove(sample_cache_entry *entry) {
  if (!entry || !entry->used || entry->refs) return;
  free(entry->sample);
  if (sample_cache_bytes >= entry->bytes) sample_cache_bytes -= entry->bytes;
  else sample_cache_bytes = 0;
  memset(entry, 0, sizeof(*entry));
}

static void SampleCacheClear(void) {
  int i;
  for (i = 0; i < SAMPLE_CACHE_ENTRY_COUNT; i++) {
    sample_cache[i].refs = 0;
    SampleCacheRemove(&sample_cache[i]);
  }
  sample_cache_bytes = 0;
  sample_cache_clock = 0;
}

static sample_cache_entry *SampleCacheFind(uint32_t eid) {
  int i;
  for (i = 0; i < SAMPLE_CACHE_ENTRY_COUNT; i++) {
    if (sample_cache[i].used && sample_cache[i].eid == eid)
      return &sample_cache[i];
  }
  return 0;
}

static sample_cache_entry *SampleCacheReserve(size_t bytes) {
  sample_cache_entry *oldest;
  int i;

  if (bytes > SAMPLE_CACHE_MAX_BYTES) return 0;
  for (;;) {
    for (i = 0; i < SAMPLE_CACHE_ENTRY_COUNT; i++) {
      if (!sample_cache[i].used && sample_cache_bytes + bytes <= SAMPLE_CACHE_MAX_BYTES)
        return &sample_cache[i];
    }
    oldest = 0;
    for (i = 0; i < SAMPLE_CACHE_ENTRY_COUNT; i++) {
      if (sample_cache[i].used && !sample_cache[i].refs
       && (!oldest || sample_cache[i].last_used < oldest->last_used))
        oldest = &sample_cache[i];
    }
    if (!oldest) return 0;
    SampleCacheRemove(oldest);
  }
}

static void SampleNext(sampler_t *sampler, int len, int16_t *data) {
  sample_t *sample;
  double t;
  int16_t *p, s, next_s;
  int loop_len, loop_start;
  int i, idx, next_idx;

  sample = sampler->sample;
  if (!sample) {
    memset(data, 0, len*2*sizeof(int16_t));
    return;
  }
  p = data;
  for (i=0;i<len;i++) {
    idx = (int)sampler->t;
    t = sampler->t - (double)idx;
    if (sample->loop_idx) {
      loop_start = sample->len + sample->loop_idx;
      loop_len = sample->len - loop_start;
      while (idx >= sample->len && loop_len > 0) {
        sampler->t -= (double)loop_len;
        idx = (int)sampler->t;
        t = sampler->t - (double)idx;
      }
    }
    if (idx >= sample->len) {
      sampler->done=1;
      break;
    }
    s = sample->data[idx];
    next_idx = idx + 1;
    if (next_idx < sample->len)
      next_s = sample->data[next_idx];
    else if (sample->loop_idx)
      next_s = sample->data[sample->len + sample->loop_idx];
    else
      next_s = s;
    s += t*(next_s-s);
    *(p++) = (int16_t)((double)s*sampler->amp[0]);
    *(p++) = (int16_t)((double)s*sampler->amp[1]);
    sampler->t += sampler->freq;
  }
  for (;i<len;i++) {
    *(p++) = 0;
    *(p++) = 0;
  }
}

static void AudioCallback(void *userdata, uint8_t *stream, size_t size) {
  svoice_t *svoice;
  sampler_t *sampler;
  int32_t *p32, music32[AUDIO_MAX_FRAMES*2], sfx32[AUDIO_MAX_FRAMES*2];
  int16_t *p16, buf16[AUDIO_MAX_FRAMES*2];
  int64_t music_square_sum, sfx_square_sum;
  int i, j, len, peak, music_peak, sfx_peak, active_sfx;
#ifdef __EMSCRIPTEN__
  double callback_start_ms, callback_end_ms, gap_ms;
#endif

  (void)userdata;
#ifdef __EMSCRIPTEN__
  callback_start_ms = emscripten_get_now();
  if (audio_last_callback_ms > 0.0) {
    gap_ms = callback_start_ms - audio_last_callback_ms;
    if ((uint32_t)(gap_ms * 1000.0) > audio_max_gap_us)
      audio_max_gap_us = (uint32_t)(gap_ms * 1000.0);
    if (audio_expected_callback_ms > 0.0
     && gap_ms > audio_expected_callback_ms * 1.05)
      audio_deadline_miss_count++;
  }
  audio_last_callback_ms = callback_start_ms;
#endif
  len = size/(2*sizeof(int16_t));
  if (len > AUDIO_MAX_FRAMES) {
    memset(stream, 0, size);
    audio_deadline_miss_count++;
    return;
  }
  memset(music32, 0, len*2*sizeof(int32_t));
  memset(sfx32, 0, len*2*sizeof(int32_t));
  active_sfx = 0;
  for (i=0;i<24;i++) {
    svoice = &svoices[i];
    if (!svoice->on) { continue; }
    if (!svoice->callback) {
      sampler = &svoice->sampler;
      SampleNext(sampler, len, buf16);
      if (sampler->done)
        svoice->on = 0;
    }
    else { /* sampler override */
      svoice_callback_t callback;
      float amp[2], freq;
      callback = svoice->callback;
      amp[0] = (double)svoice->vol[0] / SVOICE_VOL_BASE;
      amp[1] = (double)svoice->vol[1] / SVOICE_VOL_BASE;
      freq = (double)svoice->pitch / SVOICE_PITCH_BASE;
      callback(i, amp, freq, len, buf16);
    }
    p32 = i == 0 ? music32 : sfx32;
    if (i != 0) active_sfx++;
    p16 = buf16;
    for (j=0;j<len;j++) {
      *(p32++) += ((int32_t)*(p16++))*svoice->gain;
      *(p32++) += ((int32_t)*(p16++))*svoice->gain;
    }
  }
  p16 = (int16_t*)stream;
  peak = 0;
  music_peak = 0;
  sfx_peak = 0;
  music_square_sum = 0;
  sfx_square_sum = 0;
  for (i=0;i<len;i++) {
    for (j=0;j<2;j++) {
      int index = i*2+j;
      int32_t music_sample = music32[index] / AUDIO_MIX_DIVISOR;
      int32_t sfx_sample = sfx32[index] / AUDIO_MIX_DIVISOR;
      int32_t sample = (int32_t)((music_sample + sfx_sample) * m_amp);
      int magnitude, music_magnitude, sfx_magnitude;
      music_magnitude = music_sample < 0 ? -music_sample : music_sample;
      sfx_magnitude = sfx_sample < 0 ? -sfx_sample : sfx_sample;
      if (music_magnitude > music_peak) music_peak = music_magnitude;
      if (sfx_magnitude > sfx_peak) sfx_peak = sfx_magnitude;
      music_square_sum += (int64_t)music_sample * music_sample;
      sfx_square_sum += (int64_t)sfx_sample * sfx_sample;
      if (sample > 32767) { sample = 32767; audio_clip_count++; }
      if (sample < -32768) { sample = -32768; audio_clip_count++; }
      *(p16++) = (int16_t)sample;
      magnitude = sample < 0 ? (int)-sample : (int)sample;
      if (magnitude > peak) peak = magnitude;
    }
  }
  audio_last_peak = peak;
  audio_music_peak = music_peak;
  audio_sfx_peak = sfx_peak;
  audio_music_rms = (int)__builtin_sqrt((double)music_square_sum / (len*2));
  audio_sfx_rms = (int)__builtin_sqrt((double)sfx_square_sum / (len*2));
  audio_active_sfx = active_sfx;
  audio_callback_count++;
#ifdef __EMSCRIPTEN__
  callback_end_ms = emscripten_get_now();
  if ((uint32_t)((callback_end_ms-callback_start_ms)*1000.0) > audio_max_callback_us)
    audio_max_callback_us = (uint32_t)((callback_end_ms-callback_start_ms)*1000.0);
#endif
}

void SwAudioInit() {
  SDL_AudioSpec spec;
  svoice_t *voice;
  sampler_t *sampler;
  int i;

  m_amp = 1.0;
  audio_callback_count = 0;
  audio_last_peak = 0;
  audio_clip_count = 0;
  audio_deadline_miss_count = 0;
  audio_max_gap_us = 0;
  audio_max_callback_us = 0;
  audio_music_peak = audio_sfx_peak = 0;
  audio_music_rms = audio_sfx_rms = 0;
  audio_active_sfx = 0;
  SampleCacheClear();
  sample_cache_hits = 0;
  sample_cache_misses = 0;
#ifdef __EMSCRIPTEN__
  audio_last_callback_ms = 0.0;
  audio_expected_callback_ms = 1000.0 * AUDIO_MAX_FRAMES / 44100.0;
#endif
  for (i=0;i<24;i++)
    svoices[i] = def_svoice;
#ifdef __EMSCRIPTEN__
  if (audio_device_open) {
    /* Level changes happen inside one game frame. Keep the browser's
     * AudioContext/device alive instead of destroying and recreating it for
     * every NS transition; AudioKill has already paused and drained voices. */
    SDL_PauseAudio(0);
    return;
  }
#endif
  SDL_zero(spec);
  spec.freq = 44100;
  spec.callback = (SDL_AudioCallback)AudioCallback;
  spec.format = AUDIO_S16;
  spec.samples = AUDIO_MAX_FRAMES;
  spec.channels = 2;
  if (SDL_OpenAudio(&spec, 0) < 0) {
    printf("Error initializing audio: %s\n", SDL_GetError());
    return;
  }
#ifdef __EMSCRIPTEN__
  audio_device_open = 1;
#endif
  SDL_PauseAudio(0);
}

void SwAudioKill() {
  int i;

  SDL_PauseAudio(1);
  for (i=0;i<24;i++) {
    SwUnloadSample(i);
  }
  SampleCacheClear();
#ifndef __EMSCRIPTEN__
  SDL_CloseAudio();
#endif
}

void SwSetMVol(int32_t vol) {
  if (vol < 0) vol = 0;
  if (vol > SVOICE_VOL_BASE) vol = SVOICE_VOL_BASE;
  m_amp = (double)vol / SVOICE_VOL_BASE;
}

void SwLoadSample(int voice_idx, uint32_t eid, uint8_t *data, size_t size) {
  svoice_t *svoice;
  sampler_t *sampler;
  sample_t *sample;
  sample_cache_entry *cache_entry;
  uint8_t *decoded;
  size_t allocation_size, decoded_capacity;
  int loop_offs, loop_idx;

  svoice = &svoices[voice_idx];
  sampler = &svoice->sampler;
  svoice->on = 0;
  sampler->done = 0;
  sampler->t = 0;
  if (sampler->sample)
    SwUnloadSample(voice_idx);
  cache_entry = SampleCacheFind(eid);
  if (cache_entry) {
    cache_entry->refs++;
    cache_entry->last_used = ++sample_cache_clock;
    sampler->sample = cache_entry->sample;
    sampler->cache_entry = cache_entry;
    sample_cache_hits++;
    return;
  }
  sample_cache_misses++;
  if (!data || size < ADPCM_BLOCK_SIZE) return;
  /* Decode the exact SPU payload. ADPCMToPCM16 stops at the first end marker,
   * so any trailing sentinel is unreachable just as it is on the console. */
  if (!ADPCMDecodedSize(size, &decoded_capacity)
   || decoded_capacity < 2*sizeof(int16_t)
   || decoded_capacity / sizeof(int16_t) > INT_MAX) return;
  decoded = (uint8_t*)malloc(decoded_capacity);
  if (!decoded) return;
  size = ADPCMToPCM16(data, size, decoded, &loop_offs);
  if (size < 2*sizeof(int16_t)
   || size / sizeof(int16_t) > INT_MAX
   || size > SIZE_MAX - sizeof(sample_t) - sizeof(int16_t)) {
    free(decoded);
    return;
  }
  allocation_size = sizeof(sample_t)+size+sizeof(int16_t);
  sample = (sample_t*)malloc(allocation_size);
  if (!sample) {
    free(decoded);
    return;
  }
  sampler->sample = sample;
  sampler->cache_entry = 0;
  sample->len = size/sizeof(int16_t);
  sample->loop_idx = 0;
  if (loop_offs != -1) {
    loop_idx = loop_offs/sizeof(int16_t);
    sample->loop_idx = loop_idx - sample->len;
  }
  memcpy(sample->data, decoded, size);
  free(decoded);
  sample->data[sample->len] = sample->data[sample->len-1];
  cache_entry = SampleCacheReserve(allocation_size);
  if (cache_entry) {
    cache_entry->eid = eid;
    cache_entry->sample = sample;
    cache_entry->bytes = allocation_size;
    cache_entry->last_used = ++sample_cache_clock;
    cache_entry->refs = 1;
    cache_entry->used = 1;
    sample_cache_bytes += allocation_size;
    sampler->cache_entry = cache_entry;
  }
}

void SwUnloadSample(int voice_idx) {
  svoice_t *svoice;
  sampler_t *sampler;

  svoice = &svoices[voice_idx];
  sampler = &svoice->sampler;
  if (sampler->sample) {
    if (sampler->cache_entry) {
      if (sampler->cache_entry->refs) sampler->cache_entry->refs--;
      sampler->cache_entry->last_used = ++sample_cache_clock;
    }
    else {
      free(sampler->sample);
    }
  }
  sampler->sample = 0;
  sampler->cache_entry = 0;
}

void SwNoteOn(int voice_idx) {
  svoice_t *svoice;
  sampler_t *sampler;

  svoice = &svoices[voice_idx];
  if (!svoice->on) {
    svoice->on = 1;
    sampler = &svoice->sampler;
    sampler->done = 0;
    sampler->t = 0;
    sampler->amp[0] = (double)svoice->vol[0] / SVOICE_VOL_BASE;
    sampler->amp[1] = (double)svoice->vol[1] / SVOICE_VOL_BASE;
    sampler->freq = (double)svoice->pitch / SVOICE_PITCH_BASE;
  }
}

void SwNoteOff(int voice_idx) {
  svoice_t *svoice;

  svoice = &svoices[voice_idx];
  if (svoice->on) {
    svoice->on = 0;
  }
}

void SwVoiceSetVolume(int voice_idx, uint32_t voll, uint32_t volr) {
  svoice_t *svoice;
  sampler_t *sampler;

  svoice = &svoices[voice_idx];
  sampler = &svoice->sampler;
  svoice->vol[0] = limit(voll,0,0x3FFF);
  svoice->vol[1] = limit(volr,0,0x3FFF);
  sampler->amp[0] = (double)svoice->vol[0] / SVOICE_VOL_BASE;
  sampler->amp[1] = (double)svoice->vol[1] / SVOICE_VOL_BASE;
}

void SwVoiceSetPitch(int voice_idx, uint32_t pitch) {
  svoice_t *svoice;
  sampler_t *sampler;

  svoice = &svoices[voice_idx];
  sampler = &svoice->sampler;
  svoice->pitch = pitch;
  sampler->freq = (double)svoice->pitch / SVOICE_PITCH_BASE;
}

void SwGetAllKeysStatus(uint8_t *status) {
  svoice_t *svoice;
  int i;

  for (i=0;i<24;i++) {
    svoice = &svoices[i];
    if (svoice->sampler.done)
      status[i] = SW_KEY_STATUS_ON_ENV_OFF;
    else
      status[i] = svoice->on ? SW_KEY_STATUS_ON : SW_KEY_STATUS_OFF;
  }
}

void SwVoiceSetCallback(int voice_idx, svoice_callback_t callback) {
  svoice_t *svoice;

  svoice = &svoices[voice_idx];
  svoice->on = 1;
  svoice->callback = callback;
}

void SwVoiceSetGain(int voice_idx, float gain) {
  svoice_t *svoice;

  svoice = &svoices[voice_idx];
  svoice->gain = gain;
}
