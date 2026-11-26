#ifndef SAMPLE_DOWNLOADER_HPP__
#define SAMPLE_DOWNLOADER_HPP__

#include <QObject>
#include <QSettings>
#include <QString>

#include "pimpl_h.hpp"

class QWidget;
class QNetworkAccessManager;
class Configuration;

//
// SampleDownloader - A Dialog to maintain sample files
//
// This  uses a  Qt Dialog  window that  contains a  tree view  of the
// available sample  files on a  web or ftp  server. The files  can be
// installed locally by ticking a check  box or removed from the local
// machine by un-ticking the check boxes.
//
// The class requires a pointer to an open QSettings instance where it
// will save its persistent state, a pointer to a WSJT-X Configuration
// instance that is used to  obtain configuration information like the
// current    file   save    location    and,   a    pointer   to    a
// QNetworkAccessManager instance which is used for network requests.
//
// An instance  of SampleDownloader need  not be destroyed  after use,
// just  call  SampleDownloader::show()  to make  the  dialog  visible
// again.
//
class SampleDownloader final
  : public QObject
{
  Q_OBJECT;

public:
  SampleDownloader (QSettings * settings
                    , Configuration const *
                    , QNetworkAccessManager *
                    , QWidget * parent = nullptr);
  ~SampleDownloader ();

  Q_SLOT void show ();

private:
  class impl;
  pimpl<impl> m_;
};

// The SourceForge default of releases before 3.3 reads as PROJECT_SAMPLES_URL.
inline QString saved_samples_url (QSettings const& settings)
{
  auto const url = settings.value ("SamplesURL", PROJECT_SAMPLES_URL).toString ();
  return url == "http://downloads.sourceforge.net/project/wsjt/" ? QString {PROJECT_SAMPLES_URL} : url;
}

// Only a custom URL is saved, so releases before 3.3 sharing these settings keep their own default.
inline void save_samples_url (QSettings& settings, QString const& url)
{
  if (url == PROJECT_SAMPLES_URL)
    {
      settings.remove ("SamplesURL");
    }
  else
    {
      settings.setValue ("SamplesURL", url);
    }
}

#endif
