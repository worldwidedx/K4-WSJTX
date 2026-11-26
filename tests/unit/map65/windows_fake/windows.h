#ifndef MAP65_TEST_WINDOWS_H
#define MAP65_TEST_WINDOWS_H

#include <stdint.h>

typedef void *HANDLE;
typedef uint32_t DWORD;
typedef int BOOL;

#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define GENERIC_WRITE 0x40000000
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x80
#define SETRTS 3
#define CLRRTS 4
#define SETDTR 5
#define CLRDTR 6
#define CLRBREAK 9
#define ERROR_ACCESS_DENIED 5
#define ERROR_GEN_FAILURE 31
#define ERROR_NO_SUCH_DEVICE 433
#define ERROR_DEVICE_NOT_CONNECTED 1167

HANDLE CreateFileA(const char *, DWORD, DWORD, void *, DWORD, DWORD, HANDLE);
BOOL EscapeCommFunction(HANDLE, DWORD);
BOOL CloseHandle(HANDLE);
DWORD GetLastError(void);

#endif
