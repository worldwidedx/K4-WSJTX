#include <QtTest>

#include <memory>
#include <QAbstractButton>
#include <QAction>
#include <QDir>
#include <QItemSelectionModel>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>

#include "Configuration.hpp"
#include "MetaDataRegistry.hpp"
#include "widgets/AbstractLogWindow.hpp"
#include "widgets/itoneAndicw.h"

int volatile itone[MAX_NUM_SYMBOLS] {};
float gran () { return 0; }

namespace
{
  class LogWindow final : public AbstractLogWindow
  {
  public:
    LogWindow (QSettings * settings, Configuration const * configuration)
      : AbstractLogWindow {"log", settings, configuration} {}

    int changes {};

  private:
    void log_model_changed (int) override { ++changes; }
  };
}

class TestLogWindowDeletion final : public QObject
{
  Q_OBJECT

  QTemporaryDir directory_;
  QNetworkAccessManager network_;
  std::unique_ptr<QSettings> settings_;
  std::unique_ptr<Configuration> configuration_;
  QSqlDatabase database_;
  std::unique_ptr<QSqlTableModel> model_;
  std::unique_ptr<LogWindow> window_;
  QTableView * view_ {};

  void select_ids (std::initializer_list<int> ids)
  {
    while (model_->canFetchMore (QModelIndex {})) model_->fetchMore (QModelIndex {});
    auto selection = view_->selectionModel ();
    selection->clearSelection ();
    for (int id : ids)
      {
        for (int row = 0; row < view_->model ()->rowCount (); ++row)
          {
            auto index = view_->model ()->index (row, 0);
            if (index.data (Qt::EditRole).toInt () == id)
              selection->select (index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
          }
      }
    QCOMPARE (selection->selectedRows ().size (), static_cast<int> (ids.size ()));
  }

  bool append_contact ()
  {
    model_->setEditStrategy (QSqlTableModel::OnManualSubmit);
    auto record = model_->record ();
    record.setValue ("id", 601);
    record.setValue ("call", "NEW");
    record.setValue ("band", "20m");
    record.setValue ("datetime", "000601");
    bool const result = model_->insertRecord (-1, record) && model_->submitAll ();
    model_->setEditStrategy (QSqlTableModel::OnFieldChange);
    return result;
  }

  bool contains_id (int id)
  {
    QSqlQuery query {database_};
    query.prepare ("SELECT id FROM contacts WHERE id = ?");
    query.addBindValue (id);
    return query.exec () && query.next ();
  }

  template<typename DuringConfirmation>
  void delete_selected (QMessageBox::StandardButton answer, DuringConfirmation during_confirmation)
  {
    QTimer::singleShot (0, this, [=] {
        auto box = qobject_cast<QMessageBox *> (QApplication::activeModalWidget ());
        Q_ASSERT (box);
        during_confirmation ();
        box->button (answer)->click ();
      });
    view_->actions ().first ()->trigger ();
  }

private slots:
  void initTestCase ()
  {
    register_types ();
    QStandardPaths::setTestModeEnabled (true);
    QCoreApplication::setApplicationName ("test_log_window_deletion");
    QVERIFY (directory_.isValid ());
    settings_.reset (new QSettings {directory_.filePath ("settings.ini"), QSettings::IniFormat});
    configuration_.reset (new Configuration {&network_, QDir {directory_.path ()}, settings_.get (), nullptr});
  }

  void init ()
  {
    database_ = QSqlDatabase::addDatabase ("QSQLITE", "log-window-deletion");
    database_.setDatabaseName (":memory:");
    QVERIFY (database_.open ());
    QSqlQuery query {database_};
    QVERIFY (query.exec ("CREATE TABLE contacts (id INTEGER PRIMARY KEY, call TEXT, band TEXT, datetime TEXT)"));
    QVERIFY (database_.transaction ());
    query.prepare ("INSERT INTO contacts VALUES (?, ?, '20m', ?)");
    for (int id = 1; id <= 600; ++id)
      {
        query.bindValue (0, id);
        query.bindValue (1, QString {"CALL%1"}.arg (id));
        query.bindValue (2, QString {"%1"}.arg (id, 6, 10, QChar {'0'}));
        QVERIFY (query.exec ());
      }
    QVERIFY (database_.commit ());
    model_.reset (new QSqlTableModel {nullptr, database_});
    model_->setTable ("contacts");
    model_->setEditStrategy (QSqlTableModel::OnFieldChange);
    QVERIFY (model_->select ());
    QVERIFY (model_->rowCount () < 600);
    window_.reset (new LogWindow {settings_.get (), configuration_.get ()});
    view_ = new QTableView {window_.get ()};
    view_->setModel (model_.get ());
    window_->set_log_view (view_);
  }

  void cleanup ()
  {
    // Finish fetching before draining auto-scroll callbacks, which can otherwise fetch more rows.
    while (model_->canFetchMore (QModelIndex {})) model_->fetchMore (QModelIndex {});
    QCoreApplication::processEvents ();
    window_.reset ();
    model_.reset ();
    database_.close ();
    database_ = QSqlDatabase {};
    QSqlDatabase::removeDatabase ("log-window-deletion");
  }

  void confirmed_ids_survive_refresh ()
  {
    select_ids ({550, 600});
    bool inserted = false;
    int fetched_after_refresh = 600;
    delete_selected (QMessageBox::Yes, [&] {
        inserted = append_contact ();
        fetched_after_refresh = model_->rowCount ();
      });
    QVERIFY (inserted);
    QVERIFY (fetched_after_refresh < 550);
    QVERIFY (!contains_id (550));
    QVERIFY (!contains_id (600));
    QVERIFY (contains_id (601));
    QVERIFY (contains_id (42));
    QCOMPARE (window_->changes, 1);
  }

  void cancel_after_refresh_preserves_contacts ()
  {
    select_ids ({550, 600});
    bool inserted = false;
    delete_selected (QMessageBox::No, [&] { inserted = append_contact (); });
    QVERIFY (inserted);
    QVERIFY (contains_id (550));
    QVERIFY (contains_id (600));
    QVERIFY (contains_id (601));
    QCOMPARE (window_->changes, 0);
  }

  void nonadjacent_ids_survive_sort_change ()
  {
    select_ids ({12, 80, 599});
    delete_selected (QMessageBox::Yes, [&] { model_->sort (3, Qt::DescendingOrder); });
    QVERIFY (!contains_id (12));
    QVERIFY (!contains_id (80));
    QVERIFY (!contains_id (599));
    QVERIFY (contains_id (13));
    QVERIFY (contains_id (79));
    QVERIFY (contains_id (600));
    QCOMPARE (window_->changes, 1);
  }
};

QTEST_MAIN (TestLogWindowDeletion)
#include "test_log_window_deletion.moc"
