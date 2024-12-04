// Copyright 2010-2024 Google LLC
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

#include "gtest/gtest.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_test_utils.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {
namespace {

// This just check that the code compile and runs.
TEST(LbTreeSearch, BooleanLinearOptimizationProblem) {
  const CpModelProto model_proto = RandomLinearProblem(50, 50);
  SatParameters params;
  params.set_optimize_with_lb_tree_search(true);
  params.set_log_search_progress(true);
  const CpSolverResponse response = SolveWithParameters(model_proto, params);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
