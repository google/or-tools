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

#include "ortools/sat/rins.h"

#include <cstdint>
#include <limits>

#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"

namespace operations_research {
namespace sat {

void RecordLPRelaxationValues(Model* model) {
  auto* lp_solutions = model->Mutable<SharedLPSolutionRepository>();
  if (lp_solutions == nullptr) return;

  const LPVariables& lp_vars = *model->GetOrCreate<LPVariables>();
  std::vector<double> relaxation_values(
      lp_vars.model_vars_size, std::numeric_limits<double>::infinity());

  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  for (const LPVariable& lp_var : lp_vars.vars) {
    const IntegerVariable positive_var = lp_var.positive_var;
    if (integer_trail->IsCurrentlyIgnored(positive_var)) continue;

    LinearProgrammingConstraint* lp = lp_var.lp;
    if (lp == nullptr || !lp->HasSolution()) continue;

    relaxation_values[lp_var.model_var] = lp->GetSolutionValue(positive_var);
  }
  lp_solutions->NewLPSolution(std::move(relaxation_values));
}

namespace {

std::vector<double> GetLPRelaxationValues(
    const SharedLPSolutionRepository* lp_solutions, absl::BitGenRef random) {
  std::vector<double> relaxation_values;

  if (lp_solutions == nullptr || lp_solutions->NumSolutions() == 0) {
    return relaxation_values;
  }

  // TODO(user): Experiment with random biased solutions.
  const SharedSolutionRepository<double>::Solution lp_solution =
      lp_solutions->GetRandomBiasedSolution(random);

  for (int model_var = 0; model_var < lp_solution.variable_values.size();
       ++model_var) {
    relaxation_values.push_back(lp_solution.variable_values[model_var]);
  }
  return relaxation_values;
}

std::vector<double> GetGeneralRelaxationValues(
    const SharedRelaxationSolutionRepository* relaxation_solutions,
    absl::BitGenRef random) {
  std::vector<double> relaxation_values;

  if (relaxation_solutions == nullptr ||
      relaxation_solutions->NumSolutions() == 0) {
    return relaxation_values;
  }
  const SharedSolutionRepository<int64_t>::Solution relaxation_solution =
      relaxation_solutions->GetRandomBiasedSolution(random);

  for (int model_var = 0;
       model_var < relaxation_solution.variable_values.size(); ++model_var) {
    relaxation_values.push_back(relaxation_solution.variable_values[model_var]);
  }
  return relaxation_values;
}

std::vector<double> GetIncompleteSolutionValues(
    SharedIncompleteSolutionManager* incomplete_solutions) {
  std::vector<double> empty_solution_values;

  if (incomplete_solutions == nullptr ||
      !incomplete_solutions->HasNewSolution()) {
    return empty_solution_values;
  }

  return incomplete_solutions->GetNewSolution();
}
}  // namespace

RINSNeighborhood GetRINSNeighborhood(
    const SharedResponseManager* response_manager,
    const SharedRelaxationSolutionRepository* relaxation_solutions,
    const SharedLPSolutionRepository* lp_solutions,
    SharedIncompleteSolutionManager* incomplete_solutions,
    absl::BitGenRef random) {
  RINSNeighborhood rins_neighborhood;

  const bool use_only_relaxation_values =
      (response_manager == nullptr ||
       response_manager->SolutionsRepository().NumSolutions() == 0);

  if (use_only_relaxation_values && lp_solutions == nullptr &&
      incomplete_solutions == nullptr) {
    // As of now RENS doesn't generate good neighborhoods from integer
    // relaxation solutions.
    return rins_neighborhood;
  }

  std::vector<double> relaxation_values;
  if (incomplete_solutions != nullptr) {
    relaxation_values = GetIncompleteSolutionValues(incomplete_solutions);
  } else if (lp_solutions != nullptr) {
    relaxation_values = GetLPRelaxationValues(lp_solutions, random);
  } else {
    CHECK(relaxation_solutions != nullptr)
        << "No relaxation solutions repository or lp solutions repository "
           "provided.";
    relaxation_values =
        GetGeneralRelaxationValues(relaxation_solutions, random);
  }
  if (relaxation_values.empty()) return rins_neighborhood;

  const double tolerance = 1e-6;
  const SharedSolutionRepository<int64_t>::Solution solution =
      use_only_relaxation_values
          ? SharedSolutionRepository<int64_t>::Solution()
          : response_manager->SolutionsRepository().GetRandomBiasedSolution(
                random);
  for (int model_var = 0; model_var < relaxation_values.size(); ++model_var) {
    const double relaxation_value = relaxation_values[model_var];

    if (relaxation_value == std::numeric_limits<double>::infinity()) {
      continue;
    }

    if (use_only_relaxation_values) {
      // The tolerance make sure that if the relaxation_value is close to an
      // integer, then we fix the variable to this integer value.
      //
      // Important: the LP relaxation doesn't know about holes in the variable
      // domains, so the intersection of [domain_lb, domain_ub] with the
      // initial variable domain might be empty.
      const int64_t domain_lb =
          static_cast<int64_t>(std::floor(relaxation_value + tolerance));
      const int64_t domain_ub =
          static_cast<int64_t>(std::ceil(relaxation_value - tolerance));
      if (domain_lb == domain_ub) {
        rins_neighborhood.fixed_vars.push_back({model_var, domain_lb});
      } else {
        rins_neighborhood.reduced_domain_vars.push_back(
            {model_var, {domain_lb, domain_ub}});
      }

    } else {
      const IntegerValue best_solution_value =
          IntegerValue(solution.variable_values[model_var]);
      if (std::abs(best_solution_value.value() - relaxation_value) < 1e-4) {
        rins_neighborhood.fixed_vars.push_back(
            {model_var, best_solution_value.value()});
      }
    }
  }

  return rins_neighborhood;
}

}  // namespace sat
}  // namespace operations_research
