#include "DecoderIpc.hpp"

#include "lib/decoder_ipc_control.h"
#include <QCryptographicHash>
#include <QFile>
#include <QThread>
#include <QElapsedTimer>
#include "lib/DecoderWorkerLock.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace
{
  int attemptCount (decoder_params_t const& params)
  {
    return params.nmode == 8 && params.ndiskdat && !params.nagain
      && (!params.lmultift8 || params.ndecoderstart < 2) ? 3 : 1;
  }

  bool publishRequest (shared_dec_data_t& shared, dec_data_t const& payload,
                       decoder_params_t const& params, bool copySamples,
                       qint32 generation, decoder_input_metadata_t const& metadata);

  int sampleCountToCopy (int period)
  {
    switch (period)
      {
      case 7: // FT4's 7.5-second period is stored as an integer.
      case 15:
      case 30:
      case 60:
      case 120:
      case 300:
      case 900:
      case 1800:
        // Legacy decoders read fixed windows beyond the received sample count.
        return std::min (NTMAX, std::max (60, period)) * RX_SAMPLE_RATE;
      default:
        return NTMAX * RX_SAMPLE_RATE;
      }
  }

  bool hasCurrentProtocol (shared_dec_data_t const& shared)
  {
    return DECODER_IPC_LAYOUT_OK == decoder_ipc_validate_layout (
        &shared, sizeof shared, nullptr);
  }

  bool parseField (QByteArray const& field, qint32& value)
  {
    bool ok {false};
    auto const parsed = field.toInt (&ok);
    if (!ok) return false;
    value = parsed;
    return true;
  }

  bool parseGeneration (QByteArray const& text, qint32& generation)
  {
    if (text.isEmpty ()) return false;
    for (auto const digit: text)
      {
        if (digit < '0' || digit > '9') return false;
      }
    return parseField (text, generation) && generation > 0;
  }
}

void DecoderIpc::InputState::beginInput ()
{
  inputId_ = ++nextInputId_;
  analysisId_ = ++nextAnalysisId_;
  lastAttempt_ = 0;
}

decoder_input_metadata_t DecoderIpc::InputState::nextMetadata (
    decoder_params_t const& params, SampleSnapshot const * reused)
{
  if (!inputId_) beginInput ();
  auto const input = reused ? reused->metadata.input_id : inputId_;
  auto analysis = reused ? reused->metadata.analysis_id : analysisId_;
  auto lastAttempt = reused ? reused->lastAttempt : lastAttempt_;
  if (input == inputId_ && analysis == analysisId_)
    lastAttempt = std::max (lastAttempt, lastAttempt_);
  auto const count = attemptCount (params);
  if (params.nagain || lastAttempt > std::numeric_limits<qint32>::max () - count)
    {
      analysis = ++nextAnalysisId_;
      lastAttempt = 0;
    }
  auto const firstAttempt = lastAttempt + 1;
  if (input == inputId_)
    {
      analysisId_ = analysis;
      lastAttempt_ = lastAttempt + count;
    }
  auto const validSamples = reused ? reused->metadata.valid_samples
    : std::max (0, std::min (params.kin, NTMAX * RX_SAMPLE_RATE));
  return {input, analysis, firstAttempt, validSamples};
}

DecoderIpc::Request::Request (Kind kind, dec_data_t const * source,
                              Ft8MtdPayload const * compact, SampleIdentity samples,
                              decoder_input_metadata_t metadata)
  : kind_ {kind}, source_ {source}, compact_ {compact}, samples_ {samples}
  , options_ {compact ? compact->params : source->params}
  , metadata_ {compact ? compact->metadata : metadata}
{
}

DecoderIpc::Request DecoderIpc::Request::snapshot (dec_data_t const& source,
                                                decoder_input_metadata_t metadata)
{
  return {Kind::Snapshot, &source, nullptr, {}, metadata};
}

