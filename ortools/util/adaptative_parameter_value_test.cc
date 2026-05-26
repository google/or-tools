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

#include "ortools/util/adaptative_parameter_value.h"

#include <cmath>

#include "absl/random/random.h"
#include "gtest/gtest.h"

namespace operations_research {
namespace {

TEST(AdaptiveParameterValueTest, StayBelowOne) {
  AdaptiveParameterValue param(0.5);
  for (int i = 0; i < 1e6; ++i) param.Increase();
  EXPECT_GE(param.value(), 0.99);
  EXPECT_LE(param.value(), 1.0);
}

TEST(AdaptiveParameterValueTest, StayAboveZero) {
  AdaptiveParameterValue param(0.5);
  for (int i = 0; i < 1e6; ++i) param.Decrease();
  EXPECT_GE(param.value(), 0.0);
  EXPECT_LE(param.value(), 0.001);
}

TEST(AdaptiveParameterValueTest, ConvergeToTarget) {
  absl::BitGen random;
  AdaptiveParameterValue param(0.5);
  for (int i = 0; i < 50; ++i) {
    const double target = absl::Uniform<double>(random, 0.0, 1.0);
    for (int j = 0; j < 10000; ++j) {
      if (param.value() < target) param.Increase();
      if (param.value() > target) param.Decrease();
    }

    // We expect convergence even after a lot of updates.
    EXPECT_GE(param.value(), target - 0.1);
    EXPECT_LE(param.value(), target + 0.1);
  }
}

TEST(AdaptiveParameterValueTest, ExponentialConvergenceToOne) {
  AdaptiveParameterValue param(0.5);
  for (int i = 0; i < 100; ++i) param.Increase();
  EXPECT_GE(param.value(), 1.0 - 1e-6);
}

TEST(AdaptiveParameterValueTest, ExponentialConvergenceToZero) {
  AdaptiveParameterValue param(0.5);
  for (int i = 0; i < 100; ++i) param.Decrease();
  EXPECT_LE(param.value(), 1e-6);
}

TEST(AdaptiveParameterValueTest, ConvergenceToMedian) {
  AdaptiveParameterValue param(0.5);

  // The test should work (modulo tolerances) for any increasing function f in
  // [0, 1] with f(0) = 0 and f(1) = 1.
  const auto f = [](double x) { return x * x; };

  // The final value should be around f(param.value) = 0.5 !
  // The convergence is pretty slow though.
  absl::BitGen random;
  for (int i = 0; i < 1e6; ++i) {
    const double decrease = absl::Bernoulli(random, f(param.value()));
    if (decrease) {
      param.Decrease();
    } else {
      param.Increase();
    }
  }

  // No flakiness in 1000 runs with this tolerance.
  EXPECT_NEAR(param.value(), sqrt(0.5), 0.05);
}

TEST(AdaptiveParameterValueTest, UpdateVsIncrease) {
  AdaptiveParameterValue param1(0.1);
  AdaptiveParameterValue param2(0.1);
  param1.Increase();
  param1.Increase();
  param1.Increase();
  param2.Update(/*num_decreases=*/0, /*num_increases=*/3);
  EXPECT_EQ(param1.value(), param2.value());
}

TEST(AdaptiveParameterValueTest, UpdateVsDecrease) {
  AdaptiveParameterValue param1(0.1);
  AdaptiveParameterValue param2(0.1);
  param1.Decrease();
  param1.Decrease();
  param2.Update(/*num_decreases=*/2, /*num_increases=*/0);
  EXPECT_EQ(param1.value(), param2.value());
}

TEST(AdaptiveParameterValueTest, GenericUpdate) {
  AdaptiveParameterValue param1(0.1);
  AdaptiveParameterValue param2(0.1);
  param1.Decrease();
  param1.Increase();
  param1.Increase();
  param1.Decrease();
  param2.Update(/*num_decreases=*/2, /*num_increases=*/2);
  EXPECT_EQ(param2.value(), 0.1);
  EXPECT_NE(param1.value(), param2.value());
}

}  // namespace
}  // namespace operations_research
