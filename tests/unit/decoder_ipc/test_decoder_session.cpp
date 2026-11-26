#include <QtTest>
#include <QUuid>
#include <QTemporaryDir>
#include <QFile>
#include <QScopeGuard>
#include <QTextCodec>

#include <memory>
#include <limits>

#include "DecoderIpc.hpp"
#include "lib/NativeSharedMemory.hpp"

class TestDecoderSession final : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void ownsGenerationsAndSamples ();
  void sharedLockUsesNativePath ();
  void compactSnapshotAndContextRefresh ();
  void preparedReuseRetainsCompletedInput_data ();
  void preparedReuseRetainsCompletedInput ();
  void incompatibleSnapshotLeavesNextReceptionUntouched_data ();
  void incompatibleSnapshotLeavesNextReceptionUntouched ();
  void diskPassesReserveAttemptNumbers ();
  void deferredSnapshotKeepsItsInputAfterRollover ();
  void reuseStartsNewAnalysisBeforeAttemptOverflow ();
  void resetRejectsCorruption ();
};

void TestDecoderSession::sharedLockUsesNativePath ()
{
#ifdef Q_OS_WIN
  QSKIP ("The native worker lock is POSIX-only");
#else
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  auto * previousCodec = QTextCodec::codecForLocale ();
  auto restoreCodec = qScopeGuard ([&] { QTextCodec::setCodecForLocale (previousCodec); });
#ifndef Q_OS_DARWIN
  QTextCodec::setCodecForLocale (QTextCodec::codecForName ("ISO-8859-1"));
#endif
  auto const path = directory.filePath (QString::fromUtf8 ("décoder.lock"));
  auto cleanup = qScopeGuard ([&] { QFile::remove (path); });
  DecoderIpc::Session session;
  auto const name = DecoderIpc::memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (name, path), DecoderIpc::Status::Ok);
  QVERIFY (QFile::exists (path));
#endif
}

