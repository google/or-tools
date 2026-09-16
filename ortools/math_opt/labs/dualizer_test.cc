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

#include "ortools/math_opt/labs/dualizer.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::EqualsProto;
using ::testing::status::IsOkAndHolds;

constexpr double kInf = std::numeric_limits<double>::infinity();

// We test the problem
//
// min_{x, rhs}   rhs
// s.t.
//                                       Sum(x) == 2
//     max_w{ Sum((w[i]+w[i+1]+i) * x[i] : w in W} <= rhs
//
// for W = { w : Sum(w) == 3}. The optimal solution has x[0] = x[1] = 1 and
// x[i] = 0 for i > 1. For this x, the max over w yields w[0] = w[1] = w[2] = 1.
// Then the robust constraint implies  (2 + 0) * 1 + (2 + 1) * 1 <= rhs. Hence,
// the optimal value is rhs = 5.

void BuildModels(Model& uncertainty_model, Model& dualized_model, const int n,
                 const int k) {
  const int x_size = n - 1;

  std::vector<Variable> u;
  for (int i = 0; i < n; ++i) {
    u.push_back(uncertainty_model.AddContinuousVariable(0.0, 1.0,
                                                        absl::StrCat("u_", i)));
  }
  uncertainty_model.AddLinearConstraint(Sum(u) == k);

  Variable dualized_obj_var =
      dualized_model.AddContinuousVariable(-kInf, kInf, "obj");
  std::vector<Variable> dualized_x;
  for (int i = 0; i < x_size; ++i) {
    dualized_x.push_back(
        dualized_model.AddContinuousVariable(0.0, 1.0, absl::StrCat("x_", i)));
  }
  dualized_model.AddLinearConstraint(Sum(dualized_x) == 2);
  dualized_model.Minimize(dualized_obj_var);

  std::vector<std::pair<LinearExpression, Variable>> uncertain_coefficients;
  for (int i = 0; i < x_size; ++i) {
    uncertain_coefficients.push_back({u[i] + u[i + 1] + i, dualized_x[i]});
  }

  AddRobustConstraint(uncertainty_model, dualized_obj_var,
                      uncertain_coefficients, dualized_model);
}

TEST(AddRobustConstraintTest, SimpleTest) {
  const int n = 5;
  const int k = 3;
  Model uncertainty_model("Uncertainty model");
  Model dualized_model("Dualized model");
  BuildModels(uncertainty_model, dualized_model, n, k);
  EXPECT_THAT(Solve(dualized_model, SolverType::kGlop),
              IsOkAndHolds(IsOptimal(5.0)));
}

TEST(AddRobustConstraintTest, DeterminismTest) {
  const int n = 5;
  const int k = 3;
  Model uncertainty_model("Uncertainty model");
  Model dualized_model("Dualized model");
  BuildModels(uncertainty_model, dualized_model, n, k);
  for (int i = 0; i < 3; ++i) {
    Model uncertainty_model_copy("Uncertainty model");
    Model dualized_model_copy("Dualized model");
    BuildModels(uncertainty_model_copy, dualized_model_copy, n, k);
    EXPECT_THAT(dualized_model_copy.ExportModel(),
                EqualsProto(dualized_model.ExportModel()));
  }
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
