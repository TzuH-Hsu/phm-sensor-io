// SPDX-License-Identifier: Apache-2.0
#include "fixtures.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

namespace phm::sensor_io::testing {

std::filesystem::path fixture_path(std::string_view relative) {
  return std::filesystem::path(PHM_SENSOR_IO_FIXTURE_DIR) / relative;
}

std::string read_fixture(std::string_view relative) {
  const std::filesystem::path path = fixture_path(relative);
  const std::ifstream in(path, std::ios::binary);
  if (!in) {
    throw std::runtime_error("cannot open fixture " + path.string());
  }
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

std::vector<std::uint8_t> frame(std::string_view name) {
  const auto frames = nlohmann::json::parse(read_fixture("modbus_rtu/frames.json"));
  const auto hex = frames.at(std::string(name)).get<std::string>();
  std::vector<std::uint8_t> bytes;
  std::istringstream in(hex);
  std::string byte;
  while (in >> byte) {
    bytes.push_back(static_cast<std::uint8_t>(std::stoul(byte, nullptr, 16)));
  }
  return bytes;
}

}  // namespace phm::sensor_io::testing
