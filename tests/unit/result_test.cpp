// SPDX-License-Identifier: Apache-2.0
#include "phm/sensor_io/result.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

namespace sio = phm::sensor_io;

namespace {

sio::Result<int, std::string> half(int number) {
  if (number % 2 != 0) {
    return sio::Unexpected<std::string>{"odd"};
  }
  return number / 2;
}

}  // namespace

TEST(Result, HoldsAValue) {
  const auto result = half(4);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(static_cast<bool>(result));
  EXPECT_EQ(result.value(), 2);
  EXPECT_EQ(*result, 2);
}

TEST(Result, HoldsAnError) {
  const auto result = half(3);
  ASSERT_FALSE(result.has_value());
  EXPECT_FALSE(static_cast<bool>(result));
  EXPECT_EQ(result.error(), "odd");
}

TEST(Result, ArrowReachesTheValue) {
  sio::Result<std::string, int> result = std::string("abc");
  EXPECT_EQ(result->size(), 3U);
  result->push_back('d');
  EXPECT_EQ(*result, "abcd");
}

TEST(Result, MovesAMoveOnlyValueOut) {
  sio::Result<std::unique_ptr<int>, int> result = std::make_unique<int>(7);
  const std::unique_ptr<int> value = std::move(result).value();
  ASSERT_NE(value, nullptr);
  EXPECT_EQ(*value, 7);
}
