#ifndef C1_TEST_SDL_H
#define C1_TEST_SDL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef void (*SDL_AudioCallback)(void *userdata, Uint8 *stream, size_t len);

typedef struct SDL_AudioSpec {
  int freq;
  Uint16 format;
  Uint8 channels;
  Uint16 samples;
  SDL_AudioCallback callback;
  void *userdata;
} SDL_AudioSpec;

#define AUDIO_S16 0x8010
#define SDL_zero(value) memset(&(value), 0, sizeof(value))

int SDL_OpenAudio(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
void SDL_PauseAudio(int pause_on);
void SDL_CloseAudio(void);
const char *SDL_GetError(void);

#endif