void TestDecoderSession::ownsGenerationsAndSamples ()
{
  using namespace DecoderIpc;
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  Session session;
  auto const key = memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (key, directory.filePath ("worker.lock")), Status::Ok);
  QVERIFY (!session.ready ());
  auto unavailable = std::make_unique<dec_data_t> ();
  QCOMPARE (session.submit (Request::snapshot (*unavailable)).status, Status::Unavailable);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION - 1), Status::Incompatible);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  NativeSharedMemory peer;
  QVERIFY2 (peer.attach (key.toStdString ()), peer.errorString ().c_str ());
  auto& shared = *static_cast<shared_dec_data_t *> (peer.data ());
  auto source = std::make_unique<dec_data_t> ();
  source->params.nmode = 8;
  source->params.ntrperiod = 15;
  source->params.kin = 150000;
  source->params.newdat = true;
  source->d2[0] = 17;
  source->d2[60 * RX_SAMPLE_RATE - 1] = 29;

  QCOMPARE (session.submit (Request::reuse (*source, {})).status, Status::StaleSamples);
  decoder_input_metadata_t metadata {11, 12, 1, 150000};
  auto first = session.submit (Request::snapshot (*source, metadata));
  QVERIFY (first);
  QVERIFY (first.samples);
  QVERIFY (!session.samples ());
  QVERIFY (!session.completedSnapshot ());
  QCOMPARE (first.generation.value (), 1);
  QCOMPARE (shared.metadata.input_id, metadata.input_id);
  QCOMPARE (shared.metadata.analysis_id, metadata.analysis_id);
  QCOMPARE (shared.metadata.attempt_no, metadata.attempt_no);
  QCOMPARE (shared.metadata.valid_samples, metadata.valid_samples);
  QCOMPARE (shared.payload.d2[0], short {17});
  QCOMPARE (shared.payload.d2[60 * RX_SAMPLE_RATE - 1], short {29});
  QCOMPARE (session.submit (Request::snapshot (*source)).status, Status::Busy);
  QVERIFY (!session.accepts (0));
  QVERIFY (session.accepts (1));
  QCOMPARE (session.complete ({0, 0, 0, 2}), Status::StaleGeneration);
  qint32 generation {0};
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);
  QVERIFY (session.samples ());
  QCOMPARE (session.completedSnapshot ().metadata.input_id, metadata.input_id);
  QCOMPARE (session.completedSnapshot ().lastAttempt, qint32 {1});
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::StaleGeneration);

  source->d2[0] = 99;
  source->params.newdat = false;
  source->params.nfqso = 1234;
  auto const samples = session.samples ();
  metadata.analysis_id = 13;
  metadata.attempt_no = 2;
  auto second = session.submit (Request::reuse (*source, samples, metadata));
  QVERIFY (second);
  QCOMPARE (second.generation.value (), 2);
  QVERIFY (second.samples == samples);
  QCOMPARE (shared.metadata.analysis_id, metadata.analysis_id);
  QCOMPARE (shared.metadata.attempt_no, metadata.attempt_no);
  QCOMPARE (shared.payload.d2[0], short {17});
  QCOMPARE (shared.payload.params.nfqso, 1234);
  QCOMPARE (session.complete ({0, 0, 0, 1}), Status::StaleGeneration);
  QCOMPARE (shared.control.generation, 2);
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);

  metadata = {14, 15, 1, 150000};
  auto refreshed = session.submit (Request::reuse (*source, samples, metadata));
  QVERIFY (refreshed);
  QVERIFY (refreshed.samples != samples);
  QCOMPARE (shared.payload.d2[0], short {99});
  QCOMPARE (shared.metadata.input_id, metadata.input_id);
  QVERIFY (shared.payload.params.newdat);
  QVERIFY (!shared.payload.params.nagain);

  session.abort ();
  QVERIFY (!session.samples ());
  QCOMPARE (session.completedSnapshot ().metadata.input_id, int64_t {0});
  QCOMPARE (session.completedSnapshot ().lastAttempt, qint32 {0});
  QVERIFY (!session.accepts (2));
  session.shutdown ();
  QVERIFY (session.reset ());
  QVERIFY (!session.ready ());
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  QCOMPARE (session.submit (Request::reuse (*source, samples)).status, Status::StaleSamples);
  auto third = session.submit (Request::snapshot (*source));
  QVERIFY (third);
  QCOMPARE (third.generation.value (), 4);
  QCOMPARE (shared.payload.d2[0], short {99});
  session.shutdown ();
}

void TestDecoderSession::compactSnapshotAndContextRefresh ()
{
  using namespace DecoderIpc;
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  Session session;
  auto const key = memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (key, directory.filePath ("worker.lock")), Status::Ok);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  NativeSharedMemory peer;
  QVERIFY2 (peer.attach (key.toStdString ()), peer.errorString ().c_str ());
  auto& shared = *static_cast<shared_dec_data_t *> (peer.data ());
  auto compact = std::make_unique<Ft8MtdPayload> ();
  compact->params.nmode = 8;
  compact->params.ntrperiod = 15;
  compact->params.kin = 150000;
  compact->params.lmultift8 = true;
  compact->metadata = {21, 22, 3, 150000};
  compact->samples.front () = 31;
  compact->samples.back () = 47;
  shared.payload.ss[0] = 5.5F;
  auto first = session.submit (Request::ft8 (*compact));
  QVERIFY (first);
  QCOMPARE (shared.metadata.input_id, compact->metadata.input_id);
  QCOMPARE (shared.metadata.analysis_id, compact->metadata.analysis_id);
  QCOMPARE (shared.metadata.attempt_no, compact->metadata.attempt_no);
  QCOMPARE (shared.metadata.valid_samples, compact->metadata.valid_samples);
  QCOMPARE (shared.payload.d2[0], short {31});
  QCOMPARE (shared.payload.d2[Ft8SampleCount - 1], short {47});
  QCOMPARE (shared.payload.ss[0], 5.5F);
  qint32 generation {0};
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);
  QCOMPARE (session.completedSnapshot ().metadata.input_id, compact->metadata.input_id);
  QCOMPARE (session.completedSnapshot ().metadata.valid_samples, compact->metadata.valid_samples);
  QCOMPARE (session.completedSnapshot ().lastAttempt, compact->metadata.attempt_no);

  auto source = std::make_unique<dec_data_t> ();
  source->params.nmode = 65;
  source->params.ntrperiod = 60;
  source->params.kin = 60 * RX_SAMPLE_RATE;
  source->params.nagain = true;
  source->d2[0] = 73;
  InputState inputs;
  auto const refreshed = session.prepareRequest (*source, false, inputs);
  QVERIFY (refreshed.options ().newdat);
  QVERIFY (!refreshed.options ().nagain);
  QCOMPARE (refreshed.metadata ().valid_samples, source->params.kin);
  QVERIFY (session.submit (refreshed));
  QCOMPARE (shared.payload.d2[0], short {73});
  QVERIFY (shared.payload.params.newdat);
  QVERIFY (!shared.payload.params.nagain);
  session.shutdown ();
}

