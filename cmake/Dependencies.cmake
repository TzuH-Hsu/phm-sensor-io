# SPDX-License-Identifier: Apache-2.0
#
# Third-party dependencies.
#
# Library: libmodbus (LGPL-2.1-or-later, used under LGPL-3.0) is found with
# pkg-config and linked dynamically as libmodbus.so; it is never linked
# statically and its source is never copied into this repository. nlohmann/json
# (MIT) is header-only and used only by src/config_json.cpp. An installed
# nlohmann/json of the pinned version is used when present; otherwise the
# release archive is downloaded and verified against the hash below.
#
# Tests: GoogleTest is a test-only dependency: it is not linked into the
# library and is not delivered with it. It is pinned and fetched the same way.
#
# A sanitized build always builds the fetched dependencies from source, so the
# test framework is instrumented like the code under test.
include(FetchContent)

if(PHM_SENSOR_IO_SANITIZE)
  set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE NEVER)
endif()

# --- libmodbus ----------------------------------------------------------------
# The imported target is PkgConfig::PHM_SENSOR_IO_LIBMODBUS. The installed
# package configuration file (cmake/phm_sensor_ioConfig.cmake.in) creates the
# same target for a project that links a static build of this library.
find_package(PkgConfig REQUIRED)
pkg_check_modules(PHM_SENSOR_IO_LIBMODBUS REQUIRED IMPORTED_TARGET libmodbus)

# The LGPL condition: refuse a static archive, so the library can only be
# linked against the shared libmodbus.
foreach(library IN LISTS PHM_SENSOR_IO_LIBMODBUS_LINK_LIBRARIES)
  if(library MATCHES "\\.a$")
    message(FATAL_ERROR
      "libmodbus resolved to the static archive ${library}. libmodbus must be "
      "linked dynamically (libmodbus.so); install the shared library.")
  endif()
endforeach()

# --- nlohmann/json ------------------------------------------------------------
set(PHM_SENSOR_IO_JSON_VERSION 3.12.0)

FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v${PHM_SENSOR_IO_JSON_VERSION}/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  DOWNLOAD_EXTRACT_TIMESTAMP ON
  FIND_PACKAGE_ARGS ${PHM_SENSOR_IO_JSON_VERSION} EXACT
)

set(JSON_Install OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(nlohmann_json)

# --- GoogleTest ---------------------------------------------------------------
if(PHM_SENSOR_IO_BUILD_TESTS)
  set(PHM_SENSOR_IO_GTEST_VERSION 1.17.0)

  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/releases/download/v${PHM_SENSOR_IO_GTEST_VERSION}/googletest-${PHM_SENSOR_IO_GTEST_VERSION}.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    FIND_PACKAGE_ARGS ${PHM_SENSOR_IO_GTEST_VERSION} EXACT NAMES GTest
  )

  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)

  FetchContent_MakeAvailable(googletest)

  include(GoogleTest)
endif()
