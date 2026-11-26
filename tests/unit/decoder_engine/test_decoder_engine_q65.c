#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include "DecoderEngine.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif
#ifdef __linux__
#include <link.h>
#endif

#define CHECK(condition) do { \
  if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
    exit(EXIT_FAILURE); \
  } \
} while (0)

enum { SAMPLE_CAPACITY = 300 * 12000 };
extern void decoder_engine_test_q65_signal(int, int, int, int, float, float, float, int16_t *);
extern void decoder_engine_test_q65_message(const char *, int, int, int, int, float, float, float, int16_t *);

typedef struct {
  decoder_engine_handle engine;
  decoder_attempt_request request;
  decoder_audio_view audio;
  int16_t *samples, *before;
  int count, bit78, max_average, method;
  const char *expected[2];
  float frequencies[2];
  int seen[2];
  char message[38];
} fixture;

static void observe(const decoder_observation *record, void *opaque)
{
  fixture *f = opaque;
  CHECK(record->input_id == f->request.input_id);
  CHECK(record->analysis_id == f->request.analysis_id);
  CHECK(record->attempt_no == f->request.attempt_no);
  CHECK(record->mode == DECODER_MODE_Q65);
  CHECK(record->variant == f->request.q65.submode);
  CHECK(record->q65.period_seconds == f->request.q65.period_seconds);
  CHECK(record->q65.recovered_bit78 == f->bit78);
  CHECK(record->q65.method >= 0);
  CHECK(record->q65.average_count >= 1);
  CHECK(isfinite(record->sync) && isfinite(record->dt_seconds));
  CHECK(record->ft8.payload_origin == DECODER_PAYLOAD_NONE);
  CHECK(record->ft4.payload_origin == DECODER_PAYLOAD_NONE);
  int candidate = 0;
  if (f->expected[0]) {
    while (candidate < 2 && (!f->expected[candidate] ||
           strcmp(record->message, f->expected[candidate]))) ++candidate;
    if (candidate == 2) fprintf(stderr, "unexpected Q65 message [%s]\n", record->message);
    CHECK(candidate < 2);
  }
  CHECK(!f->seen[candidate]++);
  CHECK(fabsf(record->frequency_hz - f->frequencies[candidate]) <= 10.0f);
  snprintf(f->message, sizeof f->message, "%s", record->message);
  CHECK(fabsf(record->dt_seconds) <= 0.3f);
  ++f->count;
  if (record->q65.average_count > f->max_average) f->max_average = record->q65.average_count;
  f->method = record->q65.method;
  CHECK(decoder_engine_release_input(f->engine, record->input_id) == DECODER_BUSY);
  CHECK(decoder_engine_clear_q65_averages(f->engine) == DECODER_BUSY);
  CHECK(decoder_engine_set_q65_callers(f->engine, NULL, 0) == DECODER_BUSY);
}

static void initialize_request(fixture *f, int period, int submode, int utc, int bit78)
{
  static int64_t next_id = 200;
  memset(&f->request, 0, sizeof f->request);
  f->request.input_id = ++next_id;
  f->request.analysis_id = next_id;
  f->request.attempt_no = 1;
  f->request.mode = DECODER_MODE_Q65;
  f->request.phase = DECODER_PHASE_NORMAL;
  f->request.source = DECODER_SOURCE_FILE;
  f->request.q65.utc = utc;
  f->request.q65.period_seconds = period;
  f->request.q65.submode = submode;
  f->request.q65.receive_frequency_hz = 1000;
  f->request.q65.tolerance_hz = 50;
  f->request.q65.search_low_hz = 850;
  f->request.q65.search_high_hz = 1150;
  f->request.q65.depth = 1;
  f->request.q65.single_decode = 1;
  f->request.q65.extended_eme_search = 1;
  f->request.q65.now_seconds = 1700000000;
  memset(f->request.q65.mycall, ' ', sizeof f->request.q65.mycall);
  memset(f->request.q65.hiscall, ' ', sizeof f->request.q65.hiscall);
  memset(f->request.q65.hisgrid, ' ', sizeof f->request.q65.hisgrid);
  f->bit78 = bit78;
  f->expected[0] = "K1ABC W9XYZ FN42";
  f->expected[1] = NULL;
  f->frequencies[0] = 1000.0f;
  f->audio.sample_count = period * 12000;
  f->audio.sample_rate_hz = 12000;
}

