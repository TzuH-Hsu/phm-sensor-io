// SPDX-License-Identifier: Apache-2.0
//
// parse_config() and load_config(): the JSON document to BusConfig. The only
// file that includes the JSON parser.
//
// The parser reports only what a BusConfig cannot hold, in document order:
// unknown and missing keys, wrong JSON types, an integer that does not fit
// its field's type, a text value that is not one of its field's choices. Every
// other rule is validate()'s, so a document and the equivalent BusConfig give
// the same first error. One rule needs the document itself: a "word_order"
// key on a 16-bit value, which a BusConfig cannot tell from the default; it is
// reported last, once validate() has passed.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "config_rules.hpp"
#include "phm/sensor_io/config.hpp"

namespace phm::sensor_io {

namespace {

// ordered_json keeps the members in document order, so "the first error
// found" is the first one in the document.
using Json = nlohmann::ordered_json;
using Error = std::optional<ConfigError>;

Error error_at(std::string pointer, std::string_view message) {
  return ConfigError{.pointer = std::move(pointer), .message = std::string(message)};
}

std::string member_at(const std::string& at, std::string_view key) {
  return at + "/" + detail::pointer_token(key);
}

const Json* find_member(const Json& object, std::string_view key) {
  const auto it = object.find(key);
  return it == object.end() ? nullptr : &*it;
}

bool contains(std::initializer_list<std::string_view> keys, std::string_view key) {
  return std::ranges::find(keys, key) != keys.end();
}

// `node` must be an object whose keys are all in `allowed` and that has every
// key in `required`.
Error check_object(const Json& node, const std::string& at,
                   std::initializer_list<std::string_view> allowed,
                   std::initializer_list<std::string_view> required) {
  if (!node.is_object()) {
    return error_at(at, "must be an object");
  }
  for (const auto& item : node.items()) {
    if (!contains(allowed, item.key())) {
      return error_at(member_at(at, item.key()), "unknown key");
    }
  }
  for (const std::string_view key : required) {
    if (find_member(node, key) == nullptr) {
      return error_at(member_at(at, key), "is required");
    }
  }
  return std::nullopt;
}

// Each read_* function leaves `out` unchanged when the key is absent, so it
// keeps its default.

Error read_string(const Json& object, const std::string& at, std::string_view key,
                  std::string& out) {
  const Json* member = find_member(object, key);
  if (member == nullptr) {
    return std::nullopt;
  }
  if (!member->is_string()) {
    return error_at(member_at(at, key), "must be a string");
  }
  out = member->get<std::string>();
  return std::nullopt;
}

// An integer that fits `T`, the type of its field; whether it meets the
// field's rule is validate()'s. 19200.0 and "19200" are not integers. A value
// that does not fit `T` cannot meet the rule either, so it is reported with
// the rule's message, as validate() would report it.
template <class T>
Error read_integer(const Json& object, const std::string& at, std::string_view key,
                   std::string_view rule, T& out) {
  const Json* member = find_member(object, key);
  if (member == nullptr) {
    return std::nullopt;
  }
  if (!member->is_number_integer()) {
    return error_at(member_at(at, key), "must be an integer");
  }
  if (member->is_number_unsigned()) {
    const auto value = member->get<std::uint64_t>();
    if (!std::in_range<T>(value)) {
      return error_at(member_at(at, key), rule);
    }
    out = static_cast<T>(value);
    return std::nullopt;
  }
  const auto value = member->get<std::int64_t>();
  if (!std::in_range<T>(value)) {
    return error_at(member_at(at, key), rule);
  }
  out = static_cast<T>(value);
  return std::nullopt;
}

Error read_number(const Json& object, const std::string& at, std::string_view key, double& out) {
  const Json* member = find_member(object, key);
  if (member == nullptr) {
    return std::nullopt;
  }
  if (!member->is_number()) {
    return error_at(member_at(at, key), "must be a number");
  }
  out = member->get<double>();
  return std::nullopt;
}

Error read_bound(const Json& object, const std::string& at, std::string_view key,
                 std::optional<double>& out) {
  const Json* member = find_member(object, key);
  if (member == nullptr || member->is_null()) {
    return std::nullopt;
  }
  if (!member->is_number()) {
    return error_at(member_at(at, key), "must be a number or null");
  }
  out = member->get<double>();
  return std::nullopt;
}

// The JSON text of one enumerator, and the enumerator.
template <class Enum>
using Choice = std::pair<std::string_view, Enum>;

template <class Enum>
Error read_choice(const Json& object, const std::string& at, std::string_view key,
                  std::initializer_list<Choice<Enum>> choices, std::string_view rule, Enum& out) {
  const Json* member = find_member(object, key);
  if (member == nullptr) {
    return std::nullopt;
  }
  if (!member->is_string()) {
    return error_at(member_at(at, key), "must be a string");
  }
  const auto& text = member->get_ref<const std::string&>();
  for (const auto& [choice_text, choice_value] : choices) {
    if (choice_text == text) {
      out = choice_value;
      return std::nullopt;
    }
  }
  return error_at(member_at(at, key), rule);
}

Error read_serial(const Json& node, const std::string& at, SerialSettings& serial) {
  if (Error error = read_integer(node, at, "baud", detail::baud_rule(), serial.baud)) {
    return error;
  }
  if (Error error = read_choice<Parity>(
          node, at, "parity",
          {{"none", Parity::none}, {"even", Parity::even}, {"odd", Parity::odd}},
          detail::kParityRule, serial.parity)) {
    return error;
  }
  if (Error error = read_integer(node, at, "stop_bits", detail::kStopBitsRule, serial.stop_bits)) {
    return error;
  }
  return std::nullopt;
}

Error read_transaction(const Json& node, const std::string& at, TransactionSettings& transaction) {
  auto timeout_ms = transaction.response_timeout.count();
  if (Error error = read_integer(
          node, at, "response_timeout_ms",
          detail::integer_range_rule(detail::kResponseTimeoutMinMs, detail::kResponseTimeoutMaxMs),
          timeout_ms)) {
    return error;
  }
  transaction.response_timeout = std::chrono::milliseconds{timeout_ms};
  if (Error error = read_integer(
          node, at, "retries", detail::integer_range_rule(detail::kRetriesMin, detail::kRetriesMax),
          transaction.retries)) {
    return error;
  }
  return std::nullopt;
}

Error read_port(const Json& node, const std::string& at, PortConfig& port) {
  if (Error error = check_object(
          node, at,
          {"name", "device", "baud", "parity", "stop_bits", "response_timeout_ms", "retries"},
          {"name", "device"})) {
    return error;
  }
  if (Error error = read_string(node, at, "name", port.name)) {
    return error;
  }
  std::string device;
  if (Error error = read_string(node, at, "device", device)) {
    return error;
  }
  port.device = device;
  if (Error error = read_serial(node, at, port.serial)) {
    return error;
  }
  return read_transaction(node, at, port.transaction);
}

Error read_value_type(const Json& node, const std::string& at, ValueConfig& value) {
  if (Error error = read_choice<ValueType>(node, at, "type",
                                           {
                                               {"int16", ValueType::int16},
                                               {"uint16", ValueType::uint16},
                                               {"int32", ValueType::int32},
                                               {"uint32", ValueType::uint32},
                                               {"float32", ValueType::float32},
                                           },
                                           detail::kTypeRule, value.type)) {
    return error;
  }
  if (Error error = read_choice<WordOrder>(
          node, at, "word_order",
          {{"high_first", WordOrder::high_first}, {"low_first", WordOrder::low_first}},
          detail::kWordOrderRule, value.word_order)) {
    return error;
  }
  return std::nullopt;
}

Error read_value(const Json& node, const std::string& at, ValueConfig& value) {
  if (Error error = check_object(
          node, at,
          {"name", "register", "type", "word_order", "scale", "offset", "unit", "min", "max"},
          {"name", "register", "type"})) {
    return error;
  }
  if (Error error = read_string(node, at, "name", value.name)) {
    return error;
  }
  if (Error error =
          read_integer(node, at, "register",
                       detail::integer_range_rule(detail::kRegisterMin, detail::kRegisterMax),
                       value.register_address)) {
    return error;
  }
  if (Error error = read_value_type(node, at, value)) {
    return error;
  }
  if (Error error = read_number(node, at, "scale", value.scale)) {
    return error;
  }
  if (Error error = read_number(node, at, "offset", value.offset)) {
    return error;
  }
  if (Error error = read_string(node, at, "unit", value.unit)) {
    return error;
  }
  if (Error error = read_bound(node, at, "min", value.min)) {
    return error;
  }
  return read_bound(node, at, "max", value.max);
}

// A required array; its elements are read with `read_element`.
template <class T, class ReadElement>
Error read_array(const Json& object, const std::string& at, std::string_view key,
                 std::vector<T>& out, ReadElement read_element) {
  const Json& array = *find_member(object, key);  // required: check_object found it
  const std::string array_at = member_at(at, key);
  if (!array.is_array()) {
    return error_at(array_at, "must be an array");
  }
  out.reserve(array.size());
  std::size_t index = 0;
  for (const Json& node : array) {
    T element{};
    if (Error error = read_element(node, array_at + "/" + std::to_string(index++), element)) {
      return error;
    }
    out.push_back(std::move(element));
  }
  return std::nullopt;
}

Error read_point(const Json& node, const std::string& at, PointConfig& point) {
  if (Error error = check_object(node, at, {"name", "port", "slave_address", "function", "values"},
                                 {"name", "port", "slave_address", "function", "values"})) {
    return error;
  }
  if (Error error = read_string(node, at, "name", point.name)) {
    return error;
  }
  if (Error error = read_string(node, at, "port", point.port)) {
    return error;
  }
  if (Error error =
          read_integer(node, at, "slave_address", detail::kSlaveAddressRule, point.slave_address)) {
    return error;
  }
  if (Error error = read_choice<ReadFunction>(
          node, at, "function",
          {
              {"read_holding_registers", ReadFunction::read_holding_registers},
              {"read_input_registers", ReadFunction::read_input_registers},
          },
          detail::kFunctionRule, point.function)) {
    return error;
  }
  return read_array(node, at, "values", point.values, read_value);
}

// A "word_order" key on a 16-bit value. `config` is the BusConfig read from
// `root`, so the two have the same points and values in the same order.
Error check_word_order_keys(const Json& root, const BusConfig& config) {
  std::size_t point_index = 0;
  for (const PointConfig& point : config.points) {
    const Json& values = root.at("points").at(point_index).at("values");
    const std::string at = "/points/" + std::to_string(point_index++) + "/values/";
    std::size_t value_index = 0;
    for (const ValueConfig& value : point.values) {
      const bool is_16_bit = value.type == ValueType::int16 || value.type == ValueType::uint16;
      if (is_16_bit && values.at(value_index).contains("word_order")) {
        return error_at(at + std::to_string(value_index) + "/word_order",
                        detail::kWordOrderTypeRule);
      }
      ++value_index;
    }
  }
  return std::nullopt;
}

Error read_document(const Json& root, BusConfig& config) {
  if (Error error =
          check_object(root, "", {"bus", "ports", "points"}, {"bus", "ports", "points"})) {
    return error;
  }
  std::string bus;
  if (Error error = read_string(root, "", "bus", bus)) {
    return error;
  }
  if (bus != "modbus_rtu") {
    return error_at("/bus", R"(must be "modbus_rtu")");
  }
  if (Error error = read_array(root, "", "ports", config.ports, read_port)) {
    return error;
  }
  return read_array(root, "", "points", config.points, read_point);
}

}  // namespace

Result<BusConfig, ConfigError> parse_config(std::string_view json_text) {
  Json root;
  try {
    root = Json::parse(json_text);
  } catch (const Json::exception& error) {  // parse_error, or out_of_range 406 for a number
                                            // that does not fit a double, such as 1e400
    return Unexpected<ConfigError>{{.pointer = "", .message = error.what()}};
  }
  BusConfig config;
  if (Error error = read_document(root, config)) {
    return Unexpected<ConfigError>{std::move(*error)};
  }
  if (Error error = validate(config)) {
    return Unexpected<ConfigError>{std::move(*error)};
  }
  if (Error error = check_word_order_keys(root, config)) {
    return Unexpected<ConfigError>{std::move(*error)};
  }
  return config;
}

Result<BusConfig, ConfigError> load_config(const std::filesystem::path& file) {
  std::error_code code;
  if (std::filesystem::is_directory(file, code)) {
    return Unexpected<ConfigError>{{.pointer = "", .message = file.string() + " is a directory"}};
  }
  const std::ifstream in(file, std::ios::binary);
  if (!in) {
    return Unexpected<ConfigError>{{.pointer = "", .message = "cannot open " + file.string()}};
  }
  std::ostringstream text;
  text << in.rdbuf();
  if (in.bad()) {
    return Unexpected<ConfigError>{{.pointer = "", .message = "cannot read " + file.string()}};
  }
  return parse_config(text.str());
}

}  // namespace phm::sensor_io
