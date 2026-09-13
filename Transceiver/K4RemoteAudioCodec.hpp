#ifndef K4_REMOTE_AUDIO_CODEC_HPP__
#define K4_REMOTE_AUDIO_CODEC_HPP__

#include <QByteArray>
#include <QLibrary>
#include <QString>
#include <QVector>

// Opus is loaded at runtime so the normal WSJT-X dependency graph remains
// unchanged. Distributions and installers must ship the ordinary Xiph Opus
// shared library when EM2/EM3 is offered to the operator.
class K4RemoteAudioCodec final {
public:
  K4RemoteAudioCodec();
  ~K4RemoteAudioCodec();

  K4RemoteAudioCodec(K4RemoteAudioCodec const &) = delete;
  K4RemoteAudioCodec &operator=(K4RemoteAudioCodec const &) = delete;

  bool available() const { return decoder_ && encoder_ && monitor_; }
  QString error_string() const { return error_; }
  QString library_file_name() const { return library_.fileName(); }
  bool reset();

  // Decode a complete K4 audio payload to 12 kHz mono signed PCM. The main
  // receiver (left channel) is the WSJT-X source; the sub receiver is ignored.
  QByteArray decode(QByteArray const &payload);

  // Encode one 12 kHz mono S16 frame using the selected K4 EM mode. EM0 and
  // EM1 are uncompressed and do not require Opus.
  QByteArray encode(QVector<qint16> const &mono, int encode_mode);

private:
  struct OpusEncoder;
  struct OpusDecoder;
  using encoder_create_t = OpusEncoder *(*)(qint32, int, int, int *);
  using encoder_destroy_t = void (*)(OpusEncoder *);
  using encode_t = int (*)(OpusEncoder *, qint16 const *, int, unsigned char *,
                           qint32);
  using encoder_ctl_t = int (*)(OpusEncoder *, int, ...);
  using decoder_create_t = OpusDecoder *(*)(qint32, int, int *);
  using decoder_destroy_t = void (*)(OpusDecoder *);
  using decode_t = int (*)(OpusDecoder *, unsigned char const *, qint32,
                           qint16 *, int, int);
  using decode_float_t = int (*)(OpusDecoder *, unsigned char const *, qint32,
                                 float *, int, int);
  using decoder_ctl_t = int (*)(OpusDecoder *, int, ...);

  template <typename T> T resolve(char const *name) {
    return reinterpret_cast<T>(library_.resolve(name));
  }
  void unload();

  QLibrary library_;
  QString error_;
  OpusEncoder *encoder_{nullptr};
  OpusDecoder *decoder_{nullptr};
  OpusDecoder *monitor_{nullptr};
  encoder_destroy_t encoder_destroy_{nullptr};
  encode_t encode_{nullptr};
  encoder_ctl_t encoder_ctl_{nullptr};
  decoder_destroy_t decoder_destroy_{nullptr};
  decode_t decode_{nullptr};
  decode_float_t decode_float_{nullptr};
  decoder_ctl_t decoder_ctl_{nullptr};
};

#endif
