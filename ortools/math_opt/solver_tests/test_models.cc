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

#include "ortools/math_opt/solver_tests/test_models.h"

#include <memory>
#include <vector>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

std::unique_ptr<Model> SmallModel(const bool integer) {
  auto model = std::make_unique<Model>("small_model");
  const Variable x_1 = model->AddVariable(0.0, 1.0, integer, "x_1");
  const Variable y_1 = model->AddVariable(0.0, 1.0, integer, "y_1");
  model->AddLinearConstraint(x_1 + y_1 <= 1.5);

  const Variable x_2 = model->AddVariable(0.0, 1.0, integer, "x_2");
  const Variable y_2 = model->AddVariable(0.0, 1.0, integer, "y_2");
  model->AddLinearConstraint(x_2 + y_2 <= 1.5);

  const Variable x_3 = model->AddVariable(0.0, 1.0, integer, "x_3");
  const Variable y_3 = model->AddVariable(0.0, 1.0, integer, "y_3");
  model->AddLinearConstraint(x_3 + y_3 <= 1.5);

  model->Maximize(3.0 * x_1 + 2.0 * y_1 + 3.0 * x_2 + 2.0 * y_2 + 3.0 * x_3 +
                  2.0 * y_3);
  return model;
}

std::unique_ptr<Model> DenseIndependentSet(const bool integer, const int n) {
  CHECK_GT(n, 0);

  auto model = std::make_unique<Model>("dense_independent_set");

  // Problem data.
  constexpr int m = 3;
  const std::vector<double> c = {5.0, 4.0, 3.0};

  // Add the variables
  std::vector<std::vector<Variable>> x(m);
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      x[i].push_back(
          model->AddVariable(0, 1, integer, absl::StrCat("x_", i, "_", j)));
    }
  }

  // Set the objective.
  {
    LinearExpression objective;
    for (int i = 0; i < m; ++i) {
      for (int j = 0; j < n; ++j) {
        objective += c[i] * x[i][j];
      }
    }
    model->Maximize(objective);
  }

  // Constraints of type (A).
  for (int i = 0; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      for (int k = j + 1; k < n; ++k) {
        model->AddLinearConstraint(x[i][j] + x[i][k] <= 1);
      }
    }
  }

  // Constraints of type (B).
  static_assert(m >= 3);
  for (int i = 1; i < m; ++i) {
    for (int j = 0; j < n; ++j) {
      for (int k = 0; k < n; ++k) {
        model->AddLinearConstraint(x[0][j] + x[i][k] <= 1);
      }
    }
  }

  return model;
}

ModelSolveParameters::SolutionHint DenseIndependentSetHint5(
    const Model& model) {
  CHECK_EQ(model.num_variables() % 3, 0) << model.num_variables();
  ModelSolveParameters::SolutionHint result;
  const auto vars = model.SortedVariables();
  for (const Variable v : vars) {
    result.variable_values[v] = 0.0;
  }
  if (!vars.empty()) {
    result.variable_values[vars.front()] = 1.0;
  }
  return result;
}

std::unique_ptr<Model> IndependentSetCompleteGraph(const bool integer,
                                                   const int n) {
  CHECK_GT(n, 0);

  auto model = std::make_unique<Model>("Simple incomplete solve LP");

  std::vector<Variable> x;
  for (int i = 0; i < n; ++i) {
    x.push_back(model->AddVariable(0.0, 1.0, integer));
  }

  for (int i = 0; i < n; ++i) {
    for (int j = i + 1; j < n; ++j) {
      model->AddLinearConstraint(x[i] + x[j] <= 1);
    }
  }

  model->Maximize(Sum(x));

  return model;
}

}  // namespace operations_research::math_opt
