# SPDX-License-Identifier: Apache-2.0
#
# A separate CMake project that uses an installed phm_sensor_io the way a
# downstream project does: find_package() through CMAKE_PREFIX_PATH, then link
# the exported target. It is not part of this repository's build; `make
# test-install` installs build/dev into a prefix under build/ and configures,
# builds and runs this project against it.
cmake_minimum_required(VERSION 3.28...4.4)

project(phm_sensor_io_consumer LANGUAGES CXX)

# The version passed to find_package(); empty accepts any installed version.
set(PHM_SENSOR_IO_CONSUMER_VERSION "" CACHE STRING
  "Version of phm_sensor_io to ask find_package() for")
# `make test-install` turns this on so that find_package() searches only
# CMAKE_PREFIX_PATH: a copy installed elsewhere on the machine can then neither
# satisfy the check nor hide a failure. A downstream project leaves it off.
option(PHM_SENSOR_IO_CONSUMER_PREFIX_PATH_ONLY
  "Search for phm_sensor_io in CMAKE_PREFIX_PATH only" OFF)

set(search_options "")
if(PHM_SENSOR_IO_CONSUMER_PREFIX_PATH_ONLY)
  set(search_options NO_PACKAGE_ROOT_PATH NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH
    NO_CMAKE_PACKAGE_REGISTRY NO_CMAKE_SYSTEM_PATH NO_CMAKE_SYSTEM_PACKAGE_REGISTRY)
endif()

find_package(phm_sensor_io ${PHM_SENSOR_IO_CONSUMER_VERSION} CONFIG REQUIRED ${search_options})

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE phm_sensor_io::phm_sensor_io)
