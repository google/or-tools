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

#include "ortools/math_opt/solvers/gscip/gscip_parameters.h"

#include "absl/time/time.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"

namespace operations_research {
namespace {

using ::testing::Contains;
using ::testing::EqualsProto;
using ::testing::Pair;

TEST(GScipParametersTest, Threads) {
  GScipParameters parameters;
  EXPECT_EQ(1, GScipMaxNumThreads(parameters));
  GScipSetMaxNumThreads(2, &parameters);
  EXPECT_EQ(2, GScipMaxNumThreads(parameters));
  GScipParameters expected;
  (*expected.mutable_int_params())["parallel/maxnthreads"] = 2;
  EXPECT_THAT(parameters, EqualsProto(expected));
}

TEST(GScipParametersTest, GScipTimeLimit) {
  GScipParameters parameters;
  EXPECT_EQ(absl::InfiniteDuration(), GScipTimeLimit(parameters));
  GScipSetTimeLimit(absl::Seconds(10), &parameters);
  EXPECT_EQ(absl::Seconds(10), GScipTimeLimit(parameters));
  GScipParameters expected;
  (*expected.mutable_real_params())["limits/time"] = 10.0;
  EXPECT_THAT(parameters, EqualsProto(expected));
}

TEST(GScipParametersTest, GScipTimeLimitZero) {
  GScipParameters parameters;
  GScipSetTimeLimit(absl::Duration(), &parameters);
  EXPECT_EQ(absl::Duration(), GScipTimeLimit(parameters));
  GScipParameters expected;
  (*expected.mutable_real_params())["limits/time"] = 0.0;
  EXPECT_THAT(parameters, EqualsProto(expected));
}

TEST(GScipParametersTest, CatchCtrlC) {
  GScipParameters parameters;

  EXPECT_FALSE(GScipCatchCtrlCSet(parameters));
  EXPECT_TRUE(GScipCatchCtrlC(parameters));

  GScipSetCatchCtrlC(true, &parameters);

  EXPECT_TRUE(GScipCatchCtrlCSet(parameters));
  EXPECT_TRUE(GScipCatchCtrlC(parameters));

  GScipSetCatchCtrlC(false, &parameters);

  EXPECT_TRUE(GScipCatchCtrlCSet(parameters));
  EXPECT_FALSE(GScipCatchCtrlC(parameters));
}

TEST(GScipParametersTest, DisableAllCutsExceptUserDefinedNoCrash) {
  GScipParameters parameters;
  DisableAllCutsExceptUserDefined(&parameters);
  EXPECT_THAT(parameters.int_params(),
              Contains(Pair("separating/clique/freq", -1)));
}

}  // namespace
}  // namespace operations_research
