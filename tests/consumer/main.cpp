// SPDX-License-Identifier: Apache-2.0
//
// Built by tests/consumer/CMakeLists.txt against an installed phm_sensor_io.
// It compiles only when phm_sensor_io::phm_sensor_io passes on the installed
// include directory and the C++20 requirement; the consumer project sets no
// language standard of its own. It links only when the installed package
// brings libmodbus along, and it runs the library once: a point on a device
// that does not exist reads as io_error, without any hardware.
#include <cstdlib>
#include <iostream>
#include <utility>

#include "phm/sensor_io/sensor_io.hpp"

static_assert(__cplusplus >= 202002L, "phm_sensor_io::phm_sensor_io must require C++20");

namespace {

constexpr const char* kConfig = R"({
  "bus": "modbus_rtu",
  "ports": [{ "name": "line-a", "device": "/nonexistent/phm-sensor-io-consumer-tty" }],
  "points": [{ "name": "point-1", "port": "line-a", "slave_address": 10,
               "function": "read_input_registers",
               "values": [{ "name": "value_a", "register": 16, "type": "uint16" }] }]
})";

}  // namespace

int main() {
  namespace sio = phm::sensor_io;
  auto config = sio::parse_config(kConfig);
  if (!config) {
    std::cerr << "consumer: parse_config failed\n";
    return EXIT_FAILURE;
  }
  auto bus = sio::SensorBus::create(std::move(config).value());
  if (!bus) {
    std::cerr << "consumer: SensorBus::create failed\n";
    return EXIT_FAILURE;
  }
  const auto reading = bus->read("point-1");
  if (!reading || reading->fault != sio::Fault::io_error) {
    std::cerr << "consumer: expected io_error from a device that does not exist\n";
    return EXIT_FAILURE;
  }
  std::cout << "consumer: point-1 " << sio::to_string(reading->quality) << " ("
            << sio::to_string(reading->fault) << ")\n";
  return EXIT_SUCCESS;
}
