#include "K4RemoteAudioCodec.hpp"

#include "K4RemoteProtocol.hpp"

#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#include <QCoreApplication>
#include <QDir>

namespace {
constexpr int opus_ok{0};
constexpr int opus_application_voip{2048};
constexpr int opus_set_bitrate_request{4002};
constexpr int opus_reset_state{4028};
constexpr int maximum_opus_packet{4000};
constexpr int maximum_frame_samples{1440};

qint16 clamp_sample(double value) {
  return static_cast<qint16>(std::max<double>(
      std::numeric_limits<qint16>::min(),
      std::min<double>(std::numeric_limits<qint16>::max(), std::round(value))));
}
} // namespace

K4RemoteAudioCodec::K4RemoteAudioCodec() {
#if defined(Q_OS_WIN)
  // Do not use the generic name "opus" on Windows. Windows on ARM includes an
  // unrelated C:\Windows\System32\opus.dll, and QLibrary may select it before
  // the Xiph runtime shipped with this application. Calling that DLL through
  // the Xiph ABI corrupts the heap after RX streaming begins. QK4 Windows links
  // its controlled Opus runtime; preserve that invariant here by loading only
  // a DLL located beside wsjtx.exe.
  auto const application_directory = QCoreApplication::applicationDirPath();
  for (auto const &name : {QStringLiteral("libopus-0.dll"),
                           QStringLiteral("opus.dll")}) {
    library_.setFileName(QDir{application_directory}.filePath(name));
    if (library_.load())
      break;
  }
#else
  // Unix-like deployments provide libopus.so/dylib through the normal loader.
  library_.setFileName(QStringLiteral("opus"));
  if (!library_.load())
    library_.setFileNameAndVersion(QStringLiteral("opus"), 0);
#endif
  if (!library_.isLoaded()) {
    error_ = QStringLiteral("Could not load the packaged Xiph Opus runtime: %1")
                 .arg(library_.errorString());
    return;
  }

  auto encoder_create = resolve<encoder_create_t>("opus_encoder_create");
  encoder_destroy_ = resolve<encoder_destroy_t>("opus_encoder_destroy");
  encode_ = resolve<encode_t>("opus_encode");
  encoder_ctl_ = resolve<encoder_ctl_t>("opus_encoder_ctl");
  auto decoder_create = resolve<decoder_create_t>("opus_decoder_create");
  decoder_destroy_ = resolve<decoder_destroy_t>("opus_decoder_destroy");
  decode_ = resolve<decode_t>("opus_decode");
  decode_float_ = resolve<decode_float_t>("opus_decode_float");
  decoder_ctl_ = resolve<decoder_ctl_t>("opus_decoder_ctl");
  if (!encoder_create || !encoder_destroy_ || !encode_ || !encoder_ctl_ ||
      !decoder_create || !decoder_destroy_ || !decode_ || !decode_float_ ||
      !decoder_ctl_) {
    error_ =
        QStringLiteral("The Opus library is missing required codec symbols.");
    unload();
    return;
  }

  int status = opus_ok;
  encoder_ = encoder_create(12000, 1, opus_application_voip, &status);
  if (!encoder_ || status != opus_ok) {
    error_ = QStringLiteral("The Opus encoder could not be initialized.");
    unload();
    return;
  }
  encoder_ctl_(encoder_, opus_set_bitrate_request, 24000);

  decoder_ = decoder_create(12000, 2, &status);
  if (!decoder_ || status != opus_ok) {
    error_ = QStringLiteral("The Opus decoder could not be initialized.");
    unload();
    return;
  }
  monitor_ = decoder_create(12000, 1, &status);
  if (!monitor_ || status != opus_ok) {
    error_ =
        QStringLiteral("The Opus transmit monitor could not be initialized.");
    unload();
  }
}

K4RemoteAudioCodec::~K4RemoteAudioCodec() { unload(); }

void K4RemoteAudioCodec::unload() {
  if (monitor_ && decoder_destroy_)
    decoder_destroy_(monitor_);
  if (decoder_ && decoder_destroy_)
    decoder_destroy_(decoder_);
  if (encoder_ && encoder_destroy_)
    encoder_destroy_(encoder_);
  decoder_ = nullptr;
  monitor_ = nullptr;
  encoder_ = nullptr;
  if (library_.isLoaded())
    library_.unload();
}

bool K4RemoteAudioCodec::reset() {
  return available() && encoder_ctl_(encoder_, opus_reset_state) == opus_ok &&
         decoder_ctl_(decoder_, opus_reset_state) == opus_ok &&
         decoder_ctl_(monitor_, opus_reset_state) == opus_ok;
}

