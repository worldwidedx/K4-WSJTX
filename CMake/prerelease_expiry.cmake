if (NOT CHANNEL MATCHES "^(DEVEL|BETA|RC|GA)$" OR NOT SOURCE_DIR OR NOT OUTPUT_DIR
    OR NOT CONFIGURE_EPOCH MATCHES "^[0-9]+$")
  message (FATAL_ERROR "CHANNEL (DEVEL, BETA, RC or GA), SOURCE_DIR, OUTPUT_DIR and CONFIGURE_EPOCH are required; got CHANNEL \"${CHANNEL}\"")
endif ()
include ("${CMAKE_CURRENT_LIST_DIR}/Modules/set_build_type.cmake")
wsjt_prerelease_expiry ("${CHANNEL}" "${SOURCE_DIR}" expiry "${CONFIGURE_EPOCH}")
message (STATUS "Prerelease expiry: ${expiry}")
set (header "${OUTPUT_DIR}/prerelease_expiry.h")
file (WRITE "${header}.txt" "#define WSJT_PRERELEASE_EXPIRY ${expiry}\n")
# An unchanged header keeps main.cpp from recompiling.
execute_process (COMMAND ${CMAKE_COMMAND} -E copy_if_different "${header}.txt" "${header}"
  RESULT_VARIABLE copy_result)
file (REMOVE "${header}.txt")
if (NOT copy_result EQUAL 0)
  message (FATAL_ERROR "Could not update ${header}")
endif ()
