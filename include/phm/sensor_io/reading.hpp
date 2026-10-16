// SPDX-License-Identifier: Apache-2.0
//
// The result of one read of one point.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "phm/sensor_io/quality.hpp"

namespace phm::sensor_io {

// One configured value of one read.
//
//   quality              value                               raw
//   ok, stale            set                                 set
//   out_of_range         empty for each value outside its    set
//                        range, set for the others
//   timeout, crc_error   empty                               empty
struct ValueReading {
  std::string name;  // ValueConfig::name
  // Register words exactly as received, in register order, before sign, word
  // order and scale: 1 word for int16/uint16, 2 for int32/uint32/float32.
  // Empty when quality is timeout or crc_error.
  std::vector<std::uint16_t> raw;
  // raw read as ValueConfig::type and word_order, times scale, plus offset.
  // Empty when quality is timeout or crc_error, and for this value only when
  // it is outside [min, max] or a float32 NaN or infinity.
  std::optional<double> value;
  std::string unit;  // ValueConfig::unit
};

// The result of one read of one point: one transaction, one quality. A failed
// read is still a result; it is never filled in from an earlier read.
struct PointReading {
  std::string point;  // PointConfig::name
  Quality quality = Quality::timeout;
  Fault fault = Fault::none;
  std::optional<std::uint8_t> exception_code;  // set only when fault == Fault::exception_response
  // Request frames sent. Retries count; a frame the port failed to write does
  // not, so it is 0 when the port could not be opened.
  std::uint8_t attempts = 0;
  std::vector<ValueReading> values;  // one per ValueConfig, in configuration order, always
};

}  // namespace phm::sensor_io
