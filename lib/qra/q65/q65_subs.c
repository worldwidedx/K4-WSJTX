/* Owned and legacy Fortran interfaces for the Q65 codec. */

#include "qra15_65_64_irr_e23.h"
#include "q65.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *q65_codec_create(void)
{
  q65_codec_ds *codec = calloc(1, sizeof(*codec));
  if (!codec) return NULL;
  if (q65_init(codec, &qra15_65_64_irr_e23) < 0) {
    free(codec);
    return NULL;
  }
  return codec;
}

void q65_codec_destroy(void *handle)
{
  if (!handle) return;
  q65_free(handle);
  free(handle);
}

int q65_codec_encode(void *handle, const int *x, int *y)
{
  memset(y, 0, 63 * sizeof(*y));
  if (!handle) return -1;
  return q65_encode(handle, y, x) < 0 ? -1 : 0;
}

int q65_codec_intrinsics(void *handle, const float *s3, int submode,
                         float b90ts, int fading_model, float *prob)
{
  memset(prob, 0, 64 * 63 * sizeof(*prob));
  if (!handle) return -1;
  return q65_intrinsics_fastfading(handle, prob, s3, submode, b90ts,
                                    fading_model) < 0 ? -1 : 0;
}

int q65_codec_decode(void *handle, const float *s3, const float *prob,
                     const int *mask, const int *symbols, int maxiters,
                     float *esnodb, int *xdec, int *decode_rc)
{
  int ydec[63];
  *esnodb = 0;
  *decode_rc = Q65_DECODE_INVPARAMS;
  memset(xdec, 0, 13 * sizeof(*xdec));
  if (!handle) return -1;
  *decode_rc = q65_decode(handle, ydec, xdec, prob, mask, symbols, maxiters);
  if (*decode_rc == Q65_DECODE_INVPARAMS) return -1;
  if (*decode_rc < 0) return 0;
  if (q65_esnodb_fastfading(handle, esnodb, ydec, s3) < 0) {
    *decode_rc = Q65_DECODE_INVPARAMS;
    *esnodb = 0;
    return -1;
  }
  return 0;
}

int q65_codec_decode_fullaplist(void *handle, const float *s3, const float *prob,
                                const int *codewords, int ncw, float *esnodb,
                                int *xdec, float *plog, int *decode_rc)
{
  int ydec[63];
  *esnodb = 0;
  *plog = 0;
  *decode_rc = Q65_DECODE_INVPARAMS;
  memset(xdec, 0, 13 * sizeof(*xdec));
  if (!handle) return -1;
  *decode_rc = q65_decode_fullaplist(handle, ydec, xdec, prob, codewords, ncw);
  if (*decode_rc == Q65_DECODE_INVPARAMS) return -1;
  *plog = q65_llh;
  if (*decode_rc < 0) return 0;
  if (q65_esnodb_fastfading(handle, esnodb, ydec, s3) < 0) {
    *decode_rc = Q65_DECODE_INVPARAMS;
    *esnodb = 0;
    return -1;
  }
  return 0;
}

static q65_codec_ds *legacy_codec(void)
{
  static q65_codec_ds *codec;
  if (!codec) {
    codec = q65_codec_create();
    if (!codec) {
      fputs("Unable to initialize Q65 codec\n", stderr);
      exit(EXIT_FAILURE);
    }
  }
  return codec;
}

static void check_legacy_status(int status)
{
  if (status < 0) {
    fputs("Q65 codec operation failed\n", stderr);
    exit(EXIT_FAILURE);
  }
}

void q65_enc_(int x[], int y[])
{
  check_legacy_status(q65_codec_encode(legacy_codec(), x, y));
}

void q65_intrinsics_ff_(float s3[], int *submode, float *b90ts,
                        int *fading_model, float prob[])
{
  check_legacy_status(q65_codec_intrinsics(legacy_codec(), s3, *submode,
                                           *b90ts, *fading_model, prob));
}

void q65_dec_(float s3[], float prob[], int mask[], int symbols[],
              int *maxiters, float *esnodb, int xdec[], int *rc)
{
  q65_codec_decode(legacy_codec(), s3, prob, mask, symbols,
                   *maxiters, esnodb, xdec, rc);
}

void q65_dec_fullaplist_(float s3[], float prob[], int codewords[],
                         int *ncw, float *esnodb, int xdec[], float *plog, int *rc)
{
  q65_codec_decode_fullaplist(legacy_codec(), s3, prob, codewords,
                              *ncw, esnodb, xdec, plog, rc);
}
