#ifndef MAP65_PTT_H
#define MAP65_PTT_H

#ifdef __cplusplus
extern "C" {
#endif

/* Device loss resets logical PTT but does not confirm physical release. */
enum { PTT_OK = 0, PTT_ERROR = 1, PTT_DEVICE_LOST = 2 };

int ptt_(int *nport, int *ntx, int *iptt);
void ptt_close(void);
void ptt_set_override(const char *path);

#ifdef __cplusplus
}
#endif

#endif
