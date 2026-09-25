#include "K4RemoteTransceiver.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <utility>

#include <QDateTime>
#include <QEventLoop>
#include <QHostAddress>
#include <QHostInfo>
#include <QSslCipher>
#include <QSslConfiguration>
#include <QSslPreSharedKeyAuthenticator>

#include "commons.h"
#include "moc_K4RemoteTransceiver.cpp"
#include "widgets/itoneAndicw.h"

extern dec_data dec_data;

namespace {
constexpr double two_pi{6.283185307179586476925286766559};
constexpr int sample_rate{12000};
constexpr int connection_timeout_ms{10000};
constexpr int guard_service_ms{50};
char const *const transceiver_name{"Elecraft K4 Remote"};
} // namespace

QString const &K4RemoteTransceiver::name() {
  static QString const value{QString::fromLatin1(transceiver_name)};
  return value;
}

void K4RemoteTransceiver::register_transceiver(
    TransceiverFactory::Transceivers *registry, unsigned model_number) {
  (*registry)[name()] = TransceiverFactory::Capabilities{
      model_number, TransceiverFactory::Capabilities::k4_remote,
      true,         false,
      false,        true};
}

K4RemoteTransceiver::K4RemoteTransceiver(
    logger_type *logger, QString host, quint16 port, QString password, bool tls,
    QString tls_identity, int encode_mode, int streaming_latency,
    float calibrated_gain, bool has_calibration,
    QString calibration_radio_context, int poll_interval, QObject *parent)
    : PollingTransceiver{logger, poll_interval, parent}, host_{std::move(host)},
      port_{port}, password_{std::move(password)}, tls_{tls},
      tls_identity_{std::move(tls_identity)},
      encode_mode_{qBound(0, encode_mode, 3)},
      streaming_latency_{qBound(0, streaming_latency, 7)},
      frame_samples_{frame_samples_for_latency(streaming_latency_)},
      socket_{new QSslSocket(this)},
      parser_{new K4RemoteProtocolParser(this)},
      tx_timer_{new QTimer(this)}, guard_timer_{new QTimer(this)},
      keepalive_timer_{new QTimer(this)},
      has_calibration_{has_calibration},
      calibration_radio_context_{std::move(calibration_radio_context)},
      tx_control_{std::make_shared<K4RemoteTxControl>()},
      tx_guard_{tx_control_} {
  tx_control_->gain.store(std::isfinite(calibrated_gain)
                              ? qBound(K4RemoteTxGuard::minimum_gain,
                                       calibrated_gain,
                                       K4RemoteTxGuard::maximum_gain)
                              : K4RemoteTxGuard::calibration_start_gain);
  tx_audio_.reset(tx_control_->gain.load());
  clock_.start();

  connect(socket_, &QSslSocket::connected, this,
          &K4RemoteTransceiver::socket_connected);
  connect(socket_, &QSslSocket::encrypted, this,
          &K4RemoteTransceiver::socket_encrypted);
  connect(socket_, &QSslSocket::readyRead, this,
          &K4RemoteTransceiver::socket_ready_read);
  connect(socket_, &QSslSocket::disconnected, this,
          &K4RemoteTransceiver::socket_disconnected);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
  connect(socket_, &QSslSocket::errorOccurred, this,
          &K4RemoteTransceiver::socket_error);
#else
  connect(socket_,
          QOverload<QAbstractSocket::SocketError>::of(&QSslSocket::error), this,
          &K4RemoteTransceiver::socket_error);
#endif
  connect(socket_,
          QOverload<QList<QSslError> const &>::of(&QSslSocket::sslErrors), this,
          &K4RemoteTransceiver::ssl_errors);
  connect(socket_, &QSslSocket::preSharedKeyAuthenticationRequired, this,
          &K4RemoteTransceiver::provide_psk);

  connect(parser_, &K4RemoteProtocolParser::packet_received, this,
          &K4RemoteTransceiver::packet_received);
  connect(parser_, &K4RemoteProtocolParser::cat_received, this,
          &K4RemoteTransceiver::cat_received);
  connect(parser_, &K4RemoteProtocolParser::audio_received, this,
          &K4RemoteTransceiver::audio_received);
  connect(parser_, &K4RemoteProtocolParser::protocol_error, this,
          [this](QString const &message) { connection_error_ = message; });

  tx_timer_->setTimerType(Qt::PreciseTimer);
  tx_timer_->setInterval(qMax(1, qRound(frame_samples_ * 1000. / sample_rate)));
  connect(tx_timer_, &QTimer::timeout, this, &K4RemoteTransceiver::service_tx);
  guard_timer_->setTimerType(Qt::PreciseTimer);
  guard_timer_->setInterval(guard_service_ms);
  connect(guard_timer_, &QTimer::timeout, this,
          &K4RemoteTransceiver::service_guard);
  keepalive_timer_->setInterval(1000);
  connect(keepalive_timer_, &QTimer::timeout, this, [this] {
    send_cat(QStringLiteral("PING%1;").arg(QDateTime::currentSecsSinceEpoch()));
  });
}

