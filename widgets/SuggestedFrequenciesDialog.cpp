#include "widgets/SuggestedFrequenciesDialog.hpp"

#include <algorithm>
#include <utility>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

#include "Radio.hpp"
#include "models/Bands.hpp"
#include "models/FrequencySuggestions.hpp"

SuggestedFrequenciesDialog::SuggestedFrequenciesDialog (
  Items const& catalog, Items const& working, Bands const& bands,
  QSet<QString> new_keys, QWidget * parent)
  : QDialog {parent}
  , catalog_ {catalog}
  , working_ {working}
  , bands_ {bands}
  , new_keys_ {std::move (new_keys)}
  , band_filter_ {new QComboBox {this}}
  , mode_filter_ {new QComboBox {this}}
  , show_present_ {new QCheckBox {tr ("Show frequencies already in my list"), this}}
  , new_only_ {new QCheckBox {tr ("New suggestions only"), this}}
  , selected_only_ {new QCheckBox {tr ("Show selected only"), this}}
  , list_ {new QScrollArea {this}}
  , empty_label_ {new QLabel {this}}
{
  setWindowTitle (tr ("Add suggested frequencies"));
  resize (760, 480);

  auto * layout = new QVBoxLayout {this};
  layout->addWidget (new QLabel {tr ("Select the frequencies to add to your working list."), this});

  auto * filters = new QHBoxLayout;
  band_filter_->addItem (tr ("All bands"));
  mode_filter_->addItem (tr ("All modes"));
  band_filter_->setAccessibleName (tr ("Filter suggested frequencies by band"));
  mode_filter_->setAccessibleName (tr ("Filter suggested frequencies by mode"));
  QSet<QString> bands_seen;
  QSet<int> modes_seen;
  for (auto const& item : catalog_)
    {
      bands_seen.insert (bands_.find (item.frequency_));
      if (item.mode_ != Modes::ALL) modes_seen.insert (item.mode_);
    }
  auto band_names = bands_seen.values ();
  band_names.sort ();
  for (auto const& band : band_names)
    band_filter_->addItem (band);
  QList<int> modes = modes_seen.values ();
  std::sort (modes.begin (), modes.end (), [] (int lhs, int rhs) {
    return QString::compare (QString::fromLatin1 (Modes::name (static_cast<Modes::Mode> (lhs))),
                             QString::fromLatin1 (Modes::name (static_cast<Modes::Mode> (rhs))),
                             Qt::CaseInsensitive) < 0;
  });
  for (auto mode : modes)
    mode_filter_->addItem (Modes::name (static_cast<Modes::Mode> (mode)), mode);

  filters->addWidget (new QLabel {tr ("Band:"), this});
  filters->addWidget (band_filter_);
  filters->addStretch ();
  filters->addWidget (new QLabel {tr ("Mode:"), this});
  filters->addWidget (mode_filter_);
  layout->addLayout (filters);
  layout->addWidget (show_present_);
  new_only_->setVisible (!new_keys_.isEmpty ());
  layout->addWidget (new_only_);
  selected_only_->setEnabled (false);
  layout->addWidget (selected_only_);

  list_->setAccessibleName (tr ("Suggested frequencies"));
  list_->setAccessibleDescription (tr ("Check individual frequencies to add them to your working list."));
  list_->setObjectName (QStringLiteral ("suggested_frequencies_list"));
  list_->setWidgetResizable (true);
  list_->setFocusPolicy (Qt::NoFocus);
  layout->addWidget (list_);
  empty_label_->setWordWrap (true);
  layout->addWidget (empty_label_);

  auto * buttons = new QDialogButtonBox {QDialogButtonBox::Cancel, this};
  cancel_button_ = buttons->button (QDialogButtonBox::Cancel);
  add_button_ = buttons->addButton (tr ("Add selected"), QDialogButtonBox::AcceptRole);
  cancel_button_->setFocusPolicy (Qt::StrongFocus);
  add_button_->setFocusPolicy (Qt::StrongFocus);
  cancel_button_->installEventFilter (this);
  add_button_->installEventFilter (this);
  add_button_->setEnabled (false);
  layout->addWidget (buttons);

  connect (band_filter_, QOverload<int>::of (&QComboBox::currentIndexChanged),
           this, [this] { refresh (); });
  connect (mode_filter_, QOverload<int>::of (&QComboBox::currentIndexChanged),
           this, [this] { refresh (); });
  connect (show_present_, &QCheckBox::toggled, this, [this] { refresh (); });
  connect (new_only_, &QCheckBox::toggled, this, [this] { refresh (); });
  connect (selected_only_, &QCheckBox::toggled, this, [this] { refresh (); });
  connect (buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect (buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

  refresh ();
}

bool SuggestedFrequenciesDialog::eventFilter (QObject * watched, QEvent * event)
{
  if (event->type () == QEvent::KeyPress)
    {
      auto * key = static_cast<QKeyEvent *> (event);
      auto const row = row_checkboxes_.indexOf (qobject_cast<QCheckBox *> (watched));
      if (row >= 0 && key->key () == Qt::Key_Tab)
        {
          if (row + 1 < row_checkboxes_.size ()) row_checkboxes_.at (row + 1)->setFocus (Qt::TabFocusReason);
          else cancel_button_->setFocus (Qt::TabFocusReason);
          return true;
        }
      if (row >= 0 && key->key () == Qt::Key_Backtab)
        {
          if (row > 0) row_checkboxes_.at (row - 1)->setFocus (Qt::BacktabFocusReason);
          else (selected_only_->isEnabled () ? static_cast<QWidget *> (selected_only_)
                                             : static_cast<QWidget *> (show_present_))
                 ->setFocus (Qt::BacktabFocusReason);
          return true;
        }
      if (watched == cancel_button_ && key->key () == Qt::Key_Tab)
        {
          (add_button_->isEnabled () ? static_cast<QWidget *> (add_button_)
                                    : static_cast<QWidget *> (band_filter_))
            ->setFocus (Qt::TabFocusReason);
          return true;
        }
      if (watched == cancel_button_ && key->key () == Qt::Key_Backtab)
        {
          if (!row_checkboxes_.isEmpty ()) row_checkboxes_.last ()->setFocus (Qt::BacktabFocusReason);
          else (selected_only_->isEnabled () ? static_cast<QWidget *> (selected_only_)
                                             : static_cast<QWidget *> (show_present_))
                 ->setFocus (Qt::BacktabFocusReason);
          return true;
        }
      if (watched == add_button_ && key->key () == Qt::Key_Tab)
        {
          band_filter_->setFocus (Qt::TabFocusReason);
          return true;
        }
      if (watched == add_button_ && key->key () == Qt::Key_Backtab)
        {
          cancel_button_->setFocus (Qt::BacktabFocusReason);
          return true;
        }
    }
  return QDialog::eventFilter (watched, event);
}

auto SuggestedFrequenciesDialog::selected_items () const -> Items
{
  Items items;
  for (int index = 0; index < catalog_.size (); ++index)
    if (selected_.contains (index)) items.append (catalog_.at (index));
  return items;
}

void SuggestedFrequenciesDialog::refresh ()
{
  row_checkboxes_.clear ();
  auto * content = new QWidget;
  auto * rows = new QGridLayout {content};
  rows->setColumnStretch (5, 1);
  QString const titles[] = {tr ("Add"), tr ("Band"), tr ("Mode"), tr ("Region"),
                            tr ("Dial frequency"), tr ("Description"), tr ("Status")};
  for (int column = 0; column < 7; ++column)
    rows->addWidget (new QLabel {titles[column], content}, 0, column);
  int row {1};
  for (int index = 0; index < catalog_.size (); ++index)
    {
      auto const& item = catalog_.at (index);
      auto const band = bands_.find (item.frequency_);
      auto const present = FrequencySuggestions::is_present (item, working_);
      auto const is_new = new_keys_.contains (FrequencySuggestions::key (item));
      if (selected_only_->isChecked ())
        {
          if (!selected_.contains (index)) continue;
        }
      else
        {
          if (band_filter_->currentIndex () > 0 && band_filter_->currentText () != band) continue;
          if (mode_filter_->currentIndex () > 0
              && mode_filter_->currentData ().toInt () != item.mode_) continue;
          if (present && !show_present_->isChecked ()) continue;
          if (new_only_->isChecked () && !is_new) continue;
        }

      auto * checkbox = new QCheckBox {content};
      checkbox->setAccessibleName (tr ("Add %1 MHz, %2, %3")
                                     .arg (Radio::pretty_frequency_MHz_string (item.frequency_),
                                           QString::fromLatin1 (Modes::name (item.mode_)),
                                           QString::fromLatin1 (IARURegions::name (item.region_))));
      checkbox->setEnabled (!present);
      checkbox->setChecked (selected_.contains (index));
      checkbox->installEventFilter (this);
      connect (checkbox, &QCheckBox::toggled, this, [this, index] (bool checked) {
        if (checked) selected_.insert (index);
        else selected_.remove (index);
        add_button_->setEnabled (!selected_.isEmpty ());
        add_button_->setText (tr ("Add selected (%1)").arg (selected_.size ()));
        auto const refresh_needed = selected_only_->isChecked ();
        if (selected_.isEmpty ())
          {
            QSignalBlocker blocker {selected_only_};
            selected_only_->setChecked (false);
          }
        selected_only_->setEnabled (!selected_.isEmpty ());
        if (refresh_needed) QTimer::singleShot (0, this, [this] { refresh (); });
      });
      rows->addWidget (checkbox, row, 0);
      row_checkboxes_.append (checkbox);

      QString const values[] = {
        band,
        QString::fromLatin1 (Modes::name (item.mode_)),
        QString::fromLatin1 (IARURegions::name (item.region_)),
        Radio::pretty_frequency_MHz_string (item.frequency_) + tr (" MHz"),
        item.description_,
        present ? tr ("Already in your working list")
                : (is_new ? tr ("New") : QString {})
      };
      for (int column = 1; column < 7; ++column)
        {
          auto * label = new QLabel {values[column - 1], content};
          label->setTextInteractionFlags (Qt::NoTextInteraction);
          rows->addWidget (label, row, column);
        }
      ++row;
    }
  rows->setRowStretch (row, 1);
  list_->setWidget (content);
  empty_label_->setText (show_present_->isChecked () || new_only_->isChecked ()
                         || selected_only_->isChecked ()
                         || band_filter_->currentIndex () > 0 || mode_filter_->currentIndex () > 0
                           ? tr ("No suggested frequencies match these filters.")
                           : tr ("Your working list already contains every shipped frequency suggestion."));
  empty_label_->setVisible (row == 1);
}
