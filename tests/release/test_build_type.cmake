cmake_minimum_required (VERSION 3.12)

if (NOT SOURCE_DIR)
  message (FATAL_ERROR "SOURCE_DIR is required")
endif ()

include ("${SOURCE_DIR}/CMake/Modules/set_build_type.cmake")
set (PROJECT_NAME wsjtx)
set (PROJECT_VERSION_MAJOR 3)
set (PROJECT_VERSION_MINOR 3)
set (PROJECT_VERSION_PATCH 0)

if (DEFINED CHECK_CHANNEL)
  set (WSJT_RELEASE_CHANNEL "${CHECK_CHANNEL}" CACHE STRING "" FORCE)
  set (WSJT_PRERELEASE_NUMBER "${CHECK_PRERELEASE}" CACHE STRING "" FORCE)
  set_build_type ()
  return ()
endif ()
if (DEFINED CHECK_SOURCE_DATE_EPOCH)
  set (ENV{SOURCE_DATE_EPOCH} "${CHECK_SOURCE_DATE_EPOCH}")
  wsjt_prerelease_expiry (DEVEL "${SOURCE_DIR}" expiry)
  return ()
endif ()

if (NOT TEST_BINARY_DIR)
  message (FATAL_ERROR "TEST_BINARY_DIR is required")
endif ()

function (assert_equal actual expected description)
  if (NOT "${actual}" STREQUAL "${expected}")
    message (FATAL_ERROR "${description}: expected '${expected}', got '${actual}'")
  endif ()
endfunction ()

function (expect_build_type channel prerelease revision package_revision)
  set (WSJT_RELEASE_CHANNEL "${channel}" CACHE STRING "" FORCE)
  set (WSJT_PRERELEASE_NUMBER "${prerelease}" CACHE STRING "" FORCE)
  set_build_type ()
  assert_equal ("${BUILD_TYPE_REVISION}" "${revision}" "${channel} ${prerelease} version suffix")
  assert_equal ("${BUILD_TYPE_PACKAGE_REVISION}" "${package_revision}" "${channel} ${prerelease} package suffix")
endfunction ()

function (expect_rejected description)
  execute_process (
    COMMAND "${CMAKE_COMMAND}" -D SOURCE_DIR=${SOURCE_DIR} ${ARGN} -P "${CMAKE_CURRENT_LIST_FILE}"
    RESULT_VARIABLE rejected_result
    OUTPUT_QUIET
    ERROR_QUIET)
  if (rejected_result EQUAL 0)
    message (FATAL_ERROR "Invalid input was accepted: ${description}")
  endif ()
endfunction ()

expect_build_type (DEVEL "" "-devel" "-devel")
expect_build_type (BETA 1 "-beta1" "~beta1")
expect_build_type (beta 10 "-beta10" "~beta10")
expect_build_type (RC 2 "-rc2" "~rc2")
expect_build_type (GA "" "" "")

expect_rejected ("BETA without a number" -D CHECK_CHANNEL=BETA -D CHECK_PRERELEASE=)
expect_rejected ("BETA 0" -D CHECK_CHANNEL=BETA -D CHECK_PRERELEASE=0)
expect_rejected ("RC without a number" -D CHECK_CHANNEL=RC -D CHECK_PRERELEASE=)
expect_rejected ("DEVEL with a number" -D CHECK_CHANNEL=DEVEL -D CHECK_PRERELEASE=1)
expect_rejected ("GA with a number" -D CHECK_CHANNEL=GA -D CHECK_PRERELEASE=1)
expect_rejected ("unknown channel" -D CHECK_CHANNEL=ALPHA -D CHECK_PRERELEASE=1)

set (unversioned "${TEST_BINARY_DIR}/expiry-unversioned")
file (MAKE_DIRECTORY "${unversioned}")
set (checkout "${unversioned}")
if (GIT_EXECUTABLE)
  set (checkout "${TEST_BINARY_DIR}/expiry-checkout")
  file (REMOVE_RECURSE "${checkout}")
  file (MAKE_DIRECTORY "${checkout}")
  set (git "${GIT_EXECUTABLE}" -C "${checkout}"
    -c user.name=Release -c user.email=release@example.invalid -c commit.gpgsign=false)
  execute_process (COMMAND ${git} init -q RESULT_VARIABLE init_result)
  execute_process (
    COMMAND "${CMAKE_COMMAND}" -E env
      "GIT_AUTHOR_DATE=1700000000 +0000" "GIT_COMMITTER_DATE=1700000000 +0000"
      ${git} commit -q --allow-empty -m expiry
    RESULT_VARIABLE commit_result)
  if (NOT init_result EQUAL 0 OR NOT commit_result EQUAL 0)
    message (FATAL_ERROR "Could not create the expiry test checkout")
  endif ()
endif ()

# 2025-09-30T14:20:00Z expires at 2025-12-29T23:59:59Z.
set (ENV{SOURCE_DATE_EPOCH} 1759242000)
foreach (channel DEVEL BETA RC)
  wsjt_prerelease_expiry (${channel} "${checkout}" expiry)
  assert_equal ("${expiry}" "1767052799" "${channel} expiry from SOURCE_DATE_EPOCH")