int K4RemoteTransceiver::frame_samples_for_latency(int latency) {
  if (latency <= 0)
    return 240;
  if (latency <= 2)
    return 480;
  if (latency <= 5)
    return 720;
  return 1440;
}

int K4RemoteTransceiver::do_start() {
  if (socket_->thread() != thread() || parser_->thread() != thread() ||
      tx_timer_->thread() != thread() || guard_timer_->thread() != thread() ||
      keepalive_timer_->thread() != thread())
    throw error{tr("K4 remote transport objects have invalid thread affinity.")};
  if (host_.trimmed().isEmpty())
    throw error{tr("Enter the K4 remote host name or IP address.")};
  if (!port_)
    throw error{tr("The K4 remote TCP port is invalid.")};
  if (tls_ && !QSslSocket::supportsSsl())
    throw error{tr(
        "TLS is unavailable because the OpenSSL runtime could not be loaded.")};
  if (encode_mode_ >= 2 && !codec_.available())
    throw error{tr("Opus audio was selected but libopus is unavailable: %1")
                    .arg(codec_.error_string())};

  authenticated_ = false;
  tx_requested_ = ptt_ = false;
  test_state_known_ = false;
  have_mode_readback_ = false;
  have_data_mode_readback_ = false;
  initial_mode_captured_ = false;
  connection_error_.clear();
  parser_->clear();
  codec_.reset();
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  startup_loop_ = &loop;
  connect_socket();
  timeout.start(connection_timeout_ms);
  loop.exec();
  startup_loop_ = nullptr;
  if (host_lookup_id_ >= 0) {
    QHostInfo::abortHostLookup(host_lookup_id_);
    host_lookup_id_ = -1;
  }

  if (!authenticated_) {
    keepalive_timer_->stop();
    socket_->abort();
    throw error{connection_error_.isEmpty()
                    ? tr("Timed out connecting or authenticating to %1:%2.")
                          .arg(host_)
                          .arg(port_)
                    : connection_error_};
  }
  if (!initial_mode_captured_) {
    keepalive_timer_->stop();
    authenticated_ = false;
    socket_->abort();
    throw error{
        connection_error_.isEmpty()
            ? tr("Connected to the K4, but CAT mode readback timed out.")
            : connection_error_};
  }
  return 0; // K4 reports frequency to 1 Hz
}

void K4RemoteTransceiver::connect_socket() {
  auto const target = host_.trimmed();
  if (target.endsWith(QStringLiteral(".local"), Qt::CaseInsensitive)) {
    host_lookup_id_ = QHostInfo::lookupHost(
        target, this, [this, target](QHostInfo const &info) {
          host_lookup_id_ = -1;
          if (!startup_loop_ ||
              socket_->state() != QAbstractSocket::UnconnectedState)
            return;
          if (info.error() != QHostInfo::NoError ||
              info.addresses().isEmpty()) {
            connection_error_ =
                tr("Could not resolve %1: %2").arg(target, info.errorString());
            startup_loop_->quit();
            return;
          }
          QString resolved;
          for (auto const &address : info.addresses())
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
              resolved = address.toString();
              break;
            }
          if (resolved.isEmpty())
            resolved = info.addresses().first().toString();
          connect_resolved_socket(resolved);
        });
    return;
  }

  connect_resolved_socket(target);
}

void K4RemoteTransceiver::connect_resolved_socket(QString const &target) {
  if (!tls_) {
    socket_->connectToHost(target, port_);
    return;
  }

  auto config = QSslConfiguration::defaultConfiguration();
  config.setProtocol(QSsl::TlsV1_2OrLater);
  config.setPeerVerifyMode(
      QSslSocket::VerifyNone); // K4 authenticates with PSK, not certificates.
  QList<QSslCipher> psk_ciphers;
  for (auto const &cipher : QSslConfiguration::supportedCiphers())
    if (cipher.name().contains(QStringLiteral("PSK"), Qt::CaseInsensitive) &&
        (cipher.protocol() == QSsl::TlsV1_2 ||
         cipher.protocol() == QSsl::TlsV1_3))
      psk_ciphers.append(cipher);
  if (!psk_ciphers.isEmpty())
    config.setCiphers(psk_ciphers);
  socket_->setSslConfiguration(config);
  socket_->connectToHostEncrypted(target, port_);
}

void K4RemoteTransceiver::socket_connected() {
  socket_->setSocketOption(QAbstractSocket::LowDelayOption, 1);
  socket_->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
  if (!tls_) {
    // Plain K4 remote authentication is the raw lowercase SHA-384 digest.
    socket_->write(K4RemoteProtocol::build_authentication(password_));
    socket_->flush();
  }
}

void K4RemoteTransceiver::socket_encrypted() {
  // TLS-PSK authentication is completed by the handshake. The K4 now emits
  // framed data; the first valid packet is our positive authentication result.
}

