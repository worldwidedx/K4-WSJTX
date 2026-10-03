#include "K4SettingsMigration.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <iostream>

static bool write (QString const& path, QByteArray const& data)
{
  QFile file {path};
  return file.open (QIODevice::WriteOnly) && file.write (data) == data.size ();
}
static QByteArray read (QString const& path)
{
  QFile file {path};
  if (!file.open (QIODevice::ReadOnly)) return {};
  return file.readAll ();
}
int main (int argc, char **argv)
{
  QCoreApplication app {argc, argv};
  QTemporaryDir root;
  if (!root.isValid ()) return 1;
  auto const legacy = root.filePath ("K4 WSJT-X.ini");
  auto const target = root.filePath ("WSJT-X - K4/WSJT-X - K4.ini");
  QByteArray const settings {"[Configuration]\nUDPServer=127.0.0.1\nUDPServerPort=2237\n[OtherProfile]\nmarker=preserved\n"};
  if (!migrate_k4_settings (legacy, target) || QFileInfo::exists (target)) return 2;
  if (!write (legacy, settings) || !migrate_k4_settings (legacy, target)) return 3;
  if (read (legacy) != settings || read (target) != settings) return 4;
  QByteArray const existing {"existing named profile"};
  if (!write (target, existing) || !migrate_k4_settings (legacy, target)) return 5;
  if (read (target) != existing || read (legacy) != settings) return 6;
  auto const blocker = root.filePath ("blocked");
  if (!write (blocker, "file")) return 7;
  if (migrate_k4_settings (legacy, blocker + "/profile.ini")) return 8;
  if (read (legacy) != settings) return 9;
  std::cout << "K4 migration: absent source, full INI copy, existing profile, and failure preservation passed\n";
  return 0;
}
