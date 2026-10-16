// SPDX-License-Identifier: Apache-2.0
//
// Result<T, E>: either a value or an error. C++20 has no std::expected, so the
// library carries this small type with the same member names; code written
// against it keeps compiling if it later becomes an alias of std::expected.
#pragma once

#include <cassert>
#include <utility>
#include <variant>

namespace phm::sensor_io {

// The error side of a Result, like std::unexpected.
template <class E>
struct Unexpected {
  E error;
};

// Either a value of type T or an error of type E. Member names follow std::expected.
// Calling value() or operator* on an error, or error() on a value, is a
// precondition violation: undefined behaviour, checked with assert in debug builds.
template <class T, class E>
class [[nodiscard]] Result {
 public:
  // Implicit, like std::expected.
  // NOLINTNEXTLINE(*-explicit-constructor,*-explicit-conversions)
  Result(T value) : state_(std::in_place_index<0>, std::move(value)) {}
  // Implicit, like std::expected from std::unexpected.
  // NOLINTNEXTLINE(*-explicit-constructor,*-explicit-conversions)
  Result(Unexpected<E> error) : state_(std::in_place_index<1>, std::move(error.error)) {}

  [[nodiscard]] bool has_value() const noexcept { return state_.index() == 0; }
  explicit operator bool() const noexcept { return has_value(); }

  [[nodiscard]] T& value() & {
    assert(has_value());
    return *std::get_if<0>(&state_);
  }
  [[nodiscard]] const T& value() const& {
    assert(has_value());
    return *std::get_if<0>(&state_);
  }
  [[nodiscard]] T&& value() && {
    assert(has_value());
    return std::move(*std::get_if<0>(&state_));
  }
  [[nodiscard]] const E& error() const& {
    assert(!has_value());
    return *std::get_if<1>(&state_);
  }

  T& operator*() & { return value(); }
  const T& operator*() const& { return value(); }
  T* operator->() { return &value(); }
  const T* operator->() const { return &value(); }

 private:
  std::variant<T, E> state_;
};

}  // namespace phm::sensor_io