void K4RemoteTransceiver::provide_psk(
    QSslPreSharedKeyAuthenticator *authenticator) {
  authenticator->setIdentity(tls_identity_.toUtf8());
  authenticator->setPreSharedKey(password_.toUtf8());
}

void K4RemoteTransceiver::ssl_errors(QList<QSslError> const &) {
  // PSK sessions intentionally have no peer certificate. Cipher selection and
  // the pre-shared secret authenticate the connection.
  socket_->ignoreSslErrors();
}

void K4RemoteTransceiver::socket_ready_read() {
  parser_->parse(socket_->readAll());
}

void K4RemoteTransceiver::socket_disconnected() {
  auto const was_authenticated = authenticated_;
  authenticated_ = false;
  keepalive_timer_->stop();
  tx_timer_->stop();
  guard_timer_->stop();
  tx_guard_.stop();
  tx_kind_ = TxKind::None;
  tx_requested_ = ptt_ = false;
  update_PTT(false);
  Q_EMIT tci_mod_active(false);

  if (!was_authenticated) {
    connection_error_ =
        tr("The K4 closed the connection during authentication. Check the "
           "password and TLS setting.");
    if (startup_loop_)
      startup_loop_->quit();
  } else {
    connection_error_ =
        tr("The K4 remote TLS connection was closed unexpectedly.");
  }
}

void K4RemoteTransceiver::socket_error(QAbstractSocket::SocketError) {
  connection_error_ = socket_->errorString();
  if (startup_loop_)
    startup_loop_->quit();
}

void K4RemoteTransceiver::packet_received(quint8, QByteArray const &) {
  if (!authenticated_)
    authenticated();
}

void K4RemoteTransceiver::authenticated() {
  authenticated_ = true;
  send_cat(QStringLiteral("RDY;"));
  send_cat(QStringLiteral("K41;"));
  send_cat(QStringLiteral("ER1;"));
  send_cat(QStringLiteral("EM%1;").arg(encode_mode_));
  send_cat(QStringLiteral("SL%1;").arg(streaming_latency_));
  send_cat(QStringLiteral("#DSM;#HDSM;#PKM;#AR;#NB$;#NBL$;#FRZ;#FPS;#SCL;"
                          "RT$;RO$;VT;VT$;KP;PL;PL$;RP;"));
  send_cat(QStringLiteral("FA;FB;MD;DT;FT;TQ;TS;PC;SIRC1;"));
  // RDY can refresh server-side stream state without echoing SL. QK4
  // reasserts it after the state dump so transmitter packet sizing cannot
  // drift from the K4's frame bundling.
  send_cat(QStringLiteral("SL%1;").arg(streaming_latency_));
  keepalive_timer_->start();
}

void K4RemoteTransceiver::send_cat(QString const &command) {
  if (authenticated_ && socket_->state() == QAbstractSocket::ConnectedState) {
    socket_->write(K4RemoteProtocol::build_cat_packet(command));
    socket_->flush();
  }
}

void K4RemoteTransceiver::cat_received(QString const &response) {
  for (auto const &value : response.split(';', Qt::SkipEmptyParts))
    parse_cat_command(value.trimmed());
}

void K4RemoteTransceiver::parse_cat_command(QString const &command) {
  bool ok = false;
  if (command.startsWith("FA") && command.size() > 2) {
    auto const value = command.mid(2).toULongLong(&ok);
    if (ok)
      frequency_ = value;
  } else if (command.startsWith("FB") && command.size() > 2) {
    auto const value = command.mid(2).toULongLong(&ok);
    if (ok)
      tx_frequency_ = value;
  } else if (command.startsWith("MD") && command.size() > 2 &&
             command[2].isDigit()) {
    k4_mode_ = command.mid(2).toInt(&ok);
    if (ok) {
      have_mode_readback_ = true;
      mode_ = mode_from_k4(k4_mode_, k4_data_mode_);
    }
  } else if (command.startsWith("DT") && command.size() > 2) {
    k4_data_mode_ = command.mid(2).toInt(&ok);
    if (ok) {
      have_data_mode_readback_ = true;
      mode_ = mode_from_k4(k4_mode_, k4_data_mode_);
    }
  } else if (command.startsWith("PC") && command.size() >= 6) {
    double value = 0.;
    bool milliwatts = false;
    if (K4RemoteProtocol::parse_rf_power(command, &value, &milliwatts)) {
      if (!milliwatts)
        tx_volume_ = value;
      Q_EMIT rf_power_setting(value, milliwatts);
    }
  } else if (command.startsWith("FT") && command.size() > 2)
    split_ = command[2] == QLatin1Char('1');
  else if (command == "TX" || command == "TQ1")
    ptt_ = true;
  else if (command == "RX" || command == "TQ0")
    ptt_ = false;
  else if (command == "TS0" || command == "TS1") {
    test_mode_ = command == "TS1";
    test_state_known_ = true;
    if (!test_mode_ && tx_kind_ == TxKind::Calibration && tx_guard_.active())
      stop_tx_for_guard(tr("Calibration stopped: K4 TEST mode was disabled."));
  } else if (command.startsWith("LI") && command.size() > 2)
    line_input_ = command;
  else if (command.startsWith("MG") && command.size() > 2)
    mic_gain_ = command;
  else if (command.startsWith("CP") && command.size() > 2)
    compression_ = command;
  else if (command.startsWith("TE") && command.size() > 2)
    tx_equalizer_ = command;

  if (command.startsWith("TM")) {
    auto const action = tx_guard_.meter(command, clock_.elapsed());
    if (action == K4RemoteTxGuard::Action::Reduced)
      Q_EMIT remote_input_calibration_progress(
          tr("K4 ALC was high; reducing remote audio drive."),
          tx_control_->gain.load());
    else if (action == K4RemoteTxGuard::Action::Calibrated) {
      tx_timer_->stop();
      guard_timer_->stop();
      tx_guard_.stop();
      // Leave readyRead before waiting for TS0. QAbstractSocket does not emit
      // readyRead recursively, even while a nested event loop is running.
      QTimer::singleShot(0, this, [this] {
        finish_calibration(true, tr("Remote input calibration completed."));
      });
    }
    else if (action == K4RemoteTxGuard::Action::Tripped)
      stop_tx_for_guard(tx_guard_.reason());
  }

  if (!initial_mode_captured_ && have_mode_readback_ &&
      have_data_mode_readback_) {
    initial_k4_mode_ = k4_mode_;
    initial_k4_data_mode_ = k4_data_mode_;
    initial_mode_captured_ = true;
    if (startup_loop_)
      startup_loop_->quit();
  }
}

