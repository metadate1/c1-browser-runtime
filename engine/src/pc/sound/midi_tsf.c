/*
 * Browser-friendly MIDI backend using TinySoundFont and TinyMidiLoader.
 *
 * The game still supplies its original PlayStation VAB/SEP data.  The
 * existing conversion helpers turn those into an in-memory SF2 and SMF,
 * which are then rendered synchronously by SwMidiProcess at 44.1 kHz.
 */
#include "midi.h"
#include "util.h"
#include "formats/psx.h"

/* engine/src/math.h shadows the C math header on the project's include path. */
#define TSF_POW __builtin_pow
#define TSF_POWF __builtin_powf
#define TSF_EXPF __builtin_expf
#define TSF_LOG __builtin_log
#define TSF_TAN __builtin_tan
#define TSF_LOG10 __builtin_log10
#define TSF_SQRT __builtin_sqrt
#define TSF_SQRTF __builtin_sqrtf
#define TSF_NO_STDIO
#define TSF_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnull-pointer-subtraction"
#endif
#include "../../third_party/tinysoundfont/tsf.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#define TML_NO_STDIO
#define TML_IMPLEMENTATION
#include "../../third_party/tinysoundfont/tml.h"

#define MIDI_SEQUENCE_COUNT 2
#define MIDI_CHANNEL_COUNT 16
#define MIDI_SAMPLE_RATE 44100
#define MIDI_TICKS_PER_SECOND 30
#define MIDI_RENDER_BLOCK 512
#define MIDI_MAX_VOICES 64
#define MIDI_SF2_BUFFER_SIZE 0x400000
#define MIDI_SMF_BUFFER_SIZE 0x800000
#define MIDI_REWIND_BLOCKS 4
#define MIDI_OUTPUT_GAIN_DB -10.0f

extern uint8_t *vabs[16];

typedef enum {
  MIDI_STOPPED,
  MIDI_PLAYING,
  MIDI_PAUSED,
  MIDI_TAIL
} midi_state_t;

typedef struct {
  tsf *synth;
  tml_message *messages;
  tml_message *next_message;
  uint64_t sample_position;
  uint64_t end_sample;
  int loops_remaining;
  midi_state_t state;
  float volume[2];
  float fade_target[2];
  float fade_step[2];
  uint64_t fade_frames_remaining;
} midi_context_t;

static midi_context_t midi_contexts[MIDI_SEQUENCE_COUNT];
static int midi_initialized;

static float midi_clamp_volume(float volume) {
  if (volume < 0.0f)
    return 0.0f;
  if (volume > 1.0f)
    return 1.0f;
  return volume;
}

static int16_t midi_clamp_sample(float sample) {
  if (sample > 32767.0f)
    return 32767;
  if (sample < -32768.0f)
    return -32768;
  return (int16_t)sample;
}

static void midi_context_set_defaults(midi_context_t *context) {
  memset(context, 0, sizeof(*context));
  context->state = MIDI_STOPPED;
  context->volume[0] = 1.0f;
  context->volume[1] = 1.0f;
}

static void midi_context_release(midi_context_t *context) {
  if (context->messages)
    tml_free(context->messages);
  if (context->synth)
    tsf_close(context->synth);
  midi_context_set_defaults(context);
}

static int midi_valid_sequence(int seq_num) {
  return seq_num >= 0 && seq_num < MIDI_SEQUENCE_COUNT;
}

static uint64_t midi_message_sample(const tml_message *message) {
  return (((uint64_t)message->time * MIDI_SAMPLE_RATE) + 500) / 1000;
}

static uint64_t midi_find_end_sample(const tml_message *message) {
  uint64_t end_sample = 1;

  for (; message; message = message->next) {
    uint64_t event_sample = midi_message_sample(message);
    if (event_sample >= end_sample)
      end_sample = event_sample + 1;
  }
  return end_sample;
}

static int midi_prepare_synth(tsf *synth) {
  tsf_set_output(synth, TSF_STEREO_INTERLEAVED, MIDI_SAMPLE_RATE,
    MIDI_OUTPUT_GAIN_DB);
  if (!tsf_set_max_voices(synth, MIDI_MAX_VOICES))
    return 0;

  /* Allocate every MIDI channel before rendering starts. */
  return tsf_channel_set_presetindex(synth, MIDI_CHANNEL_COUNT - 1, 0);
}

