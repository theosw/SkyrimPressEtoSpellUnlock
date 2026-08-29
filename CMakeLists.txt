cmake_minimum_required(VERSION 3.24)

project(ArcaneActivation VERSION 1.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

set(BUILD_TESTS OFF CACHE BOOL "Disable CommonLibSSE tests" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Build for Skyrim AE" FORCE)
set(ENABLE_SKYRIM_SE OFF CACHE BOOL "Do not build the 1.5.97 target" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "Do not build the VR target" FORCE)
set(SKSE_SUPPORT_XBYAK OFF CACHE BOOL "ArcaneActivation does not use Xbyak" FORCE)

set(
  ARCANE_ACTIVATION_COMMONLIBSSE_NG_DIR
  "$ENV{ARCANE_ACTIVATION_COMMONLIBSSE_NG_DIR}"
  CACHE PATH
  "Path to a CommonLibSSE-NG checkout"
)
if(NOT EXISTS "${ARCANE_ACTIVATION_COMMONLIBSSE_NG_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Set ARCANE_ACTIVATION_COMMONLIBSSE_NG_DIR to CommonLibSSE-NG")
endif()

add_subdirectory(
  "${ARCANE_ACTIVATION_COMMONLIBSSE_NG_DIR}"
  "${CMAKE_CURRENT_BINARY_DIR}/CommonLibSSE"
  EXCLUDE_FROM_ALL
)

configure_file(
  cmake/plugin_version.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/plugin_version.h"
  @ONLY
)
configure_file(
  cmake/version.rc.in
  "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc"
  @ONLY
)

add_library(
  arcane_activation_core
  STATIC
  include/arcane_activation/interaction.h
  include/arcane_activation/timing.h
  src/interaction.cpp
)
target_include_directories(arcane_activation_core PUBLIC include)
target_compile_features(arcane_activation_core PUBLIC cxx_std_23)

add_library(
  ArcaneActivation
  SHARED
  src/activation.cpp
  src/activation.h
  src/config.cpp
  src/config.h
  src/plugin.cpp
  src/runtime_settings.cpp
  src/runtime_settings.h
  src/pch.h
  "${CMAKE_CURRENT_BINARY_DIR}/generated/plugin_version.h"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/version.rc"
)
target_precompile_headers(ArcaneActivation PRIVATE src/pch.h)
target_include_directories(
  ArcaneActivation
  PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
  include
  src
)
target_link_libraries(
  ArcaneActivation
  PRIVATE
  CommonLibSSE::CommonLibSSE
  arcane_activation_core
)
target_compile_definitions(ArcaneActivation PRIVATE UNICODE _UNICODE)
target_compile_features(ArcaneActivation PRIVATE cxx_std_23)

if(MSVC)
  target_compile_options(ArcaneActivation PRIVATE /MP /permissive- /utf-8 /W4)
  target_compile_options(arcane_activation_core PRIVATE /permissive- /utf-8 /W4)
endif()

option(ARCANE_ACTIVATION_BUILD_TESTS "Build pure-core tests" ON)
find_program(ARCANE_ACTIVATION_PYTHON_EXECUTABLE NAMES python REQUIRED)
find_program(ARCANE_ACTIVATION_POWERSHELL_EXECUTABLE NAMES powershell REQUIRED)
set(
  arcane_oar_root
  "${CMAKE_CURRENT_SOURCE_DIR}/package/meshes/actors/character/OpenAnimationReplacer/ArcaneActivation"
)
add_custom_target(
  validate_arcane_oar
  COMMAND "${ARCANE_ACTIVATION_PYTHON_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_oar_package.py"
          "${arcane_oar_root}"
  VERBATIM
)
add_custom_target(
  validate_arcane_defaults
  COMMAND "${ARCANE_ACTIVATION_PYTHON_EXECUTABLE}"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_release_defaults.py"
          "${CMAKE_CURRENT_SOURCE_DIR}"
  VERBATIM
)
set(
  arcane_mcm_outputs
  "${CMAKE_CURRENT_SOURCE_DIR}/package/Scripts/ArcaneActivationNative.pex"
  "${CMAKE_CURRENT_SOURCE_DIR}/package/Scripts/ArcaneActivationMCM.pex"
)
add_custom_command(
  OUTPUT ${arcane_mcm_outputs}
  COMMAND "${ARCANE_ACTIVATION_POWERSHELL_EXECUTABLE}"
          -NoProfile -ExecutionPolicy Bypass -File
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/Build-ArcaneActivationMCM.ps1"
  DEPENDS
          "${CMAKE_CURRENT_SOURCE_DIR}/package/Source/Scripts/ArcaneActivationNative.psc"
          "${CMAKE_CURRENT_SOURCE_DIR}/package/Source/Scripts/ArcaneActivationMCM.psc"
          "${CMAKE_CURRENT_SOURCE_DIR}/package/MCM/Config/ArcaneActivation/config.json"
          "${CMAKE_CURRENT_SOURCE_DIR}/package/MCM/Config/ArcaneActivation/settings.ini"
          "${CMAKE_CURRENT_SOURCE_DIR}/tools/Build-ArcaneActivationMCM.ps1"
  VERBATIM
)
add_custom_target(arcane_mcm_scripts DEPENDS ${arcane_mcm_outputs})
if(ARCANE_ACTIVATION_BUILD_TESTS)
  enable_testing()
  find_package(Catch2 3 CONFIG REQUIRED)
  add_executable(
    arcane_activation_tests
    tests/interaction_test.cpp
  )
  target_link_libraries(
    arcane_activation_tests
    PRIVATE
    arcane_activation_core
    Catch2::Catch2WithMain
  )
  include(Catch)
  catch_discover_tests(arcane_activation_tests)
  add_test(
    NAME arcane_oar_package_validation
    COMMAND "${ARCANE_ACTIVATION_PYTHON_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_oar_package.py"
            "${arcane_oar_root}"
  )
  add_test(
    NAME arcane_release_default_validation
    COMMAND "${ARCANE_ACTIVATION_PYTHON_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_release_defaults.py"
            "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()

set(release_dir "${CMAKE_CURRENT_BINARY_DIR}/release")
add_custom_target(
  package_release
  COMMAND "${CMAKE_COMMAND}"
          "-DDLL=$<TARGET_FILE:ArcaneActivation>"
          "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
          "-DRELEASE_DIR=${release_dir}"
          "-DPROJECT_VERSION=${PROJECT_VERSION}"
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/package_release.cmake"
  DEPENDS ArcaneActivation arcane_mcm_scripts validate_arcane_oar
          validate_arcane_defaults
  VERBATIM
)
