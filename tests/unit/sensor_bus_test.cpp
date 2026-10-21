// SPDX-License-Identifier: Apache-2.0
//
// SensorBus against the simulated slave: the configuration of "A read in hex"
// in issue #1 (slave 10 on port line-a, registers 16 and 17), every row of the
// fault-to-quality mapping, the fill table, retries and the request frames
// sent.
#include "phm/sensor_io/sensor_bus.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "fixtures.hpp"
#include "simulated_slave.hpp"

namespace sio = phm::sensor_io;
using phm::sensor_io::testing::frame;
using phm::sensor_io::testing::SimulatedSlave;

namespace {

constexpr std::uint8_t kSlave = 10;

// "A read in hex": point-1 reads value_a (uint16 at 16, scale 0.5) and value_b
// (int16 at 17, scale 0.25) with read input registers; 50 ms timeout, 1 retry.
sio::BusConfig hex_config(const std::filesystem::path& device) {
  sio::BusConfig config;
  sio::PortConfig port;
  port.name = "line-a";
  port.device = device;
  port.transaction.response_timeout = std::chrono::milliseconds{50};
  port.transaction.retries = 1;
  config.ports.push_back(port);

  sio::PointConfig point;
  point.name = "point-1";
  point.port = "line-a";
  point.slave_address = kSlave;
  point.function = sio::ReadFunction::read_input_registers;
  sio::ValueConfig value_a;
  value_a.name = "value_a";
  value_a.register_address = 16;
  value_a.type = sio::ValueType::uint16;
  value_a.scale = 0.5;
  value_a.unit = "unit_a";
  sio::ValueConfig value_b;
  value_b.name = "value_b";
  value_b.register_address = 17;
  value_b.type = sio::ValueType::int16;
  value_b.scale = 0.25;
  value_b.unit = "unit_b";
  point.values = {value_a, value_b};
  config.points.push_back(point);
  return config;
}

sio::SensorBus make_bus(sio::BusConfig config) {
  auto bus = sio::SensorBus::create(std::move(config));
  if (!bus) {
    throw std::runtime_error(bus.error().pointer + ": " + bus.error().message);
  }
  return std::move(bus).value();
}

sio::PointReading read_point(sio::SensorBus& bus, std::string_view point = "point-1") {
  auto reading = bus.read(point);
  if (!reading) {
    throw std::runtime_error("read() returned a ReadError");
  }
  return std::move(reading).value();
}

// The fill table's last row: timeout and crc_error leave value and raw empty.
void expect_all_empty(const sio::PointReading& reading) {
  ASSERT_EQ(reading.values.size(), 2U);
  for (const sio::ValueReading& value : reading.values) {
    EXPECT_EQ(value.value, std::nullopt) << value.name;
    EXPECT_TRUE(value.raw.empty()) << value.name;
  }
  EXPECT_EQ(reading.values.at(0).name, "value_a");
  EXPECT_EQ(reading.values.at(1).unit, "unit_b");
}

// How many of `count` reads of point-1 are ok.
int ok_reads(sio::SensorBus& bus, int count) {
  int ok = 0;
  for (int i = 0; i < count; ++i) {
    const auto reading = bus.read("point-1");
    if (reading && reading->quality == sio::Quality::ok) {
      ++ok;
    }
  }
  return ok;
}

class SensorBusTest : public ::testing::Test {
 protected:
  SensorBusTest() {
    slave_.set_input_registers(16, {0x00FA, 0xFF38});
    slave_.set_holding_registers(16, {0x00FA, 0xFF38});
  }

  // NOLINTBEGIN(*-non-private-member-variables-in-classes)
  SimulatedSlave slave_{kSlave};
  sio::BusConfig config_ = hex_config(slave_.device());
  // NOLINTEND(*-non-private-member-variables-in-classes)
};

}  // namespace

TEST_F(SensorBusTest, ValidReplyIsOk) {
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.point, "point-1");
  EXPECT_EQ(reading.quality, sio::Quality::ok);
  EXPECT_EQ(reading.fault, sio::Fault::none);
  EXPECT_EQ(reading.exception_code, std::nullopt);
  EXPECT_EQ(reading.attempts, 1);
  ASSERT_EQ(reading.values.size(), 2U);
  EXPECT_EQ(reading.values.at(0).name, "value_a");
  EXPECT_EQ(reading.values.at(0).raw, std::vector<std::uint16_t>{0x00FA});
  EXPECT_EQ(reading.values.at(0).value, 125.0);
  EXPECT_EQ(reading.values.at(0).unit, "unit_a");
  EXPECT_EQ(reading.values.at(1).name, "value_b");
  EXPECT_EQ(reading.values.at(1).raw, std::vector<std::uint16_t>{0xFF38});
  EXPECT_EQ(reading.values.at(1).value, -50.0);
  EXPECT_EQ(reading.values.at(1).unit, "unit_b");
  ASSERT_EQ(slave_.requests_received(), 1U);
  EXPECT_EQ(slave_.request_frames().at(0), frame("request_input_16_2"));
}

