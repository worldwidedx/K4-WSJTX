cmake_minimum_required (VERSION 3.12)

if (NOT SOURCE_DIR)
  message (FATAL_ERROR "SOURCE_DIR is required")
endif ()

# CONFIGURE_DEPENDS is the top-level CMAKE_CONFIGURE_DEPENDS, "|"-separated.
string (REPLACE "|" ";" depends "${CONFIGURE_DEPENDS}")
list (FIND depends "${SOURCE_DIR}/release-state.txt" index)
if (index EQUAL -1)
  message (FATAL_ERROR "release-state.txt is not a configure dependency, so a commit that changes it leaves an incremental build's channel, version and expiry stale")
endif ()
