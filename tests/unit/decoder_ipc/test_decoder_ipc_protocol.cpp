#include <QtTest>
#include <QProcess>
#include <QUuid>
#include <QTemporaryDir>

#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include "DecoderIpc.hpp"
#include "lib/NativeSharedMemory.hpp"
#include "lib/decoder_ipc_control.h"

extern "C"
{
  void decoder_ipc_fortran_probe (void *, int[11]);
}

class TestDecoderIpcProtocol final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void layoutMatchesFortran ();
  void sharedMemorySizeAllowsPlatformRounding ();
  void publicationAndClaimValidateControl ();
  void compactFt8MtdPublicationCopiesRequiredPayload ();
  void compactFt8MtdPublicationRejectsInvalidRequests ();
  void newInputForcesSamplePublication ();
  void transitionsRequireMatchingGeneration ();
  void sharedProgressTracksActiveGeneration ();
  void delayedCompletionCannotClobberNextRequest ();
  void compatibleShutdownReleasesWorkers ();
  void sharedMemoryWorkerExitsOnShutdown_data ();
  void sharedMemoryWorkerExitsOnShutdown ();
  void rejectedEngineRequestCompletes ();
  void shutdownReplacesAnyState_data ();
  void shutdownReplacesAnyState ();
  void generationWrapsWithoutUsingZero ();
  void startMarkerParsing ();
  void startMarkerRejections_data ();
  void startMarkerRejections ();
  void completionMarkerParsing ();
  void completionMarkerRejections_data ();
  void completionMarkerRejections ();
};

void TestDecoderIpcProtocol::layoutMatchesFortran ()
{
  struct CompatibleSharedData
  {
    int ipc[4];
    decoder_ipc_layout_t layout;
    dec_data_t payload;
    decoder_input_metadata_t metadata;
  };

  QCOMPARE (sizeof (decoder_ipc_control_t), size_t {16});
  QCOMPARE (offsetof (decoder_ipc_control_t, state), size_t {4});
  QCOMPARE (offsetof (shared_dec_data_t, payload),
            sizeof (decoder_ipc_control_t) + sizeof (decoder_ipc_layout_t));
  QCOMPARE (sizeof (decoder_input_metadata_t), size_t {24});
  QCOMPARE (offsetof (shared_dec_data_t, metadata),
            offsetof (CompatibleSharedData, metadata));
  QCOMPARE (sizeof (shared_dec_data_t), sizeof (CompatibleSharedData));
  QCOMPARE (offsetof (CompatibleSharedData, ipc) + sizeof (int),
            offsetof (decoder_ipc_control_t, state));
  QCOMPARE (offsetof (CompatibleSharedData, payload),
            offsetof (shared_dec_data_t, payload));

  std::unique_ptr<shared_dec_data_t> shared {new shared_dec_data_t};
  shared->control.generation = 41;
  int values[11] {};
  decoder_ipc_fortran_probe (shared.get (), values);

  QCOMPARE (values[0], static_cast<int> (sizeof (decoder_ipc_control_t)));
  QCOMPARE (values[1], static_cast<int> (sizeof (decoder_params_t)));
  QCOMPARE (values[2], static_cast<int> (sizeof (dec_data_t)));
  QCOMPARE (values[3], static_cast<int> (sizeof (shared_dec_data_t)));
  QCOMPARE (values[4], int {DECODER_IPC_VERSION});
  QCOMPARE (values[5], int {DECODER_IPC_IDLE});
  QCOMPARE (values[6], int {DECODER_IPC_READY});
  QCOMPARE (values[7], int {DECODER_IPC_DECODING});
  QCOMPARE (values[8], int {DECODER_IPC_COMPLETE});
  QCOMPARE (values[9], int {DECODER_IPC_SHUTDOWN});
  QCOMPARE (values[10], static_cast<int> (sizeof (decoder_input_metadata_t)));
  QCOMPARE (shared->control.generation, 42);
  QCOMPARE (shared->payload.d2[0], short {1234});
  QCOMPARE (shared->metadata.input_id, int64_t {12345678901});
  QCOMPARE (shared->metadata.analysis_id, int64_t {12345678902});
  QCOMPARE (shared->metadata.attempt_no, int32_t {3});
  QCOMPARE (shared->metadata.valid_samples, int32_t {180000});
}

