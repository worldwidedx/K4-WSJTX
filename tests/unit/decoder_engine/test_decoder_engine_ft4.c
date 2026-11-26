#include "DecoderEngine.h"
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); \
  } \
} while (0)

extern void decoder_engine_test_message(int mode, const char text[37], int16_t samples[180000],
                                       int8_t payload[77], int8_t tones[103], int start_sample);
extern void decoder_engine_test_legacy_isolation(void);
extern void decoder_engine_test_gui_codec(void);

typedef struct {
  decoder_engine_handle engine;
  decoder_attempt_request request;
  decoder_audio_view audio;
  const char *expected_message;
  int8_t payload[77], tones[103];
  float start;
  int count, found;
} fixture;

static void observe(const decoder_observation *record, void *opaque)
{
  fixture *f = opaque;
  CHECK(record->input_id == f->request.input_id);
  CHECK(record->analysis_id == f->request.analysis_id);
  CHECK(record->attempt_no == f->request.attempt_no);
  CHECK(record->mode == f->request.mode);
  ++f->count;
  if (strcmp(record->message, f->expected_message)) return;
  ++f->found;
  CHECK(fabsf(record->frequency_hz - 1500.0f) < 2.0f);
  if (record->mode == DECODER_MODE_FT4) {
    CHECK(record->ft4.payload_origin == DECODER_PAYLOAD_DECODED);
    CHECK(record->ft4.has_tones && record->ft4.has_waveform_start);
    CHECK(!memcmp(record->ft4.payload77, f->payload, 77));
    CHECK(!memcmp(record->ft4.tones, f->tones, 103));
    CHECK(fabsf(record->ft4.waveform_start_seconds - f->start) < 0.01f);
    CHECK(fabsf(record->dt_seconds - (f->start + 0.048f - 0.5f)) < 0.01f);
    CHECK(record->ft8.payload_origin == DECODER_PAYLOAD_NONE && record->superfox.kind == 0);
  } else {
    CHECK(!memcmp(record->ft8.payload77, f->payload, 77));
    CHECK(!memcmp(record->ft8.tones, f->tones, 79));
    CHECK(record->ft4.payload_origin == DECODER_PAYLOAD_NONE);
  }
  CHECK(decoder_engine_release_input(f->engine, record->input_id) == DECODER_BUSY);
}

static void prepare(fixture *f, int mode, const char *message, float start)
{
  static int64_t next_id = 100;
  char padded[37];
  memset(padded, ' ', sizeof padded);
  memcpy(padded, message, strlen(message));
  memset(&f->request, 0, sizeof f->request);
  f->request.input_id = ++next_id;
  f->request.analysis_id = next_id;
  f->request.attempt_no = 1;
  f->request.mode = mode;
  f->request.phase = DECODER_PHASE_NORMAL;
  f->request.source = DECODER_SOURCE_FILE;
  f->request.ft4.utc = 120000;
  f->request.ft4.receive_frequency_hz = 1500;
  f->request.ft4.search_low_hz = 200;
  f->request.ft4.search_high_hz = 3500;
  f->request.ft4.depth = 3;
  memset(f->request.ft4.mycall, ' ', 12);
  memset(f->request.ft4.hiscall, ' ', 12);
  f->request.ft8.half_symbol_stage = 50;
  f->request.ft8.utc = 120000;
  f->request.ft8.receive_frequency_hz = 1500;
  f->request.ft8.transmit_frequency_hz = 1500;
  f->request.ft8.search_low_hz = 1300;
  f->request.ft8.search_high_hz = 1800;
  f->request.ft8.depth = 3;
  f->request.ft8.cycles = 1;
  f->request.ft8.threads = 2;
  f->request.ft8.candidate_thinning = 100;
  f->request.ft8.receive_sensitivity = 3;
  f->request.ft8.low_threshold = 1;
  f->request.ft8.subtract_pass = 1;
  memset(f->request.ft8.mycall, ' ', 12);
  memset(f->request.ft8.hiscall, ' ', 12);
  memset(f->request.ft8.my_base_call, ' ', 12);
  memset(f->request.ft8.his_base_call, ' ', 12);
  memset(f->request.ft8.mygrid, ' ', 6);
  memset(f->request.ft8.hisgrid, ' ', 6);
  f->audio.sample_count = mode == DECODER_MODE_FT4 ? 72576 : 180000;
  f->audio.sample_rate_hz = 12000;
  f->expected_message = message;
  f->start = start;
  decoder_engine_test_message(mode, padded, (int16_t *)f->audio.samples, f->payload, f->tones,
                              (int)lroundf(start * 12000.0f));
}

static void decode(fixture *f)
{
  decoder_attempt_outcome outcome;
  int16_t before[180000];
  memcpy(before, f->audio.samples, sizeof before);
  f->count = f->found = 0;
  CHECK(decoder_engine_decode(f->engine, &f->request, &f->audio, observe, f, &outcome) == DECODER_OK);
  if (!f->found) fprintf(stderr, "Missing mode %d message: %s (start %.3f, depth %d, input %lld, observations %d)\n",
                        f->request.mode, f->expected_message, f->start, f->request.ft4.depth,
                        (long long)f->request.input_id, f->count);
  CHECK(f->found > 0 && f->count == outcome.observation_count);
  CHECK(outcome.retained_count >= f->count && outcome.evidence_dropped == 0);
  CHECK(!memcmp(before, f->audio.samples, sizeof before));
}

