#include "DecodeHighlightingModel.hpp"

#include <algorithm>

#include <QString>
#include <QVariant>
#include <QList>
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QMap>
#include <QHash>
#include <QVector>
#include <QTextStream>
#include <QDataStream>
#include <QMetaType>
#include <QDebug>

#include "pimpl_impl.hpp"

#include "moc_DecodeHighlightingModel.cpp"

namespace
{
  using Highlight = DecodeHighlightingModel::Highlight;
  using HighlightItems = DecodeHighlightingModel::HighlightItems;
  using ColorPreset = DecodeHighlightingModel::ColorPreset;

  struct HighlightBehavior final
  {
    Highlight type_;
    bool enabled_;
  };
  using HighlightBehaviorItems = QList<HighlightBehavior>;

  HighlightItems make_highlight_items (HighlightBehaviorItems const& behavior, ColorPreset const& colors)
  {
    HighlightItems items;
    for (auto const& behavior_item : behavior)
      {
        auto const color = std::find_if (colors.cbegin (), colors.cend (), [&behavior_item] (auto const& candidate) {
            return candidate.type_ == behavior_item.type_;
          });
        Q_ASSERT (color != colors.cend ());
        if (color != colors.cend ())
          {
            items.append ({behavior_item.type_, behavior_item.enabled_, color->foreground_, color->background_});
          }
      }
    return items;
  }

  HighlightBehaviorItems const default_behavior = {
    {Highlight::MyCall, true}
    , {Highlight::Continent, true}
    , {Highlight::ContinentBand, true}
    , {Highlight::CQZone, true}
    , {Highlight::CQZoneBand, true}
    , {Highlight::ITUZone, false}
    , {Highlight::ITUZoneBand, false}
    , {Highlight::DXCC, true}
    , {Highlight::DXCCBand, true}
    , {Highlight::Grid, false}
    , {Highlight::GridBand, false}
    , {Highlight::Call, false}
    , {Highlight::CallBand, false}
    , {Highlight::LotW, false}
    , {Highlight::CQ, true}
    , {Highlight::Tx, true}
  };

  HighlightBehaviorItems const default2_behavior = {
    {Highlight::LotW, true}
    , {Highlight::MyCall, true}
    , {Highlight::Continent, true}
    , {Highlight::ContinentBand, true}
    , {Highlight::DXCC, true}
    , {Highlight::DXCCBand, true}
    , {Highlight::CQZone, true}
    , {Highlight::CQZoneBand, true}
    , {Highlight::ITUZone, true}
    , {Highlight::ITUZoneBand, true}
    , {Highlight::Grid, true}
    , {Highlight::GridBand, true}
    , {Highlight::Call, true}
    , {Highlight::CallBand, true}
    , {Highlight::CQ, true}
    , {Highlight::Tx, true}
  };
}

class DecodeHighlightingModel::impl final
{
public:
  explicit impl ()
    : data_ {defaults_}
  {
  }

  HighlightItems static const defaults_;
  HighlightItems static const defaults2_;
  ColorPreset static const default_colors_;
  ColorPreset static const default2_colors_;
  ColorPreset static const red_green_color_vision_colors_;
  ColorPreset static const blue_yellow_color_vision_colors_;
  ColorPreset static const high_contrast_colors_;
  ColorPreset static const dark_shack_colors_;
  ColorPreset static const solarized_colors_;
  ColorPreset static const monochrome_colors_;
  HighlightItems data_;
  QFont font_;
};

