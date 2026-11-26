cmake_minimum_required (VERSION 3.12)

if (NOT SOURCE_DIR)
  message (FATAL_ERROR "SOURCE_DIR is required")
endif ()

include ("${SOURCE_DIR}/CMake/Modules/read_release_state.cmake")

function (assert_equal actual expected description)
  if (NOT "${actual}" STREQUAL "${expected}")
    message (FATAL_ERROR "${description}: expected '${expected}', got '${actual}'")
  endif ()
endfunction ()

function (expect_invalid name contents)
  set (_fixture "${_fixture_dir}/${name}.txt")
  file (WRITE "${_fixture}" "${contents}")
  execute_process (
    COMMAND "${CMAKE_COMMAND}"
      -D RELEASE_STATE_READER=${SOURCE_DIR}/CMake/Modules/read_release_state.cmake
      -D RELEASE_STATE_FILE=${_fixture}
      -P ${SOURCE_DIR}/tests/release/validate_release_state_fixture.cmake
    RESULT_VARIABLE invalid_result
    OUTPUT_QUIET
    ERROR_QUIET)
  if (invalid_result EQUAL 0)
    message (FATAL_ERROR "Invalid release state was accepted: ${name}")
  endif ()
endfunction ()

wsjt_read_release_state (
  "${SOURCE_DIR}/release-state.txt"
  version channel prerelease revision)

if (EXPECT_ARCHIVE_REVISION)
  assert_equal ("${revision}" "${EXPECT_ARCHIVE_REVISION}" "archive revision")
elseif (EXISTS "${SOURCE_DIR}/.git")
  # git archive expands the placeholder, so only a checkout must carry it.
  assert_equal ("${revision}" "" "working-tree archive placeholder")
endif ()

if (TEST_BINARY_DIR)
  set (_fixture_dir "${TEST_BINARY_DIR}")
