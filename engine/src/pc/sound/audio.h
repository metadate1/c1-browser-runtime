#ifndef _PC_AUDIO_H_
#define _PC_AUDIO_H_

#include "common.h"

extern void SwAudioInit();
extern void SwAudioKill();
extern void SwSetMVol(int32_t vol);
extern void SwLoadSample(int voice_idx, uint32_t eid, uint8_t *data, size_t size);
extern void SwUnloadSample(int voice_idx);
extern void SwNoteOn(int voice_idx);
extern void SwNoteOff(int voice_idx);
extern void SwVoiceSetVolume(int voice_idx, uint32_t voll, uint32_t volr);
extern void SwVoiceSetPitch(int voice_idx, uint32_t pitch);
extern void SwGetAllKeysStatus(uint8_t *status);

/* Software equivalents of the SPU key/envelope states used by AudioUpdate.
 * A completed one-shot remains distinguishable from a voice that has never
 * been keyed on so the high-level repeat counter is consumed exactly once. */
enum {
  SW_KEY_STATUS_OFF = 0,
  SW_KEY_STATUS_ON = 1,
  SW_KEY_STATUS_ON_ENV_OFF = 3
};

typedef void (*svoice_callback_t)(int id, float amp[2], float freq, int len, int16_t *data);
extern void SwVoiceSetCallback(int voice_idx, svoice_callback_t callback);
extern void SwVoiceSetGain(int voice_idx, float gain);

#endif /* _PC_AUDIO_H_ */