TEST_F(SensorBusTest, ReadsHoldingRegisters) {
  config_.points.at(0).function = sio::ReadFunction::read_holding_registers;
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::ok);
  EXPECT_EQ(reading.values.at(1).value, -50.0);
  EXPECT_EQ(slave_.request_frames().at(0), frame("request_holding_16_2"));
}

TEST_F(SensorBusTest, AppliesOffset) {
  config_.points.at(0).values.at(1).offset = 1.5;
  sio::SensorBus bus = make_bus(config_);
  EXPECT_EQ(read_point(bus).values.at(1).value, -48.5);
}

TEST_F(SensorBusTest, SilentSlaveIsATimeout) {
  slave_.stay_silent(2);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::timeout);
  EXPECT_EQ(reading.fault, sio::Fault::no_reply);
  EXPECT_EQ(reading.attempts, 2);
  EXPECT_EQ(slave_.requests_received(), 2U);
  expect_all_empty(reading);
}

TEST_F(SensorBusTest, CorruptedCrcIsACrcError) {
  slave_.corrupt_crc(2);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::crc_error);
  EXPECT_EQ(reading.fault, sio::Fault::crc_mismatch);
  EXPECT_EQ(reading.attempts, 2);
  expect_all_empty(reading);
}

TEST_F(SensorBusTest, ReplyFromAnotherSlaveIsMalformed) {
  slave_.reply_from_address(11, 2);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::crc_error);
  EXPECT_EQ(reading.fault, sio::Fault::malformed_reply);
  EXPECT_EQ(reading.attempts, 2);
  expect_all_empty(reading);
}

TEST_F(SensorBusTest, ReplyWithAnotherFunctionIsMalformed) {
  slave_.reply_with_function(0x03, 2);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::crc_error);
  EXPECT_EQ(reading.fault, sio::Fault::malformed_reply);
  expect_all_empty(reading);
}

TEST_F(SensorBusTest, ReplyWithAWrongByteCountIsMalformed) {
  slave_.reply_with_byte_count(3, 2);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::crc_error);
  EXPECT_EQ(reading.fault, sio::Fault::malformed_reply);
  expect_all_empty(reading);
}

TEST_F(SensorBusTest, ExceptionResponseCarriesItsCodeAndIsNotRetried) {
  slave_.reply_exception(0x02, 2);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::crc_error);
  EXPECT_EQ(reading.fault, sio::Fault::exception_response);
  EXPECT_EQ(reading.exception_code, 0x02);
  EXPECT_EQ(reading.attempts, 1);
  EXPECT_EQ(slave_.requests_received(), 1U);
  expect_all_empty(reading);
}

TEST_F(SensorBusTest, ExceptionCodeUnknownToLibmodbusIsKept) {
  slave_.reply_exception(0x42, 1);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.fault, sio::Fault::exception_response);
  EXPECT_EQ(reading.exception_code, 0x42);
}

TEST_F(SensorBusTest, DeviceThatCannotBeOpenedIsAnIoError) {
  config_.ports.at(0).device =
      std::filesystem::temp_directory_path() / "phm-sensor-io-test" / "no-such-device";
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::timeout);
  EXPECT_EQ(reading.fault, sio::Fault::io_error);
  EXPECT_EQ(reading.attempts, 0);
  expect_all_empty(reading);
  EXPECT_EQ(slave_.requests_received(), 0U);
}

TEST_F(SensorBusTest, ValueOutsideItsRangeIsEmptyAlone) {
  config_.points.at(0).values.at(1).min = -40.0;
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::out_of_range);
  EXPECT_EQ(reading.fault, sio::Fault::none);
  EXPECT_EQ(reading.values.at(0).value, 125.0);
  EXPECT_EQ(reading.values.at(0).raw, std::vector<std::uint16_t>{0x00FA});
  EXPECT_EQ(reading.values.at(1).value, std::nullopt);
  EXPECT_EQ(reading.values.at(1).raw, std::vector<std::uint16_t>{0xFF38});
}

TEST_F(SensorBusTest, BoundsAreInclusive) {
  config_.points.at(0).values.at(0).max = 125.0;
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::ok);
  EXPECT_EQ(reading.values.at(0).value, 125.0);
}

