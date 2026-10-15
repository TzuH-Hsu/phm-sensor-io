# SPDX-License-Identifier: Apache-2.0
#
# The phm_sensor_io library (alias phm_sensor_io::phm_sensor_io). Public
# headers live in include/phm/sensor_io/ and are included as
# "phm/sensor_io/<name>.hpp"; private sources and headers live here in src/.
# The library has no sources yet, so it is an INTERFACE library; adding
# sources turns it into a compiled one, which then calls
# phm_sensor_io_set_warnings(). Until then the public headers are compiled
# with the project warnings through tests/unit/skeleton_test.cpp, and no
# warning flag is passed on to a project that links the library.
add_library(phm_sensor_io INTERFACE)
add_library(phm_sensor_io::phm_sensor_io ALIAS phm_sensor_io)
target_include_directories(phm_sensor_io INTERFACE
  "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_compile_features(phm_sensor_io INTERFACE cxx_std_20)

# Installation. `cmake --install <build dir> --prefix <dir>` puts the public
# headers under <dir>/include/phm/sensor_io/ and a CMake package under
# <dir>/<libdir>/cmake/phm_sensor_io/, so another project calls
# find_package(phm_sensor_io CONFIG REQUIRED) with <dir> on CMAKE_PREFIX_PATH
# and links phm_sensor_io::phm_sensor_io, the same name as the alias above.
# A compiled library adds its archive and shared-library files to the same
# install(TARGETS) call through the GNUInstallDirs defaults.
include(CMakePackageConfigHelpers)

set(PHM_SENSOR_IO_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/phm_sensor_io")

install(TARGETS phm_sensor_io EXPORT phm_sensor_ioTargets)
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/" TYPE INCLUDE)
install(EXPORT phm_sensor_ioTargets
  FILE phm_sensor_ioTargets.cmake
  NAMESPACE phm_sensor_io::
  DESTINATION "${PHM_SENSOR_IO_CMAKE_DIR}")

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/phm_sensor_ioConfig.cmake.in"
  "${PROJECT_BINARY_DIR}/phm_sensor_ioConfig.cmake"
  INSTALL_DESTINATION "${PHM_SENSOR_IO_CMAKE_DIR}")
# SameMajorVersion: a consumer asking for 0.x accepts any installed 0.y; one
# asking for another major version is refused at find_package().
# ARCH_INDEPENDENT while the library is header-only: nothing installed depends
# on the pointer size, so a 32-bit consumer may use a package installed from a
# 64-bit build. Remove it when the first source file makes the library a
# compiled one.
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/phm_sensor_ioConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion
  ARCH_INDEPENDENT)
install(FILES
  "${PROJECT_BINARY_DIR}/phm_sensor_ioConfig.cmake"
  "${PROJECT_BINARY_DIR}/phm_sensor_ioConfigVersion.cmake"
  DESTINATION "${PHM_SENSOR_IO_CMAKE_DIR}")
