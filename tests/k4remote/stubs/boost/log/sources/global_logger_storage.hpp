#pragma once
#define BOOST_LOG_GLOBAL_LOGGER(Name, Type)                                    \
  struct Name {                                                                \
    static Type &get();                                                        \
  }
