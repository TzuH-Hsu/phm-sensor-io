# SPDX-License-Identifier: Apache-2.0
cmake_minimum_required(VERSION 3.28...4.4)

project(phm_sensor_io LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# No C++20 modules are used. Scanning for them adds GCC-only flags to
# compile_commands.json, which clang-tidy cannot parse.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PHM_SENSOR_IO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(PHM_SENSOR_IO_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
# The tests build only when this is the top-level project, so an application
# that adds the library with add_subdirectory() or FetchContent gets neither
# the tests nor their GoogleTest download.
option(PHM_SENSOR_IO_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Sanitizers)

add_subdirectory(src)

if(PHM_SENSOR_IO_BUILD_TESTS)
  enable_testing()
  include(Dependencies)
  add_subdirectory(tests)
endif()
