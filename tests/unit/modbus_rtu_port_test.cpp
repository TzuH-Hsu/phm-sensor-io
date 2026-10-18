// SPDX-License-Identifier: Apache-2.0
//
// The RTU inter-frame silence the port keeps before every request frame.
#include "modbus_rtu_port.hpp"

#include <gtest/gtest.h>

#include <chrono>

using phm::sensor_io::detail::inter_frame_silence;
using std::chrono::microseconds;

TEST(InterFrameSilence, IsThreeAndAHalfCharactersUpTo19200Baud) {
  // 38.5 bit times, rounded up to whole microseconds.
  EXPECT_EQ(inter_frame_silence(1200), microseconds{32084});
  EXPECT_EQ(inter_frame_silence(9600), microseconds{4011});
  EXPECT_EQ(inter_frame_silence(19200), microseconds{2006});
}

TEST(InterFrameSilence, IsFixedAbove19200Baud) {
  EXPECT_EQ(inter_frame_silence(38400), microseconds{1750});
  EXPECT_EQ(inter_frame_silence(115200), microseconds{1750});
}
