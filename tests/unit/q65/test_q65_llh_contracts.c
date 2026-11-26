#include "q65.h"
#include "qra15_65_64_irr_e23.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require (int condition, char const * message)
{
  if (!condition)
    {
      fprintf (stderr, "FAIL: %s\n", message);
      exit (1);
    }
}

static void set_candidate_probabilities (float * intrinsics, int const * codeword,
                                         float probability)
{
  for (int symbol = 0; symbol < 63; ++symbol)
    {
      for (int tone = 0; tone < 64; ++tone)
        intrinsics[symbol * 64 + tone] = (1.0f - probability) / 63.0f;
      intrinsics[symbol * 64 + codeword[symbol]] = probability;
    }
}

static void check_full_ap_list (void)
{
  q65_codec_ds codec;
  int (*codewords)[63] = calloc (Q65_FULLAPLIST_SIZE, sizeof *codewords);
  float intrinsics[63 * 64];
  int decoded[63], message[13], expected[13] = {0};

  require (codewords != NULL, "full AP list allocation failed");
  require (q65_init (&codec, &qra15_65_64_irr_e23) >= 0,
           "Q65 codec initialization failed");
  require (Q65_FULLAPLIST_SIZE == 511, "Q65 full AP list capacity changed");
  for (int index = 0; index < Q65_FULLAPLIST_SIZE; ++index)
    {
      expected[0] = index % 64;
      expected[1] = index / 64;
      require (q65_encode (&codec, codewords[index], expected) >= 0,
               "full AP candidate encoding failed");
    }

  int last = Q65_FULLAPLIST_SIZE - 1;
  set_candidate_probabilities (intrinsics, codewords[last], 0.9f);
  require (q65_decode_fullaplist (&codec, decoded, message, intrinsics,
                                 codewords[0], Q65_FULLAPLIST_SIZE) == last,
           "full AP decoder did not reach the final current-DX candidate");
  require (memcmp (decoded, codewords[last], sizeof decoded) == 0 &&
           memcmp (message, expected, sizeof message) == 0,
           "full AP decoder returned the wrong final candidate payload");

  int last_stored = last - 10;
  set_candidate_probabilities (intrinsics, codewords[last_stored], 0.9f);
  require (q65_decode_fullaplist (&codec, decoded, message, intrinsics,
                                 codewords[0], Q65_FULLAPLIST_SIZE) == last_stored,
           "full AP decoder did not reach the final stored-caller candidate");

  for (int index = 0; index < 63 * 64; ++index)
    intrinsics[index] = 1.0f / 64.0f;
  require (q65_decode_fullaplist (&codec, decoded, message, intrinsics,
                                 codewords[0], Q65_FULLAPLIST_SIZE) == Q65_DECODE_FAILED,
           "full AP decoder accepted uniform evidence at maximum capacity");
  require (q65_decode_fullaplist (&codec, decoded, message, intrinsics,
                                 codewords[0], Q65_FULLAPLIST_SIZE + 1) == Q65_DECODE_INVPARAMS,
           "full AP decoder accepted a list beyond capacity");

  // This likelihood lies between the admission thresholds for 411 and 511 entries.
  set_candidate_probabilities (intrinsics, codewords[0], expf (-255.0f / 63.0f));
  require (q65_decode_fullaplist (&codec, decoded, message, intrinsics,
                                 codewords[0], 411) == 0,
           "full AP decoder rejected the smaller-list boundary fixture");
  require (q65_decode_fullaplist (&codec, decoded, message, intrinsics,
                                 codewords[0], Q65_FULLAPLIST_SIZE) == Q65_DECODE_FAILED,
           "growing the full AP list failed to tighten admission");

  q65_free (&codec);
  free (codewords);
}

int main (void)
{
  float intrinsics[64] = {0};
  int symbol = 0;

  for (int i = 0; i < 64; ++i)
    intrinsics[i] = 1.0f;

  require (q65_check_llh (NULL, &symbol, 1, 64, intrinsics) == 1,
           "valid codeword symbol rejected");

  symbol = -1;
  float llh = 123.0f;
  require (q65_check_llh (&llh, &symbol, 1, 64, intrinsics) == 0,
           "negative codeword symbol accepted");
  require (isinf (llh) && llh < 0.0f,
           "invalid codeword symbol left LLH undefined");

  symbol = 64;
  llh = 123.0f;
  require (q65_check_llh (&llh, &symbol, 1, 64, intrinsics) == 0,
           "out-of-range codeword symbol accepted");
  require (isinf (llh) && llh < 0.0f,
           "out-of-range codeword symbol left LLH undefined");

  check_full_ap_list ();
  return 0;
}
