#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#ifndef PTT_WINDOWS_SOURCE
#define PTT_WINDOWS_SOURCE "../../../map65/libm65/ptt.c"
#endif
/* Redirect ptt.c's call (and our mock below) so our definition doesn't collide with libkernel32's real GetLastError at link time. */
#define GetLastError GetLastErr
DWORD GetLastErr(void);
#include PTT_WINDOWS_SOURCE

enum { OPEN_PORT = 100, CLOSE_PORT, RTS = 1, DTR = 2 };
struct event {
    DWORD operation;
    int handle;
    char path[64];
};

static struct event events[128];
static int event_count;
static int opened[64];
static int lines[64];
static int next_handle = 10;
static int fail_open;
static int fail_close;
static int failures[10];
static DWORD errors[10];
static DWORD last_error;
static int mock_failed;
static int iptt;
static int port;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); \
        return 0; \
    } \
} while (0)

static struct event *record(DWORD operation, int handle)
{
    struct event *event;
    if (event_count == (int)(sizeof(events) / sizeof(events[0])))
        exit(2);
    event = &events[event_count++];
    memset(event, 0, sizeof(*event));
    event->operation = operation;
    event->handle = handle;
    return event;
}

static int valid_handle(HANDLE handle)
{
    intptr_t value = (intptr_t)handle;
    if (value < 0 || value >= 64 || !opened[value]) {
        mock_failed = 1;
        return 0;
    }
    return 1;
}

HANDLE CreateFileA(const char *path, DWORD access, DWORD sharing, void *security,
                   DWORD creation, DWORD attributes, HANDLE template_file)
{
    int handle = next_handle++;
    struct event *event = record(OPEN_PORT, handle);
    snprintf(event->path, sizeof(event->path), "%s", path);
    if (access != GENERIC_WRITE || sharing || security || creation != OPEN_EXISTING
        || attributes != FILE_ATTRIBUTE_NORMAL || template_file || handle >= 64)
        mock_failed = 1;
    if (mock_failed || fail_open) {
        last_error = ERROR_ACCESS_DENIED;
        return INVALID_HANDLE_VALUE;
    }
    opened[handle] = 1;
    return (HANDLE)(intptr_t)handle;
}

BOOL EscapeCommFunction(HANDLE handle, DWORD operation)
{
    int value = (int)(intptr_t)handle;
    record(operation, value);
    if (!valid_handle(handle))
        return 0;
    if (operation != SETRTS && operation != SETDTR && operation != CLRRTS
        && operation != CLRDTR && operation != CLRBREAK) {
        mock_failed = 1;
        return 0;
    }
    if (failures[operation]) {
        if (failures[operation] > 0)
            --failures[operation];
        last_error = errors[operation];
        return 0;
    }
    last_error = 0;
    if (operation == SETRTS) lines[value] |= RTS;
    if (operation == SETDTR) lines[value] |= DTR;
    if (operation == CLRRTS) lines[value] &= ~RTS;
    if (operation == CLRDTR) lines[value] &= ~DTR;
    return 1;
}

BOOL CloseHandle(HANDLE handle)
{
    record(CLOSE_PORT, (int)(intptr_t)handle);
    if (!valid_handle(handle))
        return 0;
    if (fail_close) {
        last_error = ERROR_GEN_FAILURE;
        return 0;
    }
    opened[(intptr_t)handle] = 0;
    return 1;
}

DWORD GetLastError(void) { return last_error; }

static int command(int on) { return ptt_(&port, &on, &iptt); }

static int count(DWORD operation)
{
    int total = 0;
    int i;
    for (i = 0; i < event_count; ++i)
        total += events[i].operation == operation;
    return total;
}

static void fail(DWORD operation, DWORD error, int attempts)
{
    failures[operation] = attempts;
    errors[operation] = error;
}

static void reset(void)
{
    memset(failures, 0, sizeof(failures));
    fail_open = fail_close = 0;
    ptt_close();
    memset(opened, 0, sizeof(opened));
    memset(lines, 0, sizeof(lines));
    event_count = mock_failed = iptt = 0;
    next_handle = 10;
    port = 3;
}

static int disabled_and_unopened_release(void)
{
    CHECK(command(0) == PTT_OK && !iptt);
    port = 0;
    CHECK(command(1) == PTT_OK && iptt);
    CHECK(command(0) == PTT_OK && !iptt);
    ptt_close();
    CHECK(event_count == 0);
    return 1;
}

static int normal_cycle_and_switch(void)
{
    CHECK(command(1) == PTT_OK && iptt);
    CHECK(strcmp(events[0].path, "COM3") == 0);
    CHECK(lines[10] == (RTS | DTR));
    event_count = 0;
    port = 12;
    CHECK(command(1) == PTT_OK && iptt);
    CHECK(!event_count);
    CHECK(command(0) == PTT_OK && !iptt);
    CHECK(!lines[10] && !opened[10]);
    CHECK(events[0].operation == CLRRTS && events[1].operation == CLRDTR);
    CHECK(events[2].operation == CLRBREAK && events[3].operation == CLOSE_PORT);
    CHECK(command(1) == PTT_OK && iptt);
    CHECK(strcmp(events[4].path, "\\\\.\\COM12") == 0);
    port = 0;
    CHECK(command(0) == PTT_OK && !iptt && !opened[11] && !lines[11]);
    event_count = 0;
    CHECK(command(1) == PTT_OK && iptt);
    CHECK(command(0) == PTT_OK && !iptt && !event_count);
    return 1;
}

