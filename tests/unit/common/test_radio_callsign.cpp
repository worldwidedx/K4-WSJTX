#include <QtTest>

#include "Radio.hpp"

class TestRadioCallsign final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void identity_data ();
  void identity ();
  void classification_data ();
  void classification ();
  void standard_data ();
  void standard ();
  void decodedGrid_data ();
  void decodedGrid ();
};

void TestRadioCallsign::identity_data ()
{
  QTest::addColumn<QString> ("callsign");
  QTest::addColumn<QString> ("base");
  QTest::addColumn<QString> ("prefix");

  QTest::newRow ("ordinary") << QString {"K1ABC"} << QString {"K1ABC"} << QString {"K1ABC"};
  QTest::newRow ("portable prefix") << QString {"EA8/K1ABC"} << QString {"K1ABC"} << QString {"EA8"};
  QTest::newRow ("portable suffix") << QString {"K1ABC/EA8"} << QString {"K1ABC"} << QString {"EA8"};
  QTest::newRow ("equal length") << QString {"EA8/K1A"} << QString {"K1A"} << QString {"EA8"};
}

void TestRadioCallsign::identity ()
{
  QFETCH (QString, callsign);
  QFETCH (QString, base);
  QFETCH (QString, prefix);

  for (auto const& suffix : {QString {}, QString {"/P"}, QString {"/QRP"}, QString {"/P/QRP"}})
    {
      auto const input = callsign + suffix;
      QCOMPARE (Radio::base_callsign (input), base);
      QCOMPARE (Radio::effective_prefix (input), prefix);
      QCOMPARE (Radio::base_callsign (input.toLower ()), base);
      QCOMPARE (Radio::effective_prefix (input.toLower ()), prefix);
    }
}

void TestRadioCallsign::classification_data ()
{
  QTest::addColumn<QString> ("callsign");
  QTest::addColumn<bool> ("valid");
  QTest::addColumn<bool> ("compound");
  QTest::addColumn<bool> ("nonstandard");

  QTest::newRow ("standard") << QString {"K1ABC"} << true << false << false;
  QTest::newRow ("compound") << QString {"K1ABC/P"} << true << true << true;
  QTest::newRow ("long prefix") << QString {"3DA0XYZ"} << true << false << true;
  QTest::newRow ("invalid alphabet") << QString {"---"} << false << false << false;
  QTest::newRow ("empty") << QString {} << false << false << false;
}

void TestRadioCallsign::classification ()
{
  QFETCH (QString, callsign);
  QFETCH (bool, valid);
  QFETCH (bool, compound);
  QFETCH (bool, nonstandard);

  QCOMPARE (Radio::is_callsign (callsign), valid);
  QCOMPARE (Radio::is_compound_callsign (callsign), compound);
  QCOMPARE (Radio::is_77bit_nonstandard_callsign (callsign), nonstandard);
}

void TestRadioCallsign::standard_data ()
{
  QTest::addColumn<QString> ("callsign");
  QTest::addColumn<bool> ("standard");
  QTest::newRow ("ordinary") << QString {"K1ABC"} << true;
  QTest::newRow ("case and whitespace") << QString {"  k1abc  "} << true;
  QTest::newRow ("portable") << QString {"K1ABC/P"} << true;
  QTest::newRow ("rover") << QString {"K1ABC/r"} << true;
  QTest::newRow ("digit prefix") << QString {"3D2ABC"} << true;
  QTest::newRow ("letter digit prefix") << QString {"A12ABC"} << true;
  QTest::newRow ("no suffix letters") << QString {"K1"} << true;
  QTest::newRow ("no prefix") << QString {"1ABC"} << true;
  QTest::newRow ("single digit") << QString {"1"} << true;
  QTest::newRow ("compound prefix") << QString {"EA8/K1ABC"} << false;
  QTest::newRow ("unsupported suffix") << QString {"K1ABC/QRP"} << false;
  QTest::newRow ("long prefix") << QString {"3DA0XYZ"} << false;
  QTest::newRow ("long suffix") << QString {"K1ABCD"} << false;
  QTest::newRow ("internal whitespace") << QString {"K1 ABC"} << false;
  QTest::newRow ("empty") << QString {} << false;
}

void TestRadioCallsign::standard ()
{
  QFETCH (QString, callsign);
  QFETCH (bool, standard);
  QCOMPARE (Radio::is_standard_callsign (callsign), standard);
}

void TestRadioCallsign::decodedGrid_data ()
{
  QTest::addColumn<QString> ("grid");
  QTest::addColumn<bool> ("valid");
  QTest::newRow ("four characters") << QString {"FN42"} << true;
  QTest::newRow ("six characters mixed case") << QString {"fn42aB"} << true;
  QTest::newRow ("range limits") << QString {"RR99XX"} << true;
  QTest::newRow ("acknowledgement") << QString {"RR73"} << false;
  QTest::newRow ("acknowledgement case") << QString {"rR73"} << false;
  QTest::newRow ("acknowledgement prefix") << QString {"RR73AA"} << false;
  QTest::newRow ("field range") << QString {"SS42"} << false;
  QTest::newRow ("subsquare range") << QString {"FN42YY"} << false;
  QTest::newRow ("leading space") << QString {" FN42"} << false;
  QTest::newRow ("trailing space") << QString {"FN42 "} << false;
  QTest::newRow ("empty") << QString {} << false;
}

void TestRadioCallsign::decodedGrid ()
{
  QFETCH (QString, grid);
  QFETCH (bool, valid);
  QCOMPARE (grid.contains (QRegularExpression {Radio::decoded_grid_pattern ()}), valid);
}

QTEST_GUILESS_MAIN (TestRadioCallsign)

#include "test_radio_callsign.moc"
