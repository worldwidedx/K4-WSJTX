#include "models/FrequencySuggestions.hpp"

#include "models/Bands.hpp"

namespace
{
  bool region_covers (IARURegions::Region existing, IARURegions::Region suggestion)
  {
    return existing == suggestion || existing == IARURegions::ALL;
  }

  bool mode_covers (Modes::Mode existing, Modes::Mode suggestion)
  {
    return existing == suggestion
      || (existing == Modes::ALL && suggestion != Modes::FreqCal);
  }

  bool regions_overlap (IARURegions::Region lhs, IARURegions::Region rhs)
  {
    return region_covers (lhs, rhs) || region_covers (rhs, lhs);
  }

  bool modes_overlap (Modes::Mode lhs, Modes::Mode rhs)
  {
    return mode_covers (lhs, rhs) || mode_covers (rhs, lhs);
  }

  bool contains_catalog (FrequencySuggestions::Items const& catalog,
                         FrequencySuggestions::Items const& working)
  {
    for (auto const& item : catalog)
      if (!FrequencySuggestions::is_present (item, working)) return false;
    return true;
  }
}

QString FrequencySuggestions::key (Item const& item)
{
  return QString::number (item.region_) + ':'
    + QString::number (item.mode_) + ':'
    + QString::number (item.frequency_);
}

bool FrequencySuggestions::is_present (Item const& suggestion, Items const& working)
{
  for (auto const& item : working)
    {
      if (item.frequency_ == suggestion.frequency_
          && region_covers (item.region_, suggestion.region_)
          && mode_covers (item.mode_, suggestion.mode_))
        {
          return true;
        }
    }
  return false;
}

auto FrequencySuggestions::prepare_additions (Items const& selected,
                                               Items const& working,
                                               Bands const& bands) -> Items
{
  Items additions;
  Items combined = working;
  for (auto suggestion : selected)
    {
      if (is_present (suggestion, combined)) continue;

      if (suggestion.preferred_)
        {
          auto const band = bands.find (suggestion.frequency_);
          for (auto const& item : combined)
            {
              if (item.preferred_ && !band.isEmpty ()
                  && bands.find (item.frequency_) == band
                  && regions_overlap (item.region_, suggestion.region_)
                  && modes_overlap (item.mode_, suggestion.mode_))
                {
                  suggestion.preferred_ = false;
                  break;
                }
            }
        }

      additions.append (suggestion);
      combined.append (suggestion);
    }
  return additions;
}

FrequencySuggestions::NoticeState::NoticeState (QStringList seen_keys, bool has_seen_catalog)
  : has_seen_catalog_ {has_seen_catalog}
{
  for (auto const& key : seen_keys) seen_keys_.insert (key);
}

auto FrequencySuggestions::NoticeState::visibility (Items const& catalog,
                                                     Items const& working) const -> Visibility
{
  bool missing {false};
  for (auto const& item : catalog)
    {
      if (is_present (item, working)) continue;
      missing = true;
      if (has_seen_catalog_ && !seen_keys_.contains (key (item))) return Visibility::New;
    }
  return missing && !has_seen_catalog_ ? Visibility::Available : Visibility::Hidden;
}

void FrequencySuggestions::NoticeState::selected_additions (Items const& additions)
{
  pending_additions_ += additions;
}

void FrequencySuggestions::NoticeState::cancel ()
{
  pending_additions_.clear ();
}

bool FrequencySuggestions::NoticeState::commit (Items const& catalog,
                                                 Items const& committed_working)
{
  bool saved_addition {false};
  for (auto const& item : pending_additions_)
    if (is_present (item, committed_working))
      {
        saved_addition = true;
        break;
      }
  auto const acknowledge_catalog = saved_addition || contains_catalog (catalog, committed_working);
  pending_additions_.clear ();
  if (!acknowledge_catalog) return false;
  acknowledge (catalog);
  return true;
}

bool FrequencySuggestions::NoticeState::acknowledge_present_catalog (
  Items const& catalog, Items const& committed_working)
{
  if (!contains_catalog (catalog, committed_working)) return false;
  QSet<QString> keys;
  for (auto const& item : catalog) keys.insert (key (item));
  if (has_seen_catalog_ && seen_keys_ == keys) return false;
  acknowledge (catalog);
  return true;
}

void FrequencySuggestions::NoticeState::dismiss (Items const& catalog)
{
  acknowledge (catalog);
}

QStringList FrequencySuggestions::NoticeState::seen_keys () const
{
  auto keys = seen_keys_.values ();
  keys.sort ();
  return keys;
}

void FrequencySuggestions::NoticeState::acknowledge (Items const& catalog)
{
  seen_keys_.clear ();
  for (auto const& item : catalog) seen_keys_.insert (key (item));
  has_seen_catalog_ = true;
}