void TestDecoderIpcProtocol::sharedMemorySizeAllowsPlatformRounding ()
{
  auto const required = static_cast<qint64> (sizeof (shared_dec_data_t));
  QVERIFY (!DecoderIpc::hasUsableSize (required - 1));
  QVERIFY (DecoderIpc::hasUsableSize (required));
  QVERIFY (DecoderIpc::hasUsableSize (required + 80));

}

void TestDecoderIpcProtocol::publicationAndClaimValidateControl ()
{
  std::unique_ptr<shared_dec_data_t> shared {new shared_dec_data_t};
  std::unique_ptr<dec_data_t> payload {new dec_data_t {}};
  DecoderIpc::initialize (*shared);

  QVERIFY (!DecoderIpc::publish (*shared, *payload, false, 0));
  QCOMPARE (shared->control.state, int {DECODER_IPC_IDLE});

  shared->control.version = DECODER_IPC_VERSION + 1;
  QVERIFY (!DecoderIpc::publish (*shared, *payload, false, 1));
  QCOMPARE (shared->control.state, int {DECODER_IPC_IDLE});

  shared->control.version = DECODER_IPC_VERSION;
  shared->control.generation = 0;
  shared->control.state = DECODER_IPC_READY;
  qint32 generation {0};
  QVERIFY (!DecoderIpc::claim (*shared, generation));
  QCOMPARE (shared->control.state, int {DECODER_IPC_READY});
}

void TestDecoderIpcProtocol::compactFt8MtdPublicationCopiesRequiredPayload ()
{
  std::unique_ptr<shared_dec_data_t> shared {new shared_dec_data_t};
  std::unique_ptr<DecoderIpc::Ft8MtdPayload> payload {new DecoderIpc::Ft8MtdPayload};
  DecoderIpc::initialize (*shared);
  shared->payload.ss[0] = 3.5F;
  shared->payload.savg[0] = 4.5F;
  shared->payload.sred[0] = 5.5F;
  shared->payload.d2[DecoderIpc::Ft8SampleCount] = 61;
  shared->payload.d2[NTMAX * RX_SAMPLE_RATE - 1] = 62;
  payload->params.nmode = 8;
  payload->params.lmultift8 = true;
  payload->params.nutc = 123456;
  payload->metadata = {11, 12, 2, 162432};
  payload->samples.front () = 17;
  payload->samples.back () = 29;

  QVERIFY (DecoderIpc::publishFt8Mtd (*shared, *payload, 7));

  QCOMPARE (shared->control.state, int {DECODER_IPC_READY});
  QCOMPARE (shared->control.generation, 7);
  QCOMPARE (shared->payload.params.nutc, 123456);
  QCOMPARE (shared->metadata.input_id, int64_t {11});
  QCOMPARE (shared->metadata.analysis_id, int64_t {12});
  QCOMPARE (shared->metadata.attempt_no, int32_t {2});
  QCOMPARE (shared->metadata.valid_samples, int32_t {162432});
  QCOMPARE (shared->payload.d2[0], short {17});
  QCOMPARE (shared->payload.d2[DecoderIpc::Ft8SampleCount - 1], short {29});
  QCOMPARE (shared->payload.d2[DecoderIpc::Ft8SampleCount], short {61});
  QCOMPARE (shared->payload.d2[NTMAX * RX_SAMPLE_RATE - 1], short {62});
  QCOMPARE (shared->payload.ss[0], 3.5F);
  QCOMPARE (shared->payload.savg[0], 4.5F);
  QCOMPARE (shared->payload.sred[0], 5.5F);
}

