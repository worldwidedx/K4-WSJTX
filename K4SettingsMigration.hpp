#ifndef K4_SETTINGS_MIGRATION_HPP
#define K4_SETTINGS_MIGRATION_HPP

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

// QFile::copy refuses to overwrite a destination, including a concurrent import.
// Leave the source intact so an upgrade never removes the previous profile.
inline bool migrate_k4_settings (QString const& legacy, QString const& target)
{
  if (QFileInfo::exists (target) || !QFileInfo::exists (legacy)) return true;
  if (!QDir {}.mkpath (QFileInfo {target}.absolutePath ())) return false;
  return QFile::copy (legacy, target);
}

#endif
