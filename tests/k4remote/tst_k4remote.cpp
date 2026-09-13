#include <memory>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSignalSpy>
#include <QtEndian>
#include <QtTest>

#include "Transceiver/K4RemoteAudioCodec.hpp"
#include "Transceiver/K4RemoteProtocol.hpp"
#include "Transceiver/K4RemoteTxGuard.hpp"

class K4RemoteTests final : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void framing_survives_fragmentation();
  void framing_survives_coalescing();
  void password_authentication_is_sha384_hex();
  void audio_packet_matches_k4_wire_header();
  void rf_power_commands_match_qk4();
  void rf_power_readback_matches_qk4();
  void raw32_receive_selects_main_receiver();
  void raw16_receive_selects_main_receiver();
  void raw16_transmit_is_stereo();
  void windows_opus_is_application_local();
  void sustained_raw_receive_is_stable();
  void sustained_opus_receive_is_stable();
  void opus_transmit_has_protected_monitor();
  void guard_calibrates_in_qk4_target_range();
  void guard_trips_on_emergency_alc();
};

void K4RemoteTests::framing_survives_fragmentation() {
  K4RemoteProtocolParser parser;
  QSignalSpy cat{&parser, &K4RemoteProtocolParser::cat_received};
  auto const frame =
      K4RemoteProtocol::build_cat_packet(QStringLiteral("FA00014074000;"));
  for (auto byte : frame)
    parser.parse(QByteArray{1, byte});
  QCOMPARE(cat.count(), 1);
  QCOMPARE(cat.takeFirst().at(0).toString(), QStringLiteral("FA00014074000;"));
}

void K4RemoteTests::framing_survives_coalescing() {
  K4RemoteProtocolParser parser;
  QSignalSpy cat{&parser, &K4RemoteProtocolParser::cat_received};
  parser.parse(K4RemoteProtocol::build_cat_packet(QStringLiteral("MD6;")) +
               K4RemoteProtocol::build_cat_packet(QStringLiteral("DT0;")));
  QCOMPARE(cat.count(), 2);
  QCOMPARE(cat.at(0).at(0).toString(), QStringLiteral("MD6;"));
  QCOMPARE(cat.at(1).at(0).toString(), QStringLiteral("DT0;"));
}

void K4RemoteTests::password_authentication_is_sha384_hex() {
  auto const result =
      K4RemoteProtocol::build_authentication(QStringLiteral("K4 password"));
  QCOMPARE(result.size(), 96);
  QCOMPARE(result,
           QByteArrayLiteral(
               "cc4c712fecbe6307e9370a95c12807a3f22c9511f868b21efd56635ea8d0"
               "84f1ac63ba99745ddd801b791383c2e83156"));
  QVERIFY(QRegularExpression{QStringLiteral("^[0-9a-f]{96}$")}
              .match(QString::fromLatin1(result))
              .hasMatch());
}

void K4RemoteTests::audio_packet_matches_k4_wire_header() {
  auto const frame = K4RemoteProtocol::build_audio_packet(
      QByteArray::fromHex("123456"), 0xa5, 3, 720);
  QCOMPARE(frame.left(4), K4RemoteProtocol::start_marker);
  QCOMPARE(frame.right(4), K4RemoteProtocol::end_marker);
  QCOMPARE(qFromBigEndian<quint32>(
               reinterpret_cast<uchar const *>(frame.constData() + 4)),
           quint32(10));
  auto const payload = frame.mid(8, 10);
  QCOMPARE(static_cast<quint8>(payload[0]), quint8(K4RemoteProtocol::Audio));
  QCOMPARE(static_cast<quint8>(payload[1]), quint8(1));
  QCOMPARE(static_cast<quint8>(payload[2]), quint8(0xa5));
  QCOMPARE(static_cast<quint8>(payload[3]), quint8(3));
  QCOMPARE(qFromLittleEndian<quint16>(
               reinterpret_cast<uchar const *>(payload.constData() + 4)),
           quint16(720));
  QCOMPARE(static_cast<quint8>(payload[6]), quint8(0));
}

