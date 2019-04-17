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
    const absl::flat_hash_map<IntegerVariable, IntegerValue>& fixed_vars,
    const Model& model) {
  auto* mapping = model.Get<CpModelMapping>();
  if (mapping == nullptr) {
    return false;
  }

  // Don't store this neighborhood if the current storage is already too large.
  // TODO(user): Consider instead removing one of the older neighborhoods.
  if (total_num_fixed_vars_ + fixed_vars.size() > max_fixed_vars()) {
    return false;
  }

  RINSNeighborhood rins_neighborhood;
  // TODO(user): Use GetProtoVariableFromIntegerVariable instead and change
  // the api to take a list of fixed vars instead of a hash map.
  for (int i = 0; i < num_model_vars_; ++i) {
    if (mapping->IsInteger(i)) {
      const IntegerVariable var = mapping->Integer(i);
      if (fixed_vars.contains(var)) {
        rins_neighborhood.fixed_vars.push_back(
            {i, fixed_vars.find(var)->second.value()});
      } else if (fixed_vars.contains(NegationOf(var))) {
        rins_neighborhood.fixed_vars.push_back(
            {i, -fixed_vars.find(var)->second.value()});
      }
    }
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
  absl::flat_hash_map<IntegerVariable, IntegerValue> fixed_vars;
  auto* solution_details = model->GetOrCreate<SolutionDetails>();

  const int size = solution_details->best_solution.size();
  for (IntegerVariable var(0); var < size; ++var) {
    auto* lp_dispatcher = model->GetOrCreate<LinearProgrammingDispatcher>();

    const IntegerVariable positive_var = PositiveVariable(var);

    if (integer_trail->IsCurrentlyIgnored(positive_var)) continue;

    // TODO(user): Perform caching to make this more efficient.
    LinearProgrammingConstraint* lp =
        gtl::FindWithDefault(*lp_dispatcher, positive_var, nullptr);
    if (lp == nullptr || !lp->HasSolution()) continue;

    const IntegerValue best_solution_value =
        solution_details->best_solution[var];
    if (std::abs(best_solution_value.value() -
                 lp->GetSolutionValue(positive_var)) < 1e-4) {
      if (best_solution_value >= integer_trail->LowerBound(var) ||
          best_solution_value <= integer_trail->UpperBound(var)) {
        fixed_vars[positive_var] = best_solution_value;
      } else {
        // The last lp_solution might not always be up to date.
        VLOG(2) << "RINS common value out of bounds: " << best_solution_value
                << " LB: " << integer_trail->LowerBound(var)
                << " UB: " << integer_trail->UpperBound(var);
      }
    }
  }
  VLOG(2) << "RINS Fixed: " << fixed_vars.size() << "/" << size;
  model->Mutable<SharedRINSNeighborhoodManager>()->AddNeighborhood(fixed_vars,
                                                                   *model);
}

}  // namespace sat
}  // namespace operations_research
