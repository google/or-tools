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

void SolutionDetails::LoadFromTrail(const IntegerTrail& integer_trail) {
  const IntegerVariable num_vars = integer_trail.NumIntegerVariables();
  best_solution.resize(num_vars.value());
  // NOTE: There might be some variables which are not fixed.
  for (IntegerVariable var(0); var < num_vars; ++var) {
    best_solution[var] = integer_trail.LowerBound(var);
  }
  solution_count++;
}

bool SharedRINSNeighborhoodManager::AddNeighborhood(
    const RINSNeighborhood& rins_neighborhood) {
  absl::MutexLock lock(&mutex_);

  // Don't store this neighborhood if the current storage is already too large.
  // TODO(user): Consider instead removing one of the older neighborhoods.
  const int64 neighborhood_size = rins_neighborhood.fixed_vars.size() +
                                  rins_neighborhood.reduced_domain_vars.size();
  if (total_stored_vars_ + neighborhood_size > max_stored_vars()) {
    return false;
  }
  total_stored_vars_ += neighborhood_size;
  neighborhoods_.push_back(rins_neighborhood);
  VLOG(1) << "total stored vars: " << total_stored_vars_;
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
  const int64 neighborhood_size =
      neighborhood.fixed_vars.size() + neighborhood.reduced_domain_vars.size();
  total_stored_vars_ -= neighborhood_size;
  VLOG(1) << "total stored vars: " << total_stored_vars_;
  return neighborhood;
}

void AddRINSNeighborhood(Model* model) {
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* solution_details = model->Get<SolutionDetails>();
  const RINSVariables& rins_vars = *model->GetOrCreate<RINSVariables>();

  RINSNeighborhood rins_neighborhood;

  const double tolerance = 1e-6;
  for (const RINSVariable& rins_var : rins_vars.vars) {
    const IntegerVariable positive_var = rins_var.positive_var;

    if (integer_trail->IsCurrentlyIgnored(positive_var)) continue;

    LinearProgrammingConstraint* lp = rins_var.lp;
    if (lp == nullptr || !lp->HasSolution()) continue;
    const double lp_value = lp->GetSolutionValue(positive_var);

    if (solution_details == nullptr || solution_details->solution_count == 0) {
      // The tolerance make sure that if the lp_values is close to an integer,
      // then we fix the variable to this integer value.
      //
      // Important: the LP relaxation doesn't know about holes in the variable
      // domains, so the intersection of [domain_lb, domain_ub] with the initial
      // variable domain might be empty.
      const int64 domain_lb =
          static_cast<int64>(std::floor(lp_value + tolerance));
      const int64 domain_ub =
          static_cast<int64>(std::ceil(lp_value - tolerance));
      if (domain_lb >= integer_trail->LowerBound(positive_var) &&
          domain_ub <= integer_trail->UpperBound(positive_var)) {
        if (domain_lb == domain_ub) {
          rins_neighborhood.fixed_vars.push_back({rins_var, domain_lb});
        } else {
          rins_neighborhood.reduced_domain_vars.push_back(
              {rins_var, {domain_lb, domain_ub}});
        }

      } else {
        // The last lp_solution might not always be up to date.
        VLOG(2) << "RENS lp value out of bounds: " << lp_value
                << " LB: " << integer_trail->LowerBound(positive_var)
                << " UB: " << integer_trail->UpperBound(positive_var);
      }
    } else {
      if (positive_var >= solution_details->best_solution.size()) continue;

      const IntegerValue best_solution_value =
          solution_details->best_solution[positive_var];
      if (std::abs(best_solution_value.value() - lp_value) < 1e-4) {
        if (best_solution_value >= integer_trail->LowerBound(positive_var) &&
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
  }

  const int64 neighborhood_size = rins_neighborhood.fixed_vars.size() +
                                  rins_neighborhood.reduced_domain_vars.size();
  if (neighborhood_size > 0) {
    model->Mutable<SharedRINSNeighborhoodManager>()->AddNeighborhood(
        rins_neighborhood);
  }
}

}  // namespace sat
}  // namespace operations_research