DecodeHighlightingModel::ColorPreset const DecodeHighlightingModel::impl::default_colors_ = {
  {Highlight::MyCall, {}, {{0xff, 0x66, 0x66}}}
  , {Highlight::Continent, {}, {{0xff, 0x00, 0x63}}}
  , {Highlight::ContinentBand, {}, {{0xff, 0x99, 0xc2}}}
  , {Highlight::CQZone, {}, {{0xff, 0xbf, 0x00}}}
  , {Highlight::CQZoneBand, {}, {{0xff, 0xe4, 0x99}}}
  , {Highlight::ITUZone, {}, {{0xa6, 0xff, 0x00}}}
  , {Highlight::ITUZoneBand, {}, {{0xdd, 0xff, 0x99}}}
  , {Highlight::DXCC, {}, {{0xff, 0x00, 0xff}}}
  , {Highlight::DXCCBand, {}, {{0xff, 0xaa, 0xff}}}
  , {Highlight::Grid, {}, {{0xff, 0x80, 0x00}}}
  , {Highlight::GridBand, {}, {{0xff, 0xcc, 0x99}}}
  , {Highlight::Call, {}, {{0x00, 0xff, 0xff}}}
  , {Highlight::CallBand, {}, {{0x99, 0xff, 0xff}}}
  , {Highlight::LotW, {{0x99, 0x00, 0x00}}, {}}
  , {Highlight::CQ, {}, {{0x66, 0xff, 0x66}}}
  , {Highlight::Tx, {}, {Qt::yellow}}
};

DecodeHighlightingModel::ColorPreset const DecodeHighlightingModel::impl::default2_colors_ = {
  {Highlight::LotW, {Qt::black}, {}}
  , {Highlight::MyCall, {{0xaa, 0x00, 0x00}}, {{0x00, 0xff, 0xff}}}
  , {Highlight::Continent, {{0xaa, 0x00, 0x00}}, {{0xff, 0x50, 0xff}}}
  , {Highlight::ContinentBand, {{0xaa, 0x00, 0x00}}, {{0xff, 0x82, 0xff}}}
  , {Highlight::DXCC, {{0xaa, 0x00, 0x00}}, {Qt::yellow}}
  , {Highlight::DXCCBand, {{0xaa, 0x00, 0x00}}, {{0xff, 0xff, 0x9b}}}
  , {Highlight::CQZone, {{0xaa, 0x00, 0x00}}, {{0xaa, 0xaa, 0x00}}}
  , {Highlight::CQZoneBand, {{0xaa, 0x00, 0x00}}, {{0xaa, 0xaa, 0x7f}}}
  , {Highlight::ITUZone, {{0xaa, 0x00, 0x00}}, {{0x37, 0xbd, 0xff}}}
  , {Highlight::ITUZoneBand, {{0xaa, 0x00, 0x00}}, {{0x91, 0xde, 0xff}}}
  , {Highlight::Grid, {{0xaa, 0x00, 0x00}}, {{0xff, 0xa8, 0x80}}}
  , {Highlight::GridBand, {{0xaa, 0x00, 0x00}}, {{0xff, 0xcd, 0x9b}}}
  , {Highlight::Call, {{0xaa, 0x00, 0x00}}, {{0x64, 0xff, 0x64}}}
  , {Highlight::CallBand, {{0xaa, 0x00, 0x00}}, {{0xb4, 0xff, 0xb4}}}
  , {Highlight::CQ, {{0xaa, 0x00, 0x00}}, {{0xc3, 0xc3, 0xc3}}}
  , {Highlight::Tx, {Qt::black}, {{0xff, 0xa5, 0xc6}}}
};

DecodeHighlightingModel::ColorPreset const DecodeHighlightingModel::impl::red_green_color_vision_colors_ = {
  {Highlight::MyCall, {Qt::black}, {{0xd5, 0x5e, 0x00}}}
  , {Highlight::Continent, {Qt::white}, {{0x00, 0x72, 0xb2}}}
  , {Highlight::ContinentBand, {Qt::black}, {{0xa8, 0xcc, 0xe3}}}
  , {Highlight::CQZone, {Qt::black}, {{0xe6, 0x9f, 0x00}}}
  , {Highlight::CQZoneBand, {Qt::black}, {{0xf5, 0xd9, 0x99}}}
  , {Highlight::ITUZone, {Qt::black}, {{0x56, 0xb4, 0xe9}}}
  , {Highlight::ITUZoneBand, {Qt::black}, {{0xbe, 0xe3, 0xf5}}}
  , {Highlight::DXCC, {Qt::black}, {{0xcc, 0x79, 0xa7}}}
  , {Highlight::DXCCBand, {Qt::black}, {{0xe8, 0xbf, 0xd6}}}
  , {Highlight::Grid, {Qt::white}, {Qt::black}}
  , {Highlight::GridBand, {Qt::black}, {{0xcc, 0xcc, 0xcc}}}
  , {Highlight::Call, {Qt::black}, {{0x99, 0x99, 0x99}}}
  , {Highlight::CallBand, {Qt::black}, {{0xdd, 0xdd, 0xdd}}}
  , {Highlight::LotW, {{0x00, 0x55, 0x44}}, {Qt::white}}
  , {Highlight::CQ, {Qt::black}, {{0x00, 0x9e, 0x73}}}
  , {Highlight::Tx, {Qt::black}, {{0xf0, 0xe4, 0x42}}}
};