DecoderIpc::Request DecoderIpc::Request::reuse (dec_data_t const& source,
                                             SampleIdentity samples,
                                             decoder_input_metadata_t metadata)
{
  return {Kind::Reuse, &source, nullptr, samples, metadata};
}

DecoderIpc::Request DecoderIpc::Request::ft8 (Ft8MtdPayload const& source)
{
  return {Kind::Ft8, nullptr, &source};
}

decoder_params_t const& DecoderIpc::Request::options () const
{
  return options_;
}

QString DecoderIpc::memoryName (QString const& lockIdentity)
{
  auto const token = QCryptographicHash::hash (lockIdentity.toUtf8 (), QCryptographicHash::Sha256)
    .toHex ().left (24);
#ifdef Q_OS_WIN
  return QStringLiteral ("Local\\wsjt-") + QString::fromLatin1 (token);
#else
  return QStringLiteral ("/wsjt-") + QString::fromLatin1 (token);
#endif
}

DecoderIpc::Status DecoderIpc::Session::open (QString const& key, QString const& workerLockPath)
{
  detach ();
  error_.clear ();
  name_ = key;
  workerLockPath_ = workerLockPath;
  auto const nativeName = key.toStdString ();
  auto stopExisting = [&] {
    if (memory_.attach (nativeName))
      {
        if (DECODER_IPC_LAYOUT_OK == decoder_ipc_validate_layout (
              memory_.data (), memory_.size (), nullptr))
          DecoderIpc::shutdown (*static_cast<shared_dec_data_t *> (memory_.data ()));
        memory_.detach ();
      }
  };
#ifdef Q_OS_WIN
  for (int attempt = 0; ; ++attempt)
    {
      if (memory_.create (nativeName, sizeof (shared_dec_data_t))) break;
      error_ = QString::fromStdString (memory_.errorString ());
      if (!memory_.alreadyExists ()) return Status::CreateFailed;
      if (attempt == 3)
        {
          error_.prepend (QStringLiteral ("Previous jt9 decoder shared memory remained after shutdown attempts; wait for it to exit, then retry: "));
          return Status::Orphaned;
        }
      stopExisting ();
      QThread::sleep (1);
    }
#else
  stopExisting ();
  DecoderWorkerLock workerLock {QFile::encodeName (workerLockPath).constData ()};
  QElapsedTimer deadline;
  deadline.start ();
  while (!workerLock.tryLock ())
    {
      if (!workerLock.busy ())
        {
          error_ = QStringLiteral ("Cannot lock decoder worker: %1")
            .arg (QString::fromStdString (workerLock.errorString ()));
          return Status::CreateFailed;
        }
      if (deadline.elapsed () >= 3000)
        {
          error_ = QStringLiteral ("Previous jt9 decoder is still running after the shutdown wait. Wait for it to exit, then retry.");
          return Status::Orphaned;
        }
      QThread::msleep (50);
    }
  std::string error;
  if (!NativeSharedMemory::remove (nativeName, &error))
    {
      error_ = QString::fromStdString (error);
      return Status::CreateFailed;
    }
  if (!memory_.create (nativeName, sizeof (shared_dec_data_t)))
    {
      error_ = QString::fromStdString (memory_.errorString ());
      return Status::CreateFailed;
    }
#endif
  error_.clear ();
  initialize (*static_cast<shared_dec_data_t *> (memory_.data ()));
  return Status::Ok;
}

shared_dec_data_t const * DecoderIpc::Session::storage () const
{
  if (!hasUsableSize (memory_.size ()) || !memory_.data ()) return nullptr;
  auto const * shared = static_cast<shared_dec_data_t const *> (memory_.data ());
  return hasCurrentProtocol (*shared) ? shared : nullptr;
}

shared_dec_data_t * DecoderIpc::Session::storage ()
{
  return const_cast<shared_dec_data_t *> (static_cast<Session const *> (this)->storage ());
}

bool DecoderIpc::Session::reset ()
{
  auto * shared = storage ();
  if (!shared) return false;
  beginStartup ();
  initialize (*shared);
  return true;
}