static void decode_silence(fixture *f, int retained_count)
{
  decoder_attempt_outcome outcome;
  f->count = f->found = 0;
  CHECK(feclearexcept(FE_ALL_EXCEPT) == 0);
  CHECK(decoder_engine_decode(f->engine, &f->request, &f->audio, observe, f, &outcome) == DECODER_OK);
  CHECK(fetestexcept(FE_DIVBYZERO | FE_INVALID) == 0);
  CHECK(outcome.status == DECODER_OK && outcome.observation_count == 0);
  CHECK(f->count == 0 && outcome.retained_count == retained_count);
  CHECK(outcome.evidence_dropped == 0);
}

int main(void)
{
  decoder_engine_options options = {DECODER_ENGINE_ABI};
  decoder_attempt_outcome outcome;
  int16_t samples[180000];
  fixture f = {0};
  f.audio.samples = samples;
  CHECK(decoder_engine_create(&options, &f.engine) == DECODER_OK);

  prepare(&f, DECODER_MODE_FT4, "CQ K1JT FN20", 0.452f);
  memset(samples, 0, sizeof samples);
  decode_silence(&f, 0);
  CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);

  prepare(&f, DECODER_MODE_FT4, "CQ K1JT FN20", 0.452f);
  decode(&f);
  int retained_count = f.count;
  f.request.phase = DECODER_PHASE_REPEAT;
  ++f.request.attempt_no;
  f.audio.sample_count = 1;
  samples[0] = 0;
  /* A decodable signal beyond the valid prefix must not reach the decoder. */
  decode_silence(&f, retained_count);
  ++f.request.attempt_no;
  f.audio.sample_count = 72576;
  memset(samples, 0, sizeof samples);
  decode_silence(&f, retained_count);
  CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);

  for (int depth = 1; depth <= 3; ++depth) {
    prepare(&f, DECODER_MODE_FT4, "CQ K1JT FN20", 0.452f);
    f.request.ft4.depth = depth;
    decode(&f);
    f.request.phase = DECODER_PHASE_REPEAT;
    ++f.request.analysis_id;
    ++f.request.attempt_no;
    decode(&f);
    f.request.mode = DECODER_MODE_FT8;
    ++f.request.attempt_no;
    CHECK(decoder_engine_decode(f.engine, &f.request, &f.audio, observe, &f, &outcome) == DECODER_INVALID);
    CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);
  }

  /* Repeat with a short valid prefix and poisoned storage beyond it. */
  prepare(&f, DECODER_MODE_FT4, "CQ K1JT FN20", -0.048f);
  f.audio.sample_count = 64000;
  for (int i = 64000; i < 180000; ++i) samples[i] = 32767;
  decode(&f);
  CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);

  for (int mtd = 0; mtd <= 1; ++mtd) {
    CHECK(decoder_engine_reset_session(f.engine) == DECODER_OK);
    prepare(&f, DECODER_MODE_FT4, "CQ K1JT FN20", 0.452f);
    decode(&f);
    CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);
    prepare(&f, DECODER_MODE_FT8, "PJ2/W1AW <K1JT> RR73", 0.5f);
    f.request.ft8.mtd = mtd;
    decode(&f);
    CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);

    CHECK(decoder_engine_reset_session(f.engine) == DECODER_OK);
    prepare(&f, DECODER_MODE_FT8, "CQ K1JT FN20", 0.5f);
    f.request.ft8.mtd = mtd;
    decode(&f);
    CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);
    prepare(&f, DECODER_MODE_FT4, "PJ2/W1AW <K1JT> RR73", 0.452f);
    decode(&f);
    CHECK(decoder_engine_reset_session(f.engine) == DECODER_OK);
    f.expected_message = "PJ2/W1AW <...> RR73";
    decode(&f);
    CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);
  }

  /* Contest configurations must remain usable across repeated calls and resets. */
  for (int contest = 3; contest <= 4; ++contest) {
    prepare(&f, DECODER_MODE_FT4, "CQ K1JT FN20", 0.452f);
    f.request.ft4.contest = contest;
    memcpy(f.request.ft4.mycall, "W9XYZ", 5);
    memcpy(f.request.ft4.hiscall, "K1JT", 4);
    decode(&f);
    ++f.request.attempt_no;
    decode(&f);
    CHECK(decoder_engine_reset_session(f.engine) == DECODER_OK);
    decode(&f);
    CHECK(decoder_engine_release_input(f.engine, f.request.input_id) == DECODER_OK);
  }
  decoder_engine_test_legacy_isolation();
  CHECK(decoder_engine_destroy(f.engine) == DECODER_OK);
  CHECK(decoder_engine_create(&options, &f.engine) == DECODER_OK);
  prepare(&f, DECODER_MODE_FT4, "CQ K1JT FN20", 0.452f);
  decode(&f);
  CHECK(decoder_engine_destroy(f.engine) == DECODER_OK);
  decoder_engine_test_gui_codec();
  return EXIT_SUCCESS;
}