void TestDecoderSession::incompatibleSnapshotLeavesNextReceptionUntouched_data ()
{
  QTest::addColumn<int> ("previousMode");
  QTest::addColumn<int> ("previousPeriod");
  QTest::addColumn<int> ("requestedMode");
  QTest::addColumn<int> ("requestedPeriod");
  QTest::newRow ("no-snapshot") << 0 << 0 << 66 << 60;
  QTest::newRow ("different-mode") << 65 << 60 << 66 << 60;
  QTest::newRow ("different-period") << 66 << 30 << 66 << 60;
  QTest::newRow ("fst4-mode") << 241 << 120 << 240 << 15;
  QTest::newRow ("fst4-period") << 240 << 30 << 240 << 15;
  QTest::newRow ("fst4w-mode") << 240 << 15 << 241 << 120;
  QTest::newRow ("fst4w-period") << 241 << 300 << 241 << 120;
}

void TestDecoderSession::incompatibleSnapshotLeavesNextReceptionUntouched ()
{
  using namespace DecoderIpc;
  QFETCH (int, previousMode);
  QFETCH (int, previousPeriod);
  QFETCH (int, requestedMode);
  QFETCH (int, requestedPeriod);
  int const validSamples = (requestedPeriod - 4) * RX_SAMPLE_RATE;
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  Session session;
  auto const key = memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (key, directory.filePath ("worker.lock")), Status::Ok);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  NativeSharedMemory peer;
  QVERIFY2 (peer.attach (key.toStdString ()), peer.errorString ().c_str ());
  auto& shared = *static_cast<shared_dec_data_t *> (peer.data ());
  auto source = std::make_unique<dec_data_t> ();
  qint32 generation {0};
  if (previousMode)
    {
      source->params.nmode = previousMode;
      source->params.ntrperiod = previousPeriod;
      source->params.kin = previousPeriod * RX_SAMPLE_RATE;
      source->params.newdat = true;
      decoder_input_metadata_t metadata {7, 9, 1, source->params.kin};
      QVERIFY (session.submit (Request::snapshot (*source, metadata)));
      QVERIFY (claim (shared, generation));
      QVERIFY (finish (shared, generation));
      QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);
      QVERIFY (session.canReuseSamples (previousMode, previousPeriod));
    }

  InputState inputs;
  inputs.beginInput ();
  auto const input = inputs.inputId ();
  auto const analysis = inputs.analysisId ();
  source->params.nmode = requestedMode;
  source->params.ntrperiod = requestedPeriod;
  source->params.kin = 10 * RX_SAMPLE_RATE;
  source->params.newdat = false;
  source->params.nagain = true;
  QVERIFY (!session.canReuseSamples (requestedMode, requestedPeriod));

  source->params.kin = validSamples;
  source->params.newdat = true;
  source->params.nagain = false;
  auto const scheduled = session.prepareRequest (*source, true, inputs);
  QCOMPARE (scheduled.metadata ().input_id, input);
  QCOMPARE (scheduled.metadata ().analysis_id, analysis);
  QCOMPARE (scheduled.metadata ().attempt_no, int32_t {1});
  QCOMPARE (scheduled.metadata ().valid_samples, source->params.kin);
  QVERIFY (session.submit (scheduled));
  QCOMPARE (shared.metadata.valid_samples, validSamples);
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);
  QVERIFY (session.canReuseSamples (requestedMode, requestedPeriod));

  source->params.kin = (requestedPeriod - 1) * RX_SAMPLE_RATE;
  source->params.newdat = false;
  source->params.nagain = true;
  auto const repeat = session.prepareRequest (*source, false, inputs);
  QCOMPARE (repeat.metadata ().input_id, input);
  QCOMPARE (repeat.metadata ().valid_samples, validSamples);
  QCOMPARE (repeat.options ().kin, validSamples);
  QVERIFY (!repeat.options ().newdat);
  session.shutdown ();
}

