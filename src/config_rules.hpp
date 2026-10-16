// SPDX-License-Identifier: Apache-2.0
//
// Limits and messages shared by validate() (config.cpp) and the JSON parser
// (config_json.cpp), so a document and the equivalent BusConfig built in code
// give the same error. Private.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace phm::sensor_io::detail {

inline constexpr std::size_t kPortNameMax = 32;
inline constexpr std::size_t kPointNameMax = 64;
inline constexpr std::size_t kValueNameMax = 32;
inline constexpr std::size_t kUnitMax = 16;

inline constexpr std::array<std::uint32_t, 8> kBaudRates{
    1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200,
};
inline constexpr std::int64_t kStopBitsMin = 1;
inline constexpr std::int64_t kStopBitsMax = 2;
inline constexpr std::int64_t kResponseTimeoutMinMs = 10;
inline constexpr std::int64_t kResponseTimeoutMaxMs = 10000;
inline constexpr std::int64_t kRetriesMin = 0;
inline constexpr std::int64_t kRetriesMax = 5;
inline constexpr std::int64_t kSlaveAddressMin = 1;
inline constexpr std::int64_t kSlaveAddressMax = 247;
inline constexpr std::int64_t kRegisterMin = 0;
inline constexpr std::int64_t kRegisterMax = 65535;
inline constexpr std::uint32_t kMaxRegisterSpan = 125;

// True when `name` matches ^[A-Za-z0-9_.-]{1,max_length}$.
[[nodiscard]] bool is_valid_name(std::string_view name, std::size_t max_length) noexcept;
// "must match ^[A-Za-z0-9_.-]{1,<max_length>}$"
[[nodiscard]] std::string name_rule(std::size_t max_length);
// "must be an integer from <min> to <max>"
[[nodiscard]] std::string integer_range_rule(std::int64_t min, std::int64_t max);
// "must be one of 1200, 2400, ..."
[[nodiscard]] std::string baud_rule();
[[nodiscard]] bool is_valid_baud(std::int64_t baud) noexcept;
// Number of Unicode code points in UTF-8 text.
[[nodiscard]] std::size_t code_points(std::string_view text) noexcept;
// One reference token of an RFC 6901 JSON Pointer: "~" becomes "~0", "/" becomes "~1".
[[nodiscard]] std::string pointer_token(std::string_view key);

inline constexpr std::string_view kSlaveAddressRule =
    "must be an integer from 1 to 247 (0 is broadcast)";
inline constexpr std::string_view kStopBitsRule = "must be 1 or 2";
inline constexpr std::string_view kParityRule = R"(must be "none", "even" or "odd")";
inline constexpr std::string_view kFunctionRule =
    R"(must be "read_holding_registers" or "read_input_registers")";
inline constexpr std::string_view kTypeRule =
    R"(must be "int16", "uint16", "int32", "uint32" or "float32")";
inline constexpr std::string_view kWordOrderRule = R"(must be "high_first" or "low_first")";
inline constexpr std::string_view kWordOrderTypeRule = "is allowed for 32-bit types only";
inline constexpr std::string_view kFiniteRule = "must be a finite number";

}  // namespace phm::sensor_io::detail
