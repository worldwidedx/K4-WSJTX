#include <QtTest>
#include <QAccessible>
#include <QCheckBox>
#include <QScrollArea>
#include <QStyle>
#include <QStyleOptionButton>

#include "models/Bands.hpp"
#include "models/FrequencySuggestions.hpp"
#include "widgets/SuggestedFrequenciesDialog.hpp"

namespace
{
  using Item = FrequencyList_v2_101::Item;
  using Items = FrequencyList_v2_101::FrequencyItems;

  Item frequency (Radio::Frequency dial, Modes::Mode mode = Modes::FT8,
                  IARURegions::Region region = IARURegions::ALL,
                  QString description = {}, bool preferred = false)
  {
    return {dial, mode, region, description, {}, {}, {}, preferred};
  }
}

class TestFrequencySuggestions final : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void matchesOperatingIdentity ()
  {
    auto const proposed = frequency (14090000, Modes::FT8, IARURegions::R2);
    auto const customized = frequency (14090000, Modes::FT8, IARURegions::ALL,
                                       "My DX frequency", true);
    QVERIFY (FrequencySuggestions::is_present (proposed, Items {customized}));
    QVERIFY (!FrequencySuggestions::is_present (
      frequency (14091000, Modes::FT8, IARURegions::R2), Items {customized}));
    QVERIFY (!FrequencySuggestions::is_present (
      frequency (14090000, Modes::JTTY, IARURegions::R2), Items {customized}));
    QVERIFY (!FrequencySuggestions::is_present (
      frequency (14090000, Modes::FT8, IARURegions::ALL),
      Items {frequency (14090000, Modes::FT8, IARURegions::R2)}));
    QVERIFY (FrequencySuggestions::is_present (
      proposed, Items {frequency (14090000, Modes::ALL)}));
  }

  void preservesCustomRowsAndPreferences ()
  {
    Bands bands;
    auto const custom = frequency (14074000, Modes::FT8, IARURegions::ALL,
                                   "Personal calling spot", true);
    Items working {custom};
    Items selected {frequency (14074000, Modes::FT8, IARURegions::ALL),
                    frequency (14090000, Modes::FT8, IARURegions::ALL, {}, true),
                    frequency (14090000, Modes::FT8, IARURegions::ALL, {}, true)};

    auto additions = FrequencySuggestions::prepare_additions (selected, working, bands);
    QCOMPARE (additions.size (), 1);
    QCOMPARE (additions.first ().frequency_, Radio::Frequency {14090000});
    QVERIFY (!additions.first ().preferred_);
    QCOMPARE (working.first ().description_, QStringLiteral ("Personal calling spot"));
    QVERIFY (working.first ().preferred_);
  }

  void noticeDoesNotAcknowledgeTemporaryReset ()
  {
    Items previous_catalog {frequency (14074000)};
    Items catalog {frequency (14074000), frequency (14090000)};
    Items saved {frequency (14074000)};
    FrequencySuggestions::NoticeState notice;
    QVERIFY (notice.acknowledge_present_catalog (previous_catalog, saved));
    QCOMPARE (notice.visibility (catalog, saved),
              FrequencySuggestions::NoticeState::Visibility::New);

    auto temporary = catalog;
    QCOMPARE (notice.visibility (catalog, temporary),
              FrequencySuggestions::NoticeState::Visibility::Hidden);
    notice.cancel ();
    QCOMPARE (notice.visibility (catalog, saved),
              FrequencySuggestions::NoticeState::Visibility::New);
    QCOMPARE (notice.seen_keys ().size (), 1);
  }

  void noticeAcknowledgesOnlyCommittedSelections ()
  {
    Items catalog {frequency (14074000), frequency (14090000), frequency (14091000)};
    Items working {frequency (14074000)};
    FrequencySuggestions::NoticeState notice;
    QCOMPARE (notice.visibility (catalog, working),
              FrequencySuggestions::NoticeState::Visibility::Available);
    QVERIFY (!notice.commit (catalog, working));
    notice.selected_additions (Items {frequency (14090000)});
    notice.cancel ();
    QVERIFY (!notice.commit (catalog, working));
    QCOMPARE (notice.visibility (catalog, working),
              FrequencySuggestions::NoticeState::Visibility::Available);

    notice.selected_additions (Items {frequency (14090000)});
    Items partial {frequency (14074000), frequency (14090000)};
    QVERIFY (notice.commit (catalog, partial));
    QCOMPARE (notice.visibility (catalog, partial),
              FrequencySuggestions::NoticeState::Visibility::Hidden);
    QCOMPARE (notice.seen_keys ().size (), 3);
  }

  void noticeDoesNotAcknowledgeRemovedSelection ()
  {
    Items catalog {frequency (14074000), frequency (14090000)};
    Items saved {frequency (14074000)};
    FrequencySuggestions::NoticeState notice;
    notice.selected_additions (Items {frequency (14090000)});
    QVERIFY (!notice.commit (catalog, saved));
    QCOMPARE (notice.visibility (catalog, saved),
              FrequencySuggestions::NoticeState::Visibility::Available);
  }

  void dismissPersistsIndependentlyOfSettingsCancel ()
  {
    Items catalog {frequency (14074000), frequency (14090000)};
    Items working {frequency (14074000)};
    FrequencySuggestions::NoticeState notice;
    notice.dismiss (catalog);
    notice.cancel ();
    QCOMPARE (notice.visibility (catalog, working),
              FrequencySuggestions::NoticeState::Visibility::Hidden);
    catalog.append (frequency (14091000));
    QCOMPARE (notice.visibility (catalog, working),
              FrequencySuggestions::NoticeState::Visibility::New);
  }

  void chooserAddsOnlyCheckedRows ()
  {
    Bands bands;
    Items catalog {frequency (14074000), frequency (14090000)};
    Items working {frequency (14074000, Modes::FT8, IARURegions::ALL,
                             "My row")};
    SuggestedFrequenciesDialog dialog {catalog, working, bands, {}};
    auto * list = dialog.findChild<QScrollArea *> (QStringLiteral ("suggested_frequencies_list"));
    QVERIFY (list);
    auto checkboxes = list->findChildren<QCheckBox *> ();
    QCOMPARE (checkboxes.size (), 1);
    auto * checkbox = checkboxes.first ();
    QVERIFY (dialog.selected_items ().isEmpty ());

    dialog.show ();
    checkbox->setFocus ();
    QTest::keyClick (checkbox, Qt::Key_Space);
    QCOMPARE (dialog.selected_items ().size (), 1);
    QCOMPARE (dialog.selected_items ().first ().frequency_, Radio::Frequency {14090000});
    QCOMPARE (working.size (), 1);

    auto * accessible = QAccessible::queryAccessibleInterface (checkbox);
    QVERIFY (accessible);
    QCOMPARE (accessible->role (), QAccessible::CheckBox);
    auto * action = accessible->actionInterface ();
    QVERIFY (action);
    QVERIFY (action->actionNames ().contains (QAccessibleActionInterface::toggleAction ()));
    action->doAction (QAccessibleActionInterface::toggleAction ());
    QVERIFY (dialog.selected_items ().isEmpty ());
    action->doAction (QAccessibleActionInterface::toggleAction ());
    QCOMPARE (dialog.selected_items ().size (), 1);
    QStyleOptionButton option;
    option.initFrom (checkbox);
    auto const indicator = checkbox->style ()->subElementRect (
      QStyle::SE_CheckBoxIndicator, &option, checkbox);
    QVERIFY (indicator.isValid ());
    QTest::mouseClick (checkbox, Qt::LeftButton, Qt::NoModifier, indicator.center ());
    QVERIFY (dialog.selected_items ().isEmpty ());
    QTest::mouseClick (checkbox, Qt::LeftButton, Qt::NoModifier, indicator.center ());
    QCOMPARE (dialog.selected_items ().size (), 1);

    QTest::keyClick (checkbox, Qt::Key_Tab);
    QVERIFY (dialog.focusWidget ());
    QCOMPARE (dialog.focusWidget ()->property ("text").toString (), QStringLiteral ("Cancel"));
    QTest::keyClick (dialog.focusWidget (), Qt::Key_Tab);
    QVERIFY (dialog.focusWidget ());
    QCOMPARE (dialog.focusWidget ()->property ("text").toString (), QStringLiteral ("Add selected (1)"));
    QTest::keyClick (dialog.focusWidget (), Qt::Key_Backtab);
    QVERIFY (dialog.focusWidget ());
    QCOMPARE (dialog.focusWidget ()->property ("text").toString (), QStringLiteral ("Cancel"));
    checkbox->setFocus ();
    QTest::keyClick (checkbox, Qt::Key_Backtab);
    QVERIFY (dialog.focusWidget ());
    QCOMPARE (dialog.focusWidget ()->property ("text").toString (),
              QStringLiteral ("Show selected only"));

    auto * selected_only = dialog.focusWidget ();
    QTest::mouseClick (selected_only, Qt::LeftButton);
    checkboxes = list->findChildren<QCheckBox *> ();
    QCOMPARE (checkboxes.size (), 1);
    checkboxes.first ()->setChecked (false);
    QCoreApplication::processEvents ();
    QVERIFY (!selected_only->property ("checked").toBool ());
    QVERIFY (dialog.selected_items ().isEmpty ());
    QCOMPARE (list->findChildren<QCheckBox *> ().size (), 1);
  }
};

QTEST_MAIN (TestFrequencySuggestions)
#include "test_frequency_suggestions.moc"
