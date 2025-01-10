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

// TODO(user): once we have three solvers, test with two registered and one
// unregistered (to test sorting).

#include <string>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/parameters.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::StrEq;

TEST(NoRegisteredSolverTest, AllSolversRegistryTest) {
  EXPECT_FALSE(
      AllSolversRegistry::Instance()->IsRegistered(SOLVER_TYPE_GUROBI));
  EXPECT_TRUE(AllSolversRegistry::Instance()->IsRegistered(SOLVER_TYPE_GSCIP));
  EXPECT_THAT(AllSolversRegistry::Instance()->RegisteredSolvers(),
              ElementsAre(SOLVER_TYPE_GSCIP));
  EXPECT_THAT(AllSolversRegistry::Instance()->RegisteredSolversToString(),
              StrEq("[SOLVER_TYPE_GSCIP]"));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
