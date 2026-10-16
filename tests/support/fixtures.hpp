// SPDX-License-Identifier: Apache-2.0
//
// Access to the files under tests/fixtures/. Test code, not installed.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace phm::sensor_io::testing {

// The path of tests/fixtures/<relative>.
[[nodiscard]] std::filesystem::path fixture_path(std::string_view relative);

// The contents of tests/fixtures/<relative>.
[[nodiscard]] std::string read_fixture(std::string_view relative);

// The frame called `name` in tests/fixtures/modbus_rtu/frames.json, as bytes.
[[nodiscard]] std::vector<std::uint8_t> frame(std::string_view name);

}  // namespace phm::sensor_io::testing