static void midi_all_notes_off(midi_context_t *context) {
  int channel;

  if (!context->synth)
    return;
  for (channel = 0; channel < MIDI_CHANNEL_COUNT; ++channel)
    tsf_channel_note_off_all(context->synth, channel);
}

static void midi_rewind_synth(midi_context_t *context) {
  short discard[MIDI_RENDER_BLOCK * 2];
  int channel;
  int block;

  if (!context->synth)
    return;

  for (channel = 0; channel < MIDI_CHANNEL_COUNT; ++channel)
    tsf_channel_sounds_off_all(context->synth, channel);

  /* TinySoundFont uses a short anti-click release for "sounds off". */
  for (block = 0;
       block < MIDI_REWIND_BLOCKS && tsf_active_voice_count(context->synth);
       ++block)
    tsf_render_short(context->synth, discard, MIDI_RENDER_BLOCK, 0);

  for (channel = 0; channel < MIDI_CHANNEL_COUNT; ++channel) {
    tsf_channel_midi_control(context->synth, channel, TML_ALL_CTRL_OFF, 0);
    tsf_channel_set_bank(context->synth, channel, 0);
    tsf_channel_set_presetnumber(context->synth, channel, 0, channel == 9);
  }
}

static void midi_rewind_sequence(midi_context_t *context) {
  midi_rewind_synth(context);
  context->next_message = context->messages;
  context->sample_position = 0;
}

static void midi_dispatch_message(midi_context_t *context,
                                  const tml_message *message) {
  int channel = message->channel;

  switch (message->type) {
  case TML_PROGRAM_CHANGE:
    tsf_channel_set_presetnumber(context->synth, channel,
      (unsigned char)message->program, channel == 9);
    break;
  case TML_NOTE_ON:
    if ((unsigned char)message->velocity == 0)
      tsf_channel_note_off(context->synth, channel,
        (unsigned char)message->key);
    else
      tsf_channel_note_on(context->synth, channel,
        (unsigned char)message->key,
        (unsigned char)message->velocity / 127.0f);
    break;
  case TML_NOTE_OFF:
    tsf_channel_note_off(context->synth, channel,
      (unsigned char)message->key);
    break;
  case TML_CONTROL_CHANGE:
    tsf_channel_midi_control(context->synth, channel,
      (unsigned char)message->control,
      (unsigned char)message->control_value);
    break;
  case TML_PITCH_BEND:
    tsf_channel_set_pitchwheel(context->synth, channel,
      message->pitch_bend);
    break;
  default:
    /* Tempo has already been folded into tml_message::time. */
    break;
  }
}

static void midi_finish_sequence_cycle(midi_context_t *context) {
  if (context->loops_remaining != 0) {
    if (context->loops_remaining > 0)
      --context->loops_remaining;
    midi_rewind_sequence(context);
    return;
  }

  midi_all_notes_off(context);
  context->state = MIDI_TAIL;
}

static void midi_render_context(midi_context_t *context, int frames,
                                short *output) {
  int rendered = 0;

  memset(output, 0, (size_t)frames * 2 * sizeof(*output));
  if (!context->synth || !context->messages)
    return;

  while (rendered < frames) {
    int block_frames;
    uint64_t boundary;
    uint64_t available;

    if (context->state == MIDI_STOPPED || context->state == MIDI_PAUSED)
      return;

    if (context->state == MIDI_TAIL) {
      if (!tsf_active_voice_count(context->synth)) {
        context->state = MIDI_STOPPED;
        return;
      }
      block_frames = frames - rendered;
      tsf_render_short(context->synth, output + (rendered * 2),
        block_frames, 0);
      if (!tsf_active_voice_count(context->synth))
        context->state = MIDI_STOPPED;
      return;
    }

    while (context->next_message &&
           midi_message_sample(context->next_message) <=
             context->sample_position) {
      midi_dispatch_message(context, context->next_message);
      context->next_message = context->next_message->next;
    }

    if (context->next_message)
      boundary = midi_message_sample(context->next_message);
    else
      boundary = context->end_sample;

    if (boundary <= context->sample_position) {
      midi_finish_sequence_cycle(context);
      continue;
    }

    available = boundary - context->sample_position;
    block_frames = frames - rendered;
    if ((uint64_t)block_frames > available)
      block_frames = (int)available;

    tsf_render_short(context->synth, output + (rendered * 2),
      block_frames, 0);
    context->sample_position += (uint64_t)block_frames;
    rendered += block_frames;
  }
}

