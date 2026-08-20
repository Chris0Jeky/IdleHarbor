cmake_minimum_required(VERSION 3.21)

project(
  IdleHarbor
  VERSION 0.1.0
  DESCRIPTION "A tiny native Windows utility for transparent, configurable idle prevention"
  LANGUAGES CXX RC
)

if(NOT WIN32)
  message(FATAL_ERROR "IdleHarbor is a Windows-only application")
endif()

option(IDLEHARBOR_BUILD_TESTS "Build IdleHarbor tests" ON)

function(idleharbor_configure_msvc target)
  if(MSVC)
    set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  endif()
endfunction()

add_library(idleharbor_core STATIC src/core/core.cpp)
target_include_directories(idleharbor_core PUBLIC include)
target_compile_features(idleharbor_core PUBLIC cxx_std_20)
idleharbor_configure_msvc(idleharbor_core)

add_library(
  idleharbor_app_support
  STATIC
  src/app/command_line.cpp
  src/app/settings_store.cpp
  src/app/window_layout.cpp
)
target_include_directories(idleharbor_app_support PUBLIC include)
target_compile_features(idleharbor_app_support PUBLIC cxx_std_20)
target_compile_definitions(idleharbor_app_support PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(idleharbor_app_support PUBLIC idleharbor_core PRIVATE Shell32 Ole32)
idleharbor_configure_msvc(idleharbor_app_support)

add_library(
  idleharbor_windows
  STATIC
  src/platform/windows/input_monitor.cpp
  src/platform/windows/motion_emitter.cpp
  src/platform/windows/power_request.cpp
  src/platform/windows/system_snapshot.cpp
)
target_include_directories(idleharbor_windows PUBLIC include)
target_compile_features(idleharbor_windows PUBLIC cxx_std_20)
target_compile_definitions(idleharbor_windows PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(idleharbor_windows PUBLIC User32 Wtsapi32)
idleharbor_configure_msvc(idleharbor_windows)

add_executable(
  IdleHarbor
  WIN32
  src/app/main.cpp
  resources/IdleHarbor.rc
  resources/app.manifest
)

target_include_directories(IdleHarbor PRIVATE include)
target_compile_features(IdleHarbor PRIVATE cxx_std_20)
target_link_libraries(
  IdleHarbor
  PRIVATE idleharbor_core idleharbor_app_support idleharbor_windows Comctl32 Shell32 Wtsapi32
)
target_compile_definitions(
  IdleHarbor
  PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
  target_link_options(IdleHarbor PRIVATE $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
endif()
idleharbor_configure_msvc(IdleHarbor)

if(IDLEHARBOR_BUILD_TESTS)
  include(CTest)
  add_executable(idleharbor_core_tests tests/core_tests.cpp)
  target_link_libraries(idleharbor_core_tests PRIVATE idleharbor_core)
  target_compile_features(idleharbor_core_tests PRIVATE cxx_std_20)
  idleharbor_configure_msvc(idleharbor_core_tests)
  add_test(NAME core COMMAND idleharbor_core_tests)

  add_executable(idleharbor_command_line_tests tests/command_line_tests.cpp)
  target_link_libraries(idleharbor_command_line_tests PRIVATE idleharbor_app_support)
  target_compile_features(idleharbor_command_line_tests PRIVATE cxx_std_20)
  idleharbor_configure_msvc(idleharbor_command_line_tests)
  add_test(NAME command_line COMMAND idleharbor_command_line_tests)

  add_executable(idleharbor_settings_store_tests tests/settings_store_tests.cpp)
  target_link_libraries(idleharbor_settings_store_tests PRIVATE idleharbor_app_support)
  target_compile_features(idleharbor_settings_store_tests PRIVATE cxx_std_20)
  idleharbor_configure_msvc(idleharbor_settings_store_tests)
  add_test(NAME settings_store COMMAND idleharbor_settings_store_tests)

  add_executable(idleharbor_window_layout_tests tests/window_layout_tests.cpp)
  target_link_libraries(idleharbor_window_layout_tests PRIVATE idleharbor_app_support)
  target_compile_features(idleharbor_window_layout_tests PRIVATE cxx_std_20)
  idleharbor_configure_msvc(idleharbor_window_layout_tests)
  add_test(NAME window_layout COMMAND idleharbor_window_layout_tests)

  add_executable(idleharbor_motion_math_tests tests/motion_emitter_math_tests.cpp)
  target_link_libraries(idleharbor_motion_math_tests PRIVATE idleharbor_windows)
  target_compile_features(idleharbor_motion_math_tests PRIVATE cxx_std_20)
  idleharbor_configure_msvc(idleharbor_motion_math_tests)
  add_test(NAME motion_math COMMAND idleharbor_motion_math_tests)

  add_executable(idleharbor_system_snapshot_tests tests/system_snapshot_tests.cpp)
  target_link_libraries(idleharbor_system_snapshot_tests PRIVATE idleharbor_windows)
  target_compile_features(idleharbor_system_snapshot_tests PRIVATE cxx_std_20)
  idleharbor_configure_msvc(idleharbor_system_snapshot_tests)
  add_test(NAME system_snapshot COMMAND idleharbor_system_snapshot_tests)

  add_executable(idleharbor_foundation_tests tests/foundation_tests.cpp)
  target_include_directories(idleharbor_foundation_tests PRIVATE include)
  target_compile_features(idleharbor_foundation_tests PRIVATE cxx_std_20)
  idleharbor_configure_msvc(idleharbor_foundation_tests)
  add_test(NAME foundation COMMAND idleharbor_foundation_tests)
endif()