void TestDecoderSession::preparedReuseRetainsCompletedInput_data ()
{
  QTest::addColumn<int> ("mode");
  QTest::addColumn<bool> ("rollover");
  QTest::addColumn<bool> ("again");
  for (auto mode : {8, 5, 66, 240, 241})
    for (auto rollover : {false, true})
      for (auto again : {false, true})
        QTest::newRow (qPrintable (QString {"mode-%1-rollover-%2-again-%3"}
          .arg (mode).arg (rollover).arg (again))) << mode << rollover << again;
}

void TestDecoderSession::preparedReuseRetainsCompletedInput ()
{
  using namespace DecoderIpc;
  QFETCH (int, mode);
  QFETCH (bool, rollover);
  QFETCH (bool, again);
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  Session session;
  auto const key = memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (key, directory.filePath ("worker.lock")), Status::Ok);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  NativeSharedMemory peer;
  QVERIFY2 (peer.attach (key.toStdString ()), peer.errorString ().c_str ());
  auto& shared = *static_cast<shared_dec_data_t *> (peer.data ());
  InputState inputs;
  auto source = std::make_unique<dec_data_t> ();
  source->params.nmode = mode;
  source->params.ntrperiod = mode == 5 ? 7 : mode == 8 || mode == 240 ? 15 : mode == 241 ? 120 : 60;
  source->params.kin = mode == 5 ? 72000 : 150000;
  source->params.newdat = true;
  source->d2[0] = 17;
  auto const initial = session.prepareRequest (*source, true, inputs);
  auto const first = session.submit (initial);
  QVERIFY (first);
  qint32 generation {0};
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);
  auto const retained = session.completedSnapshot ();

  if (rollover) inputs.beginInput ();
  auto const liveInput = inputs.inputId ();
  auto const liveAnalysis = inputs.analysisId ();
  source->params.kin = rollover ? 4096 : source->params.kin + 2048;
  source->params.newdat = false;
  source->params.nagain = again;
  source->params.nfqso = 1234;
  source->d2[0] = 99;
  auto const liveSamples = source->params.kin;
  auto candidate = inputs;
  auto const repeat = session.prepareRequest (*source, false, candidate);
  QCOMPARE (repeat.metadata ().input_id, retained.metadata.input_id);
  QCOMPARE (repeat.metadata ().valid_samples, retained.metadata.valid_samples);
  QCOMPARE (repeat.options ().kin, retained.metadata.valid_samples);
  QCOMPARE (source->params.kin, liveSamples);
  QVERIFY (!repeat.options ().newdat);
  QCOMPARE (repeat.options ().nagain, again);
  if (again)
    {
      QVERIFY (repeat.metadata ().analysis_id != retained.metadata.analysis_id);
      QCOMPARE (repeat.metadata ().attempt_no, int32_t {1});
    }
  else
    {
      QCOMPARE (repeat.metadata ().analysis_id, retained.metadata.analysis_id);
      QCOMPARE (repeat.metadata ().attempt_no, retained.lastAttempt + 1);
    }
  auto const repeated = session.submit (repeat);
  QVERIFY (repeated);
  inputs = candidate;
  QVERIFY (repeated.samples == retained.samples);
  QCOMPARE (shared.payload.d2[0], short {17});
  QCOMPARE (shared.payload.params.nfqso, 1234);
  QCOMPARE (shared.payload.params.kin, retained.metadata.valid_samples);
  QCOMPARE (shared.metadata.valid_samples, retained.metadata.valid_samples);
  QCOMPARE (shared.metadata.input_id, retained.metadata.input_id);
  QVERIFY (!shared.payload.params.newdat);
  QCOMPARE (shared.payload.params.nagain, again);
  QCOMPARE (source->params.kin, liveSamples);
  QCOMPARE (inputs.inputId (), liveInput);
  QCOMPARE (inputs.analysisId (), rollover ? liveAnalysis : repeat.metadata ().analysis_id);
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);

  source->params.nagain = false;
  auto const continued = session.prepareRequest (*source, false, inputs);
  QCOMPARE (continued.metadata ().input_id, repeat.metadata ().input_id);
  QCOMPARE (continued.metadata ().analysis_id, repeat.metadata ().analysis_id);
  QCOMPARE (continued.metadata ().attempt_no, repeat.metadata ().attempt_no + 1);
  QVERIFY (session.submit (continued));
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);

  source->params.newdat = true;
  auto const nextLive = session.prepareRequest (*source, true, inputs);
  QCOMPARE (nextLive.metadata ().input_id, liveInput);
  QCOMPARE (nextLive.metadata ().analysis_id,
            rollover ? liveAnalysis : continued.metadata ().analysis_id);
  QCOMPARE (nextLive.metadata ().attempt_no,
            rollover ? 1 : continued.metadata ().attempt_no + 1);
  QCOMPARE (nextLive.metadata ().valid_samples, liveSamples);
  auto const fresh = session.submit (nextLive);
  QVERIFY (fresh);
  QVERIFY (fresh.samples != retained.samples);
  QCOMPARE (shared.payload.d2[0], short {99});
  session.shutdown ();
}

