#include "K4RemoteTxGuard.hpp"

#include <algorithm>
#include <cmath>

#include <QtGlobal>

// C++11 requires definitions for constexpr static data members that are
// odr-used through Qt's reference-taking qBound/qMin/qMax helpers.
constexpr float K4RemoteTxGuard::minimum_gain;
constexpr float K4RemoteTxGuard::calibration_start_gain;
constexpr float K4RemoteTxGuard::maximum_gain;
constexpr int K4RemoteTxGuard::reduce_alc;
constexpr int K4RemoteTxGuard::stop_alc;
constexpr int K4RemoteTxGuard::meter_timeout_ms;

K4RemoteTxGuard::K4RemoteTxGuard(std::shared_ptr<K4RemoteTxControl> control)
    : control_{std::move(control)} {}

bool K4RemoteTxGuard::begin(quint64 generation, qint64 now, bool calibration) {
  if (active_ || latched_ || !generation)
    return false;

  generation_ = generation;
  active_ = true;
  calibrating_ = calibration;
  started_ = adjusted_ = last_meter_ = now;
  stable_since_ = first_audio_ = last_audio_ = high_since_ = -1;
  last_reduction_ = last_high_sample_ = -1;
  high_samples_ = 0;
  meter_diagnostic_.clear();
  reason_.clear();
  control_->audio_fault.store(0, std::memory_order_release);
  control_->calibrating.store(calibration, std::memory_order_release);
  auto gain = control_->gain.load(std::memory_order_relaxed);
  control_->gain.store(calibration ? calibration_start_gain
                       : std::isfinite(gain)
                           ? qBound(minimum_gain, gain, maximum_gain)
                           : minimum_gain,
                       std::memory_order_release);
  control_->generation.store(generation, std::memory_order_release);
  return true;
}

void K4RemoteTxGuard::stop() {
  control_->close(generation_);
  control_->calibrating.store(false, std::memory_order_release);
  active_ = false;
}

void K4RemoteTxGuard::acknowledge() {
  if (!active_) {
    latched_ = false;
    reason_.clear();
  }
}

void K4RemoteTxGuard::audio_accepted(qint64 now) {
  if (!active_)
    return;
  if (first_audio_ < 0) {
    first_audio_ = now;
    adjusted_ = now;
  }
  last_audio_ = now;
}

auto K4RemoteTxGuard::trip(QString const &reason) -> Action {
  if (!active_)
    return Action::None;
  stop();
  latched_ = true;
  reason_ = reason + (meter_diagnostic_.isEmpty()
                          ? QString{}
                          : QString{"\n"} + meter_diagnostic_);
  return Action::Tripped;
}

auto K4RemoteTxGuard::tick(qint64 now) -> Action {
  if (!active_)
    return Action::None;
  if (control_->audio_fault.load(std::memory_order_acquire) == generation_)
    return trip("TX stopped: digital audio exceeded its headroom limit.");
  if (!control_->allows(generation_))
    return trip("TX stopped: digital audio was cancelled.");
  if (now < last_meter_ || now - last_meter_ >= meter_timeout_ms)
    return trip("TX stopped: fresh K4 ALC readings are unavailable. Check the "
                "connection and metering.");
  if (high_samples_ >= 3 && high_since_ >= 0 && now - high_since_ >= 1500)
    return trip("TX stopped: ALC stayed high after automatic audio-drive "
                "reduction. Check K4 input settings.");
  if (calibrating_ && now - started_ >= 15000)
    return trip(
        "Calibration stopped: a stable audio level could not be established.");
  if (now - (last_audio_ < 0 ? started_ : last_audio_) >= 1500)
    return trip(calibrating_ ? "Calibration stopped: the test tone is not streaming."
                            : "TX stopped: transmit audio is not streaming.");
  return Action::None;
}

