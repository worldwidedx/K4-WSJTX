#ifndef DECODER_IPC_HPP
#define DECODER_IPC_HPP

#include <QByteArray>
#include <QtGlobal>
#include <QString>
#include "lib/NativeSharedMemory.hpp"

#include <array>

#include "commons.h"

namespace DecoderIpc
{
  // FT8 reads the complete 15-second buffer even when decoding starts earlier.
  constexpr std::size_t Ft8SampleCount {15 * RX_SAMPLE_RATE};

  struct Ft8MtdPayload
  {
    std::array<short, Ft8SampleCount> samples {};
    decoder_params_t params {};
    decoder_input_metadata_t metadata {};
  };

  struct Completion
  {
    qint32 synchronized;
    qint32 decoded;
    qint32 average;
    qint32 generation;
  };

  class Session;

  class Generation
  {
  public:
    Generation () = default;
    qint32 value () const { return value_; }
  private:
    friend class Session;
    explicit Generation (qint32 value) : value_ {value} {}
    qint32 value_ {0};
  };

  class SampleIdentity
  {
  public:
    SampleIdentity () = default;
    explicit operator bool () const { return owner_ && value_; }
    bool operator== (SampleIdentity const& other) const
    { return owner_ == other.owner_ && value_ == other.value_; }
    bool operator!= (SampleIdentity const& other) const { return !(*this == other); }
  private:
    friend class Session;
    SampleIdentity (Session const * owner, quint64 value)
      : owner_ {owner}, value_ {value} {}
    Session const * owner_ {nullptr};
    quint64 value_ {0};
  };

  struct SampleSnapshot
  {
    SampleIdentity samples;
    decoder_input_metadata_t metadata {};
    qint32 lastAttempt {0};
    int mode {0};
    int trPeriod {0};
    explicit operator bool () const { return bool {samples}; }
  };

  class InputState
  {
  public:
    void beginInput ();
    qint64 inputId () const { return inputId_; }
    qint64 analysisId () const { return analysisId_; }
    decoder_input_metadata_t nextMetadata (
        decoder_params_t const& params, SampleSnapshot const * reused = nullptr);
  private:
    qint64 nextInputId_ {0};
    qint64 nextAnalysisId_ {0};
    qint64 inputId_ {0};
    qint64 analysisId_ {0};
    qint32 lastAttempt_ {0};
  };

  // Sources are borrowed only for the duration of synchronous submission.
  class Request
  {
  public:
    static Request snapshot (dec_data_t const& source, decoder_input_metadata_t metadata = {});
    static Request reuse (dec_data_t const& source, SampleIdentity samples,
                          decoder_input_metadata_t metadata = {});
    static Request ft8 (Ft8MtdPayload const& source);
    int sampleCount () const { return options ().kin; }
    int mode () const { return options ().nmode; }
    decoder_params_t const& options () const;
    decoder_input_metadata_t const& metadata () const { return metadata_; }
  private:
    friend class Session;
    enum class Kind { Snapshot, Reuse, Ft8 };
    Request (Kind kind, dec_data_t const * source,
             Ft8MtdPayload const * compact, SampleIdentity samples = {},
             decoder_input_metadata_t metadata = {});
    Kind kind_;
    dec_data_t const * source_;
    Ft8MtdPayload const * compact_;
    SampleIdentity samples_;
    decoder_params_t options_;
    decoder_input_metadata_t metadata_;
  };

  enum class Status { Ok, Unavailable, Busy, InvalidRequest, StaleSamples,
                      StaleGeneration, Incompatible, Orphaned, CreateFailed };

  struct Submission
  {
    Status status {Status::Unavailable};
    Generation generation;
    SampleIdentity samples;
    explicit operator bool () const { return Status::Ok == status; }
  };

  struct Diagnostics
  {
    qint32 state {-1};
    qint32 generation {-1};
    qint32 progress {-1};
  };

  class Session
  {
  public:
    Status open (QString const& key, QString const& workerLockPath);
    QString name () const { return name_; }
    QString workerLockPath () const { return workerLockPath_; }
    QString errorString () const { return error_; }
    qint64 size () const { return memory_.size (); }
    bool reset ();
    void beginStartup ();
    Status acceptReady (int version);
    bool ready () const { return ready_; }
    void shutdown ();
    void detach ();
    bool canReuseSamples (int mode, int period) const;
    Request prepareRequest (dec_data_t const& source, bool copySamples,
                            InputState& inputs) const;
    Submission submit (Request const& request);
    SampleIdentity samples () const { return completedSnapshot_.samples; }
    SampleSnapshot const& completedSnapshot () const { return completedSnapshot_; }
    bool accepts (qint32 generation) const;
    Status complete (Completion const& completion);
    Diagnostics diagnostics () const;
    void abort ();
  private:
    shared_dec_data_t * storage ();
    shared_dec_data_t const * storage () const;
    NativeSharedMemory memory_;
    QString name_;
    QString workerLockPath_;
    QString error_;
    qint32 lastGeneration_ {0};
    Generation activeGeneration_;
    quint64 lastSamples_ {0};
    SampleSnapshot activeSnapshot_;
    SampleSnapshot completedSnapshot_;
    bool ready_ {false};
  };

  QString memoryName (QString const& lockIdentity);
  qint32 nextGeneration (qint32 current);
  bool hasUsableSize (qint64 size);
  qint32 state (shared_dec_data_t const& shared);
  qint32 generation (shared_dec_data_t const& shared);
  qint32 progress (shared_dec_data_t const& shared);
  qint32 protocolVersion (shared_dec_data_t const& shared);
  void initialize (shared_dec_data_t& shared);
  void shutdown (shared_dec_data_t& shared);
  bool publish (shared_dec_data_t& shared, dec_data_t const& payload,
                bool copySamples, qint32 generation,
                decoder_input_metadata_t const& metadata = {});
  bool publishFt8Mtd (shared_dec_data_t& shared,
                      Ft8MtdPayload const& payload, qint32 generation);
  bool claim (shared_dec_data_t& shared, qint32& generation);
  bool finish (shared_dec_data_t& shared, qint32 generation);
  bool consume (shared_dec_data_t& shared, qint32 generation);
  bool parseStart (QByteArray line, qint32 * generation);
  bool parseCompletion (QByteArray line, Completion * completion);
}

#endif
