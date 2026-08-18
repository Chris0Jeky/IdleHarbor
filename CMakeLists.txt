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

add_library(idleharbor_core STATIC src/core/core.cpp)
target_include_directories(idleharbor_core PUBLIC include)
target_compile_features(idleharbor_core PUBLIC cxx_std_20)
if(MSVC)
  set_property(TARGET idleharbor_core PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(idleharbor_core PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
endif()

add_executable(
  IdleHarbor
  WIN32
  src/app/main.cpp
  resources/IdleHarbor.rc
  resources/app.manifest
)

target_include_directories(IdleHarbor PRIVATE include)
target_compile_features(IdleHarbor PRIVATE cxx_std_20)
target_link_libraries(IdleHarbor PRIVATE idleharbor_core)
target_compile_definitions(
  IdleHarbor
  PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

if(MSVC)
  set_property(TARGET IdleHarbor PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  target_compile_options(IdleHarbor PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  target_link_options(IdleHarbor PRIVATE $<$<CONFIG:Release>:/OPT:REF> $<$<CONFIG:Release>:/OPT:ICF>)
endif()

if(IDLEHARBOR_BUILD_TESTS)
  include(CTest)
  add_executable(idleharbor_core_tests tests/core_tests.cpp)
  target_link_libraries(idleharbor_core_tests PRIVATE idleharbor_core)
  target_compile_features(idleharbor_core_tests PRIVATE cxx_std_20)
  if(MSVC)
    set_property(
      TARGET idleharbor_core_tests
      PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(idleharbor_core_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  endif()
  add_test(NAME core COMMAND idleharbor_core_tests)

  add_executable(idleharbor_foundation_tests tests/foundation_tests.cpp)
  target_include_directories(idleharbor_foundation_tests PRIVATE include)
  target_compile_features(idleharbor_foundation_tests PRIVATE cxx_std_20)
  if(MSVC)
    set_property(
      TARGET idleharbor_foundation_tests
      PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
    target_compile_options(idleharbor_foundation_tests PRIVATE /W4 /WX /permissive- /Zc:__cplusplus /utf-8)
  endif()
  add_test(NAME foundation COMMAND idleharbor_foundation_tests)
endif()