TEST_F(SensorBusTest, NanFloat32IsOutOfRange) {
  config_.points.at(0).values = {config_.points.at(0).values.at(0)};
  config_.points.at(0).values.at(0).type = sio::ValueType::float32;
  config_.points.at(0).values.at(0).scale = 1.0;
  slave_.set_input_registers(16, {0x7FC0, 0x0000});
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::out_of_range);
  EXPECT_EQ(reading.fault, sio::Fault::none);
  EXPECT_EQ(reading.values.at(0).value, std::nullopt);
  EXPECT_EQ(reading.values.at(0).raw, (std::vector<std::uint16_t>{0x7FC0, 0x0000}));
}

TEST_F(SensorBusTest, FailedReadAfterASuccessfulOneIsEmpty) {
  sio::SensorBus bus = make_bus(config_);
  EXPECT_EQ(read_point(bus).quality, sio::Quality::ok);
  slave_.stay_silent(2);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::timeout);
  expect_all_empty(reading);
}

TEST_F(SensorBusTest, RetryAfterASilentRequestIsOk) {
  slave_.stay_silent(1);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::ok);
  EXPECT_EQ(reading.attempts, 2);
  EXPECT_EQ(slave_.requests_received(), 2U);
  EXPECT_EQ(reading.values.at(0).value, 125.0);
}

TEST_F(SensorBusTest, NoRetriesSendsOneFrame) {
  config_.ports.at(0).transaction.retries = 0;
  slave_.stay_silent(1);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::timeout);
  EXPECT_EQ(reading.attempts, 1);
  EXPECT_EQ(slave_.requests_received(), 1U);
}

TEST_F(SensorBusTest, MostRetriesSendsSixFrames) {
  config_.ports.at(0).transaction.retries = 5;
  slave_.stay_silent(10);
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.fault, sio::Fault::no_reply);
  EXPECT_EQ(reading.attempts, 6);
  EXPECT_EQ(slave_.requests_received(), 6U);
}

TEST_F(SensorBusTest, UnknownPointSendsNothing) {
  sio::SensorBus bus = make_bus(config_);
  const auto reading = bus.read("no-such-point");
  ASSERT_FALSE(reading.has_value());
  EXPECT_EQ(reading.error(), sio::ReadError::unknown_point);
  EXPECT_EQ(slave_.requests_received(), 0U);
}

TEST_F(SensorBusTest, RegisterAt65535) {
  config_.points.at(0).values = {config_.points.at(0).values.at(0)};
  config_.points.at(0).values.at(0).register_address = 65535;
  slave_.set_input_registers(65535, {0x0001});
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::ok);
  EXPECT_EQ(slave_.request_frames().at(0), frame("request_input_65535_1"));
}

TEST_F(SensorBusTest, SpanOf125RegistersIsOneRequest) {
  config_.points.at(0).values.at(0).register_address = 0;
  config_.points.at(0).values.at(1).register_address = 124;
  slave_.set_input_registers(0, std::vector<std::uint16_t>(125, 0x0002));
  sio::SensorBus bus = make_bus(config_);
  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::ok);
  EXPECT_EQ(reading.values.at(0).raw, std::vector<std::uint16_t>{0x0002});
  EXPECT_EQ(reading.values.at(1).raw, std::vector<std::uint16_t>{0x0002});
  EXPECT_EQ(slave_.request_frames().at(0), frame("request_input_0_125"));
}

TEST_F(SensorBusTest, MovedBusKeepsReading) {
  sio::SensorBus first = make_bus(config_);
  sio::SensorBus second = std::move(first);
  EXPECT_EQ(read_point(second).quality, sio::Quality::ok);
}

TEST_F(SensorBusTest, ConcurrentReadsOnOnePortRunInTurn) {
  sio::SensorBus bus = make_bus(config_);
  constexpr std::size_t kThreads = 4;
  constexpr int kReadsPerThread = 5;
  std::vector<int> ok_counts(kThreads, 0);
  {
    std::vector<std::jthread> threads;
    threads.reserve(kThreads);
    for (int& ok_count : ok_counts) {
      threads.emplace_back([&bus, &ok_count] { ok_count = ok_reads(bus, kReadsPerThread); });
    }
  }
  EXPECT_EQ(ok_counts, std::vector<int>(kThreads, kReadsPerThread));
  // One request outstanding at a time: every frame arrived whole.
  EXPECT_EQ(slave_.request_frames(),
            std::vector<std::vector<std::uint8_t>>(kThreads * std::size_t{kReadsPerThread},
                                                   frame("request_input_16_2")));
}

// Line state before every request (#1, contract addition): slave 10 on line-a,
// no slave at address 11, response_timeout_ms 50, retries 0.

