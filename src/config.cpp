// SPDX-License-Identifier: Apache-2.0
//
// validate(): the configuration rules, applied to a BusConfig. The JSON parser
// (config_json.cpp) checks what a BusConfig cannot hold (keys, JSON types,
// integers that do not fit) and then calls validate() for everything else.
#include "phm/sensor_io/config.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "config_rules.hpp"
#include "decode.hpp"

namespace phm::sensor_io {

namespace detail {

bool is_valid_name(std::string_view name, std::size_t max_length) noexcept {
  if (name.empty() || name.size() > max_length) {
    return false;
  }
  return std::ranges::all_of(name, [](char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '.' || c == '-';
  });
}

std::string name_rule(std::size_t max_length) {
  return "must match ^[A-Za-z0-9_.-]{1," + std::to_string(max_length) + "}$";
}

std::string integer_range_rule(std::int64_t min, std::int64_t max) {
  return "must be an integer from " + std::to_string(min) + " to " + std::to_string(max);
}

std::string baud_rule() {
  std::string rule = "must be one of";
  for (std::size_t i = 0; i < kBaudRates.size(); ++i) {
    rule += (i == 0 ? " " : ", ") + std::to_string(kBaudRates.at(i));
  }
  return rule;
}

bool is_valid_baud(std::int64_t baud) noexcept {
  return std::ranges::any_of(kBaudRates,
                             [baud](std::uint32_t rate) { return std::int64_t{rate} == baud; });
}

std::size_t code_points(std::string_view text) noexcept {
  // Every byte that is not a UTF-8 continuation byte (10xxxxxx) starts a code point.
  return static_cast<std::size_t>(std::ranges::count_if(
      text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0U) != 0x80U; }));
}

std::string pointer_token(std::string_view key) {
  std::string token;
  token.reserve(key.size());
  for (const char c : key) {
    if (c == '~') {
      token += "~0";
    } else if (c == '/') {
      token += "~1";
    } else {
      token += c;
    }
  }
  return token;
}

}  // namespace detail