else ()
  set (_fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/release-state-test")
endif ()
file (MAKE_DIRECTORY "${_fixture_dir}")
file (WRITE "${_fixture_dir}/rc.txt"
  "version=3.2.0\nchannel=RC\nprerelease=7\nrevision=0123456789abcdef0123456789abcdef01234567\n")
wsjt_read_release_state (
  "${_fixture_dir}/rc.txt"
  version channel prerelease revision)
assert_equal ("${channel}" "RC" "RC channel")
assert_equal ("${prerelease}" "7" "RC number")
assert_equal ("${revision}" "0123456789abcdef0123456789abcdef01234567" "expanded revision")

file (WRITE "${_fixture_dir}/beta.txt"
  "version=3.3.0\nchannel=BETA\nprerelease=12\nrevision=$Format:%H$\nwindows_signing=unsigned\n")
wsjt_read_release_state (
  "${_fixture_dir}/beta.txt"
  version channel prerelease revision)
assert_equal ("${channel}" "BETA" "BETA channel")
assert_equal ("${prerelease}" "12" "BETA number")

file (WRITE "${_fixture_dir}/unsigned.txt"
  "version=3.2.0\nchannel=RC\nprerelease=1\nrevision=$Format:%H$\nwindows_signing=unsigned\n")
wsjt_read_release_state (
  "${_fixture_dir}/unsigned.txt"
  version channel prerelease revision)
assert_equal ("${channel}" "RC" "unsigned release channel")
assert_equal ("${prerelease}" "1" "unsigned release number")

file (WRITE "${_fixture_dir}/ga.txt"
  "version=3.2.0\nchannel=GA\nprerelease=\nrevision=$Format:%H$\n")
wsjt_read_release_state (
  "${_fixture_dir}/ga.txt"
  version channel prerelease revision)
assert_equal ("${channel}" "GA" "GA channel")
assert_equal ("${prerelease}" "" "GA prerelease number")

expect_invalid (ga_with_prerelease
  "version=3.2.0\nchannel=GA\nprerelease=1\nrevision=$Format:%H$\n")
expect_invalid (devel_with_prerelease
  "version=3.3.0\nchannel=DEVEL\nprerelease=1\nrevision=$Format:%H$\n")
expect_invalid (rc_without_number
  "version=3.2.0\nchannel=RC\nprerelease=\nrevision=$Format:%H$\n")
expect_invalid (beta_without_number
  "version=3.3.0\nchannel=BETA\nprerelease=\nrevision=$Format:%H$\nwindows_signing=unsigned\n")
expect_invalid (beta_zero
  "version=3.3.0\nchannel=BETA\nprerelease=0\nrevision=$Format:%H$\nwindows_signing=unsigned\n")
expect_invalid (old_rc_key
  "version=3.2.0\nchannel=RC\nrc=1\nrevision=$Format:%H$\n")
expect_invalid (invalid_version
  "version=3.2\nchannel=DEVEL\nprerelease=\nrevision=$Format:%H$\n")
expect_invalid (invalid_revision
  "version=3.2.0\nchannel=DEVEL\nprerelease=\nrevision=not-a-git-object-id\n")
expect_invalid (unknown_key
  "version=3.2.0\nchannel=DEVEL\nprerelease=\ncommit=$Format:%H$\n")
expect_invalid (invalid_windows_signing
  "version=3.2.0\nchannel=RC\nprerelease=1\nrevision=$Format:%H$\nwindows_signing=ephemeral\n")
expect_invalid (duplicate_windows_signing
  "version=3.2.0\nchannel=RC\nprerelease=1\nrevision=$Format:%H$\nwindows_signing=unsigned\nwindows_signing=unsigned\n")

set (_cache_source "${_fixture_dir}/cache-source")
set (_cache_build "${_fixture_dir}/cache-build")
file (MAKE_DIRECTORY "${_cache_source}")
file (WRITE "${_cache_source}/CMakeLists.txt"
  "cmake_minimum_required(VERSION 3.12)\n"
  "project(release_cache NONE)\n"
  "include(\"${SOURCE_DIR}/CMake/Modules/read_release_state.cmake\")\n"
  "wsjt_read_release_state(\"${_cache_source}/state.txt\" version channel prerelease revision)\n"
  "wsjt_set_release_channel_cache(\"\${channel}\" \"\${prerelease}\")\n"
  "file(WRITE \"\${CMAKE_BINARY_DIR}/result.txt\" \"\${WSJT_RELEASE_CHANNEL}:\${WSJT_PRERELEASE_NUMBER}\")\n")

function (configure_cache build expected description)
  execute_process (
    COMMAND "${CMAKE_COMMAND}" -S "${_cache_source}" -B "${build}" ${ARGN}
    RESULT_VARIABLE cache_result OUTPUT_QUIET ERROR_QUIET)
  if (NOT cache_result EQUAL 0)
    message (FATAL_ERROR "${description}: configure failed")
  endif ()
  file (READ "${build}/result.txt" cache_result_value)
  assert_equal ("${cache_result_value}" "${expected}" "${description}")
endfunction ()

file (WRITE "${_cache_source}/state.txt"
  "version=3.3.0\nchannel=DEVEL\nprerelease=\nrevision=$Format:%H$\n")
configure_cache ("${_cache_build}" "DEVEL:" "initial release cache")
file (WRITE "${_cache_source}/state.txt"
  "version=3.3.0\nchannel=BETA\nprerelease=2\nrevision=$Format:%H$\nwindows_signing=unsigned\n")
configure_cache ("${_cache_build}" "BETA:2" "tracked metadata refresh")

set (_override_build "${_fixture_dir}/override-build")
configure_cache ("${_override_build}" "GA:" "explicit cache override"
  -D WSJT_RELEASE_CHANNEL=GA -D WSJT_PRERELEASE_NUMBER=)
configure_cache ("${_override_build}" "BETA:2" "cleared channel override"
  -D WSJT_RELEASE_CHANNEL=)

configure_cache ("${_fixture_dir}/empty-override-build" "BETA:2" "empty channel override"
  -D WSJT_RELEASE_CHANNEL= -D WSJT_PRERELEASE_NUMBER=5)