void DecoderIpc::Session::beginStartup ()
{
  ready_ = false;
  abort ();
}

DecoderIpc::Status DecoderIpc::Session::acceptReady (int version)
{
  auto const * shared = storage ();
  if (ready_ || version != DECODER_IPC_VERSION || !shared
      || state (*shared) != DECODER_IPC_IDLE || generation (*shared) != 0)
    {
      ready_ = false;
      error_ = QStringLiteral ("Invalid or incompatible decoder startup handshake");
      return Status::Incompatible;
    }
  ready_ = true;
  return Status::Ok;
}

void DecoderIpc::Session::shutdown ()
{
  ready_ = false;
  if (auto * shared = storage ()) DecoderIpc::shutdown (*shared);
}

void DecoderIpc::Session::detach ()
{
  beginStartup ();
  if (memory_.isAttached ()) memory_.detach ();
}

bool DecoderIpc::Session::canReuseSamples (int mode, int period) const
{
  return completedSnapshot_ && completedSnapshot_.mode == mode
    && completedSnapshot_.trPeriod == period;
}

DecoderIpc::Request DecoderIpc::Session::prepareRequest (
    dec_data_t const& source, bool copySamples, InputState& inputs) const
{
  auto const reuse = !copySamples
    && canReuseSamples (source.params.nmode, source.params.ntrperiod);
  auto request = reuse ? Request::reuse (source, completedSnapshot_.samples)
                       : Request::snapshot (source);
  if (!reuse && !copySamples)
    {
      request.options_.newdat = true;
      request.options_.nagain = false;
    }
  request.metadata_ = inputs.nextMetadata (
    request.options_, reuse ? &completedSnapshot_ : nullptr);
  // The producer's kin remains its live append cursor, independent of retained audio.
  if (reuse) request.options_.kin = request.metadata_.valid_samples;
  return request;
}

DecoderIpc::Submission DecoderIpc::Session::submit (Request const& request)
{
  if (!ready_) return {Status::Unavailable, {}, {}};
  auto * shared = storage ();
  if (!shared) return {Status::Incompatible, {}, {}};
  if (activeGeneration_.value () || DECODER_IPC_IDLE != state (*shared))
    return {Status::Busy, {}, {}};
  if (request.sampleCount () < 0 || request.sampleCount () > NTMAX * RX_SAMPLE_RATE)
    return {Status::InvalidRequest, {}, {}};
  if (Request::Kind::Reuse == request.kind_
      && (!completedSnapshot_ || request.samples_ != completedSnapshot_.samples))
    return {Status::StaleSamples, {}, {}};
  auto const generation = nextGeneration (lastGeneration_);
  bool const refreshSamples = Request::Kind::Reuse != request.kind_
    || shared->payload.params.nmode != request.mode ()
    || shared->payload.params.ntrperiod != request.options ().ntrperiod
    || shared->metadata.input_id != request.metadata_.input_id;
  auto options = request.options ();
  if (Request::Kind::Reuse == request.kind_ && refreshSamples)
    {
      options.newdat = true;
      options.nagain = false;
    }
  auto const count = attemptCount (options);
  if (request.metadata_.attempt_no > std::numeric_limits<qint32>::max () - count + 1)
    return {Status::InvalidRequest, {}, {}};
  bool const published = Request::Kind::Ft8 == request.kind_
    ? publishFt8Mtd (*shared, *request.compact_, generation)
    : publishRequest (*shared, *request.source_, options,
                      Request::Kind::Snapshot == request.kind_, generation, request.metadata_);
  if (!published) return {Status::InvalidRequest, {}, {}};
  lastGeneration_ = generation;
  activeGeneration_ = Generation {generation};
  activeSnapshot_ = {
    refreshSamples ? SampleIdentity {this, ++lastSamples_} : completedSnapshot_.samples,
    shared->metadata, shared->metadata.attempt_no + (count - 1),
    shared->payload.params.nmode, shared->payload.params.ntrperiod};
  return {Status::Ok, activeGeneration_, activeSnapshot_.samples};
}

