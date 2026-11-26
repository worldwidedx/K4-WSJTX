#ifndef MAP65_LIVECQ_DECODE_HPP
#define MAP65_LIVECQ_DECODE_HPP

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrlQuery>

namespace Map65LiveCQ
{
  struct Spot
  {
    QString scheduledFrequency;
    QString receiveFrequency;
    QString polarization {"--"};
    QString deltaTime;
    QString snr;
    QString messageType;
    QString callsign;
    QString grid {"--"};
    QString mode;
    QString transmitPolarization {"--"};
    QDateTime utcDateTime;
  };

  QList<Spot> parseSpots(QStringList const& decodeLines,
                         QString const& receiverCallsign,
                         QString const& receiverLocator,
                         bool crossPolarization,
                         QDateTime const& nowUtc,
                         QStringList& seenDecodes);

  QUrlQuery spotQuery(Spot const& spot,
                      QString const& receiverCallsign,
                      QString const& receiverLocator);
}

#endif
