#include "livecq_decode.hpp"

#include "../Network/DecodedTime.hpp"
#include "../validators/LiveCQCallsign.hpp"

namespace
{
  bool isReportableMessageType(QString const& messageType)
  {
    return messageType == "CQ" || messageType == "QRZ"
      || messageType == "CQV" || messageType == "CQH"
      || messageType == "QRT";
  }

  bool parseMode(QString const& modeToken, QString& mode)
  {
    if (modeToken.contains('#')) {
      mode = QString {"JT65"} + modeToken.back();
      return true;
    }
    if (modeToken.contains(':')) {
      mode = QString {"Q65-60"} + modeToken.back();
      return true;
    }
    return false;
  }
}

namespace Map65LiveCQ
{
  QList<Spot> parseSpots(QStringList const& decodeLines,
                         QString const& receiverCallsign,
                         QString const& receiverLocator,
                         bool crossPolarization,
                         QDateTime const& nowUtc,
                         QStringList& seenDecodes)
  {
    QList<Spot> spots;
    if (receiverCallsign.length() < 3 || receiverLocator.length() < 4) {
      return spots;
    }

    for (auto const& line : decodeLines) {
      auto const tokens = line.split(' ', Qt::SkipEmptyParts);
      if (tokens.size() < 6
          || !isReportableMessageType(tokens.at(5).trimmed())
          || !seenDecodes.filter(line.left(53)).isEmpty()) {
        continue;
      }
      seenDecodes.append(line);

      Spot spot;
      spot.scheduledFrequency = tokens.at(0).trimmed();
      spot.receiveFrequency = tokens.at(1).trimmed();
      spot.snr = tokens.at(4).trimmed();
      spot.messageType = tokens.at(5).trimmed().toUpper();

      QString callsign;
      QString modeToken;
      QString grid {"--"};
      QString polarization {"--"};
      QString transmitPolarization {"--"};

      if (tokens.size() >= 9 && tokens.at(7).contains('.')) {
        callsign = tokens.at(6).trimmed().toUpper();
        spot.deltaTime = tokens.at(7).trimmed();
        modeToken = tokens.at(8).trimmed();
        if (crossPolarization) {
          polarization = tokens.at(2).trimmed();
        }
      } else if (tokens.size() >= 10 && tokens.at(8).contains('.')) {
        auto const firstCandidate = tokens.at(6).trimmed().toUpper();
        if (LiveCQ::isValidCallsign(firstCandidate)) {
          callsign = firstCandidate;
          grid = tokens.at(7).trimmed();
        } else {
          callsign = tokens.at(7).trimmed().toUpper();
        }
        spot.deltaTime = tokens.at(8).trimmed();
        modeToken = tokens.at(9).trimmed();
        if (crossPolarization) {
          polarization = tokens.at(2).trimmed();
          if (tokens.size() == 11) {
            transmitPolarization = tokens.at(10).trimmed();
          }
        }
      } else if (tokens.size() >= 11 && tokens.at(9).contains('.')) {
        callsign = tokens.at(7).trimmed().toUpper();
        grid = tokens.at(8).trimmed();
        spot.deltaTime = tokens.at(9).trimmed();
        modeToken = tokens.at(10).trimmed();
        if (crossPolarization) {
          polarization = tokens.at(2).trimmed();
          if (tokens.size() == 12) {
            transmitPolarization = tokens.at(11).trimmed();
          }
        }
      } else {
        continue;
      }

      if (!LiveCQ::isValidCallsign(callsign)
          || !parseMode(modeToken, spot.mode)) {
        continue;
      }

      spot.utcDateTime = DecodedTime::spotTime(tokens.at(3).trimmed(), nowUtc, 60);
      if (!spot.utcDateTime.isValid()) {
        continue;
      }

      spot.callsign = callsign;
      spot.grid = grid;
      spot.polarization = polarization;
      spot.transmitPolarization = transmitPolarization;
      spots.append(spot);
    }
    return spots;
  }

  QUrlQuery spotQuery(Spot const& spot,
                      QString const& receiverCallsign,
                      QString const& receiverLocator)
  {
    QUrlQuery query;
    query.addQueryItem("skedfreq", spot.scheduledFrequency);
    query.addQueryItem("rxfreq", spot.receiveFrequency);
    query.addQueryItem("rpol", spot.polarization);
    query.addQueryItem("dt", spot.deltaTime);
    query.addQueryItem("dB", spot.snr);
    query.addQueryItem("msgtype", spot.messageType);
    query.addQueryItem("callsign", spot.callsign);
    query.addQueryItem("grid", spot.grid.toUpper());
    query.addQueryItem("mode", spot.mode);
    query.addQueryItem("utcdatetime",
                       spot.utcDateTime.toUTC().toString("yyyy-MM-ddTHH:mm:ss") + "Z");
    query.addQueryItem("spotter", receiverCallsign.toUpper());
    query.addQueryItem("spottergrid", receiverLocator.toUpper());
    query.addQueryItem("txpol", spot.transmitPolarization);
    query.addQueryItem("apptype", "MAP65");
    return query;
  }
}
