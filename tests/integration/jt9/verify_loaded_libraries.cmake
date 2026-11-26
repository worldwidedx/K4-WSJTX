# Fails if EXECUTABLE loads Boost, Qt or ICU, which the release tarballs'
# decoder programs use no symbol from: they load them only when linked
# without --as-needed (Linux) or -dead_strip_dylibs (macOS).
foreach (required_variable LISTER EXECUTABLE)
  if (NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message (FATAL_ERROR "${required_variable} is required")
  endif ()
endforeach ()

if (LISTER MATCHES "otool$")
  set (lister_arguments -L)
else ()
  set (lister_arguments -dW)
endif ()
execute_process (
  COMMAND "${LISTER}" ${lister_arguments} "${EXECUTABLE}"
  RESULT_VARIABLE lister_result
  OUTPUT_VARIABLE lister_stdout
  ERROR_VARIABLE lister_stderr)
if (NOT "${lister_result}" STREQUAL "0")
  message (FATAL_ERROR
    "${LISTER} exited with ${lister_result}\n"
    "stdout:\n${lister_stdout}\n"
    "stderr:\n${lister_stderr}")
endif ()

if (LISTER MATCHES "otool$")
  # The first line names the file itself.
  string (FIND "${lister_stdout}" "\n" first_newline)
  string (SUBSTRING "${lister_stdout}" ${first_newline} -1 libraries)
else ()
  string (REGEX MATCHALL "\\(NEEDED\\)[^\n]*" libraries "${lister_stdout}")
  string (REPLACE ";" "\n" libraries "${libraries}")
endif ()
string (REGEX MATCHALL "[^\n]*(boost|QtCore|Qt5Core|libicu)[^\n]*" unused "${libraries}")
if (unused)
  string (REPLACE ";" "\n" unused "${unused}")
  message (FATAL_ERROR "${EXECUTABLE} loads libraries it uses no symbol from:\n${unused}")
endif ()