static void prepare(fixture *f, int period, int submode, int utc, int bit78)
{
  initialize_request(f, period, submode, utc, bit78);
  decoder_engine_test_q65_signal(period, submode, bit78, utc + 1, 300.0f, 100.0f, 1000.0f, f->samples);
}

static void decode(fixture *f, int expect_message)
{
  decoder_attempt_outcome outcome;
  decoder_attempt_request before_request = f->request;
  size_t bytes = f->audio.sample_count * sizeof *f->samples;
  memcpy(f->before, f->samples, bytes);
  f->count = f->max_average = 0;
  memset(f->seen, 0, sizeof f->seen);
  memset(f->message, 0, sizeof f->message);
  f->method = -1;
  CHECK(decoder_engine_decode(f->engine, &f->request, &f->audio, observe, f, &outcome) == DECODER_OK);
  CHECK(outcome.status == DECODER_OK && outcome.observation_count == f->count);
  CHECK(outcome.evidence_dropped == 0);
  CHECK(!memcmp(&before_request, &f->request, sizeof before_request));
  CHECK(!memcmp(f->before, f->samples, bytes));
  if (expect_message >= 0) {
    if (!!f->count != !!expect_message)
      fprintf(stderr, "Q65-%d%c UTC %06d attempt %d: got %d messages, expected %d\n",
              f->request.q65.period_seconds, 'A' + f->request.q65.submode,
              f->request.q65.utc, f->request.attempt_no, f->count, expect_message);
    CHECK(!!f->count == !!expect_message);
  }
}

static void counts(fixture *f, int even, int odd)
{
  decoder_q65_snapshot snapshot;
  CHECK(decoder_engine_get_q65_snapshot(f->engine, &snapshot, NULL, NULL, 0) == DECODER_OK);
  if (snapshot.even_count != even || snapshot.odd_count != odd)
    fprintf(stderr, "Q65 averages: got %d/%d, expected %d/%d\n",
            snapshot.even_count, snapshot.odd_count, even, odd);
  CHECK(snapshot.even_count == even && snapshot.odd_count == odd);
  CHECK(snapshot.curve_count >= 0);
  if (snapshot.curve_count) {
    float *instant = malloc(snapshot.curve_count * sizeof *instant);
    float *averaged = malloc(snapshot.curve_count * sizeof *averaged);
    CHECK(instant && averaged);
    CHECK(decoder_engine_get_q65_snapshot(f->engine, &snapshot, instant, averaged, snapshot.curve_count) == DECODER_OK);
    for (int i = 0; i < snapshot.curve_count; ++i) CHECK(isfinite(instant[i]) && isfinite(averaged[i]));
    free(instant);
    free(averaged);
  }
}

static void release(fixture *f)
{
  CHECK(decoder_engine_release_input(f->engine, f->request.input_id) == DECODER_OK);
}

static void repeat(fixture *f)
{
  f->request.phase = DECODER_PHASE_REPEAT;
  ++f->request.analysis_id;
  ++f->request.attempt_no;
}

static void signal_message(fixture *f, const char *message, int seed, float amplitude,
                           float noise, float frequency, int16_t *samples)
{
  char padded[37];
  CHECK(strlen(message) <= sizeof padded);
  memset(padded, ' ', sizeof padded);
  memcpy(padded, message, strlen(message));
  decoder_engine_test_q65_message(padded, f->request.q65.period_seconds,
                                 f->request.q65.submode, f->bit78, seed,
                                 amplitude, noise, frequency, samples);
}