auto K4RemoteTransceiver::mode_from_k4(int mode, int data_mode) -> MODE {
  switch (mode) {
  case 1:
    return LSB;
  case 2:
    return USB;
  case 3:
    return CW;
  case 4:
    return FM;
  case 5:
    return AM;
  case 6:
    return data_mode == 0 ? DIG_U : DIG_U;
  case 7:
    return CW_R;
  case 9:
    return DIG_L;
  default:
    return UNK;
  }
}

QString K4RemoteTransceiver::frequency_command(char vfo, Frequency frequency) {
  return QStringLiteral("F%1%2;")
      .arg(QChar{vfo})
      .arg(frequency, 11, 10, QLatin1Char('0'));
}

void K4RemoteTransceiver::do_frequency(Frequency frequency, MODE mode, bool) {
  send_cat(frequency_command('A', frequency));
  if (mode != UNK)
    do_mode(mode);
}

void K4RemoteTransceiver::do_tx_frequency(Frequency frequency, MODE mode,
                                          bool) {
  if (frequency) {
    // QK4 always operates the K4 on VFO A with split disabled.  Refuse a
    // request from any inherited WSJT-X path that would silently change that
    // invariant or the operator's VFO B.
    emit remote_tx_error(
        tr("K4 Remote does not support split transmit operation."));
    send_cat(QStringLiteral("FT0;FT;"));
  }
  if (mode != UNK)
    do_mode(mode);
}

void K4RemoteTransceiver::set_data_a() {
  // DATA-A is MD6 + DT0 on the K4. Read-back commands make state convergence
  // deterministic and mirror QK4's FT8/FT4 entry sequence.
  send_cat(QStringLiteral("MD6;DT0;MD;DT;LI;MG;CP;TE;"));
}

void K4RemoteTransceiver::do_mode(MODE mode) {
  switch (mode) {
  case DIG_U:
    set_data_a();
    break;
  case DIG_L:
    send_cat(QStringLiteral("MD9;MD;"));
    break;
  case USB:
    send_cat(QStringLiteral("MD2;MD;"));
    break;
  case LSB:
    send_cat(QStringLiteral("MD1;MD;"));
    break;
  case CW:
    send_cat(QStringLiteral("MD3;MD;"));
    break;
  case CW_R:
    send_cat(QStringLiteral("MD7;MD;"));
    break;
  case AM:
    send_cat(QStringLiteral("MD5;MD;"));
    break;
  case FM:
  case DIG_FM:
    send_cat(QStringLiteral("MD4;MD;"));
    break;
  default:
    break;
  }
}

void K4RemoteTransceiver::begin_guard(bool calibration) {
  tx_guard_.acknowledge();
  ++tx_generation_;
  if (!tx_generation_)
    ++tx_generation_;
  if (!tx_guard_.begin(tx_generation_, clock_.elapsed(), calibration))
    throw error{tr("Digital TX protection is latched. Stop transmitting before "
                   "retrying.")};
  tx_audio_.reset(tx_control_->gain.load());
  tx_sequence_ = 0;
  last_meter_query_ = 0;
  send_cat(QStringLiteral("TM1;TM;"));
  guard_timer_->start();
}

