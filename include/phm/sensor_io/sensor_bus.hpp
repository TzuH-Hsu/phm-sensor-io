// SPDX-License-Identifier: Apache-2.0
//
// SensorBus: the ports and points of one configuration, read one point per call.
#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "phm/sensor_io/config.hpp"
#include "phm/sensor_io/reading.hpp"
#include "phm/sensor_io/result.hpp"

namespace phm::sensor_io {

// Caller errors of SensorBus::read. Bus failures are never a ReadError.
enum class ReadError : std::uint8_t { unknown_point };

// The ports and points of one validated configuration.
//
// Lifecycle: build or load a BusConfig, create(), then any number of read()
// calls, then destruction. The library has no thread and no timer of its own:
// the caller decides when to read and assigns sample times.
//
// Threads: read() may be called from several threads at the same time. The
// library holds one lock per port, so reads of points on the same port run one
// after another and a serial line never has more than one request outstanding;
// reads of points on different ports run in parallel. config() is safe from any
// thread. create, move, move-assignment and the destructor must not overlap
// with any other call on the same object; the destructor does not wait for a
// read() in progress.
//
// Errors: the library throws nothing but std::bad_alloc. Using a moved-from
// SensorBus is undefined behaviour.
class SensorBus {
 public:
  // Validates `config` (validate()) and takes ownership of it. Opens no device:
  // a port is opened on the first read() of one of its points and stays open.
  [[nodiscard]] static Result<SensorBus, ConfigError> create(BusConfig config);

  SensorBus(SensorBus&& other) noexcept;
  SensorBus& operator=(SensorBus&& other) noexcept;
  SensorBus(const SensorBus&) = delete;
  SensorBus& operator=(const SensorBus&) = delete;
  ~SensorBus();  // closes every open port

  // Reads every value of `point` in one transaction, with the port's retries.
  // Before every request frame, the first and each retry, the line must have
  // been silent for the RTU inter-frame silence (3.5 characters of 11 bits,
  // 1.75 ms above 19200 baud) since the last frame sent or received on the
  // port, and the port's receive buffer is discarded; discarded bytes are not
  // reported and do not count as attempts. A reply that arrives before the
  // next request is never taken as that request's reply.
  // no_reply, crc_mismatch and malformed_reply are retried; exception_response
  // and io_error are not. At most 1 + retries request frames are sent, and the
  // result reports the last attempt. After io_error the port is closed and
  // opened again on its next read(). A failed transaction is a PointReading
  // with its quality and fault, never a ReadError; an unknown point name is
  // ReadError::unknown_point and sends nothing.
  [[nodiscard]] Result<PointReading, ReadError> read(std::string_view point);

  // The configuration as validated, with every default filled in.
  [[nodiscard]] const BusConfig& config() const noexcept;

 private:
  struct Impl;
  explicit SensorBus(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
};

}  // namespace phm::sensor_io
