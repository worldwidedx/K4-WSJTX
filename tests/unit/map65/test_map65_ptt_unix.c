#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <poll.h>

static int serial_open(const char *, int, ...);
static int serial_ioctl(int, unsigned long, ...);
static int serial_close(int);
static int serial_poll(struct pollfd *, nfds_t, int);

#define open serial_open
#define ioctl serial_ioctl
#define close serial_close
#define poll serial_poll
#ifndef PTT_UNIX_SOURCE
#define PTT_UNIX_SOURCE "../../../map65/libm65/ptt_unix.c"
#endif
#include PTT_UNIX_SOURCE
#undef open
#undef ioctl
#undef close
#undef poll

enum operation { OPEN_PORT, GET_LINES, SET_LINES, CLOSE_PORT };
struct event {
    enum operation operation;
    int descriptor;
    int lines;
    char path[256];
};

static struct event events[128];
static int event_count;
static int lines[64];
static int opened[64];
static int next_descriptor = 10;
static int fail_open;
static unsigned long fail_ioctl_request;
static int fail_ioctl_countdown;
static int failed_descriptor;
static short poll_events;
static int fail_poll;
static int mock_failed;
static int iptt;
static const int ptt_lines = TIOCM_RTS | TIOCM_DTR;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); \
        return 0; \
    } \
} while (0)

static struct event *record(enum operation operation, int descriptor)
{
    struct event *event;
    if (event_count == (int)(sizeof(events) / sizeof(events[0]))) {
        fprintf(stderr, "Serial event buffer exhausted\n");
        exit(2);
    }
    event = &events[event_count++];
    memset(event, 0, sizeof(*event));
    event->operation = operation;
    event->descriptor = descriptor;
    return event;
}

static int serial_open(const char *path, int flags, ...)
{
    int descriptor = next_descriptor++;
    struct event *event = record(OPEN_PORT, descriptor);
    snprintf(event->path, sizeof(event->path), "%s", path);
    if (!(flags & O_NONBLOCK) || !(flags & O_NOCTTY)
        || (flags & O_ACCMODE) != O_RDWR
        || descriptor >= (int)(sizeof(opened) / sizeof(opened[0]))) {
        mock_failed = 1;
        return -1;
    }
    if (fail_open) {
        errno = EACCES;
        return -1;
    }
    opened[descriptor] = 1;
    lines[descriptor] = TIOCM_CTS | TIOCM_DTR | TIOCM_RTS;
    return descriptor;
}

static int serial_ioctl(int descriptor, unsigned long request, ...)
{
    va_list arguments;
    int *value;
    struct event *event = record(request == TIOCMGET ? GET_LINES : SET_LINES,
                                descriptor);
    va_start(arguments, request);
    value = va_arg(arguments, int *);
    va_end(arguments);
    if (descriptor < 0 || descriptor >= 64 || !opened[descriptor]
        || (request != TIOCMGET && request != TIOCMSET)) {
        mock_failed = 1;
        return -1;
    }
    event->lines = request == TIOCMSET ? *value : lines[descriptor];
    if (descriptor == failed_descriptor
        || (request == fail_ioctl_request && --fail_ioctl_countdown == 0)) {
        errno = EIO;
        return -1;
    }
    if (request == TIOCMGET)
        *value = lines[descriptor];
    else
        lines[descriptor] = *value;
    return 0;
}

static int serial_close(int descriptor)
{
    record(CLOSE_PORT, descriptor);
    if (descriptor < 0 || descriptor >= 64 || !opened[descriptor]) {
        mock_failed = 1;
        return -1;
    }
    opened[descriptor] = 0;
    return 0;
}

static int serial_poll(struct pollfd *ports, nfds_t count, int timeout)
{
    if (count != 1 || timeout != 0 || ports[0].events != 0
        || ports[0].fd < 0 || ports[0].fd >= 64 || !opened[ports[0].fd]) {
        mock_failed = 1;
        return -1;
    }
    if (fail_poll) {
        errno = EINTR;
        return -1;
    }
    ports[0].revents = poll_events;
    return poll_events ? 1 : 0;
}

