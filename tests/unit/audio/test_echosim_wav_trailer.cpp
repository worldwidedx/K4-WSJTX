#include <QtTest>

#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QtEndian>
#include <QtMultimedia/QAudioFormat>

#include "Audio/BWFFile.hpp"

namespace
{
  bool wait_for_echosim (QProcess& process)
  {
    auto const finished = process.waitForFinished (30000);
    if (!finished || process.exitStatus () != QProcess::NormalExit || process.exitCode () != 0)
      {
        auto const reason = !finished ? process.errorString ()
          : process.exitStatus () != QProcess::NormalExit ? QString {"abnormal termination"}
          : QString {"nonzero exit"};
        if (process.state () != QProcess::NotRunning)
          {
            process.kill ();
            process.waitForFinished (5000);
          }
        qWarning ().noquote () << QString {
          "echosim %1 failed: %2 (exit status %3, exit code %4)\nstdout:\n%5\nstderr:\n%6"}
          .arg (process.arguments ().join (' '), reason)
          .arg (process.exitStatus () == QProcess::NormalExit ? "normal" : "crashed")
          .arg (process.exitCode ())
          .arg (QString::fromLocal8Bit (process.readAllStandardOutput ()),
                QString::fromLocal8Bit (process.readAllStandardError ()));
      }
    return finished;
  }

  QAudioFormat default_format ()
  {
    QAudioFormat format;
    format.setByteOrder (QAudioFormat::LittleEndian);
    format.setChannelCount (1);
    format.setCodec ("audio/pcm");
    format.setSampleRate (12000);
    format.setSampleSize (16);
    format.setSampleType (QAudioFormat::SignedInt);
    return format;
  }
}

class TestEchosimWavTrailer
  : public QObject
{
  Q_OBJECT

private:
  Q_SLOT void preserves_generated_sample_alignment ()
  {
    QTemporaryDir dir;
    QVERIFY (dir.isValid ());

    QProcess process;
    process.setWorkingDirectory (dir.path ());
    process.start (ECHOSIM_EXECUTABLE, {"1500", "0.0", "0.0", "1", "100"});
    QVERIFY (wait_for_echosim (process));
    QCOMPARE (process.exitStatus (), QProcess::NormalExit);
    QCOMPARE (process.exitCode (), 0);

    QDir out {dir.path ()};
    auto const wavs = out.entryList ({"*.wav"}, QDir::Files);
    QCOMPARE (wavs.size (), 1);

    BWFFile file {default_format (), out.filePath (wavs.first ())};
    QVERIFY (file.open (QIODevice::ReadOnly));

    // The first ten samples contain Echo metadata; sample 11 is generated audio.
    qint16 sample;
    QVERIFY (file.seek (10 * sizeof sample));
    QCOMPARE (file.read (reinterpret_cast<char *> (&sample), sizeof sample),
              static_cast<qint64> (sizeof sample));
    sample = qFromLittleEndian (sample);
    QVERIFY (sample >= 23168 && sample <= 23172);

    file.close ();
  }

  Q_SLOT void writes_correct_data_size_and_list_info_trailer ()
  {
    QTemporaryDir dir;
    QVERIFY (dir.isValid ());

    QProcess process;
    process.setWorkingDirectory (dir.path ());
    process.start (ECHOSIM_EXECUTABLE, {"1500", "0.0", "4.0", "1", "-22"});
    QVERIFY (wait_for_echosim (process));
    QCOMPARE (process.exitStatus (), QProcess::NormalExit);
    QCOMPARE (process.exitCode (), 0);

    QDir out {dir.path ()};
    auto const wavs = out.entryList ({"*.wav"}, QDir::Files);
    QCOMPARE (wavs.size (), 1);

    BWFFile file {default_format (), out.filePath (wavs.first ())};
    QVERIFY (file.open (QIODevice::ReadOnly));

    qint64 constexpr sample_count = 36000;
    QCOMPARE (file.size (), 2 * sample_count);

    BWFFile::InfoDictionary::key_type const isft {{'I', 'S', 'F', 'T'}};
    BWFFile::InfoDictionary::key_type const icrd {{'I', 'C', 'R', 'D'}};
    BWFFile::InfoDictionary::key_type const icmt {{'I', 'C', 'M', 'T'}};
    QVERIFY (file.list_info ().contains (isft));
    QVERIFY (file.list_info ().contains (icrd));
    QVERIFY (file.list_info ().contains (icmt));
    QVERIFY (!file.list_info ().value (icmt).isEmpty ());

    file.close ();
  }
};

QTEST_GUILESS_MAIN (TestEchosimWavTrailer)

#include "test_echosim_wav_trailer.moc"
