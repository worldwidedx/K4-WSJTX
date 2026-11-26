#include <windows.h>
#include <stdio.h>
#include "ptt.h"

static HANDLE ptt_handle = INVALID_HANDLE_VALUE;
static enum { PTT_IDLE, PTT_KEYED, PTT_UNCERTAIN } ptt_state = PTT_IDLE;

static int control_line(DWORD operation)
{
    DWORD error;
    if (EscapeCommFunction(ptt_handle, operation))
        return PTT_OK;
    error = GetLastError();
    return error == ERROR_DEVICE_NOT_CONNECTED || error == ERROR_NO_SUCH_DEVICE
        ? PTT_DEVICE_LOST : PTT_ERROR;
}

static void close_port(void)
{
    if (ptt_handle != INVALID_HANDLE_VALUE)
        CloseHandle(ptt_handle);
    ptt_handle = INVALID_HANDLE_VALUE;
    ptt_state = PTT_IDLE;
}

static int release_lines(void)
{
    int rts = control_line(CLRRTS);
    int dtr = control_line(CLRDTR);
    EscapeCommFunction(ptt_handle, CLRBREAK);
    if (rts == PTT_DEVICE_LOST || dtr == PTT_DEVICE_LOST)
        return PTT_DEVICE_LOST;
    return rts || dtr ? PTT_ERROR : PTT_OK;
}

static int control_failure(int result, int *iptt)
{
    if (result == PTT_DEVICE_LOST) {
        close_port();
        *iptt = 0;
    } else {
        ptt_state = PTT_UNCERTAIN;
    }
    return result;
}

int ptt_(int *nport, int *ntx, int *iptt)
{
    int result;
    char path[64];

    if (!*ntx) {
        if (ptt_handle != INVALID_HANDLE_VALUE) {
            result = release_lines();
            if (result != PTT_OK)
                return control_failure(result, iptt);
            *iptt = 0;
            if (!CloseHandle(ptt_handle)) {
                ptt_state = PTT_UNCERTAIN;
                return PTT_ERROR;
            }
            ptt_handle = INVALID_HANDLE_VALUE;
        }
        ptt_state = PTT_IDLE;
        *iptt = 0;
        return PTT_OK;
    }

    if (ptt_state == PTT_UNCERTAIN)
        return PTT_ERROR;
    if (ptt_state == PTT_KEYED) {
        *iptt = 1;
        return PTT_OK;
    }
    if (!*nport) {
        *iptt = *ntx;
        return PTT_OK;
    }

    if (*nport < 10)
        snprintf(path, sizeof(path), "COM%d", *nport);
    else
        snprintf(path, sizeof(path), "\\\\.\\COM%d", *nport);
    ptt_handle = CreateFileA(path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL, NULL);
    if (ptt_handle == INVALID_HANDLE_VALUE)
        return PTT_ERROR;

    result = control_line(SETRTS);
    if (result == PTT_OK)
        result = control_line(SETDTR);
    if (result != PTT_OK)
        return control_failure(result, iptt);
    ptt_state = PTT_KEYED;
    *iptt = 1;
    return PTT_OK;
}

void ptt_close(void)
{
    if (ptt_handle != INVALID_HANDLE_VALUE) {
        release_lines();
        close_port();
    }
}