void K4RemoteTransceiver::do_ptt(bool on) {
  if (on) {
    if (test_mode_) {
      Q_EMIT remote_tx_error(
          tr("Turn K4 TEST off before transmitting on air."));
      return;
    }
    if (split_) {
      Q_EMIT remote_tx_error(
          tr("Turn K4 split off before transmitting."));
      return;
    }
    auto const context = radio_input_context();
    if (!has_calibration_ || context.isEmpty() ||
        context != calibration_radio_context_) {
      has_calibration_ = false;
      // QK4 permits PTT without a prior calibration. Start conservatively and
      // retain the live ALC/meter guard; calibration remains available to
      // optimize the network-audio drive.
      tx_control_->gain.store(K4RemoteTxGuard::calibration_start_gain);
      tx_audio_.reset(tx_control_->gain.load());
    }
    set_data_a();
    if (!tx_guard_.active())
      begin_guard(false);
    // QK4's remote PTT opens its audio gate. The first audio packet keys the
    // K4; waiting for TQ1 here prevents WSJT-X from ever starting modulation.
    tx_requested_ = true;
    update_PTT(true);
  } else {
    tx_requested_ = false;
    update_PTT(false);
    send_cat(QStringLiteral("RX;TM0;"));
    tx_guard_.stop();
    guard_timer_->stop();
    if (tx_kind_ != TxKind::Calibration)
      do_modulator_stop(true);
  }
}

void K4RemoteTransceiver::do_poll() {
  if (!authenticated_ || socket_->state() != QAbstractSocket::ConnectedState)
    throw error{
        tr("The K4 remote connection was lost: %1").arg(socket_->errorString())};
  send_cat(QStringLiteral("FA;FB;MD;DT;FT;TQ;PC;"));
  update_rx_frequency(frequency_);
  update_other_frequency(split_ ? tx_frequency_ : 0);
  update_split(split_);
  update_mode(mode_);
  update_PTT(tx_requested_);
}

void K4RemoteTransceiver::do_stop() {
  tx_requested_ = false;
  update_PTT(false);
  tx_timer_->stop();
  guard_timer_->stop();
  keepalive_timer_->stop();
  tx_guard_.stop();
  if (authenticated_) {
    send_cat(QStringLiteral("RX;TM0;"));
    if (initial_mode_captured_) {
      send_cat(QStringLiteral("MD%1;").arg(initial_k4_mode_));
      if (initial_k4_mode_ == 6 || initial_k4_mode_ == 9)
        send_cat(QStringLiteral("DT%1;").arg(initial_k4_data_mode_));
    }
    send_cat(QStringLiteral("RRN;"));
  }
  authenticated_ = false;
  if (host_lookup_id_ >= 0) {
    QHostInfo::abortHostLookup(host_lookup_id_);
    host_lookup_id_ = -1;
  }
  socket_->disconnectFromHost();
  if (socket_->state() != QAbstractSocket::UnconnectedState)
    socket_->waitForDisconnected(500);
  parser_->clear();
}

void K4RemoteTransceiver::do_audio(bool on) {
  audio_enabled_ = on;
  if (on) {
    dec_data.params.kin = 0;
    samples_since_signal_ = 0;
    last_period_ms_ = 999999;
  }
}
void K4RemoteTransceiver::do_period(double value) {
  period_ = value > 0. ? value : 15.;
}
void K4RemoteTransceiver::do_blocksize(qint32 value) {
  block_size_ = qMax(1, value);
}
void K4RemoteTransceiver::do_spread(double) {}
void K4RemoteTransceiver::do_nsym(int value) { tx_symbols_ = qMax(0, value); }
void K4RemoteTransceiver::do_trfrequency(double value) {
  tx_frequency_hz_ = value;
}
void K4RemoteTransceiver::do_volume(qreal value) { rx_volume_ = value; }
void K4RemoteTransceiver::do_txvolume(qreal value) {
  if (!authenticated_ || !std::isfinite(value) || value <= 0.)
    return;
  auto const normalized =
      value <= 10. ? qBound(0.1, qRound(value * 10.) / 10., 10.)
                   : qBound(11., double(qRound(value)), 110.);
  if (qFuzzyCompare(tx_volume_ + 1., normalized + 1.))
    return;
  tx_volume_ = normalized;
  send_cat(K4RemoteProtocol::build_rf_power_command(normalized));
}

void K4RemoteTransceiver::audio_received(QByteArray const &payload) {
  if (!audio_enabled_ || ptt_)
    return;
  auto const pcm = codec_.decode(payload);
  if (!pcm.isEmpty())
    write_rx_audio(pcm);
}

