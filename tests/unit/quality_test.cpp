// SPDX-License-Identifier: Apache-2.0
#include "phm/sensor_io/quality.hpp"

#include <gtest/gtest.h>

namespace sio = phm::sensor_io;

TEST(Quality, ToString) {
  EXPECT_EQ(sio::to_string(sio::Quality::ok), "ok");
  EXPECT_EQ(sio::to_string(sio::Quality::stale), "stale");
  EXPECT_EQ(sio::to_string(sio::Quality::timeout), "timeout");
  EXPECT_EQ(sio::to_string(sio::Quality::crc_error), "crc_error");
  EXPECT_EQ(sio::to_string(sio::Quality::out_of_range), "out_of_range");
}

TEST(Fault, ToString) {
  EXPECT_EQ(sio::to_string(sio::Fault::none), "none");
  EXPECT_EQ(sio::to_string(sio::Fault::no_reply), "no_reply");
  EXPECT_EQ(sio::to_string(sio::Fault::io_error), "io_error");
  EXPECT_EQ(sio::to_string(sio::Fault::crc_mismatch), "crc_mismatch");
  EXPECT_EQ(sio::to_string(sio::Fault::malformed_reply), "malformed_reply");
  EXPECT_EQ(sio::to_string(sio::Fault::exception_response), "exception_response");
}
