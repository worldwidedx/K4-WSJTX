#ifndef WAV_INPUT_LOADER_HPP
#define WAV_INPUT_LOADER_HPP

#include <vector>

#include <QString>
#include <QDateTime>

namespace Radio
{

struct WavInputResult
{
  std::vector<short> samples;
  QString fileDateTime;
  QDateTime firstSampleUtc;
  quint64 samplesSinceMidnight {0};
  int timeReferenceRate {0};
  int frames {0};
  int nutc {0};
  int yymmdd {0};
  bool valid {false};
};

WavInputResult load_wav_input (QString const& name, int sample_limit);

}

#endif // WAV_INPUT_LOADER_HPP