void TestDecoderIpcProtocol::compactFt8MtdPublicationRejectsInvalidRequests ()
{
  std::unique_ptr<shared_dec_data_t> shared {new shared_dec_data_t};
  std::unique_ptr<DecoderIpc::Ft8MtdPayload> payload {new DecoderIpc::Ft8MtdPayload};
  DecoderIpc::initialize (*shared);
  shared->payload.params.nutc = 111111;
  shared->payload.d2[0] = 11;
  payload->params.nmode = 9;
  payload->params.lmultift8 = true;
  payload->params.nutc = 222222;
  payload->samples[0] = 22;

  QVERIFY (!DecoderIpc::publishFt8Mtd (*shared, *payload, 1));
  QCOMPARE (shared->control.state, int {DECODER_IPC_IDLE});
  QCOMPARE (shared->payload.params.nutc, 111111);
  QCOMPARE (shared->payload.d2[0], short {11});

  payload->params.nmode = 8;
  payload->params.lmultift8 = false;
  QVERIFY (!DecoderIpc::publishFt8Mtd (*shared, *payload, 1));
  QCOMPARE (shared->control.state, int {DECODER_IPC_IDLE});
  QCOMPARE (shared->payload.params.nutc, 111111);
  QCOMPARE (shared->payload.d2[0], short {11});

  payload->params.lmultift8 = true;
  QVERIFY (!DecoderIpc::publishFt8Mtd (*shared, *payload, 0));
  shared->control.version = DECODER_IPC_VERSION + 1;
  QVERIFY (!DecoderIpc::publishFt8Mtd (*shared, *payload, 1));
  shared->control.version = DECODER_IPC_VERSION;
  QVERIFY (DecoderIpc::publishFt8Mtd (*shared, *payload, 1));
  payload->params.nutc = 333333;
  payload->samples[0] = 33;
  QVERIFY (!DecoderIpc::publishFt8Mtd (*shared, *payload, 2));
  QCOMPARE (shared->payload.params.nutc, 222222);
  QCOMPARE (shared->payload.d2[0], short {22});

  qint32 generation {0};
  QVERIFY (DecoderIpc::claim (*shared, generation));
  QVERIFY (!DecoderIpc::publishFt8Mtd (*shared, *payload, 2));
  QCOMPARE (shared->payload.params.nutc, 222222);
  QCOMPARE (shared->payload.d2[0], short {22});
}

void TestDecoderIpcProtocol::newInputForcesSamplePublication ()
{
  std::unique_ptr<shared_dec_data_t> shared {new shared_dec_data_t};
  std::unique_ptr<dec_data_t> payload {new dec_data_t {}};
  DecoderIpc::initialize (*shared);
  payload->params.nmode = 8;
  payload->params.ntrperiod = 15;
  payload->d2[0] = 17;

  QVERIFY (DecoderIpc::publish (*shared, *payload, true, 1, {11, 12, 1, 141696}));
  qint32 generation {0};
  QVERIFY (DecoderIpc::claim (*shared, generation));
  QVERIFY (DecoderIpc::finish (*shared, 1));
  QVERIFY (DecoderIpc::consume (*shared, 1));

  payload->d2[0] = 29;
  QVERIFY (DecoderIpc::publish (*shared, *payload, false, 2, {11, 12, 2, 169344}));
  QCOMPARE (shared->payload.d2[0], short {17});
  QCOMPARE (shared->metadata.attempt_no, int32_t {2});
  QVERIFY (DecoderIpc::claim (*shared, generation));
  QVERIFY (DecoderIpc::finish (*shared, 2));
  QVERIFY (DecoderIpc::consume (*shared, 2));

  QVERIFY (DecoderIpc::publish (*shared, *payload, false, 3, {13, 14, 1, 141696}));
  QCOMPARE (shared->payload.d2[0], short {29});
  QCOMPARE (shared->metadata.input_id, int64_t {13});
  QCOMPARE (shared->metadata.analysis_id, int64_t {14});
  QCOMPARE (shared->metadata.attempt_no, int32_t {1});
}

void TestDecoderIpcProtocol::transitionsRequireMatchingGeneration ()
{
  std::unique_ptr<shared_dec_data_t> shared {new shared_dec_data_t};
  std::unique_ptr<dec_data_t> payload {new dec_data_t {}};
  DecoderIpc::initialize (*shared);
  payload->params.nutc = 123456;
  payload->d2[0] = 17;

  QVERIFY (DecoderIpc::publish (*shared, *payload, true, 7));
  QCOMPARE (shared->control.state, int {DECODER_IPC_READY});
  QCOMPARE (shared->control.generation, 7);
  QCOMPARE (shared->payload.params.nutc, 123456);
  QCOMPARE (shared->payload.d2[0], short {17});

  payload->params.nutc = 654321;
  payload->d2[0] = 99;
  QVERIFY (!DecoderIpc::publish (*shared, *payload, true, 8));
  QCOMPARE (shared->payload.params.nutc, 123456);
  QCOMPARE (shared->payload.d2[0], short {17});

  qint32 claimed {0};
  QVERIFY (DecoderIpc::claim (*shared, claimed));
  QCOMPARE (claimed, qint32 {7});
  QVERIFY (!DecoderIpc::publish (*shared, *payload, true, 8));
  QCOMPARE (shared->payload.params.nutc, 123456);
  QCOMPARE (shared->payload.d2[0], short {17});
  QVERIFY (!DecoderIpc::consume (*shared, 6));
  QCOMPARE (shared->control.state, int {DECODER_IPC_DECODING});
  QVERIFY (!DecoderIpc::consume (*shared, 7));
  QCOMPARE (shared->control.state, int {DECODER_IPC_DECODING});
  QVERIFY (DecoderIpc::finish (*shared, 7));
  QVERIFY (DecoderIpc::consume (*shared, 7));
  QCOMPARE (shared->control.state, int {DECODER_IPC_IDLE});
  QVERIFY (!DecoderIpc::consume (*shared, 7));

  QVERIFY (DecoderIpc::publish (*shared, *payload, false, 8));
  QCOMPARE (shared->payload.params.nutc, 654321);
  QCOMPARE (shared->control.generation, 8);
  QCOMPARE (shared->payload.d2[0], short {17});
  QVERIFY (!DecoderIpc::consume (*shared, 8));
  DecoderIpc::shutdown (*shared);
}

