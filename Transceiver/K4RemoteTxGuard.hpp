#ifndef K4_REMOTE_TX_GUARD_HPP__
#define K4_REMOTE_TX_GUARD_HPP__

#include <atomic>
#include <memory>

#include <QString>
#include <QVector>

// Shared by the CAT and audio paths.  Closing a generation immediately
// prevents further synthesis and socket delivery.
struct K4RemoteTxControl {
  std::atomic<quint64> generation{0};
  std::atomic<float> gain{0.5f};
  std::atomic<quint64> audio_fault{0};
  std::atomic<bool> calibrating{false};

  bool allows(quint64 id) const {
    return id && generation.load(std::memory_order_acquire) == id;
  }

  void close(quint64 id) {
    generation.compare_exchange_strong(id, 0, std::memory_order_acq_rel);
  }
};

// QK4's monotonic-time ALC policy, shared by calibration and live FT8/FT4.
class K4RemoteTxGuard {
public:
  enum class Action { None, Reduced, Tripped, Calibrated };

  static constexpr float minimum_gain = 1.f / 32768.f;
  static constexpr float calibration_start_gain = 1.f / 32.f;
  static constexpr float maximum_gain = 0.5f;
  static constexpr int reduce_alc = 6;
  static constexpr int stop_alc = 10;
  static constexpr int meter_timeout_ms = 1500;

  explicit K4RemoteTxGuard(std::shared_ptr<K4RemoteTxControl>);

  bool begin(quint64 generation, qint64 now, bool calibration = false);
  void stop();
  void acknowledge();
  void audio_accepted(qint64 now);
  Action meter(QString const &command, qint64 now);
  Action tick(qint64 now);
  Action trip(QString const &reason);

  bool active() const { return active_; }
  bool latched() const { return latched_; }
  bool calibrating() const { return calibrating_; }
  QString const &reason() const { return reason_; }
  QString const &meter_diagnostic() const { return meter_diagnostic_; }

private:
  std::shared_ptr<K4RemoteTxControl> control_;
  quint64 generation_{0};
  bool active_{false};
  bool latched_{false};
  bool calibrating_{false};
  qint64 started_{0};
  qint64 stable_since_{-1};
  qint64 adjusted_{0};
  qint64 first_audio_{-1};
  qint64 last_audio_{-1};
  qint64 last_meter_{0};
  qint64 high_since_{-1};
  qint64 last_reduction_{-1};
  qint64 last_high_sample_{-1};
  int high_samples_{0};
  QString meter_diagnostic_;
  QString reason_;
};

class K4RemoteTxAudio {
public:
  void reset(float gain) { gain_ = gain; }
  bool process(QVector<qint16> &, K4RemoteTxControl &, quint64 generation);

private:
  float gain_{0.5f};
};

#endif
