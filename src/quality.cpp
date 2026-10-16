// SPDX-License-Identifier: Apache-2.0
#include "phm/sensor_io/quality.hpp"

#include <string_view>

namespace phm::sensor_io {

std::string_view to_string(Quality quality) noexcept {
  switch (quality) {
    case Quality::ok:
      return "ok";
    case Quality::stale:
      return "stale";
    case Quality::timeout:
      return "timeout";
    case Quality::crc_error:
      return "crc_error";
    case Quality::out_of_range:
      return "out_of_range";
  }
  return "unknown";
}

std::string_view to_string(Fault fault) noexcept {
  switch (fault) {
    case Fault::none:
      return "none";
    case Fault::no_reply:
      return "no_reply";
    case Fault::io_error:
      return "io_error";
    case Fault::crc_mismatch:
      return "crc_mismatch";
    case Fault::malformed_reply:
      return "malformed_reply";
    case Fault::exception_response:
      return "exception_response";
  }
  return "unknown";
}

}  // namespace phm::sensor_io
