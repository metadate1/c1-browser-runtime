#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pc/sound/formats/psx.h"

/* Include the implementation to exercise the private decoded-sample and
 * interpolation state without opening a host audio device. */
#include "../src/pc/sound/audio.c"

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained) {
  (void)desired;
  (void)obtained;
  return 0;
}

void SDL_PauseAudio(int pause_on) { (void)pause_on; }
void SDL_CloseAudio(void) {}
const char *SDL_GetError(void) { return "test"; }

static void assert_loader_keeps_terminal_and_loop_metadata(void) {
  VagLine one_shot[5];
  VagLine looped[5];
  sample_t *sample;

  memset(one_shot, 0, sizeof(one_shot));
  one_shot[1].flags = 4;
  one_shot[3].flags = 1;
  one_shot[3].data[13].adh = 1;
  one_shot[4].flags = 7;
  SwLoadSample(1, 0x11111111u, (uint8_t*)one_shot, sizeof(one_shot));
  sample = svoices[1].sampler.sample;
  assert(sample);
  assert(sample->len == 4 * 28);
  assert(sample->loop_idx == 0);
  assert(sample->data[sample->len-1] == 4096);

  memset(looped, 0, sizeof(looped));
  looped[1].flags = 2;
  looped[2].flags = 6;
  looped[3].flags = 2;
  looped[4].flags = 3; /* terminal-last payload (no sentinel) */
  SwLoadSample(2, 0x22222221u, (uint8_t*)looped, sizeof(looped));
  sample = svoices[2].sampler.sample;
  assert(sample);
  assert(sample->len == 5 * 28);
  assert(sample->len + sample->loop_idx == 2 * 28);

  SwUnloadSample(1);
  SwUnloadSample(2);
  SampleCacheClear();
}

static sample_t *make_interpolation_sample(int loop_start) {
  sample_t *sample = (sample_t*)calloc(1,
    sizeof(sample_t) + 5 * sizeof(int16_t));
  assert(sample);
  sample->len = 4;
  sample->loop_idx = loop_start >= 0 ? loop_start - sample->len : 0;
  sample->data[0] = 0;
  sample->data[1] = 1000;
  sample->data[2] = 2000;
  sample->data[3] = 3000;
  return sample;
}

static void assert_fractional_loop_boundary_interpolation(void) {
  sampler_t sampler = def_sampler;
  int16_t output[4];

  sampler.sample = make_interpolation_sample(1);
  sampler.t = 3.5;
  sampler.freq = 1.0;
  SampleNext(&sampler, 2, output);
  assert(output[0] == 2000);
  assert(output[1] == 2000);
  assert(output[2] == 1500);
  assert(output[3] == 1500);
  assert(!sampler.done);
  assert(sampler.t == 2.5);
  free(sampler.sample);

  sampler = def_sampler;
  sampler.sample = make_interpolation_sample(-1);
  sampler.t = 3.5;
  sampler.freq = 1.0;
  SampleNext(&sampler, 2, output);
  assert(output[0] == 3000);
  assert(output[1] == 3000);
  assert(output[2] == 0);
  assert(output[3] == 0);
  assert(sampler.done);
  free(sampler.sample);
}

int main(void) {
  assert_loader_keeps_terminal_and_loop_metadata();
  assert_fractional_loop_boundary_interpolation();
  return 0;
}