void TestDecoderIpcProtocol::sharedProgressTracksActiveGeneration ()
{
  std::unique_ptr<shared_dec_data_t> shared {new shared_dec_data_t};
  std::unique_ptr<dec_data_t> payload {new dec_data_t {}};
  DecoderIpc::initialize (*shared);

  QVERIFY (DecoderIpc::publish (*shared, *payload, false, 7));
  qint32 claimed {0};
  QVERIFY (DecoderIpc::claim (*shared, claimed));
  QCOMPARE (DecoderIpc::progress (*shared), qint32 {0});

  decoder_ipc_progress_bind (&shared->control.generation,
                             &shared->control.state,
                             &shared->control.version,
                             &shared->control.progress);
  decoder_ipc_progress_report (6);
  QCOMPARE (DecoderIpc::progress (*shared), qint32 {0});
  std::vector<std::thread> workers;
  for (int worker = 0; worker < 4; ++worker)
    {
      workers.emplace_back ([] {
          for (int report = 0; report < 1000; ++report)
            {
              decoder_ipc_progress_report (7);
            }
        });
    }
  for (auto& worker: workers) worker.join ();
  QCOMPARE (DecoderIpc::progress (*shared), qint32 {4000});

  QVERIFY (DecoderIpc::finish (*shared, 7));
  decoder_ipc_progress_report (7);
  QCOMPARE (DecoderIpc::progress (*shared), qint32 {4000});
  QVERIFY (DecoderIpc::consume (*shared, 7));
  QVERIFY (DecoderIpc::publish (*shared, *payload, false, 8));
  QCOMPARE (DecoderIpc::progress (*shared), qint32 {0});

  decoder_ipc_progress_unbind ();
  decoder_ipc_progress_report (8);
  QCOMPARE (DecoderIpc::progress (*shared), qint32 {0});
}

void TestDecoderIpcProtocol::delayedCompletionCannotClobberNextRequest ()
{
  std::unique_ptr<shared_dec_data_t> shared {new shared_dec_data_t};
  std::unique_ptr<dec_data_t> payload {new dec_data_t};
  DecoderIpc::initialize (*shared);
  payload->params = {};

  QVERIFY (DecoderIpc::publish (*shared, *payload, false, 10));
  qint32 claimed {0};
  QVERIFY (DecoderIpc::claim (*shared, claimed));
  QVERIFY (DecoderIpc::finish (*shared, 10));
  QVERIFY (DecoderIpc::consume (*shared, 10));
  QVERIFY (DecoderIpc::publish (*shared, *payload, false, 11));

  QVERIFY (!DecoderIpc::finish (*shared, 10));
  QCOMPARE (shared->control.generation, 11);
  QCOMPARE (shared->control.state, int {DECODER_IPC_READY});

  DecoderIpc::shutdown (*shared);
  QCOMPARE (shared->control.state, int {DECODER_IPC_SHUTDOWN});
}

void TestDecoderIpcProtocol::compatibleShutdownReleasesWorkers ()
{
  auto shared = std::make_unique<shared_dec_data_t> ();
  DecoderIpc::initialize (*shared);
  auto& control = shared->control;
  control.generation = 17;
  control.progress = 4;
  DecoderIpc::shutdown (*shared);

  QCOMPARE (control.generation, 17);
  QCOMPARE (control.state, int {DECODER_IPC_SHUTDOWN});
  QCOMPARE (control.version, int {DECODER_IPC_VERSION});
  QCOMPARE (control.progress, 4);
}

