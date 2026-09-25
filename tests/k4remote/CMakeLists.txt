cmake_minimum_required(VERSION 3.21)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(k4_remote_tests LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(TARGET Qt5::Core)
  set(QT_VERSION_MAJOR 5)
else()
  find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Test)
endif()
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network Test)

add_executable(k4_remote_tests
  tst_k4remote.cpp
  ../../Transceiver/K4RemoteProtocol.cpp
  ../../Transceiver/K4RemoteAudioCodec.cpp
  ../../Transceiver/K4RemoteTxGuard.cpp)
target_include_directories(k4_remote_tests PRIVATE ../.. ../../Transceiver)
target_link_libraries(k4_remote_tests PRIVATE
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network
  Qt${QT_VERSION_MAJOR}::Test)

# Compile the complete transceiver object as part of the fast protocol suite.
# Linking it requires the full WSJT-X/Fortran application, but compiling here
# catches Qt socket, TLS, CAT, and generator regressions on developer machines.
add_library(k4_remote_transceiver_compile OBJECT
  ../../Transceiver/K4RemoteTransceiver.cpp
  ../../Transceiver/TransceiverFactory.cpp)
target_include_directories(k4_remote_transceiver_compile PRIVATE stubs ../.. ../../Transceiver)
target_link_libraries(k4_remote_transceiver_compile PRIVATE
  Qt${QT_VERSION_MAJOR}::Core
  Qt${QT_VERSION_MAJOR}::Network)

enable_testing()
add_test(NAME k4_remote_tests COMMAND k4_remote_tests)

add_executable(k4_remote_transport_tests
  tst_k4transport.cpp
  ../../Transceiver/Transceiver.cpp
  ../../Transceiver/TransceiverBase.cpp
  ../../Transceiver/PollingTransceiver.cpp
  ../../Transceiver/K4RemoteTransceiver.cpp
  ../../Transceiver/K4RemoteProtocol.cpp
  ../../Transceiver/K4RemoteAudioCodec.cpp
  ../../Transceiver/K4RemoteTxGuard.cpp)
target_include_directories(k4_remote_transport_tests PRIVATE stubs ../.. ../../Transceiver)
target_link_libraries(k4_remote_transport_tests PRIVATE
  Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network Qt${QT_VERSION_MAJOR}::Test)
add_test(NAME k4_remote_transport_tests COMMAND k4_remote_transport_tests)
set_tests_properties(k4_remote_tests k4_remote_transport_tests PROPERTIES
  TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

if(WIN32)
  get_target_property(_qt_core Qt${QT_VERSION_MAJOR}::Core IMPORTED_LOCATION_RELEASE)
  get_filename_component(_qt_bin "${_qt_core}" DIRECTORY)
  find_file(K4_TEST_OPUS_DLL NAMES libopus-0.dll opus.dll HINTS "${_qt_bin}")
  if(NOT K4_TEST_OPUS_DLL)
    message(FATAL_ERROR "K4 tests require the packaged Xiph Opus DLL")
  endif()
  foreach(_target k4_remote_tests k4_remote_transport_tests)
    add_custom_command(TARGET ${_target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E
      copy_if_different "${K4_TEST_OPUS_DLL}" "$<TARGET_FILE_DIR:${_target}>")
  endforeach()
endif()
