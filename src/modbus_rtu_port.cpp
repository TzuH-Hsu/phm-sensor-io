// SPDX-License-Identifier: Apache-2.0
//
// The Modbus RTU transport. The only file that includes <modbus.h>.
//
// libmodbus opens the device, applies the serial settings, frames the request
// with its CRC, waits for the reply within the response timeout and checks the
// reply's CRC. The request is sent with modbus_send_raw_request() and the
// reply taken with modbus_receive_confirmation(), so this file classifies the
// reply itself: libmodbus's own read functions turn an exception code they do
// not know into a generic error and lose it.
//
// Line state before every request frame, the first attempt of a read as well
// as each retry: the port waits until the RTU inter-frame silence has passed
// since its last exchange, then discards anything in the receive buffer, so a
// late reply to an earlier request, or bytes from a slave that answered after
// it was given up, are never taken as the start of this request's reply.
#include "modbus_rtu_port.hpp"

#include <modbus.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace phm::sensor_io::detail {

namespace {

constexpr std::uint8_t kExceptionBit = 0x80;
constexpr std::size_t kCrcLength = 2;
// slave, function, byte count
constexpr std::size_t kReplyHeaderLength = 3;
// slave, function, exception code, CRC
constexpr std::size_t kExceptionReplyLength = 3 + kCrcLength;

struct ModbusCloser {
  void operator()(modbus_t* context) const noexcept {
    modbus_close(context);
    modbus_free(context);
  }
};
using Context = std::unique_ptr<modbus_t, ModbusCloser>;

char parity_letter(Parity parity) noexcept {
  switch (parity) {
    case Parity::none:
      return 'N';
    case Parity::odd:
      return 'O';
    case Parity::even:
      break;
  }
  return 'E';
}

bool is_retried(Fault fault) noexcept {
  return fault == Fault::no_reply || fault == Fault::crc_mismatch ||
         fault == Fault::malformed_reply;
}

// The fault of a failed modbus_send_raw_request() or
// modbus_receive_confirmation(), from errno.
Fault fault_from_errno(int error) noexcept {
  switch (error) {
    case ETIMEDOUT:
      return Fault::no_reply;
    case EMBBADCRC:
      return Fault::crc_mismatch;
    case EMBBADDATA:
    case EMBBADEXC:
    case EMBBADSLAVE:
    case EMBMDATA:
    case EMBUNKEXC:
      return Fault::malformed_reply;
    default:
      return Fault::io_error;
  }
}

// One request frame and its reply.
struct Attempt {
  bool sent = false;
  Fault fault = Fault::none;
  std::optional<std::uint8_t> exception_code;
  std::vector<std::uint16_t> words;
};

// The registers in the data field of a valid reply, high byte first.
std::vector<std::uint16_t> words_of(std::span<const std::uint8_t> data) {
  std::vector<std::uint16_t> words;
  words.reserve(data.size() / 2);
  for (auto rest = data; rest.size() >= 2; rest = rest.subspan(2)) {
    const unsigned high = rest.front();
    const unsigned low = rest.subspan(1).front();
    words.push_back(static_cast<std::uint16_t>((high << 8U) | low));
  }
  return words;
}

// Checks a reply whose CRC libmodbus has verified and takes its registers.
Attempt parse_reply(std::span<const std::uint8_t> reply, std::uint8_t slave, std::uint8_t function,
                    std::uint16_t count) {
  Attempt attempt{.sent = true, .fault = Fault::malformed_reply, .exception_code = {}, .words = {}};
  if (reply.size() < kExceptionReplyLength) {
    return attempt;  // also a reply from another slave address, which libmodbus reports as empty
  }
  std::array<std::uint8_t, kReplyHeaderLength> header{};
  std::ranges::copy(reply.first(kReplyHeaderLength), header.begin());
  const auto [reply_slave, reply_function, third] = header;
  if (reply_slave != slave) {
    return attempt;
  }
  if (reply_function == (function | kExceptionBit)) {
    if (reply.size() == kExceptionReplyLength) {
      attempt.fault = Fault::exception_response;
      attempt.exception_code = third;  // the exception code
    }
    return attempt;
  }
  const std::size_t byte_count = std::size_t{count} * 2;
  if (reply_function != function || third != byte_count ||
      reply.size() != kReplyHeaderLength + byte_count + kCrcLength) {
    return attempt;
  }
  attempt.fault = Fault::none;
  attempt.words = words_of(reply.subspan(kReplyHeaderLength, byte_count));
  return attempt;
}

// One request frame on an open context and its reply.
Attempt transact(modbus_t* context, std::uint8_t slave, ReadFunction function, std::uint16_t first,
                 std::uint16_t count) {
  const auto code = static_cast<std::uint8_t>(function);
  if (modbus_set_slave(context, slave) == -1) {
    return Attempt{.sent = false, .fault = Fault::io_error, .exception_code = {}, .words = {}};
  }
  const std::array<std::uint8_t, 6> request{
      slave,
      code,
      static_cast<std::uint8_t>(first >> 8U),
      static_cast<std::uint8_t>(first & 0xFFU),
      static_cast<std::uint8_t>(count >> 8U),
      static_cast<std::uint8_t>(count & 0xFFU),
  };
  if (modbus_send_raw_request(context, request.data(), static_cast<int>(request.size())) == -1) {
    return Attempt{.sent = false, .fault = Fault::io_error, .exception_code = {}, .words = {}};
  }
  std::array<std::uint8_t, MODBUS_RTU_MAX_ADU_LENGTH> reply{};
  const int length = modbus_receive_confirmation(context, reply.data());
  if (length == -1) {
    return Attempt{
        .sent = true,
        .fault = fault_from_errno(errno),
        .exception_code = {},
        .words = {},
    };
  }
  return parse_reply(std::span(reply).first(static_cast<std::size_t>(length)), slave, code, count);
}

// A context for `config` with its device open, or empty when it cannot be opened.
Context open_device(const PortConfig& config) {
  const std::filesystem::path device = resolve_device(config);
  Context context{modbus_new_rtu(device.c_str(), static_cast<int>(config.serial.baud),
                                 parity_letter(config.serial.parity), 8, config.serial.stop_bits)};
  if (!context) {
    return context;
  }
  // These rules are the only retries; libmodbus neither reconnects nor
  // flushes on its own.
  modbus_set_error_recovery(context.get(), MODBUS_ERROR_RECOVERY_NONE);
  const auto timeout = config.transaction.response_timeout;
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(timeout);
  const auto microseconds =
      std::chrono::duration_cast<std::chrono::microseconds>(timeout - seconds);
  modbus_set_response_timeout(context.get(), static_cast<std::uint32_t>(seconds.count()),
                              static_cast<std::uint32_t>(microseconds.count()));
  if (modbus_connect(context.get()) == -1) {
    context.reset();  // modbus_close() on a context that failed to connect is harmless
  }
  return context;
}

}  // namespace