static int command(int on)
{
    int unused = 0;
    return ptt_(&unused, &on, &iptt);
}

static void fail_next_ioctl(unsigned long request, int occurrence)
{
    fail_ioctl_request = request;
    fail_ioctl_countdown = occurrence;
}

static void reset(void)
{
    fail_open = 0;
    fail_ioctl_request = 0;
    failed_descriptor = -1;
    poll_events = 0;
    fail_poll = 0;
    ptt_close();
    ptt_set_override(NULL);
    event_count = 0;
    next_descriptor = 10;
    mock_failed = 0;
    iptt = 0;
    memset(opened, 0, sizeof(opened));
    memset(lines, 0, sizeof(lines));
}

static int count(enum operation operation)
{
    int result = 0;
    int i;
    for (i = 0; i < event_count; ++i)
        result += events[i].operation == operation;
    return result;
}

static int first(enum operation operation)
{
    int i;
    for (i = 0; i < event_count; ++i)
        if (events[i].operation == operation)
            return i;
    return -1;
}

static int disabled_and_unopened_off(void)
{
    CHECK(command(1) == 0 && iptt == 1);
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(event_count == 0);
    ptt_set_override("/dev/serial-A");
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(event_count == 0);
    return 1;
}

static int reuse_and_preserve_lines(void)
{
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == 0 && iptt == 1);
    CHECK(events[0].operation == OPEN_PORT);
    CHECK(events[1].operation == GET_LINES);
    CHECK(events[2].operation == SET_LINES && events[2].lines == TIOCM_CTS);
    CHECK(lines[10] == (TIOCM_CTS | ptt_lines));
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(lines[10] == TIOCM_CTS);
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == 0 && iptt == 1);
    CHECK(count(OPEN_PORT) == 1 && count(CLOSE_PORT) == 0);
    return 1;
}

static int idle_switch(void)
{
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == 0);
    CHECK(command(0) == 0);
    event_count = 0;
    ptt_set_override("/dev/serial-B");
    CHECK(command(1) == 0 && iptt == 1);
    CHECK(count(CLOSE_PORT) == 1 && count(OPEN_PORT) == 1);
    CHECK(first(CLOSE_PORT) < first(OPEN_PORT));
    CHECK(strcmp(events[first(OPEN_PORT)].path, "/dev/serial-B") == 0);
    CHECK(!opened[10] && opened[11]);
    CHECK(lines[10] == TIOCM_CTS && lines[11] == (TIOCM_CTS | ptt_lines));
    return 1;
}

static int active_switch(void)
{
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == 0);
    event_count = 0;
    ptt_set_override("/dev/serial-B");
    CHECK(count(OPEN_PORT) == 0 && count(CLOSE_PORT) == 0);
    CHECK(command(1) == 0 && iptt == 1);
    CHECK(count(OPEN_PORT) == 0 && count(CLOSE_PORT) == 0);
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(count(OPEN_PORT) == 0 && count(CLOSE_PORT) == 1);
    CHECK(lines[10] == TIOCM_CTS);
    CHECK(first(SET_LINES) < first(CLOSE_PORT));
    CHECK(command(1) == 0);
    CHECK(strcmp(events[first(OPEN_PORT)].path, "/dev/serial-B") == 0);
    return 1;
}

static int active_disable(void)
{
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == 0);
    event_count = 0;
    ptt_set_override(NULL);
    CHECK(count(CLOSE_PORT) == 0);
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(lines[10] == TIOCM_CTS && !opened[10]);
    CHECK(count(CLOSE_PORT) == 1 && count(OPEN_PORT) == 0);
    event_count = 0;
    CHECK(command(1) == 0 && iptt == 1);
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(event_count == 0);
    return 1;
}

