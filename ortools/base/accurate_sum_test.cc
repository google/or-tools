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

#include "ortools/base/accurate_sum.h"

#include <cmath>
#include <random>
#include <vector>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/base/accurate_sum.h"

namespace {

TEST(AccurateSumTest, ExactConsistencyWithUtilMath) {
  std::vector<double> data;
  {
    std::mt19937 random(12345);
    const int kNumNumbers = 1000000;
    data.assign(kNumNumbers, 0.0);
    for (int i = 0; i < kNumNumbers; ++i) {
      const double abs_value = exp2(absl::Uniform<double>(random, -100, 100.0));
      data[i] = absl::Bernoulli(random, 1.0 / 2) ? abs_value : -abs_value;
    }
  }
  operations_research::AccurateSum<double> forked_sum;
  AccurateSum<double> reference_sum;
  // Runtime: August 2013, fastbuild, forge: 4 seconds.
  const int kNumPasses = 100;
  for (int i = 0; i < kNumPasses; ++i) {
    for (const double v : data) {
      forked_sum.Add(v);
      reference_sum.Add(v);
    }
  }
  // We *do* mean to expect a rigorous floating-point equality.
  EXPECT_EQ(reference_sum.Value(), forked_sum.Value());
}

}  // namespace
