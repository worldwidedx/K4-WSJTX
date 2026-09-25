#ifndef K4_REMOTE_TRANSCEIVER_HPP__
#define K4_REMOTE_TRANSCEIVER_HPP__

#include <memory>

#include <QElapsedTimer>
#include <QSslSocket>
#include <QTimer>
#include <QVector>

#include "K4RemoteAudioCodec.hpp"
#include "K4RemoteProtocol.hpp"
#include "K4RemoteTxGuard.hpp"
#include "PollingTransceiver.hpp"
#include "TransceiverFactory.hpp"

class QSslPreSharedKeyAuthenticator;
class QEventLoop;

class K4RemoteTransceiver final : public PollingTransceiver {
  Q_OBJECT

public:
  static QString const &name();
  static void register_transceiver(TransceiverFactory::Transceivers *,
                                   unsigned model_number);

  explicit K4RemoteTransceiver(logger_type *, QString host, quint16 port,
                               QString password, bool tls, QString tls_identity,
                               int encode_mode, int streaming_latency,
                               float calibrated_gain, bool has_calibration,
                               QString calibration_radio_context,
                               int poll_interval, QObject *parent = nullptr);

  void calibrate_remote_input() override;
  void cancel_remote_input_calibration() override;

protected:
  int do_start() override;
  void do_stop() override;
  void do_frequency(Frequency, MODE, bool) override;
  void do_tx_frequency(Frequency, MODE, bool) override;
  void do_mode(MODE) override;
  void do_ptt(bool) override;
  void do_poll() override;
  bool do_pre_update() override { return true; }

  void do_audio(bool) override;
  void do_tune(bool) override;
  void do_period(double) override;
  void do_blocksize(qint32) override;
  void do_spread(double) override;
  void do_nsym(int) override;
  void do_trfrequency(double) override;
  void do_volume(qreal) override;
  void do_txvolume(qreal) override;
  void do_modulator_start(QString, unsigned, double, double, double, bool, bool,
                          double, double) override;
  void do_modulator_stop(bool) override;

private Q_SLOTS:
  void socket_connected();
  void socket_encrypted();
  void socket_ready_read();
  void socket_disconnected();
  void socket_error(QAbstractSocket::SocketError);
  void ssl_errors(QList<QSslError> const &);
  void provide_psk(QSslPreSharedKeyAuthenticator *);
  void packet_received(quint8, QByteArray const &);
  void cat_received(QString const &);
  void audio_received(QByteArray const &);
  void service_tx();
  void service_guard();

private:
  enum class TxKind { None, Message, Tune, Calibration };

  void connect_socket();
  void connect_resolved_socket(QString const &host);
  void authenticated();
  void send_cat(QString const &);
  void send_audio(QVector<qint16>);
  void parse_cat_command(QString const &);
  void write_rx_audio(QByteArray const &);
  void set_data_a();
  void begin_guard(bool calibration);
  void finish_calibration(bool, QString const &);
  void stop_tx_for_guard(QString const &);
  bool read_test_state(int timeout_ms = 2500);
  bool set_test_state(bool enabled, int timeout_ms = 2500);
  QString radio_input_context() const;
  QVector<qint16> generate_tx_frame(int count);
  static int frame_samples_for_latency(int);
  static QString frequency_command(char vfo, Frequency);
  static MODE mode_from_k4(int mode, int data_mode);

  QString host_;
  quint16 port_;
  QString password_;
  bool tls_;
  QString tls_identity_;
  int encode_mode_;
  int streaming_latency_;
  int frame_samples_;

  // These are heap children rather than QObject value members so moving the
  // transceiver to WSJT-X's rig thread also moves every asynchronous transport
  // object. QObjects without a parent retain their construction-thread
  // affinity, which is unsafe for QSslSocket and QTimer.
  QSslSocket *socket_;
  K4RemoteProtocolParser *parser_;
  K4RemoteAudioCodec codec_;
  QTimer *tx_timer_;
  QTimer *guard_timer_;
  QTimer *keepalive_timer_;
  QElapsedTimer clock_;
  QEventLoop *startup_loop_{nullptr};
  int host_lookup_id_{-1};
  QString connection_error_;
  bool authenticated_{false};
  bool audio_enabled_{true};
  quint8 tx_sequence_{0};

  Frequency frequency_{0};
  Frequency tx_frequency_{0};
  MODE mode_{UNK};
  int k4_mode_{0};
  int k4_data_mode_{0};
  int initial_k4_mode_{0};
  int initial_k4_data_mode_{0};
  bool have_mode_readback_{false};
  bool have_data_mode_readback_{false};
  bool initial_mode_captured_{false};
  bool split_{false};
  bool ptt_{false};
  // WSJT-X PTT means permission to start remote audio. Radio TX readback is
  // separate: K4 keys from the first audio packet, so it cannot gate startup.
  bool tx_requested_{false};
  bool test_mode_{false};
  bool test_state_known_{false};
  bool has_calibration_{false};
  QString calibration_radio_context_;
  QString line_input_;
  QString mic_gain_;
  QString compression_;
  QString tx_equalizer_;

  double period_{15.};
  qint32 block_size_{3456};
  qint64 samples_since_signal_{0};
  unsigned last_period_ms_{999999};
  qreal rx_volume_{0.};
  qreal tx_volume_{0.}; // K4 RF-power setpoint in watts, not audio attenuation.

  TxKind tx_kind_{TxKind::None};
  QString tx_mode_{"FT8"};
  unsigned tx_symbols_{79};
  double tx_frames_per_symbol_{1920.};
  double tx_frequency_hz_{1500.};
  double tx_tone_spacing_{6.25};
  double tx_phase_{0.};
  qint64 tx_sample_{0};
  qint64 tx_silence_{0};
  bool synchronize_{true};
  bool tuning_{false};

  std::shared_ptr<K4RemoteTxControl> tx_control_;
  K4RemoteTxGuard tx_guard_;
  K4RemoteTxAudio tx_audio_;
  quint64 tx_generation_{0};
  qint64 last_meter_query_{0};
  bool restore_test_mode_{false};
};

#endif