void TestDecoderIpcProtocol::rejectedEngineRequestCompletes ()
{
  auto const key = DecoderIpc::memoryName (QUuid::createUuid ().toString ());
  NativeSharedMemory memory;
  QVERIFY2 (memory.create (key.toStdString (), sizeof (shared_dec_data_t)), memory.errorString ().c_str ());
  auto * shared = static_cast<shared_dec_data_t *> (memory.data ());
  DecoderIpc::initialize (*shared);
  auto payload = std::make_unique<dec_data_t> ();
  payload->params.nmode = 8;
  payload->params.ntrperiod = 15;
  payload->params.nzhsym = 50;
  payload->params.newdat = true;
  payload->params.nfa = -1;
  payload->params.nfb = 4000;

  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  QProcess decoder;
  QByteArray output;
  connect (&decoder, &QProcess::readyReadStandardOutput, this, [&] {
      output += decoder.readAllStandardOutput ();
    });
  decoder.start (QString::fromLocal8Bit (JT9_TEST_EXECUTABLE), {"-s", key, "--ipc-lock", directory.filePath ("worker.lock"),
                 "-a", directory.path (), "-t", directory.path ()});
  QVERIFY (decoder.waitForStarted ());
  QTRY_VERIFY_WITH_TIMEOUT (output.contains ("<DecoderReady>"), 5000);
  QVERIFY (DecoderIpc::publish (*shared, *payload, true, 7, {11, 12, 1, 180000}));
  QTRY_VERIFY_WITH_TIMEOUT (output.contains ("<DecodeFinished>"), 5000);
  QVERIFY2 (output.contains ("<DecodeRejected> status=invalid operation=decode mode=8 input=11 analysis=12 attempt=1"),
            output.constData ());
  QVERIFY (!output.contains ("<DecoderError>"));
  QVERIFY (output.indexOf ("<DecodeRejected>") < output.indexOf ("<DecodeFinished>"));
  QVERIFY (DecoderIpc::consume (*shared, 7));

  output.clear ();
  payload->params.nfa = 200;
  QVERIFY (DecoderIpc::publish (*shared, *payload, true, 8, {11, 12, 2, 180000}));
  QTRY_VERIFY_WITH_TIMEOUT (output.contains ("<DecodeFinished>"), 5000);
  QVERIFY2 (!output.contains ("<DecodeRejected>"), output.constData ());
  QVERIFY (DecoderIpc::consume (*shared, 8));
  DecoderIpc::shutdown (*shared);
  QVERIFY (decoder.waitForFinished (5000));
  QCOMPARE (decoder.exitStatus (), QProcess::NormalExit);
  QCOMPARE (decoder.exitCode (), 0);
}

void TestDecoderIpcProtocol::sharedMemoryWorkerExitsOnShutdown_data ()
{
  QTest::addColumn<bool> ("explicitMode");
  QTest::newRow ("mode-from-shared-memory") << false;
  QTest::newRow ("explicit-ft8-mode") << true;
}

void TestDecoderIpcProtocol::sharedMemoryWorkerExitsOnShutdown ()
{
  QFETCH (bool, explicitMode);

  auto const key = DecoderIpc::memoryName (QUuid::createUuid ().toString ());
  NativeSharedMemory memory;
  QVERIFY2 (memory.create (key.toStdString (), sizeof (shared_dec_data_t)),
            memory.errorString ().c_str ());

  auto * shared = static_cast<shared_dec_data_t *> (memory.data ());
  DecoderIpc::initialize (*shared);
  DecoderIpc::shutdown (*shared);

  QStringList args;
  if (explicitMode) args << QStringLiteral ("-8");
  args << QStringLiteral ("-s") << key;

  QTemporaryDir directory;
  QVERIFY (directory.isValid ());
  QProcess decoder;
  args << "--ipc-lock" << directory.filePath ("worker.lock")
       << "-a" << directory.path () << "-t" << directory.path ();
  decoder.start (QString::fromLocal8Bit (JT9_TEST_EXECUTABLE), args);
  if (!decoder.waitForFinished (15000))
    {
      decoder.kill ();
      decoder.waitForFinished ();
      QFAIL ("jt9 did not exit after shared-memory shutdown");
    }
  QCOMPARE (decoder.exitStatus (), QProcess::NormalExit);
  QCOMPARE (decoder.exitCode (), 0);
}

