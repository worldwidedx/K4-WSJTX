include (CMakeParseArguments)

# set_build_type() function
#
# Configure  the output  artefacts  and their  names for  development,
# Beta (BETA), Release Candidate (RC), or General Availability (GA) build type.
#
# Usage:
#	set_build_type ()	channel and number from WSJT_RELEASE_CHANNEL and WSJT_PRERELEASE_NUMBER
#	set_build_type (RC n)	RC n, or DEVEL when n is 0
#	set_build_type (GA)
#
#	BUILD_TYPE_REVISION is "-devel", "-betaN", "-rcN", or empty for GA.
#
macro (set_build_type)
  set (WSJT_RELEASE_CHANNEL "DEVEL" CACHE STRING "Build release channel: DEVEL, BETA, RC, or GA.")
  set_property (CACHE WSJT_RELEASE_CHANNEL PROPERTY STRINGS DEVEL BETA RC GA)
  set (WSJT_PRERELEASE_NUMBER "" CACHE STRING "Prerelease number used when WSJT_RELEASE_CHANNEL is BETA or RC.")

  set (options GA)
  set (oneValueArgs RC)
  set (multiValueArgs)
  cmake_parse_arguments (BUILD_TYPE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if (BUILD_TYPE_UNPARSED_ARGUMENTS)
    message (FATAL_ERROR "Unrecognized macro arguments: \"${BUILD_TYPE_UNPARSED_ARGUMENTS}\"")
  endif ()
  if (BUILD_TYPE_GA AND BUILD_TYPE_RC)
    message (FATAL_ERROR "Only specify one build type from RC or GA.")
  endif ()

  string (TOUPPER "${WSJT_RELEASE_CHANNEL}" _WSJT_RELEASE_CHANNEL)
  if (${ARGC} GREATER 0)
    if (BUILD_TYPE_GA)
      set (_WSJT_RELEASE_CHANNEL "GA")
    elseif (BUILD_TYPE_RC)
      if ("${BUILD_TYPE_RC}" STREQUAL "0")
        set (_WSJT_RELEASE_CHANNEL "DEVEL")
      else ()
        set (_WSJT_RELEASE_CHANNEL "RC")
        set (WSJT_PRERELEASE_NUMBER "${BUILD_TYPE_RC}")
      endif ()
    else ()
      set (_WSJT_RELEASE_CHANNEL "DEVEL")
    endif ()
  endif ()

  if (NOT _WSJT_RELEASE_CHANNEL MATCHES "^(DEVEL|BETA|RC|GA)$")
    message (FATAL_ERROR "WSJT_RELEASE_CHANNEL must be DEVEL, BETA, RC, or GA; got \"${WSJT_RELEASE_CHANNEL}\".")
  endif ()

  set (BUILD_TYPE_REVISION "")
  set (BUILD_TYPE_PACKAGE_REVISION "")
  if (_WSJT_RELEASE_CHANNEL MATCHES "^(BETA|RC)$")
    if (NOT WSJT_PRERELEASE_NUMBER MATCHES "^[1-9][0-9]*$")
      message (FATAL_ERROR "WSJT_PRERELEASE_NUMBER must be a positive integer when WSJT_RELEASE_CHANNEL is ${_WSJT_RELEASE_CHANNEL}.")
    endif ()
    string (TOLOWER "${_WSJT_RELEASE_CHANNEL}${WSJT_PRERELEASE_NUMBER}" _WSJT_PRERELEASE)
    set (BUILD_TYPE_REVISION "-${_WSJT_PRERELEASE}")
    # Debian and RPM order a "~" suffix below the release it precedes.
    set (BUILD_TYPE_PACKAGE_REVISION "~${_WSJT_PRERELEASE}")
  elseif (NOT WSJT_PRERELEASE_NUMBER STREQUAL "")
    message (FATAL_ERROR "WSJT_PRERELEASE_NUMBER must be empty when WSJT_RELEASE_CHANNEL is ${_WSJT_RELEASE_CHANNEL}.")
  elseif (_WSJT_RELEASE_CHANNEL STREQUAL "DEVEL")
    set (BUILD_TYPE_REVISION "-devel")
    set (BUILD_TYPE_PACKAGE_REVISION "-devel")
  endif ()
  set (WSJT_RELEASE_CHANNEL "${_WSJT_RELEASE_CHANNEL}" CACHE STRING "Build release channel: DEVEL, BETA, RC, or GA." FORCE)
  message (STATUS "Building ${PROJECT_NAME} v${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}${BUILD_TYPE_REVISION}")
endmacro ()

function (wsjt_prerelease_expiry channel source_dir expiry_out)
  if (channel STREQUAL "GA")
    set (${expiry_out} 0 PARENT_SCOPE)
    return ()
  endif ()
  set (build_epoch "$ENV{SOURCE_DATE_EPOCH}")
  if (NOT build_epoch MATCHES "^[0-9]*$")
    message (FATAL_ERROR "SOURCE_DATE_EPOCH must be a Unix time; got \"${build_epoch}\".")
  endif ()
  if (build_epoch STREQUAL "" AND GIT_EXECUTABLE AND EXISTS "${source_dir}/.git")
    execute_process (
      COMMAND "${GIT_EXECUTABLE}" -c "safe.directory=${source_dir}" -C "${source_dir}" log -1 --format=%ct
      OUTPUT_VARIABLE build_epoch
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET)
  endif ()
  if (NOT build_epoch MATCHES "^[0-9]+$" AND ARGC GREATER 3)
    set (build_epoch "${ARGV3}")
  elseif (NOT build_epoch MATCHES "^[0-9]+$")
    string (TIMESTAMP build_epoch "%s" UTC)
  endif ()
  # The last second (UTC) of the 90th day after the build date.
  math (EXPR expiry "(${build_epoch} / 86400 + 91) * 86400 - 1")
  set (${expiry_out} "${expiry}" PARENT_SCOPE)
endfunction ()