static void retained_options(fixture *f)
{
  CHECK(decoder_engine_reset_session(f->engine) == DECODER_OK);
  prepare(f, 15, 0, 0, 0);
  f->request.q65.receive_frequency_hz = 1500;
  f->request.q65.search_low_hz = 1450;
  f->request.q65.search_high_hz = 1550;
  decode(f, 0);
  repeat(f);
  f->request.q65.receive_frequency_hz = 1000;
  f->request.q65.search_low_hz = 950;
  f->request.q65.search_high_hz = 1050;
  decode(f, 1);
  CHECK(f->count == 1);
  int method = f->method;
  release(f);
  CHECK(decoder_engine_reset_session(f->engine) == DECODER_OK);
  prepare(f, 15, 0, 0, 0);
  f->request.q65.search_low_hz = 950;
  f->request.q65.search_high_hz = 1050;
  decode(f, 1);
  CHECK(f->count == 1 && f->method == method);
  release(f);
}

static void knowledge_lifetime(fixture *f)
{
  CHECK(decoder_engine_reset_session(f->engine) == DECODER_OK);
  initialize_request(f, 15, 0, 0, 0);
  f->expected[0] = "CQ PJ4/K1ABC";
  signal_message(f, "CQ PJ4/K1ABC", 1, 300, 100, 1000, f->samples);
  decode(f, 1);
  release(f);
  initialize_request(f, 15, 0, 15, 0);
  f->expected[0] = "<PJ4/K1ABC> W9XYZ -10";
  signal_message(f, "<PJ4/K1ABC> W9XYZ -10", 2, 300, 100, 1000, f->samples);
  decode(f, 1);
  CHECK(f->count == 1);
  release(f);
  CHECK(decoder_engine_reset_session(f->engine) == DECODER_OK);
  initialize_request(f, 15, 0, 15, 0);
  f->expected[0] = NULL;
  signal_message(f, "<PJ4/K1ABC> W9XYZ -10", 2, 300, 100, 1000, f->samples);
  decode(f, -1);
  CHECK(!strstr(f->message, "PJ4/K1ABC"));
  int reset_count = f->count;
  char reset_message[sizeof f->message];
  memcpy(reset_message, f->message, sizeof reset_message);
  release(f);
  CHECK(decoder_engine_destroy(f->engine) == DECODER_OK);
  decoder_engine_options options = {DECODER_ENGINE_ABI};
  CHECK(decoder_engine_create(&options, &f->engine) == DECODER_OK);
  initialize_request(f, 15, 0, 15, 0);
  f->expected[0] = NULL;
  signal_message(f, "<PJ4/K1ABC> W9XYZ -10", 2, 300, 100, 1000, f->samples);
  decode(f, -1);
  CHECK(f->count == reset_count && !strcmp(f->message, reset_message));
  release(f);
}

static void contest_retries(fixture *f)
{
  decoder_q65_caller callers[2] = {
    {"W9XYZ       ", "FN42", 1700000000, 1000},
    {"K9XYZ       ", "EN50", 1700000000, 1250}
  };
  for (int control = 0; control < 5; ++control) {
    CHECK(decoder_engine_reset_session(f->engine) == DECODER_OK);
    initialize_request(f, 60, 0, 10000, 0);
    f->request.q65.contest = 1;
    f->request.q65.receive_frequency_hz = 1500;
    f->request.q65.search_high_hz = 1350;
    f->request.q65.single_decode = 0;
    f->request.q65.qso_progress = 5;
    memcpy(f->request.q65.mycall, "K1ABC       ", 12);
    f->expected[1] = "K1ABC K9XYZ EN50";
    f->frequencies[1] = 1250;
    signal_message(f, f->expected[0], 1, 3.0f, 100, 1000, f->samples);
    signal_message(f, f->expected[1], 2, 3.0f, 0, 1250, f->before);
    for (int i = 0; i < f->audio.sample_count; ++i) f->samples[i] += f->before[i];
    if (control == 1 || control == 2)
      CHECK(decoder_engine_set_q65_callers(f->engine, callers + control - 1, 1) == DECODER_OK);
    if (control >= 3)
      CHECK(decoder_engine_set_q65_callers(f->engine, callers, 2) == DECODER_OK);
    if (control == 4) f->request.phase = DECODER_PHASE_REPEAT;
    decode(f, control >= 1 && control <= 3);
    CHECK(f->count == (control == 3 ? 2 : control >= 1 && control <= 2));
    CHECK(f->seen[0] == (control == 1 || control == 3));
    CHECK(f->seen[1] == (control == 2 || control == 3));
    if (f->count) CHECK(f->method == 3 && f->max_average == 1);
    counts(f, control == 4 ? 0 : 1, 0);
    release(f);
  }
}

