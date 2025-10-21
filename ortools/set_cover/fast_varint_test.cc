// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ortools/set_cover/fast_varint.h"

#include <cstdint>
#include <limits>

#include "gtest/gtest.h"

namespace operations_research {
namespace {

TEST(VonNeumannVarintTest, EncodingLength) {
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b0ULL), 1);
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b1ULL), 2);
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b11ULL), 3);
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b111ULL), 4);
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b1111ULL), 5);
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b11111ULL), 6);
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b111111ULL), 7);
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b1111111ULL), 8);
  EXPECT_EQ(VonNeumannVarint::EncodingLength(0b11111111ULL), 9);
}

TEST(VonNeumannVarintTest, BitWidth) {
  EXPECT_EQ(VonNeumannVarint::BitWidth(0), 1);
  EXPECT_EQ(VonNeumannVarint::BitWidth(1), 1);
  EXPECT_EQ(VonNeumannVarint::BitWidth(63), 6);
  EXPECT_EQ(VonNeumannVarint::BitWidth(64), 7);
  EXPECT_EQ(VonNeumannVarint::BitWidth(127), 7);
  EXPECT_EQ(VonNeumannVarint::BitWidth(128), 8);
  EXPECT_EQ(VonNeumannVarint::BitWidth(16383), 14);
  EXPECT_EQ(VonNeumannVarint::BitWidth(16384), 15);
  EXPECT_EQ(VonNeumannVarint::BitWidth(static_cast<uint64_t>(1) << 63), 64);
  EXPECT_EQ(VonNeumannVarint::BitWidth(std::numeric_limits<uint64_t>::max()),
            64);
}

TEST(VonNeumannVarintTest, DivBy7) {
  for (uint32_t n = 0; n < 90; ++n) {
    EXPECT_EQ(VonNeumannVarint::DivBy7(n), n / 7) << " n = " << n;
  }
}

TEST(VonNeumannVarintTest, SeptetWidth) {
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(0), 1);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 7) - 1), 1);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(1ULL << 7), 2);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 14) - 1), 2);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 14)), 3);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 21) - 1), 3);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(1ULL << 21), 4);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 28) - 1), 4);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(1ULL << 28), 5);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 35) - 1), 5);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(1ULL << 35), 6);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 42) - 1), 6);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(1ULL << 42), 7);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 49) - 1), 7);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(1ULL << 49), 8);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth((1ULL << 56) - 1), 8);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(1ULL << 56), 9);
  EXPECT_EQ(VonNeumannVarint::SeptetWidth(std::numeric_limits<uint64_t>::max()),
            10);
}

TEST(VonNeumannVarintTest, LowerBitsMask) {
  EXPECT_EQ(VonNeumannVarint::LowerBitsMask(0), 0);
  EXPECT_EQ(VonNeumannVarint::LowerBitsMask(1), 1);
  EXPECT_EQ(VonNeumannVarint::LowerBitsMask(7), 127);
  EXPECT_EQ(VonNeumannVarint::LowerBitsMask(8), 255);
  EXPECT_EQ(VonNeumannVarint::LowerBitsMask(63), (1ULL << 63) - 1);
  EXPECT_EQ(VonNeumannVarint::LowerBitsMask(64),
            std::numeric_limits<uint64_t>::max());
}

TEST(VonNeumannVarintTest, EncodeSmallVarint) {
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(0), 0ULL << 1 | 0);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(1), 1ULL << 1 | 0);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(127), 127ULL << 1 | 0);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(128), 128ULL << 2 | 1);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(16383), 16383ULL << 2 | 1);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(16384), 16384ULL << 3 | 3);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint((1ULL << 21) - 1),
            ((1ULL << 21) - 1) << 3 | 3);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(1ULL << 21),
            (1ULL << 21) << 4 | 7);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint((1ULL << 28) - 1),
            ((1ULL << 28) - 1) << 4 | 7);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(1ULL << 28),
            (1ULL << 28) << 5 | 15);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint((1ULL << 35) - 1),
            ((1ULL << 35) - 1) << 5 | 15);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(1ULL << 35),
            (1ULL << 35) << 6 | 31);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint((1ULL << 42) - 1),
            ((1ULL << 42) - 1) << 6 | 31);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(1ULL << 42),
            (1ULL << 42) << 7 | 63);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint((1ULL << 49) - 1),
            ((1ULL << 49) - 1) << 7 | 63);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint(1ULL << 49),
            (1ULL << 49) << 8 | 127);
  EXPECT_EQ(VonNeumannVarint::EncodeSmallVarint((1ULL << 56) - 1),
            ((1ULL << 56) - 1) << 8 | 127);
}

TEST(VonNeumannVarintTest, DecodeSmallVarint) {
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(0ULL << 1 | 0), 0);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(1ULL << 1 | 0), 1);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(127ULL << 1 | 0), 127);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(128ULL << 2 | 1), 128);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(16383ULL << 2 | 1), 16383);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(16384ULL << 3 | 3), 16384);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(((1ULL << 21) - 1) << 3 | 3),
            (1ULL << 21) - 1);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint((1ULL << 21) << 4 | 7),
            1ULL << 21);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(((1ULL << 28) - 1) << 4 | 7),
            (1ULL << 28) - 1);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint((1ULL << 28) << 5 | 15),
            1ULL << 28);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(((1ULL << 35) - 1) << 5 | 15),
            (1ULL << 35) - 1);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint((1ULL << 35) << 6 | 31),
            1ULL << 35);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(((1ULL << 42) - 1) << 6 | 31),
            (1ULL << 42) - 1);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint((1ULL << 42) << 7 | 63),
            1ULL << 42);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(((1ULL << 49) - 1) << 7 | 63),
            (1ULL << 49) - 1);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint((1ULL << 49) << 8 | 127),
            1ULL << 49);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(((1ULL << 56) - 1) << 8 | 127),
            (1ULL << 56) - 1);
  EXPECT_EQ(VonNeumannVarint::DecodeSmallVarint(
                std::numeric_limits<uint64_t>::max() << 8 | 127),
            std::numeric_limits<uint64_t>::max() >> 8);
}

}  // namespace
}  // namespace operations_research
