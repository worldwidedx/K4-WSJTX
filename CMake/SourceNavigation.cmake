find_program (CTAGS NAMES universal-ctags ctags-universal exuberant-ctags ctags-exuberant ctags)
find_program (ETAGS NAMES etags etags.emacs)

foreach (_tag_target IN ITEMS ctags etags)
  string (TOUPPER "${_tag_target}" _tag_variable)
  if (NOT ${_tag_variable})
    continue ()
  endif ()

  execute_process (
    COMMAND "${${_tag_variable}}" --version
    RESULT_VARIABLE _tag_result
    OUTPUT_VARIABLE _tag_version
    ERROR_QUIET
    )
  if (_tag_result STREQUAL "0" AND _tag_version MATCHES "(Universal|Exuberant) Ctags")
    set (_tag_tool_kind ctags)
  elseif (_tag_result STREQUAL "0" AND _tag_target STREQUAL "etags" AND _tag_version MATCHES "GNU Emacs")
    set (_tag_tool_kind emacs)
  else ()
    message (STATUS "Skipping ${_tag_target} target: unsupported tool ${${_tag_variable}}")
    continue ()
  endif ()

  if (_tag_target STREQUAL "ctags")
    set (_tag_file tags)
  else ()
    set (_tag_file TAGS)
  endif ()
  add_custom_target (${_tag_target}
    COMMAND "${CMAKE_COMMAND}"
      "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}"
      "-DTAG_TOOL=${${_tag_variable}}"
      "-DTAG_TOOL_KIND=${_tag_tool_kind}"
      "-DTAG_FILE=${_tag_file}"
      -P "${CMAKE_CURRENT_LIST_DIR}/generate_tags.cmake"
    COMMENT "Generating ${_tag_file} for source navigation"
    VERBATIM
    )
endforeach ()
