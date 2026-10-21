// SPDX-License-Identifier: Apache-2.0
//
// The configuration of one bus: its ports and the points read from them.
// Points are configuration, not code: a deployment describes them in a JSON
// document (parse_config, load_config) or builds a BusConfig in code, and both
// are checked against the same rules (validate). The rules are listed in the
// README, section "Configuration".
#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "phm/sensor_io/result.hpp"

namespace phm::sensor_io {

enum class Parity : std::uint8_t { none, even, odd };

// Serial line settings of one port. Data bits are always 8 (RTU framing).
// The defaults are the Modbus serial line defaults.
struct SerialSettings {
  std::uint32_t baud = 19200;  // 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200
  Parity parity = Parity::even;
  std::uint8_t stop_bits = 1;  // 1 or 2
};

// Timeout and retries of every transaction on one port.
struct TransactionSettings {
  std::chrono::milliseconds response_timeout{500};  // 10-10000 ms
  std::uint8_t retries = 1;  // 0-5; one read sends at most 1 + retries requests
};

struct PortConfig {
  std::string name;              // ^[A-Za-z0-9_.-]{1,32}$, unique among ports
  std::filesystem::path device;  // absolute path of the serial device, opened as given
  SerialSettings serial;
  TransactionSettings transaction;
};

enum class ReadFunction : std::uint8_t {
  read_holding_registers = 0x03,
  read_input_registers = 0x04,
};

// int16 and uint16 take one register; int32, uint32 and float32 (IEEE 754) take two.
enum class ValueType : std::uint8_t { int16, uint16, int32, uint32, float32 };
// Which register of a 32-bit value holds its high 16 bits. Bytes within a
// register are always big-endian.
enum class WordOrder : std::uint8_t { high_first, low_first };

// One value of a point. Its decoded value is raw * scale + offset, where raw
// is the register content read as `type` and `word_order` define.
struct ValueConfig {
  std::string name;                    // ^[A-Za-z0-9_.-]{1,32}$, unique within the point
  std::uint16_t register_address = 0;  // JSON key "register"; zero-based, as sent on the wire.
                                       // A 32-bit value also uses register_address + 1, so
                                       // it cannot start at 65535
  ValueType type = ValueType::uint16;  // required in JSON; no default there
  WordOrder word_order = WordOrder::high_first;  // 32-bit types only: a 16-bit value
                                                 // keeps high_first, and its JSON object
                                                 // has no "word_order" key
  double scale = 1.0;                            // finite
  double offset = 0.0;                           // finite
  std::string unit;                              // 0-16 characters, copied to the result
  std::optional<double> min;                     // inclusive; empty = no bound; finite
  std::optional<double> max;                     // inclusive; empty = no bound; finite;
                                                 // min <= max when both are set
};

// One point: the values read from one slave in one request, which covers the
// lowest to the highest register the values use.
struct PointConfig {
  std::string name;                // ^[A-Za-z0-9_.-]{1,64}$, unique among points
  std::string port;                // a PortConfig::name
  std::uint8_t slave_address = 0;  // 1-247 (0 is broadcast)
  ReadFunction function = ReadFunction::read_holding_registers;  // required in JSON
  std::vector<ValueConfig> values;                               // at least 1; register span 1-125
};

// The whole configuration. The bus is always Modbus RTU ("bus": "modbus_rtu").
struct BusConfig {
  std::vector<PortConfig> ports;    // at least 1
  std::vector<PointConfig> points;  // at least 1
};

// The first error found. `pointer` is an RFC 6901 JSON Pointer to the offending
// element, for example "/points/1/values/0/register"; it is the same for a
// document and for the equivalent BusConfig built in code. It is "" when the
// whole document is at fault (unreadable file, invalid JSON).
struct ConfigError {
  std::string pointer;
  std::string message;
};

// Parse a configuration document (JSON text) and validate it. Unknown keys,
// wrong JSON types (an integer key given 19200.0 or "19200") and every rule of
// the configuration are errors. Omitted keys take their defaults.
[[nodiscard]] Result<BusConfig, ConfigError> parse_config(std::string_view json_text);

// Read the file at `file` and parse_config() its contents.
[[nodiscard]] Result<BusConfig, ConfigError> load_config(const std::filesystem::path& file);

// Check a BusConfig built in code against the same rules as parse_config().
// Empty when valid.
[[nodiscard]] std::optional<ConfigError> validate(const BusConfig& config);

}  // namespace phm::sensor_io
