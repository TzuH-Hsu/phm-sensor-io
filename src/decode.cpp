// SPDX-License-Identifier: Apache-2.0
#include "decode.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>

namespace phm::sensor_io::detail {

RegisterSpan register_span(const std::vector<ValueConfig>& values) noexcept {
  assert(!values.empty());
  RegisterSpan span{
      .first = values.front().register_address,
      .last = values.front().register_address,
  };
  for (const ValueConfig& value : values) {
    span.first = std::min<std::uint32_t>(span.first, value.register_address);
    span.last = std::max(span.last, value.register_address + register_count(value.type) - 1);
  }
  return span;
}

namespace {

// The register content read as `value.type` and `value.word_order` define.
double raw_value(const ValueConfig& value, std::span<const std::uint16_t> words) noexcept {
  switch (value.type) {
    case ValueType::int16:
      return static_cast<std::int16_t>(words.front());
    case ValueType::uint16:
      return words.front();
    case ValueType::int32:
    case ValueType::uint32:
    case ValueType::float32:
      break;
  }
  const bool high_first = value.word_order == WordOrder::high_first;
  const std::uint32_t high = high_first ? words.front() : words.back();
  const std::uint32_t low = high_first ? words.back() : words.front();
  const std::uint32_t bits = high << 16U | low;
  if (value.type == ValueType::int32) {
    return static_cast<std::int32_t>(bits);
  }
  if (value.type == ValueType::uint32) {
    return bits;
  }
  return static_cast<double>(std::bit_cast<float>(bits));
}

}  // namespace

std::optional<double> decode(const ValueConfig& value,
                             std::span<const std::uint16_t> words) noexcept {
  assert(words.size() == register_count(value.type));
  const double decoded = (raw_value(value, words) * value.scale) + value.offset;
  if (!std::isfinite(decoded)) {
    return std::nullopt;
  }
  if ((value.min && decoded < *value.min) || (value.max && decoded > *value.max)) {
    return std::nullopt;
  }
  return decoded;
}

}  // namespace phm::sensor_io::detail
