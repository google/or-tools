// Copyright 2010-2018 Google LLC
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

#include "ortools/sat/rins.h"

#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"

namespace operations_research {
namespace sat {

bool SharedRINSNeighborhoodManager::AddNeighborhood(
    const RINSNeighborhood& rins_neighborhood) {
  // Don't store this neighborhood if the current storage is already too large.
  // TODO(user): Consider instead removing one of the older neighborhoods.
  if (total_num_fixed_vars_ + rins_neighborhood.fixed_vars.size() >
      max_fixed_vars()) {
    return false;
  }

  absl::MutexLock lock(&mutex_);
  total_num_fixed_vars_ += rins_neighborhood.fixed_vars.size();
  neighborhoods_.push_back(std::move(rins_neighborhood));
  VLOG(1) << "total fixed vars: " << total_num_fixed_vars_;
  return true;
}

absl::optional<RINSNeighborhood>
SharedRINSNeighborhoodManager::GetUnexploredNeighborhood() {
  absl::MutexLock lock(&mutex_);
  if (neighborhoods_.empty()) {
    VLOG(2) << "No neighborhood to consume.";
    return absl::nullopt;
  }

  // return the last added neighborhood and remove it.
  const RINSNeighborhood neighborhood = std::move(neighborhoods_.back());
  neighborhoods_.pop_back();
  total_num_fixed_vars_ -= neighborhood.fixed_vars.size();
  VLOG(1) << "total fixed vars: " << total_num_fixed_vars_;
  return neighborhood;
}

void AddRINSNeighborhood(Model* model) {
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* solution_details = model->Get<SolutionDetails>();
  const RINSVariables& rins_vars = *model->GetOrCreate<RINSVariables>();

  if (solution_details == nullptr) return;

  RINSNeighborhood rins_neighborhood;

  for (const RINSVariable& rins_var : rins_vars.vars) {
    const IntegerVariable positive_var = rins_var.positive_var;

    if (integer_trail->IsCurrentlyIgnored(positive_var)) continue;

    // TODO(user): Perform caching to make this more efficient.
    LinearProgrammingConstraint* lp = rins_var.lp;
    if (lp == nullptr || !lp->HasSolution()) continue;
    if (positive_var >= solution_details->best_solution.size()) continue;

    const IntegerValue best_solution_value =
        solution_details->best_solution[positive_var];
    if (std::abs(best_solution_value.value() -
                 lp->GetSolutionValue(positive_var)) < 1e-4) {
      if (best_solution_value >= integer_trail->LowerBound(positive_var) ||
          best_solution_value <= integer_trail->UpperBound(positive_var)) {
        rins_neighborhood.fixed_vars.push_back(
            {rins_var, best_solution_value.value()});
      } else {
        // The last lp_solution might not always be up to date.
        VLOG(2) << "RINS common value out of bounds: " << best_solution_value
                << " LB: " << integer_trail->LowerBound(positive_var)
                << " UB: " << integer_trail->UpperBound(positive_var);
      }
    }
  }
  model->Mutable<SharedRINSNeighborhoodManager>()->AddNeighborhood(
      rins_neighborhood);
}

}  // namespace sat
}  // namespace operations_research
