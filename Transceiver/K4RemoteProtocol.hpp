#ifndef K4_REMOTE_PROTOCOL_HPP__
#define K4_REMOTE_PROTOCOL_HPP__

#include <QByteArray>
#include <QObject>
#include <QString>

namespace K4RemoteProtocol {
extern QByteArray const start_marker;
extern QByteArray const end_marker;

enum PayloadType : quint8 {
  CAT = 0x00,
  Audio = 0x01,
  Pan = 0x02,
  MiniPan = 0x03
};

constexpr quint16 default_port{9205};
constexpr quint16 tls_port{9204};
constexpr int maximum_buffer_size{1024 * 1024};

namespace AudioPacket {
constexpr int type_offset{0};
constexpr int version_offset{1};
constexpr int sequence_offset{2};
constexpr int mode_offset{3};
constexpr int frame_size_offset{4};
constexpr int sample_rate_offset{6};
constexpr int data_offset{7};
constexpr int header_size{7};
} // namespace AudioPacket

QByteArray build_packet(QByteArray const &payload);
QByteArray build_cat_packet(QString const &command);
QByteArray build_authentication(QString const &password);
QByteArray build_audio_packet(QByteArray const &audio, quint8 sequence,
                              quint8 encode_mode, quint16 frame_samples);
QString build_rf_power_command(double watts);
bool parse_rf_power(QString const &command, double *value,
                    bool *milliwatts = nullptr);
} // namespace K4RemoteProtocol

// Parser for the framed TCP stream used by the K4 remote server. This is kept
// deliberately independent from WSJT-X CAT state handling so it can be tested
// with fragmented and coalesced socket reads.
class K4RemoteProtocolParser final : public QObject {
  Q_OBJECT

public:
  explicit K4RemoteProtocolParser(QObject *parent = nullptr);
  void parse(QByteArray const &bytes);
  void clear();

Q_SIGNALS:
  void packet_received(quint8 type, QByteArray const &payload);
  void cat_received(QString const &response);
  void audio_received(QByteArray const &payload);
  void protocol_error(QString const &message);

private:
  void process(QByteArray const &payload);
  QByteArray buffer_;
};

#endif
