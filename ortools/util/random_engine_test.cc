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

#include "ortools/util/random_engine.h"

#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

#include "absl/random/bit_gen_ref.h"
#include "gtest/gtest.h"

// This test is just here to make sure that a type has been selected that
// models UniformRandomBitGenerator.
TEST(RandomEngineCompileTest, TestIt) {
  operations_research::random_engine_t engine;
  engine.seed(10);
  ASSERT_LT(engine.min(), engine.max());
  EXPECT_LT(-1, std::uniform_int_distribution<int>(0, 1)(engine));
}

namespace operations_research {
namespace {

TEST(RandomEngineTest, StableUniformIndexHasGoldenSequence) {
  random_engine_t engine(123456789);
  absl::BitGenRef random(engine);

  std::vector<uint64_t> values;
  for (int i = 0; i < 10; ++i) {
    values.push_back(StableUniformIndex(random, 17));
  }

  EXPECT_EQ(values,
            (std::vector<uint64_t>{5, 4, 2, 0, 14, 5, 1, 5, 9, 14}));
}

TEST(RandomEngineTest, StableUniformDoubleHasGoldenSequence) {
  random_engine_t engine(123456789);
  absl::BitGenRef random(engine);

  std::vector<double> values;
  for (int i = 0; i < 5; ++i) {
    values.push_back(StableUniformDouble(random));
  }

  EXPECT_EQ(values,
            (std::vector<double>{0x1.653e9fc646472p-2,
                                 0x1.114a7ccca34d7p-2,
                                 0x1.17da032f8db6dp-3,
                                 0x1.d3e02ed304ecfp-6,
                                 0x1.bce4d24af5c82p-1}));
}

TEST(RandomEngineTest, StableShuffleHasGoldenPermutation) {
  random_engine_t engine(123456789);
  absl::BitGenRef random(engine);
  std::vector<int> values(12);
  std::iota(values.begin(), values.end(), 0);

  StableShuffle(values.begin(), values.end(), random);

  EXPECT_EQ(values, (std::vector<int>{6, 7, 1, 10, 5, 2, 4, 8, 9, 0, 11, 3}));
}

}  // namespace
}  // namespace operations_research
