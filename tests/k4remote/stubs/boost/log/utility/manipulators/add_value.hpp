#pragma once
namespace boost {
namespace log {
template <typename T> T const &add_value(char const *, T const &value) {
  return value;
}
} // namespace log
} // namespace boost