static void create_fixture(fixture *f)
{
  decoder_engine_options options = {DECODER_ENGINE_ABI};
  decoder_engine_capabilities capabilities;
  f->samples = malloc(SAMPLE_CAPACITY * sizeof *f->samples);
  f->before = malloc(SAMPLE_CAPACITY * sizeof *f->before);
  CHECK(f->samples && f->before);
  f->audio.samples = f->samples;
  CHECK(decoder_engine_create(&options, &f->engine) == DECODER_OK);
  CHECK(decoder_engine_get_capabilities(f->engine, &capabilities) == DECODER_OK);
  CHECK(capabilities.supported_modes & DECODER_SUPPORT_Q65);
}

static void check_recreate_and_destroy(fixture *f)
{
  decoder_engine_options options = {DECODER_ENGINE_ABI};
  CHECK(decoder_engine_destroy(f->engine) == DECODER_OK);
  CHECK(decoder_engine_create(&options, &f->engine) == DECODER_OK);
  prepare(f, 15, 0, 0, 0);
  decode(f, 1);
  CHECK(decoder_engine_destroy(f->engine) == DECODER_OK);
  free(f->before);
  free(f->samples);
}

static void run_smoke(void)
{
  fixture f = {0};
  create_fixture(&f);
  prepare(&f, 15, 0, 0, 0);
  decode(&f, 1);
  counts(&f, 1, 0);
  release(&f);
  CHECK(decoder_engine_reset_session(f.engine) == DECODER_OK);
  counts(&f, 0, 0);
  prepare(&f, 15, 0, 0, 0);
  decode(&f, 1);
  release(&f);
  check_recreate_and_destroy(&f);
}

