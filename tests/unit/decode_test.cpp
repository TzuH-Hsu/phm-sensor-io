// SPDX-License-Identifier: Apache-2.0
//
// Register words to value: the Normal and Boundary word values of issue #1.
#include "decode.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace sio = phm::sensor_io;
using sio::detail::decode;

namespace {

sio::ValueConfig value_of(sio::ValueType type, sio::WordOrder order = sio::WordOrder::high_first) {
  sio::ValueConfig value;
  value.name = "value_a";
  value.type = type;
  value.word_order = order;
  return value;
}

std::optional<double> decoded(const sio::ValueConfig& value, std::vector<std::uint16_t> words) {
  return decode(value, words);
}

}  // namespace

TEST(Decode, ScalesUint16AndInt16) {
  auto value_a = value_of(sio::ValueType::uint16);
  value_a.scale = 0.5;
  EXPECT_EQ(decoded(value_a, {0x00FA}), 125.0);
  auto value_b = value_of(sio::ValueType::int16);
  value_b.scale = 0.25;
  EXPECT_EQ(decoded(value_b, {0xFF38}), -50.0);
}

TEST(Decode, AddsOffsetAfterScale) {
  auto value_b = value_of(sio::ValueType::int16);
  value_b.scale = 0.25;
  value_b.offset = 1.5;
  EXPECT_EQ(decoded(value_b, {0xFF38}), -48.5);
}

TEST(Decode, Float32InBothWordOrders) {
  EXPECT_EQ(decoded(value_of(sio::ValueType::float32), {0x41C8, 0x0000}), 25.0);
  EXPECT_EQ(decoded(value_of(sio::ValueType::float32, sio::WordOrder::low_first), {0x0000, 0x41C8}),
            25.0);
}

TEST(Decode, Uint32AndInt32InBothWordOrders) {
  EXPECT_EQ(decoded(value_of(sio::ValueType::uint32), {0x0001, 0x0002}), 65538.0);
  EXPECT_EQ(decoded(value_of(sio::ValueType::uint32, sio::WordOrder::low_first), {0x0001, 0x0002}),
            131073.0);
  EXPECT_EQ(decoded(value_of(sio::ValueType::int32), {0xFFFF, 0xFFFE}), -2.0);
  EXPECT_EQ(decoded(value_of(sio::ValueType::uint32), {0xFFFF, 0xFFFF}), 4294967295.0);
}

TEST(Decode, SixteenBitLimits) {
  EXPECT_EQ(decoded(value_of(sio::ValueType::int16), {0x8000}), -32768.0);
  EXPECT_EQ(decoded(value_of(sio::ValueType::int16), {0x7FFF}), 32767.0);
  EXPECT_EQ(decoded(value_of(sio::ValueType::uint16), {0xFFFF}), 65535.0);
  EXPECT_EQ(decoded(value_of(sio::ValueType::uint16), {0x0000}), 0.0);
}

TEST(Decode, BoundsAreInclusive) {
  auto value_a = value_of(sio::ValueType::uint16);
  value_a.scale = 0.5;
  value_a.min = 125.0;
  value_a.max = 125.0;
  EXPECT_EQ(decoded(value_a, {0x00FA}), 125.0);
}

TEST(Decode, OutsideBoundsIsEmpty) {
  auto value_b = value_of(sio::ValueType::int16);
  value_b.scale = 0.25;
  value_b.min = -40.0;
  EXPECT_EQ(decoded(value_b, {0xFF38}), std::nullopt);
  value_b.min.reset();
  value_b.max = -60.0;
  EXPECT_EQ(decoded(value_b, {0xFF38}), std::nullopt);
}

TEST(Decode, NonFiniteFloat32IsEmpty) {
  EXPECT_EQ(decoded(value_of(sio::ValueType::float32), {0x7FC0, 0x0000}), std::nullopt);  // NaN
  EXPECT_EQ(decoded(value_of(sio::ValueType::float32), {0x7F80, 0x0000}), std::nullopt);  // +inf
  EXPECT_EQ(decoded(value_of(sio::ValueType::float32), {0xFF80, 0x0000}), std::nullopt);  // -inf
}

TEST(RegisterSpan, CoversLowestToHighestRegister) {
  auto first = value_of(sio::ValueType::uint16);
  first.register_address = 16;
  auto second = value_of(sio::ValueType::int16);
  second.register_address = 17;
  const auto span = sio::detail::register_span({first, second});
  EXPECT_EQ(span.first, 16U);
  EXPECT_EQ(sio::detail::register_count(span), 2U);
}

TEST(RegisterSpan, ThirtyTwoBitValueTakesTwoRegisters) {
  auto value = value_of(sio::ValueType::float32);
  value.register_address = 102;
  const auto span = sio::detail::register_span({value});
  EXPECT_EQ(span.first, 102U);
  EXPECT_EQ(span.last, 103U);
}

TEST(RegisterSpan, ZeroTo124Is125Registers) {
  auto low = value_of(sio::ValueType::uint16);
  low.register_address = 0;
  auto high = value_of(sio::ValueType::uint16);
  high.register_address = 124;
  EXPECT_EQ(sio::detail::register_count(sio::detail::register_span({high, low})), 125U);
}
