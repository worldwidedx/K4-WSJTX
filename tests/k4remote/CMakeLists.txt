cmake_minimum_required(VERSION 3.21)
project(k4_remote_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network Test)
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
