cmake_minimum_required (VERSION 3.12)

function (collect_tag_sources directory)
  file (GLOB entries RELATIVE "${SOURCE_DIR}" "${directory}/*")
  set (sources)
  foreach (entry IN LISTS entries)
    set (path "${SOURCE_DIR}/${entry}")
    if (IS_SYMLINK "${path}")
      continue ()
    endif ()
    if (IS_DIRECTORY "${path}")
      if (entry MATCHES "(^|/)(\\.[^/]*|CMakeFiles)$" OR
          path STREQUAL BINARY_DIR OR EXISTS "${path}/CMakeCache.txt")
        continue ()
      endif ()
      collect_tag_sources ("${path}")
      list (APPEND sources ${tag_sources})
    else ()
      string (TOLOWER "${entry}" source_name)
      if (source_name MATCHES "\\.(c|cc|cpp|cxx|h|hh|hpp|hxx|f|for|f90|f95|f03|f08|inc)$")
        list (APPEND sources "${entry}")
      endif ()
    endif ()
  endforeach ()
  set (tag_sources "${sources}" PARENT_SCOPE)
endfunction ()

collect_tag_sources ("${SOURCE_DIR}")
string (REPLACE ";" "\n" tag_sources "${tag_sources}")
set (source_list "${BINARY_DIR}/CMakeFiles/${TAG_FILE}-sources.txt")
file (WRITE "${source_list}" "${tag_sources}\n")

if (TAG_TOOL_KIND STREQUAL "emacs")
  set (options -)
else ()
  set (options -L -)
  if (TAG_FILE STREQUAL "TAGS")
    set (options -e ${options})
  endif ()
endif ()
# A file list on stdin avoids command-line length limits on Windows.
execute_process (
  COMMAND "${TAG_TOOL}" -f "${TAG_FILE}" ${options}
  INPUT_FILE "${source_list}"
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE result
  )
if (NOT result STREQUAL "0")
  message (FATAL_ERROR "Failed to generate ${TAG_FILE}: ${result}")
endif ()
