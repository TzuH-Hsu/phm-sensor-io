# SPDX-License-Identifier: Apache-2.0
#
# The phm_sensor_io library (alias phm_sensor_io::phm_sensor_io). Public
# headers live in include/phm/sensor_io/ and are included as
# "phm/sensor_io/<name>.hpp"; private sources and headers live here in src/.
# The library has no sources yet, so it is an INTERFACE library; adding
# sources turns it into a compiled one.
add_library(phm_sensor_io INTERFACE)
add_library(phm_sensor_io::phm_sensor_io ALIAS phm_sensor_io)
target_include_directories(phm_sensor_io INTERFACE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(phm_sensor_io INTERFACE cxx_std_20)
phm_sensor_io_set_warnings(phm_sensor_io)
