cmake_minimum_required (VERSION 3.12)

if (NOT SOURCE_DIR OR NOT BINARY_DIR OR NOT TEST_BINARY_DIR)
  message (FATAL_ERROR "SOURCE_DIR, BINARY_DIR, and TEST_BINARY_DIR are required")
endif ()

# A pending re-run of CMake must see the real environment, not the SOURCE_DATE_EPOCH set below.
execute_process (
  COMMAND "${CMAKE_COMMAND}" --build "${BINARY_DIR}" --target revisiontag
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_output)
if (NOT build_result EQUAL 0)
  message (FATAL_ERROR "Building revisiontag failed:\n${build_output}")
endif ()
file (STRINGS "${BINARY_DIR}/CMakeCache.txt" channel REGEX "^WSJT_RELEASE_CHANNEL:[A-Z]+=")
string (REGEX REPLACE "^[^=]*=" "" channel "${channel}")
if (NOT channel MATCHES "^(DEVEL|BETA|RC|GA)$")
  message (FATAL_ERROR "No release channel in ${BINARY_DIR}/CMakeCache.txt")
endif ()

include ("${SOURCE_DIR}/CMake/Modules/set_build_type.cmake")
set (ENV{SOURCE_DATE_EPOCH} 1700000000)
wsjt_prerelease_expiry ("${channel}" "${SOURCE_DIR}" expected)

set (header "${BINARY_DIR}/prerelease_expiry.h")
# file (COPY) keeps the timestamp, so restoring the header does not recompile main.cpp.
file (REMOVE_RECURSE "${TEST_BINARY_DIR}")
file (COPY "${header}" DESTINATION "${TEST_BINARY_DIR}")
execute_process (
  COMMAND "${CMAKE_COMMAND}" --build "${BINARY_DIR}" --target revisiontag
  RESULT_VARIABLE build_result
  OUTPUT_VARIABLE build_output
  ERROR_VARIABLE build_output)
file (READ "${header}" built)
file (COPY "${TEST_BINARY_DIR}/prerelease_expiry.h" DESTINATION "${BINARY_DIR}")

if (NOT build_result EQUAL 0)
  message (FATAL_ERROR "Building revisiontag failed:\n${build_output}")
endif ()
if (NOT built STREQUAL "#define WSJT_PRERELEASE_EXPIRY ${expected}\n")
  message (FATAL_ERROR "revisiontag wrote '${built}', expected ${expected} from SOURCE_DATE_EPOCH")
endif ()
