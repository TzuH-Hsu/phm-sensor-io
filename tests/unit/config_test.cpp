// SPDX-License-Identifier: Apache-2.0
//
// parse_config, load_config, validate and SensorBus::create against the
// configuration rules: the fixture documents of issue #1, the boundary values
// built from them, and the same errors for a BusConfig built in code.
#include "phm/sensor_io/config.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fixtures.hpp"
#include "phm/sensor_io/sensor_bus.hpp"

namespace sio = phm::sensor_io;
using phm::sensor_io::testing::fixture_path;
using phm::sensor_io::testing::read_fixture;

namespace {

// defaults.json with the first `from` replaced by `to`.
std::string defaults_with(std::string_view from, std::string_view to) {
  std::string text = read_fixture("config/defaults.json");
  const auto at = text.find(from);
  if (at == std::string::npos) {
    ADD_FAILURE() << "defaults.json has no " << from;
    return text;
  }
  text.replace(at, from.size(), to);
  return text;
}

// The error pointer of parsing `text`, or "accepted".
std::string error_pointer(std::string_view text) {
  const auto result = sio::parse_config(text);
  return result ? "accepted" : result.error().pointer;
}

sio::BusConfig defaults_config() {
  auto result = sio::parse_config(read_fixture("config/defaults.json"));
  EXPECT_TRUE(result.has_value());
  return result ? *result : sio::BusConfig{};
}

// A document and the error pointer expected from it ("accepted" when valid).
struct Case {
  std::string document;
  std::string pointer;
};

void expect_pointers(const std::vector<Case>& cases) {
  for (const Case& each : cases) {
    EXPECT_EQ(error_pointer(each.document), each.pointer) << each.document;
  }
}

// defaults.json with `extra` keys added to its port.
std::string port_with(std::string_view extra) {
  const std::string device = R"("device": "/dev/ttyTEST0")";
  return defaults_with(device, device + ", " + std::string(extra));
}

std::string validate_pointer(const sio::BusConfig& config) {
  const std::optional<sio::ConfigError> error = sio::validate(config);
  return error ? error->pointer : "accepted";
}

}  // namespace

TEST(Config, EveryKeyIsRead) {
  const auto result = sio::parse_config(read_fixture("config/every_key.json"));
  ASSERT_TRUE(result.has_value()) << result.error().pointer << ": " << result.error().message;
  const sio::BusConfig& config = *result;

  ASSERT_EQ(config.ports.size(), 1U);
  const sio::PortConfig& port = config.ports.at(0);
  EXPECT_EQ(port.name, "line-a");
  EXPECT_EQ(port.device, std::filesystem::path("/dev/ttyTEST0"));
  EXPECT_EQ(port.serial.baud, 9600U);
  EXPECT_EQ(port.serial.parity, sio::Parity::none);
  EXPECT_EQ(port.serial.stop_bits, 2);
  EXPECT_EQ(port.transaction.response_timeout, std::chrono::milliseconds{200});
  EXPECT_EQ(port.transaction.retries, 2);

  ASSERT_EQ(config.points.size(), 2U);
  const sio::PointConfig& point_1 = config.points.at(0);
  EXPECT_EQ(point_1.name, "point-1");
  EXPECT_EQ(point_1.port, "line-a");
  EXPECT_EQ(point_1.slave_address, 10);
  EXPECT_EQ(point_1.function, sio::ReadFunction::read_input_registers);
  ASSERT_EQ(point_1.values.size(), 2U);
  const sio::ValueConfig& value_a = point_1.values.at(0);
  EXPECT_EQ(value_a.name, "value_a");
  EXPECT_EQ(value_a.register_address, 16);
  EXPECT_EQ(value_a.type, sio::ValueType::uint16);
  EXPECT_EQ(value_a.scale, 0.5);
  EXPECT_EQ(value_a.offset, 0.0);
  EXPECT_EQ(value_a.unit, "unit_a");
  EXPECT_EQ(value_a.min, 0.0);
  EXPECT_EQ(value_a.max, 1000.0);
  const sio::ValueConfig& value_b = point_1.values.at(1);
  EXPECT_EQ(value_b.type, sio::ValueType::int16);
  EXPECT_EQ(value_b.scale, 0.25);
  EXPECT_EQ(value_b.offset, 1.5);
  EXPECT_EQ(value_b.min, -40.0);
  EXPECT_EQ(value_b.max, std::nullopt);

  const sio::PointConfig& point_2 = config.points.at(1);
  EXPECT_EQ(point_2.slave_address, 247);
  EXPECT_EQ(point_2.function, sio::ReadFunction::read_holding_registers);
  ASSERT_EQ(point_2.values.size(), 3U);
  EXPECT_EQ(point_2.values.at(0).type, sio::ValueType::uint32);
  EXPECT_EQ(point_2.values.at(0).word_order, sio::WordOrder::low_first);
  EXPECT_EQ(point_2.values.at(1).type, sio::ValueType::float32);
  EXPECT_EQ(point_2.values.at(1).word_order, sio::WordOrder::high_first);
  EXPECT_EQ(point_2.values.at(2).type, sio::ValueType::int32);
  EXPECT_EQ(point_2.values.at(2).register_address, 104);
}