static void midi_advance_fade(midi_context_t *context) {
  if (!context->fade_frames_remaining)
    return;

  context->volume[0] += context->fade_step[0];
  context->volume[1] += context->fade_step[1];
  --context->fade_frames_remaining;
  if (!context->fade_frames_remaining) {
    context->volume[0] = context->fade_target[0];
    context->volume[1] = context->fade_target[1];
  }
}

static void midi_start_fade(midi_context_t *context, float amount, int ticks,
                            int direction) {
  uint64_t frames;
  int side;

  amount = midi_clamp_volume(amount);
  for (side = 0; side < 2; ++side)
    context->fade_target[side] = midi_clamp_volume(
      context->volume[side] + (direction * amount));

  if (ticks <= 0) {
    context->volume[0] = context->fade_target[0];
    context->volume[1] = context->fade_target[1];
    context->fade_frames_remaining = 0;
    return;
  }

  frames = ((uint64_t)ticks * MIDI_SAMPLE_RATE) / MIDI_TICKS_PER_SECOND;
  if (!frames)
    frames = 1;
  context->fade_frames_remaining = frames;
  for (side = 0; side < 2; ++side)
    context->fade_step[side] =
      (context->fade_target[side] - context->volume[side]) / (float)frames;
}

void SwMidiProcess(int ch, float amp[2], float freq, int len, int16_t *data) {
  int32_t mixed[MIDI_RENDER_BLOCK * 2];
  short rendered[MIDI_RENDER_BLOCK * 2];
  float output_volume[2] = { 1.0f, 1.0f };
  int offset = 0;
  int context_idx;
  int frame;

  (void)ch;
  (void)freq;
  if (!data || len <= 0)
    return;
  if (amp) {
    output_volume[0] = midi_clamp_volume(amp[0]);
    output_volume[1] = midi_clamp_volume(amp[1]);
  }

  while (offset < len) {
    int block_frames = len - offset;
    if (block_frames > MIDI_RENDER_BLOCK)
      block_frames = MIDI_RENDER_BLOCK;
    memset(mixed, 0, (size_t)block_frames * 2 * sizeof(*mixed));

    for (context_idx = 0; context_idx < MIDI_SEQUENCE_COUNT;
         ++context_idx) {
      midi_context_t *context = &midi_contexts[context_idx];
      midi_render_context(context, block_frames, rendered);
      for (frame = 0; frame < block_frames; ++frame) {
        mixed[frame * 2] += (int32_t)
          (rendered[frame * 2] * context->volume[0]);
        mixed[(frame * 2) + 1] += (int32_t)
          (rendered[(frame * 2) + 1] * context->volume[1]);
        midi_advance_fade(context);
      }
    }

    for (frame = 0; frame < block_frames; ++frame) {
      data[(offset + frame) * 2] = midi_clamp_sample(
        mixed[frame * 2] * output_volume[0]);
      data[((offset + frame) * 2) + 1] = midi_clamp_sample(
        mixed[(frame * 2) + 1] * output_volume[1]);
    }
    offset += block_frames;
  }
}

void SwMidiInit(void) {
  int i;

  if (midi_initialized)
    SwMidiKill();
  for (i = 0; i < MIDI_SEQUENCE_COUNT; ++i)
    midi_context_set_defaults(&midi_contexts[i]);
  midi_initialized = 1;
}

void SwMidiKill(void) {
  int i;

  for (i = 0; i < MIDI_SEQUENCE_COUNT; ++i)
    midi_context_release(&midi_contexts[i]);
  midi_initialized = 0;
}

