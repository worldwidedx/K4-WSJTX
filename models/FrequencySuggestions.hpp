#ifndef FREQUENCY_SUGGESTIONS_HPP__
#define FREQUENCY_SUGGESTIONS_HPP__

#include "models/FrequencyList.hpp"

#include <QSet>
#include <QStringList>

class Bands;

namespace FrequencySuggestions
{
  using Item = FrequencyList_v2_101::Item;
  using Items = FrequencyList_v2_101::FrequencyItems;

  QString key (Item const&);
  bool is_present (Item const&, Items const& working);
  Items prepare_additions (Items const& selected, Items const& working, Bands const&);

  class NoticeState
  {
  public:
    enum class Visibility { Hidden, Available, New };

    NoticeState (QStringList seen_keys = {}, bool has_seen_catalog = false);

    Visibility visibility (Items const& catalog, Items const& working) const;
    void selected_additions (Items const& additions);
    void cancel ();
    bool commit (Items const& catalog, Items const& committed_working);
    bool acknowledge_present_catalog (Items const& catalog, Items const& committed_working);
    void dismiss (Items const& catalog);
    QStringList seen_keys () const;

  private:
    void acknowledge (Items const& catalog);

    QSet<QString> seen_keys_;
    bool has_seen_catalog_ {false};
    Items pending_additions_;
  };
}

#endif