auto K4RemoteTxGuard::meter(QString const &command, qint64 now) -> Action {
  if (!active_ || !command.startsWith("TM") || command == "TM0" ||
      command == "TM1")
    return Action::None;
  if (command.size() != 14)
    return trip("TX stopped: the K4 returned an invalid ALC meter response.");
  for (int i = 2; i != command.size(); ++i)
    if (!command[i].isDigit())
      return trip("TX stopped: the K4 returned an invalid ALC meter response.");
  if (tick(now) == Action::Tripped)
    return Action::Tripped;

  last_meter_ = now;
  auto const alc = command.mid(2, 3).toInt();
  auto const compression = command.mid(5, 3).toInt();
  auto const rf = command.mid(8, 3).toInt();
  meter_diagnostic_ =
      QString{"%1; - raw ALC %2 - drive %3 dB"}.arg(command).arg(alc).arg(
          20. * std::log10(control_->gain.load()), 0, 'f', 1);

  if (calibrating_ && rf > 0)
    return trip("Calibration stopped: RF output was reported in K4 TEST mode.");
  if (compression > 0)
    return trip("TX stopped: speech compression was detected. Use DATA-A with "
                "compression off.");
  if (alc >= stop_alc)
    return trip("TX stopped: K4 ALC is excessively high. Check the radio's "
                "input level.");

  if (calibrating_ &&
      (first_audio_ < 0 || now - first_audio_ < 600 || now - adjusted_ < 600)) {
    high_since_ = stable_since_ = last_high_sample_ = -1;
    high_samples_ = 0;
    return Action::None;
  }

  if (alc < reduce_alc) {
    high_since_ = last_high_sample_ = -1;
    high_samples_ = 0;
    if (calibrating_) {
      if (first_audio_ < 0 || now - first_audio_ < 600 ||
          now - last_audio_ > 500 || now - adjusted_ < 600) {
        stable_since_ = -1;
        return Action::None;
      }
      if (alc >= 3) {
        if (stable_since_ < 0)
          stable_since_ = now;
        if (now - stable_since_ >= 1200)
          return Action::Calibrated;
      } else {
        stable_since_ = -1;
        if (now - adjusted_ >= 600) {
          auto const gain = control_->gain.load();
          if (gain >= maximum_gain)
            return trip("Calibration stopped: input gain or passband needs "
                        "adjustment; safe audio limit reached.");
          control_->gain.store(qMin(maximum_gain, gain * 1.41421356f),
                               std::memory_order_release);
          adjusted_ = now;
        }
      }
    }
    return Action::None;
  }

  stable_since_ = -1;
  if (high_since_ < 0)
    high_since_ = now;
  if (last_high_sample_ < 0 || now - last_high_sample_ >= 200) {
    ++high_samples_;
    last_high_sample_ = now;
  }
  if (last_reduction_ >= 0 && now - last_reduction_ < 400)
    return Action::None;
  auto const gain = control_->gain.load(std::memory_order_relaxed);
  if (gain <= minimum_gain) {
    if (high_samples_ >= 3 && now - high_since_ >= 600)
      return trip("TX stopped: ALC remains high at the PCM resolution limit. "
                  "Check K4 input gain.");
    return Action::None;
  }
  control_->gain.store(qMax(minimum_gain, gain * 0.70710678f),
                       std::memory_order_release);
  adjusted_ = last_reduction_ = now;
  if (calibrating_) {
    high_since_ = last_high_sample_ = -1;
    high_samples_ = 0;
  }
  return Action::Reduced;
}

bool K4RemoteTxAudio::process(QVector<qint16> &samples,
                              K4RemoteTxControl &control, quint64 generation) {
  if (!control.allows(generation))
    return false;
  auto const target = control.gain.load(std::memory_order_acquire);
  bool valid = std::isfinite(target) && target >= 0.f && target <= 0.5f &&
               std::isfinite(gain_) && gain_ >= 0.f && gain_ <= 0.5f;
  for (auto sample : samples)
    valid = valid && std::abs(int(sample)) < 32767;
  if (!valid) {
    control.audio_fault.store(generation, std::memory_order_release);
    control.close(generation);
    return false;
  }
  auto const next = control.calibrating.load(std::memory_order_acquire)
                        ? target
                        : qMin(gain_, target);
  auto const ramp = qMin(60, samples.size());
  auto const previous = gain_;
  for (int i = 0; i != samples.size(); ++i) {
    auto const gain =
        i < ramp ? previous + (next - previous) * (i + 1) / ramp : next;
    samples[i] = qint16(qRound(samples[i] * gain));
  }
  gain_ = next;
  return control.allows(generation);
}
