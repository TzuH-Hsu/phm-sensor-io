# SPDX-License-Identifier: Apache-2.0
#
# The phm_sensor_io library (alias phm_sensor_io::phm_sensor_io), static or
# shared as BUILD_SHARED_LIBS selects. Public headers live in
# include/phm/sensor_io/ and are included as "phm/sensor_io/<name>.hpp";
# private sources and headers live here in src/. libmodbus and nlohmann/json
# are linked PRIVATE: no public header includes either, so a project that
# links the library needs neither include path.
add_library(phm_sensor_io
  config.cpp
  config_json.cpp
  decode.cpp
  modbus_rtu_port.cpp
  quality.cpp
  sensor_bus.cpp
)
add_library(phm_sensor_io::phm_sensor_io ALIAS phm_sensor_io)
target_include_directories(phm_sensor_io
  PUBLIC
    "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(phm_sensor_io PUBLIC cxx_std_20)
# nlohmann/json is header-only, so it is needed only while building; the
# installed package does not refer to it.
target_link_libraries(phm_sensor_io
  PRIVATE
    PkgConfig::PHM_SENSOR_IO_LIBMODBUS
    "$<BUILD_INTERFACE:nlohmann_json::nlohmann_json>")
set_target_properties(phm_sensor_io PROPERTIES
  VERSION "${PROJECT_VERSION}"
  SOVERSION "${PROJECT_VERSION_MAJOR}")
phm_sensor_io_set_warnings(phm_sensor_io)

# Installation. `cmake --install <build dir> --prefix <dir>` puts the public
# headers under <dir>/include/phm/sensor_io/ and a CMake package under
# <dir>/<libdir>/cmake/phm_sensor_io/, so another project calls
# find_package(phm_sensor_io CONFIG REQUIRED) with <dir> on CMAKE_PREFIX_PATH
# and links phm_sensor_io::phm_sensor_io, the same name as the alias above.
# The archive or shared-library file is installed by the same install(TARGETS)
# call through the GNUInstallDirs defaults.
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
write_basic_package_version_file(
  "${PROJECT_BINARY_DIR}/phm_sensor_ioConfigVersion.cmake"
  COMPATIBILITY SameMajorVersion)
install(FILES
  "${PROJECT_BINARY_DIR}/phm_sensor_ioConfig.cmake"
  "${PROJECT_BINARY_DIR}/phm_sensor_ioConfigVersion.cmake"
  DESTINATION "${PHM_SENSOR_IO_CMAKE_DIR}")