static int open_failure(void)
{
    fail_open = 1;
    CHECK(command(1) == PTT_ERROR && !iptt);
    CHECK(event_count == 1);
    CHECK(command(0) == PTT_OK && !iptt && event_count == 1);
    fail_open = 0;
    CHECK(command(1) == PTT_OK && iptt);
    return 1;
}

static int key_failure(DWORD operation)
{
    fail(operation, ERROR_GEN_FAILURE, 1);
    CHECK(command(1) == PTT_ERROR && !iptt);
    CHECK(opened[10] && count(CLOSE_PORT) == 0);
    CHECK(lines[10] == (operation == SETDTR ? RTS : 0));
    if (operation == SETRTS) CHECK(count(SETDTR) == 0);
    event_count = 0;
    port = 12;
    CHECK(command(1) == PTT_ERROR && !iptt && !event_count);
    CHECK(command(0) == PTT_OK && !iptt && !opened[10] && !lines[10]);
    CHECK(command(1) == PTT_OK && iptt);
    CHECK(strcmp(events[4].path, "\\\\.\\COM12") == 0);
    return 1;
}

static int rts_key_failure(void) { return key_failure(SETRTS); }
static int dtr_key_failure(void) { return key_failure(SETDTR); }

static int release_failure(DWORD operation, int attempts)
{
    CHECK(command(1) == PTT_OK && iptt);
    event_count = 0;
    port = 0;
    fail(operation, ERROR_GEN_FAILURE, attempts);
    CHECK(command(0) == PTT_ERROR && iptt && opened[10]);
    CHECK(count(CLRRTS) == 1 && count(CLRDTR) == 1 && count(CLRBREAK) == 1);
    CHECK(count(CLOSE_PORT) == 0);
    CHECK(command(1) == PTT_ERROR && iptt);
    CHECK(count(SETRTS) == 0 && count(SETDTR) == 0 && count(OPEN_PORT) == 0);
    if (attempts < 0) {
        CHECK(command(0) == PTT_ERROR && iptt && opened[10]);
        CHECK(count(CLOSE_PORT) == 0);
        failures[operation] = 0;
    }
    CHECK(command(0) == PTT_OK && !iptt && !opened[10] && !lines[10]);
    CHECK(count(CLOSE_PORT) == 1);
    event_count = 0;
    CHECK(command(1) == PTT_OK && iptt && !event_count);
    return 1;
}

static int transient_rts_failure(void) { return release_failure(CLRRTS, 1); }
static int persistent_dtr_failure(void) { return release_failure(CLRDTR, -1); }
static int unsupported_break(void)
{
    CHECK(command(1) == PTT_OK && iptt);
    event_count = 0;
    fail(CLRBREAK, ERROR_GEN_FAILURE, -1);
    CHECK(command(0) == PTT_OK && !iptt && !opened[10] && !lines[10]);
    CHECK(count(CLRRTS) == 1 && count(CLRDTR) == 1 && count(CLRBREAK) == 1);
    CHECK(count(CLOSE_PORT) == 1);
    CHECK(command(1) == PTT_OK && iptt && opened[11]);
    return 1;
}

static int device_loss(DWORD operation, DWORD error)
{
    if (operation == CLRRTS) CHECK(command(1) == PTT_OK && iptt);
    fail(operation, error, -1);
    CHECK(command(operation == SETDTR) == PTT_DEVICE_LOST && !iptt);
    CHECK(!opened[10] && count(CLOSE_PORT) == 1);
    event_count = 0;
    CHECK(command(0) == PTT_OK && !iptt && !event_count);
    failures[operation] = 0;
    CHECK(command(1) == PTT_OK && iptt && opened[11]);
    return 1;
}

static int lost_during_key(void) { return device_loss(SETDTR, ERROR_NO_SUCH_DEVICE); }
static int lost_during_release(void) { return device_loss(CLRRTS, ERROR_DEVICE_NOT_CONNECTED); }

static int close_failure(void)
{
    CHECK(command(1) == PTT_OK);
    fail_close = 1;
    CHECK(command(0) == PTT_ERROR && !iptt && opened[10] && !lines[10]);
    event_count = 0;
    CHECK(command(1) == PTT_ERROR && !event_count);
    fail_close = 0;
    CHECK(command(0) == PTT_OK && !opened[10]);
    return 1;
}

static int shutdown_cleanup(void)
{
    CHECK(command(1) == PTT_OK);
    event_count = 0;
    fail(CLRRTS, ERROR_GEN_FAILURE, -1);
    ptt_close();
    CHECK(count(CLRRTS) == 1 && count(CLRDTR) == 1 && count(CLRBREAK) == 1);
    CHECK(count(CLOSE_PORT) == 1 && !opened[10]);
    event_count = 0;
    ptt_close();
    CHECK(!event_count);
    return 1;
}

int main(void)
{
    const struct {
        const char *name;
        int (*run)(void);
    } tests[] = {
        {"disabled and unopened release", disabled_and_unopened_release},
        {"normal cycle and switch", normal_cycle_and_switch},
        {"open failure", open_failure},
        {"RTS key failure", rts_key_failure},
        {"DTR key failure", dtr_key_failure},
        {"transient RTS release failure", transient_rts_failure},
        {"persistent DTR release failure", persistent_dtr_failure},
        {"unsupported break", unsupported_break},
        {"lost during key", lost_during_key},
        {"lost during release", lost_during_release},
        {"close failure", close_failure},
        {"shutdown cleanup", shutdown_cleanup}
    };
    int failed = 0;
    size_t i;
    for (i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        int passed;
        reset();
        passed = tests[i].run() && !mock_failed;
        printf("%s: %s\n", passed ? "PASS" : "FAIL", tests[i].name);
        failed += !passed;
    }
    reset();
    return failed ? 1 : 0;
}
