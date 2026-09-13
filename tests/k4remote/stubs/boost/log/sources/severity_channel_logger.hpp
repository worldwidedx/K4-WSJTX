#pragma once
namespace boost {
namespace log {
namespace keywords {
struct channel_keyword {
  template <typename T> int operator=(T const &) const { return 0; }
};
static channel_keyword channel;
} // namespace keywords
namespace sources {
template <typename Severity> class severity_channel_logger_mt {
public:
  severity_channel_logger_mt() {}
  template <typename T> explicit severity_channel_logger_mt(T const &) {}
};
} // namespace sources
} // namespace log
} // namespace boost