DecodeHighlightingModel::ColorPreset const DecodeHighlightingModel::impl::blue_yellow_color_vision_colors_ = {
  {Highlight::MyCall, {Qt::white}, {{0xe4, 0x1a, 0x1c}}}
  , {Highlight::Continent, {Qt::black}, {{0xf7, 0x81, 0xbf}}}
  , {Highlight::ContinentBand, {Qt::black}, {{0xfc, 0xe0, 0xef}}}
  , {Highlight::CQZone, {Qt::white}, {{0xa6, 0x56, 0x28}}}
  , {Highlight::CQZoneBand, {Qt::black}, {{0xe3, 0xc6, 0xb3}}}
  , {Highlight::ITUZone, {Qt::white}, {{0xb0, 0x30, 0x60}}}
  , {Highlight::ITUZoneBand, {Qt::black}, {{0xe8, 0xb9, 0xc7}}}
  , {Highlight::DXCC, {Qt::white}, {{0x98, 0x4e, 0xa3}}}
  , {Highlight::DXCCBand, {Qt::black}, {{0xe0, 0xc6, 0xe6}}}
  , {Highlight::Grid, {Qt::black}, {{0x99, 0x99, 0x99}}}
  , {Highlight::GridBand, {Qt::black}, {{0xdd, 0xdd, 0xdd}}}
  , {Highlight::Call, {Qt::black}, {{0xfd, 0xbf, 0x6f}}}
  , {Highlight::CallBand, {Qt::black}, {{0xfd, 0xe9, 0xd0}}}
  , {Highlight::LotW, {{0x6e, 0x14, 0x23}}, {Qt::white}}
  , {Highlight::CQ, {Qt::black}, {{0x4d, 0xaf, 0x4a}}}
  , {Highlight::Tx, {Qt::black}, {{0xff, 0x7f, 0x00}}}
};

DecodeHighlightingModel::ColorPreset const DecodeHighlightingModel::impl::high_contrast_colors_ = {
  {Highlight::MyCall, {Qt::black}, {{0xff, 0x00, 0x00}}}
  , {Highlight::Continent, {Qt::black}, {{0xff, 0x80, 0x00}}}
  , {Highlight::ContinentBand, {Qt::white}, {{0x99, 0x50, 0x00}}}
  , {Highlight::CQZone, {Qt::white}, {{0x00, 0x40, 0xff}}}
  , {Highlight::CQZoneBand, {Qt::white}, {{0x00, 0x29, 0x66}}}
  , {Highlight::ITUZone, {Qt::white}, {{0x00, 0x80, 0x80}}}
  , {Highlight::ITUZoneBand, {Qt::white}, {{0x00, 0x4d, 0x4d}}}
  , {Highlight::DXCC, {Qt::white}, {{0xcc, 0x00, 0xcc}}}
  , {Highlight::DXCCBand, {Qt::white}, {{0x7a, 0x00, 0x7a}}}
  , {Highlight::Grid, {Qt::white}, {{0x66, 0x66, 0x66}}}
  , {Highlight::GridBand, {Qt::white}, {{0x33, 0x33, 0x33}}}
  , {Highlight::Call, {Qt::white}, {{0xd6, 0x00, 0x6d}}}
  , {Highlight::CallBand, {Qt::white}, {{0x80, 0x00, 0x41}}}
  , {Highlight::LotW, {{0x00, 0x1a, 0x66}}, {Qt::white}}
  , {Highlight::CQ, {Qt::black}, {{0x00, 0xa0, 0x00}}}
  , {Highlight::Tx, {Qt::black}, {{0xff, 0xff, 0x00}}}
};