bool DecoderIpc::Session::accepts (qint32 generation) const
{
  return generation > 0 && generation == activeGeneration_.value ();
}

DecoderIpc::Status DecoderIpc::Session::complete (Completion const& completion)
{
  if (!accepts (completion.generation)) return Status::StaleGeneration;
  auto * shared = storage ();
  if (!shared) return Status::Unavailable;
  if (!consume (*shared, completion.generation)) return Status::Busy;
  completedSnapshot_ = activeSnapshot_;
  activeGeneration_ = {};
  activeSnapshot_ = {};
  return Status::Ok;
}

DecoderIpc::Diagnostics DecoderIpc::Session::diagnostics () const
{
  auto const * shared = storage ();
  return shared ? Diagnostics {state (*shared), generation (*shared), progress (*shared)}
                : Diagnostics {};
}

void DecoderIpc::Session::abort ()
{
  activeGeneration_ = {};
  activeSnapshot_ = {};
  completedSnapshot_ = {};
}

qint32 DecoderIpc::nextGeneration (qint32 current)
{
  return current <= 0 || current == std::numeric_limits<qint32>::max ()
    ? 1 : current + 1;
}

bool DecoderIpc::hasUsableSize (qint64 size)
{
  return size >= static_cast<qint64> (sizeof (shared_dec_data_t));
}

qint32 DecoderIpc::state (shared_dec_data_t const& shared)
{
  return decoder_ipc_atomic_load (&shared.control.state);
}

qint32 DecoderIpc::generation (shared_dec_data_t const& shared)
{
  return decoder_ipc_atomic_load (&shared.control.generation);
}

qint32 DecoderIpc::progress (shared_dec_data_t const& shared)
{
  return decoder_ipc_atomic_load (&shared.control.progress);
}

qint32 DecoderIpc::protocolVersion (shared_dec_data_t const& shared)
{
  return decoder_ipc_atomic_load (&shared.control.version);
}

void DecoderIpc::initialize (shared_dec_data_t& shared)
{
  std::memset (&shared, 0, sizeof (shared));
  decoder_ipc_expected_layout (&shared.layout);
  decoder_ipc_control_initialize (&shared.control.generation, &shared.control.state,
                                  &shared.control.version, &shared.control.progress);
}

void DecoderIpc::shutdown (shared_dec_data_t& shared)
{
  if (hasCurrentProtocol (shared))
    decoder_ipc_control_shutdown (&shared.control.state, &shared.control.version);
}

namespace
{
bool publishRequest (shared_dec_data_t& shared, dec_data_t const& payload,
                     decoder_params_t const& params, bool copySamples,
                     qint32 generation, decoder_input_metadata_t const& metadata)
{
  using namespace DecoderIpc;
  if (!hasCurrentProtocol (shared)
      || DECODER_IPC_IDLE != state (shared)
      || generation <= 0)
    {
      return false;
    }

  // A different mode or period must not reuse an incomplete sample snapshot.
  bool const contextChanged = !copySamples
    && (shared.payload.params.nmode != params.nmode
        || shared.payload.params.ntrperiod != params.ntrperiod
        || shared.metadata.input_id != metadata.input_id);
  copySamples = copySamples || contextChanged;
  if (copySamples)
    {
      std::memcpy (shared.payload.ss, payload.ss, sizeof payload.ss);
      std::memcpy (shared.payload.savg, payload.savg, sizeof payload.savg);
      std::memcpy (shared.payload.sred, payload.sred, sizeof payload.sred);
      std::memcpy (shared.payload.d2, payload.d2,
                   sampleCountToCopy (params.ntrperiod) * sizeof payload.d2[0]);
    }
  shared.payload.params = params;
  if (contextChanged)
    {
      shared.payload.params.newdat = true;
      shared.payload.params.nagain = false;
    }
  shared.metadata = metadata;
  return decoder_ipc_control_publish (&shared.control.generation,
                                      &shared.control.state,
                                      &shared.control.version,
                                      &shared.control.progress, generation);
}
}

