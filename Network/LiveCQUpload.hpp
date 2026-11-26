#ifndef LIVE_CQ_UPLOAD_HPP
#define LIVE_CQ_UPLOAD_HPP

#include <QByteArray>
#include <QNetworkRequest>
#include <QObject>
#include <QSet>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

class QNetworkReply;

class LiveCQUpload final
  : public QObject
{
  Q_OBJECT

public:
  class Transport
  {
  public:
    virtual ~Transport () = default;
    virtual QNetworkReply *post (QNetworkRequest const& request, QByteArray const& body) = 0;
  };

  explicit LiveCQUpload (QObject *parent = nullptr);
  LiveCQUpload (Transport *transport, bool take_transport_ownership,
                QObject *parent = nullptr);
  ~LiveCQUpload () override;

  bool setEndpoint (QUrl const& endpoint);
  QUrl endpoint () const { return endpoint_; }

  void setUserAgent (QByteArray const& user_agent) { user_agent_ = user_agent; }

  void postSpot (QUrlQuery const& query);
  void abortOutstandingRequests ();

  Q_SIGNAL void errorOccurred (QString const& reason);
  Q_SIGNAL void spotPosted ();

private:
  void handleReply (QNetworkReply *reply);

  Transport *transport_;
  bool owns_transport_;
  QUrl endpoint_;
  QByteArray user_agent_;
  QSet<QNetworkReply *> outstanding_;
};

#endif