static void run_tests(void)
{
  decoder_attempt_outcome outcome;
  fixture f = {0};
  create_fixture(&f);

  decoder_q65_caller caller = {"W9XYZ       ", "FN42", 1700000000, 1000};
  decoder_q65_caller copied;
  int32_t caller_count;
  CHECK(decoder_engine_set_q65_callers(f.engine, &caller, 1) == DECODER_OK);
  memset(&caller, 0, sizeof caller);
  CHECK(decoder_engine_get_q65_callers(f.engine, NULL, 0, &caller_count) == DECODER_OK);
  CHECK(caller_count == 1);
  CHECK(decoder_engine_get_q65_callers(f.engine, &copied, 1, &caller_count) == DECODER_OK);
  CHECK(!memcmp(copied.call, "W9XYZ       ", 12) && !memcmp(copied.grid, "FN42", 4));
  CHECK(copied.last_seen == 1700000000 && copied.frequency_hz == 1000);
  CHECK(decoder_engine_set_q65_callers(f.engine, NULL, 1) == DECODER_INVALID);
  CHECK(decoder_engine_set_q65_callers(f.engine, NULL, DECODER_Q65_CALL_CAPACITY + 1) == DECODER_INVALID);

  prepare(&f, 60, 0, 1500, 0);
  decode(&f, 1);
  counts(&f, 0, 1);
  --f.audio.sample_count;
  ++f.request.attempt_no;
  CHECK(decoder_engine_decode(f.engine, &f.request, &f.audio, observe, &f, &outcome) == DECODER_INVALID);
  ++f.audio.sample_count;
  int64_t reused_input_id = f.request.input_id;
  repeat(&f);
  decode(&f, 1);
  counts(&f, 0, 1);
  CHECK(decoder_engine_clear_q65_averages(f.engine) == DECODER_OK);
  counts(&f, 0, 0);
  repeat(&f);
  decode(&f, 1);
  counts(&f, 0, 0);
  release(&f);
  prepare(&f, 60, 0, 1700, 0);
  f.request.input_id = reused_input_id;
  decode(&f, 1);
  counts(&f, 0, 1);
  release(&f);
  prepare(&f, 60, 0, 1800, 0);
  decode(&f, 1);
  counts(&f, 1, 1);
  release(&f);

  prepare(&f, 15, 0, 0, 1);
  f.request.q65.pileup = 1;
  decode(&f, 1);
  counts(&f, 1, 0);
  release(&f);

  prepare(&f, 15, 0, 30, 0);
  f.request.q65.auto_clear = 1;
  decode(&f, 1);
  counts(&f, 0, 0);
  repeat(&f);
  decode(&f, 1);
  counts(&f, 0, 0);
  release(&f);

  prepare(&f, 15, 1, 15, 0);
  decode(&f, 1);
  counts(&f, 0, 1);
  release(&f);
  CHECK(decoder_engine_reset_session(f.engine) == DECODER_OK);
  counts(&f, 0, 0);
  CHECK(decoder_engine_get_q65_callers(f.engine, NULL, 0, &caller_count) == DECODER_OK);
  CHECK(caller_count == 0);

  for (int reception = 0; reception < 2; ++reception) {
    initialize_request(&f, 60, 0, 10000 + reception * 200, 0);
    f.request.q65.averaging = 1;
    f.request.q65.auto_clear = 1;
    decoder_engine_test_q65_signal(60, 0, 0, reception + 1, 3.8f, 100.0f, 1000.0f, f.samples);
    decode(&f, reception == 1);
    counts(&f, reception == 0 ? 1 : 0, 0);
    if (reception == 1) {
      decoder_q65_snapshot snapshot;
      CHECK(f.max_average == 2);
      CHECK(decoder_engine_get_q65_snapshot(f.engine, &snapshot, NULL, NULL, 0) == DECODER_OK);
      CHECK(snapshot.curve_average_count == 2);
    }
    release(&f);
  }

  contest_retries(&f);

  prepare(&f, 60, 0, 2000, 0);
  f.audio.sample_count = 50 * 12000;
  decode(&f, 1);
  release(&f);
  prepare(&f, 15, 0, 0, 0);
  memset(f.samples + 5 * 12000, 0, 1200 * sizeof *f.samples);
  decode(&f, 1);
  release(&f);

  initialize_request(&f, 15, 0, 0, 0);
  f.request.phase = DECODER_PHASE_EARLY;
  CHECK(decoder_engine_decode(f.engine, &f.request, &f.audio, observe, &f, &outcome) == DECODER_INVALID);
  f.request.phase = DECODER_PHASE_NORMAL;
  f.request.q65.search_high_hz = 5001;
  CHECK(decoder_engine_decode(f.engine, &f.request, &f.audio, observe, &f, &outcome) == DECODER_INVALID);
  f.request.q65.search_high_hz = 1150;
  f.request.q65.period_seconds = INT32_MAX;
  CHECK(decoder_engine_decode(f.engine, &f.request, &f.audio, observe, &f, &outcome) == DECODER_INVALID);
  f.request.q65.period_seconds = 15;
  f.audio.sample_count++;
  CHECK(decoder_engine_decode(f.engine, &f.request, &f.audio, observe, &f, &outcome) == DECODER_INVALID);
  f.audio.sample_count = 1;
  f.samples[0] = 0;
  decode(&f, 0);
  release(&f);

  retained_options(&f);
  knowledge_lifetime(&f);
  prepare(&f, 120, 5, 0, 0);
  decode(&f, 1);
  CHECK(f.count == 1);
  release(&f);

  prepare(&f, 300, 0, 0, 0);
  decode(&f, 1);
  release(&f);
  check_recreate_and_destroy(&f);
}

