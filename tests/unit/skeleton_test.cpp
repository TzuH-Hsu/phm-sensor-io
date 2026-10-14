// SPDX-License-Identifier: Apache-2.0
//
// Includes every public header, so each one is compiled with the project
// warnings and checked by clang-tidy even before the library has sources.
#include <gtest/gtest.h>

#include "phm/sensor_io/sensor_io.hpp"

TEST(Skeleton, PublicHeadersCompile) { SUCCEED(); }