TEST(Config, OmittedKeysTakeTheirDefaults) {
  const auto result = sio::parse_config(read_fixture("config/defaults.json"));
  ASSERT_TRUE(result.has_value()) << result.error().pointer << ": " << result.error().message;
  const sio::PortConfig& port = result->ports.at(0);
  EXPECT_EQ(port.serial.baud, 19200U);
  EXPECT_EQ(port.serial.parity, sio::Parity::even);
  EXPECT_EQ(port.serial.stop_bits, 1);
  EXPECT_EQ(port.transaction.response_timeout, std::chrono::milliseconds{500});
  EXPECT_EQ(port.transaction.retries, 1);
  const sio::ValueConfig& value = result->points.at(0).values.at(0);
  EXPECT_EQ(value.word_order, sio::WordOrder::high_first);
  EXPECT_EQ(value.scale, 1.0);
  EXPECT_EQ(value.offset, 0.0);
  EXPECT_EQ(value.unit, "");
  EXPECT_EQ(value.min, std::nullopt);
  EXPECT_EQ(value.max, std::nullopt);
}

TEST(Config, UnknownKeyIsRejected) {
  const auto result = sio::parse_config(read_fixture("config/unknown_key.json"));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().pointer, "/ports/0/speed");
  EXPECT_EQ(result.error().message, "unknown key");
}

TEST(Config, SpanOver125RegistersIsRejected) {
  const auto result = sio::parse_config(read_fixture("config/span_126.json"));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().pointer, "/points/0/values");
}

TEST(Config, LoadConfigReadsAFile) {
  const auto result = sio::load_config(fixture_path("config/every_key.json"));
  ASSERT_TRUE(result.has_value()) << result.error().message;
  EXPECT_EQ(result->points.size(), 2U);
}

TEST(Config, LoadConfigReportsAMissingFile) {
  const auto result = sio::load_config(fixture_path("config/no-such-file.json"));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().pointer, "");
  EXPECT_NE(result.error().message.find("no-such-file.json"), std::string::npos);
}

TEST(Config, InvalidJsonIsADocumentError) {
  const auto result = sio::parse_config(R"({"bus": "modbus_rtu",)");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().pointer, "");
  EXPECT_FALSE(result.error().message.empty());
}

TEST(Config, NumberBeyondDoubleIsADocumentError) {
  const auto result =
      sio::parse_config(R"({"bus": "modbus_rtu", "ports": [], "points": [], "x": 1e400})");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().pointer, "");
  EXPECT_FALSE(result.error().message.empty());
}

TEST(Config, SlaveAddressBoundaries) {
  EXPECT_EQ(error_pointer(defaults_with(R"("slave_address": 10)", R"("slave_address": 1)")),
            "accepted");
  EXPECT_EQ(error_pointer(defaults_with(R"("slave_address": 10)", R"("slave_address": 247)")),
            "accepted");
  EXPECT_EQ(error_pointer(defaults_with(R"("slave_address": 10)", R"("slave_address": 0)")),
            "/points/0/slave_address");
  EXPECT_EQ(error_pointer(defaults_with(R"("slave_address": 10)", R"("slave_address": 248)")),
            "/points/0/slave_address");
}

TEST(Config, ResponseTimeoutAndRetriesBoundaries) {
  expect_pointers({
      {.document = port_with(R"("response_timeout_ms": 10)"), .pointer = "accepted"},
      {.document = port_with(R"("response_timeout_ms": 10000)"), .pointer = "accepted"},
      {.document = port_with(R"("retries": 0)"), .pointer = "accepted"},
      {.document = port_with(R"("retries": 5)"), .pointer = "accepted"},
      {
          .document = port_with(R"("response_timeout_ms": 9)"),
          .pointer = "/ports/0/response_timeout_ms",
      },
      {
          .document = port_with(R"("response_timeout_ms": 10001)"),
          .pointer = "/ports/0/response_timeout_ms",
      },
      {.document = port_with(R"("retries": 6)"), .pointer = "/ports/0/retries"},
  });
}

TEST(Config, SerialSettingRules) {
  for (const int baud : {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200}) {
    EXPECT_EQ(error_pointer(port_with("\"baud\": " + std::to_string(baud))), "accepted") << baud;
  }
  expect_pointers({
      {.document = port_with(R"("baud": 14400)"), .pointer = "/ports/0/baud"},
      {.document = port_with(R"("baud": -9600)"), .pointer = "/ports/0/baud"},
      {.document = port_with(R"("parity": "mark")"), .pointer = "/ports/0/parity"},
      {.document = port_with(R"("stop_bits": 3)"), .pointer = "/ports/0/stop_bits"},
  });
}

