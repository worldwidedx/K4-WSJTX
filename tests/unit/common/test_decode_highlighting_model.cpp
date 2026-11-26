#include <QtTest>

#include <algorithm>
#include <cmath>

#include <QMetaEnum>
#include <QSettings>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "models/DecodeHighlightingModel.hpp"

class TestDecodeHighlightingModel
  : public QObject
{
  Q_OBJECT

  using Highlight = DecodeHighlightingModel::Highlight;
  using HighlightItems = DecodeHighlightingModel::HighlightItems;
  using HighlightColors = DecodeHighlightingModel::HighlightColors;
  using ColorPreset = DecodeHighlightingModel::ColorPreset;

  static HighlightColors const * colors_for (ColorPreset const& preset, Highlight type)
  {
    for (auto const& colors : preset)
      {
        if (colors.type_ == type)
          {
            return &colors;
          }
      }
    return nullptr;
  }

  static qreal relative_luminance (QColor const& color)
  {
    auto linear = [] (int component) {
        auto const value = component / 255.;
        return value <= .04045 ? value / 12.92 : std::pow ((value + .055) / 1.055, 2.4);
      };
    return .2126 * linear (color.red ())
      + .7152 * linear (color.green ())
      + .0722 * linear (color.blue ());
  }

  static qreal contrast_ratio (QColor const& first, QColor const& second)
  {
    auto const first_luminance = relative_luminance (first);
    auto const second_luminance = relative_luminance (second);
    auto const lighter = std::max (first_luminance, second_luminance);
    auto const darker = std::min (first_luminance, second_luminance);
    return (lighter + .05) / (darker + .05);
  }

  template<typename Items>
  static void verify_complete_type_coverage (Items const& items, char const * name)
  {
    auto const meta = QMetaEnum::fromType<Highlight> ();
    QSet<int> types;
    for (auto const& item : items)
      {
        auto const type = static_cast<int> (item.type_);
        auto const message = QString {"%1 contains unknown or duplicate type %2"}.arg (name).arg (type);
        QVERIFY2 (meta.valueToKey (type), qPrintable (message));
        QVERIFY2 (!types.contains (type), qPrintable (message));
        types.insert (type);
      }
    QCOMPARE (types.size (), meta.keyCount ());
    for (int key {0}; key < meta.keyCount (); ++key)
      {
        auto const message = QString {"%1 omits %2"}.arg (name).arg (meta.key (key));
        QVERIFY2 (types.contains (meta.value (key)), qPrintable (message));
      }
  }

private Q_SLOTS:
  void refactored_defaults_preserve_existing_values ()
  {
    HighlightItems const expected_default = {
      {Highlight::MyCall, true, {}, {{0xff, 0x66, 0x66}}}
      , {Highlight::Continent, true, {}, {{0xff, 0x00, 0x63}}}
      , {Highlight::ContinentBand, true, {}, {{0xff, 0x99, 0xc2}}}
      , {Highlight::CQZone, true, {}, {{0xff, 0xbf, 0x00}}}
      , {Highlight::CQZoneBand, true, {}, {{0xff, 0xe4, 0x99}}}
      , {Highlight::ITUZone, false, {}, {{0xa6, 0xff, 0x00}}}
      , {Highlight::ITUZoneBand, false, {}, {{0xdd, 0xff, 0x99}}}
      , {Highlight::DXCC, true, {}, {{0xff, 0x00, 0xff}}}
      , {Highlight::DXCCBand, true, {}, {{0xff, 0xaa, 0xff}}}
      , {Highlight::Grid, false, {}, {{0xff, 0x80, 0x00}}}
      , {Highlight::GridBand, false, {}, {{0xff, 0xcc, 0x99}}}
      , {Highlight::Call, false, {}, {{0x00, 0xff, 0xff}}}
      , {Highlight::CallBand, false, {}, {{0x99, 0xff, 0xff}}}
      , {Highlight::LotW, false, {{0x99, 0x00, 0x00}}, {}}
      , {Highlight::CQ, true, {}, {{0x66, 0xff, 0x66}}}
      , {Highlight::Tx, true, {}, {Qt::yellow}}
    };
    HighlightItems const expected_default2 = {
      {Highlight::LotW, true, {Qt::black}, {}}
      , {Highlight::MyCall, true, {{0xaa, 0x00, 0x00}}, {{0x00, 0xff, 0xff}}}
      , {Highlight::Continent, true, {{0xaa, 0x00, 0x00}}, {{0xff, 0x50, 0xff}}}
      , {Highlight::ContinentBand, true, {{0xaa, 0x00, 0x00}}, {{0xff, 0x82, 0xff}}}
      , {Highlight::DXCC, true, {{0xaa, 0x00, 0x00}}, {Qt::yellow}}
      , {Highlight::DXCCBand, true, {{0xaa, 0x00, 0x00}}, {{0xff, 0xff, 0x9b}}}
      , {Highlight::CQZone, true, {{0xaa, 0x00, 0x00}}, {{0xaa, 0xaa, 0x00}}}
      , {Highlight::CQZoneBand, true, {{0xaa, 0x00, 0x00}}, {{0xaa, 0xaa, 0x7f}}}
      , {Highlight::ITUZone, true, {{0xaa, 0x00, 0x00}}, {{0x37, 0xbd, 0xff}}}
      , {Highlight::ITUZoneBand, true, {{0xaa, 0x00, 0x00}}, {{0x91, 0xde, 0xff}}}
      , {Highlight::Grid, true, {{0xaa, 0x00, 0x00}}, {{0xff, 0xa8, 0x80}}}
      , {Highlight::GridBand, true, {{0xaa, 0x00, 0x00}}, {{0xff, 0xcd, 0x9b}}}
      , {Highlight::Call, true, {{0xaa, 0x00, 0x00}}, {{0x64, 0xff, 0x64}}}
      , {Highlight::CallBand, true, {{0xaa, 0x00, 0x00}}, {{0xb4, 0xff, 0xb4}}}
      , {Highlight::CQ, true, {{0xaa, 0x00, 0x00}}, {{0xc3, 0xc3, 0xc3}}}
      , {Highlight::Tx, true, {Qt::black}, {{0xff, 0xa5, 0xc6}}}
    };

    QVERIFY (DecodeHighlightingModel::default_items () == expected_default);
    QVERIFY (DecodeHighlightingModel::default_items2 () == expected_default2);
  }

  void built_in_presets_are_complete ()
  {
    DecodeHighlightingModel model;

    verify_complete_type_coverage (DecodeHighlightingModel::default_items (), "Default 1 items");
    verify_complete_type_coverage (DecodeHighlightingModel::default_items2 (), "Default 2 items");
    verify_complete_type_coverage (DecodeHighlightingModel::default_color_preset (), "Default 1 preset");
    verify_complete_type_coverage (DecodeHighlightingModel::default2_color_preset (), "Default 2 preset");
    verify_complete_type_coverage (DecodeHighlightingModel::red_green_color_vision_preset (), "Red/green preset");
    verify_complete_type_coverage (DecodeHighlightingModel::blue_yellow_color_vision_preset (), "Blue/yellow preset");
    verify_complete_type_coverage (DecodeHighlightingModel::high_contrast_color_preset (), "High contrast preset");
    verify_complete_type_coverage (DecodeHighlightingModel::dark_shack_color_preset (), "Dark shack preset");
    verify_complete_type_coverage (DecodeHighlightingModel::solarized_color_preset (), "Solarized preset");
    verify_complete_type_coverage (DecodeHighlightingModel::monochrome_color_preset (), "Monochrome preset");

    QCOMPARE (DecodeHighlightingModel::default_color_preset ().size (), model.items ().size ());
    QCOMPARE (DecodeHighlightingModel::default2_color_preset ().size (), model.items ().size ());
    QCOMPARE (DecodeHighlightingModel::red_green_color_vision_preset ().size (), model.items ().size ());
    QCOMPARE (DecodeHighlightingModel::blue_yellow_color_vision_preset ().size (), model.items ().size ());
    QCOMPARE (DecodeHighlightingModel::high_contrast_color_preset ().size (), model.items ().size ());
    QCOMPARE (DecodeHighlightingModel::dark_shack_color_preset ().size (), model.items ().size ());
    QCOMPARE (DecodeHighlightingModel::solarized_color_preset ().size (), model.items ().size ());
    QCOMPARE (DecodeHighlightingModel::monochrome_color_preset ().size (), model.items ().size ());
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::default_color_preset ()));
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::default2_color_preset ()));
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::red_green_color_vision_preset ()));
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::blue_yellow_color_vision_preset ()));
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::high_contrast_color_preset ()));
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::dark_shack_color_preset ()));
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::solarized_color_preset ()));
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::monochrome_color_preset ()));
  }

  void default_presets_match_reset_colors ()
  {
    struct DefaultPair final
    {
      HighlightItems const& items_;
      ColorPreset const& preset_;
    };
    DefaultPair const defaults[] = {
      {DecodeHighlightingModel::default_items (), DecodeHighlightingModel::default_color_preset ()}
      , {DecodeHighlightingModel::default_items2 (), DecodeHighlightingModel::default2_color_preset ()}
    };

    for (auto const& default_pair : defaults)
      {
        for (auto const& item : default_pair.items_)
          {
            auto const colors = colors_for (default_pair.preset_, item.type_);
            QVERIFY (colors);
            QCOMPARE (colors->foreground_, item.foreground_);
            QCOMPARE (colors->background_, item.background_);
          }
      }
  }

  void color_resolution_honors_priority_and_reordering ()
  {
    HighlightItems items {
      {Highlight::MyCall, true, QBrush {Qt::white}, QBrush {Qt::red}}
      , {Highlight::CQ, true, QBrush {Qt::black}, QBrush {Qt::yellow}}
    };
    DecodeHighlightingModel::HighlightTypes matches {Highlight::CQ, Highlight::MyCall};

    auto resolved = DecodeHighlightingModel::resolve_colors (items, matches, Qt::magenta, Qt::cyan);
    QCOMPARE (resolved.type_, Highlight::MyCall);
    QCOMPARE (resolved.background_, QColor {Qt::red});
    QCOMPARE (resolved.foreground_, QColor {Qt::white});

    items.prepend (items.takeLast ());
    resolved = DecodeHighlightingModel::resolve_colors (items, matches, Qt::magenta, Qt::cyan);
    QCOMPARE (resolved.type_, Highlight::CQ);
    QCOMPARE (resolved.background_, QColor {Qt::yellow});
    QCOMPARE (resolved.foreground_, QColor {Qt::black});
  }

  void color_resolution_composes_independent_channels ()
  {
    HighlightItems const items {
      {Highlight::MyCall, true, {}, QBrush {Qt::red}}
      , {Highlight::DXCC, true, QBrush {Qt::white}, {}}
      , {Highlight::CQ, true, QBrush {Qt::black}, QBrush {Qt::yellow}}
    };
    DecodeHighlightingModel::HighlightTypes const matches {
      Highlight::CQ, Highlight::DXCC, Highlight::MyCall
    };

    auto const resolved = DecodeHighlightingModel::resolve_colors (items, matches, Qt::magenta, Qt::cyan);
    QCOMPARE (resolved.type_, Highlight::MyCall);
    QCOMPARE (resolved.background_, QColor {Qt::red});
    QCOMPARE (resolved.foreground_, QColor {Qt::white});
  }

  void color_resolution_ignores_disabled_rules ()
  {
    HighlightItems const items {
      {Highlight::MyCall, false, QBrush {Qt::white}, QBrush {Qt::red}}
      , {Highlight::CQ, true, QBrush {Qt::black}, QBrush {Qt::yellow}}
    };
    DecodeHighlightingModel::HighlightTypes const matches {Highlight::CQ, Highlight::MyCall};

    auto const resolved = DecodeHighlightingModel::resolve_colors (items, matches, Qt::magenta, Qt::cyan);
    QCOMPARE (resolved.type_, Highlight::CQ);
    QCOMPARE (resolved.background_, QColor {Qt::yellow});
    QCOMPARE (resolved.foreground_, QColor {Qt::black});
  }

  void complete_pair_presets_meet_text_contrast ()
  {
    struct NamedPreset final
    {
      char const * name_;
      ColorPreset const& preset_;
    };
    NamedPreset const presets[] = {
      {"red/green", DecodeHighlightingModel::red_green_color_vision_preset ()}
      , {"blue/yellow", DecodeHighlightingModel::blue_yellow_color_vision_preset ()}
      , {"high contrast", DecodeHighlightingModel::high_contrast_color_preset ()}
      , {"dark shack", DecodeHighlightingModel::dark_shack_color_preset ()}
      , {"solarized", DecodeHighlightingModel::solarized_color_preset ()}
      , {"monochrome", DecodeHighlightingModel::monochrome_color_preset ()}
    };

    for (auto const& named_preset : presets)
      {
        for (auto const& colors : named_preset.preset_)
          {
            auto const rule_name = DecodeHighlightingModel::highlight_name (colors.type_);
            auto const unset_message = QString {"%1: %2 must set both foreground and background colors"}
              .arg (named_preset.name_)
              .arg (rule_name);
            QVERIFY2 (Qt::NoBrush != colors.foreground_.style (), qPrintable (unset_message));
            QVERIFY2 (Qt::NoBrush != colors.background_.style (), qPrintable (unset_message));
            auto const ratio = contrast_ratio (colors.foreground_.color (), colors.background_.color ());
            auto const message = QString {"%1: %2 has contrast ratio %3"}
              .arg (named_preset.name_)
              .arg (rule_name)
              .arg (ratio, 0, 'f', 2);
            QVERIFY2 (ratio >= 4.5, qPrintable (message));
          }
      }
  }

  void monochrome_preset_is_achromatic ()
  {
    for (auto const& colors : DecodeHighlightingModel::monochrome_color_preset ())
      {
        QColor const pair[] = {colors.foreground_.color (), colors.background_.color ()};
        for (auto const& color : pair)
          {
            QCOMPARE (color.red (), color.green ());
            QCOMPARE (color.green (), color.blue ());
          }
      }
  }

  void dark_shack_preset_uses_dark_backgrounds ()
  {
    qreal constexpr maximum_background_luminance {.15};
    for (auto const& colors : DecodeHighlightingModel::dark_shack_color_preset ())
      {
        auto const luminance = relative_luminance (colors.background_.color ());
        auto const message = QString {"%1 background luminance %2 exceeds %3"}
          .arg (DecodeHighlightingModel::highlight_name (colors.type_))
          .arg (luminance, 0, 'f', 3)
          .arg (maximum_background_luminance, 0, 'f', 3);
        QVERIFY2 (luminance <= maximum_background_luminance, qPrintable (message));
      }
  }

  void operating_presets_separate_all_band_backgrounds_from_cq ()
  {
    struct AlertPair final
    {
      Highlight all_band_;
      Highlight on_band_;
    };
    AlertPair const alert_pairs[] = {
      {Highlight::Continent, Highlight::ContinentBand}
      , {Highlight::CQZone, Highlight::CQZoneBand}
      , {Highlight::ITUZone, Highlight::ITUZoneBand}
      , {Highlight::DXCC, Highlight::DXCCBand}
      , {Highlight::Grid, Highlight::GridBand}
      , {Highlight::Call, Highlight::CallBand}
    };
    ColorPreset const * presets[] = {
      &DecodeHighlightingModel::dark_shack_color_preset ()
      , &DecodeHighlightingModel::solarized_color_preset ()
      , &DecodeHighlightingModel::monochrome_color_preset ()
    };

    for (auto const preset : presets)
      {
        auto const cq = colors_for (*preset, Highlight::CQ);
        QVERIFY (cq);
        for (auto const& pair : alert_pairs)
          {
            auto const all_band = colors_for (*preset, pair.all_band_);
            auto const on_band = colors_for (*preset, pair.on_band_);
            QVERIFY (all_band);
            QVERIFY (on_band);
            auto const all_band_contrast = contrast_ratio (
              all_band->background_.color (), cq->background_.color ());
            auto const on_band_contrast = contrast_ratio (
              on_band->background_.color (), cq->background_.color ());
            auto const minimum_contrast = on_band_contrast * 1.1;
            auto const message = QString {"%1 background contrast %2 must be at least 10% greater "
                                          "than %3's %4"}
              .arg (DecodeHighlightingModel::highlight_name (pair.all_band_))
              .arg (all_band_contrast, 0, 'f', 2)
              .arg (DecodeHighlightingModel::highlight_name (pair.on_band_))
              .arg (on_band_contrast, 0, 'f', 2);
            QVERIFY2 (all_band_contrast >= minimum_contrast, qPrintable (message));
          }
      }
  }

  void preset_changes_only_colors ()
  {
    DecodeHighlightingModel model;
    auto custom = DecodeHighlightingModel::default_items ();
    custom.append (custom.takeFirst ());
    for (int row {0}; row < custom.size (); ++row)
      {
        custom[row].enabled_ = 0 == row % 2;
        custom[row].foreground_ = QBrush {QColor {row, row + 1, row + 2}};
        custom[row].background_ = QBrush {QColor {row + 3, row + 4, row + 5}};
      }
    model.items (custom);
    auto const before = model.items ();
    QSignalSpy changes {&model, &QAbstractItemModel::dataChanged};
    QSignalSpy resets {&model, &QAbstractItemModel::modelReset};

    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::default2_color_preset ()));

    auto const after = model.items ();
    QCOMPARE (after.size (), before.size ());
    for (int row {0}; row < after.size (); ++row)
      {
        QCOMPARE (after[row].type_, before[row].type_);
        QCOMPARE (after[row].enabled_, before[row].enabled_);
        auto const colors = colors_for (DecodeHighlightingModel::default2_color_preset (), after[row].type_);
        QVERIFY (colors);
        QCOMPARE (after[row].foreground_, colors->foreground_);
        QCOMPARE (after[row].background_, colors->background_);
      }
    QCOMPARE (changes.size (), 1);
    auto const change = changes.takeFirst ();
    QCOMPARE (change.at (0).value<QModelIndex> (), model.index (0, 0));
    QCOMPARE (change.at (1).value<QModelIndex> (), model.index (after.size () - 1, 0));
    auto const roles = change.at (2).value<QVector<int>> ();
    QCOMPARE (roles, QVector<int> ({Qt::DisplayRole, Qt::ForegroundRole, Qt::BackgroundRole}));
    QCOMPARE (resets.size (), 0);
    auto const lotw = std::find_if (after.cbegin (), after.cend (), [] (auto const& item) {
        return item.type_ == Highlight::LotW;
      });
    QVERIFY (lotw != after.cend ());
    auto const lotw_row = static_cast<int> (std::distance (after.cbegin (), lotw));
    auto const& abstract_model = static_cast<QAbstractItemModel const&> (model);
    QVERIFY (abstract_model.data (model.index (lotw_row, 0), Qt::DisplayRole).toString ().contains ("b/g unset"));

    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::default2_color_preset ()));
    QCOMPARE (changes.size (), 0);
  }

  void color_changes_notify_display_role ()
  {
    DecodeHighlightingModel model;
    auto items = model.items ();
    items[0].foreground_ = QBrush {Qt::black};
    items[0].background_ = QBrush {Qt::white};
    model.items (items);
    auto const index = model.index (0, 0);
    auto& abstract_model = static_cast<QAbstractItemModel&> (model);
    QSignalSpy changes {&model, &QAbstractItemModel::dataChanged};

    QVERIFY (abstract_model.setData (index, QBrush {}, Qt::ForegroundRole));
    QCOMPARE (changes.size (), 1);
    QCOMPARE (changes.takeFirst ().at (2).value<QVector<int>> (),
              QVector<int> ({Qt::ForegroundRole, Qt::DisplayRole}));
    QVERIFY (abstract_model.data (index, Qt::DisplayRole).toString ().contains ("f/g unset"));

    QVERIFY (abstract_model.setData (index, QBrush {}, Qt::BackgroundRole));
    QCOMPARE (changes.size (), 1);
    QCOMPARE (changes.takeFirst ().at (2).value<QVector<int>> (),
              QVector<int> ({Qt::BackgroundRole, Qt::DisplayRole}));
    QVERIFY (abstract_model.data (index, Qt::DisplayRole).toString ().contains ("b/g unset"));
  }

  void complete_presets_apply_to_legacy_models ()
  {
    DecodeHighlightingModel model;
    auto legacy = DecodeHighlightingModel::default_items ();
    legacy.erase (std::remove_if (legacy.begin (), legacy.end (), [] (auto const& item) {
        return item.type_ == Highlight::Continent
          || item.type_ == Highlight::ContinentBand
          || item.type_ == Highlight::CQZone
          || item.type_ == Highlight::CQZoneBand
          || item.type_ == Highlight::ITUZone
          || item.type_ == Highlight::ITUZoneBand;
      }), legacy.end ());
    model.items (legacy);

    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::high_contrast_color_preset ()));
    QCOMPARE (model.items ().size (), legacy.size ());
    for (auto const& item : model.items ())
      {
        auto const colors = colors_for (DecodeHighlightingModel::high_contrast_color_preset (), item.type_);
        QVERIFY (colors);
        QCOMPARE (item.foreground_, colors->foreground_);
        QCOMPARE (item.background_, colors->background_);
      }
  }

  void presets_apply_to_duplicate_model_rules ()
  {
    DecodeHighlightingModel model;
    auto malformed = DecodeHighlightingModel::default_items ();
    malformed.last ().type_ = malformed.first ().type_;
    malformed.first ().foreground_ = QBrush {Qt::magenta};
    malformed.last ().foreground_ = QBrush {Qt::cyan};
    model.items (malformed);

    auto const& preset = DecodeHighlightingModel::high_contrast_color_preset ();
    QVERIFY (model.apply_color_preset (preset));

    auto const colors = colors_for (preset, malformed.first ().type_);
    QVERIFY (colors);
    auto const after = model.items ();
    QCOMPARE (after.first ().foreground_, colors->foreground_);
    QCOMPARE (after.first ().background_, colors->background_);
    QCOMPARE (after.last ().foreground_, colors->foreground_);
    QCOMPARE (after.last ().background_, colors->background_);
  }

  void invalid_presets_do_not_modify_model ()
  {
    DecodeHighlightingModel model;
    auto custom = model.items ();
    for (int row {0}; row < custom.size (); ++row)
      {
        custom[row].foreground_ = QBrush {QColor {row + 1, row + 2, row + 3}};
        custom[row].background_ = QBrush {QColor {row + 4, row + 5, row + 6}};
      }
    model.items (custom);
    auto const before = model.items ();
    QSignalSpy changes {&model, &QAbstractItemModel::dataChanged};

    auto incomplete = DecodeHighlightingModel::high_contrast_color_preset ();
    incomplete.removeLast ();
    QVERIFY (!model.apply_color_preset (incomplete));
    QVERIFY (model.items () == before);

    auto duplicate = DecodeHighlightingModel::default_color_preset ();
    duplicate.last ().type_ = duplicate.first ().type_;
    QVERIFY (!model.apply_color_preset (duplicate));
    QVERIFY (model.items () == before);
    QCOMPARE (changes.size (), 0);
  }

  void empty_models_reject_presets ()
  {
    DecodeHighlightingModel model;
    model.items ({});
    QSignalSpy changes {&model, &QAbstractItemModel::dataChanged};

    QVERIFY (!model.apply_color_preset (DecodeHighlightingModel::default_color_preset ()));
    QVERIFY (model.items ().isEmpty ());
    QCOMPARE (changes.size (), 0);
  }

  void preset_colors_round_trip_through_settings ()
  {
    qRegisterMetaTypeStreamOperators<DecodeHighlightingModel::HighlightInfo> ("HighlightInfo");
    qRegisterMetaTypeStreamOperators<HighlightItems> ("HighlightItems");

    DecodeHighlightingModel model;
    auto customized = model.items ();
    customized.append (customized.takeFirst ());
    for (int row {0}; row < customized.size (); ++row)
      {
        customized[row].enabled_ = 0 == row % 2;
      }
    model.items (customized);
    QVERIFY (model.apply_color_preset (DecodeHighlightingModel::solarized_color_preset ()));

    QTemporaryDir directory;
    QVERIFY (directory.isValid ());
    auto const filename = directory.filePath ("wsjtx.ini");
    {
      QSettings settings {filename, QSettings::IniFormat};
      settings.setValue ("DecodeHighlighting", QVariant::fromValue (model.items ()));
      settings.sync ();
      QCOMPARE (settings.status (), QSettings::NoError);
    }

    QSettings settings {filename, QSettings::IniFormat};
    auto const restored = settings.value ("DecodeHighlighting").value<HighlightItems> ();
    QVERIFY (restored == model.items ());
  }
};

QTEST_APPLESS_MAIN (TestDecodeHighlightingModel);

#include "test_decode_highlighting_model.moc"
