#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "pc/sound/formats/psx.h"
#include "pc/sound/util.h"

int main(void) {
  VagLine lines[2];
  VagLine loop_lines[4];
  int16_t output[112];
  size_t bytes, decoded_size;
  uint8_t *large_adpcm, *large_pcm;
  int i, loop;

  memset(lines, 0, sizeof(lines));
  lines[0].factor = 0;
  lines[0].predict = 0;
  lines[0].flags = 1;
  lines[0].data[0].adl = 7;
  lines[0].data[0].adh = 8;
  bytes = ADPCMToPCM16((uint8_t*)lines, sizeof(VagLine),
    (uint8_t*)output, &loop);
  assert(bytes == 28 * sizeof(int16_t));
  assert(loop == -1);
  assert(output[0] == 28672);
  assert(output[1] == -32768);

  /* Predictor feedback must use the saturated 16-bit sample. Otherwise an
   * over-range intermediate wraps in output and poisons every later sample. */
  memset(lines, 0, sizeof(lines));
  for (i=0;i<14;i++) {
    lines[0].data[i].adl = 7;
    lines[0].data[i].adh = 7;
    lines[1].data[i].adl = 8;
    lines[1].data[i].adh = 8;
  }
  lines[0].predict = lines[1].predict = 4;
  lines[0].factor = lines[1].factor = 0;
  lines[1].flags = 1;
  bytes = ADPCMToPCM16((uint8_t*)lines, sizeof(lines),
    (uint8_t*)output, &loop);
  assert(bytes == 2 * 28 * sizeof(int16_t));
  assert(output[0] == 28672);
  assert(output[1] == 32767);
  assert(output[27] == 32767);
  assert(output[28] == -1025);
  assert(output[29] == -32768);
  assert(output[55] == -32768);

  /* A loop-start marker does not make a one-shot repeat unless its end block
   * also carries the repeat bit. */
  memset(loop_lines, 0, sizeof(loop_lines));
  loop_lines[0].flags = 4;
  loop_lines[2].flags = 1;
  loop_lines[3].flags = 7; /* unreachable retail sentinel */
  loop = 99;
  bytes = ADPCMToPCM16((uint8_t*)loop_lines, sizeof(loop_lines),
    (uint8_t*)output, &loop);
  assert(bytes == 3 * 28 * sizeof(int16_t));
  assert(loop == -1);

  /* End+repeat loops from the latest loop-start marker, including one at the
   * first decoded block. */
  memset(loop_lines, 0, sizeof(loop_lines));
  loop_lines[0].flags = 6;
  loop_lines[2].flags = 3;
  loop = -1;
  bytes = ADPCMToPCM16((uint8_t*)loop_lines, sizeof(loop_lines),
    (uint8_t*)output, &loop);
  assert(bytes == 3 * 28 * sizeof(int16_t));
  assert(loop == 0);

  memset(loop_lines, 0, sizeof(loop_lines));
  loop_lines[0].flags = 6;
  loop_lines[1].flags = 6;
  loop_lines[3].flags = 3;
  loop = -1;
  bytes = ADPCMToPCM16((uint8_t*)loop_lines, sizeof(loop_lines),
    (uint8_t*)output, &loop);
  assert(bytes == sizeof(loop_lines) / sizeof(VagLine)
    * 28 * sizeof(int16_t));
  assert(loop == 28 * (int)sizeof(int16_t));

  /* The runtime allocates from the exact expansion size instead of decoding
   * into the old fixed 512 KiB scratch buffer. */
  assert(ADPCMDecodedSize(150000, &decoded_size));
  assert(decoded_size == (150000 / sizeof(VagLine)) * 28 * sizeof(int16_t));
  assert(decoded_size > 0x80000);
  assert(!ADPCMDecodedSize(SIZE_MAX, 0));
  large_adpcm = (uint8_t*)calloc(1, 150000);
  large_pcm = (uint8_t*)malloc(decoded_size);
  assert(large_adpcm && large_pcm);
  assert(ADPCMToPCM16(large_adpcm, 150000, large_pcm, 0) == decoded_size);
  free(large_pcm);
  free(large_adpcm);
  return 0;
}
