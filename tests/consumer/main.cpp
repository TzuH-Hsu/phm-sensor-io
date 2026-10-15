// SPDX-License-Identifier: Apache-2.0
//
// Built by tests/consumer/CMakeLists.txt against an installed phm_sensor_io.
// It compiles only when phm_sensor_io::phm_sensor_io passes on the installed
// include directory and the C++20 requirement; the consumer project sets no
// language standard of its own.
#include "phm/sensor_io/sensor_io.hpp"

static_assert(__cplusplus >= 202002L, "phm_sensor_io::phm_sensor_io must require C++20");

int main() { return 0; }