DecodeHighlightingModel::ColorPreset const DecodeHighlightingModel::impl::dark_shack_colors_ = {
  {Highlight::MyCall, {{0xff, 0xf6, 0xf2}}, {{0x8a, 0x24, 0x32}}}
  , {Highlight::Continent, {{0xf2, 0xee, 0xe3}}, {{0x7c, 0x5a, 0x10}}}
  , {Highlight::ContinentBand, {{0xd9, 0xd4, 0xc8}}, {{0x4a, 0x3d, 0x1f}}}
  , {Highlight::CQZone, {{0xf2, 0xee, 0xe3}}, {{0x1d, 0x60, 0x78}}}
  , {Highlight::CQZoneBand, {{0xd5, 0xdd, 0xe0}}, {{0x25, 0x3f, 0x49}}}
  , {Highlight::ITUZone, {{0xf2, 0xee, 0xe3}}, {{0x1f, 0x66, 0x5e}}}
  , {Highlight::ITUZoneBand, {{0xd3, 0xdd, 0xda}}, {{0x29, 0x43, 0x3f}}}
  , {Highlight::DXCC, {{0xf8, 0xee, 0xf9}}, {{0x6d, 0x34, 0x7d}}}
  , {Highlight::DXCCBand, {{0xdd, 0xd1, 0xdf}}, {{0x49, 0x30, 0x4f}}}
  , {Highlight::Grid, {{0xf7, 0xee, 0xe5}}, {{0x7a, 0x46, 0x1c}}}
  , {Highlight::GridBand, {{0xdb, 0xd2, 0xc9}}, {{0x4a, 0x38, 0x28}}}
  , {Highlight::Call, {{0xe7, 0xf0, 0xe3}}, {{0x3e, 0x5d, 0x37}}}
  , {Highlight::CallBand, {{0xd1, 0xda, 0xcd}}, {{0x30, 0x3f, 0x2f}}}
  , {Highlight::LotW, {{0xb9, 0xd6, 0xdc}}, {{0x17, 0x31, 0x3a}}}
  , {Highlight::CQ, {{0xc1, 0xdf, 0xc7}}, {{0x17, 0x33, 0x23}}}
  , {Highlight::Tx, {{0xff, 0xf0, 0xaa}}, {{0x5f, 0x4a, 0x10}}}
};

DecodeHighlightingModel::ColorPreset const DecodeHighlightingModel::impl::solarized_colors_ = {
  {Highlight::MyCall, {{0xfd, 0xf6, 0xe3}}, {{0xc4, 0x2d, 0x2a}}}
  , {Highlight::Continent, {{0x00, 0x2b, 0x36}}, {{0xb5, 0x89, 0x00}}}
  , {Highlight::ContinentBand, {{0xee, 0xe8, 0xd5}}, {{0x5c, 0x49, 0x0f}}}
  , {Highlight::CQZone, {{0x00, 0x2b, 0x36}}, {{0x2a, 0xa1, 0x98}}}
  , {Highlight::CQZoneBand, {{0xee, 0xe8, 0xd5}}, {{0x15, 0x56, 0x50}}}
  , {Highlight::ITUZone, {{0x00, 0x2b, 0x36}}, {{0x85, 0x99, 0x00}}}
  , {Highlight::ITUZoneBand, {{0xee, 0xe8, 0xd5}}, {{0x46, 0x52, 0x0a}}}
  , {Highlight::DXCC, {{0xfd, 0xf6, 0xe3}}, {{0xbd, 0x2f, 0x75}}}
  , {Highlight::DXCCBand, {{0xee, 0xe8, 0xd5}}, {{0x6f, 0x24, 0x49}}}
  , {Highlight::Grid, {{0x00, 0x2b, 0x36}}, {{0x3a, 0x9e, 0xe0}}}
  , {Highlight::GridBand, {{0xee, 0xe8, 0xd5}}, {{0x16, 0x57, 0x7c}}}
  , {Highlight::Call, {{0xfd, 0xf6, 0xe3}}, {{0x5d, 0x62, 0xad}}}
  , {Highlight::CallBand, {{0xee, 0xe8, 0xd5}}, {{0x3f, 0x42, 0x79}}}
  , {Highlight::LotW, {{0x2a, 0xa1, 0x98}}, {{0x00, 0x2b, 0x36}}}
  , {Highlight::CQ, {{0x93, 0xa1, 0xa1}}, {{0x07, 0x36, 0x42}}}
  , {Highlight::Tx, {{0xfd, 0xf6, 0xe3}}, {{0xb8, 0x43, 0x14}}}
};