TEST(Config, IntegerKeysRejectOtherJsonTypes) {
  const std::string device = R"("device": "/dev/ttyTEST0")";
  for (const std::string_view baud : {R"("baud": 19200.0)", R"("baud": "19200")"}) {
    const auto result = sio::parse_config(defaults_with(device, device + ", " + std::string(baud)));
    ASSERT_FALSE(result.has_value()) << baud;
    EXPECT_EQ(result.error().pointer, "/ports/0/baud");
    EXPECT_EQ(result.error().message, "must be an integer");
  }
}

TEST(Config, DocumentLevelRules) {
  EXPECT_EQ(error_pointer(defaults_with(R"("bus": "modbus_rtu")", R"("bus": "modbus_tcp")")),
            "/bus");
  EXPECT_EQ(
      error_pointer(defaults_with(R"("bus": "modbus_rtu",)", R"("bus": "modbus_rtu", "x": 1,)")),
      "/x");
  EXPECT_EQ(error_pointer(R"({"bus": "modbus_rtu", "ports": [], "points": []})"), "/ports");
  EXPECT_EQ(error_pointer(R"({"bus": "modbus_rtu", "points": []})"), "/ports");
  EXPECT_EQ(error_pointer("[]"), "");
}

TEST(Config, RequiredKeysAreReported) {
  const auto result = sio::parse_config(defaults_with(R"(, "type": "uint16")", ""));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().pointer, "/points/0/values/0/type");
  EXPECT_EQ(result.error().message, "is required");
}

