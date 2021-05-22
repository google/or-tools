// Copyright 2010-2021 Google LLC
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

#include "ortools/math_opt/cpp/objective.h"

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_map.h"
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research {
namespace math_opt {

void Objective::Maximize(const LinearExpression& objective) const {
  SetObjective(objective, true);
}

void Objective::Minimize(const LinearExpression& objective) const {
  SetObjective(objective, false);
}

void Objective::SetObjective(const LinearExpression& objective,
                             bool is_maximize) const {
  // LinearExpression that have no terms have a null model().
  if (!objective.raw_terms().empty()) {
    CHECK_EQ(objective.model(), model_)
        << internal::kObjectsFromOtherIndexedModel;
  }
  model_->clear_objective();
  model_->set_is_maximize(is_maximize);
  model_->set_objective_offset(objective.offset());
  for (auto [var, coef] : objective.raw_terms()) {
    model_->set_linear_objective_coefficient(var, coef);
  }
}

void Objective::Add(const LinearExpression& objective_terms) const {
  // LinearExpression that have no terms have a null model().
  if (!objective_terms.raw_terms().empty()) {
    CHECK_EQ(objective_terms.model(), model_)
        << internal::kObjectsFromOtherIndexedModel;
  }
  model_->set_objective_offset(objective_terms.offset() +
                               model_->objective_offset());
  for (auto [var, coef] : objective_terms.raw_terms()) {
    model_->set_linear_objective_coefficient(
        var, coef + model_->linear_objective_coefficient(var));
  }
}

LinearExpression Objective::AsLinearExpression() const {
  LinearExpression result = model_->objective_offset();
  for (const auto& [v, coef] : model_->linear_objective()) {
    result += Variable(model_, v) * coef;
  }
  return result;
}

}  // namespace math_opt
}  // namespace operations_research
