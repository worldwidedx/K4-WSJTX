cmake_minimum_required (VERSION 3.12 FATAL_ERROR)

if (APPLE)
  #
  # The following variables define the portability and compatability attributes of the Mac macOS build
  # they are choosen with care and should not be changed without good cause.
  #
  # Among other things these options are chosen to match the portability and compatability options of the
  # Qt framework dylibs which can be checked as follows:
  #
  # otool -l <binary> | grep -A3 LC_VERSION_MIN_MACOSX
  #
  if (NOT CMAKE_OSX_SYSROOT)
    execute_process (COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    set (CMAKE_OSX_SYSROOT ${_SDK_PATH} CACHE STRING "Path to macOS SDK")
  endif ()

  # CI and release builds pass this explicitly. For local macOS builds, choose
  # the default deployment target from the target architecture.
  if (NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    if (DEFINED ENV{MACOSX_DEPLOYMENT_TARGET} AND NOT "$ENV{MACOSX_DEPLOYMENT_TARGET}" STREQUAL "")
      set (_WSJTX_MACOS_DEPLOYMENT_TARGET "$ENV{MACOSX_DEPLOYMENT_TARGET}")
    else ()
      set (_WSJTX_MACOS_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}")
      if (NOT _WSJTX_MACOS_ARCHITECTURES)
        execute_process (COMMAND uname -m
          OUTPUT_VARIABLE _WSJTX_MACOS_ARCHITECTURES
          OUTPUT_STRIP_TRAILING_WHITESPACE)
      endif ()

      if (_WSJTX_MACOS_ARCHITECTURES MATCHES "(^|;)arm64($|;)")
        set (_WSJTX_MACOS_DEPLOYMENT_TARGET 11.0)
      else ()
        set (_WSJTX_MACOS_DEPLOYMENT_TARGET 10.13)
      endif ()
    endif ()

    set (CMAKE_OSX_DEPLOYMENT_TARGET "${_WSJTX_MACOS_DEPLOYMENT_TARGET}"
      CACHE STRING "Earliest macOS version supported by the target architecture")
  endif ()

  set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-headerpad_max_install_names")
endif (APPLE)

if(APPLE)
    add_compile_definitions(FORTRAN_APPLE)
endif()

#
# CMake policies
#
if (POLICY CMP0020)
  cmake_policy (SET CMP0020 NEW) # link to Qt winmain on Windows
endif ()

if (POLICY CMP0043)
  cmake_policy (SET CMP0043 NEW) # ignore COMPILE_DEFINITIONS_<CONFIG>
endif ()

if (POLICY CMP0048)
  cmake_policy (SET CMP0048 NEW) # clear PROJECT_Version_* variables if not set in project() command
endif ()

if (POLICY CMP0063)
  cmake_policy (SET CMP0063 NEW) # honour visibility properties for all library types
endif ()

if (POLICY CMP0071)
  cmake_policy (SET CMP0071 NEW) # run automoc and autouic on generated sources
endif ()

if (POLICY CMP0075)
  cmake_policy (SET CMP0075 NEW) # honour CMAKE_REQUIRED_LIBRARIES in config checks
endif ()

include ("${CMAKE_CURRENT_LIST_DIR}/CMake/Modules/read_release_state.cmake")
wsjt_read_release_state (
  "${CMAKE_CURRENT_LIST_DIR}/release-state.txt"
  WSJT_SOURCE_VERSION
  WSJT_SOURCE_RELEASE_CHANNEL
  WSJT_SOURCE_RC_NUMBER
  WSJT_ARCHIVE_SOURCE_REVISION)

project (wsjtx
  VERSION ${WSJT_SOURCE_VERSION}.0
  LANGUAGES C CXX Fortran
  )
set (PROJECT_DESCRIPTION "K4 WSJT-X: WSJT-X with Elecraft K4 remote CAT and audio")
set (CMAKE_PROJECT_DESCRIPTION ${PROJECT_DESCRIPTION})
set (CMAKE_AUTOMOC OFF)
set (CMAKE_AUTOUIC OFF)
set (CMAKE_AUTORCC OFF)

function (wsjt_enable_qt_automoc target)
  set_target_properties (${target} PROPERTIES AUTOMOC ON)
endfunction ()

# Align windres with the active MinGW gcc to prevent MSYS path mangling
if(MINGW)
  # Derive windres.exe from the C compiler's directory (Qt MinGW toolchain)
  get_filename_component(_CC_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
  find_program(WINDRES_EXECUTABLE NAMES windres.exe windres PATHS "${_CC_DIR}" NO_DEFAULT_PATH)
  if(NOT WINDRES_EXECUTABLE)
    # Fallback to normal PATH if not found beside gcc
    find_program(WINDRES_EXECUTABLE NAMES windres.exe windres)
  endif()
  if(NOT WINDRES_EXECUTABLE)
    message(FATAL_ERROR "windres not found; ensure MinGW bin dir is on PATH")
  endif()

  # Use MinGW windres directly; feed it forward-slash paths; avoid bash and MSYS gcc
  set(CMAKE_RC_COMPILER "${WINDRES_EXECUTABLE}")
  set(CMAKE_RC_COMPILE_OBJECT
    "<CMAKE_COMMAND> -E env MSYS2_ARG_CONV_EXCL='*' \"<CMAKE_RC_COMPILER>\" -O coff <DEFINES> <INCLUDES> <FLAGS> \"<SOURCE>\" \"<OBJECT>\""
  )
endif()

#
# Local CMake modules and support files
#
set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake/Modules ${CMAKE_MODULE_PATH})

set (PROJECT_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
if (NOT PROJECT_ARCHITECTURE)
  # This is supposed to happen already on Windows
  if (CMAKE_SIZEOF_VOID_P MATCHES 8)
    set (PROJECT_ARCHITECTURE "x64")
  else ()
    set (PROJECT_ARCHITECTURE "$ENV{PROCESSOR_ARCHITECTURE}")
  endif ()
endif ()
message (STATUS "******************************************************")
message (STATUS "Building for for: ${CMAKE_SYSTEM_NAME}-${PROJECT_ARCHITECTURE}")
message (STATUS "******************************************************")

include (set_build_type)
wsjt_set_source_default_cache (WSJT_RELEASE_CHANNEL "${WSJT_SOURCE_RELEASE_CHANNEL}" "Build release channel: DEVEL, RC, or GA.")
wsjt_set_source_default_cache (WSJT_RC_NUMBER "${WSJT_SOURCE_RC_NUMBER}" "Release candidate number used when WSJT_RELEASE_CHANNEL is RC.")
set_build_type ()
set (wsjtx_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}${BUILD_TYPE_REVISION}")

#
# project information
#
set (PROJECT_BUNDLE_NAME "K4 WSJT-X")
set (PROJECT_VENDOR "WSJT Development Group")
set (PROJECT_CONTACT "jtaylor@princeton.edu")
set (PROJECT_COPYRIGHT "Copyright (C) 2001-2026 by Joe Taylor, K1JT")
set (PROJECT_HOMEPAGE https://wsjtx.github.io/wsjtx/)
set (PROJECT_MANUAL wsjtx-main)
set (PROJECT_ONLINE_MANUAL_URL https://wsjtx.github.io/wsjtx/guide-full.html)
set (PROJECT_SAMPLES_URL http://downloads.sourceforge.net/project/wsjt/)
set (PROJECT_SAMPLES_UPLOAD_DEST frs.sourceforge.net:/home/frs/project/wsjt/)

# make sure that the default configuration is a RELEASE
if (NOT CMAKE_BUILD_TYPE)
  set (CMAKE_BUILD_TYPE RELEASE CACHE STRING
    "Choose the type of build, options are: None Debug Release."
    FORCE)
endif (NOT CMAKE_BUILD_TYPE)
if (CMAKE_BUILD_TYPE MATCHES "[Dd][Ee][Bb][Uu][Gg]")
  set (is_debug_build 1)
endif ()

#uncomment line below to test for Qt 5.15.2 and later deprecations
#add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050F00)

#
# Options & features
#
#	Some of these directly effect compilation by being defined in
#	wsjtx_config.h.in which makes them available to the C/C++
#	pre-processor.
#
include (CMakeDependentOption)

# Allow the developer to select if Dynamic or Static libraries are built
OPTION (BUILD_SHARED_LIBS "Build Shared Libraries" OFF)
# Set the LIB_TYPE variable to STATIC
SET (LIB_TYPE STATIC)
wsjt_set_source_default_cache (WSJT_SOURCE_REVISION "${WSJT_ARCHIVE_SOURCE_REVISION}" "Source revision associated with this build")
if (WSJT_SOURCE_REVISION)
  string (LENGTH "${WSJT_SOURCE_REVISION}" _WSJT_SOURCE_REVISION_LENGTH)
  if (NOT WSJT_SOURCE_REVISION MATCHES "^[0-9a-fA-F]+$"
      OR (NOT _WSJT_SOURCE_REVISION_LENGTH EQUAL 40
          AND NOT _WSJT_SOURCE_REVISION_LENGTH EQUAL 64))
    message (FATAL_ERROR "WSJT_SOURCE_REVISION must be a full Git object ID")
  endif ()
  string (TOLOWER "${WSJT_SOURCE_REVISION}" WSJT_SOURCE_REVISION)
endif ()
if (BUILD_SHARED_LIBS)
  # User wants to build Dynamic Libraries, so change the LIB_TYPE variable to CMake keyword 'SHARED'
  set (LIB_TYPE SHARED)
endif (BUILD_SHARED_LIBS)

option (UPDATE_TRANSLATIONS "Update source translation translations/*.ts
files (WARNING: make clean will delete the source .ts files! Danger!)")
option (WSJT_SHARED_RUNTIME "Debugging option that allows running from a shared Cloud directory.")
option (WSJT_QDEBUG_TO_FILE "Redirect Qt debuging messages to a trace file.")
option (WSJT_TRACE_CAT "Debugging option that turns on CAT diagnostics.")
option (WSJT_TRACE_CAT_POLLS "Debugging option that turns on CAT diagnostics during polling.")
option (WSJT_SOFT_KEYING "Apply a ramp to CW keying envelope to reduce transients." ON)
option (WSJT_SKIP_MANPAGES "Skip *nix manpage generation.")
option (WSJT_GENERATE_DOCS "Generate documentation files." ON)
option (WSJT_RIG_NONE_CAN_SPLIT "Allow split operation with \"None\" as rig.")
option (WSJT_TRACE_UDP "Debugging option that turns on UDP message protocol diagnostics.")
option (WSJT_BUILD_UTILS "Build simulators and code demonstrators." ON)
option (WSJT_FOX_OTP "Enable Fox OTP Verification Messages." ON)
option (WSJT_SKIP_BUNDLE_FIXUP "Skip install-time bundle dependency fixup." OFF)
option (WSJT_MACOS_INSTALL_TIME_FIXUP
  "Run legacy install-time BundleUtilities fixup for macOS bundles." OFF)
option (WSJT_SKIP_QMAP "Skip building QMAP." OFF)
option (WSJT_ENABLE_TESTS "Enable developer tests." ON)
option (WSJT_ENABLE_GUI_SMOKE_TESTS
  "Register GUI-driven smoke tests with CTest." OFF)
option (WSJT_ENABLE_ASAN_UBSAN "Instrument in-tree code with AddressSanitizer and UndefinedBehaviorSanitizer." OFF)
option (WSJT_ENABLE_TSAN "Instrument in-tree code with ThreadSanitizer." OFF)
option (WSJT_BUILD_JT9STREAM "Build the Qt-free streaming decoder jt9stream alongside jt9 (uses the jt9a stub, never links fort_qt)." ON)
set (WSJT_FORTRAN_LIBRARY_VARIANTS "AUTO" CACHE STRING
  "Fortran library variants to build: AUTO, BOTH, OPENMP_ONLY, or NON_OPENMP_ONLY.")
set_property (CACHE WSJT_FORTRAN_LIBRARY_VARIANTS PROPERTY STRINGS
  AUTO BOTH OPENMP_ONLY NON_OPENMP_ONLY)
CMAKE_DEPENDENT_OPTION (WSJT_QDEBUG_IN_RELEASE "Leave Qt debugging statements in Release configuration." OFF
  "NOT is_debug_build" OFF)
CMAKE_DEPENDENT_OPTION (WSJT_ENABLE_EXPERIMENTAL_FEATURES "Enable features not fully ready for public releases." ON
  is_debug_build OFF)
CMAKE_DEPENDENT_OPTION (WSJT_CREATE_WINMAIN
  "The wsjtx target is normally built as GUI executable with a WinMain entry point on Windows,
if you want a console application instead then set this option to OFF.

If you just want to see the debug output from the application then the easiest way is to
attach a debugger which will then receive the console output inside its console." ON
  "WIN32" OFF)

#
# install locations
#

if (APPLE)
  set (CMAKE_INSTALL_BINDIR ${CMAKE_PROJECT_NAME}.app/Contents/MacOS)
  set (CMAKE_INSTALL_DATAROOTDIR ${CMAKE_PROJECT_NAME}.app/Contents/Resources)
endif ()

include (GNUInstallDirs)

set (WSJT_DATA_DESTINATION "${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}")

set (PLUGIN_DESTINATION ${CMAKE_INSTALL_LIBDIR}/plugins)
set (QT_CONF_DESTINATION ${CMAKE_INSTALL_BINDIR})
if (WIN32)
  set (PLUGIN_DESTINATION plugins)
elseif (APPLE)
  set (PLUGIN_DESTINATION ${CMAKE_INSTALL_BINDIR}/../PlugIns)
  set (QT_CONF_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR})
endif ()

set (WSJT_PLUGIN_DESTINATION ${PLUGIN_DESTINATION} CACHE PATH "Path for plugins")
set (WSJT_QT_CONF_DESTINATION ${QT_CONF_DESTINATION} CACHE PATH "Path for the qt.conf file")

if (WSJT_FOX_OTP)
  set (wsjt_fox_CXXSRCS
  )
  message (STATUS "Including Fox verification code feature")
endif ()

if (WSJT_ENABLE_TESTS)
     enable_testing ()
     message (STATUS "Including tests")
 endif()

set (wsjt_qt_CXXSRCS
  helper_functions.cpp
  qt_helpers.cpp
  widgets/MessageBox.cpp
  MetaDataRegistry.cpp
  Network/NetworkServerLookup.cpp
  revision_utils.cpp
  L10nLoader.cpp
  WFPalette.cpp
  Radio.cpp
  RadioMetaType.cpp
  NonInheritingProcess.cpp
  models/IARURegions.cpp
  models/Bands.cpp
  models/Modes.cpp
  models/FrequencyList.cpp
  models/StationList.cpp
  widgets/FrequencyLineEdit.cpp
  widgets/FrequencyDeltaLineEdit.cpp
  item_delegates/CandidateKeyFilter.cpp
  item_delegates/ForeignKeyDelegate.cpp
  item_delegates/MessageItemDelegate.cpp
  validators/LiveFrequencyValidator.cpp
  GetUserId.cpp
  Audio/AudioDevice.cpp
  Transceiver/Transceiver.cpp
  Transceiver/TransceiverBase.cpp
  Transceiver/EmulateSplitTransceiver.cpp
  Transceiver/TransceiverFactory.cpp
  Transceiver/PollingTransceiver.cpp
  Transceiver/HamlibTransceiver.cpp
  Transceiver/TCITransceiver.cpp
  Transceiver/K4RemoteProtocol.cpp
  Transceiver/K4RemoteAudioCodec.cpp
  Transceiver/K4RemoteTxGuard.cpp
  Transceiver/K4RemoteTransceiver.cpp
  Transceiver/HRDMessage.cpp
  Transceiver/HRDTransceiver.cpp
  Transceiver/DXLabSuiteCommanderTransceiver.cpp
  Network/NetworkMessage.cpp
  Network/MessageClient.cpp
  widgets/LettersSpinBox.cpp
  widgets/HintedSpinBox.cpp
  widgets/RestrictedSpinBox.cpp
  widgets/HelpTextWindow.cpp
  SampleDownloader.cpp
  SampleDownloader/DirectoryDelegate.cpp
  SampleDownloader/Directory.cpp
  SampleDownloader/FileNode.cpp
  SampleDownloader/RemoteFile.cpp
  DisplayManual.cpp
  MultiSettings.cpp
  validators/MaidenheadLocatorValidator.cpp
  validators/CallsignValidator.cpp
  widgets/SplashScreen.cpp
  EqualizationToolsDialog.cpp
  widgets/DoubleClickablePushButton.cpp
  widgets/DoubleClickableRadioButton.cpp
  mycustomspinbox.cpp
  Network/LotWUsers.cpp
  Network/FileDownload.cpp
  Network/FoxVerifier.cpp
  Network/Cloudlog.cpp
  Network/eqsl.cpp
  models/DecodeHighlightingModel.cpp
  widgets/DecodeHighlightingListView.cpp
  models/FoxLog.cpp
  widgets/AbstractLogWindow.cpp
  widgets/FoxLogWindow.cpp
  widgets/CabrilloLogWindow.cpp
  item_delegates/CallsignDelegate.cpp
  item_delegates/MaidenheadLocatorDelegate.cpp
  item_delegates/FrequencyDelegate.cpp
  item_delegates/FrequencyDeltaDelegate.cpp
  item_delegates/SQLiteDateTimeDelegate.cpp
  models/CabrilloLog.cpp
  logbook/AD1CCty.cpp
  logbook/WorkedBefore.cpp
  logbook/Multiplier.cpp
  Network/NetworkAccessManager.cpp
  widgets/LazyFillComboBox.cpp
  widgets/CheckableItemComboBox.cpp
  widgets/BandComboBox.cpp
  otpgenerator.cpp
  )

if (WSJT_ENABLE_TSAN)
  # A CTest ENVIRONMENT assignment replaces the inherited TSAN_OPTIONS. Build
  # complete per-family suppression files from the cross-cutting baseline so
  # scoped tests retain the same TSan controls and classified Hamlib/Boost
  # suppressions as the rest of the suite.
  set (wsjt_tsan_suppression_directory
    "${CMAKE_BINARY_DIR}/tsan-suppressions")
  file (MAKE_DIRECTORY "${wsjt_tsan_suppression_directory}")
  file (READ "${CMAKE_SOURCE_DIR}/tests/sanitizers/tsan.supp"
    wsjt_tsan_base_suppressions)
  foreach (wsjt_tsan_scope IN ITEMS fortran network gui)
    file (READ
      "${CMAKE_SOURCE_DIR}/tests/sanitizers/tsan-${wsjt_tsan_scope}.supp"
      wsjt_tsan_scope_suppressions)
    file (GENERATE
      OUTPUT "${wsjt_tsan_suppression_directory}/tsan-${wsjt_tsan_scope}.supp"
      CONTENT "${wsjt_tsan_base_suppressions}\n${wsjt_tsan_scope_suppressions}")
    set (wsjt_tsan_${wsjt_tsan_scope}_options
      "halt_on_error=0:second_deadlock_stack=1:suppressions=${wsjt_tsan_suppression_directory}/tsan-${wsjt_tsan_scope}.supp")
  endforeach ()
endif ()

set (wsjt_CXXSRCS
  Logger.cpp
  lib/crc10.cpp
  lib/crc13.cpp
  lib/crc14.cpp
  ${wsjt_fox_CXXSRCS}
  )
# deal with a GCC v6 UB error message
set_source_files_properties (
  lib/crc10.cpp
  lib/crc13.cpp
  lib/crc14.cpp
  PROPERTIES COMPILE_FLAGS -fpermissive)

if (WIN32)
  set (wsjt_CXXSRCS
    ${wsjt_CXXSRCS}
    killbyname.cpp
    )
endif (WIN32)

set (wsjt_FSRCS
  # put module sources first in the hope that they get rebuilt before use
  lib/types.f90
  lib/C_interface_module.f90
  lib/shmem.f90
  lib/crc.f90
  lib/fftw3mod.f90
  lib/hashing.f90
  lib/iso_c_utilities.f90
  lib/jt4.f90
  lib/jt4_decode.f90
  lib/jt65_decode.f90
  lib/jt65_mod.f90
  lib/jt65_mod6.f90 #ft8md
  lib/ft8_decode.f90
  lib/ft4_decode.f90
  lib/fst4_decode.f90
  lib/get_q3list.f90
  lib/jt9_decode.f90
  lib/options.f90
  lib/packjt.f90
  lib/77bit/packjt77.f90
  lib/qra/q65/q65.f90
  lib/q65_decode.f90
  lib/readwav.f90
  lib/timer_C_wrapper.f90
  lib/timer_impl.f90
  lib/timer_module.f90
  lib/wavhdr.f90
  lib/qra/q65/q65_encoding_modules.f90
  lib/ft8/ft8_a7.f90
  lib/ft8/ft8_a8d.f90
  lib/superfox/sfox_mod.f90
  lib/superfox/julian.f90
  lib/superfox/popen_module.f90
  lib/superfox/qpc/qpc_mod.f90
  lib/ft8var/ft8_decodevar.f90

  # remaining non-module sources
  lib/addit.f90
  lib/afc65b.f90
  lib/afc9.f90
  lib/ana64.f90
  lib/ana932.f90
  lib/analytic.f90
  lib/astro.f90
  lib/astrosub.f90
  lib/astro0.f90
  lib/avecho.f90
  lib/averms.f90
  lib/azdist.f90
  lib/ft8/baseline.f90
  lib/ft4/ft4_baseline.f90
  lib/blanker.f90
  lib/bpdecode40.f90
  lib/bpdecode128_90.f90
  lib/ft8/bpdecode174_91.f90
  lib/baddata.f90
  lib/cablog.f90
  lib/calibrate.f90
  lib/ccf2.f90
  lib/ccf65.f90
  lib/ft8/chkcrc13a.f90
  lib/ft8/chkcrc14a.f90
  lib/chkcall.f90
  lib/chkhist.f90
  lib/chkmsg.f90
  lib/chkss2.f90
  lib/ft4/clockit.f90
  lib/ft8/compress.f90
  lib/coord.f90
  lib/db.f90
  lib/decode4.f90
  lib/decode65a.f90
  lib/decode65b.f90
  lib/decode9w.f90
  lib/decode_echo.f90
  lib/ft8/decode174_91.f90
  lib/decoder.f90
  lib/env_module.f90
  lib/deep4.f90
  lib/deg2grid.f90
  lib/degrade_snr.f90
  lib/demod64a.f90
  lib/determ.f90
  lib/downsam9.f90
  lib/echosim.f90
  lib/echo_snr.f90
  lib/encode232.f90
  lib/encode4.f90
  lib/encode_msk40.f90
  lib/encode_128_90.f90
  lib/ft8/encode174_91.f90
  lib/ft8/encode174_91_nocrc.f90
  lib/entail.f90
  lib/ephem.f90
  lib/extract.f90
  lib/extract4.f90
  lib/extractmessage77.f90
  lib/fano232.f90
  lib/fast9.f90
  lib/fast_decode.f90
  lib/fchisq.f90
  lib/fchisq0.f90
  lib/fchisq65.f90
  lib/fil3.f90
  lib/fil3c.f90
  lib/fil4.f90
  lib/fil6521.f90
  lib/filbig.f90
  lib/ft8/filt8.f90
  lib/fitcal.f90
  lib/flat1.f90
  lib/flat1a.f90
  lib/flat1b.f90
  lib/flat2.f90
  lib/flat4.f90
  lib/flat65.f90
  lib/fmtmsg.f90
  lib/foldspec9f.f90
  lib/four2a.f90
  lib/fspread_lorentz.f90
  lib/ft8/foxfilt.f90
  lib/ft8/foxgen.f90
#  lib/ft8/foxgen_wrap.f90
  lib/freqcal.f90
  lib/ft8/ft8apset.f90
  lib/ft8/ft8b.f90
  lib/ft8/ft8code.f90
  lib/ft8/ft8_downsample.f90
  lib/ft8/ft8sim.f90
  lib/gen4.f90
  lib/gen65.f90
  lib/gen9.f90
  lib/gen_cw_wave.f90
  lib/gen_echocall.f90
  lib/genwave.f90
  lib/ft8/genft8.f90
  lib/qra/q65/genq65.f90
  lib/genmsk_128_90.f90
  lib/genmsk40.f90
  lib/ft4/ft4code.f90
  lib/ft4/genft4.f90
  lib/ft4/gen_ft4wave.f90
  lib/ft8/gen_ft8wave.f90
  lib/ft8/genft8refsig.f90
  lib/genwspr.f90
  lib/geodist.f90
  lib/ft8/get_crc14.f90
  lib/getlags.f90
  lib/getmet4.f90
  lib/ft8/get_spectrum_baseline.f90
  lib/ft2/gfsk_pulse.f90
  lib/graycode.f90
  lib/graycode65.f90
  lib/grayline.f90
  lib/grid2deg.f90
  lib/ft8/h1.f90
  lib/hash.f90
  lib/hint65.f90
  lib/hspec.f90
  lib/indexx.f90
  lib/init_random_seed.f90
  lib/interleave4.f90
  lib/interleave63.f90
  lib/interleave9.f90
  lib/inter_wspr.f90
  lib/jplsubs.f
  lib/jt9fano.f90
  lib/libration.f90
  lib/lorentzian.f90
  lib/fst4/lorentzian_fading.f90
  lib/lpf1.f90
  lib/map65_mmdec.f90
  lib/mixlpf.f90
  lib/makepings.f90
  lib/moondopjpl.f90
  lib/morse.f90
  lib/move.f90
  lib/msk40decodeframe.f90
  lib/msk144decodeframe.f90
  lib/msk40spd.f90
  lib/msk144spd.f90
  lib/msk40sync.f90
  lib/msk144sync.f90
  lib/msk40_freq_search.f90
  lib/msk144_freq_search.f90
  lib/mskrtd.f90
  lib/msk144signalquality.f90
  lib/msk144sim.f90
  lib/mskrtd.f90
  lib/nuttal_window.f90
  lib/decode_msk144.f90
  lib/ft4/ft4sim.f90
  lib/ft4/ft4sim_mult.f90
  lib/ft4/ft4_downsample.f90
  lib/77bit/my_hash.f90
  lib/wsprd/osdwspr.f90
  lib/ft8/osd174_91.f90
  lib/osd128_90.f90
  lib/pctile.f90
  lib/peakdt9.f90
  lib/peakup.f90
  lib/plotsave.f90
  lib/platanh.f90
  lib/pltanh.f90
  lib/polyfit.f90
  lib/prog_args.f90
  lib/ps4.f90
  lib/qra/q65/q65_ap.f90
  lib/qra/q65/q65_loops.f90
  lib/qra/q65/q65_set_list.f90
  lib/qra/q65/q65_set_list2.f90
  lib/refspectrum.f90
  lib/savec2.f90
  lib/save_dxbase.f90
  lib/save_echo_params.f90
  lib/sec0.f90
  lib/sec_midn.f90
  lib/setup65.f90
  lib/sh65.f90
  lib/sh65snr.f90
  lib/slasubs.f
  lib/sleep_msec.f90
  lib/slope.f90
  lib/smo.f90
  lib/smo121.f90
  lib/softsym.f90
  lib/softsym9f.f90
  lib/softsym9w.f90
  lib/shell.f90
  lib/spec64.f90
  lib/spec9f.f90
  lib/stdmsg.f90
  lib/subtract65.f90
  lib/ft8/subtractft8.f90
  lib/ft4/subtractft4.f90
  lib/sun.f90
  lib/symspec.f90
  lib/symspec2.f90
  lib/symspec65.f90
  lib/sync4.f90
  lib/sync65.f90
  lib/ft4/getcandidates4.f90
  lib/ft4/get_ft4_bitmetrics.f90
  lib/ft8/sync8.f90
  lib/ft8/sync8d.f90
  lib/ft4/sync4d.f90
  lib/sync9.f90
  lib/sync9f.f90
  lib/sync9w.f90
  lib/test_snr.f90
  lib/timf2.f90
  lib/tweak1.f90
  lib/twkfreq.f90
  lib/ft8/twkfreq1.f90
  lib/twkfreq65.f90
  lib/update_recent_calls.f90
  lib/update_msk40_hasharray.f90
  lib/ft8/watterson.f90
  lib/wav11.f90
  lib/wav12.f90
  lib/xcor.f90
  lib/xcor4.f90
  lib/wqdecode.f90
  lib/wqencode.f90
  lib/wspr_downsample.f90
  lib/zplot9.f90
  lib/fst4/decode240_101.f90
  lib/fst4/decode240_74.f90
  lib/fst4/encode240_101.f90
  lib/fst4/encode240_74.f90
  lib/fst4/fst4sim.f90
  lib/fst4/gen_fst4wave.f90
  lib/fst4/genfst4.f90
  lib/fst4/get_fst4_bitmetrics.f90
  lib/fst4/get_fst4_bitmetrics2.f90
  lib/fst4/ldpcsim240_101.f90
  lib/fst4/ldpcsim240_74.f90
  lib/fst4/osd240_101.f90
  lib/fst4/osd240_74.f90
  lib/fst4/fastosd240_74.f90
  lib/fst4/get_crc24.f90
  lib/fst4/fst4_baseline.f90
  lib/77bit/hash22calc.f90
  lib/ft8var/agccft8.f90
  lib/ft8var/baddatavar.f90
  lib/ft8var/bpdecode174_91var.f90
  lib/ft8var/chkfalse8var.f90
  lib/ft8var/chkflscallvar.f90
  lib/ft8var/chkgridvar.f90
  lib/ft8var/chklong8.f90
  lib/ft8var/chkspecial8var.f90
  lib/ft8var/cwfilter.f90
  lib/ft8var/datacor.f90
  lib/ft8var/encode174_91var.f90
  lib/ft8var/extract_callvar.f90
  lib/ft8var/filbigvar.f90
  lib/ft8var/fillhashvar.f90
  lib/ft8var/filtersfreevar.f90
  lib/ft8var/four2avar.f90
  lib/ft8var/ft4_mod1.f90
  lib/ft8var/ft8apsetvar.f90
  lib/ft8var/ft8bvar.f90
  lib/ft8var/ft8_downsamplevar.f90
  lib/ft8var/ft8_mod1.f90
  lib/ft8var/ft8mf1var.f90
  lib/ft8var/ft8mfcqvar.f90  
  lib/ft8var/ft8svar.f90
  lib/ft8var/ft8sdvar.f90
  lib/ft8var/ft8sd1var.f90
  lib/ft8var/genft8var.f90
  lib/ft8var/gen_ft8wavevar.f90
  lib/ft8var/genft8sdvar.f90
  lib/ft8var/jt65_mod2var.f90
  lib/ft8var/jt65_mod5.f90
  lib/ft8var/jt65_mod9.f90
  lib/ft8var/msgparservar.f90
  lib/ft8var/osd174_91var.f90
  lib/ft8var/packjt77sdvar.f90
  lib/ft8var/partint.f90
  lib/ft8var/partintft8.f90
  lib/ft8var/rms_augapvar.f90
  lib/ft8var/searchcallsvar.f90
  lib/ft8var/subtractft8var.f90
  lib/ft8var/sync8var.f90
  lib/ft8var/sync8dvar.f90
  lib/ft8var/tone8.f90
  lib/ft8var/tone8myc.f90
  lib/ft8var/tonesdvar.f90
  lib/ft8var/twkfreq1var.f90  
  lib/superfox/foxgen2.f90
  lib/superfox/qpc_decode2.f90
  lib/superfox/qpc_likelihoods2.f90
  lib/superfox/qpc_snr.f90
  lib/superfox/qpc_sync.f90
  lib/superfox/sfox_ana.f90
  lib/superfox/sfox_assemble.f90
  lib/superfox/sfox_demod.f90
  lib/superfox/sfox_pack.f90
  lib/superfox/sfox_remove_ft8.f90
  lib/superfox/sfox_remove_tone.f90
  lib/superfox/sfox_unpack.f90
  lib/superfox/sfox_wave.f90
  lib/superfox/sfox_wave_gfsk.f90
  lib/superfox/sfrx_sub.f90
  lib/superfox/sftx_sub.f90
  lib/superfox/twkfreq2.f90
  lib/superfox/sfox_gen_gfsk.f90
  lib/superfox/ran1.f90
  lib/superfox/sfoxsim.f90
  )

# temporary workaround for a gfortran v7.3 ICE on Fedora 27 64-bit
set_source_files_properties (lib/slasubs.f PROPERTIES COMPILE_FLAGS -O2)

set (ka9q_CSRCS
  lib/ftrsd/decode_rs.c
  lib/ftrsd/encode_rs.c
  lib/ftrsd/init_rs.c
  )
set_source_files_properties (${ka9q_CSRCS} PROPERTIES COMPILE_FLAGS -Wno-sign-compare)

set (qra_CSRCS
  lib/qra/qracodes/qra12_63_64_irr_b.c
  lib/qra/qracodes/qra13_64_64_irr_e.c
  lib/qra/q65/npfwht.c
  lib/qra/q65/pdmath.c
  lib/qra/q65/qracodes.c
  lib/qra/q65/normrnd.c
  lib/qra/q65/qra15_65_64_irr_e23.c
  lib/qra/q65/q65.c
  lib/qra/q65/q65_subs.c
  )

set (wsjt_CSRCS
  ${ka9q_CSRCS}
  lib/ftrsd/ftrsdap.c
  lib/sgran.c
  lib/golay24_table.c
  lib/gran.c
  lib/igray.c
  lib/init_random_seed.c
  lib/ldpc32_table.c
  lib/wsprd/nhash.c
  lib/tab.c
  lib/tmoonsub.c
  lib/usleep.c
  lib/vit213.c
  lib/wisdom.c
  lib/wrapkarn.c
  ${ldpc_CSRCS}
  ${qra_CSRCS}

  lib/superfox/qpc/dbgprintf.c
  lib/superfox/qpc/nhash2.c
  lib/superfox/qpc/np_qpc.c
  lib/superfox/qpc/np_rnd.c
  lib/superfox/qpc/qpc_fwht.c
  lib/superfox/qpc/qpc_n127k50q128.c
  lib/superfox/qpc/qpc_subs.c

  )

set (wsjt_qt_UISRCS
  wf_palette_design_dialog.ui
  widgets/FoxLogWindow.ui
  widgets/CabrilloLogWindow.ui
  )

set (wsprsim_CSRCS
  lib/wsprd/wsprsim.c
  lib/wsprd/wsprsim_utils.c
  lib/wsprd/wsprd_utils.c
  lib/wsprd/fano.c
  lib/wsprd/tab.c
  lib/wsprd/nhash.c
  )

set (wsprd_CSRCS
  lib/wsprd/wsprd.c
  lib/wsprd/wsprsim_utils.c
  lib/wsprd/wsprd_utils.c
  lib/wsprd/fano.c
  lib/wsprd/jelinek.c
  lib/wsprd/tab.c
  lib/wsprd/nhash.c
  lib/init_random_seed.c
  )

set (wsjtx_UISRCS
  widgets/mainwindow.ui
  widgets/about.ui
  widgets/astro.ui
  widgets/colorhighlighting.ui
  widgets/echograph.ui
  widgets/fastgraph.ui
  widgets/messageaveraging.ui
  widgets/activeStations.ui
  widgets/widegraph.ui
  widgets/logqso.ui
  Configuration.ui
  widgets/ExportCabrillo.ui
  widgets/QSYMessage.ui
  widgets/QSYMessageCreator.ui
  widgets/qsymonitor.ui
  )

set (UDP_library_CXXSRCS
  Radio.cpp
  RadioMetaType.cpp
  Network/NetworkMessage.cpp
  UDPExamples/MessageServer.cpp
  )

set (UDP_library_HEADERS
  Radio.hpp
  UDPExamples/MessageServer.hpp
  ${PROJECT_BINARY_DIR}/udp_export.h
  )

set (message_aggregator_CXXSRCS
  UDPExamples/MessageAggregator.cpp
  UDPExamples/MessageAggregatorMainWindow.cpp
  UDPExamples/DecodesModel.cpp
  UDPExamples/BeaconsModel.cpp
  UDPExamples/ClientWidget.cpp
  validators/MaidenheadLocatorValidator.cpp
  )

set (message_aggregator_STYLESHEETS
  UDPExamples/qss/default.qss
  )

set (qcp_CXXSRCS
  qcustomplot-source/qcustomplot.cpp
  )

set (all_CXXSRCS
  ${wsjt_CXXSRCS}
  ${fort_qt_CXXSRCS}
  ${wsjt_qt_CXXSRCS}
  ${wsjt_qtmm_CXXSRCS}
  ${wsjtx_CXXSRCS}
  ${qcp_CXXSRCS}
  )

set (all_C_and_CXXSRCS
  ${wsjt_CSRCS}
  ${wsprsim_CSRCS}
  ${wsprd_CSRCS}
  ${all_CXXSRCS}
  )

set (TOP_LEVEL_RESOURCES
  icons/Darwin/wsjtx.iconset/icon_128x128.png
  contrib/gpl-v3-logo.svg
  artwork/splash.png
  )

set (PALETTE_FILES
  Palettes/Banana.pal
  Palettes/Blue1.pal
  Palettes/Blue2.pal
  Palettes/Blue3.pal
  Palettes/Brown.pal
  Palettes/Cyan1.pal
  Palettes/Cyan2.pal
  Palettes/Cyan3.pal
  Palettes/Default.pal
  Palettes/Digipan.pal
  Palettes/Fldigi.pal
  Palettes/Gray1.pal
  Palettes/Gray2.pal
  Palettes/Green1.pal
  Palettes/Green2.pal
  Palettes/Jungle.pal
  Palettes/Linrad.pal
  Palettes/W9MDB.pal
  Palettes/Negative.pal
  Palettes/Orange.pal
  Palettes/Pink.pal
  Palettes/Rainbow.pal
  Palettes/Scope.pal
  Palettes/Sunburst.pal
  Palettes/VK4BDJ.pal
  Palettes/YL2KF.pal
  Palettes/Yellow1.pal
  Palettes/Yellow2.pal
  Palettes/ZL1FZ.pal
)

if (APPLE)
  set (WSJTX_ICON_FILE ${CMAKE_PROJECT_NAME}.icns)
  set (ICONSRCS
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_16x16.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_16x16@2x.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_32x32.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_32x32@2x.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_128x128.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_128x128@2x.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_256x256.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_256x256@2x.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_512x512.png
    icons/Darwin/${CMAKE_PROJECT_NAME}.iconset/icon_512x512@2x.png
    )
  add_custom_command (
    OUTPUT ${WSJTX_ICON_FILE}
    COMMAND iconutil -c icns --output "${CMAKE_BINARY_DIR}/${WSJTX_ICON_FILE}" "${CMAKE_SOURCE_DIR}/icons/Darwin/${CMAKE_PROJECT_NAME}.iconset"
    DEPENDS ${ICONSRCS}
    COMMENT "Building Icons"
    )
else ()
  set (WSJTX_ICON_FILE icons/windows-icons/wsjtx.ico)
endif (APPLE)

set_source_files_properties (${WSJTX_ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

# suppress intransigent compiler diagnostics
set_source_files_properties (lib/decoder.f90 PROPERTIES COMPILE_FLAGS "-Wno-unused-dummy-argument")
set_source_files_properties (lib/filbig.f90 PROPERTIES COMPILE_FLAGS "-Wno-aliasing")

## disable Qt trace and warning messages from release configurations
# set_property (DIRECTORY APPEND PROPERTY
#  COMPILE_DEFINITIONS $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG_OUTPUT;QT_NO_WARNING_OUTPUT>
#  )

set_property (SOURCE ${all_C_and_CXXSRCS} APPEND_STRING PROPERTY COMPILE_FLAGS " -include wsjtx_config.h")
set_property (SOURCE ${all_C_and_CXXSRCS} APPEND PROPERTY OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/wsjtx_config.h)

#
# decide on platform specifc packing and fixing up
#
if (APPLE)
  set (WSJTX_BUNDLE_VERSION ${wsjtx_VERSION})

  # make sure CMAKE_INSTALL_PREFIX ends in /
  string (LENGTH "${CMAKE_INSTALL_PREFIX}" LEN)
  math (EXPR LEN "${LEN} -1" )
  string (SUBSTRING "${CMAKE_INSTALL_PREFIX}" ${LEN} 1 ENDCH)
  if (NOT "${ENDCH}" STREQUAL "/")
    set (CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}/")
  endif ()
endif (APPLE)


#
# find some useful tools
#
include (CheckTypeSize)
include (CheckCSourceCompiles)
include (CheckIncludeFiles)
include (CheckSymbolExists)
include (generate_version_info)

find_program(CTAGS ctags)
find_program(ETAGS etags)

#
# Platform checks
#
check_include_files ("stdlib.h;stdarg.h;string.h;float.h" STDC_HEADERS)
check_include_files (stdio.h HAVE_STDIO_H)
check_include_files (stdlib.h HAVE_STDLIB_H)
check_include_files (unistd.h HAVE_UNISTD_H)
check_include_files (sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_files (sys/types.h HAVE_SYS_TYPES_H)
check_include_files (fcntl.h HAVE_FCNTL_H)
check_include_files (sys/stat.h HAVE_SYS_STAT_H)
check_include_files ("linux/ppdev.h;linux/parport.h" HAVE_LINUX_PPDEV_H)
check_include_files ("dev/ppbus/ppi.h;dev/ppbus/ppbconf.h" HAVE_DEV_PPBUS_PPI_H)

#
# Standard C Math Library
#
set (LIBM_TEST_SOURCE "#include<math.h>\nfloat f; int main(){sqrt(f);return 0;}")
check_c_source_compiles ("${LIBM_TEST_SOURCE}" HAVE_MATH)
if (HAVE_MATH)
  set (LIBM_LIBRARIES)
else ()
  set (CMAKE_REQUIRED_LIBRARIES m)
  check_c_source_compiles ("${LIBM_TEST_SOURCE}" HAVE_LIBM_MATH)
  unset (CMAKE_REQUIRED_LIBRARIES)
  if (NOT HAVE_LIBM_MATH)
    message (FATAL_ERROR "Unable to use C math library functions")
  endif ()
  set (LIBM_LIBRARIES m)
endif ()

#
# Boost
#
if (WIN32)
  set (Boost_USE_STATIC_LIBS OFF)
endif ()
find_package (Boost 1.62 REQUIRED COMPONENTS log_setup log)

#
# OpenMP
#
find_package (OpenMP)

if (APPLE)
  set (WSJT_MACOS_LIBGOMP_LIBRARY "" CACHE FILEPATH "Path to the static GNU OpenMP runtime archive")
  if (NOT WSJT_MACOS_LIBGOMP_LIBRARY)
    execute_process (
      COMMAND ${CMAKE_Fortran_COMPILER} -print-file-name=libgomp.a
      OUTPUT_VARIABLE WSJT_MACOS_LIBGOMP_LIBRARY
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      )
  endif ()
  if (NOT WSJT_MACOS_LIBGOMP_LIBRARY OR NOT EXISTS "${WSJT_MACOS_LIBGOMP_LIBRARY}")
    message (FATAL_ERROR "Static GNU OpenMP runtime archive not found. Install a GNU Fortran/OpenMP runtime that provides libgomp.a, or set WSJT_MACOS_LIBGOMP_LIBRARY to the archive path. Deployable macOS builds intentionally link libgomp statically instead of bundling a package-manager libgomp dylib.")
  endif ()
endif ()

#
# fftw3 single precision library
#
find_package (FFTW3 COMPONENTS single threads REQUIRED)

#
# hamlib setup
#
find_package (Hamlib REQUIRED)
find_program (RIGCTL_EXE rigctl)
find_program (RIGCTLD_EXE rigctld)
find_program (RIGCTLCOM_EXE rigctlcom)

check_type_size (CACHE_ALL HAMLIB_OLD_CACHING)
check_symbol_exists (rig_set_cache_timeout_ms "hamlib/rig.h" HAVE_HAMLIB_CACHING)

find_package (Usb REQUIRED)

if (WSJT_FOX_OTP)
  add_definitions (-DFOX_OTP)
endif ()

#
# Qt5 setup
#

# Widgets finds its own dependencies.
find_package (Qt5 COMPONENTS Widgets SerialPort Multimedia PrintSupport Sql LinguistTools WebSockets REQUIRED)

if (WIN32)
  add_definitions (-DQT_NEEDS_QTMAIN)
endif (WIN32)

#
# Library building setup
#
include (GenerateExportHeader)
set (CMAKE_CXX_VISIBILITY_PRESET hidden)
set (CMAKE_C_VISIBILITY_PRESET hidden)
set (CMAKE_Fortran_VISIBILITY_PRESET hidden)
set (CMAKE_VISIBILITY_INLINES_HIDDEN ON)
#set (CMAKE_INCLUDE_CURRENT_DIR_IN_INTERFACE ON)


#
# C & C++ setup
#
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -fexceptions -frtti")

# MinGW gcc 16.x (MSYS2 rolled forward 2026-05) raises false-positive
# -Wmaybe-uninitialized on the placement-new'd HRDMessage flexible-array
# struct in Transceiver/HRDTransceiver.cpp. Demote to a warning on Windows
# + gcc 16+ until upstream gcc tightens the analysis or HRDMessage is
# refactored. Scoped narrowly so other platforms keep -Werror discipline.
if (WIN32
    AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
    AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "16.0")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=maybe-uninitialized")
endif ()

if (NOT APPLE)
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pragmas")
  if (${OPENMP_FOUND})
    if (OpenMP_C_FLAGS)
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_C_FLAGS}")
      set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    endif ()
  endif ()
  set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -fdata-sections -ffunction-sections")
  set (CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} -fdata-sections -ffunction-sections")
  set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fdata-sections -ffunction-sections")
  set (CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} -fdata-sections -ffunction-sections")
endif (NOT APPLE)

if (WIN32)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
endif (WIN32)

if (APPLE AND ${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
else ()
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")
  set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --std=gnu++11 -pthread")
endif ()


#
# Fortran setup
#
set (General_FFLAGS "-Wall -Wno-conversion -Wno-c-binding-type -fno-second-underscore")

# FFLAGS depend on the compiler
get_filename_component (Fortran_COMPILER_NAME ${CMAKE_Fortran_COMPILER} NAME)

if (Fortran_COMPILER_NAME MATCHES "gfortran.*")
  # gfortran

  # CMake compiler test is supposed to do this but doesn't yet
  if (CMAKE_OSX_DEPLOYMENT_TARGET)
    set (CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  endif (CMAKE_OSX_DEPLOYMENT_TARGET)
  if (CMAKE_OSX_SYSROOT)
    set (CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -isysroot ${CMAKE_OSX_SYSROOT}")
  endif (CMAKE_OSX_SYSROOT)

  # Add assembler flag to disable executable stack
  if (UNIX AND NOT APPLE AND Fortran_COMPILER_NAME MATCHES "gfortran.*")
     set (CMAKE_Fortran_FLAGS "${CMAKE_Fortran_FLAGS} -Wa,--noexecstack")
  endif()

  set (CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -fbounds-check -funroll-all-loops -fno-f2c -ffpe-summary=invalid,zero,overflow,underflow ${General_FFLAGS}")

  set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} -ggdb -O0 -fbacktrace -fcheck=all -fbounds-check -fno-f2c -ffpe-summary=invalid,zero,overflow,underflow ${General_FFLAGS}")

  # FPE traps currently disabled in Debug configuration builds until
  # we decide if they are meaningful, without these FP instructions
  # run in nonstop mode and do not trap
  #set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} ${CMAKE_Fortran_FLAGS_DEBUG}  -ffpe-trap=invalid,zero,overflow")
  
elseif (Fortran_COMPILER_NAME MATCHES "ifort.*")
  # ifort (untested)
  set (CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -f77rtl ${General_FFLAGS}")
  set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} -f77rtl ${General_FFLAGS}")
elseif (Fortran_COMPILER_NAME MATCHES "g77")
  # g77
  set (CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -funroll-all-loops -fno-f2c -m32 ${General_FFLAGS}")
  set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} -fbounds-check -fno-f2c -m32 ${General_FFLAGS}")

else()
  message ("CMAKE_Fortran_COMPILER full path: ${CMAKE_Fortran_COMPILER}")
  message ("Fortran compiler: ${Fortran_COMPILER_NAME}")
  message ("No optimized Fortran compiler flags are known, we just try -O3...")
  set (CMAKE_Fortran_FLAGS_RELEASE "${CMAKE_Fortran_FLAGS_RELEASE} -O3 ${General_FFLAGS}")
  set (CMAKE_Fortran_FLAGS_DEBUG   "${CMAKE_Fortran_FLAGS_DEBUG} -g -fbacktrace -fcheck=all -fbounds-check ${General_FFLAGS}")
endif()

#
# Linker setup
#
if (NOT APPLE)
  set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
  set (CMAKE_EXE_LINKER_FLAGS_MINSIZEREL "${CMAKE_EXE_LINKER_FLAGS_MINSIZEREL} -Wl,--gc-sections")
endif (NOT APPLE)


#
# setup and test Fortran C/C++ interaction
#

include (FortranCInterface)
FortranCInterface_VERIFY (CXX)
FortranCInterface_HEADER (FC.h MACRO_NAMESPACE "FC_" SYMBOL_NAMESPACE "FC_"
  SYMBOLS
  grayline
  )


#
# sort out pre-requisites
#

#
# Setup RPATH so that built executable targets will run in both the
# build tree and the install location without having to set a
# (DYLD|LD)_LIBRARY_PATH override.
#

# use the full RPATH of the build tree
set (CMAKE_SKIP_BUILD_RPATH FALSE)

# when building, don't use the install RPATH, it will still be used
# later on in the install phase
set (CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

# set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

# add the automaticaly determined parts of the RPATH which point to
# directories outside of the build tree to the install RPATH
set (CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# the RPATH to be used when installing, but only if it's not a system
# directory
# list (FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}" isSystemDir)
# if ("${isSystemDir}" STREQUAL "-1")
#   set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
# endif ("${isSystemDir}" STREQUAL "-1")

set (QT_NEED_RPATH FALSE)
if (NOT "${QT_LIBRARY_DIR}" STREQUAL "/lib" AND NOT "${QT_LIBRARY_DIR}" STREQUAL "/usr/lib" AND NOT "${QT_LIBRARY_DIR}" STREQUAL "/lib64" AND NOT "${QT_LIBRARY_DIR}" STREQUAL "/usr/lib64")
  set (QT_NEED_RPATH TRUE)
endif ()

#
# stuff only qmake can tell us
#
get_target_property (QMAKE_EXECUTABLE Qt5::qmake LOCATION)
get_target_property (LCONVERT_EXECUTABLE Qt5::lconvert LOCATION)
function (QUERY_QMAKE VAR RESULT)
  exec_program (${QMAKE_EXECUTABLE} ARGS "-query ${VAR}" RETURN_VALUE return_code OUTPUT_VARIABLE output)
  if (NOT return_code)
    file (TO_CMAKE_PATH "${output}" output)
    set (${RESULT} ${output} PARENT_SCOPE)
  endif (NOT return_code)
  message (STATUS "Asking qmake for ${RESULT} and got ${output}")
endfunction (QUERY_QMAKE)

query_qmake (QT_INSTALL_PLUGINS QT_PLUGINS_DIR)
query_qmake (QT_INSTALL_TRANSLATIONS QT_TRANSLATIONS_DIR)
query_qmake (QT_INSTALL_IMPORTS QT_IMPORTS_DIR)
query_qmake (QT_HOST_DATA QT_DATA_DIR)
set (QT_MKSPECS_DIR ${QT_DATA_DIR}/mkspecs)

# project definitions
add_definitions (-DQT5 -DCMAKE_BUILD -DBIGSYM=1 -DBOOST_ALL_DYN_LINK)
if (CMAKE_HOST_UNIX)
  add_definitions (-DUNIX)
elseif (CMAKE_HOST_WIN32)
  add_definitions (-DWIN32)
endif ()

#
# sub-directories
#
if (EXISTS ${CMAKE_SOURCE_DIR}/samples AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/samples)
  add_subdirectory (samples)
endif ()
if (WSJT_GENERATE_DOCS)
  add_subdirectory (doc)
endif (WSJT_GENERATE_DOCS)
set (WSJT_OPENMP_FORTRAN_AVAILABLE FALSE)
set (WSJT_OPENMP_FORTRAN_FLAGS "")
# C/C++ OpenMP and Fortran OpenMP can resolve to different runtimes,
# especially on macOS where Apple Clang and GNU Fortran are mixed.
if (OpenMP_Fortran_FOUND)
  set (WSJT_OPENMP_FORTRAN_AVAILABLE TRUE)
  set (WSJT_OPENMP_FORTRAN_FLAGS "${OpenMP_Fortran_FLAGS}")
elseif (CMAKE_Fortran_COMPILER_ID STREQUAL "GNU")
  set (WSJT_OPENMP_FORTRAN_AVAILABLE TRUE)
  set (WSJT_OPENMP_FORTRAN_FLAGS "-fopenmp")
endif ()

string (TOUPPER "${WSJT_FORTRAN_LIBRARY_VARIANTS}" WSJT_FORTRAN_LIBRARY_VARIANTS)
if (NOT WSJT_FORTRAN_LIBRARY_VARIANTS MATCHES "^(AUTO|BOTH|OPENMP_ONLY|NON_OPENMP_ONLY)$")
  message (FATAL_ERROR
    "WSJT_FORTRAN_LIBRARY_VARIANTS must be AUTO, BOTH, OPENMP_ONLY, or NON_OPENMP_ONLY")
endif ()

set (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS ${WSJT_FORTRAN_LIBRARY_VARIANTS})
if (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS STREQUAL "AUTO")
  if (APPLE)
    set (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS BOTH)
  elseif (WSJT_OPENMP_FORTRAN_AVAILABLE)
    set (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS BOTH)
  else ()
    set (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS NON_OPENMP_ONLY)
  endif ()
endif ()

if (APPLE AND WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS STREQUAL "OPENMP_ONLY")
  # The macOS app links the non-OpenMP Fortran library while jt9 carries
  # the GNU OpenMP runtime separately, so OPENMP_ONLY would change the
  # runtime boundary that keeps Apple Clang libomp out of the bundle.
  message (FATAL_ERROR "WSJT_FORTRAN_LIBRARY_VARIANTS=OPENMP_ONLY is not supported on macOS")
endif ()
if (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS MATCHES "^(BOTH|OPENMP_ONLY)$"
    AND NOT WSJT_OPENMP_FORTRAN_AVAILABLE)
  message (FATAL_ERROR
    "WSJT_FORTRAN_LIBRARY_VARIANTS=${WSJT_FORTRAN_LIBRARY_VARIANTS} requires OpenMP support")
endif ()

set (WSJT_BUILD_FORTRAN_NON_OPENMP FALSE)
set (WSJT_BUILD_FORTRAN_OPENMP FALSE)
if (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS MATCHES "^(BOTH|NON_OPENMP_ONLY)$")
  set (WSJT_BUILD_FORTRAN_NON_OPENMP TRUE)
endif ()
if (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS MATCHES "^(BOTH|OPENMP_ONLY)$")
  set (WSJT_BUILD_FORTRAN_OPENMP TRUE)
endif ()

# The concrete Fortran library that non-app consumers (utilities, qmap_impl,
# m65impl, tests) should link. These consumers use the non-OpenMP library when
# it exists and the OpenMP one only when it is the single variant built.
# Consumers link the wsjt_fort_common interface target (created once both
# libraries exist) rather than naming this.
set (WSJT_COMMON_FORT_TARGET wsjt_fort)
if (WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS STREQUAL "OPENMP_ONLY")
  set (WSJT_COMMON_FORT_TARGET wsjt_fort_omp)
endif ()
message (STATUS "Fortran library variants: requested=${WSJT_FORTRAN_LIBRARY_VARIANTS}, effective=${WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS}")
message (STATUS "Common Fortran target: ${WSJT_COMMON_FORT_TARGET}")
message (STATUS "QMAP: skip=${WSJT_SKIP_QMAP}")

# Internal usage requirements for targets whose own Fortran sources contain
# OpenMP directives. Static libraries keep the runtime dependency link-
# transitive without exporting the compile option to their consumers.
if (WSJT_OPENMP_FORTRAN_AVAILABLE)
  add_library (wsjt_fortran_openmp INTERFACE)
  if (TARGET OpenMP::OpenMP_Fortran AND NOT APPLE)
    target_link_libraries (wsjt_fortran_openmp
      INTERFACE OpenMP::OpenMP_Fortran)
  elseif (WSJT_OPENMP_FORTRAN_FLAGS)
    target_compile_options (wsjt_fortran_openmp INTERFACE
      $<$<COMPILE_LANGUAGE:Fortran>:${WSJT_OPENMP_FORTRAN_FLAGS}>)
    if (APPLE)
      target_link_libraries (wsjt_fortran_openmp INTERFACE
        "${WSJT_MACOS_LIBGOMP_LIBRARY}")
    else ()
      target_link_libraries (wsjt_fortran_openmp INTERFACE
        ${WSJT_OPENMP_FORTRAN_FLAGS})
    endif ()
  endif ()
endif ()

# build a library of package functionality (without and optionally with OpenMP support)
add_library (wsjt_cxx STATIC ${wsjt_CSRCS} ${wsjt_CXXSRCS})
target_link_libraries (wsjt_cxx ${LIBM_LIBRARIES} Boost::log_setup ${LIBM_LIBRARIES})

# build Fortran library routines without and optionally with OpenMP support
if (WSJT_BUILD_FORTRAN_NON_OPENMP)
  set (WSJT_FORTRAN_MODULE_DIR ${CMAKE_BINARY_DIR}/fortran_modules)
  file (MAKE_DIRECTORY ${WSJT_FORTRAN_MODULE_DIR})
  add_library (wsjt_fort STATIC ${wsjt_FSRCS})
  set_target_properties (wsjt_fort
    PROPERTIES
    Fortran_MODULE_DIRECTORY ${WSJT_FORTRAN_MODULE_DIR}
    )
  target_include_directories (wsjt_fort INTERFACE
    $<BUILD_INTERFACE:${WSJT_FORTRAN_MODULE_DIR}>
    )
  target_link_libraries (wsjt_fort PUBLIC wsjt_cxx ${FFTW3_LIBRARIES})
endif ()
if (WSJT_BUILD_FORTRAN_OPENMP)
  set (WSJT_FORTRAN_OPENMP_MODULE_DIR ${CMAKE_BINARY_DIR}/fortran_modules_omp)
  file (MAKE_DIRECTORY ${WSJT_FORTRAN_OPENMP_MODULE_DIR})
  add_library (wsjt_fort_omp STATIC ${wsjt_FSRCS})
  set_target_properties (wsjt_fort_omp
    PROPERTIES
    Fortran_MODULE_DIRECTORY ${WSJT_FORTRAN_OPENMP_MODULE_DIR}
    )
  target_include_directories (wsjt_fort_omp INTERFACE
    $<BUILD_INTERFACE:${WSJT_FORTRAN_OPENMP_MODULE_DIR}>
    )
  target_link_libraries (wsjt_fort_omp
    PUBLIC wsjt_cxx ${FFTW3_LIBRARIES}
    PRIVATE wsjt_fortran_openmp)
endif ()

# Naming a missing target in target_link_libraries does not fail at configure
# time — CMake passes -l<name> to the linker and the failure surfaces later as
# a cryptic link error. Fail loudly here instead, naming the cause.
if (NOT TARGET wsjt_fort AND NOT TARGET wsjt_fort_omp)
  message (FATAL_ERROR
    "No Fortran variant was built (WSJT_FORTRAN_LIBRARY_VARIANTS='${WSJT_FORTRAN_LIBRARY_VARIANTS}', "
    "effective='${WSJT_EFFECTIVE_FORTRAN_LIBRARY_VARIANTS}'). This is an internal build "
    "configuration error.")
endif ()
if (NOT TARGET ${WSJT_COMMON_FORT_TARGET})
  message (FATAL_ERROR
    "Selected common Fortran target '${WSJT_COMMON_FORT_TARGET}' was not built. "
    "This is an internal build configuration error.")
endif ()

# Interface target for non-app consumers. The concrete Fortran archive carries
# its module path plus the C/C++ helper archive it calls into, preserving static
# link order without every consumer repeating wsjt_cxx.
add_library (wsjt_fort_common INTERFACE)
target_link_libraries (wsjt_fort_common INTERFACE ${WSJT_COMMON_FORT_TARGET})

function (wsjt_link_common_fortran tgt)
  target_link_libraries (${tgt} PUBLIC wsjt_fort_common)
endfunction ()

function (wsjt_add_simulator_usage_test tgt usage_pattern)
  if (WSJT_ENABLE_TESTS)
    string (TOLOWER "${tgt}" test_target)
    add_test (
      NAME test_${test_target}_usage
      COMMAND $<TARGET_FILE:${tgt}>
    )
    set_tests_properties (test_${test_target}_usage PROPERTIES
      PASS_REGULAR_EXPRESSION "${usage_pattern}"
      TIMEOUT 10
    )
  endif ()
endfunction ()

# Link the OpenMP Fortran variant onto an app target (jt9, wsjtx).
# Per-app stack/heap sizing stays at the call site.
function (wsjt_link_fortran_openmp tgt)
  target_link_libraries (${tgt} wsjt_fort_omp)
  set_target_properties (${tgt} PROPERTIES
    Fortran_MODULE_DIRECTORY ${WSJT_FORTRAN_OPENMP_MODULE_DIR})
endfunction ()

if (WSJT_ENABLE_TESTS AND EXISTS ${CMAKE_SOURCE_DIR}/tests AND IS_DIRECTORY ${CMAKE_SOURCE_DIR}/tests)
  add_subdirectory (tests)
endif ()

include (CMake/Utils.cmake)



add_executable (fmtave lib/fmtave.f90)

add_executable (fcal lib/fcal.f90)

add_executable (fmeasure lib/fmeasure.f90)

# build the wsprd WSPR mode decoder driver
generate_version_info (wsprd_VERSION_RESOURCES
  NAME wsprd
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "WSPR mode decoder"
)
# message(STATUS "RC compiler: ${CMAKE_RC_COMPILER}")
#  message(STATUS "RC rule: ${CMAKE_RC_COMPILE_OBJECT}")
#  message(STATUS "wsprd_VERSION_RESOURCES: ${wsprd_VERSION_RESOURCES}")

add_executable(wsprd
  ${wsprd_CSRCS}
  lib/indexx.f90
  lib/wsprd/osdwspr.f90
)

if(WIN32)
  # Attach the generated rc file; the RC rule above will compile it
  target_sources(wsprd PRIVATE ${wsprd_VERSION_RESOURCES})
endif()


target_include_directories (wsprd PRIVATE ${FFTW3_INCLUDE_DIRS})
target_link_libraries (wsprd ${FFTW3_LIBRARIES} ${LIBM_LIBRARIES})



include_directories (${CMAKE_CURRENT_BINARY_DIR})

# As moc files are generated in the binary dir, tell CMake to always
# look for includes there:
set (CMAKE_INCLUDE_CURRENT_DIR ON)

#
# source navigation
#
set (sources
  ${CMAKE_SOURCE_DIR}/*
  ${CMAKE_SOURCE_DIR}/logbook/*
  ${CMAKE_SOURCE_DIR}/lib/*
  )
add_custom_target (ctags COMMAND ${CTAGS} -o ${CMAKE_SOURCE_DIR}/tags -R ${sources})
add_custom_target (etags COMMAND ${ETAGS} -o ${CMAKE_SOURCE_DIR}/TAGS -R ${sources})


# Qt i18n - always include the country generic if any regional variant is included
set (LANGUAGES
  ca				# Catalan
  da				# Danish
  de				# German
  en				# English (we need this to stop
				# translation loaders loading the
				# second preference UI languge, it
				# doesn't need to be populated)
  en_GB				# English UK
  es				# Spanish
  hu				# Hungarian
  it				# Italian
  ja				# Japanese
  #no				# Norwegian
  pl				# Polish
  #pt				# Portuguese
  ru				# Russian
  #sv				# Swedish
  uk				# Ukrainian
  zh				# Chinese
  zh_TW				# Chinese traditional
  )
foreach (lang_ ${LANGUAGES})
  file (TO_NATIVE_PATH ${CMAKE_SOURCE_DIR}/translations/wsjtx_${lang_}.ts ts_)
  list (APPEND TS_FILES ${ts_})
  set (qt_translations_ "${QT_TRANSLATIONS_DIR}/qtbase_${lang_}.qm")
  if (EXISTS "${qt_translations_}")
    add_custom_command (
      OUTPUT "${CMAKE_BINARY_DIR}/qt_${lang_}.qm"
      COMMAND ${LCONVERT_EXECUTABLE} -o "${CMAKE_BINARY_DIR}/qt_${lang_}.qm" ${qt_translations_}
      COMMENT "Building required Qt translations for language ${lang_}"
      )
    list (APPEND QM_FILES "${CMAKE_BINARY_DIR}/qt_${lang_}.qm")
  endif ()
endforeach ()
if (UPDATE_TRANSLATIONS)
  message (STATUS "UPDATE_TRANSLATIONS option is set.")
  qt5_create_translation (
    QM_FILES ${wsjt_qt_UISRCS} ${wsjtx_UISRCS} ${wsjt_qt_CXXSRCS} ${wsjtx_CXXSRCS}
    ${TS_FILES}
    OPTIONS -I${CMAKE_CURRENT_SOURCE_DIR}
    )
else ()
  qt5_add_translation (QM_FILES ${TS_FILES})
endif ()
add_custom_target (translations DEPENDS ${QM_FILES})
set_property (DIRECTORY PROPERTY CLEAN_NO_CUSTOM TRUE)

# embedded resources
function (add_resources resources path)
  foreach (resource_file_ ${ARGN})
    get_filename_component (name_ ${resource_file_} NAME)
    if (IS_ABSOLUTE "${resource_file_}")
      file (TO_NATIVE_PATH ${resource_file_} source_)
    else ()
      file (TO_NATIVE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/${resource_file_} source_)
    endif ()
    file (TO_NATIVE_PATH ${path}/${name_} dest_)
    set (resources_ "${resources_}\n    <file alias=\"${dest_}\">${source_}</file>")
    set (${resources} ${${resources}}${resources_} PARENT_SCOPE)
  endforeach (resource_file_ ${ARGN})
endfunction (add_resources resources path)

add_resources (wsjtx_RESOURCES "" ${TOP_LEVEL_RESOURCES})
add_resources (wsjtx_RESOURCES /Palettes ${PALETTE_FILES})
add_resources (wsjtx_RESOURCES /Translations ${QM_FILES})

configure_file (wsjtx.qrc.in wsjtx.qrc @ONLY)

# UI generation
qt5_wrap_ui (wsjt_qt_GENUISRCS ${wsjt_qt_UISRCS})
qt5_wrap_ui (wsjtx_GENUISRCS ${wsjtx_UISRCS})

# Resource generation
qt5_add_resources (wsjtx_RESOURCES_RCC
  ${CMAKE_BINARY_DIR}/wsjtx.qrc
  contrib/QDarkStyleSheet/qdarkstyle/style.qrc
  )

#
# targets dependent on Qt
#

# build a library for the QCustomPlot widget
add_library (qcp STATIC ${qcp_CXXSRCS})
wsjt_enable_qt_automoc (qcp)
target_include_directories (qcp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/qcustomplot-source>)
target_link_libraries (qcp Qt5::Widgets Qt5::PrintSupport)

# build a library of package Qt functionality
add_library (wsjt_qt STATIC ${wsjt_qt_CXXSRCS} ${wsjt_qt_GENUISRCS} ${GENAXSRCS})
wsjt_enable_qt_automoc (wsjt_qt)
# set wsjtx_udp exports to static variants
target_compile_definitions (wsjt_qt PUBLIC UDP_STATIC_DEFINE)
target_link_libraries (wsjt_qt Hamlib::Hamlib Boost::log qcp Qt5::Widgets Qt5::Network Qt5::Sql Qt5::WebSockets)

# build a library of package Qt functionality used in Fortran utilities
add_library (fort_qt STATIC ${fort_qt_CXXSRCS})
target_link_libraries (fort_qt PRIVATE Qt5::Core)

# build a library of WSJT Qt multimedia components
add_library (wsjt_qtmm STATIC ${wsjt_qtmm_CXXSRCS} ${wsjt_qtmm_GENUISRCS})
wsjt_enable_qt_automoc (wsjt_qtmm)
target_link_libraries (wsjt_qtmm Qt5::Multimedia)

# build the jt9 slow mode decoder driver
generate_version_info (jt9_VERSION_RESOURCES
  NAME jt9
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "jt9 - WSJT-X slow mode decoder"
  )

add_executable (record_time_signal Audio/tools/record_time_signal.cpp)
wsjt_enable_qt_automoc (record_time_signal)
target_link_libraries (record_time_signal wsjt_cxx wsjt_qtmm wsjt_qt)

add_executable (jt9 ${jt9_FSRCS} ${jt9_VERSION_RESOURCES})

if (TARGET test_streaming_consumers)
  add_dependencies (test_streaming_consumers jt9 wsprd)
endif ()

if (WSJT_BUILD_UTILS)
  add_executable (ft8sim lib/ft8/ft8sim.f90 lib/print_version.c ${jt9_VERSION_RESOURCES})
  wsjt_link_common_fortran (ft8sim)
  wsjt_add_simulator_usage_test (ft8sim "Usage:[ ]+ft8sim")
endif ()

if (WSJT_ENABLE_TESTS)
  add_test (NAME test_jt9_version COMMAND $<TARGET_FILE:jt9> --version)
  set_tests_properties (test_jt9_version PROPERTIES
    PASS_REGULAR_EXPRESSION "^jt9 [0-9]+\\.[0-9]+\\.[0-9]+[^ ]* \\(revision ([0-9a-f]+(-dirty)?|unavailable)\\)"
    )
  add_test (NAME test_jt9_cli_validation
    COMMAND ${CMAKE_COMMAND} -DJT9=$<TARGET_FILE:jt9>
            -P ${CMAKE_SOURCE_DIR}/tests/unit/jt9/test_jt9_cli_validation.cmake)

  # Golden output is pinned to the required GNU/Linux x86-64 decoder path.
  if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
      CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    # mode_args is one or more jt9 flags (e.g. -8, or --q65 -p 30 -f 1000 -d 3).
    # $<SEMICOLON> protects the list separators through the single -DMODE=...
    # COMMAND argument; compare_decoder_output.cmake expands ${MODE} unquoted
    # so it splits back into separate jt9 arguments on the far side.
    function (wsjt_add_jt9_golden_test test_name sample expected)
      set (mode_args ${ARGN})
      string (REPLACE ";" "$<SEMICOLON>" mode_args "${mode_args}")
      set (work_dir "${CMAKE_BINARY_DIR}/Testing/Golden/${test_name}")
      file (MAKE_DIRECTORY "${work_dir}")
      add_test (NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
          -DJT9=$<TARGET_FILE:jt9>
          "-DMODE=${mode_args}"
          -DSAMPLE=${CMAKE_SOURCE_DIR}/${sample}
          -DEXPECTED=${CMAKE_SOURCE_DIR}/${expected}
          -DWORK_DIR=${work_dir}
          -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
          -P ${CMAKE_SOURCE_DIR}/tests/integration/jt9/compare_decoder_output.cmake)
      set_tests_properties (${test_name} PROPERTIES
        LABELS "decoder;golden;integration"
        TIMEOUT 120)
      if (WSJT_ENABLE_TSAN)
        set_tests_properties (${test_name} PROPERTIES
          ENVIRONMENT "TSAN_OPTIONS=${wsjt_tsan_fortran_options}")
      endif ()
    endfunction ()

    wsjt_add_jt9_golden_test (
      test_jt9_golden_ft8_210703_133430
      samples/FT8/210703_133430.wav
      test/fixtures/ft8_210703_133430.expected.txt
      -8)
    if (WSJT_BUILD_FORTRAN_OPENMP)
      function (wsjt_add_jt9_mtd_golden_test test_name processors mtd_workers
          decode_cycles fft_threads)
        set (options CONSECUTIVE_PERIODS)
        set (one_value_args MY_CALL DX_CALL QSO_FREQUENCY QSO_PROGRESS)
        cmake_parse_arguments (MTD_TEST "${options}" "${one_value_args}" "" ${ARGN})
        set (work_dir "${CMAKE_BINARY_DIR}/Testing/Golden/${test_name}")
        file (MAKE_DIRECTORY "${work_dir}")
        add_test (NAME ${test_name}
          COMMAND ${CMAKE_COMMAND}
            -DJT9=$<TARGET_FILE:jt9>
            -DSAMPLE=${CMAKE_SOURCE_DIR}/samples/FT8/210703_133430.wav
            -DEXPECTED=${CMAKE_SOURCE_DIR}/test/fixtures/ft8_210703_133430_mtd.expected.txt
            -DWORK_DIR=${work_dir}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DMTD_WORKERS=${mtd_workers}
            -DDECODE_CYCLES=${decode_cycles}
            -DFFT_THREADS=${fft_threads}
            -DCONSECUTIVE_PERIODS=${MTD_TEST_CONSECUTIVE_PERIODS}
            -DMY_CALL=${MTD_TEST_MY_CALL}
            -DDX_CALL=${MTD_TEST_DX_CALL}
            -DQSO_FREQUENCY=${MTD_TEST_QSO_FREQUENCY}
            -DQSO_PROGRESS=${MTD_TEST_QSO_PROGRESS}
            -P ${CMAKE_SOURCE_DIR}/tests/integration/jt9/compare_mtd_messages.cmake)
        set_tests_properties (${test_name} PROPERTIES
          LABELS "decoder;golden;integration;mtd"
          PROCESSORS ${processors}
          TIMEOUT 120)
        if (WSJT_ENABLE_TSAN)
          set_tests_properties (${test_name} PROPERTIES
            ENVIRONMENT "TSAN_OPTIONS=${wsjt_tsan_fortran_options}")
        endif ()
      endfunction ()

      if (WSJT_ENABLE_TSAN)
        # One decode cycle covers the same full sample and golden message set
        # while keeping the instrumented two-worker synchronization bounded.
        wsjt_add_jt9_mtd_golden_test (
          test_jt9_mtd_ft8_210703_133430_tsan 2 2 1 1)
        # Reusing the sample in one process carries decoded messages into the
        # next period and exercises signal-directed and QSO-state concurrency.
        wsjt_add_jt9_mtd_golden_test (
          test_jt9_mtd_ft8_prior_period_qso_tsan 2 2 1 1
          CONSECUTIVE_PERIODS
          MY_CALL K1JT
          DX_CALL HA0DU
          QSO_FREQUENCY 590
          QSO_PROGRESS 1)
        set_tests_properties (test_jt9_mtd_ft8_prior_period_qso_tsan PROPERTIES
          TIMEOUT 360)
      else ()
        wsjt_add_jt9_mtd_golden_test (
          test_jt9_mtd_ft8_210703_133430 4 4 3 3)
        # Keep the stateful path covered in ordinary OpenMP builds as well as
        # sanitizer builds.
        wsjt_add_jt9_mtd_golden_test (
          test_jt9_mtd_ft8_prior_period_qso 4 4 1 1
          CONSECUTIVE_PERIODS
          MY_CALL K1JT
          DX_CALL HA0DU
          QSO_FREQUENCY 590
          QSO_PROGRESS 1)
        wsjt_add_jt9_mtd_golden_test (
          test_jt9_mtd_ft8_210703_133430_limited_team 2 4 3 3)
        # The -N 4 request requires at least three visible processors for this
        # limit to reduce the runtime team.
        set_tests_properties (test_jt9_mtd_ft8_210703_133430_limited_team PROPERTIES
          ENVIRONMENT "OMP_THREAD_LIMIT=2")
      endif ()
    endif ()
    wsjt_add_jt9_golden_test (
      test_jt9_golden_jt9_130418_1742
      samples/JT9/130418_1742.wav
      test/fixtures/jt9_130418_1742.expected.txt
      -9)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_q65_30a_201203_024000
      samples/Q65/30A_Ionoscatter_6m/201203_024000.wav
      test/fixtures/q65_30a_201203_024000.expected.txt
      --q65 -p 30 -f 1000 -d 3)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_q65_300a_201210_0505
      samples/Q65/300A_Optical_Scatter/201210_0505.wav
      test/fixtures/q65_300a_201210_0505.expected.txt
      --q65 -p 300 -f 1000 -d 3)
    set_tests_properties (test_jt9_golden_q65_300a_201210_0505 PROPERTIES
      COST 18)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_q65_60d_201212_1838
      samples/Q65/60D_EME_10GHz/201212_1838.wav
      test/fixtures/q65_60d_201212_1838.expected.txt
      --q65 -b D -p 60 -f 1000 -d 3)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_msk144_181211_120800
      samples/MSK144/181211_120800.wav
      test/fixtures/msk144_181211_120800.expected.txt
      --msk144 -p 15 -f 1500 -F 50 -d 3)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_fst4_210115_0058
      samples/FST4+FST4W/210115_0058.wav
      test/fixtures/fst4_210115_0058.expected.txt
      --fst4 -p 60 -f 1331 -L 1000 -H 1400 -d 3)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_fst4w_201230_0300
      samples/FST4+FST4W/201230_0300.wav
      test/fixtures/fst4w_201230_0300.expected.txt
      --fst4w -p 1800 -f 1433 -F 100 -d 3)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_ft4_000000_000002
      samples/FT4/000000_000002.wav
      test/fixtures/ft4_000000_000002.expected.txt
      --ft4 -d 3)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_jt65_000000_0004
      samples/JT65/JT65B/000000_0004.wav
      test/fixtures/jt65_000000_0004.expected.txt
      --jt65 -b B -f 1700 -d 3)
    wsjt_add_jt9_golden_test (
      test_jt9_golden_jt4_OK1KIR_141105_175700
      samples/JT4/JT4F/OK1KIR_141105_175700.WAV
      test/fixtures/jt4_OK1KIR_141105_175700.expected.txt
      --jt4 -b F -f 1200 -d 3)
    if (TARGET jt4sim)
      set (jt4_read_extent_work_dir
        "${CMAKE_BINARY_DIR}/Testing/JT4/test_jt4_12000_read_extent")
      add_test (NAME test_jt4_12000_read_extent
        COMMAND ${CMAKE_COMMAND}
          -DJT9=$<TARGET_FILE:jt9>
          -DJT4SIM=$<TARGET_FILE:jt4sim>
          -DWORK_DIR=${jt4_read_extent_work_dir}
          -P ${CMAKE_SOURCE_DIR}/tests/integration/jt9/verify_jt4_12000_read_extent.cmake)
      set_tests_properties (test_jt4_12000_read_extent PROPERTIES
        LABELS "decoder;jt4;integration"
        TIMEOUT 120)
    endif ()

    # WSPR decodes via the standalone wsprd binary, not jt9. -a isolates its
    # writable side files (wspr_wisdom.dat, ALL_WSPR.TXT, wspr_spots.txt,
    # wspr_timer.out, hashtable.txt) in the per-test work dir; confirmed this
    # doesn't affect stdout.
    function (wsjt_add_wsprd_golden_test test_name sample expected)
      set (work_dir "${CMAKE_BINARY_DIR}/Testing/Golden/${test_name}")
      file (MAKE_DIRECTORY "${work_dir}")
      add_test (NAME ${test_name}
        COMMAND ${CMAKE_COMMAND}
          -DWSPRD=$<TARGET_FILE:wsprd>
          -DSAMPLE=${CMAKE_SOURCE_DIR}/${sample}
          -DEXPECTED=${CMAKE_SOURCE_DIR}/${expected}
          -DWORK_DIR=${work_dir}
          -P ${CMAKE_SOURCE_DIR}/tests/integration/wsprd/compare_wsprd_output.cmake)
      set_tests_properties (${test_name} PROPERTIES
        LABELS "decoder;golden;integration"
        TIMEOUT 120)
    endfunction ()

    wsjt_add_wsprd_golden_test (
      test_wsprd_golden_150426_0918
      samples/WSPR/150426_0918.wav
      test/fixtures/wspr_150426_0918.expected.txt)
  else ()
    set (jt9_smoke_work_dir
      "${CMAKE_BINARY_DIR}/Testing/Smoke/test_jt9_ft8_callback_smoke")
    file (MAKE_DIRECTORY "${jt9_smoke_work_dir}")
    add_test (NAME test_jt9_ft8_callback_smoke
      COMMAND ${CMAKE_COMMAND}
        -DJT9=$<TARGET_FILE:jt9>
        -DSAMPLE=${CMAKE_SOURCE_DIR}/samples/FT8/210703_133430.wav
        -DEXPECTED_MESSAGE=CQ\ F5RXL\ IN94
        -DWORK_DIR=${jt9_smoke_work_dir}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_SOURCE_DIR}/tests/integration/jt9/smoke_decoder_callback.cmake)
    set_tests_properties (test_jt9_ft8_callback_smoke PROPERTIES
      LABELS "decoder;ft8;integration"
      TIMEOUT 120)
  endif ()

  if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set (WSJT_READELF "${CMAKE_READELF}")
    if (NOT WSJT_READELF)
      find_program (WSJT_READELF NAMES readelf llvm-readelf)
    endif ()
    if (NOT WSJT_READELF)
      message (FATAL_ERROR
        "readelf is required to verify that jt9 has a non-executable stack")
    endif ()
    add_test (NAME test_jt9_non_executable_stack
      COMMAND ${CMAKE_COMMAND}
        -DREADELF=${WSJT_READELF}
        -DEXECUTABLE=$<TARGET_FILE:jt9>
        -P ${CMAKE_SOURCE_DIR}/tests/integration/jt9/verify_elf_stack.cmake)
    set_tests_properties (test_jt9_non_executable_stack PROPERTIES
      LABELS "build;elf;security"
      TIMEOUT 30)
  endif ()
endif ()

if (WSJT_BUILD_FORTRAN_OPENMP)
  wsjt_link_fortran_openmp (jt9)
  target_link_libraries (jt9 wsjt_cxx fort_qt)
  if (WIN32)
    set_property (TARGET jt9 APPEND_STRING PROPERTY
      LINK_FLAGS " -Wl,--stack,16777216")
  endif ()
else ()
  target_link_libraries (jt9 wsjt_fort wsjt_cxx fort_qt)
endif ()

# build jt9stream: the Qt-free streaming decoder (B2). Same jt9.f90 main as jt9,
# but ALWAYS compiled with the jt9a stub and NEVER linked against fort_qt, so it
# links zero Qt. Mirrors develop's jt9 OpenMP/APPLE/WIN32 structure exactly.
if (WSJT_BUILD_JT9STREAM)
  generate_version_info (jt9stream_VERSION_RESOURCES
    NAME jt9stream
    BUNDLE ${PROJECT_BUNDLE_NAME}
    ICON ${WSJTX_ICON_FILE}
    FILE_DESCRIPTION "jt9stream - WSJT-X Qt-free streaming decoder"
    )
  add_executable (jt9stream ${jt9stream_FSRCS} ${jt9stream_VERSION_RESOURCES})
  target_compile_definitions (jt9stream PRIVATE JT9_PROGRAM_NAME=\"jt9stream\")
  # jt9stream shares jt9's Fortran module directory (fortran_modules_omp) so it
  # can READ the modules emitted by wsjt_fort_omp (streaming_emit, etc.). jt9.f90
  # is a program and streaming_io.f90's jt9_stream is a bare subroutine, so this
  # target emits no .mod of its own — sharing the dir is collision-free.
  if (WSJT_BUILD_FORTRAN_OPENMP)
    wsjt_link_fortran_openmp (jt9stream)
    if (WIN32)
      set_property (TARGET jt9stream APPEND_STRING PROPERTY
        LINK_FLAGS " -Wl,--stack,16777216")
    endif ()
    target_link_libraries (jt9stream wsjt_cxx)
  else ()
    target_link_libraries (jt9stream wsjt_fort wsjt_cxx)
  endif ()
  # jt9stream NEVER links fort_qt — that is the Qt-free guarantee.
endif (WSJT_BUILD_JT9STREAM)

find_package (Portaudio REQUIRED)
add_subdirectory (map65)
if (TARGET map65)
  wsjt_enable_qt_automoc (map65)
endif ()

if (NOT WSJT_SKIP_QMAP)
  add_subdirectory (qmap)
endif ()
if (TARGET qmap)
  wsjt_enable_qt_automoc (qmap)
endif ()

# build the main application
generate_version_info (wsjtx_VERSION_RESOURCES
  NAME wsjtx
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "WSJT-X"
  )

# Embed a Windows application manifest declaring per-monitor V2 DPI
# awareness. Without it, Windows treats wsjtx.exe as DPI-unaware and
# applies legacy bitmap-stretching scaling on high-DPI displays —
# fonts/widgets appear larger than locally-built (MSVC) executables
# which auto-embed a manifest. Resource ID 1, type RT_MANIFEST (24).
set (wsjtx_PLATFORM_RESOURCES)
if (WIN32)
  set (WSJTX_MANIFEST_PATH "${CMAKE_CURRENT_SOURCE_DIR}/wsjtx.exe.manifest")
  configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules/wsjtx-manifest.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/wsjtx-manifest.rc"
    @ONLY
    )
  list (APPEND wsjtx_PLATFORM_RESOURCES
    "${CMAKE_CURRENT_BINARY_DIR}/wsjtx-manifest.rc")
endif (WIN32)

add_executable (wsjtx MACOSX_BUNDLE
  ${wsjtx_CXXSRCS}
  ${wsjtx_GENUISRCS}
  ${WSJTX_ICON_FILE}
  ${wsjtx_RESOURCES_RCC}
  ${wsjtx_VERSION_RESOURCES}
  ${wsjtx_PLATFORM_RESOURCES}
  ${wsjtx_BUNDLE_DATA_FILES}
  )

wsjt_enable_qt_automoc (wsjtx)

if (WSJT_ENABLE_TESTS)
  target_compile_definitions (wsjtx PRIVATE WSJT_ENABLE_LIVE_AUDIO_TEST=1)
endif ()

if (APPLE)
  add_custom_command(
      TARGET wsjtx
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different 
      "${CMAKE_BINARY_DIR}/jt9"
      "$<TARGET_FILE_DIR:wsjtx>"
      COMMENT "Copying $<TARGET_FILE_DIR:jt9> into the Contents/MacOS/ directory"
  )
endif (APPLE)

if (WSJT_CREATE_WINMAIN)
  set_target_properties (wsjtx PROPERTIES WIN32_EXECUTABLE ON)
endif (WSJT_CREATE_WINMAIN)

set_target_properties (wsjtx PROPERTIES
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Darwin/Info.plist.in"
  MACOSX_BUNDLE_INFO_STRING "${PROJECT_DESCRIPTION}"
  MACOSX_BUNDLE_ICON_FILE "${WSJTX_ICON_FILE}"
  MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
  MACOSX_BUNDLE_SHORT_VERSION_STRING "v${wsjtx_VERSION}"
  MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${wsjtx_VERSION}"
  MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_BUNDLE_NAME}"
  MACOSX_BUNDLE_BUNDLE_EXECUTABLE_NAME "${PROJECT_NAME}"
  MACOSX_BUNDLE_COPYRIGHT "${PROJECT_COPYRIGHT}"
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.k1jt.wsjtx"
  )

target_include_directories (wsjtx PRIVATE ${FFTW3_INCLUDE_DIRS})
# On macOS the app links the non-OpenMP variant while jt9 carries the GNU
# OpenMP runtime separately; this keeps Apple Clang libomp out of the bundle.
if ((NOT WSJT_BUILD_FORTRAN_OPENMP) OR APPLE)
  target_link_libraries (wsjtx wsjt_fort)
else ()
  wsjt_link_fortran_openmp (wsjtx)
  if (WIN32)
    set_property (TARGET wsjtx APPEND_STRING PROPERTY
      LINK_FLAGS " -Wl,--stack,0x1000000,--heap,0x20000000")
  endif ()
endif ()
target_link_libraries (wsjtx Qt5::SerialPort Qt5::Network wsjt_cxx wsjt_qt wsjt_qtmm ${FFTW3_LIBRARIES} ${LIBM_LIBRARIES})

if (WSJT_ENABLE_TSAN)
  target_compile_definitions (wsjtx PRIVATE WSJT_TSAN_TEST_PROFILE)
endif ()

if (WSJT_ENABLE_TESTS AND NOT WIN32)
  set (wsjtx_startup_test_root
    "${CMAKE_BINARY_DIR}/Testing/Startup/test_wsjtx_startup")
  file (MAKE_DIRECTORY
    "${wsjtx_startup_test_root}/config"
    "${wsjtx_startup_test_root}/data"
    "${wsjtx_startup_test_root}/cache"
    "${wsjtx_startup_test_root}/tmp")
  set (wsjtx_startup_test_environment
    "XDG_CONFIG_HOME=${wsjtx_startup_test_root}/config"
    "XDG_DATA_HOME=${wsjtx_startup_test_root}/data"
    "XDG_CACHE_HOME=${wsjtx_startup_test_root}/cache"
    "WSJT_QMAP_SHARED_MEMORY_KEY=mem_qmap-test_wsjtx_startup")
  if (NOT APPLE)
    list (APPEND wsjtx_startup_test_environment
      "TMPDIR=${wsjtx_startup_test_root}/tmp")
  endif ()
  if (APPLE)
    add_test (
      NAME test_wsjtx_startup
      COMMAND /bin/bash "${CMAKE_SOURCE_DIR}/test/run-wsjtx-startup-macos.sh"
        "${CMAKE_BINARY_DIR}/startup-smoke-diagnostics"
        "$<TARGET_FILE:wsjtx>" --startup-smoke-test --rig-name CTEST-STARTUP
    )
    set (wsjtx_startup_test_timeout 45)
  else ()
    add_test (
      NAME test_wsjtx_startup
      COMMAND $<TARGET_FILE:wsjtx> --startup-smoke-test --rig-name CTEST-STARTUP
    )
    set (wsjtx_startup_test_timeout 20)
  endif ()
  set_tests_properties (test_wsjtx_startup PROPERTIES
    ENVIRONMENT "${wsjtx_startup_test_environment}"
    PROCESSORS 1
    TIMEOUT ${wsjtx_startup_test_timeout}
  )
  if (WSJT_ENABLE_TSAN)
    set_tests_properties (test_wsjtx_startup PROPERTIES
      ENVIRONMENT "TSAN_OPTIONS=${wsjt_tsan_gui_options}")
  endif ()
endif ()

if (WSJT_ENABLE_TESTS AND WSJT_ENABLE_GUI_SMOKE_TESTS
    AND WSJT_BUILD_FORTRAN_OPENMP
    AND (APPLE OR
      (CMAKE_SYSTEM_NAME STREQUAL "Linux"
        AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")))
  add_dependencies (wsjtx sjtty)

  # Each full WSJT-X process allocates about 48 MiB of System V shared memory.
  # The macOS CI pool is sized for three concurrent instances, so keep GUI
  # process concurrency at three unless that limit is raised as well.

  set (wsjtx_live_audio_test_root
    "${CMAKE_BINARY_DIR}/Testing/LiveAudio/test_wsjtx_live_audio_ft8")
  file (MAKE_DIRECTORY
    "${wsjtx_live_audio_test_root}/config"
    "${wsjtx_live_audio_test_root}/data"
    "${wsjtx_live_audio_test_root}/cache"
    "${wsjtx_live_audio_test_root}/tmp")
  set (wsjtx_live_audio_test_environment
    "XDG_CONFIG_HOME=${wsjtx_live_audio_test_root}/config"
    "XDG_DATA_HOME=${wsjtx_live_audio_test_root}/data"
    "XDG_CACHE_HOME=${wsjtx_live_audio_test_root}/cache"
    "WSJT_QMAP_SHARED_MEMORY_KEY=mem_qmap-test_wsjtx_live_audio_ft8")
  if (NOT APPLE)
    list (APPEND wsjtx_live_audio_test_environment
      "TMPDIR=${wsjtx_live_audio_test_root}/tmp")
  endif ()
  if (WSJT_ENABLE_TSAN)
    list (APPEND wsjtx_live_audio_test_environment
      "TSAN_OPTIONS=${wsjt_tsan_gui_options}")
  endif ()
  add_test (
    NAME test_wsjtx_live_audio_ft8
    COMMAND $<TARGET_FILE:wsjtx>
      --live-audio-test
      "${CMAKE_SOURCE_DIR}/samples/FT8/210703_133430.wav"
      --live-audio-expected
      "${CMAKE_SOURCE_DIR}/test/fixtures/ft8_210703_133430_mtd.expected.txt"
      --live-audio-data-dir
      "${CMAKE_SOURCE_DIR}"
      --rig-name CTEST-LIVE-AUDIO)
  set_tests_properties (test_wsjtx_live_audio_ft8 PROPERTIES
    COST 25
    ENVIRONMENT "${wsjtx_live_audio_test_environment}"
    LABELS "gui;audio;decoder;integration"
    PROCESSORS 1
    TIMEOUT 120)

  set (wsjtx_receive_handoff_test_root
    "${CMAKE_BINARY_DIR}/Testing/LiveAudio/test_wsjtx_receive_handoff")
  file (MAKE_DIRECTORY
    "${wsjtx_receive_handoff_test_root}/config"
    "${wsjtx_receive_handoff_test_root}/data"
    "${wsjtx_receive_handoff_test_root}/cache"
    "${wsjtx_receive_handoff_test_root}/tmp")
  set (wsjtx_receive_handoff_test_environment
    "XDG_CONFIG_HOME=${wsjtx_receive_handoff_test_root}/config"
    "XDG_DATA_HOME=${wsjtx_receive_handoff_test_root}/data"
    "XDG_CACHE_HOME=${wsjtx_receive_handoff_test_root}/cache"
    "WSJT_QMAP_SHARED_MEMORY_KEY=mem_qmap-test_wsjtx_receive_handoff")
  if (NOT APPLE)
    list (APPEND wsjtx_receive_handoff_test_environment
      "TMPDIR=${wsjtx_receive_handoff_test_root}/tmp")
  endif ()
  if (WSJT_ENABLE_TSAN)
    list (APPEND wsjtx_receive_handoff_test_environment
      "TSAN_OPTIONS=${wsjt_tsan_gui_options}")
  endif ()
  add_test (
    NAME test_wsjtx_receive_handoff
    COMMAND $<TARGET_FILE:wsjtx>
      --receive-handoff-test
      --rig-name CTEST-RECEIVE-HANDOFF)
  set_tests_properties (test_wsjtx_receive_handoff PROPERTIES
    ENVIRONMENT "${wsjtx_receive_handoff_test_environment}"
    LABELS "gui;audio;integration;receive-handoff"
    PROCESSORS 1
    TIMEOUT 30)

  set (wsjtx_jtty_live_audio_test_root
    "${CMAKE_BINARY_DIR}/Testing/LiveAudio/test_wsjtx_live_audio_jtty")
  add_test (
    NAME test_wsjtx_live_audio_jtty
    COMMAND ${CMAKE_COMMAND}
      "-DSJTTY=$<TARGET_FILE:sjtty>"
      "-DWSJTX=$<TARGET_FILE:wsjtx>"
      "-DWORK_DIR=${wsjtx_jtty_live_audio_test_root}"
      "-DEXPECTED=${CMAKE_SOURCE_DIR}/tests/integration/jtty/live_audio.expected.txt"
      -P ${CMAKE_SOURCE_DIR}/tests/integration/jtty/run_live_audio.cmake)
  set_tests_properties (test_wsjtx_live_audio_jtty PROPERTIES
    COST 21
    LABELS "gui;audio;jtty;integration"
    PROCESSORS 1
    TIMEOUT 120)

  set (wsjtx_jtty_tx_loopback_test_root
    "${CMAKE_BINARY_DIR}/Testing/LiveAudio/test_wsjtx_jtty_tx_loopback")
  add_test (
    NAME test_wsjtx_jtty_tx_loopback
    COMMAND ${CMAKE_COMMAND}
      "-DWSJTX=$<TARGET_FILE:wsjtx>"
      "-DWORK_DIR=${wsjtx_jtty_tx_loopback_test_root}"
      "-DEXPECTED=${CMAKE_SOURCE_DIR}/tests/integration/jtty/tx_loopback.expected.txt"
      -P ${CMAKE_SOURCE_DIR}/tests/integration/jtty/run_tx_loopback.cmake)
  set_tests_properties (test_wsjtx_jtty_tx_loopback PROPERTIES
    COST 37
    LABELS "gui;audio;jtty;integration"
    PROCESSORS 1
    TIMEOUT 180)

  set (wsjtx_ft8_tx_loopback_test_root
    "${CMAKE_BINARY_DIR}/Testing/LiveAudio/test_wsjtx_ft8_tx_loopback")
  add_test (
    NAME test_wsjtx_ft8_tx_loopback
    COMMAND ${CMAKE_COMMAND}
      "-DWSJTX=$<TARGET_FILE:wsjtx>"
      "-DWORK_DIR=${wsjtx_ft8_tx_loopback_test_root}"
      "-DEXPECTED=${CMAKE_SOURCE_DIR}/tests/integration/ft8/tx_loopback.expected.txt"
      "-DDATA_DIR=${CMAKE_SOURCE_DIR}"
      -P ${CMAKE_SOURCE_DIR}/tests/integration/ft8/run_tx_loopback.cmake)
  set_tests_properties (test_wsjtx_ft8_tx_loopback PROPERTIES
    COST 43
    LABELS "gui;audio;decoder;ft8;integration"
    PROCESSORS 2
    TIMEOUT 150)
  if (WSJT_ENABLE_TSAN)
    set_tests_properties (
      test_wsjtx_live_audio_jtty
      test_wsjtx_jtty_tx_loopback
      test_wsjtx_ft8_tx_loopback
      PROPERTIES ENVIRONMENT "TSAN_OPTIONS=${wsjt_tsan_gui_options}")
  endif ()
endif ()

# make a library for WSJT-X UDP servers
# add_library (wsjtx_udp SHARED ${UDP_library_CXXSRCS})
add_library (wsjtx_udp-static STATIC ${UDP_library_CXXSRCS})
wsjt_enable_qt_automoc (wsjtx_udp-static)
#target_include_directories (wsjtx_udp
#  INTERFACE
#  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/wsjtx>
#  )
target_include_directories (wsjtx_udp-static
  INTERFACE
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/wsjtx>
  )
#set_target_properties (wsjtx_udp PROPERTIES
#  PUBLIC_HEADER "${UDP_library_HEADERS}"
#  )
set_target_properties (wsjtx_udp-static PROPERTIES
  OUTPUT_NAME wsjtx_udp
  )
target_compile_definitions (wsjtx_udp-static PUBLIC UDP_STATIC_DEFINE)
target_link_libraries (wsjtx_udp-static Qt5::Network Qt5::Gui)
generate_export_header (wsjtx_udp-static BASE_NAME udp)

generate_version_info (udp_daemon_VERSION_RESOURCES
  NAME udp_daemon
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "Example WSJT-X UDP Message Protocol daemon"
  )
add_executable (udp_daemon UDPExamples/UDPDaemon.cpp ${udp_daemon_VERSION_RESOURCES})
wsjt_enable_qt_automoc (udp_daemon)
target_link_libraries (udp_daemon wsjtx_udp-static)

generate_version_info (wsjtx_app_version_VERSION_RESOURCES
  NAME wsjtx_app_version
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "Display WSJT-X Application Version on console"
  )
add_executable (wsjtx_app_version AppVersion/AppVersion.cpp ${wsjtx_app_version_VERSION_RESOURCES})
target_link_libraries (wsjtx_app_version wsjt_qt)

generate_version_info (message_aggregator_VERSION_RESOURCES
  NAME message_aggregator
  BUNDLE ${PROJECT_BUNDLE_NAME}
  ICON ${WSJTX_ICON_FILE}
  FILE_DESCRIPTION "Example WSJT-X UDP Message Protocol application"
  )
add_resources (message_aggregator_RESOURCES /qss ${message_aggregator_STYLESHEETS})
configure_file (UDPExamples/message_aggregator.qrc.in message_aggregator.qrc @ONLY)
qt5_add_resources (message_aggregator_RESOURCES_RCC
  ${CMAKE_CURRENT_BINARY_DIR}/message_aggregator.qrc
  contrib/QDarkStyleSheet/qdarkstyle/style.qrc
  )
add_executable (message_aggregator
  ${message_aggregator_CXXSRCS}
  ${message_aggregator_RESOURCES_RCC}
  ${message_aggregator_VERSION_RESOURCES}
  )
wsjt_enable_qt_automoc (message_aggregator)
target_link_libraries (message_aggregator wsjt_qt Qt5::Widgets wsjtx_udp-static)

if (WSJT_CREATE_WINMAIN)
  set_target_properties (message_aggregator PROPERTIES WIN32_EXECUTABLE ON)
endif (WSJT_CREATE_WINMAIN)

if (UNIX)
  if (NOT WSJT_SKIP_MANPAGES)
    add_subdirectory (manpages)
    add_dependencies (wsjtx manpages)
  endif (NOT WSJT_SKIP_MANPAGES)
  if (NOT APPLE)
    add_subdirectory (debian)
    add_dependencies (wsjtx debian)
  endif (NOT APPLE)
endif (UNIX)

#
# installation
#
install (TARGETS wsjtx
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION . COMPONENT runtime
  )

# install (TARGETS wsjtx_udp EXPORT udp
#   RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
#   LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/wsjtx
#   )
# install (TARGETS wsjtx_udp-static EXPORT udp-static
#   DESTINATION ${CMAKE_INSTALL_LIBDIR}
#   )

# install (EXPORT udp NAMESPACE wsjtx::
#   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wsjtx
#   )
# install (EXPORT udp-static NAMESPACE wsjtx::
#   DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/wsjtx
#   )

install (TARGETS udp_daemon message_aggregator wsjtx_app_version
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )

install (TARGETS jt9 wsprd fmtave fcal fmeasure
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )

install (TARGETS qmap
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )

if (NOT WSJT_SKIP_MAP65)
  install (TARGETS map65
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
    )
endif ()

if(WSJT_BUILD_UTILS)
install (TARGETS ft8code jt65code jt9code jt4code msk144code 
  q65code fst4sim ft8sim q65sim EchoCallSim testEchoCall echosim hash22calc cablog
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )
endif(WSJT_BUILD_UTILS)  

install (PROGRAMS
  ${RIGCTL_EXE}
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  #COMPONENT runtime
  RENAME rigctl-wsjtx${CMAKE_EXECUTABLE_SUFFIX}
  )

install (PROGRAMS
  ${RIGCTLD_EXE}
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  #COMPONENT runtime
  RENAME rigctld-wsjtx${CMAKE_EXECUTABLE_SUFFIX}
  )

install (PROGRAMS
  ${RIGCTLCOM_EXE}
  DESTINATION ${CMAKE_INSTALL_BINDIR}
  #COMPONENT runtime
  RENAME rigctlcom-wsjtx${CMAKE_EXECUTABLE_SUFFIX}
  )

install (FILES
  README
  COPYING
  AUTHORS
  THANKS
  NEWS
  BUGS
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  #COMPONENT runtime
  )

install (FILES
  cty.dat
  cty.dat_copyright.txt
  grid.dat
  sat.dat
  contrib/Ephemeris/JPLEPH
  eclipse.txt
  ALLCALL7.TXT
  DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}
  #COMPONENT runtime
  )

install (DIRECTORY
  example_log_configurations
  DESTINATION ${CMAKE_INSTALL_DOCDIR}
  FILES_MATCHING REGEX "^.*[^~]$"
  #COMPONENT runtime
  )

if (APPLE)
  set (WSJT_SOUND_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR})
else ()
  set (WSJT_SOUND_DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()
install (DIRECTORY
  ${PROJECT_SOURCE_DIR}/sounds
  DESTINATION ${WSJT_SOUND_DESTINATION}
  #COMPONENT runtime
  )

if (WIN32)
  # Bundle OpenSSL runtime DLLs from the build environment.
  # Qt's QSslSocket loads OpenSSL by filename at runtime; the names depend
  # on what Qt was built against (libssl-1_1*.dll for older Qt-on-OpenSSL-1.1,
  # libssl-3-x64.dll for current MSYS2 Qt-on-OpenSSL-3). Glob whatever the
  # runner provides so SuperFox verification, Update Hamlib, LotW download,
  # and eQSL keep working as the build environment advances.
  set (_openssl_msys_prefix "$ENV{MSYSTEM_PREFIX}")
  if (NOT _openssl_msys_prefix)
    set (_openssl_msys_prefix "/mingw64")
  endif ()
  file (GLOB _openssl_runtime_dlls
    "${_openssl_msys_prefix}/bin/libssl-*.dll"
    "${_openssl_msys_prefix}/bin/libcrypto-*.dll"
  )
  if (_openssl_runtime_dlls)
    install (FILES ${_openssl_runtime_dlls}
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      #COMPONENT runtime
    )
  else ()
    message (WARNING
      "OpenSSL runtime DLLs not found at ${_openssl_msys_prefix}/bin — "
      "TLS features (SuperFox, Update Hamlib, LotW, eQSL) will fail on "
      "installs without OpenSSL in PATH."
    )
  endif ()

  # EM2/EM3 K4 remote audio loads Xiph Opus at runtime. Accommodate both
  # native Windows (opus.dll) and MSYS2 (libopus-0.dll) naming.
  file (GLOB _opus_runtime_dlls
    "${_openssl_msys_prefix}/bin/opus.dll"
    "${_openssl_msys_prefix}/bin/libopus*.dll"
  )
  if (_opus_runtime_dlls)
    install (FILES ${_opus_runtime_dlls}
      DESTINATION ${CMAKE_INSTALL_BINDIR}
      #COMPONENT runtime
    )
  else ()
    message (WARNING
      "Xiph Opus runtime DLL not found at ${_openssl_msys_prefix}/bin — "
      "K4 EM2/EM3 audio will be unavailable; EM0/EM1 remain usable."
    )
  endif ()
endif (WIN32)

#
# Mac installer files
#
if (APPLE)
    install (FILES
        Darwin/ReadMe.txt
        Darwin/com.wsjtx.sysctl.plist
        DESTINATION .
        #COMPONENT runtime
    )
endif (APPLE)

#
# uninstall support
#
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/CMake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  @ONLY)
add_custom_target (uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake")


# creates or updates ${PROJECT_BINARY_DIR}/scs_version.h using cmake script
add_custom_target (revisiontag
  COMMAND ${CMAKE_COMMAND}
  -D SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
  -D BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
  -D OUTPUT_DIR=${PROJECT_BINARY_DIR}
  -D SOURCE_REVISION=${WSJT_SOURCE_REVISION}
  -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake/getsvn.cmake
  VERBATIM
  BYPRODUCTS scs_version.h
  COMMENT "Getting source control system revision information"
  )
# explicitly say that the wsjt_qt depends on custom target, this is
# done indirectly so that the revisiontag target gets built exactly
# once per build
add_dependencies (wsjt_qt revisiontag)
add_dependencies (jt9 revisiontag)
add_dependencies (wsprd revisiontag)


#
# versioning and configuration
#
configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/wsjtx_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/wsjtx_config.h"
  )


if (NOT WIN32 AND NOT APPLE)
  # install a desktop file so wsjtx appears in the application start
  # menu with an icon
  install (
    FILES wsjtx.desktop message_aggregator.desktop
    DESTINATION share/applications
    #COMPONENT runtime
    )
  install (
    FILES icons/Unix/wsjtx_icon.png
    DESTINATION share/pixmaps
    #COMPONENT runtime
    )
endif (NOT WIN32 AND NOT APPLE)

if (APPLE)
  set (CMAKE_POSTFLIGHT_SCRIPT
    "${wsjtx_BINARY_DIR}/postflight.sh")
  set (CMAKE_POSTUPGRADE_SCRIPT
    "${wsjtx_BINARY_DIR}/postupgrade.sh")
  configure_file ("${wsjtx_SOURCE_DIR}/Darwin/postflight.sh.in"
    "${CMAKE_POSTFLIGHT_SCRIPT}")
  configure_file ("${wsjtx_SOURCE_DIR}/Darwin/postupgrade.sh.in"
    "${CMAKE_POSTUPGRADE_SCRIPT}")
  add_custom_command(
          TARGET wsjtx POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy
          $<TARGET_FILE:jt9>
          $<TARGET_BUNDLE_CONTENT_DIR:wsjtx>/MacOS/
          COMMENT "Copying jt9 into WSJT-X bundle"
  )
endif ()


set (wsjt_enable_bundle_fixup FALSE)
if (NOT is_debug_build)
  if (WIN32 AND NOT WSJT_SKIP_BUNDLE_FIXUP)
    set (wsjt_enable_bundle_fixup TRUE)
  elseif (APPLE AND WSJT_MACOS_INSTALL_TIME_FIXUP AND NOT WSJT_SKIP_BUNDLE_FIXUP)
    set (wsjt_enable_bundle_fixup TRUE)
  endif ()
endif ()

if (wsjt_enable_bundle_fixup)
  # add this sub-sirectory after all install steps and other
  # sub-directories to ensure that all executables are in-place before
  # any fixup is done
  add_subdirectory (bundle_fixup)
endif ()


#
# packaging
#
set (CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/package_description.txt")
set (CPACK_MONOLITHIC_INSTALL 1)
set (CPACK_PACKAGE_NAME "K4-WSJT-X")
set (CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set (CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set (CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}${BUILD_TYPE_REVISION}")

if (WIN32)
  set (CPACK_GENERATOR "NSIS")
elseif (APPLE)
  set (CPACK_GENERATOR "DragNDrop")
else ()
  find_program (DPKG_BUILDER dpkg-buildpackage DOC "Debian package builder")
  if (DPKG_BUILDER)
    #
    # Derive the correct filename for a Debian package because the DEB
    # generator doesn't do this correctly at present.
    #
    if (NOT CPACK_DEBIAN_PACKAGE_ARCHITECTURE)
      find_program (DPKG_PROGRAM dpkg DOC "dpkg program of Debian-based systems")
      if (DPKG_PROGRAM)
        execute_process (
	  COMMAND ${DPKG_PROGRAM} --print-architecture
	  OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
	  OUTPUT_STRIP_TRAILING_WHITESPACE
	  )
      else (DPKG_PROGRAM)
        set (CPACK_DEBIAN_PACKAGE_ARCHITECTURE noarch)
      endif (DPKG_PROGRAM)
    endif ()

    list (APPEND CPACK_GENERATOR "DEB")
  endif (DPKG_BUILDER)

  find_program (RPMBUILDER rpmbuild DOC "RPM package builder")
  if (RPMBUILDER)
    list (APPEND CPACK_GENERATOR "RPM")
  endif (RPMBUILDER)
endif ()

set (CPACK_DEBIAN_PACKAGE_HOMEPAGE "${PROJECT_HOMEPAGE}")
set (CPACK_DEBIAN_PACKAGE_DEPENDS "libgfortran5 (>=8.3) | libgfortran4 (>=7.3) | libgfortran3 (>=6.3), libfftw3-single3 (>=3.3), libgomp1 (>=6), libqt5serialport5 (>=5.7), libqt5multimedia5-plugins (>=5.7), libqt5widgets5 (>=5.7), libqt5network5 (>=5.7), libqt5printsupport5 (>=5.7), libqt5websockets5 (>=5.7), libqt5sql5-sqlite (>=5.7), libusb-1.0-0 (>=1.0.21), libopus0, libboost-log1.62.0 (>=1.62.0) | libboost-log1.65.1 (>=1.65.1) | libboost-log1.67.0 (>=1.67.0) | libboost-log1.71.0 (>=1.71.0) | libboost-log1.74.0 (>=1.74.0) | libboost-log1.81.0 (>=1.81.0) | libboost-log1.83.0 (>=1.83.0) | libboost-log1.85.0 (>=1.85.0)")
set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
# The downstream WSJT-X Improved fork uses the same executable, data, and doc
# paths as mainline. Replace its packages so the installations cannot mix,
# while preserving dependencies such as `js8call` through virtual packages.
set (CPACK_DEBIAN_PACKAGE_PROVIDES "wsjtx-data (= ${wsjtx_VERSION}), wsjtx-doc (= ${wsjtx_VERSION}), wsjtx-improved-data (= ${wsjtx_VERSION}), wsjtx-improved-doc (= ${wsjtx_VERSION})")
set (CPACK_DEBIAN_PACKAGE_REPLACES "wsjtx-improved, wsjtx-data, wsjtx-doc, wsjtx-improved-data, wsjtx-improved-doc")
# WSJT-X Improved is a downstream fork, not a versioned upgrade.
# Its packages can compare newer than mainline despite incompatible binaries
# and shared files, so these relationships must not be version-qualified.
set (CPACK_DEBIAN_PACKAGE_BREAKS "wsjtx-improved, wsjtx-improved-data, wsjtx-improved-doc")
set (CPACK_DEBIAN_PACKAGE_CONFLICTS "wsjtx-data (<< ${wsjtx_VERSION}), wsjtx-doc (<< ${wsjtx_VERSION})")

set (CPACK_RPM_PACKAGE_ARCHITECTURE ${CMAKE_SYSTEM_PROCESSOR})
set (CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase >= 5.9, qt5-qtserialport >= 5.9, qt5-qtmultimedia >= 5.9, qt5-qtwebsockets >= 5.9, qt5-qtsvg >= 5.9, libusbx >= 1.0.22, libgfortran >= 7, libgomp >= 7, fftw-libs-single >= 3.3, opus, boost-log >= 1.62")
set (CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /usr/share/pixmaps /usr/share/applications /usr/share/man /usr/share/man1)

configure_file ("${PROJECT_SOURCE_DIR}/CMakeCPackOptions.cmake.in"
  "${PROJECT_BINARY_DIR}/CMakeCPackOptions.cmake" @ONLY)
set (CPACK_PROJECT_CONFIG_FILE "${PROJECT_BINARY_DIR}/CMakeCPackOptions.cmake")

include (CPack)
