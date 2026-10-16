// SPDX-License-Identifier: Apache-2.0
//
// The quality flag every reading carries, and the exact fault behind it.
#pragma once

#include <cstdint>
#include <string_view>

namespace phm::sensor_io {

// The coarse flag a consumer filters on. The library never produces `stale`:
// whether a reading is too old is decided by the consuming application.
enum class Quality : std::uint8_t { ok, stale, timeout, crc_error, out_of_range };

// The exact cause of a transaction outcome. Each fault gives one quality:
//
//   Outcome                                                   fault               quality
//   Valid reply, every value within range                     none                ok
//   Valid reply, a value outside [min, max] or a float32
//     that is NaN or infinite                                 none                out_of_range
//   No complete reply within the response timeout             no_reply            timeout
//   The port could not be opened, read or written             io_error            timeout
//   Reply with a CRC-16/MODBUS mismatch                       crc_mismatch        crc_error
//   Reply from another slave address, with another
//     function code or with a wrong byte count                malformed_reply     crc_error
//   Modbus exception response                                 exception_response  crc_error
enum class Fault : std::uint8_t {
  none,
  no_reply,
  io_error,
  crc_mismatch,
  malformed_reply,
  exception_response,
};

// "ok", "stale", "timeout", "crc_error", "out_of_range".
[[nodiscard]] std::string_view to_string(Quality quality) noexcept;
// "none", "no_reply", "io_error", "crc_mismatch", "malformed_reply", "exception_response".
[[nodiscard]] std::string_view to_string(Fault fault) noexcept;

}  // namespace phm::sensor_io