std::filesystem::path resolve_device(const PortConfig& port) { return port.device; }

std::chrono::microseconds inter_frame_silence(std::uint32_t baud) noexcept {
  constexpr std::uint32_t kFixedAbove = 19200;
  constexpr std::chrono::microseconds kFixedSilence{1750};
  if (baud > kFixedAbove) {
    return kFixedSilence;
  }
  // 3.5 characters of 11 bits: 38.5 bit times, rounded up to whole microseconds.
  constexpr std::uint64_t kBitTimesMicro = 38'500'000;
  const std::uint64_t rate = std::max<std::uint32_t>(baud, 1);
  return std::chrono::microseconds{static_cast<std::int64_t>((kBitTimesMicro + rate - 1) / rate)};
}

struct ModbusRtuPort::State {
  PortConfig config;
  Context context;  // empty while the device is closed
  // When the last request frame on this port finished: its reply was received,
  // or the wait for it ended. Empty before the first one.
  std::optional<std::chrono::steady_clock::time_point> last_exchange;
};

ModbusRtuPort::ModbusRtuPort(PortConfig config)
    : state_(std::make_unique<State>(
          State{.config = std::move(config), .context = nullptr, .last_exchange = {}})) {}

ModbusRtuPort::~ModbusRtuPort() = default;

Exchange ModbusRtuPort::read_registers(std::uint8_t slave, ReadFunction function,
                                       std::uint16_t first, std::uint16_t count) {
  Exchange exchange;
  if (!state_->context) {
    state_->context = open_device(state_->config);
    if (!state_->context) {
      exchange.fault = Fault::io_error;
      return exchange;
    }
  }
  const unsigned max_attempts = 1U + state_->config.transaction.retries;
  const std::chrono::microseconds silence = inter_frame_silence(state_->config.serial.baud);
  for (;;) {
    if (const auto last = state_->last_exchange) {
      std::this_thread::sleep_until(*last + silence);
    }
    modbus_flush(state_->context.get());  // discard a late or partial reply; not reported
    Attempt attempt = transact(state_->context.get(), slave, function, first, count);
    if (attempt.sent) {
      ++exchange.attempts;
      state_->last_exchange = std::chrono::steady_clock::now();
    }
    exchange.fault = attempt.fault;
    exchange.exception_code = attempt.exception_code;
    exchange.words = std::move(attempt.words);
    if (attempt.fault == Fault::io_error) {
      state_->context.reset();  // opened again on the next read
      break;
    }
    if (!is_retried(attempt.fault) || exchange.attempts >= max_attempts) {
      break;
    }
  }
  return exchange;
}

}  // namespace phm::sensor_io::detail
