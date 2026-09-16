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

#include "ortools/math_opt/cpp/executor/time_limit_util.h"

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {
namespace {

TEST(UpdateTimeLimit, ShrinkWithRelativeDeadline) {
  math_opt::SolveParameters params;
  UpdateTimeLimit(absl::Seconds(2), params);
  EXPECT_EQ(params.time_limit, absl::Seconds(2));
}

TEST(UpdateTimeLimit, NoShrinkWithRelativeDeadline) {
  math_opt::SolveParameters params;
  params.time_limit = absl::Seconds(4);
  UpdateTimeLimit(absl::Seconds(8), params);
  EXPECT_EQ(params.time_limit, absl::Seconds(4));
}

TEST(UpdateTimeLimit, NegativeLimitIsZeroRelativeDeadline) {
  math_opt::SolveParameters params;
  UpdateTimeLimit(absl::Seconds(-2), params);
  EXPECT_EQ(params.time_limit, absl::ZeroDuration());
}

TEST(UpdateTimeLimit, AbsoluteDeadline) {
  math_opt::SolveParameters params;
  UpdateTimeLimit(absl::Now() + absl::Seconds(5), params);
  EXPECT_LE(params.time_limit, absl::Seconds(5));
  EXPECT_GE(params.time_limit, absl::Seconds(4));
}

}  // namespace
}  // namespace operations_research::math_opt