DecodeHighlightingModel::ColorPreset const DecodeHighlightingModel::impl::monochrome_colors_ = {
  {Highlight::MyCall, {Qt::white}, {Qt::black}}
  , {Highlight::Continent, {Qt::black}, {{0xf5, 0xf5, 0xf5}}}
  , {Highlight::ContinentBand, {Qt::black}, {{0xc8, 0xc8, 0xc8}}}
  , {Highlight::CQZone, {Qt::white}, {{0x1f, 0x1f, 0x1f}}}
  , {Highlight::CQZoneBand, {Qt::white}, {{0x55, 0x55, 0x55}}}
  , {Highlight::ITUZone, {Qt::black}, {{0xde, 0xde, 0xde}}}
  , {Highlight::ITUZoneBand, {Qt::black}, {{0xa8, 0xa8, 0xa8}}}
  , {Highlight::DXCC, {Qt::white}, {{0x17, 0x17, 0x17}}}
  , {Highlight::DXCCBand, {Qt::white}, {{0x45, 0x45, 0x45}}}
  , {Highlight::Grid, {Qt::black}, {{0xd0, 0xd0, 0xd0}}}
  , {Highlight::GridBand, {Qt::white}, {{0x58, 0x58, 0x58}}}
  , {Highlight::Call, {Qt::black}, {{0xb8, 0xb8, 0xb8}}}
  , {Highlight::CallBand, {Qt::white}, {{0x66, 0x66, 0x66}}}
  , {Highlight::LotW, {{0xd0, 0xd0, 0xd0}}, {{0x33, 0x33, 0x33}}}
  , {Highlight::CQ, {{0xc0, 0xc0, 0xc0}}, {{0x3f, 0x3f, 0x3f}}}
  , {Highlight::Tx, {{0x11, 0x11, 0x11}}, {{0xef, 0xef, 0xef}}}
};

DecodeHighlightingModel::HighlightItems const DecodeHighlightingModel::impl::defaults_ =
  make_highlight_items (default_behavior, default_colors_);

DecodeHighlightingModel::HighlightItems const DecodeHighlightingModel::impl::defaults2_ =
  make_highlight_items (default2_behavior, default2_colors_);

bool operator == (DecodeHighlightingModel::HighlightInfo const& lhs, DecodeHighlightingModel::HighlightInfo const& rhs)
{
  return lhs.type_ == rhs.type_
    && lhs.enabled_ == rhs.enabled_
    && lhs.foreground_ == rhs.foreground_
    && lhs.background_ == rhs.background_;
}

QDataStream& operator << (QDataStream& os, DecodeHighlightingModel::HighlightInfo const& item)
{
  return os << item.type_
            << item.enabled_
            << item.foreground_
            << item.background_;
}

QDataStream& operator >> (QDataStream& is, DecodeHighlightingModel::HighlightInfo& item)
{
  return is >> item.type_
           >> item.enabled_
           >> item.foreground_
           >> item.background_;
}