void K4RemoteTransceiver::write_rx_audio(QByteArray const &pcm) {
  auto const *samples = reinterpret_cast<qint16 const *>(pcm.constData());
  auto count = pcm.size() / static_cast<int>(sizeof(qint16));
  auto const capacity = int(sizeof(dec_data.d2) / sizeof(dec_data.d2[0]));
  // dec_data is shared with the decoder thread. Never trust a concurrently
  // reset or stale index as a destination pointer.
  if (dec_data.params.kin < 0 || dec_data.params.kin > capacity) {
    dec_data.params.kin = 0;
    samples_since_signal_ = 0;
  }
  auto const ms = static_cast<unsigned>(QDateTime::currentMSecsSinceEpoch() %
                                        qint64(qMax(1., period_) * 1000.));
  if (ms < last_period_ms_ / 2) {
    dec_data.params.kin = 0;
    samples_since_signal_ = 0;
  }
  last_period_ms_ = ms;

  int consumed = 0;
  while (consumed < count && dec_data.params.kin < capacity) {
    auto const until_signal = block_size_ - int(samples_since_signal_);
    auto const accepted =
        qMin(count - consumed,
             qMin(capacity - dec_data.params.kin, qMax(1, until_signal)));
    std::memcpy(&dec_data.d2[dec_data.params.kin], samples + consumed,
                accepted * sizeof(qint16));
    consumed += accepted;
    dec_data.params.kin += accepted;
    samples_since_signal_ += accepted;
    if (samples_since_signal_ >= block_size_) {
      samples_since_signal_ = 0;
      Q_EMIT tciframeswritten(dec_data.params.kin);
    }
  }
}

void K4RemoteTransceiver::do_tune(bool on) {
  tuning_ = on;
  if (on) {
    tx_kind_ = TxKind::Tune;
    tx_sample_ = tx_silence_ = 0;
    tx_phase_ = 0.;
    tx_timer_->start();
  } else if (tx_kind_ == TxKind::Tune)
    do_modulator_stop(true);
}

void K4RemoteTransceiver::do_modulator_start(
    QString mode, unsigned symbols, double frames_per_symbol, double frequency,
    double tone_spacing, bool synchronize, bool fast_mode, double,
    double tr_period) {
  if (tuning_) {
    // WSJT-X calls transmit() after Tune's PTT-ready update too. Preserve the
    // continuous tune generator instead of replacing it with a finite message.
    tx_kind_ = TxKind::Tune;
    tx_timer_->start();
    Q_EMIT tci_mod_active(true);
    return;
  }
  if (!symbols || frames_per_symbol <= 0.)
    throw error{tr("The transmit mode supplied invalid symbol timing.")};
  tx_mode_ = std::move(mode);
  tx_symbols_ = symbols;
  tx_frames_per_symbol_ = frames_per_symbol;
  tx_frequency_hz_ = frequency;
  tx_tone_spacing_ = tone_spacing;
  synchronize_ = synchronize;
  fast_mode_ = fast_mode;
  period_ = tr_period;
  tx_kind_ = TxKind::Message;
  tx_sample_ = 0;
  tx_phase_ = 0.;
  tx_silence_ = 0;
  tx_cw_sample_ = 0;
  tx_cw_symbols_ = period_ > 16. ? qBound(0, int(icw[0]),
                                         NUM_CW_SYMBOLS - 1) : 0;
  tx_cw_gain_ = 0.;
  tx_envelope_ = 1.;

  if (synchronize_ && !fast_mode_ && tx_mode_ != QStringLiteral("Echo")) {
    int delay = 1000;
    if ((tx_mode_ == QStringLiteral("FT8") &&
         tx_frames_per_symbol_ == 1920.) ||
        (tx_mode_ == QStringLiteral("FST4") &&
         tx_frames_per_symbol_ == 720.) ||
        (tx_mode_ == QStringLiteral("Q65") &&
         tx_frames_per_symbol_ <= 3600.))
      delay = 500;
    if (tx_mode_ == QStringLiteral("FT8") &&
        tx_frames_per_symbol_ == 1024.)
      delay = 400;
    if (tx_mode_ == QStringLiteral("FT4"))
      delay = 300;
    auto const period_ms = qMax(1, qRound(period_ * 1000.));
    auto const elapsed = int(QDateTime::currentMSecsSinceEpoch() % period_ms);
    if (elapsed < delay)
      tx_silence_ = qint64(delay - elapsed) * sample_rate / 1000;
    else
      tx_sample_ = qint64(elapsed - delay) * sample_rate / 1000;
  }
  tx_timer_->start();
  Q_EMIT tci_mod_active(true);
}

void K4RemoteTransceiver::do_modulator_stop(bool) {
  tx_timer_->stop();
  tx_kind_ = TxKind::None;
  tuning_ = false;
  Q_EMIT tci_mod_active(false);
}

