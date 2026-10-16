// SPDX-License-Identifier: Apache-2.0
//
// Register words to value: type, word order, scale, offset, range. Private.
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "phm/sensor_io/config.hpp"

namespace phm::sensor_io::detail {

// Registers one value of `type` takes: 1 for int16 and uint16, 2 for the 32-bit types.
[[nodiscard]] constexpr std::uint32_t register_count(ValueType type) noexcept {
  return type == ValueType::int16 || type == ValueType::uint16 ? 1U : 2U;
}

// The registers one request must read to cover `values`: from the lowest
// register a value uses to the highest. `last` can exceed 65535 for a 32-bit
// value at 65535, which validate() rejects.
struct RegisterSpan {
  std::uint32_t first = 0;
  std::uint32_t last = 0;
};

// Registers in `span`: last - first + 1.
[[nodiscard]] constexpr std::uint32_t register_count(RegisterSpan span) noexcept {
  return span.last - span.first + 1;
}

// Precondition: `values` is not empty.
[[nodiscard]] RegisterSpan register_span(const std::vector<ValueConfig>& values) noexcept;

// The decoded value of `words`, the register_count(value.type) words of one
// value in register order: raw * scale + offset. Empty when that is outside
// [min, max] or not finite (a float32 NaN or infinity).
// Precondition: words.size() == register_count(value.type).
[[nodiscard]] std::optional<double> decode(const ValueConfig& value,
                                           std::span<const std::uint16_t> words) noexcept;

}  // namespace phm::sensor_io::detail
