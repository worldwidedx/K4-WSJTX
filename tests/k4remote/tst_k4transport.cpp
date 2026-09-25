#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtEndian>
#include <cmath>
#include "Transceiver/K4RemoteTransceiver.hpp"
#include "commons.h"
#include "widgets/itoneAndicw.h"

// Only the waveform/decoder storage is substituted. The socket, parser,
// transceiver base, polling, codec and protection are production code.
struct dec_data dec_data {};
extern "C" { decltype(foxcom_) foxcom_ {}; }
int volatile itone[MAX_NUM_SYMBOLS] {};
int volatile icw[NUM_CW_SYMBOLS] {};

class FakeK4 : public QObject {
public:
  QTcpServer server;
  QTcpSocket *client {nullptr};
  K4RemoteProtocolParser parser;
  QByteArray authentication;
  QStringList commands;
  QList<QByteArray> audio;
  bool transmitting {false};
  bool meters {true};
  bool test {false};
  QByteArray meter {"TM003000000000;"};

  FakeK4() {
    connect(&server, &QTcpServer::newConnection, this, [this] {
      client = server.nextPendingConnection();
      connect(client, &QTcpSocket::readyRead, this, [this] {
        auto bytes = client->readAll();
        if (authentication.size() < 96) {
          auto count = qMin(96 - int(authentication.size()), int(bytes.size()));
          authentication += bytes.left(count);
          bytes.remove(0, count);
          if (authentication.size() == 96)
            reply("FA00014074000;FB00014074000;MD6;DT0;FT0;TS0;TQ0;LI050;MG050;CP000;TE000;");
        }
        parser.parse(bytes);
      });
    });
    connect(&parser, &K4RemoteProtocolParser::cat_received, this, [this](QString text) {
      for (auto command : text.split(';', Qt::SkipEmptyParts)) {
        commands << command;
        if (command == "TQ") reply(transmitting ? "TQ1;" : "TQ0;");
        if (command == "TM" && meters) reply(meter.constData());
        if (command == "TS1") test = true;
        if (command == "TS0") test = false;
        if (command == "TS") reply(test ? "TS1;" : "TS0;");
        if (command == "RX") { transmitting = false; reply("RX;TQ0;"); }
        // Deliberately do not key on TX CAT: remote audio must initiate TX.
      }
    });
    connect(&parser, &K4RemoteProtocolParser::audio_received, this, [this](QByteArray packet) {
      audio << packet;
      transmitting = true;
      reply("TQ1;");
    });
  }
  void reply(char const *text) { client->write(K4RemoteProtocol::build_cat_packet(QString::fromLatin1(text))); }
};

