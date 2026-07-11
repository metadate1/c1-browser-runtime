#include "audio.h"
#include "util.h"

#include <SDL2/SDL.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#define SVOICE_VOL_BASE 0x3FFF
#define SVOICE_PITCH_BASE 0x1000

typedef struct {
  int len;
  int loop_idx; /* negative index relative to sample end */
  int16_t data[];
} sample_t;

typedef struct {
  int done;
  double t;
  double amp[2];
  double freq;
  sample_t *sample;
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

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE uint32_t C1GetAudioCallbackCount(void) {
  return audio_callback_count;
}

EMSCRIPTEN_KEEPALIVE int C1GetAudioPeak(void) {
  return audio_last_peak;
}
#endif

static void SampleNext(sampler_t *sampler, int len, int16_t *data) {
  sample_t *sample;
  double t;
  int16_t *p, s;
  int loop_len;
  int i, idx;

  sample = sampler->sample;
  if (!sample) {
    memset(data, 0, len*2*sizeof(int16_t));
    return;
  }
  p = data;
  for (i=0;i<len;i++) {
    idx = (int)sampler->t;
    t = sampler->t - (double)idx;
    if (sample->loop_idx && idx >= sample->len) {
      /* add negative index to go back to loop start */
      sampler->t += (double)sample->loop_idx;
      idx = (int)sampler->t;
    }
    if (idx == sample->len-1) { --idx; t+=1.0; }
    else if (idx >= sample->len) {
      sampler->done=1;
      break;
    }
    s = sample->data[idx];
    s+=t*(sample->data[idx+1]-s);
    *(p++) = (int16_t)((double)s*sampler->amp[0]*m_amp);
    *(p++) = (int16_t)((double)s*sampler->amp[1]*m_amp);
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
  int32_t *p32, buf32[2048];
  int16_t *p16, buf16[2048];
  int i, j, len, peak;

  (void)userdata;

  len = size/(2*sizeof(int16_t));
  memset(buf32, 0, len*2*sizeof(int32_t));
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
    p32 = buf32;
    p16 = buf16;
    for (j=0;j<len;j++) {
      *(p32++) += ((int32_t)*(p16++))*svoice->gain;
      *(p32++) += ((int32_t)*(p16++))*svoice->gain;
    }
  }
  p32 = buf32;
  p16 = (int16_t*)stream;
  peak = 0;
  for (i=0;i<len;i++) {
    for (j=0;j<2;j++) {
      int32_t sample = *(p32++) / 24;
      int magnitude;
      if (sample > 32767) sample = 32767;
      if (sample < -32768) sample = -32768;
      *(p16++) = (int16_t)sample;
      magnitude = sample < 0 ? (int)-sample : (int)sample;
      if (magnitude > peak) peak = magnitude;
    }
  }
  audio_last_peak = peak;
  audio_callback_count++;
}

void SwAudioInit() {
  SDL_AudioSpec spec;
  svoice_t *voice;
  sampler_t *sampler;
  int i;

  m_amp = 1.0;
  audio_callback_count = 0;
  audio_last_peak = 0;
  for (i=0;i<24;i++)
    svoices[i] = def_svoice;
  SDL_zero(spec);
  spec.freq = 44100;
  spec.callback = (SDL_AudioCallback)AudioCallback;
  spec.format = AUDIO_S16;
  spec.samples = 1024;
  spec.channels = 2;
  if (SDL_OpenAudio(&spec, 0) < 0) {
    printf("Error initializing audio: %s\n", SDL_GetError());
    return;
  }
  SDL_PauseAudio(0);
}

void SwAudioKill() {
  int i;

  for (i=0;i<24;i++) {
    SwUnloadSample(i);
  }
  SDL_PauseAudio(1);
  SDL_CloseAudio();
}

void SwSetMVol(int32_t vol) {
  m_amp = (double)vol / 127.0;
}

uint8_t pcm_buf[0x80000];
void SwLoadSample(int voice_idx, uint8_t *data, size_t size) {
  svoice_t *svoice;
  sampler_t *sampler;
  sample_t *sample;
  int loop_offs, loop_idx;

  size -= 32;
  size = ADPCMToPCM16(data, size, pcm_buf, &loop_offs);
  data = pcm_buf;
  svoice = &svoices[voice_idx];
  sampler = &svoice->sampler;
  if (sampler->sample)
    SwUnloadSample(voice_idx);
  sample = (sample_t*)malloc(sizeof(sample_t)+size+sizeof(int16_t));
  sampler->sample = sample;
  sample->len = size/sizeof(int16_t);
  sample->loop_idx = 0;
  if (loop_offs != -1) {
    loop_idx = loop_offs/sizeof(int16_t);
    sample->loop_idx = loop_idx - sample->len;
  }
  memcpy(sample->data, data, size);
  /* used when interpolating looping samples */
  sample->data[sample->len] = sample->data[0];
}

void SwUnloadSample(int voice_idx) {
  svoice_t *svoice;
  sampler_t *sampler;

  svoice = &svoices[voice_idx];
  sampler = &svoice->sampler;
  if (sampler->sample)
    free(sampler->sample);
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
    status[i] = svoice->on;
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