static int repeated_switches(void)
{
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == 0);
    ptt_set_override("/dev/serial-B");
    ptt_set_override("/dev/serial-A");
    CHECK(command(0) == 0);
    CHECK(count(OPEN_PORT) == 1 && count(CLOSE_PORT) == 0);
    CHECK(command(1) == 0);
    ptt_set_override("/dev/serial-B");
    ptt_set_override(NULL);
    ptt_set_override("/dev/serial-C");
    CHECK(command(0) == 0);
    CHECK(count(OPEN_PORT) == 1 && count(CLOSE_PORT) == 1);
    event_count = 0;
    CHECK(command(1) == 0);
    CHECK(strcmp(events[first(OPEN_PORT)].path, "/dev/serial-C") == 0);
    return 1;
}

static int open_failure(void)
{
    ptt_set_override("/dev/serial-A");
    fail_open = 1;
    CHECK(command(1) != 0 && iptt == 0);
    CHECK(count(GET_LINES) == 0 && count(SET_LINES) == 0);
    fail_open = 0;
    CHECK(command(1) == 0 && iptt == 1);
    return 1;
}

static int initialization_failure(unsigned long request)
{
    int i;
    ptt_set_override("/dev/serial-A");
    fail_next_ioctl(request, 1);
    CHECK(command(1) != 0 && iptt == 0);
    for (i = 0; i < event_count; ++i)
        CHECK(events[i].operation != SET_LINES || !(events[i].lines & ptt_lines));
    event_count = 0;
    CHECK(command(1) != 0 && iptt == 0);
    CHECK(count(OPEN_PORT) == 0 && count(SET_LINES) == 0);
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(command(1) == 0 && iptt == 1);
    return 1;
}

static int initial_get_failure(void) { return initialization_failure(TIOCMGET); }
static int initial_set_failure(void) { return initialization_failure(TIOCMSET); }

static int keying_failure(unsigned long request)
{
    ptt_set_override("/dev/serial-A");
    fail_next_ioctl(request, 2);
    CHECK(command(1) != 0 && iptt == 0);
    event_count = 0;
    ptt_set_override("/dev/serial-B");
    CHECK(command(1) != 0 && iptt == 0);
    CHECK(count(OPEN_PORT) == 0 && count(CLOSE_PORT) == 0);
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(!opened[10]);
    CHECK(command(1) == 0 && iptt == 1);
    CHECK(strcmp(events[first(OPEN_PORT)].path, "/dev/serial-B") == 0);
    return 1;
}

static int keying_get_failure(void) { return keying_failure(TIOCMGET); }
static int keying_set_failure(void) { return keying_failure(TIOCMSET); }

static int release_failure(unsigned long request, const char *next_path)
{
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == 0 && iptt == 1);
    event_count = 0;
    ptt_set_override(next_path);
    fail_next_ioctl(request, 1);
    CHECK(command(0) != 0 && iptt == 1);
    CHECK(opened[10] && count(OPEN_PORT) == 0 && count(CLOSE_PORT) == 0);
    CHECK(command(1) != 0 && iptt == 1);
    CHECK(count(OPEN_PORT) == 0 && count(CLOSE_PORT) == 0);
    CHECK(command(0) == 0 && iptt == 0);
    CHECK(lines[10] == TIOCM_CTS && !opened[10]);
    CHECK(count(CLOSE_PORT) == 1);
    CHECK(command(1) == 0 && iptt == 1);
    CHECK(count(OPEN_PORT) == (next_path ? 1 : 0));
    return 1;
}

static int release_get_failure(void) { return release_failure(TIOCMGET, "/dev/serial-B"); }
static int release_set_failure(void) { return release_failure(TIOCMSET, "/dev/serial-B"); }
static int disable_release_failure(void) { return release_failure(TIOCMSET, NULL); }

static int disconnected_port(int during_tx, const char *next_path)
{
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == PTT_OK);
    if (!during_tx)
        CHECK(command(0) == PTT_OK);
    failed_descriptor = 10;
    poll_events = POLLHUP | POLLERR;
    event_count = 0;
    CHECK(command(during_tx ? 0 : 1) == PTT_DEVICE_LOST && iptt == 0);
    CHECK(!opened[10] && count(CLOSE_PORT) == 1);
    CHECK(count(OPEN_PORT) == 0);
    ptt_set_override(next_path);
    poll_events = 0;
    CHECK(command(1) == PTT_OK && iptt == 1);
    CHECK(count(OPEN_PORT) == (next_path ? 1 : 0));
    if (next_path)
        CHECK(strcmp(events[first(OPEN_PORT)].path, next_path) == 0);
    CHECK(command(0) == PTT_OK && iptt == 0);
    return 1;
}

