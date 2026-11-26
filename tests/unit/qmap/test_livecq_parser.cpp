#include <QtTest>

#include "qmap/livecq_parser.h"

class TestQMapLiveCQParser final : public QObject
{
  Q_OBJECT

private slots:
  void parsesSupportedLayouts()
  {
    QMapLiveCQ::Record record;
    QVERIFY(QMapLiveCQ::parse(
      {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "N5L", "1200.0"},
      100, record));
    QCOMPARE(record.callsign, QString {"N5L"});
    QCOMPARE(record.grid, QString {"--"});
    QCOMPARE(record.message, QString {"CQ N5L"});
    QCOMPARE(record.receiveFrequency, 100);
    QCOMPARE(record.scheduledFrequency, 1200);

    QVERIFY(QMapLiveCQ::parse(
      {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "N5L", "FN20", "1200.0"},
      100, record));
    QCOMPARE(record.grid, QString {"FN20"});
    QCOMPARE(record.message, QString {"CQ N5L FN20"});

    QVERIFY(QMapLiveCQ::parse(
      {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "DX", "N5L", "1200.0"},
      100, record));
    QCOMPARE(record.callsign, QString {"N5L"});
    QCOMPARE(record.grid, QString {"--"});
    QCOMPARE(record.message, QString {"CQ N5L"});

    QVERIFY(QMapLiveCQ::parse(
      {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "DX", "N5L", "FN20", "1200.0"},
      100, record));
    QCOMPARE(record.grid, QString {"FN20"});
    QCOMPARE(record.message, QString {"CQ N5L FN20"});
  }

  void parsesHighFrequencyFraction()
  {
    QMapLiveCQ::Record record;
    QVERIFY(QMapLiveCQ::parse(
      {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "N5L", "1200.750"},
      100, record));
    QCOMPARE(record.receiveFrequency, -150);
    QCOMPARE(record.scheduledFrequency, 1201);
  }

  void rejectsMalformedRecordsBeforeIndexing()
  {
    QMapLiveCQ::Record record;
    for (auto const& tokens : {
           QStringList {},
           QStringList {"123456", "-10", "0.1", "1234", "-12", "60", "CQ"},
           QStringList {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "N5L"},
           QStringList {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "N5L", "FN20", "1200.0", "extra", "too-much"}}) {
      QVERIFY(!QMapLiveCQ::parse(tokens, 100, record));
    }
  }

  void rejectsMalformedFrequencyAndCalls()
  {
    QMapLiveCQ::Record record;
    for (auto const& frequency : {QString {"1200"}, QString {"1200."},
                                  QString {"1200.x"}, QString {"1200.0.1"}}) {
      QVERIFY(!QMapLiveCQ::parse(
        {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "N5L", frequency},
        100, record));
    }
    QVERIFY(!QMapLiveCQ::parse(
      {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "N5", "1200.0"},
      100, record));
    QVERIFY(!QMapLiveCQ::parse(
      {"123456", "-10", "0.1", "1234", "-12", "60", "CQ", "DX", "N5", "1200.0"},
      100, record));
  }

  void spotTimeUsesTodaysUtcDate()
  {
    QDateTime const now {QDate {2026, 9, 30}, QTime {18, 7, 10}, Qt::UTC};
    QCOMPARE(QMapLiveCQ::spotTime("180630", now),
             QDateTime(QDate {2026, 9, 30}, QTime {18, 6, 30}, Qt::UTC));
    QCOMPARE(QMapLiveCQ::spotTime("1806", now),
             QDateTime(QDate {2026, 9, 30}, QTime {18, 6}, Qt::UTC));
    QCOMPARE(QMapLiveCQ::spotTime("180630", now).toString("yyyy-MM-ddTHH:mm:ss") + "Z",
             QString {"2026-09-30T18:06:30Z"});
  }

  void spotTimeIsIndependentOfLocale()
  {
    QDateTime const now {QDate {2026, 9, 30}, QTime {18, 7, 10}, Qt::UTC};
    auto const previous = QLocale {};
    for (auto const name : {"cs_CZ", "es_AR", "hu_HU", "fr_FR", "de_DE", "zh_TW", "ru_RU"}) {
      QLocale::setDefault(QLocale {name});
      QCOMPARE(QMapLiveCQ::spotTime("180630", now).toString("yyyy-MM-ddTHH:mm:ss") + "Z",
               QString {"2026-09-30T18:06:30Z"});
    }
    QLocale::setDefault(previous);
  }

  void spotTimeAcrossMidnight()
  {
    // Early decode of the last period before midnight: still the same day.
    QCOMPARE(QMapLiveCQ::spotTime("235930", QDateTime(QDate {2026, 9, 30}, QTime {23, 59, 52}, Qt::UTC)),
             QDateTime(QDate {2026, 9, 30}, QTime {23, 59, 30}, Qt::UTC));
    // Decoded just after midnight: the period began yesterday.
    QCOMPARE(QMapLiveCQ::spotTime("235900", QDateTime(QDate {2026, 10, 1}, QTime {0, 0, 5}, Qt::UTC)),
             QDateTime(QDate {2026, 9, 30}, QTime {23, 59}, Qt::UTC));
    QCOMPARE(QMapLiveCQ::spotTime("235930", QDateTime(QDate {2026, 10, 1}, QTime {0, 0, 22}, Qt::UTC)),
             QDateTime(QDate {2026, 9, 30}, QTime {23, 59, 30}, Qt::UTC));
    // First period of the new day.
    QCOMPARE(QMapLiveCQ::spotTime("000000", QDateTime(QDate {2026, 10, 1}, QTime {0, 0, 52}, Qt::UTC)),
             QDateTime(QDate {2026, 10, 1}, QTime {0, 0}, Qt::UTC));
    // Clock a few seconds behind the decoder: not pushed back a day.
    QCOMPARE(QMapLiveCQ::spotTime("120000", QDateTime(QDate {2026, 9, 30}, QTime {11, 59, 58}, Qt::UTC)),
             QDateTime(QDate {2026, 9, 30}, QTime {12, 0}, Qt::UTC));
  }

  void spotTimeRejectsMalformedTimes()
  {
    QDateTime const now {QDate {2026, 9, 30}, QTime {18, 7, 10}, Qt::UTC};
    for (auto const& decodeTime : {QString {}, QString {"123"}, QString {"1234567"},
                                   QString {"2460"}, QString {"1260"}, QString {"126099"},
                                   QString {"12x4"}, QString {"12:34"}}) {
      QVERIFY(!QMapLiveCQ::spotTime(decodeTime, now).isValid());
    }
    QVERIFY(!QMapLiveCQ::spotTime("123456", QDateTime {}).isValid());
  }
};

QTEST_APPLESS_MAIN(TestQMapLiveCQParser)

#include "test_livecq_parser.moc"
