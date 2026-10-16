// SPDX-License-Identifier: Apache-2.0
#include "simulated_slave.hpp"

#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace phm::sensor_io::testing {

namespace {

constexpr std::uint8_t kReadHoldingRegisters = 0x03;
constexpr std::uint8_t kReadInputRegisters = 0x04;
constexpr std::uint8_t kExceptionBit = 0x80;
constexpr std::uint8_t kIllegalFunction = 0x01;
constexpr std::uint8_t kIllegalDataAddress = 0x02;
constexpr std::size_t kRequestLength = 8;
constexpr std::uint16_t kMaxReadCount = 125;
constexpr int kPollIntervalMs = 5;

[[noreturn]] void fail(const std::string& what) {
  throw std::system_error(errno, std::generic_category(), what);
}

std::uint16_t word_of(std::uint8_t high, std::uint8_t low) {
  return static_cast<std::uint16_t>((unsigned{high} << 8U) | unsigned{low});
}

std::vector<std::uint8_t> exception_reply(std::uint8_t slave, std::uint8_t function,
                                          std::uint8_t code) {
  return {slave, static_cast<std::uint8_t>(function | kExceptionBit), code};
}

void append_crc(std::vector<std::uint8_t>& frame) {
  const std::uint16_t crc = crc16_modbus(frame);
  frame.push_back(static_cast<std::uint8_t>(crc & 0xFFU));
  frame.push_back(static_cast<std::uint8_t>(crc >> 8U));
}

template <class T>
bool take(T& pending) {
  if (pending.count == 0) {
    return false;
  }
  --pending.count;
  return true;
}

bool take(std::size_t& pending) {
  if (pending == 0) {
    return false;
  }
  --pending;
  return true;
}

}  // namespace

std::uint16_t crc16_modbus(std::span<const std::uint8_t> bytes) noexcept {
  std::uint16_t crc = 0xFFFF;
  for (const std::uint8_t byte : bytes) {
    crc ^= byte;
    for (int bit = 0; bit < 8; ++bit) {
      const bool carry = (crc & 1U) != 0;
      crc = static_cast<std::uint16_t>(crc >> 1U);
      if (carry) {
        crc ^= 0xA001U;
      }
    }
  }
  return crc;
}

SimulatedSlave::SimulatedSlave(std::uint8_t slave_address)
    : address_(slave_address), master_fd_(posix_openpt(O_RDWR | O_NOCTTY)) {
  if (master_fd_ == -1) {
    fail("posix_openpt");
  }
  std::array<char, 128> name{};
  if (grantpt(master_fd_) == -1 || unlockpt(master_fd_) == -1 ||
      ptsname_r(master_fd_, name.data(), name.size()) != 0) {
    const int error = errno;
    close(master_fd_);
    errno = error;
    fail("pseudo-terminal setup");
  }
  device_ = name.data();
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
  slave_fd_ = open(name.data(), O_RDWR | O_NOCTTY);
  if (slave_fd_ == -1) {
    const int error = errno;
    close(master_fd_);
    errno = error;
    fail("open " + device_.string());
  }
  // Raw mode, so nothing is echoed or translated before libmodbus sets its own.
  termios settings{};
  tcgetattr(slave_fd_, &settings);
  cfmakeraw(&settings);
  tcsetattr(slave_fd_, TCSANOW, &settings);
  thread_ = std::thread([this] { serve(); });
}

SimulatedSlave::~SimulatedSlave() {
  stop_ = true;
  thread_.join();
  close(slave_fd_);
  close(master_fd_);
}

std::filesystem::path SimulatedSlave::device() const { return device_; }

// The signature is the one issue #1 gives.
// NOLINTNEXTLINE(performance-unnecessary-value-param)
void SimulatedSlave::set_input_registers(std::uint16_t first, std::vector<std::uint16_t> words) {
  const std::scoped_lock lock(mutex_);
  auto address = first;
  for (const std::uint16_t word : words) {
    input_[address++] = word;
  }
}

// The signature is the one issue #1 gives.
// NOLINTNEXTLINE(performance-unnecessary-value-param)
void SimulatedSlave::set_holding_registers(std::uint16_t first, std::vector<std::uint16_t> words) {
  const std::scoped_lock lock(mutex_);
  auto address = first;
  for (const std::uint16_t word : words) {
    holding_[address++] = word;
  }
}

void SimulatedSlave::stay_silent(std::size_t count) {
  const std::scoped_lock lock(mutex_);
  silent_ = count;
}

void SimulatedSlave::corrupt_crc(std::size_t count) {
  const std::scoped_lock lock(mutex_);
  corrupt_crc_ = count;
}

void SimulatedSlave::reply_from_address(std::uint8_t address, std::size_t count) {
  const std::scoped_lock lock(mutex_);
  address_override_ = {.value = address, .count = count};
}

void SimulatedSlave::reply_with_function(std::uint8_t function, std::size_t count) {
  const std::scoped_lock lock(mutex_);
  function_override_ = {.value = function, .count = count};
}