QString DecodeHighlightingModel::HighlightInfo::toString () const
{
  QString string;
  QTextStream ots {&string};
  ots << "HighlightInfo("
      << highlight_name (type_) << ", "
      << enabled_ << ", "
      << foreground_.color ().name () << ", "
      << background_.color ().name () << ')';
  return string;
}

#if !defined (QT_NO_DEBUG_STREAM)
QDebug operator << (QDebug debug, DecodeHighlightingModel::HighlightInfo const& item)
{
  QDebugStateSaver save {debug};
  return debug.nospace () << item.toString ();
}
#endif

ENUM_QDATASTREAM_OPS_IMPL (DecodeHighlightingModel, Highlight);
ENUM_CONVERSION_OPS_IMPL (DecodeHighlightingModel, Highlight);

DecodeHighlightingModel::DecodeHighlightingModel (QObject * parent)
  : QAbstractListModel {parent}
{
}

DecodeHighlightingModel::~DecodeHighlightingModel ()
{
}

QString DecodeHighlightingModel::highlight_name (Highlight h)
{
  switch (h)
    {
    case Highlight::CQ: return tr ("CQ in message");
    case Highlight::MyCall: return tr ("My Call in message");
    case Highlight::Tx: return tr ("Transmitted message");
    case Highlight::DXCC: return tr ("New DXCC");
    case Highlight::DXCCBand: return tr ("New DXCC on Band");
    case Highlight::Grid: return tr ("New Grid");
    case Highlight::GridBand: return tr ("New Grid on Band");
    case Highlight::Call: return tr ("New Call");
    case Highlight::CallBand: return tr ("New Call on Band");
    case Highlight::Continent: return tr ("New Continent");
    case Highlight::ContinentBand: return tr ("New Continent on Band");
    case Highlight::CQZone: return tr ("New CQ Zone");
    case Highlight::CQZoneBand: return tr ("New CQ Zone on Band");
    case Highlight::ITUZone: return tr ("New ITU Zone");
    case Highlight::ITUZoneBand: return tr ("New ITU Zone on Band");
    case Highlight::LotW: return tr ("LoTW User");
    }
  return "Unknown";
}

auto DecodeHighlightingModel::default_items () -> HighlightItems const&
{
  return impl::defaults_;
}

auto DecodeHighlightingModel::default_items2 () -> HighlightItems const&
{
  return impl::defaults2_;
}

auto DecodeHighlightingModel::default_color_preset () -> ColorPreset const&
{
  return impl::default_colors_;
}

auto DecodeHighlightingModel::default2_color_preset () -> ColorPreset const&
{
  return impl::default2_colors_;
}

auto DecodeHighlightingModel::red_green_color_vision_preset () -> ColorPreset const&
{
  return impl::red_green_color_vision_colors_;
}

auto DecodeHighlightingModel::blue_yellow_color_vision_preset () -> ColorPreset const&
{
  return impl::blue_yellow_color_vision_colors_;
}

auto DecodeHighlightingModel::high_contrast_color_preset () -> ColorPreset const&
{
  return impl::high_contrast_colors_;
}

auto DecodeHighlightingModel::dark_shack_color_preset () -> ColorPreset const&
{
  return impl::dark_shack_colors_;
}

auto DecodeHighlightingModel::solarized_color_preset () -> ColorPreset const&
{
  return impl::solarized_colors_;
}

auto DecodeHighlightingModel::monochrome_color_preset () -> ColorPreset const&
{
  return impl::monochrome_colors_;
}

auto DecodeHighlightingModel::resolve_colors (HighlightItems const& items, HighlightTypes const& types,
                                              QColor background, QColor foreground) -> ResolvedHighlight
{
  ResolvedHighlight result {Highlight::CQ, background, foreground};
  QListIterator<HighlightInfo> it {items};
  // Earlier rows have higher priority, so apply matching rules from last to first.
  it.toBack ();
  while (it.hasPrevious ())
    {
      auto const& item = it.previous ();
      if (std::find (types.cbegin (), types.cend (), item.type_) != types.cend () && item.enabled_)
        {
          if (item.background_.style () != Qt::NoBrush)
            {
              result.background_ = item.background_.color ();
            }
          if (item.foreground_.style () != Qt::NoBrush)
            {
              result.foreground_ = item.foreground_.color ();
            }
          result.type_ = item.type_;
        }
    }
  return result;
}