void TestDecoderSession::diskPassesReserveAttemptNumbers ()
{
  using namespace DecoderIpc;
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  Session session;
  auto const key = memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (key, directory.filePath ("worker.lock")), Status::Ok);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  NativeSharedMemory peer;
  QVERIFY2 (peer.attach (key.toStdString ()), peer.errorString ().c_str ());
  auto& shared = *static_cast<shared_dec_data_t *> (peer.data ());
  InputState inputs;
  auto source = std::make_unique<dec_data_t> ();
  source->params.nmode = 8;
  source->params.ntrperiod = 15;
  source->params.kin = 180000;
  source->params.ndiskdat = true;
  source->params.newdat = true;
  for (auto firstAttempt : {1, 4})
    {
      auto const request = session.prepareRequest (*source, source->params.newdat, inputs);
      QCOMPARE (request.metadata ().attempt_no, firstAttempt);
      QVERIFY (session.submit (request));
      qint32 generation {0};
      QVERIFY (claim (shared, generation));
      QVERIFY (finish (shared, generation));
      QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);
      QCOMPARE (session.completedSnapshot ().lastAttempt, firstAttempt + 2);
      source->params.newdat = false;
      source->params.lmultift8 = true;
      source->params.ndecoderstart = 1;
    }
  source->params.nagain = true;
  auto const manual = session.prepareRequest (*source, false, inputs);
  QCOMPARE (manual.metadata ().attempt_no, int32_t {1});
  QVERIFY (manual.metadata ().analysis_id != session.completedSnapshot ().metadata.analysis_id);
  QVERIFY (session.submit (manual));
  qint32 generation {0};
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);
  QCOMPARE (session.completedSnapshot ().lastAttempt, qint32 {1});
  source->params.nagain = false;
  auto const continued = session.prepareRequest (*source, false, inputs);
  QCOMPARE (continued.metadata ().analysis_id, manual.metadata ().analysis_id);
  QCOMPARE (continued.metadata ().attempt_no, int32_t {2});
  session.shutdown ();
}

