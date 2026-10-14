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
target_include_directories(phm_sensor_io INTERFACE "${PROJECT_SOURCE_DIR}/include")
target_compile_features(phm_sensor_io INTERFACE cxx_std_20)
