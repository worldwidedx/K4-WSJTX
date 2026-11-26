cmake_minimum_required (VERSION 3.12)

if (NOT SOURCE_DIR OR NOT TOOL)
  message (FATAL_ERROR "SOURCE_DIR and TOOL are required")
endif ()

find_program (TOOL_EXECUTABLE "${TOOL}")
if (NOT TOOL_EXECUTABLE)
  message ("Skipped: ${TOOL} is not installed")
  return ()
endif ()

include ("${SOURCE_DIR}/CMake/Modules/set_build_type.cmake")
set (PROJECT_NAME wsjtx)

function (package_version version channel prerelease out)
  string (REPLACE "." ";" parts "${version}")
  list (GET parts 0 PROJECT_VERSION_MAJOR)
  list (GET parts 1 PROJECT_VERSION_MINOR)
  list (GET parts 2 PROJECT_VERSION_PATCH)
  set (WSJT_RELEASE_CHANNEL "${channel}" CACHE STRING "" FORCE)
  set (WSJT_PRERELEASE_NUMBER "${prerelease}" CACHE STRING "" FORCE)
  set_build_type ()
  set (${out} "${version}${BUILD_TYPE_PACKAGE_REVISION}" PARENT_SCOPE)
endfunction ()

function (expect_older older newer)
  if (TOOL STREQUAL "dpkg")
    execute_process (
      COMMAND "${TOOL_EXECUTABLE}" --compare-versions "${older}" lt "${newer}"
      RESULT_VARIABLE comparison)
    set (expected 0)
  elseif (TOOL STREQUAL "rpm")
    execute_process (
      COMMAND "${TOOL_EXECUTABLE}" --eval "%{lua:print(rpm.vercmp('${older}', '${newer}'))}"
      OUTPUT_VARIABLE comparison
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (expected -1)
  else ()
    message (FATAL_ERROR "Unsupported package tool: ${TOOL}")
  endif ()
  if (NOT "${comparison}" STREQUAL "${expected}")
    message (FATAL_ERROR "${TOOL} does not order ${older} before ${newer} (got '${comparison}')")
  endif ()
endfunction ()

set (previous "")
foreach (state
    "3.3.0;BETA;1" "3.3.0;BETA;2" "3.3.0;BETA;10" "3.3.0;RC;1" "3.3.0;RC;2" "3.3.0;GA;"
    "3.3.1;BETA;1")
  list (GET state 0 version)
  list (GET state 1 channel)
  list (GET state 2 prerelease)
  package_version ("${version}" "${channel}" "${prerelease}" current)
  if (previous)
    expect_older ("${previous}" "${current}")
  endif ()
  set (previous "${current}")
endforeach ()