void SimulatedSlave::reply_with_byte_count(std::uint8_t byte_count, std::size_t count) {
  const std::scoped_lock lock(mutex_);
  byte_count_override_ = {.value = byte_count, .count = count};
}

void SimulatedSlave::reply_exception(std::uint8_t exception_code, std::size_t count) {
  const std::scoped_lock lock(mutex_);
  exception_ = {.value = exception_code, .count = count};
}

void SimulatedSlave::hold_replies_until(std::function<bool()> release) {
  const std::scoped_lock lock(mutex_);
  release_ = std::move(release);
}

std::size_t SimulatedSlave::requests_received() const {
  const std::scoped_lock lock(mutex_);
  return frames_.size();
}

std::vector<std::vector<std::uint8_t>> SimulatedSlave::request_frames() const {
  const std::scoped_lock lock(mutex_);
  return frames_;
}

void SimulatedSlave::serve() {
  std::vector<std::uint8_t> pending;
  std::array<std::uint8_t, 256> buffer{};
  while (!stop_) {
    pollfd poll_fd{.fd = master_fd_, .events = POLLIN, .revents = 0};
    if (poll(&poll_fd, 1, kPollIntervalMs) <= 0 ||
        (static_cast<unsigned>(poll_fd.revents) & static_cast<unsigned>(POLLIN)) == 0U) {
      continue;
    }
    const ssize_t length = ::read(master_fd_, buffer.data(), buffer.size());
    if (length <= 0) {
      continue;
    }
    const auto received = std::span(buffer).first(static_cast<std::size_t>(length));
    pending.insert(pending.end(), received.begin(), received.end());
    while (pending.size() >= kRequestLength) {
      Request request{};
      std::copy_n(pending.begin(), kRequestLength, request.begin());
      pending.erase(pending.begin(), std::next(pending.begin(), kRequestLength));
      handle(request);
    }
  }
}

void SimulatedSlave::handle(const Request& request) {
  std::vector<std::uint8_t> reply;
  std::function<bool()> release;
  {
    const std::scoped_lock lock(mutex_);
    frames_.emplace_back(request.begin(), request.end());
    reply = reply_to(request);
    release = release_;
  }
  if (reply.empty()) {
    return;
  }
  while (release && !release() && !stop_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  write_all(reply);
}

std::vector<std::uint8_t> SimulatedSlave::reply_to(const Request& request) {
  const auto [slave, function, first_high, first_low, count_high, count_low, crc_low, crc_high] =
      request;
  if (crc16_modbus(std::span(request).first(kRequestLength - 2)) != word_of(crc_high, crc_low) ||
      slave != address_ || take(silent_)) {
    return {};
  }
  std::vector<std::uint8_t> reply =
      take(exception_)
          ? exception_reply(address_, function, exception_.value)
          : read_reply(function, word_of(first_high, first_low), word_of(count_high, count_low));
  if (take(address_override_)) {
    reply.at(0) = address_override_.value;
  }
  if (take(function_override_)) {
    reply.at(1) = function_override_.value;
  }
  if (take(byte_count_override_)) {
    reply.resize(3 + std::size_t{byte_count_override_.value});  // pads with zero bytes
    reply.at(2) = byte_count_override_.value;
  }
  append_crc(reply);
  if (take(corrupt_crc_)) {
    reply.back() ^= 0x01U;
  }
  return reply;
}

std::vector<std::uint8_t> SimulatedSlave::read_reply(std::uint8_t function, std::uint16_t first,
                                                     std::uint16_t count) const {
  const std::map<std::uint16_t, std::uint16_t>* registers = nullptr;
  if (function == kReadHoldingRegisters) {
    registers = &holding_;
  } else if (function == kReadInputRegisters) {
    registers = &input_;
  } else {
    return exception_reply(address_, function, kIllegalFunction);
  }
  if (count == 0 || count > kMaxReadCount) {
    return exception_reply(address_, function, kIllegalDataAddress);
  }
  std::vector<std::uint8_t> reply{address_, function, static_cast<std::uint8_t>(count * 2)};
  for (std::uint32_t address = first; address < std::uint32_t{first} + count; ++address) {
    const auto found = registers->find(static_cast<std::uint16_t>(address));
    if (found == registers->end()) {
      return exception_reply(address_, function, kIllegalDataAddress);
    }
    reply.push_back(static_cast<std::uint8_t>(found->second >> 8U));
    reply.push_back(static_cast<std::uint8_t>(found->second & 0xFFU));
  }
  return reply;
}

void SimulatedSlave::write_all(std::span<const std::uint8_t> bytes) const {
  while (!bytes.empty()) {
    const ssize_t written = ::write(master_fd_, bytes.data(), bytes.size());
    if (written <= 0) {
      return;
    }
    bytes = bytes.subspan(static_cast<std::size_t>(written));
  }
}

}  // namespace phm::sensor_io::testing