QVector<qint16> K4RemoteTransceiver::generate_tx_frame(int count) {
  QVector<qint16> result(count, 0);
  for (int i = 0; i != count; ++i) {
    if (tx_silence_ > 0) {
      --tx_silence_;
      continue;
    }

    double frequency = tx_frequency_hz_;
    if (tx_kind_ == TxKind::Message) {
      auto const complete = fast_mode_
                                ? tx_sample_ >= qint64(qMax(0., period_ - 0.5) *
                                                      sample_rate)
                                : tx_sample_ >=
                                      qint64(tx_symbols_ * tx_frames_per_symbol_);
      if (complete) {
        if (tx_cw_symbols_) {
          tx_kind_ = TxKind::CwId;
          tx_phase_ = 0.;
        } else {
          result.resize(i);
          break;
        }
      }
      if (tx_kind_ == TxKind::Message) {
        auto const symbol = static_cast<unsigned>(
            qint64(tx_sample_ / tx_frames_per_symbol_) % tx_symbols_);
        if (tx_tone_spacing_ < 0. && itone[0] < 100) {
          // WSJT-X supplies an already filtered 48 kHz waveform for FT8,
          // FT4, and other modes marked by negative tone spacing.
          result[i] = qint16(qBound(
              -32766, qRound(32767. * foxcom_.wave[tx_sample_ * 4]), 32766));
          ++tx_sample_;
          continue;
        }
        frequency =
            itone[0] >= 100
                ? itone[0]
                : frequency +
                      itone[symbol] * (tx_tone_spacing_ == 0.
                                           ? sample_rate / tx_frames_per_symbol_
                                           : tx_tone_spacing_);
      }
    }
    if (tx_kind_ == TxKind::CwId) {
      auto const cw_symbol = tx_cw_sample_ / (2560 / 4) + 1;
      if (cw_symbol > tx_cw_symbols_) {
        result.resize(i);
        break;
      }
      auto const target = icw[cw_symbol] ? 1. : 0.;
      tx_cw_gain_ += qBound(-1. / 60., target - tx_cw_gain_, 1. / 60.);
      ++tx_cw_sample_;
    }
    tx_phase_ += two_pi * frequency / sample_rate;
    if (tx_phase_ >= two_pi)
      tx_phase_ -= two_pi;
    if (tx_kind_ == TxKind::Message) {
      auto const fade_start = fast_mode_
                                  ? qMax(0., period_ - 0.5) * sample_rate - 204.
                                  : (tx_symbols_ - 0.017) * tx_frames_per_symbol_;
      if (tx_sample_ > fade_start)
        tx_envelope_ *= 0.98;
      ++tx_sample_;
    }
    result[i] = qint16(std::sin(tx_phase_) * 32766. *
                       (tx_kind_ == TxKind::CwId ? tx_cw_gain_ : tx_envelope_));
  }
  return result;
}

void K4RemoteTransceiver::send_audio(QVector<qint16> samples) {
  if (samples.isEmpty() || !tx_guard_.active())
    return;
  auto const generation = tx_guard_.active() ? tx_generation_ : 0;
  if (!tx_audio_.process(samples, *tx_control_, generation))
    return;
  auto const encoded = codec_.encode(samples, encode_mode_);
  if (encoded.isEmpty()) {
    stop_tx_for_guard(tr("TX stopped: the selected K4 audio encoding failed."));
    return;
  }
  auto const packet = K4RemoteProtocol::build_audio_packet(
      encoded, tx_sequence_++, encode_mode_,
      static_cast<quint16>(samples.size()));
  auto const backlog_limit = qMax<qint64>(8192, packet.size() * 4LL);
  if (socket_->bytesToWrite() > backlog_limit) {
    stop_tx_for_guard(tr("TX stopped: the K4 audio link stalled."));
    return;
  }
  if (socket_->write(packet) != packet.size()) {
    stop_tx_for_guard(tr("TX stopped: the K4 audio link rejected a packet."));
    return;
  }
  tx_guard_.audio_accepted(clock_.elapsed());
}

void K4RemoteTransceiver::service_tx() {
  if (tx_kind_ == TxKind::None || !authenticated_ || !tx_guard_.active())
    return;
  auto samples = generate_tx_frame(frame_samples_);
  auto const message_complete = samples.size() < frame_samples_ &&
                                (tx_kind_ == TxKind::Message ||
                                 tx_kind_ == TxKind::CwId);
  // K4/Opus packetization requires the frame size selected by SL. Preserve
  // program length, but zero-pad the final partial packet on the wire.
  if (message_complete && !samples.isEmpty())
    samples.resize(frame_samples_);
  send_audio(samples);
  if (message_complete)
    do_modulator_stop(false);
}

void K4RemoteTransceiver::service_guard() {
  auto const now = clock_.elapsed();
  if (now - last_meter_query_ >= 250) {
    send_cat(QStringLiteral("TM;"));
    last_meter_query_ = now;
  }
  if (tx_guard_.tick(now) == K4RemoteTxGuard::Action::Tripped)
    stop_tx_for_guard(tx_guard_.reason());
}

void K4RemoteTransceiver::stop_tx_for_guard(QString const &reason) {
  tx_requested_ = false;
  update_PTT(false);
  tx_guard_.stop();
  tx_timer_->stop();
  guard_timer_->stop();
  send_cat(QStringLiteral("RX;TM0;"));
  auto const calibration = tx_kind_ == TxKind::Calibration;
  tx_kind_ = TxKind::None;
  Q_EMIT tci_mod_active(false);
  if (calibration)
    QTimer::singleShot(0, this, [this, reason] { finish_calibration(false, reason); });
  else
    Q_EMIT remote_tx_error(reason);
}

