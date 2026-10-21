// SPDX-License-Identifier: Apache-2.0
//
// The simulated slave itself, driven directly over its pseudo-terminal: its CRC
// routine against the reference vector, and its replies and faults against the
// frames of tests/fixtures/modbus_rtu/frames.json.
#include "simulated_slave.hpp"

#include <fcntl.h>
#include <gtest/gtest.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <vector>

#include "fixtures.hpp"

namespace testing_support = phm::sensor_io::testing;
using testing_support::frame;
using testing_support::SimulatedSlave;

namespace {

constexpr std::uint8_t kSlave = 10;

// The master's end of the line: the device of a SimulatedSlave, opened raw.
class Master {
 public:
  explicit Master(const SimulatedSlave& slave)
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
      : fd_(open(slave.device().c_str(), O_RDWR | O_NOCTTY)) {
    EXPECT_NE(fd_, -1);
    termios settings{};
    tcgetattr(fd_, &settings);
    cfmakeraw(&settings);
    tcsetattr(fd_, TCSANOW, &settings);
  }
  ~Master() { close(fd_); }
  Master(const Master&) = delete;
  Master& operator=(const Master&) = delete;
  Master(Master&&) = delete;
  Master& operator=(Master&&) = delete;

  void send(const std::vector<std::uint8_t>& bytes) const {
    EXPECT_EQ(write(fd_, bytes.data(), bytes.size()), static_cast<ssize_t>(bytes.size()));
  }

  // Everything that arrives until nothing more comes for `quiet`.
  [[nodiscard]] std::vector<std::uint8_t> receive(
      std::chrono::milliseconds quiet = std::chrono::milliseconds{200}) const {
    std::vector<std::uint8_t> bytes;
    std::array<std::uint8_t, 256> buffer{};
    for (;;) {
      pollfd poll_fd{.fd = fd_, .events = POLLIN, .revents = 0};
      if (poll(&poll_fd, 1, static_cast<int>(quiet.count())) <= 0) {
        return bytes;
      }
      const ssize_t length = read(fd_, buffer.data(), buffer.size());
      if (length <= 0) {
        return bytes;
      }
      const auto received = std::span(buffer).first(static_cast<std::size_t>(length));
      bytes.insert(bytes.end(), received.begin(), received.end());
    }
  }

 private:
  int fd_;
};

class SimulatedSlaveTest : public ::testing::Test {
 protected:
  SimulatedSlaveTest() {
    slave_.set_input_registers(16, {0x00FA, 0xFF38});
    slave_.set_holding_registers(16, {0x00FA, 0xFF38});
  }

  SimulatedSlave slave_{kSlave};  // NOLINT(*-non-private-member-variables-in-classes)
};

}  // namespace

namespace {

// The CRC at the end of `bytes`, sent low byte first.
std::uint16_t sent_crc(const std::vector<std::uint8_t>& bytes) {
  const unsigned low = bytes.at(bytes.size() - 2);
  const unsigned high = bytes.at(bytes.size() - 1);
  return static_cast<std::uint16_t>(low | (high << 8U));
}

// The CRC of everything in `bytes` before its last two bytes.
std::uint16_t computed_crc(const std::vector<std::uint8_t>& bytes) {
  return testing_support::crc16_modbus(std::span(bytes).first(bytes.size() - 2));
}

}  // namespace

TEST(Crc16Modbus, ReferenceVector) {
  // 01 03 00 00 00 0A, then C5 CD: the CRC 0xCDC5 sent low byte first.
  const std::vector<std::uint8_t> reference = frame("crc_reference");
  EXPECT_EQ(computed_crc(reference), 0xCDC5U);
  EXPECT_EQ(sent_crc(reference), 0xCDC5U);
}

TEST(Crc16Modbus, FixtureFramesCarryTheirCrc) {
  for (const std::string_view name : {
           "crc_reference",
           "request_input_16_2",
           "reply_input_16_2",
           "request_holding_16_2",
           "reply_holding_16_2",
           "reply_other_slave",
           "reply_bad_byte_count",
           "reply_exception_02",
           "request_input_65535_1",
           "request_input_0_125",
       }) {
    const std::vector<std::uint8_t> bytes = frame(name);
    EXPECT_EQ(sent_crc(bytes), computed_crc(bytes)) << name;
  }
  const std::vector<std::uint8_t> bad = frame("reply_bad_crc");
  EXPECT_NE(sent_crc(bad), computed_crc(bad));
}