static void write_le(FILE *file, uint32_t value, unsigned bytes)
{
  for (unsigned i = 0; i < bytes; ++i) CHECK(fputc((value >> (8 * i)) & 255, file) != EOF);
}

static void write_wav(const char *path, int period)
{
  CHECK(period == 15 || period == 60);
  const int count = period * 12000;
  int16_t *samples = malloc(count * sizeof *samples);
  CHECK(samples);
  decoder_engine_test_q65_signal(period, 0, 0, 1, 300.0f, 100.0f, 1500.0f, samples);
  FILE *file = fopen(path, "wb");
  CHECK(file);
  CHECK(fwrite("RIFF", 1, 4, file) == 4);
  write_le(file, 36 + count * 2, 4);
  CHECK(fwrite("WAVEfmt ", 1, 8, file) == 8);
  write_le(file, 16, 4);
  write_le(file, 1, 2);
  write_le(file, 1, 2);
  write_le(file, 12000, 4);
  write_le(file, 24000, 4);
  write_le(file, 2, 2);
  write_le(file, 16, 2);
  CHECK(fwrite("data", 1, 4, file) == 4);
  write_le(file, count * 2, 4);
  for (int i = 0; i < count; ++i) write_le(file, (uint16_t)samples[i], 2);
  CHECK(fclose(file) == 0);
  free(samples);
}

#ifdef _WIN32
static DWORD WINAPI run_on_small_stack(LPVOID unused)
#else
static void *run_on_small_stack(void *unused)
#endif
{
  (void)unused;
  run_tests();
  return 0;
}

#ifdef __linux__
static int add_tls_size(struct dl_phdr_info *info, size_t size, void *opaque)
{
  (void)size;
  size_t *stack_size = opaque;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr) *header = &info->dlpi_phdr[i];
    if (header->p_type == PT_TLS) *stack_size += header->p_memsz + header->p_align;
  }
  return 0;
}
#endif

int main(int argc, char **argv)
{
  if ((argc == 3 || argc == 4) && !strcmp(argv[1], "--write-wav")) {
    write_wav(argv[2], argc == 4 ? atoi(argv[3]) : 15);
  } else if (argc == 2 && !strcmp(argv[1], "--smoke")) {
    run_smoke();
  } else if (argc == 1) {
    run_tests();
  } else {
    CHECK(argc == 2 && !strcmp(argv[1], "--small-stack"));
#ifdef _WIN32
    HANDLE thread = CreateThread(NULL, 1024 * 1024, run_on_small_stack, NULL,
                                 STACK_SIZE_PARAM_IS_A_RESERVATION, NULL);
    CHECK(thread != NULL);
    CHECK(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    DWORD status;
    CHECK(GetExitCodeThread(thread, &status) && status == 0);
    CHECK(CloseHandle(thread));
#else
    pthread_attr_t attributes;
    pthread_t thread;
    size_t stack_size = 1024 * 1024;
#ifdef __linux__
    /* Linux puts static TLS in the stack mapping; reserve 1 MiB for calls. */
    dl_iterate_phdr(add_tls_size, &stack_size);
#endif
    CHECK(pthread_attr_init(&attributes) == 0);
    CHECK(pthread_attr_setstacksize(&attributes, stack_size) == 0);
    int error = pthread_create(&thread, &attributes, run_on_small_stack, NULL);
    if (error) fprintf(stderr, "pthread_create with %zu bytes: %s\n", stack_size, strerror(error));
    CHECK(error == 0);
    CHECK(pthread_attr_destroy(&attributes) == 0);
    CHECK(pthread_join(thread, NULL) == 0);
#endif
  }
  return EXIT_SUCCESS;
}
