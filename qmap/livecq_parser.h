#ifndef QMAP_LIVE_CQ_PARSER_H
#define QMAP_LIVE_CQ_PARSER_H

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace QMapLiveCQ
{
  struct Record
  {
    QString callsign;
    QString grid;
    QString message;
    int receiveFrequency {0};
    int scheduledFrequency {0};
  };

  bool parse(QStringList const& tokens, int frequencyOffset, Record& record);

  // UTC date and time of a decode whose time is given as "hhmmss" or "hhmm".
  // The date is today's (from nowUtc), or yesterday's when that would put the
  // decode more than a minute in the future (a period that began before
  // midnight). Invalid if the time is malformed.
  QDateTime spotTime(QString const& decodeTime, QDateTime const& nowUtc);
}

#endif