TEST_F(SensorBusTest, AddressThatDoesNotAnswerDoesNotAffectTheNextRead) {
  config_.ports.at(0).transaction.retries = 0;
  sio::PointConfig point_x = config_.points.at(0);
  point_x.name = "point-x";
  point_x.slave_address = 11;
  config_.points.push_back(point_x);
  sio::SensorBus bus = make_bus(config_);

  const sio::PointReading silent = read_point(bus, "point-x");
  EXPECT_EQ(silent.quality, sio::Quality::timeout);
  EXPECT_EQ(silent.fault, sio::Fault::no_reply);
  EXPECT_EQ(silent.attempts, 1);

  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::ok);
  EXPECT_EQ(reading.attempts, 1);
  EXPECT_EQ(reading.values.at(0).value, 125.0);
  EXPECT_EQ(reading.values.at(1).value, -50.0);
  ASSERT_EQ(slave_.requests_received(), 2U);
  EXPECT_EQ(slave_.request_frames().at(0).at(0), 11);  // recorded, not answered
}

TEST_F(SensorBusTest, LateReplyIsNotTakenAsTheNextReadsReply) {
  config_.ports.at(0).transaction.retries = 0;
  sio::SensorBus bus = make_bus(config_);
  slave_.reply_late(std::chrono::milliseconds{80}, 1);

  const sio::PointReading timed_out = read_point(bus);
  EXPECT_EQ(timed_out.quality, sio::Quality::timeout);
  EXPECT_EQ(timed_out.fault, sio::Fault::no_reply);

  // Wait until the late reply (0x00FA 0xFF38) has been sent, then change the
  // registers: the next read must see the new words, not the late reply.
  ASSERT_EQ(slave_.request_times().size(), 1U);
  std::this_thread::sleep_until(slave_.request_times().at(0).request_received +
                                std::chrono::milliseconds{100});
  ASSERT_TRUE(slave_.request_times().at(0).reply_sent.has_value());
  slave_.set_input_registers(16, {0x0064, 0x0028});

  const sio::PointReading reading = read_point(bus);
  EXPECT_EQ(reading.quality, sio::Quality::ok);
  EXPECT_EQ(reading.values.at(0).raw, std::vector<std::uint16_t>{0x0064});
  EXPECT_EQ(reading.values.at(1).raw, std::vector<std::uint16_t>{0x0028});
  EXPECT_EQ(reading.values.at(0).value, 50.0);
  EXPECT_EQ(reading.values.at(1).value, 10.0);
}

TEST_F(SensorBusTest, SilenceIsKeptAfterALateReply) {
  config_.ports.at(0).serial.baud = 9600;
  config_.ports.at(0).transaction.retries = 0;
  sio::SensorBus bus = make_bus(config_);
  slave_.reply_late(std::chrono::milliseconds{80}, 1);
  EXPECT_EQ(read_point(bus).fault, sio::Fault::no_reply);

  // Read again as soon as the late reply has been written.
  const auto give_up = std::chrono::steady_clock::now() + std::chrono::milliseconds{500};
  while (!slave_.request_times().at(0).reply_sent && std::chrono::steady_clock::now() < give_up) {
    std::this_thread::sleep_for(std::chrono::microseconds{100});
  }
  ASSERT_TRUE(slave_.request_times().at(0).reply_sent.has_value());
  EXPECT_EQ(read_point(bus).quality, sio::Quality::ok);

  const auto times = slave_.request_times();
  ASSERT_EQ(times.size(), 2U);
  const auto late_reply_sent =
      times.at(0).reply_sent.value_or(std::chrono::steady_clock::time_point::max());
  EXPECT_GE(times.at(1).request_received - late_reply_sent, std::chrono::microseconds{4000});
}

TEST_F(SensorBusTest, InterFrameSilenceIsKept) {
  config_.ports.at(0).serial.baud = 9600;
  sio::SensorBus bus = make_bus(config_);
  EXPECT_EQ(read_point(bus).quality, sio::Quality::ok);
  EXPECT_EQ(read_point(bus).quality, sio::Quality::ok);

  const auto times = slave_.request_times();
  ASSERT_EQ(times.size(), 2U);
  ASSERT_TRUE(times.at(0).reply_sent.has_value());
  // 3.5 characters of 11 bits at 9600 baud: 3.5 * 11 / 9600 s, just over 4.0 ms.
  const auto gap = times.at(1).request_received -
                   times.at(0).reply_sent.value_or(std::chrono::steady_clock::time_point::max());
  EXPECT_GE(gap, std::chrono::microseconds{4000});
}