TEST(Config, NamesAndReferences) {
  EXPECT_EQ(error_pointer(defaults_with(R"("name": "line-a")", R"("name": "line a")")),
            "/ports/0/name");
  EXPECT_EQ(error_pointer(defaults_with(R"("port": "line-a")", R"("port": "line-b")")),
            "/points/0/port");
  EXPECT_EQ(error_pointer(defaults_with(R"("name": "point-1")", R"("name": "")")),
            "/points/0/name");
  EXPECT_EQ(error_pointer(defaults_with(R"("device": "/dev/ttyTEST0")", R"("device": "ttyTEST0")")),
            "/ports/0/device");
  EXPECT_EQ(
      error_pointer(defaults_with(R"({ "name": "value_a", "register": 16, "type": "uint16" })",
                                  R"({ "name": "value_a", "register": 16, "type": "uint16" },
                                           { "name": "value_a", "register": 17, "type": "uint16" })")),
      "/points/0/values/1/name");
}

TEST(Config, DuplicatePortAndPointNames) {
  const std::string port = R"({ "name": "line-a", "device": "/dev/ttyTEST0" })";
  EXPECT_EQ(error_pointer(
                defaults_with(port, port + R"(, { "name": "line-a", "device": "/dev/ttyTEST1" })")),
            "/ports/1/name");
  sio::BusConfig config = defaults_config();
  config.points.push_back(config.points.at(0));
  EXPECT_EQ(validate_pointer(config), "/points/1/name");
}

TEST(Config, ValueRules) {
  const auto with = [](std::string_view replacement) {
    return defaults_with(R"("type": "uint16")", replacement);
  };
  const std::string at = "/points/0/values/0";
  expect_pointers({
      {.document = with(R"("type": "int8")"), .pointer = at + "/type"},
      {
          .document = with(R"("type": "int16", "word_order": "high_first")"),
          .pointer = at + "/word_order",
      },
      {
          .document = with(R"("type": "int32", "word_order": "middle")"),
          .pointer = at + "/word_order",
      },
      {.document = with(R"("type": "uint16", "unit": "0123456789abcdef")"), .pointer = "accepted"},
      {
          .document = with(R"("type": "uint16", "unit": "0123456789abcdefg")"),
          .pointer = at + "/unit",
      },
      {.document = with(R"("type": "uint16", "min": 2, "max": 1)"), .pointer = at + "/max"},
      {.document = with(R"("type": "uint16", "min": 1, "max": 1)"), .pointer = "accepted"},
      {.document = with(R"("type": "uint16", "scale": "2")"), .pointer = at + "/scale"},
  });
}

TEST(Config, RegisterRules) {
  const std::string value = R"("register": 16, "type": "uint16")";
  const std::string at = "/points/0/values/0/register";
  expect_pointers({
      {.document = defaults_with(value, R"("register": 65536, "type": "uint16")"), .pointer = at},
      {.document = defaults_with(value, R"("register": -1, "type": "uint16")"), .pointer = at},
      {
          .document = defaults_with(value, R"("register": 65535, "type": "uint16")"),
          .pointer = "accepted",
      },
      {
          .document = defaults_with(value, R"("register": 65534, "type": "float32")"),
          .pointer = "accepted",
      },
      {.document = defaults_with(value, R"("register": 65535, "type": "float32")"), .pointer = at},
  });
}

TEST(Config, EmptyValuesAreRejected) {
  EXPECT_EQ(error_pointer(defaults_with(
                R"([{ "name": "value_a", "register": 16, "type": "uint16" }])", "[]")),
            "/points/0/values");
}

TEST(Config, BuiltInCodeGivesTheSamePointers) {
  {
    sio::BusConfig config = defaults_config();
    config.points.at(0).slave_address = 0;
    EXPECT_EQ(validate_pointer(config), "/points/0/slave_address");
  }
  {
    sio::BusConfig config = defaults_config();
    config.points.at(0).values.at(0).word_order = sio::WordOrder::low_first;
    EXPECT_EQ(validate_pointer(config), "/points/0/values/0/word_order");
  }
  {
    sio::BusConfig config = defaults_config();
    config.points.at(0).values.at(0).scale = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(validate_pointer(config), "/points/0/values/0/scale");
  }
  {
    sio::BusConfig config = defaults_config();
    config.ports.at(0).transaction.response_timeout = std::chrono::milliseconds{9};
    EXPECT_EQ(validate_pointer(config), "/ports/0/response_timeout_ms");
  }
  {
    sio::BusConfig config = defaults_config();
    config.ports.at(0).transaction.retries = 6;
    EXPECT_EQ(validate_pointer(config), "/ports/0/retries");
  }
  {
    sio::BusConfig config = defaults_config();
    config.points.at(0).values.at(0).register_address = 141;
    sio::ValueConfig low = config.points.at(0).values.at(0);
    low.name = "value_z";
    low.register_address = 16;
    config.points.at(0).values.push_back(low);
    EXPECT_EQ(validate_pointer(config), "/points/0/values");
  }
  EXPECT_EQ(validate_pointer(defaults_config()), "accepted");
}

TEST(Config, FirstOfTwoErrorsIsTheSameForADocumentAndForCode) {
  // A relative device path and slave address 0: a document and the
  // equivalent BusConfig both report the device, which comes first.
  const std::string document =
      defaults_with(R"("device": "/dev/ttyTEST0")", R"("device": "dev/ttyTEST0")");
  const std::string both = [&document] {
    std::string text = document;
    const std::string from = R"("slave_address": 10)";
    text.replace(text.find(from), from.size(), R"("slave_address": 0)");
    return text;
  }();
  EXPECT_EQ(error_pointer(both), "/ports/0/device");

  sio::BusConfig config = defaults_config();
  config.ports.at(0).device = "dev/ttyTEST0";
  config.points.at(0).slave_address = 0;
  EXPECT_EQ(validate_pointer(config), "/ports/0/device");
}

TEST(Config, ValueThatDoesNotFitItsFieldIsReportedWithTheRule) {
  const auto result =
      sio::parse_config(defaults_with(R"("slave_address": 10)", R"("slave_address": 300)"));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().pointer, "/points/0/slave_address");
  EXPECT_EQ(result.error().message, "must be an integer from 1 to 247 (0 is broadcast)");
}

TEST(Config, WordOrderKeyOnA16BitValueIsReportedAfterTheOtherRules) {
  // high_first on an int16 cannot be told from the default in a BusConfig, so
  // the device error, which validate() also reports, comes first.
  const std::string text =
      defaults_with(R"("type": "uint16")", R"("type": "int16", "word_order": "high_first")");
  EXPECT_EQ(error_pointer(text), "/points/0/values/0/word_order");
  EXPECT_EQ(error_pointer([&text] {
              std::string both = text;
              const std::string from = R"("device": "/dev/ttyTEST0")";
              both.replace(both.find(from), from.size(), R"("device": "dev/ttyTEST0")");
              return both;
            }()),
            "/ports/0/device");
}

TEST(SensorBusCreate, RejectsAnInvalidConfiguration) {
  sio::BusConfig config = defaults_config();
  config.points.at(0).port = "line-b";
  const auto bus = sio::SensorBus::create(config);
  ASSERT_FALSE(bus.has_value());
  EXPECT_EQ(bus.error().pointer, "/points/0/port");
}

TEST(SensorBusCreate, KeepsTheConfigurationWithItsDefaults) {
  const auto bus = sio::SensorBus::create(defaults_config());
  ASSERT_TRUE(bus.has_value());
  EXPECT_EQ(bus->config().ports.at(0).serial.baud, 19200U);
  EXPECT_EQ(bus->config().points.at(0).name, "point-1");
}