auto DecodeHighlightingModel::items () const -> HighlightItems const&
{
  return m_->data_;
}

void DecodeHighlightingModel::items (HighlightItems const& items)
{
  if (items.size () != m_->data_.size ())
    {
      beginResetModel ();
      m_->data_ = items;
      endResetModel ();
      return;
    }

  m_->data_ = items;
  if (!m_->data_.isEmpty ())
    {
      QVector<int> roles;
      roles << Qt::DisplayRole << Qt::CheckStateRole << Qt::ForegroundRole << Qt::BackgroundRole << TypeRole;
      Q_EMIT dataChanged (index (0, 0), index (rowCount () - 1, 0), roles);
    }
}

bool DecodeHighlightingModel::apply_color_preset (ColorPreset const& preset)
{
  if (m_->data_.isEmpty ())
    {
      return false;
    }

  // Index the preset by rule type; a duplicated type makes the preset ambiguous.
  QHash<int, HighlightColors const *> colors;
  colors.reserve (preset.size ());
  for (auto const& entry : preset)
    {
      auto const key = static_cast<int> (entry.type_);
      if (colors.contains (key))
        {
          return false;
        }
      colors.insert (key, &entry);
    }

  // Validate that every rule has a color before touching the model.
  for (auto const& item : m_->data_)
    {
      if (!colors.contains (static_cast<int> (item.type_)))
        {
          return false;
        }
    }

  bool changed {false};
  for (auto& item : m_->data_)
    {
      auto const entry = colors.value (static_cast<int> (item.type_));
      if (item.foreground_ != entry->foreground_ || item.background_ != entry->background_)
        {
          item.foreground_ = entry->foreground_;
          item.background_ = entry->background_;
          changed = true;
        }
    }

  if (changed)
    {
      QVector<int> roles;
      roles << Qt::DisplayRole << Qt::ForegroundRole << Qt::BackgroundRole;
      Q_EMIT dataChanged (index (0, 0), index (rowCount () - 1, 0), roles);
    }
  return true;
}

void DecodeHighlightingModel::set_font (QFont const& font)
{
  m_->font_ = font;
}

int DecodeHighlightingModel::rowCount (const QModelIndex& parent) const
{
  return parent.isValid () ? 0 : m_->data_.size ();
}

QVariant DecodeHighlightingModel::data (const QModelIndex& index, int role) const
{
  QVariant result;
  if (index.isValid () && index.row () < rowCount ())
    {
      auto const& item = m_->data_[index.row ()];
      auto fg_unset = Qt::NoBrush == item.foreground_.style ();
      auto bg_unset = Qt::NoBrush == item.background_.style ();
      switch (role)
        {
        case Qt::CheckStateRole:
          result = item.enabled_ ? Qt::Checked : Qt::Unchecked;
          break;
        case Qt::DisplayRole:
          return QString {"%1%2%3%4%4%5%6"}
             .arg (highlight_name (item.type_))
             .arg (fg_unset || bg_unset ? QString {" ["} : QString {})
             .arg (fg_unset ? tr ("f/g unset") : QString {})
             .arg (fg_unset && bg_unset ? QString {" "} : QString {})
             .arg (bg_unset ? tr ("b/g unset") : QString {})
             .arg (fg_unset || bg_unset ? QString {"]"} : QString {});
          break;
        case Qt::ForegroundRole:
          if (!fg_unset)
            {
              result = item.foreground_;
            }
          break;
        case Qt::BackgroundRole:
          if (!bg_unset)
            {
              result = item.background_;
            }
          break;
        case Qt::FontRole:
          result = m_->font_;
          break;
        case TypeRole:
          result = static_cast<int> (item.type_);
          break;
        case EnabledDefaultRole:
          for (auto const& default_item : impl::defaults_)
            {
              if (default_item.type_ == item.type_)
                {
                  result = default_item.enabled_ ? Qt::Checked : Qt::Unchecked;
                }
            }
          break;
        case ForegroundDefaultRole:
          for (auto const& default_item : impl::defaults_)
            {
              if (default_item.type_ == item.type_)
                {
                  result = default_item.foreground_;
                }
            }
          break;
        case BackgroundDefaultRole:
          for (auto const& default_item : impl::defaults_)
            {
              if (default_item.type_ == item.type_)
                {
                  result = default_item.background_;
                }
            }
          break;
        }
    }
  return result;
}

