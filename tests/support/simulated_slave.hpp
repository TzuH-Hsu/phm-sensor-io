// SPDX-License-Identifier: Apache-2.0
//
// A simulated Modbus RTU slave for the unit tests. Test code, not installed.
//
// It opens a pseudo-terminal pair and serves the master side from its own
// thread; device() is the other side's path, which a test puts in
// PortConfig::device. The library under test then talks to it through
// libmodbus exactly as to a real serial device: no hardware, no root, no
// fixed device path.
//
// It answers Read Holding Registers (0x03) and Read Input Registers (0x04)
// requests addressed to its slave address from the registers set with
// set_holding_registers() and set_input_registers(); a request for a register
// that was never set gets exception 0x02 (illegal data address), another
// function exception 0x01. A request with a bad CRC or for another slave
// address gets no reply, as on a real bus. Every request frame is recorded.
#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace phm::sensor_io::testing {

// CRC-16/MODBUS of `bytes` (polynomial 0xA001 reflected, initial 0xFFFF).
// On the wire it is sent low byte first.
[[nodiscard]] std::uint16_t crc16_modbus(std::span<const std::uint8_t> bytes) noexcept;

class SimulatedSlave {
 public:
  explicit SimulatedSlave(std::uint8_t slave_address);  // opens the pty pair, starts serving
  ~SimulatedSlave();                                    // stops the thread, closes the pty
  SimulatedSlave(const SimulatedSlave&) = delete;
  SimulatedSlave& operator=(const SimulatedSlave&) = delete;
  SimulatedSlave(SimulatedSlave&&) = delete;
  SimulatedSlave& operator=(SimulatedSlave&&) = delete;

  [[nodiscard]] std::filesystem::path device() const;  // path to put in PortConfig::device

  void set_input_registers(std::uint16_t first, std::vector<std::uint16_t> words);
  void set_holding_registers(std::uint16_t first, std::vector<std::uint16_t> words);

  // Faults for the next `count` requests addressed to this slave; after them
  // the slave answers normally. Several faults can apply to one reply.
  void stay_silent(std::size_t count);
  void corrupt_crc(std::size_t count);  // flips the lowest bit of the last CRC byte
  void reply_from_address(std::uint8_t address, std::size_t count);
  void reply_with_function(std::uint8_t function, std::size_t count);
  // Sets the byte count field to `byte_count` and sends that many data bytes.
  void reply_with_byte_count(std::uint8_t byte_count, std::size_t count);
  void reply_exception(std::uint8_t exception_code, std::size_t count);
  // Sends the next `count` replies `delay` after their request arrived.
  void reply_late(std::chrono::milliseconds delay, std::size_t count);
  // Holds every reply until `release` returns true (for the concurrency test of #57).
  void hold_replies_until(std::function<bool()> release);

  [[nodiscard]] std::size_t requests_received() const;
  [[nodiscard]] std::vector<std::vector<std::uint8_t>> request_frames() const;

  // When one request frame arrived and, if the slave answered it, when the
  // last byte of the reply was written; on a steady clock.
  struct FrameTimes {
    std::chrono::steady_clock::time_point request_received;
    std::optional<std::chrono::steady_clock::time_point> reply_sent;
  };
  // One entry per request frame, in the order of request_frames().
  [[nodiscard]] std::vector<FrameTimes> request_times() const;

 private:
  template <class T>
  struct Pending {
    T value{};
    std::size_t count = 0;
  };

  // slave, function, first (2), count (2), CRC (2): every 0x03 and 0x04 request.
  using Request = std::array<std::uint8_t, 8>;

  void serve();
  void handle(const Request& request, std::chrono::steady_clock::time_point received);
  // The reply to `request` with the pending faults applied; empty for none.
  std::vector<std::uint8_t> reply_to(const Request& request);
  std::vector<std::uint8_t> read_reply(std::uint8_t function, std::uint16_t first,
                                       std::uint16_t count) const;
  void write_all(std::span<const std::uint8_t> bytes) const;

  const std::uint8_t address_;
  int master_fd_ = -1;
  int slave_fd_ = -1;  // held open so the master side never sees a hang-up
  std::filesystem::path device_;

  mutable std::mutex mutex_;
  std::map<std::uint16_t, std::uint16_t> holding_;
  std::map<std::uint16_t, std::uint16_t> input_;
  std::size_t silent_ = 0;
  std::size_t corrupt_crc_ = 0;
  Pending<std::uint8_t> address_override_;
  Pending<std::uint8_t> function_override_;
  Pending<std::uint8_t> byte_count_override_;
  Pending<std::uint8_t> exception_;
  Pending<std::chrono::milliseconds> late_;
  std::function<bool()> release_;
  std::vector<std::vector<std::uint8_t>> frames_;
  std::vector<FrameTimes> times_;

  std::atomic<bool> stop_{false};
  std::thread thread_;
};

}  // namespace phm::sensor_io::testing