QByteArray K4RemoteAudioCodec::decode(QByteArray const &payload) {
  using namespace K4RemoteProtocol::AudioPacket;
  if (payload.size() <= header_size ||
      static_cast<quint8>(payload[type_offset]) != K4RemoteProtocol::Audio)
    return {};

  auto const mode = static_cast<quint8>(payload[mode_offset]);
  auto const data = payload.mid(data_offset);
  auto const frame_samples = qFromLittleEndian<quint16>(
      reinterpret_cast<uchar const *>(payload.constData() + frame_size_offset));
  if (!frame_samples || frame_samples > maximum_frame_samples)
    return {};

  QByteArray result;
  result.resize(frame_samples * static_cast<int>(sizeof(qint16)));
  auto *mono = reinterpret_cast<qint16 *>(result.data());
  if (mode == 0) {
    // K4 receive EM0 uses a signed 32-bit container with about 17 useful
    // bits. This is QK4's empirically verified 32x normalization.
    if (data.size() < frame_samples * 2 * static_cast<int>(sizeof(qint32)))
      return {};
    auto const *bytes = reinterpret_cast<uchar const *>(data.constData());
    for (int i = 0; i < frame_samples; ++i)
      mono[i] = clamp_sample(qFromLittleEndian<qint32>(bytes + i * 8) / 4.0);
  } else if (mode == 1) {
    if (data.size() < frame_samples * 2 * static_cast<int>(sizeof(qint16)))
      return {};
    auto const *bytes = reinterpret_cast<uchar const *>(data.constData());
    for (int i = 0; i < frame_samples; ++i)
      mono[i] = clamp_sample(qFromLittleEndian<qint16>(bytes + i * 4) * 16.0);
  } else if (mode == 2 || mode == 3) {
    if (!available())
      return {};
    if (mode == 2) {
      QVector<qint16> stereo(maximum_frame_samples * 2);
      auto const count = decode_(
          decoder_, reinterpret_cast<unsigned char const *>(data.constData()),
          data.size(), stereo.data(), maximum_frame_samples, 0);
      if (count <= 0 || count > maximum_frame_samples)
        return {};
      result.resize(count * static_cast<int>(sizeof(qint16)));
      mono = reinterpret_cast<qint16 *>(result.data());
      for (int i = 0; i < count; ++i)
        mono[i] = clamp_sample(stereo[i * 2] * 32.0);
    } else {
      QVector<float> stereo(maximum_frame_samples * 2);
      auto const count = decode_float_(
          decoder_, reinterpret_cast<unsigned char const *>(data.constData()),
          data.size(), stereo.data(), maximum_frame_samples, 0);
      if (count <= 0 || count > maximum_frame_samples)
        return {};
      result.resize(count * static_cast<int>(sizeof(qint16)));
      mono = reinterpret_cast<qint16 *>(result.data());
      for (int i = 0; i < count; ++i)
        mono[i] = clamp_sample(stereo[i * 2] * 32.0 * 32768.0);
    }
  } else
    return {};
  return result;
}

QByteArray K4RemoteAudioCodec::encode(QVector<qint16> const &mono,
                                      int encode_mode) {
  if (mono.isEmpty())
    return {};
  if (encode_mode == 0) {
    QByteArray result;
    result.resize(mono.size() * 2 * static_cast<int>(sizeof(float)));
    auto *bytes = reinterpret_cast<uchar *>(result.data());
    for (int i = 0; i < mono.size(); ++i) {
      float const sample = mono[i] / 32768.f;
      quint32 bits = 0;
      std::memcpy(&bits, &sample, sizeof bits);
      qToLittleEndian<quint32>(bits, bytes + i * 8);
      qToLittleEndian<quint32>(bits, bytes + i * 8 + 4);
    }
    return result;
  }
  if (encode_mode == 1) {
    QByteArray result;
    result.resize(mono.size() * 2 * static_cast<int>(sizeof(qint16)));
    auto *bytes = reinterpret_cast<uchar *>(result.data());
    for (int i = 0; i < mono.size(); ++i) {
      qToLittleEndian<qint16>(mono[i], bytes + i * 4);
      qToLittleEndian<qint16>(mono[i], bytes + i * 4 + 2);
    }
    return result;
  }
  if ((encode_mode == 2 || encode_mode == 3) && available()) {
    QByteArray result;
    result.resize(maximum_opus_packet);
    auto const size = encode_(encoder_, mono.constData(), mono.size(),
                              reinterpret_cast<unsigned char *>(result.data()),
                              result.size());
    if (size <= 0)
      return {};
    result.resize(size);
    QVector<float> monitored(mono.size());
    auto const decoded = decode_float_(
        monitor_, reinterpret_cast<unsigned char const *>(result.constData()),
        result.size(), monitored.data(), mono.size(), 0);
    if (decoded != mono.size())
      return {};
    for (auto const sample : monitored)
      if (!std::isfinite(sample) || std::abs(sample) > 0.70710678f)
        return {}; // QK4 retains 3 dB headroom after codec overshoot.
    return result;
  }
  return {};
}
