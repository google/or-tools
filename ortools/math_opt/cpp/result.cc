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

#include "ortools/math_opt/cpp/result.h"

#include <string>
#include <utility>
#include <vector>

#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research {
namespace math_opt {

Result::PrimalSolution::PrimalSolution(IndexedModel* const model,
                                       IndexedPrimalSolution indexed_solution)
    : variable_values(model, std::move(indexed_solution.variable_values)),
      objective_value(indexed_solution.objective_value) {}

Result::PrimalRay::PrimalRay(IndexedModel* const model,
                             IndexedPrimalRay indexed_ray)
    : variable_values(model, std::move(indexed_ray.variable_values)) {}

Result::DualSolution::DualSolution(IndexedModel* const model,
                                   IndexedDualSolution indexed_solution)
    : dual_values(model, std::move(indexed_solution.dual_values)),
      reduced_costs(model, std::move(indexed_solution.reduced_costs)),
      objective_value(indexed_solution.objective_value) {}

Result::DualRay::DualRay(IndexedModel* const model, IndexedDualRay indexed_ray)
    : dual_values(model, std::move(indexed_ray.dual_values)),
      reduced_costs(model, std::move(indexed_ray.reduced_costs)) {}

Result::Basis::Basis(IndexedModel* const model, IndexedBasis indexed_basis)
    : constraint_status(model, std::move(indexed_basis.constraint_status)),
      variable_status(model, std::move(indexed_basis.variable_status)) {}

Result::Result(IndexedModel* const model, const SolveResultProto& solve_result)
    : warnings(solve_result.warnings().begin(), solve_result.warnings().end()),
      termination_reason(solve_result.termination_reason()),
      termination_detail(solve_result.termination_detail()),
      solve_stats(solve_result.solve_stats()) {
  IndexedSolutions solutions = IndexedSolutionsFromProto(solve_result);
  for (auto& primal_solution : solutions.primal_solutions) {
    primal_solutions.emplace_back(model, std::move(primal_solution));
  }
  for (auto& primal_ray : solutions.primal_rays) {
    primal_rays.emplace_back(model, std::move(primal_ray));
  }
  for (auto& dual_solution : solutions.dual_solutions) {
    dual_solutions.emplace_back(model, std::move(dual_solution));
  }
  for (auto& dual_ray : solutions.dual_rays) {
    dual_rays.emplace_back(model, std::move(dual_ray));
  }
  for (auto& base : solutions.basis) {
    basis.emplace_back(model, std::move(base));
  }
}

}  // namespace math_opt
}  // namespace operations_research
