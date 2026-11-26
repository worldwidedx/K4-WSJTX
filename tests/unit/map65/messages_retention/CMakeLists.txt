# Run the production display() against a stand-in stdout channel that
# records the "@" (Messages window) lines instead of sending them to the GUI.
# Keep replacement module files out of the production module directory.
add_executable (test_map65_messages_retention
  messages_retention_stubs.f90
  test_map65_messages_retention.f90
  ${CMAKE_SOURCE_DIR}/map65/libm65/message_history_mod.f90
  ${CMAKE_SOURCE_DIR}/map65/libm65/display.f90)
target_include_directories (test_map65_messages_retention BEFORE PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROJECT_BINARY_DIR}/map65/libm65)
target_link_libraries (test_map65_messages_retention PRIVATE m65impl)
target_compile_options (test_map65_messages_retention PRIVATE
  $<$<COMPILE_LANG_AND_ID:Fortran,GNU>:-fcheck=bounds>)
add_test (NAME test_map65_messages_retention COMMAND test_map65_messages_retention)
set_tests_properties (test_map65_messages_retention PROPERTIES
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} TIMEOUT 60)
