#ifndef SUGGESTED_FREQUENCIES_DIALOG_HPP__
#define SUGGESTED_FREQUENCIES_DIALOG_HPP__

#include <QDialog>
#include <QList>
#include <QSet>

#include "models/FrequencyList.hpp"

class Bands;
class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QScrollArea;

class SuggestedFrequenciesDialog final : public QDialog
{
public:
  using Items = FrequencyList_v2_101::FrequencyItems;

  SuggestedFrequenciesDialog (Items const& catalog, Items const& working,
                              Bands const& bands, QSet<QString> new_keys,
                              QWidget * parent = nullptr);

  Items selected_items () const;

private:
  bool eventFilter (QObject * watched, QEvent * event) override;
  void refresh ();

  Items const& catalog_;
  Items const& working_;
  Bands const& bands_;
  QSet<QString> new_keys_;
  QSet<int> selected_;
  QComboBox * band_filter_;
  QComboBox * mode_filter_;
  QCheckBox * show_present_;
  QCheckBox * new_only_;
  QCheckBox * selected_only_;
  QScrollArea * list_;
  QList<QCheckBox *> row_checkboxes_;
  QLabel * empty_label_;
  QPushButton * add_button_;
  QPushButton * cancel_button_;
};

#endif
