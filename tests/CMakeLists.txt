# SPDX-License-Identifier: Apache-2.0
#
# phm_sensor_io_add_unit_test(<name> SOURCES <file>... [LIBS <target>...])
#
# Builds a GoogleTest executable and registers each of its tests with ctest.
function(phm_sensor_io_add_unit_test name)
  cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "SOURCES;LIBS")
  add_executable(${name} ${arg_SOURCES})
  target_link_libraries(${name} PRIVATE ${arg_LIBS} GTest::gtest_main)
  phm_sensor_io_set_warnings(${name})
  gtest_discover_tests(${name} DISCOVERY_MODE PRE_TEST)
endfunction()

find_package(Threads REQUIRED)

# Test support, not installed: the simulated Modbus RTU slave and access to
# the files under tests/fixtures/.
add_library(phm_sensor_io_test_support STATIC
  support/fixtures.cpp
  support/simulated_slave.cpp
)
target_include_directories(phm_sensor_io_test_support PUBLIC support)
# tests/consumer/main.cpp is not in this build's compile_commands.json, so
# clang-tidy (make lint-cpp) borrows the flags of a nearby file under tests/;
# giving the support files the public include directory makes that work
# whichever one it picks.
target_include_directories(phm_sensor_io_test_support PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(phm_sensor_io_test_support
  PRIVATE PHM_SENSOR_IO_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_features(phm_sensor_io_test_support PUBLIC cxx_std_20)
target_link_libraries(phm_sensor_io_test_support
  PUBLIC Threads::Threads
  PRIVATE nlohmann_json::nlohmann_json)
phm_sensor_io_set_warnings(phm_sensor_io_test_support)

# The umbrella header with no include path but include/: it compiles without
# the libmodbus (or nlohmann/json) include path, and links nothing of the
# library.
phm_sensor_io_add_unit_test(public_headers_test SOURCES unit/public_headers_test.cpp)
target_include_directories(public_headers_test PRIVATE "${PROJECT_SOURCE_DIR}/include")

phm_sensor_io_add_unit_test(result_test
  SOURCES unit/result_test.cpp
  LIBS phm_sensor_io::phm_sensor_io
)
phm_sensor_io_add_unit_test(quality_test
  SOURCES unit/quality_test.cpp
  LIBS phm_sensor_io::phm_sensor_io
)
phm_sensor_io_add_unit_test(config_test
  SOURCES unit/config_test.cpp
  LIBS phm_sensor_io::phm_sensor_io phm_sensor_io_test_support
)
# decode_test reaches the private decode.hpp.
phm_sensor_io_add_unit_test(decode_test
  SOURCES unit/decode_test.cpp
  LIBS phm_sensor_io::phm_sensor_io
)
target_include_directories(decode_test PRIVATE "${PROJECT_SOURCE_DIR}/src")
phm_sensor_io_add_unit_test(simulated_slave_test
  SOURCES unit/simulated_slave_test.cpp
  LIBS phm_sensor_io_test_support
)
phm_sensor_io_add_unit_test(sensor_bus_test
  SOURCES unit/sensor_bus_test.cpp
  LIBS phm_sensor_io::phm_sensor_io phm_sensor_io_test_support
)

# The binary that holds the Modbus RTU transport must load libmodbus as a
# shared library (readelf -d: NEEDED libmodbus.so.N; otool -L on macOS) and
# contain none of its code. That binary is sensor_bus_test when the library is
# static, and the library itself when it is shared (BUILD_SHARED_LIBS).
if(APPLE)
  find_program(PHM_SENSOR_IO_OTOOL otool REQUIRED)
  set(PHM_SENSOR_IO_READELF "")
else()
  find_program(PHM_SENSOR_IO_READELF NAMES readelf "${CMAKE_READELF}" REQUIRED)
  set(PHM_SENSOR_IO_OTOOL "")
endif()
add_test(NAME LibmodbusLinkage.SharedLibraryOnly
  COMMAND "${CMAKE_COMMAND}"
    "-DBINARY=$<IF:$<STREQUAL:$<TARGET_PROPERTY:phm_sensor_io,TYPE>,SHARED_LIBRARY>,$<TARGET_FILE:phm_sensor_io>,$<TARGET_FILE:sensor_bus_test>>"
    "-DREADELF=${PHM_SENSOR_IO_READELF}"
    "-DOTOOL=${PHM_SENSOR_IO_OTOOL}"
    "-DNM=${CMAKE_NM}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/check_libmodbus_linkage.cmake")
