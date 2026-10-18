// SPDX-License-Identifier: Apache-2.0
//
// One Modbus RTU serial line: open, read registers with retries, close.
// Private. Only modbus_rtu_port.cpp includes <modbus.h>; this header names no
// libmodbus type.
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "phm/sensor_io/config.hpp"
#include "phm/sensor_io/quality.hpp"

namespace phm::sensor_io::detail {

// The outcome of one read: the last attempt's fault and, when it is
// Fault::none, the registers read.
struct Exchange {
  Fault fault = Fault::none;
  std::optional<std::uint8_t> exception_code;  // set only for Fault::exception_response
  std::uint8_t attempts = 0;                   // request frames sent
  std::vector<std::uint16_t> words;            // `count` registers from `first`
};

// One serial line. Not thread-safe: SensorBus holds the port's lock around
// every call.
class ModbusRtuPort {
 public:
  // Opens nothing yet.
  explicit ModbusRtuPort(PortConfig config);
  ~ModbusRtuPort();  // closes the device if open
  ModbusRtuPort(const ModbusRtuPort&) = delete;
  ModbusRtuPort& operator=(const ModbusRtuPort&) = delete;
  ModbusRtuPort(ModbusRtuPort&&) = delete;
  ModbusRtuPort& operator=(ModbusRtuPort&&) = delete;

  // Reads `count` registers from `first` of `slave` with `function`, opening
  // the device first if it is not open. Before every request frame, the first
  // and each retry, it keeps the inter-frame silence since the port's last
  // exchange and then discards the receive buffer. no_reply, crc_mismatch and
  // malformed_reply are retried up to the port's retries; exception_response
  // is not retried; io_error closes the device, which the next call opens
  // again.
  [[nodiscard]] Exchange read_registers(std::uint8_t slave, ReadFunction function,
                                        std::uint16_t first, std::uint16_t count);

 private:
  struct State;
  std::unique_ptr<State> state_;
};

// The RTU inter-frame silence at `baud`: 3.5 characters of 11 bits, rounded up
// to whole microseconds, or 1.75 ms above 19200 baud. About 4.0 ms at 9600 baud
// and 2.0 ms at 19200.
[[nodiscard]] std::chrono::microseconds inter_frame_silence(std::uint32_t baud) noexcept;

// The path to open for `port`. Stable port names (#55) plug in here; for now
// it is PortConfig::device unchanged.
[[nodiscard]] std::filesystem::path resolve_device(const PortConfig& port);

}  // namespace phm::sensor_io::detail