void K4RemoteTests::rf_power_commands_match_qk4() {
  QCOMPARE(K4RemoteProtocol::build_rf_power_command(0.1),
           QStringLiteral("PC001L;PC;"));
  QCOMPARE(K4RemoteProtocol::build_rf_power_command(9.9),
           QStringLiteral("PC099L;PC;"));
  QCOMPARE(K4RemoteProtocol::build_rf_power_command(10.),
           QStringLiteral("PC100L;PC;"));
  QCOMPARE(K4RemoteProtocol::build_rf_power_command(10.1),
           QStringLiteral("PC011H;PC;"));
  QCOMPARE(K4RemoteProtocol::build_rf_power_command(110.),
           QStringLiteral("PC110H;PC;"));
}

void K4RemoteTests::rf_power_readback_matches_qk4() {
  double value = 0.;
  bool milliwatts = false;
  QVERIFY(K4RemoteProtocol::parse_rf_power(QStringLiteral("PC099L"), &value,
                                           &milliwatts));
  QCOMPARE(value, 9.9);
  QVERIFY(!milliwatts);
  QVERIFY(K4RemoteProtocol::parse_rf_power(QStringLiteral("PC075H"), &value,
                                           &milliwatts));
  QCOMPARE(value, 75.);
  QVERIFY(!milliwatts);
  QVERIFY(K4RemoteProtocol::parse_rf_power(QStringLiteral("PC010X"), &value,
                                           &milliwatts));
  QCOMPARE(value, 1.);
  QVERIFY(milliwatts);
  QVERIFY(!K4RemoteProtocol::parse_rf_power(QStringLiteral("PC;"), &value,
                                            &milliwatts));
}

void K4RemoteTests::raw32_receive_selects_main_receiver() {
  K4RemoteAudioCodec codec;
  QByteArray payload(K4RemoteProtocol::AudioPacket::header_size, '\0');
  payload[0] = char(K4RemoteProtocol::Audio);
  payload[1] = 1;
  payload[3] = 0;
  qToLittleEndian<quint16>(
      2,
      reinterpret_cast<uchar *>(
          payload.data() + K4RemoteProtocol::AudioPacket::frame_size_offset));
  QByteArray stereo(16, '\0');
  auto *bytes = reinterpret_cast<uchar *>(stereo.data());
  qToLittleEndian<qint32>(4000, bytes);
  qToLittleEndian<qint32>(32000, bytes + 4);
  qToLittleEndian<qint32>(-4000, bytes + 8);
  qToLittleEndian<qint32>(-32000, bytes + 12);
  payload.append(stereo);
  auto const decoded = codec.decode(payload);
  QCOMPARE(decoded.size(), 4);
  auto const *mono = reinterpret_cast<qint16 const *>(decoded.constData());
  QCOMPARE(mono[0], qint16(1000));
  QCOMPARE(mono[1], qint16(-1000));
}

void K4RemoteTests::raw16_receive_selects_main_receiver() {
  K4RemoteAudioCodec codec;
  QByteArray payload(K4RemoteProtocol::AudioPacket::header_size, '\0');
  payload[0] = char(K4RemoteProtocol::Audio);
  payload[1] = 1;
  payload[3] = 1;
  qToLittleEndian<quint16>(
      2,
      reinterpret_cast<uchar *>(
          payload.data() + K4RemoteProtocol::AudioPacket::frame_size_offset));
  qint16 const stereo[]{100, 2000, -100, -2000};
  payload.append(reinterpret_cast<char const *>(stereo), sizeof stereo);
  auto const decoded = codec.decode(payload);
  QCOMPARE(decoded.size(), 4);
  auto const *mono = reinterpret_cast<qint16 const *>(decoded.constData());
  QCOMPARE(mono[0], qint16(1600));
  QCOMPARE(mono[1], qint16(-1600));
}

void K4RemoteTests::raw16_transmit_is_stereo() {
  K4RemoteAudioCodec codec;
  QVector<qint16> mono{123, -456};
  auto const encoded = codec.encode(mono, 1);
  QCOMPARE(encoded.size(), 8);
  auto const *stereo = reinterpret_cast<qint16 const *>(encoded.constData());
  QCOMPARE(stereo[0], qint16(123));
  QCOMPARE(stereo[1], qint16(123));
  QCOMPARE(stereo[2], qint16(-456));
  QCOMPARE(stereo[3], qint16(-456));
}