endforeach ()
wsjt_prerelease_expiry (GA "${checkout}" expiry)
assert_equal ("${expiry}" "0" "GA expiry")
expect_rejected ("malformed SOURCE_DATE_EPOCH" -D CHECK_SOURCE_DATE_EPOCH=yesterday)
unset (ENV{SOURCE_DATE_EPOCH})

# The build-time step as revisiontag runs it, configured at 2020-09-13T12:26:40Z.
function (write_expiry_header channel source_dir result_out)
  execute_process (
    COMMAND "${CMAKE_COMMAND}"
      -D CHANNEL=${channel}
      -D SOURCE_DIR=${source_dir}
      -D OUTPUT_DIR=${TEST_BINARY_DIR}
      -D GIT_EXECUTABLE=${GIT_EXECUTABLE}
      -D CONFIGURE_EPOCH=1600000000
      -P "${SOURCE_DIR}/CMake/prerelease_expiry.cmake"
    RESULT_VARIABLE header_result
    OUTPUT_QUIET
    ERROR_VARIABLE header_error)
  set (${result_out} "${header_result}" PARENT_SCOPE)
  set (header_error "${header_error}" PARENT_SCOPE)
endfunction ()

function (expect_expiry_header channel source_dir expected description)
  write_expiry_header (${channel} "${source_dir}" header_result)
  if (NOT header_result EQUAL 0)
    message (FATAL_ERROR "${description}: the build-time expiry step failed: ${header_error}")
  endif ()
  file (READ "${TEST_BINARY_DIR}/prerelease_expiry.h" header)
  assert_equal ("${header}" "#define WSJT_PRERELEASE_EXPIRY ${expected}\n" "${description}")
endfunction ()

file (REMOVE "${TEST_BINARY_DIR}/prerelease_expiry.h")
expect_expiry_header (BETA "${unversioned}" 1607817599 "build-time expiry without Git, from the configure time")
set (ENV{SOURCE_DATE_EPOCH} 1700000000)
expect_expiry_header (DEVEL "${unversioned}" 1707782399 "build-time expiry from SOURCE_DATE_EPOCH")
file (TIMESTAMP "${TEST_BINARY_DIR}/prerelease_expiry.h" written "%s" UTC)
execute_process (COMMAND "${CMAKE_COMMAND}" -E sleep 1)
expect_expiry_header (RC "${unversioned}" 1707782399 "build-time expiry, unchanged")
file (TIMESTAMP "${TEST_BINARY_DIR}/prerelease_expiry.h" rewritten "%s" UTC)
assert_equal ("${rewritten}" "${written}" "an unchanged expiry rewrote the header")
expect_expiry_header (GA "${unversioned}" 0 "build-time GA expiry")
unset (ENV{SOURCE_DATE_EPOCH})
foreach (channel "" ALPHA)
  write_expiry_header ("${channel}" "${unversioned}" header_result)
  if (header_result EQUAL 0)
    message (FATAL_ERROR "The build-time expiry step accepted the channel \"${channel}\"")
  endif ()
endforeach ()

if (GIT_EXECUTABLE)
  # Committed 2023-11-14T22:13:20Z; expires at 2024-02-12T23:59:59Z.
  wsjt_prerelease_expiry (RC "${checkout}" expiry)
  assert_equal ("${expiry}" "1707782399" "expiry from the commit date")
  expect_expiry_header (RC "${checkout}" 1707782399 "build-time expiry from the commit date")

  execute_process (
    COMMAND "${CMAKE_COMMAND}" -E env
      "GIT_AUTHOR_DATE=1759242000 +0000" "GIT_COMMITTER_DATE=1759242000 +0000"
      ${git} commit -q --allow-empty -m "expiry 2"
    RESULT_VARIABLE commit_result)
  if (NOT commit_result EQUAL 0)
    message (FATAL_ERROR "Could not add a commit to the expiry test checkout")
  endif ()
  expect_expiry_header (BETA "${checkout}" 1767052799 "build-time expiry after a new commit")
  set (ENV{SOURCE_DATE_EPOCH} 1700000000)
  expect_expiry_header (DEVEL "${checkout}" 1707782399 "build-time SOURCE_DATE_EPOCH over the commit date")
  unset (ENV{SOURCE_DATE_EPOCH})
endif ()

string (TIMESTAMP before "%s" UTC)
wsjt_prerelease_expiry (BETA "${unversioned}" expiry)
string (TIMESTAMP after "%s" UTC)
math (EXPR earliest "(${before} / 86400 + 91) * 86400 - 1")
math (EXPR latest "(${after} / 86400 + 91) * 86400 - 1")
if (NOT expiry MATCHES "^[0-9]+$" OR expiry LESS earliest OR expiry GREATER latest)
  message (FATAL_ERROR "expiry without Git: expected ${earliest}..${latest}, got '${expiry}'")
endif ()