class K4TransportTests : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() {
    // Exercise the same precomputed-waveform path used by the FT8/FT4 UI.
    for (int i = 0; i < 79 * 1920 * 4; ++i)
      foxcom_.wave[i] = float(0.8 * std::sin(6.283185307179586 * 1500 * i / 48000.));
  }
  void transmit_data() {
    QTest::addColumn<QString>("mode");
    QTest::addColumn<int>("encoding");
    QTest::addColumn<int>("latency");
    for (auto mode : {QString("FT8"), QString("FT4")})
      for (int encoding = 0; encoding < 4; ++encoding)
        QTest::newRow(qPrintable(QString("%1-EM%2").arg(mode).arg(encoding)))
          << mode << encoding << (encoding == 3 ? 7 : encoding == 2 ? 3 : 0);
  }
  void transmit() {
    QFETCH(QString, mode);
    QFETCH(int, encoding);
    QFETCH(int, latency);
    FakeK4 radio;
    QVERIFY(radio.server.listen(QHostAddress::LocalHost));
    K4RemoteTransceiver rig(nullptr, "127.0.0.1", radio.server.serverPort(),
                            "test", false, "", encoding, latency, 0.03125f, false, "", 1);
    bool ready = false;
    connect(&rig, &Transceiver::update, this,
            [&ready](Transceiver::TransceiverState const &state, unsigned) { ready = state.ptt(); });
    QSignalSpy errors(&rig, &Transceiver::remote_tx_error);
    rig.start(1);
    QTRY_VERIFY(rig.state().frequency() != 0);
    auto request = rig.state();
    request.online(true);
    request.ptt(true);
    rig.set(request, 2);
    // WSJT-X cannot start its modulator until this readiness update arrives.
    QTRY_VERIFY_WITH_TIMEOUT(rig.state().ptt(), 1000);
    QVERIFY(ready);
    QVERIFY(radio.audio.isEmpty());
    request.tx_audio(true);
    request.jtmode(mode);
    request.symbolslength(79);
    request.framespersymbol(mode == "FT8" ? 1920. : 576.);
    request.trfrequency(1500.);
    request.tonespacing(mode == "FT8" ? -3. : -2.);
    request.synchronize(false);
    request.trperiod(mode == "FT8" ? 15. : 7.5);
    rig.set(request, 3);
    QTRY_VERIFY(radio.audio.size() >= 3);
    QVERIFY(errors.isEmpty());
    auto packet = radio.audio.first();
    QCOMPARE(quint8(packet[2]), quint8(0));
    QCOMPARE(quint8(packet[3]), quint8(encoding));
    QCOMPARE(qFromLittleEndian<quint16>(reinterpret_cast<uchar const *>(packet.constData()+4)),
             quint16(latency == 7 ? 1440 : latency == 3 ? 720 : 240));
    QVERIFY(packet.mid(7) != QByteArray(packet.size()-7, '\0'));
    // Combined stop must honor PTT even when the audio fields also change.
    request.tx_audio(false);
    request.ptt(false);
    rig.set(request, 4);
    QTRY_VERIFY(!radio.transmitting);
    auto count = radio.audio.size();
    QTest::qWait(100);
    QCOMPARE(radio.audio.size(), count);
    QVERIFY(!rig.state().ptt());
    QVERIFY(!radio.commands.contains("TX"));
    // A subsequent transmission starts a new sequence, with no stale audio.
    radio.audio.clear();
    request.ptt(true);
    rig.set(request, 5);
    request.tx_audio(true);
    rig.set(request, 6);
    QTRY_VERIFY(!radio.audio.isEmpty());
    QCOMPARE(quint8(radio.audio.first()[2]), quint8(0));
    rig.stop();
  }
  void tune_and_disconnect() {
    FakeK4 radio;
    QVERIFY(radio.server.listen(QHostAddress::LocalHost));
    K4RemoteTransceiver rig(nullptr, "127.0.0.1", radio.server.serverPort(),
                            "test", false, "", 1, 0, 0.03125f, false, "", 1);
    rig.start(1);
    QTRY_VERIFY(rig.state().frequency() != 0);
    auto request = rig.state();
    request.tune(true);
    rig.set(request, 2);
    QTest::qWait(80);
    QVERIFY(radio.audio.isEmpty());
    request.ptt(true);
    rig.set(request, 3);
    request.tx_audio(true);
    request.jtmode("FT8");
    request.symbolslength(1);
    request.framespersymbol(240.);
    request.synchronize(false);
    rig.set(request, 4);
    QTRY_VERIFY(radio.audio.size() > 5); // continues past a one-symbol message
    radio.client->abort();
    QTRY_VERIFY(!rig.state().ptt());
    auto count = radio.audio.size();
    QTest::qWait(100);
    QCOMPARE(radio.audio.size(), count);
    rig.stop();
  }
  void calibration() {
    FakeK4 radio;
    QVERIFY(radio.server.listen(QHostAddress::LocalHost));
    K4RemoteTransceiver rig(nullptr, "127.0.0.1", radio.server.serverPort(),
                            "test", false, "", 1, 0, 0.03125f, false, "", 1);
    QSignalSpy result(&rig, &Transceiver::remote_input_calibration_finished);
    rig.start(1);
    QTRY_VERIFY(rig.state().frequency() != 0);
    rig.calibrate_remote_input();
    QTRY_VERIFY_WITH_TIMEOUT(!result.isEmpty(), 5000);
    QVERIFY2(result.first()[0].toBool(), qPrintable(result.first()[1].toString()));
    QVERIFY(!radio.audio.isEmpty());
    QVERIFY(radio.commands.contains("TS1"));
    QVERIFY(!radio.test);
    QVERIFY(!radio.commands.contains("TX"));
    QTRY_VERIFY(!radio.transmitting);
    rig.stop();
  }
  void missing_audio_times_out() {
    FakeK4 radio;
    QVERIFY(radio.server.listen(QHostAddress::LocalHost));
    K4RemoteTransceiver rig(nullptr, "127.0.0.1", radio.server.serverPort(),
                            "test", false, "", 1, 0, 0.03125f, false, "", 1);
    QSignalSpy errors(&rig, &Transceiver::remote_tx_error);
    rig.start(1);
    QTRY_VERIFY(rig.state().frequency() != 0);
    auto request = rig.state();
    request.ptt(true);
    rig.set(request, 2);
    QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 2500);
    QVERIFY(errors.first()[0].toString().contains("audio"));
    QVERIFY(!rig.state().ptt());
    QVERIFY(radio.audio.isEmpty());
    rig.stop();
  }
  void protection_stops_stream_data() {
    QTest::addColumn<bool>("stale");
    QTest::newRow("missing-meter") << true;
    QTest::newRow("excessive-alc") << false;
  }
  void protection_stops_stream() {
    QFETCH(bool, stale);
    FakeK4 radio;
    QVERIFY(radio.server.listen(QHostAddress::LocalHost));
    K4RemoteTransceiver rig(nullptr, "127.0.0.1", radio.server.serverPort(),
                            "test", false, "", 1, 0, 0.03125f, false, "", 1);
    QSignalSpy errors(&rig, &Transceiver::remote_tx_error);
    rig.start(1);
    QTRY_VERIFY(rig.state().frequency() != 0);
    auto request = rig.state();
    request.tune(true);
    request.ptt(true);
    rig.set(request, 2);
    QTRY_VERIFY(!radio.audio.isEmpty());
    radio.meters = !stale;
    radio.meter = "TM010000000000;";
    QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), 5000);
    QTRY_VERIFY(!radio.transmitting);
    QVERIFY(!rig.state().ptt());
    auto count = radio.audio.size();
    QTest::qWait(100);
    QCOMPARE(radio.audio.size(), count);
    rig.stop();
  }
};

QTEST_GUILESS_MAIN(K4TransportTests)
#include "tst_k4transport.moc"