void K4RemoteTests::windows_opus_is_application_local() {
#if defined(Q_OS_WIN)
  K4RemoteAudioCodec codec;
  if (!codec.available())
    QSKIP("No application-local Opus runtime was staged for this test.");
  QCOMPARE(QFileInfo{codec.library_file_name()}.absolutePath(),
           QCoreApplication::applicationDirPath());
#else
  QSKIP("The application-local library rule is Windows-specific.");
#endif
}

void K4RemoteTests::sustained_raw_receive_is_stable() {
  K4RemoteAudioCodec codec;
  QByteArray payload(K4RemoteProtocol::AudioPacket::header_size, '\0');
  payload[0] = char(K4RemoteProtocol::Audio);
  payload[1] = 1;
  payload[3] = 1;
  constexpr quint16 frames{720};
  qToLittleEndian<quint16>(
      frames,
      reinterpret_cast<uchar *>(
          payload.data() + K4RemoteProtocol::AudioPacket::frame_size_offset));
  payload.append(QByteArray(frames * 2 * int(sizeof(qint16)), '\0'));

  // More than three minutes at SL3. This catches allocator damage that only
  // becomes visible after sustained network receive traffic.
  for (int packet = 0; packet != 3200; ++packet) {
    auto const decoded = codec.decode(payload);
    QCOMPARE(decoded.size(), frames * int(sizeof(qint16)));
  }
}

void K4RemoteTests::sustained_opus_receive_is_stable() {
  K4RemoteAudioCodec codec;
  if (!codec.available())
    QSKIP("The packaged Opus runtime is unavailable on this test host.");

  constexpr quint16 frames{720};
  QVector<qint16> mono(frames);
  for (int i = 0; i != mono.size(); ++i)
    mono[i] = qint16((i % 200) * 40 - 4000);

  // More than three minutes at SL3 through the same Xiph encode/decode APIs
  // used by QK4 Windows and the live K4 EM3 stream.
  for (int packet = 0; packet != 3200; ++packet) {
    auto const encoded = codec.encode(mono, 3);
    QVERIFY(!encoded.isEmpty());
    QByteArray payload(K4RemoteProtocol::AudioPacket::header_size, '\0');
    payload[0] = char(K4RemoteProtocol::Audio);
    payload[1] = 1;
    payload[2] = char(packet & 0xff);
    payload[3] = 3;
    qToLittleEndian<quint16>(
        frames,
        reinterpret_cast<uchar *>(
            payload.data() + K4RemoteProtocol::AudioPacket::frame_size_offset));
    payload.append(encoded);
    QCOMPARE(codec.decode(payload).size(), frames * int(sizeof(qint16)));
  }
}

void K4RemoteTests::opus_transmit_has_protected_monitor() {
  K4RemoteAudioCodec codec;
  if (!codec.available())
    QSKIP("The optional Opus runtime is unavailable on this test host.");
  QVector<qint16> mono(480, 1000);
  QVERIFY(!codec.encode(mono, 3).isEmpty());
}

void K4RemoteTests::guard_calibrates_in_qk4_target_range() {
  auto control = std::make_shared<K4RemoteTxControl>();
  K4RemoteTxGuard guard{control};
  QVERIFY(guard.begin(1, 0, true));
  guard.audio_accepted(0);
  guard.audio_accepted(600);
  QCOMPARE(guard.meter(QStringLiteral("TM003000000000"), 600),
           K4RemoteTxGuard::Action::None);
  guard.audio_accepted(1800);
  QCOMPARE(guard.meter(QStringLiteral("TM003000000000"), 1800),
           K4RemoteTxGuard::Action::Calibrated);
}

void K4RemoteTests::guard_trips_on_emergency_alc() {
  auto control = std::make_shared<K4RemoteTxControl>();
  K4RemoteTxGuard guard{control};
  QVERIFY(guard.begin(7, 0));
  QCOMPARE(guard.meter(QStringLiteral("TM010000000000"), 100),
           K4RemoteTxGuard::Action::Tripped);
  QVERIFY(guard.latched());
  QVERIFY(!control->allows(7));
}

QTEST_MAIN(K4RemoteTests)
#include "tst_k4remote.moc"
