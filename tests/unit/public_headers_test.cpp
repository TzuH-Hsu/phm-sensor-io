// SPDX-License-Identifier: Apache-2.0
//
// Includes the umbrella header only, with include/ as the only include path of
// this library: every public header compiles without the libmodbus (or
// nlohmann/json) include path. The test links nothing of the library, so it
// uses only what the headers define.
#include <gtest/gtest.h>

#include <chrono>

#include "phm/sensor_io/sensor_io.hpp"

namespace sio = phm::sensor_io;

TEST(PublicHeaders, SerialDefaultsAreTheModbusSerialLineDefaults) {
  const sio::SerialSettings serial;
  EXPECT_EQ(serial.baud, 19200U);
  EXPECT_EQ(serial.parity, sio::Parity::even);
  EXPECT_EQ(serial.stop_bits, 1);
}

TEST(PublicHeaders, TransactionDefaults) {
  const sio::TransactionSettings transaction;
  EXPECT_EQ(transaction.response_timeout, std::chrono::milliseconds{500});
  EXPECT_EQ(transaction.retries, 1);
}

TEST(PublicHeaders, ValueDefaults) {
  const sio::ValueConfig value;
  EXPECT_EQ(value.word_order, sio::WordOrder::high_first);
  EXPECT_EQ(value.scale, 1.0);
  EXPECT_EQ(value.offset, 0.0);
  EXPECT_TRUE(value.unit.empty());
  EXPECT_FALSE(value.min.has_value());
  EXPECT_FALSE(value.max.has_value());
}

TEST(PublicHeaders, ReadFunctionsAreTheModbusFunctionCodes) {
  EXPECT_EQ(static_cast<int>(sio::ReadFunction::read_holding_registers), 0x03);
  EXPECT_EQ(static_cast<int>(sio::ReadFunction::read_input_registers), 0x04);
}