void K4RemoteTransceiver::calibrate_remote_input() {
  if (!authenticated_) {
    Q_EMIT remote_input_calibration_finished(
        false, tr("Connect to the K4 before calibrating."), 0.f, QString{});
    return;
  }
  if (ptt_ || tx_kind_ != TxKind::None) {
    Q_EMIT remote_input_calibration_finished(
        false, tr("Stop transmitting before calibrating."), 0.f, QString{});
    return;
  }
  if (radio_input_context().isEmpty()) {
    send_cat(QStringLiteral("LI;MG;CP;TE;"));
    Q_EMIT remote_input_calibration_finished(
        false,
        tr("K4 input settings are not available yet. Wait for CAT readback and "
           "try again."),
        0.f, QString{});
    return;
  }

  // Capture the current TEST state, then guarantee that calibration cannot
  // radiate. TS1 is restored to TS0 only when this routine enabled it.
  if (!read_test_state()) {
    Q_EMIT remote_input_calibration_finished(
        false,
        tr("K4 TEST state could not be confirmed; calibration was not "
           "started."),
        0.f, QString{});
    return;
  }
  restore_test_mode_ = !test_mode_;
  if (restore_test_mode_ && !set_test_state(true)) {
    finish_calibration(
        false, tr("K4 TEST mode could not be confirmed; calibration was not "
                  "started."));
    return;
  }
  set_data_a();
  tx_kind_ = TxKind::Calibration;
  tx_frequency_hz_ = 1500.;
  tx_sample_ = tx_silence_ = 0;
  tx_phase_ = 0.;
  try {
    begin_guard(true);
    tx_timer_->start();
    Q_EMIT remote_input_calibration_progress(
        tr("Sending a protected 1500 Hz tone in K4 TEST mode."),
        tx_control_->gain.load());
  } catch (std::exception const &exception) {
    finish_calibration(false, QString::fromLocal8Bit(exception.what()));
  }
}

void K4RemoteTransceiver::cancel_remote_input_calibration() {
  if (tx_kind_ == TxKind::Calibration)
    finish_calibration(false, tr("Remote input calibration cancelled."));
}

void K4RemoteTransceiver::finish_calibration(bool success,
                                             QString const &message) {
  QString final_message{message};
  auto const gain = tx_control_->gain.load();
  tx_timer_->stop();
  guard_timer_->stop();
  tx_guard_.stop();
  send_cat(QStringLiteral("RX;TM0;"));
  if (restore_test_mode_ && !set_test_state(false)) {
    success = false;
    final_message =
        tr("Calibration stopped: restoration of K4 TEST mode could not be "
           "confirmed. Verify that TEST is off before transmitting.");
  }
  restore_test_mode_ = false;
  tx_kind_ = TxKind::None;
  auto const context = radio_input_context();
  if (success) {
    has_calibration_ = true;
    calibration_radio_context_ = context;
  }
  Q_EMIT tci_mod_active(false);
  Q_EMIT remote_input_calibration_finished(success, final_message, gain,
                                           success ? context : QString{});
}

bool K4RemoteTransceiver::read_test_state(int timeout_ms) {
  test_state_known_ = false;
  QEventLoop wait;
  QTimer timeout;
  timeout.setSingleShot(true);
  connect(&timeout, &QTimer::timeout, &wait, &QEventLoop::quit);
  auto const response = connect(parser_, &K4RemoteProtocolParser::cat_received,
                                &wait, [this, &wait](QString const &) {
                                  if (test_state_known_)
                                    wait.quit();
                                });
  send_cat(QStringLiteral("TS;"));
  timeout.start(timeout_ms);
  wait.exec();
  disconnect(response);
  return test_state_known_;
}

bool K4RemoteTransceiver::set_test_state(bool enabled, int timeout_ms) {
  test_state_known_ = false;
  QEventLoop wait;
  QTimer timeout;
  timeout.setSingleShot(true);
  connect(&timeout, &QTimer::timeout, &wait, &QEventLoop::quit);
  auto const response =
      connect(parser_, &K4RemoteProtocolParser::cat_received, &wait,
              [this, enabled, &wait](QString const &) {
                if (test_state_known_ && test_mode_ == enabled)
                  wait.quit();
              });
  send_cat(enabled ? QStringLiteral("TS1;TS;") : QStringLiteral("TS0;TS;"));
  timeout.start(timeout_ms);
  wait.exec();
  disconnect(response);
  return test_state_known_ && test_mode_ == enabled;
}

QString K4RemoteTransceiver::radio_input_context() const {
  if (line_input_.isEmpty() || mic_gain_.isEmpty() || compression_.isEmpty() ||
      tx_equalizer_.isEmpty())
    return {};
  return QStringLiteral("%1:%2|EM%3|%4|%5|%6|%7")
      .arg(host_.trimmed().toLower())
      .arg(port_)
      .arg(encode_mode_)
      .arg(line_input_)
      .arg(mic_gain_)
      .arg(compression_)
      .arg(tx_equalizer_);
}