namespace {

using Error = std::optional<ConfigError>;

Error error_at(std::string pointer, std::string_view message) {
  return ConfigError{.pointer = std::move(pointer), .message = std::string(message)};
}

bool is_finite(std::optional<double> number) { return !number || std::isfinite(*number); }

Error check_serial(const SerialSettings& serial, const std::string& at) {
  if (!detail::is_valid_baud(serial.baud)) {
    return error_at(at + "/baud", detail::baud_rule());
  }
  if (serial.parity != Parity::none && serial.parity != Parity::even &&
      serial.parity != Parity::odd) {
    return error_at(at + "/parity", detail::kParityRule);
  }
  if (serial.stop_bits < detail::kStopBitsMin || serial.stop_bits > detail::kStopBitsMax) {
    return error_at(at + "/stop_bits", detail::kStopBitsRule);
  }
  return std::nullopt;
}

Error check_transaction(const TransactionSettings& transaction, const std::string& at) {
  const auto timeout = transaction.response_timeout.count();
  if (timeout < detail::kResponseTimeoutMinMs || timeout > detail::kResponseTimeoutMaxMs) {
    return error_at(
        at + "/response_timeout_ms",
        detail::integer_range_rule(detail::kResponseTimeoutMinMs, detail::kResponseTimeoutMaxMs));
  }
  if (transaction.retries > detail::kRetriesMax) {
    return error_at(at + "/retries",
                    detail::integer_range_rule(detail::kRetriesMin, detail::kRetriesMax));
  }
  return std::nullopt;
}

Error check_port(const PortConfig& port, std::set<std::string_view>& names, const std::string& at) {
  if (!detail::is_valid_name(port.name, detail::kPortNameMax)) {
    return error_at(at + "/name", detail::name_rule(detail::kPortNameMax));
  }
  if (!names.insert(port.name).second) {
    return error_at(at + "/name", "duplicate port name");
  }
  if (port.device.empty() || !port.device.is_absolute()) {
    return error_at(at + "/device", "must be an absolute path");
  }
  if (Error error = check_serial(port.serial, at)) {
    return error;
  }
  return check_transaction(port.transaction, at);
}

bool is_32_bit(ValueType type) { return detail::register_count(type) == 2; }

Error check_value_type(const ValueConfig& value, const std::string& at) {
  switch (value.type) {
    case ValueType::int16:
    case ValueType::uint16:
    case ValueType::int32:
    case ValueType::uint32:
    case ValueType::float32:
      break;
    default:
      return error_at(at + "/type", detail::kTypeRule);
  }
  if (value.word_order != WordOrder::high_first && value.word_order != WordOrder::low_first) {
    return error_at(at + "/word_order", detail::kWordOrderRule);
  }
  if (!is_32_bit(value.type) && value.word_order != WordOrder::high_first) {
    return error_at(at + "/word_order", detail::kWordOrderTypeRule);
  }
  if (is_32_bit(value.type) && value.register_address == detail::kRegisterMax) {
    return error_at(at + "/register",
                    "a 32-bit value takes two registers and cannot start at register 65535");
  }
  return std::nullopt;
}

Error check_value(const ValueConfig& value, std::set<std::string_view>& names,
                  const std::string& at) {
  if (!detail::is_valid_name(value.name, detail::kValueNameMax)) {
    return error_at(at + "/name", detail::name_rule(detail::kValueNameMax));
  }
  if (!names.insert(value.name).second) {
    return error_at(at + "/name", "duplicate value name in this point");
  }
  if (Error error = check_value_type(value, at)) {
    return error;
  }
  if (!std::isfinite(value.scale)) {
    return error_at(at + "/scale", detail::kFiniteRule);
  }
  if (!std::isfinite(value.offset)) {
    return error_at(at + "/offset", detail::kFiniteRule);
  }
  if (detail::code_points(value.unit) > detail::kUnitMax) {
    return error_at(at + "/unit", "must be at most 16 characters");
  }
  if (!is_finite(value.min)) {
    return error_at(at + "/min", detail::kFiniteRule);
  }
  if (!is_finite(value.max)) {
    return error_at(at + "/max", detail::kFiniteRule);
  }
  if (value.min && value.max && *value.min > *value.max) {
    return error_at(at + "/max", "must not be less than min");
  }
  return std::nullopt;
}

Error check_values(const std::vector<ValueConfig>& values, const std::string& at) {
  if (values.empty()) {
    return error_at(at, "must have at least 1 value");
  }
  std::set<std::string_view> names;
  std::size_t index = 0;
  for (const ValueConfig& value : values) {
    if (Error error = check_value(value, names, at + "/" + std::to_string(index++))) {
      return error;
    }
  }
  const detail::RegisterSpan span = detail::register_span(values);
  if (detail::register_count(span) > detail::kMaxRegisterSpan) {
    return error_at(at, "the values span " + std::to_string(detail::register_count(span)) +
                            " registers (" + std::to_string(span.first) + " to " +
                            std::to_string(span.last) + "); one request reads at most 125");
  }
  return std::nullopt;
}

Error check_point(const PointConfig& point, const std::set<std::string_view>& port_names,
                  std::set<std::string_view>& names, const std::string& at) {
  if (!detail::is_valid_name(point.name, detail::kPointNameMax)) {
    return error_at(at + "/name", detail::name_rule(detail::kPointNameMax));
  }
  if (!names.insert(point.name).second) {
    return error_at(at + "/name", "duplicate point name");
  }
  if (!port_names.contains(point.port)) {
    return error_at(at + "/port", "is not the name of a port");
  }
  if (point.slave_address < detail::kSlaveAddressMin ||
      point.slave_address > detail::kSlaveAddressMax) {
    return error_at(at + "/slave_address", detail::kSlaveAddressRule);
  }
  if (point.function != ReadFunction::read_holding_registers &&
      point.function != ReadFunction::read_input_registers) {
    return error_at(at + "/function", detail::kFunctionRule);
  }
  return check_values(point.values, at + "/values");
}

}  // namespace

std::optional<ConfigError> validate(const BusConfig& config) {
  if (config.ports.empty()) {
    return error_at("/ports", "must have at least 1 port");
  }
  std::set<std::string_view> port_names;
  std::size_t index = 0;
  for (const PortConfig& port : config.ports) {
    if (Error error = check_port(port, port_names, "/ports/" + std::to_string(index++))) {
      return error;
    }
  }

  if (config.points.empty()) {
    return error_at("/points", "must have at least 1 point");
  }
  std::set<std::string_view> point_names;
  index = 0;
  for (const PointConfig& point : config.points) {
    if (Error error =
            check_point(point, port_names, point_names, "/points/" + std::to_string(index++))) {
      return error;
    }
  }
  return std::nullopt;
}

}  // namespace phm::sensor_io
