#include <QtTest>

#include "Audio/soundin.h"

namespace
{
  class TestAudioSink final : public AudioDevice
  {
  public:
    void inputInterrupted () override {++interruptions;}
    int interruptions {0};

  protected:
    qint64 readData (char *, qint64) override {return -1;}
    qint64 writeData (char const *, qint64 size) override {return size;}
  };
}

class TestSoundInput : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void sticky_underrun_does_not_interrupt_recovered_input ()
  {
    TestAudioSink sink;
    SoundInput input;
    input.m_sink = &sink;
    input.updateStreamInterruption (QAudio::ActiveState, QAudio::NoError);
    input.updateStreamInterruption (QAudio::IdleState, QAudio::UnderrunError);
    input.updateStreamInterruption (QAudio::IdleState, QAudio::UnderrunError);
    QCOMPARE (sink.interruptions, 1);

    input.updateStreamInterruption (QAudio::ActiveState, QAudio::UnderrunError);
    for (int notification = 0; notification < 5; ++notification)
      input.updateStreamInterruption (QAudio::ActiveState, QAudio::UnderrunError);
    QCOMPARE (sink.interruptions, 1);

    input.updateStreamInterruption (QAudio::IdleState, QAudio::UnderrunError);
    QCOMPARE (sink.interruptions, 2);
    input.updateStreamInterruption (QAudio::ActiveState, QAudio::UnderrunError);
    input.updateStreamInterruption (QAudio::ActiveState, QAudio::NoError);
    input.updateStreamInterruption (QAudio::ActiveState, QAudio::UnderrunError);
    QCOMPARE (sink.interruptions, 3);
  }

  void interruption_states_rearm_after_recovery ()
  {
    TestAudioSink sink;
    SoundInput input;
    input.m_sink = &sink;
    QList<QAudio::State> states {QAudio::IdleState, QAudio::SuspendedState};
#if QT_VERSION >= QT_VERSION_CHECK (5, 10, 0)
    states.append (QAudio::InterruptedState);
#endif
    int expected = 0;
    for (auto state : states)
      {
        input.updateStreamInterruption (QAudio::ActiveState, QAudio::NoError);
        input.updateStreamInterruption (state, QAudio::NoError);
        input.updateStreamInterruption (state, QAudio::NoError);
        QCOMPARE (sink.interruptions, ++expected);
      }
  }

  void hard_errors_and_stream_replacement_remain_observable ()
  {
    TestAudioSink sink;
    SoundInput input;
    input.m_sink = &sink;
    int expected = 0;
    for (auto error : {QAudio::OpenError, QAudio::IOError, QAudio::FatalError})
      {
        input.updateStreamInterruption (QAudio::StoppedState, error);
        input.updateStreamInterruption (QAudio::StoppedState, error);
        QCOMPARE (sink.interruptions, ++expected);
        input.stop ();
      }
    input.updateStreamInterruption (QAudio::StoppedState, QAudio::FatalError);
    QCOMPARE (sink.interruptions, ++expected);
  }
};

QTEST_GUILESS_MAIN (TestSoundInput)
#include "test_sound_input.moc"