void TestDecoderSession::deferredSnapshotKeepsItsInputAfterRollover ()
{
  using namespace DecoderIpc;
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  Session session;
  auto const key = memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (key, directory.filePath ("worker.lock")), Status::Ok);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  NativeSharedMemory peer;
  QVERIFY2 (peer.attach (key.toStdString ()), peer.errorString ().c_str ());
  auto& shared = *static_cast<shared_dec_data_t *> (peer.data ());
  InputState inputs;
  auto source = std::make_unique<dec_data_t> ();
  source->params.nmode = 8;
  source->params.ntrperiod = 15;
  source->params.kin = 141696;
  source->params.newdat = true;
  QVERIFY (session.submit (session.prepareRequest (*source, true, inputs)));

  auto pending = std::make_unique<Ft8MtdPayload> ();
  pending->params = source->params;
  pending->params.lmultift8 = true;
  pending->params.kin = 170000;
  pending->metadata = inputs.nextMetadata (pending->params);
  pending->samples[0] = 23;
  QCOMPARE (pending->metadata.attempt_no, int32_t {2});
  inputs.beginInput ();
  auto const liveInput = inputs.inputId ();
  auto const liveAnalysis = inputs.analysisId ();
  qint32 generation {0};
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);
  QVERIFY (session.submit (Request::ft8 (*pending)));
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);

  source->params.kin = 4096;
  source->params.newdat = false;
  auto const repeat = session.prepareRequest (*source, false, inputs);
  QCOMPARE (repeat.metadata ().input_id, pending->metadata.input_id);
  QCOMPARE (repeat.metadata ().analysis_id, pending->metadata.analysis_id);
  QCOMPARE (repeat.metadata ().attempt_no, int32_t {3});
  QCOMPARE (repeat.options ().kin, 170000);
  QVERIFY (session.submit (repeat));
  QCOMPARE (shared.payload.d2[0], short {23});
  QCOMPARE (source->params.kin, 4096);
  QCOMPARE (inputs.inputId (), liveInput);
  QCOMPARE (inputs.analysisId (), liveAnalysis);
  session.shutdown ();
}

void TestDecoderSession::reuseStartsNewAnalysisBeforeAttemptOverflow ()
{
  using namespace DecoderIpc;
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  Session session;
  auto const key = memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (key, directory.filePath ("worker.lock")), Status::Ok);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  NativeSharedMemory peer;
  QVERIFY2 (peer.attach (key.toStdString ()), peer.errorString ().c_str ());
  auto& shared = *static_cast<shared_dec_data_t *> (peer.data ());
  InputState inputs;
  inputs.beginInput ();
  auto source = std::make_unique<dec_data_t> ();
  source->params.nmode = 8;
  source->params.ntrperiod = 15;
  source->params.kin = 150000;
  decoder_input_metadata_t metadata {
    inputs.inputId (), inputs.analysisId (), std::numeric_limits<qint32>::max (), 150000};
  QVERIFY (session.submit (Request::snapshot (*source, metadata)));
  qint32 generation {0};
  QVERIFY (claim (shared, generation));
  QVERIFY (finish (shared, generation));
  QCOMPARE (session.complete ({0, 0, 0, generation}), Status::Ok);

  auto const request = session.prepareRequest (*source, false, inputs);
  QCOMPARE (request.metadata ().input_id, metadata.input_id);
  QVERIFY (request.metadata ().analysis_id != metadata.analysis_id);
  QCOMPARE (request.metadata ().attempt_no, int32_t {1});
  QVERIFY (session.submit (request));
  session.shutdown ();
}

void TestDecoderSession::resetRejectsCorruption ()
{
  using namespace DecoderIpc;
  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  Session session;
  auto const key = memoryName (QUuid::createUuid ().toString ());
  QCOMPARE (session.open (key, directory.filePath ("worker.lock")), Status::Ok);
  QCOMPARE (session.acceptReady (DECODER_IPC_VERSION), Status::Ok);
  NativeSharedMemory peer;
  QVERIFY2 (peer.attach (key.toStdString ()), peer.errorString ().c_str ());
  auto& shared = *static_cast<shared_dec_data_t *> (peer.data ());
  --shared.layout.payload_bytes;
  QByteArray const before {static_cast<char const *> (peer.data ()), static_cast<int> (peer.size ())};
  QVERIFY (!session.reset ());
  session.shutdown ();
  QCOMPARE (QByteArray (static_cast<char const *> (peer.data ()), static_cast<int> (peer.size ())), before);
}

QTEST_GUILESS_MAIN (TestDecoderSession)
#include "test_decoder_session.moc"