TEST_F(SimulatedSlaveTest, AnswersReadInputRegisters) {
  const Master master(slave_);
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_input_16_2"));
  EXPECT_EQ(slave_.requests_received(), 1U);
  EXPECT_EQ(slave_.request_frames().at(0), frame("request_input_16_2"));
}

TEST_F(SimulatedSlaveTest, AnswersReadHoldingRegisters) {
  const Master master(slave_);
  master.send(frame("request_holding_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_holding_16_2"));
}

TEST_F(SimulatedSlaveTest, CorruptsTheCrc) {
  const Master master(slave_);
  slave_.corrupt_crc(1);
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_bad_crc"));
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_input_16_2"));
}

TEST_F(SimulatedSlaveTest, RepliesFromAnotherAddress) {
  const Master master(slave_);
  slave_.reply_from_address(11, 1);
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_other_slave"));
}

TEST_F(SimulatedSlaveTest, RepliesWithAnotherFunction) {
  const Master master(slave_);
  slave_.reply_with_function(0x03, 1);
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_holding_16_2"));
}

TEST_F(SimulatedSlaveTest, RepliesWithAWrongByteCount) {
  const Master master(slave_);
  slave_.reply_with_byte_count(3, 1);
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_bad_byte_count"));
}

TEST_F(SimulatedSlaveTest, RepliesWithAnException) {
  const Master master(slave_);
  slave_.reply_exception(0x02, 1);
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_exception_02"));
}

TEST_F(SimulatedSlaveTest, StaysSilent) {
  const Master master(slave_);
  slave_.stay_silent(1);
  master.send(frame("request_input_16_2"));
  EXPECT_TRUE(master.receive().empty());
  EXPECT_EQ(slave_.requests_received(), 1U);
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_input_16_2"));
}

TEST_F(SimulatedSlaveTest, IgnoresAnotherAddressAndABadCrc) {
  const Master master(slave_);
  master.send(frame("crc_reference"));  // slave 1
  EXPECT_TRUE(master.receive().empty());
  std::vector<std::uint8_t> bad_crc = frame("request_input_16_2");
  bad_crc.back() ^= 0x01U;
  master.send(bad_crc);
  EXPECT_TRUE(master.receive().empty());
  EXPECT_EQ(slave_.requests_received(), 2U);
}

TEST_F(SimulatedSlaveTest, UnsetRegisterIsAnIllegalDataAddress) {
  const Master master(slave_);
  master.send(frame("request_input_65535_1"));
  const std::vector<std::uint8_t> reply = master.receive();
  ASSERT_EQ(reply.size(), 5U);
  EXPECT_EQ(reply.at(1), 0x84);
  EXPECT_EQ(reply.at(2), 0x02);
}

TEST_F(SimulatedSlaveTest, HoldsRepliesUntilReleased) {
  const Master master(slave_);
  std::atomic<bool> released{false};
  slave_.hold_replies_until([&released] { return released.load(); });
  master.send(frame("request_input_16_2"));
  EXPECT_TRUE(master.receive(std::chrono::milliseconds{100}).empty());
  released = true;
  EXPECT_EQ(master.receive(), frame("reply_input_16_2"));
}

TEST_F(SimulatedSlaveTest, RepliesLate) {
  const Master master(slave_);
  slave_.reply_late(std::chrono::milliseconds{150}, 1);
  master.send(frame("request_input_16_2"));
  EXPECT_TRUE(master.receive(std::chrono::milliseconds{100}).empty());
  EXPECT_EQ(master.receive(), frame("reply_input_16_2"));
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(std::chrono::milliseconds{100}), frame("reply_input_16_2"));
}

TEST_F(SimulatedSlaveTest, RecordsRequestAndReplyTimes) {
  const Master master(slave_);
  const auto before = std::chrono::steady_clock::now();
  master.send(frame("crc_reference"));  // slave 1: recorded, not answered
  EXPECT_TRUE(master.receive(std::chrono::milliseconds{50}).empty());
  master.send(frame("request_input_16_2"));
  EXPECT_EQ(master.receive(), frame("reply_input_16_2"));

  const auto times = slave_.request_times();
  ASSERT_EQ(times.size(), 2U);
  EXPECT_GE(times.at(0).request_received, before);
  EXPECT_FALSE(times.at(0).reply_sent.has_value());
  ASSERT_TRUE(times.at(1).reply_sent.has_value());
  EXPECT_GE(times.at(1).reply_sent.value_or(std::chrono::steady_clock::time_point::min()),
            times.at(1).request_received);
}