void TestDecoderIpcProtocol::shutdownReplacesAnyState_data ()
{
  QTest::addColumn<int> ("state");
  QTest::newRow ("idle") << int {DECODER_IPC_IDLE};
  QTest::newRow ("ready") << int {DECODER_IPC_READY};
  QTest::newRow ("decoding") << int {DECODER_IPC_DECODING};
  QTest::newRow ("complete") << int {DECODER_IPC_COMPLETE};
}

void TestDecoderIpcProtocol::shutdownReplacesAnyState ()
{
  QFETCH (int, state);
  auto shared = std::make_unique<shared_dec_data_t> ();
  DecoderIpc::initialize (*shared);
  auto& control = shared->control;
  control.generation = 17;
  control.state = state;
  DecoderIpc::shutdown (*shared);

  QCOMPARE (control.generation, 17);
  QCOMPARE (control.state, int {DECODER_IPC_SHUTDOWN});
  QCOMPARE (control.version, int {DECODER_IPC_VERSION});
}

void TestDecoderIpcProtocol::generationWrapsWithoutUsingZero ()
{
  QCOMPARE (DecoderIpc::nextGeneration (0), qint32 {1});
  QCOMPARE (DecoderIpc::nextGeneration (-1), qint32 {1});
  QCOMPARE (DecoderIpc::nextGeneration (41), qint32 {42});
  QCOMPARE (DecoderIpc::nextGeneration (std::numeric_limits<qint32>::max ()), qint32 {1});
}

void TestDecoderIpcProtocol::startMarkerParsing ()
{
  qint32 generation {0};
  QVERIFY (DecoderIpc::parseStart ("<DecodeStarted> gen=42\r\n", &generation));
  QCOMPARE (generation, qint32 {42});
}

void TestDecoderIpcProtocol::startMarkerRejections_data ()
{
  QTest::addColumn<QByteArray> ("line");
  QTest::newRow ("embedded") << QByteArray {"noise <DecodeStarted> gen=42"};
  QTest::newRow ("missing generation") << QByteArray {"<DecodeStarted>"};
  QTest::newRow ("zero generation") << QByteArray {"<DecodeStarted> gen=0"};
  QTest::newRow ("negative generation") << QByteArray {"<DecodeStarted> gen=-1"};
  QTest::newRow ("overflow generation") << QByteArray {"<DecodeStarted> gen=2147483648"};
  QTest::newRow ("trailing garbage") << QByteArray {"<DecodeStarted> gen=42 extra"};
}

void TestDecoderIpcProtocol::startMarkerRejections ()
{
  QFETCH (QByteArray, line);
  qint32 generation {0};
  QVERIFY (!DecoderIpc::parseStart (line, &generation));
}

void TestDecoderIpcProtocol::completionMarkerParsing ()
{
  DecoderIpc::Completion completion {};
  QVERIFY (DecoderIpc::parseCompletion (
      "<DecodeFinished>   2  17   123456 gen=42\r\n", &completion));
  QCOMPARE (completion.synchronized, qint32 {2});
  QCOMPARE (completion.decoded, qint32 {17});
  QCOMPARE (completion.average, qint32 {123456});
  QCOMPARE (completion.generation, qint32 {42});
}

void TestDecoderIpcProtocol::completionMarkerRejections_data ()
{
  QTest::addColumn<QByteArray> ("line");
  QTest::newRow ("embedded") << QByteArray {"noise <DecodeFinished>   2  17   123456 gen=42"};
  QTest::newRow ("legacy") << QByteArray {"<DecodeFinished>   2  17   123456"};
  QTest::newRow ("zero generation") << QByteArray {"<DecodeFinished>   2  17   123456 gen=0"};
  QTest::newRow ("negative generation") << QByteArray {"<DecodeFinished>   2  17   123456 gen=-1"};
  QTest::newRow ("overflow generation") << QByteArray {"<DecodeFinished>   2  17   123456 gen=2147483648"};
  QTest::newRow ("overflow field") << QByteArray {"<DecodeFinished>****  17   123456 gen=42"};
  QTest::newRow ("trailing garbage") << QByteArray {"<DecodeFinished>   2  17   123456 gen=42 extra"};
}

void TestDecoderIpcProtocol::completionMarkerRejections ()
{
  QFETCH (QByteArray, line);
  DecoderIpc::Completion completion {};
  QVERIFY (!DecoderIpc::parseCompletion (line, &completion));
}

QTEST_GUILESS_MAIN (TestDecoderIpcProtocol)

#include "test_decoder_ipc_protocol.moc"
