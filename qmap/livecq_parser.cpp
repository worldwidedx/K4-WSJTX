#include "livecq_parser.h"

#include "../validators/LiveCQCallsign.hpp"

namespace
{
  bool isReportableMessageType(QString const& messageType)
  {
    return messageType == "CQ" || messageType == "QRZ" || messageType == "CQV"
      || messageType == "CQH" || messageType == "QRT";
  }

  bool parseFrequency(QString const& token, int frequencyOffset,
                      int& receiveFrequency, int& scheduledFrequency)
  {
    auto const fields = token.split('.');
    if (fields.size() != 2 || fields.at(0).isEmpty() || fields.at(1).isEmpty()) {
      return false;
    }

    bool ok = false;
    auto const integerPart = fields.at(0).toInt(&ok);
    if (!ok) {
      return false;
    }
    auto const fractionalPart = fields.at(1).toInt(&ok);
    if (!ok) {
      return false;
    }

    receiveFrequency = frequencyOffset + fractionalPart;
    if (receiveFrequency <= frequencyOffset + 500) {
      scheduledFrequency = integerPart;
    } else {
      scheduledFrequency = integerPart + 1;
      receiveFrequency -= 1000;
    }
    return true;
  }
}

namespace QMapLiveCQ
{
  bool parse(QStringList const& tokens, int frequencyOffset, Record& record)
  {
    if (tokens.size() < 9 || tokens.size() > 11
        || !isReportableMessageType(tokens.at(6).trimmed())) {
      return false;
    }

    Record parsed;
    parsed.grid = "--";
    auto const messageType = tokens.at(6).trimmed();
    QString frequencyToken;

    if (tokens.size() == 9) {
      parsed.callsign = tokens.at(7).trimmed();
      if (!LiveCQ::isValidCallsign(parsed.callsign)) {
        return false;
      }
      parsed.message = messageType + " " + parsed.callsign;
      frequencyToken = tokens.at(8);
    } else if (tokens.size() == 10) {
      auto const firstCandidate = tokens.at(7).trimmed();
      if (LiveCQ::isValidCallsign(firstCandidate)) {
        parsed.callsign = firstCandidate;
        parsed.grid = tokens.at(8).trimmed();
        parsed.message = messageType + " " + parsed.callsign + " " + parsed.grid;
      } else {
        parsed.callsign = tokens.at(8).trimmed();
        if (!LiveCQ::isValidCallsign(parsed.callsign)) {
          return false;
        }
        parsed.message = messageType + " " + parsed.callsign;
      }
      frequencyToken = tokens.at(9);
    } else {
      parsed.callsign = tokens.at(8).trimmed();
      if (!LiveCQ::isValidCallsign(parsed.callsign)) {
        return false;
      }
      parsed.grid = tokens.at(9).trimmed();
      parsed.message = messageType + " " + parsed.callsign + " " + parsed.grid;
      frequencyToken = tokens.at(10);
    }

    if (!parseFrequency(frequencyToken, frequencyOffset,
                        parsed.receiveFrequency, parsed.scheduledFrequency)) {
      return false;
    }

    record = parsed;
    return true;
  }

  QDateTime spotTime(QString const& decodeTime, QDateTime const& nowUtc)
  {
    if ((decodeTime.size() != 4 && decodeTime.size() != 6) || !nowUtc.isValid()) {
      return {};
    }
    for (auto const character : decodeTime) {
      if (!character.isDigit()) {
        return {};
      }
    }
    QTime const time {decodeTime.mid(0, 2).toInt(), decodeTime.mid(2, 2).toInt(),
                      decodeTime.size() == 6 ? decodeTime.mid(4, 2).toInt() : 0};
    if (!time.isValid()) {
      return {};
    }

    auto const now = nowUtc.toUTC();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QDateTime result {now.date(), time, QTimeZone::UTC};
#else
    QDateTime result {now.date(), time, Qt::UTC};
#endif
    if (result > now.addSecs(60)) {
      result = result.addDays(-1);
    }
    return result;
  }
}