static int disconnect_idle(void) { return disconnected_port(0, "/dev/serial-B"); }
static int disconnect_active(void) { return disconnected_port(1, "/dev/serial-B"); }
static int disconnect_same_path(void) { return disconnected_port(1, "/dev/serial-A"); }
static int disconnect_disable(void) { return disconnected_port(1, NULL); }

static int persistent_control_failure(void)
{
    int i;
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == PTT_OK);
    ptt_set_override("/dev/serial-B");
    failed_descriptor = 10;
    event_count = 0;
    for (i = 0; i < 3; ++i) {
        poll_events = i == 1 ? POLLERR : 0;
        fail_poll = i == 2;
        CHECK(command(0) == PTT_ERROR && iptt == 1);
        CHECK(command(1) == PTT_ERROR && iptt == 1);
        CHECK(opened[10] && count(CLOSE_PORT) == 0 && count(OPEN_PORT) == 0);
    }
    fail_poll = 0;
    poll_events = POLLHUP;
    CHECK(command(0) == PTT_DEVICE_LOST && iptt == 0);
    CHECK(!opened[10] && count(CLOSE_PORT) == 1);
    poll_events = 0;
    CHECK(command(1) == PTT_OK && iptt == 1);
    CHECK(strcmp(events[first(OPEN_PORT)].path, "/dev/serial-B") == 0);
    return 1;
}

static int disconnect_initialization(void)
{
    ptt_set_override("/dev/serial-A");
    failed_descriptor = 10;
    poll_events = POLLHUP;
    CHECK(command(1) == PTT_DEVICE_LOST && iptt == 0);
    CHECK(!opened[10] && count(CLOSE_PORT) == 1);
    CHECK(count(SET_LINES) == 0);
    poll_events = 0;
    CHECK(command(1) == PTT_OK && iptt == 1);
    return 1;
}

static int shutdown_cleanup(void)
{
    ptt_set_override("/dev/serial-A");
    CHECK(command(1) == 0);
    event_count = 0;
    ptt_close();
    CHECK(lines[10] == TIOCM_CTS && !opened[10]);
    CHECK(count(SET_LINES) == 1 && count(CLOSE_PORT) == 1);
    CHECK(first(SET_LINES) < first(CLOSE_PORT));
    event_count = 0;
    ptt_close();
    CHECK(event_count == 0);
    CHECK(command(1) == 0);
    fail_next_ioctl(TIOCMSET, 1);
    event_count = 0;
    ptt_close();
    CHECK(count(SET_LINES) == 1 && count(CLOSE_PORT) == 1 && !opened[11]);
    return 1;
}

int main(void)
{
    const struct {
        const char *name;
        int (*run)(void);
    } tests[] = {
        {"disabled and unopened off", disabled_and_unopened_off},
        {"reuse and preserve lines", reuse_and_preserve_lines},
        {"idle switch", idle_switch},
        {"active switch", active_switch},
        {"active disable", active_disable},
        {"repeated switches", repeated_switches},
        {"open failure", open_failure},
        {"initial get failure", initial_get_failure},
        {"initial set failure", initial_set_failure},
        {"keying get failure", keying_get_failure},
        {"keying set failure", keying_set_failure},
        {"release get failure", release_get_failure},
        {"release set failure", release_set_failure},
        {"disable release failure", disable_release_failure},
        {"disconnect while idle", disconnect_idle},
        {"disconnect while active", disconnect_active},
        {"disconnect and reuse path", disconnect_same_path},
        {"disconnect and disable", disconnect_disable},
        {"persistent control failure", persistent_control_failure},
        {"disconnect during initialization", disconnect_initialization},
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
