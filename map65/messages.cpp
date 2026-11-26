#include "messages.h"
#include <QSettings>
#include "SettingsGroup.hpp"
#include "ui_messages.h"
#include "mainwindow.h"
#include "qt_helpers.hpp"
#include "../revision_utils.hpp"
#include "../Network/PSKReporter.hpp"
#include "../Network/LiveCQUpload.hpp"
#include "livecq_decode.hpp"
#include "pskreporter_decode.h"
#include "pskreporter_settings.h"

#include <QCoreApplication> //liveCQ
#include <QDateTime>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <QDebug>

Messages::Messages (QString const& settings_filename, QString const& eclipse_filename,
                    QWidget * parent) :
  QDialog {parent},
  ui {new Ui::Messages},
  m_settings_filename {settings_filename}
{
  ui->setupUi(this);
  setWindowTitle("Messages");
  setWindowFlags (Qt::Dialog | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
  QSettings settings {m_settings_filename, QSettings::IniFormat};
  SettingsGroup g {&settings, "MainWindow"}; // MainWindow group for
                                             // historical reasons
  setGeometry (settings.value ("MessagesGeom", QRect {800, 400, 381, 400}).toRect ());
  ui->messagesTextBrowser->setStyleSheet( \
          "QTextBrowser { background-color : #000066; color : red; }");
  ui->messagesTextBrowser->clear();  
  
  QSettings settings2 {m_settings_filename, QSettings::IniFormat};
  auto const pskReporterSettings = readMap65PSKReporterSettings(settings2);
  SettingsGroup h {&settings2, "Common"};
  m_spot_to_psk_reporter = pskReporterSettings.enabled;

  m_cqOnly=false;
  m_cqStarOnly=false;
  connect (ui->messagesTextBrowser, &DisplayText::selectCallsign, this, &Messages::selectCallsign2);

  m_livecq = std::make_unique<LiveCQUpload>(this);
  m_livecq->setUserAgent(http_user_agent().toUtf8());
  connect(m_livecq.get(), &LiveCQUpload::errorOccurred, this, &Messages::errorOccurred);

  m_psk_reporter.reset(new PSKReporter({
    pskReporterSettings.use_tcpip,
    eclipse_filename,
    QString {"MAP65 v" + QCoreApplication::applicationVersion()
             + " " + revision()}.simplified()
  }));
  connect(m_psk_reporter.get(), &PSKReporter::errorOccurred,
          this, &Messages::errorOccurred);
  if (m_spot_to_psk_reporter) {
    initializePSKReporting();
  }
}
 
Messages::~Messages()
{
  //QSettings settings {m_settings_filename, QSettings::IniFormat};
  //SettingsGroup g {&settings, "MainWindow"};
  //settings.setValue ("MessagesGeom", geometry ());
  if (m_psk_reporter) {
    m_psk_reporter->sendReport(true);
  }
  delete ui;
}

void Messages::closeEvent(QCloseEvent *event)
{
  if (!m_closingForShutdown) {
      hide();
      event->ignore(); // Don't close, just hide
      return;
  }

  // app shutdown
  QSettings settings {m_settings_filename, QSettings::IniFormat};
  SettingsGroup g {&settings, "MainWindow"};
  settings.setValue ("MessagesGeom", geometry ());
  settings.sync(); // Ensure data is written to disk 
  event->accept(); // Allow destruction 
}   

void Messages::initializePSKReporting()
{  
  QSettings settings {m_settings_filename, QSettings::IniFormat};
  SettingsGroup g {&settings, "Common"}; 
  QString receiverCallsign=settings.value("MyCall","").toString();
  QString receiverLocator=settings.value("MyGrid","").toString();
  m_psk_reporter->setLocalStation(receiverCallsign, receiverLocator,
                                  "N/A", "N/A (MAP65)");
}

void Messages::setPSKReportingEnabled(bool enabled)
{
  auto const was_enabled = m_spot_to_psk_reporter;
  m_spot_to_psk_reporter = enabled;
  if (enabled) {
    initializePSKReporting();
  } else if (was_enabled) {
    m_psk_reporter->sendReport(true);
  }
}

void Messages::sendLiveCQData(QStringList decodeList) {
  QSettings settings {m_settings_filename, QSettings::IniFormat};
  SettingsGroup g {&settings, "Common"};
  bool const m_w3szUrl = settings.value("w3szUrl", true).toBool();
  QString const m_otherUrl = settings.value("otherUrl", "").toString();
  QString const m_myCall = settings.value("MyCall", "").toString();
  QString const m_myGrid = settings.value("MyGrid", "").toString();
  bool const m_xpol = settings.value("Xpol", false).toBool();

  QString const theUrl = m_w3szUrl ? w3szUrlAddr : m_otherUrl;
  QUrl const endpoint {theUrl};
  if (!endpoint.isValid()
      || endpoint.scheme().compare("https", Qt::CaseInsensitive) != 0) {
    return;
  }
  if (!m_livecq->setEndpoint(endpoint)) {
    return;
  }

  auto const spots = Map65LiveCQ::parseSpots(
    decodeList, m_myCall, m_myGrid, m_xpol, QDateTime::currentDateTimeUtc(),
    m_livecqSeenDecodes);

  while (m_livecqSeenDecodes.size() > maxLiveCQSeenDecodes) {
    m_livecqSeenDecodes.removeFirst();
  }

  for (auto const& spot : spots) {
    m_livecq->postSpot(Map65LiveCQ::spotQuery(spot, m_myCall, m_myGrid));
  }
}

void Messages::clearLiveCQHistory()
{
  m_livecqSeenDecodes.clear();
}

void Messages::setText(QString t, QString t2)
{
  QString cfreq,cfreq0;
  m_t=t;
  m_t2=t2;

  QStringList cqliveText;  //liveCQ
  doLiveCQ = true;         //liveCQ

  QString s="QTextBrowser{background-color: "+m_colorBackground+"}";
  ui->messagesTextBrowser->setStyleSheet(s);

  ui->messagesTextBrowser->clear();
  QStringList lines = t.split( "\n", SkipEmptyParts );
  foreach( QString line, lines ) {
    QString t1=line.mid(0,81); //was 0,75
    int ncq=t1.indexOf(" CQ ");
    if((m_cqOnly or m_cqStarOnly) and  ncq< 0) continue;
    if(m_cqStarOnly) {
      QString caller=t1.mid(ncq+4,-1);
      int nz=caller.indexOf(" ");
      caller=caller.mid(0,nz);
      int i=t2.indexOf(caller);
      if(t2.mid(i-1,1)==" ") continue;
    }
    int n=line.mid(61,2).toInt();  //was 55,2
//    if(line.indexOf(":")>0) n=-1;
//    if(n==-1) ui->messagesTextBrowser->setTextColor("#ffffff");  // white
    if(n==0) ui->messagesTextBrowser->setTextColor(m_color0);
    if(n==1) ui->messagesTextBrowser->setTextColor(m_color1);
    if(n==2) ui->messagesTextBrowser->setTextColor(m_color2);
    if(n>=3) ui->messagesTextBrowser->setTextColor(m_color3);
    QString livecqStr = t1.mid(0,59) + t1.mid(62,t1.length()-62) + " " + t1.mid(60,2); // was 53,56,56,54
    if(cqliveText.filter(livecqStr.mid(0,59)).length()==0) cqliveText.append(livecqStr); // was 0,53
    cfreq=t1.mid(5,3);
    if(cfreq == cfreq0) {
      t1="        " + t1.mid(8,-1);
    }
    cfreq0=cfreq;
    ui->messagesTextBrowser->append(t1.mid(5,67)); //was 5,61
  }
  if(doLiveCQ && cqliveText.size() > 0) {       //liveCQ
      sendLiveCQData(cqliveText);     //liveCQ
      doLiveCQ = false;               //liveCQ
    }                                 //liveCQ
  if (m_spot_to_psk_reporter && cqliveText.size() > 0) {
      sendPSKReporterData(cqliveText); //PSKReporter
  }
}

void Messages::sendPSKReporterData(QStringList decodeList) {
  QSettings settings {m_settings_filename, QSettings::IniFormat};
  SettingsGroup g {&settings, "Common"};
  auto const spots = Map65PSKReporter::parseMap65PSKReporterSpots(
      decodeList,
      settings.value("MyCall", "").toString(),
      settings.value("MyGrid", "").toString(),
      QDateTime::currentDateTimeUtc(),
      allDecodes2);

  for (auto const& spot : spots) {
    m_psk_reporter->addRemoteStation(spot.callsign, spot.locator,
                                      spot.frequency, spot.mode, spot.snr,
                                      spot.time);
  }
}

void Messages::selectCallsign2(bool ctrl)
{
  QString t = ui->messagesTextBrowser->toPlainText();   //Full contents
  int i=ui->messagesTextBrowser->textCursor().position();
  int i0=t.lastIndexOf(" ",i);
  int i1=t.indexOf(" ",i);
  QString hiscall=t.mid(i0+1,i1-i0-1);
  if(hiscall!="") {
    if(hiscall.length() < 13) {
      QString t1 = t.mid(0,i);              //contents up to text cursor
      int i1=t1.lastIndexOf("\n") + 1;
      QString t2 = t.mid(i1,-1);            //selected line to end
      int i2=t2.indexOf("\n");
      t2=t2.left(i2);                       //selected line
      emit click2OnCallsign(hiscall,t2,ctrl);
    }
  }
}

void Messages::setColors(QString t)
{
  m_colorBackground = "#"+t.mid(0,6);
  m_color0 = "#"+t.mid(6,6);
  m_color1 = "#"+t.mid(12,6);
  m_color2 = "#"+t.mid(18,6);
  m_color3 = "#"+t.mid(24,6);
  setText(m_t,m_t2);
}

void Messages::on_cbCQ_toggled(bool checked)
{
  m_cqOnly = checked;
  setText(m_t,m_t2);
}

void Messages::on_cbCQstar_toggled(bool checked)
{
  m_cqStarOnly = checked;
  setText(m_t,m_t2);
}