// Override  QAbstractItemModel::itemData()  as  it  is  used  by  the
// default mime encode  routine used in drag'n'drop  operations and we
// want to transport  the type role, this is because  the display role
// is derived from the type role.
QMap<int, QVariant> DecodeHighlightingModel::itemData (QModelIndex const& index) const
{
  auto roles = QAbstractListModel::itemData (index);
  QVariant variantData = data (index, TypeRole);
  if (variantData.isValid ())
    {
      roles.insert (TypeRole, variantData);
    }
  return roles;
}

QVariant DecodeHighlightingModel::headerData (int /*section*/, Qt::Orientation orientation, int role) const
{
  QVariant header;
  if (Qt::DisplayRole == role && Qt::Horizontal == orientation)
    {
      header = tr ("Highlight Type");
    }
  return header;
}

Qt::ItemFlags DecodeHighlightingModel::flags (QModelIndex const& index) const
{
  auto flags = QAbstractListModel::flags (index) | Qt::ItemIsDragEnabled;
  if (index.isValid ())
    {
      flags |= Qt::ItemIsUserCheckable;
    }
  else
    {
      flags |= Qt::ItemIsDropEnabled;
    }
  return flags;
}

bool DecodeHighlightingModel::setData (QModelIndex const& index, QVariant const& value, int role)
{
  bool ok {false};
  if (index.isValid () && index.row () < rowCount ())
    {
      auto& item = m_->data_[index.row ()];
      QVector<int> roles;
      roles << role;
      switch (role)
        {
        case Qt::DisplayRole:
        case Qt::FontRole:
          ok = true;
          break;
        case Qt::CheckStateRole:
          if (item.enabled_ != (Qt::Checked == value))
            {
              item.enabled_ = Qt::Checked == value;
              Q_EMIT dataChanged (index, index, roles);
            }
          ok = true;
          break;
        case Qt::ForegroundRole:
          if (item.foreground_ != value.value<QBrush> ())
            {
              item.foreground_ = value.value<QBrush> ();
              roles << Qt::DisplayRole;
              Q_EMIT dataChanged (index, index, roles);
            }
          ok = true;
          break;
        case Qt::BackgroundRole:
          if (item.background_ != value.value<QBrush> ())
            {
              item.background_ = value.value<QBrush> ();
              roles << Qt::DisplayRole;
              Q_EMIT dataChanged (index, index, roles);
            }
          ok = true;
          break;
        case TypeRole:
          if (item.type_ != static_cast<Highlight> (value.toInt ()))
            {
              item.type_ = static_cast<Highlight> (value.toInt ());
              roles << Qt::DisplayRole;
              Q_EMIT dataChanged (index, index, roles);
            }
          ok = true;
          break;
        }
    }
  return ok;
}

Qt::DropActions DecodeHighlightingModel::supportedDropActions () const
{
  return Qt::MoveAction;
}

bool DecodeHighlightingModel::insertRows (int row, int count, QModelIndex const& parent)
{
  beginInsertRows (parent, row, row + count - 1);
  for (int index = 0; index < count; ++index)
    {
      m_->data_.insert (row, HighlightInfo {Highlight::CQ, false, {}, {}});
    }
  endInsertRows ();
  return true;
}

bool DecodeHighlightingModel::removeRows (int row, int count, QModelIndex const& parent)
{
  beginRemoveRows (parent, row, row + count - 1);
  for (int index = 0; index < count; ++index)
    {
      m_->data_.removeAt (row);
    }
  endRemoveRows ();
  return true;
}