bool DecoderIpc::publish (shared_dec_data_t& shared, dec_data_t const& payload,
                          bool copySamples, qint32 generation,
                          decoder_input_metadata_t const& metadata)
{
  return publishRequest (shared, payload, payload.params, copySamples, generation, metadata);
}

bool DecoderIpc::publishFt8Mtd (shared_dec_data_t& shared,
                                Ft8MtdPayload const& payload,
                                qint32 generation)
{
  if (!hasCurrentProtocol (shared)
      || DECODER_IPC_IDLE != state (shared)
      || generation <= 0
      || 8 != payload.params.nmode
      || !payload.params.lmultift8)
    {
      return false;
    }

  shared.payload.params = payload.params;
  std::memcpy (shared.payload.d2, payload.samples.data (),
               sizeof payload.samples);
  shared.metadata = payload.metadata;
  return decoder_ipc_control_publish (&shared.control.generation,
                                      &shared.control.state,
                                      &shared.control.version,
                                      &shared.control.progress, generation);
}

bool DecoderIpc::claim (shared_dec_data_t& shared, qint32& generation)
{
  return hasCurrentProtocol (shared)
    && DECODER_IPC_CLAIMED == decoder_ipc_control_try_claim (
      &shared.control.generation, &shared.control.state, &shared.control.version,
      &generation);
}

bool DecoderIpc::finish (shared_dec_data_t& shared, qint32 generation)
{
  return hasCurrentProtocol (shared)
    && decoder_ipc_control_finish (&shared.control.generation,
                                     &shared.control.state,
                                     &shared.control.version, generation);
}

bool DecoderIpc::consume (shared_dec_data_t& shared, qint32 generation)
{
  return hasCurrentProtocol (shared)
    && decoder_ipc_control_consume (&shared.control.generation,
                                      &shared.control.state,
                                      &shared.control.version, generation);
}

bool DecoderIpc::parseStart (QByteArray line, qint32 * generation)
{
  if (!generation) return false;
  if (line.endsWith ('\n')) line.chop (1);
  if (line.endsWith ('\r')) line.chop (1);

  QByteArray const prefix {"<DecodeStarted> gen="};
  if (!line.startsWith (prefix)) return false;

  qint32 parsed {0};
  if (!parseGeneration (line.mid (prefix.size ()), parsed)) return false;
  *generation = parsed;
  return true;
}

bool DecoderIpc::parseCompletion (QByteArray line, Completion * completion)
{
  if (!completion) return false;
  if (line.endsWith ('\n')) line.chop (1);
  if (line.endsWith ('\r')) line.chop (1);

  QByteArray const prefix {"<DecodeFinished>"};
  QByteArray const generationPrefix {" gen="};
  constexpr int synchronizedWidth {4};
  constexpr int decodedWidth {4};
  constexpr int averageWidth {9};
  auto const synchronizedOffset = prefix.size ();
  auto const decodedOffset = synchronizedOffset + synchronizedWidth;
  auto const averageOffset = decodedOffset + decodedWidth;
  auto const generationOffset = averageOffset + averageWidth;
  if (!line.startsWith (prefix)
      || line.mid (generationOffset, generationPrefix.size ()) != generationPrefix)
    {
      return false;
    }

  Completion parsed {};
  if (!parseField (line.mid (synchronizedOffset, synchronizedWidth),
                   parsed.synchronized)
      || !parseField (line.mid (decodedOffset, decodedWidth), parsed.decoded)
      || !parseField (line.mid (averageOffset, averageWidth), parsed.average))
    {
      return false;
    }

  auto const generationText = line.mid (generationOffset + generationPrefix.size ());
  if (!parseGeneration (generationText, parsed.generation))
    {
      return false;
    }
  *completion = parsed;
  return true;
}
