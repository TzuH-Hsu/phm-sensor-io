// SPDX-License-Identifier: Apache-2.0
#include "phm/sensor_io/sensor_bus.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "decode.hpp"
#include "modbus_rtu_port.hpp"

namespace phm::sensor_io {

namespace {

Quality quality_of(Fault fault) noexcept {
  switch (fault) {
    case Fault::none:
      return Quality::ok;
    case Fault::no_reply:
    case Fault::io_error:
      return Quality::timeout;
    case Fault::crc_mismatch:
    case Fault::malformed_reply:
    case Fault::exception_response:
      break;
  }
  return Quality::crc_error;
}

}  // namespace

struct SensorBus::Impl {
  // One serial line and the lock that keeps one request outstanding on it.
  struct Port {
    std::mutex mutex;
    std::unique_ptr<detail::ModbusRtuPort> line;
  };

  // Where a point is read from, worked out once in create(). The pointers stay
  // valid: `config` and `ports` do not change after create().
  struct PointEntry {
    const PointConfig* point = nullptr;
    Port* port = nullptr;
    std::uint16_t first = 0;
    std::uint16_t count = 0;
  };

  BusConfig config;
  std::vector<std::unique_ptr<Port>> ports;
  std::map<std::string, PointEntry, std::less<>> points;
};

SensorBus::SensorBus(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
SensorBus::SensorBus(SensorBus&& other) noexcept = default;
SensorBus& SensorBus::operator=(SensorBus&& other) noexcept = default;
SensorBus::~SensorBus() = default;

Result<SensorBus, ConfigError> SensorBus::create(BusConfig config) {
  if (std::optional<ConfigError> error = validate(config)) {
    return Unexpected<ConfigError>{std::move(*error)};
  }
  auto impl = std::make_unique<Impl>();
  impl->config = std::move(config);

  std::map<std::string_view, Impl::Port*> ports_by_name;
  for (const PortConfig& port : impl->config.ports) {
    auto& added = impl->ports.emplace_back(std::make_unique<Impl::Port>());
    added->line = std::make_unique<detail::ModbusRtuPort>(port);
    ports_by_name.emplace(port.name, added.get());
  }
  for (const PointConfig& point : impl->config.points) {
    const detail::RegisterSpan span = detail::register_span(point.values);
    impl->points.emplace(point.name,
                         Impl::PointEntry{
                             .point = &point,
                             .port = ports_by_name.at(point.port),
                             .first = static_cast<std::uint16_t>(span.first),
                             .count = static_cast<std::uint16_t>(detail::register_count(span)),
                         });
  }
  return SensorBus(std::move(impl));
}

Result<PointReading, ReadError> SensorBus::read(std::string_view point) {
  assert(impl_ != nullptr && "read() on a moved-from SensorBus");
  const auto found = impl_->points.find(point);
  if (found == impl_->points.end()) {
    return Unexpected<ReadError>{ReadError::unknown_point};
  }
  const Impl::PointEntry& entry = found->second;
  const PointConfig& config = *entry.point;
  Impl::Port& port = *entry.port;

  detail::Exchange exchange;
  {
    const std::scoped_lock lock(port.mutex);
    exchange =
        port.line->read_registers(config.slave_address, config.function, entry.first, entry.count);
  }

  PointReading reading;
  reading.point = config.name;
  reading.quality = quality_of(exchange.fault);
  reading.fault = exchange.fault;
  reading.exception_code = exchange.exception_code;
  reading.attempts = exchange.attempts;
  reading.values.reserve(config.values.size());
  const std::span<const std::uint16_t> words(exchange.words);
  for (const ValueConfig& value : config.values) {
    ValueReading value_reading{.name = value.name, .raw = {}, .value = {}, .unit = value.unit};
    if (exchange.fault == Fault::none) {
      const auto raw =
          words.subspan(value.register_address - entry.first, detail::register_count(value.type));
      value_reading.raw.assign(raw.begin(), raw.end());
      value_reading.value = detail::decode(value, raw);
      if (!value_reading.value) {
        reading.quality = Quality::out_of_range;
      }
    }
    reading.values.push_back(std::move(value_reading));
  }
  return reading;
}

const BusConfig& SensorBus::config() const noexcept {
  assert(impl_ != nullptr && "config() on a moved-from SensorBus");
  return impl_->config;
}

}  // namespace phm::sensor_io
