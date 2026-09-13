#include "K4RemoteProtocol.hpp"

#include <cmath>

#include <QCryptographicHash>
#include <QtEndian>

namespace K4RemoteProtocol {
QByteArray const start_marker{QByteArray::fromHex("FEFDFCFB")};
QByteArray const end_marker{QByteArray::fromHex("FBFCFDFE")};

QByteArray build_packet(QByteArray const &payload) {
  QByteArray packet;
  packet.reserve(payload.size() + 12);
  packet.append(start_marker);
  char length[4];
  qToBigEndian(static_cast<quint32>(payload.size()),
               reinterpret_cast<uchar *>(length));
  packet.append(length, 4);
  packet.append(payload);
  packet.append(end_marker);
  return packet;
}

QByteArray build_cat_packet(QString const &command) {
  QByteArray payload;
  payload.reserve(command.size() + 3);
  payload.append(static_cast<char>(CAT));
  payload.append('\0');
  payload.append('\0');
  payload.append(command.toLatin1());
  return build_packet(payload);
}

QByteArray build_authentication(QString const &password) {
  return QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha384)
      .toHex()
      .toLower();
}

QByteArray build_audio_packet(QByteArray const &audio, quint8 sequence,
                              quint8 encode_mode, quint16 frame_samples) {
  QByteArray payload;
  payload.reserve(AudioPacket::header_size + audio.size());
  payload.append(static_cast<char>(Audio));
  payload.append(static_cast<char>(0x01));
  payload.append(static_cast<char>(sequence));
  payload.append(static_cast<char>(encode_mode));
  payload.append(static_cast<char>(frame_samples & 0xff));
  payload.append(static_cast<char>((frame_samples >> 8) & 0xff));
  payload.append('\0'); // 12 kHz sample-rate code
  payload.append(audio);
  return build_packet(payload);
}

QString build_rf_power_command(double watts) {
  if (!std::isfinite(watts))
    watts = 0.1;
  if (watts <= 10.) {
    auto const raw = qBound(1, qRound(watts * 10.), 100);
    return QStringLiteral("PC%1L;PC;").arg(raw, 3, 10, QLatin1Char('0'));
  }
  auto const raw = qBound(11, qRound(watts), 110);
  return QStringLiteral("PC%1H;PC;").arg(raw, 3, 10, QLatin1Char('0'));
}

bool parse_rf_power(QString const &command, double *value, bool *milliwatts) {
  if (!value || command.size() < 6 ||
      !command.startsWith(QStringLiteral("PC")))
    return false;
  bool ok = false;
  auto const raw = command.mid(2, 3).toInt(&ok);
  if (!ok)
    return false;
  auto const range = command.at(5).toUpper();
  if (range == QLatin1Char('L')) {
    *value = raw / 10.;
    if (milliwatts)
      *milliwatts = false;
    return true;
  }
  if (range == QLatin1Char('H')) {
    *value = raw;
    if (milliwatts)
      *milliwatts = false;
    return true;
  }
  if (range == QLatin1Char('X')) {
    *value = raw / 10.;
    if (milliwatts)
      *milliwatts = true;
    return true;
  }
  return false;
}
} // namespace K4RemoteProtocol

K4RemoteProtocolParser::K4RemoteProtocolParser(QObject *parent)
    : QObject{parent} {}

void K4RemoteProtocolParser::clear() { buffer_.clear(); }

void K4RemoteProtocolParser::parse(QByteArray const &bytes) {
  buffer_.append(bytes);
  if (buffer_.size() > K4RemoteProtocol::maximum_buffer_size) {
    buffer_.clear();
    Q_EMIT protocol_error(
        tr("K4 remote protocol buffer exceeded its safety limit."));
    return;
  }

  for (;;) {
    auto const start = buffer_.indexOf(K4RemoteProtocol::start_marker);
    if (start < 0) {
      if (buffer_.size() > 3)
        buffer_ = buffer_.right(3);
      return;
    }
    if (start)
      buffer_.remove(0, start);
    if (buffer_.size() < 8)
      return;

    auto const payload_length = qFromBigEndian<quint32>(
        reinterpret_cast<uchar const *>(buffer_.constData() + 4));
    if (payload_length >
        static_cast<quint32>(K4RemoteProtocol::maximum_buffer_size)) {
      buffer_.remove(0, 4);
      Q_EMIT protocol_error(
          tr("K4 remote packet advertised an invalid length."));
      continue;
    }
    auto const total = 12 + static_cast<int>(payload_length);
    if (buffer_.size() < total)
      return;
    if (buffer_.mid(total - 4, 4) != K4RemoteProtocol::end_marker) {
      buffer_.remove(0, 4);
      Q_EMIT protocol_error(tr("K4 remote packet had an invalid end marker."));
      continue;
    }

    auto const payload = buffer_.mid(8, payload_length);
    buffer_.remove(0, total);
    process(payload);
  }
}

void K4RemoteProtocolParser::process(QByteArray const &payload) {
  if (payload.isEmpty())
    return;
  auto const type = static_cast<quint8>(payload[0]);
  Q_EMIT packet_received(type, payload);
  if (K4RemoteProtocol::CAT == type && payload.size() > 3)
    Q_EMIT cat_received(QString::fromLatin1(payload.mid(3)));
  else if (K4RemoteProtocol::Audio == type &&
           payload.size() > K4RemoteProtocol::AudioPacket::header_size)
    Q_EMIT audio_received(payload);
}