int16_t SwSepOpen(uint8_t *sep, int vab_id, int count) {
  PThd *sep_header;
  tsf *base_synth;
  uint8_t *sf2_data;
  uint8_t *smf_data;
  uint8_t *seq_data;
  size_t sf2_size;
  int i;

  if (!midi_initialized)
    SwMidiInit();
  SwSepClose(0);

  if (!sep || vab_id < 0 || vab_id >= 16 || !vabs[vab_id] ||
      count <= 0 || count > MIDI_SEQUENCE_COUNT)
    return -1;

  sf2_data = (uint8_t*)malloc(MIDI_SF2_BUFFER_SIZE);
  smf_data = (uint8_t*)malloc(MIDI_SMF_BUFFER_SIZE);
  if (!sf2_data || !smf_data) {
    free(sf2_data);
    free(smf_data);
    return -1;
  }

  sf2_size = VabToSf2(vabs[vab_id], sf2_data);
  base_synth = tsf_load_memory(sf2_data, (int)sf2_size);
  free(sf2_data);
  if (!base_synth) {
    free(smf_data);
    return -1;
  }

  sep_header = (PThd*)sep;
  seq_data = sep_header->data;
  for (i = 0; i < count; ++i) {
    midi_context_t *context = &midi_contexts[i];
    size_t seq_size = 0;
    size_t smf_size = SeqToMid(seq_data, smf_data, &seq_size);

    context->synth = (i == 0) ? base_synth : tsf_copy(base_synth);
    if (!context->synth || !midi_prepare_synth(context->synth))
      goto open_failed;

    context->messages = tml_load_memory(smf_data, (int)smf_size);
    if (!context->messages || !seq_size)
      goto open_failed;
    context->next_message = context->messages;
    context->end_sample = midi_find_end_sample(context->messages);
    context->state = MIDI_STOPPED;
    seq_data += seq_size;
  }

  free(smf_data);
  return 0;

open_failed:
  /* If the first context was not assigned, it still owns base_synth. */
  if (!midi_contexts[0].synth)
    tsf_close(base_synth);
  free(smf_data);
  for (i = 0; i < MIDI_SEQUENCE_COUNT; ++i)
    midi_context_release(&midi_contexts[i]);
  return -1;
}

void SwSepClose(int san) {
  int i;

  (void)san;
  for (i = 0; i < MIDI_SEQUENCE_COUNT; ++i)
    midi_context_release(&midi_contexts[i]);
}

void SwSepSetVol(int san, int seq_num, uint32_t voll, uint32_t volr) {
  midi_context_t *context;

  (void)san;
  if (!midi_valid_sequence(seq_num))
    return;
  context = &midi_contexts[seq_num];
  context->volume[0] = midi_clamp_volume(voll / 127.0f);
  context->volume[1] = midi_clamp_volume(volr / 127.0f);
  context->fade_frames_remaining = 0;
}

void SwSepPlay(int san, int seq_num, int mode, int loops) {
  midi_context_t *context;

  (void)san;
  if (!midi_valid_sequence(seq_num))
    return;
  context = &midi_contexts[seq_num];
  if (!context->synth || !context->messages)
    return;

  context->loops_remaining = (mode == 1 || loops < 0) ? -1 : loops;
  midi_rewind_sequence(context);
  context->state = MIDI_PLAYING;
}

void SwSepStop(int san, int seq_num) {
  midi_context_t *context;

  (void)san;
  if (!midi_valid_sequence(seq_num))
    return;
  context = &midi_contexts[seq_num];
  if (!context->synth)
    return;

  midi_rewind_sequence(context);
  context->state = MIDI_STOPPED;
  context->fade_frames_remaining = 0;
}

void SwSepPause(int san, int seq_num) {
  midi_context_t *context;

  (void)san;
  if (!midi_valid_sequence(seq_num))
    return;
  context = &midi_contexts[seq_num];
  if (context->state == MIDI_PLAYING || context->state == MIDI_TAIL)
    context->state = MIDI_PAUSED;
}

void SwSepReplay(int san, int seq_num) {
  midi_context_t *context;

  (void)san;
  if (!midi_valid_sequence(seq_num))
    return;
  context = &midi_contexts[seq_num];
  if (context->synth && context->messages &&
      context->state == MIDI_PAUSED)
    context->state = MIDI_PLAYING;
}

void SwSepSetCrescendo(int san, int seq_num, uint32_t vol, int ticks) {
  (void)san;
  if (!midi_valid_sequence(seq_num))
    return;
  midi_start_fade(&midi_contexts[seq_num], vol / 127.0f, ticks, 1);
}

void SwSepSetDecrescendo(int san, int seq_num, uint32_t vol, int ticks) {
  (void)san;
  if (!midi_valid_sequence(seq_num))
    return;
  midi_start_fade(&midi_contexts[seq_num], vol / 127.0f, ticks, -1);
}
