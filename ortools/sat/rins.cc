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

#include <limits>

#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

void RecordLPRelaxationValues(Model* model) {
  auto* lp_solutions = model->Mutable<SharedLPSolutionRepository>();
  if (lp_solutions == nullptr) return;

  const LPVariables& lp_vars = *model->GetOrCreate<LPVariables>();
  std::vector<double> relaxation_values(
      lp_vars.model_vars_size, std::numeric_limits<double>::infinity());

  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();

  for (const LPVariable& lp_var : lp_vars.vars) {
    const IntegerVariable positive_var = lp_var.positive_var;
    const int model_var = lp_var.model_var;
    if (integer_trail->IsCurrentlyIgnored(positive_var)) continue;

    LinearProgrammingConstraint* lp = lp_var.lp;
    if (lp == nullptr || !lp->HasSolution()) continue;
    const double lp_value = lp->GetSolutionValue(positive_var);

    relaxation_values[model_var] = lp_value;
  }
  lp_solutions->NewLPSolution(relaxation_values);
}

namespace {

std::vector<double> GetLPRelaxationValues(const Model* model) {
  std::vector<double> relaxation_values;

  auto* lp_solutions = model->Get<SharedLPSolutionRepository>();

  if (lp_solutions == nullptr || lp_solutions->NumSolutions() == 0) {
    return relaxation_values;
  }
  // TODO(user): Experiment with random biased solutions.
  const SharedSolutionRepository<double>::Solution lp_solution =
      lp_solutions->GetSolution(0);

  for (int model_var = 0; model_var < lp_solution.variable_values.size();
       ++model_var) {
    relaxation_values.push_back(lp_solution.variable_values[model_var]);
  }
  return relaxation_values;
}

std::vector<double> GetGeneralRelaxationValues(const Model* model) {
  std::vector<double> relaxation_values;

  auto* relaxation_solutions = model->Get<SharedRelaxationSolutionRepository>();

  if (relaxation_solutions == nullptr ||
      relaxation_solutions->NumSolutions() == 0) {
    return relaxation_values;
  }
  const SharedSolutionRepository<int64>::Solution relaxation_solution =
      relaxation_solutions->GetSolution(0);

  for (int model_var = 0;
       model_var < relaxation_solution.variable_values.size(); ++model_var) {
    relaxation_values.push_back(relaxation_solution.variable_values[model_var]);
  }
  return relaxation_values;
}
}  // namespace

RINSNeighborhood GetRINSNeighborhood(const Model* model,
                                     const bool use_lp_relaxation,
                                     const bool use_only_relaxation_values) {
  RINSNeighborhood rins_neighborhood;
  if (use_only_relaxation_values && !use_lp_relaxation) {
    // As of now RENS doesn't generate good neighborhoods from integer
    // relaxation solutions.
    return rins_neighborhood;
  }

  std::vector<double> relaxation_values;
  if (use_lp_relaxation) {
    relaxation_values = GetLPRelaxationValues(model);
  } else {
    relaxation_values = GetGeneralRelaxationValues(model);
  }
  if (relaxation_values.empty()) return rins_neighborhood;

  auto* response_manager = model->Get<SharedResponseManager>();
  if (!use_only_relaxation_values &&
      (response_manager == nullptr ||
       response_manager->SolutionsRepository().NumSolutions() == 0)) {
    return rins_neighborhood;
  }

  const double tolerance = 1e-6;
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
      const int64 domain_lb =
          static_cast<int64>(std::floor(relaxation_value + tolerance));
      const int64 domain_ub =
          static_cast<int64>(std::ceil(relaxation_value - tolerance));
      if (domain_lb == domain_ub) {
        rins_neighborhood.fixed_vars.push_back({model_var, domain_lb});
      } else {
        rins_neighborhood.reduced_domain_vars.push_back(
            {model_var, {domain_lb, domain_ub}});
      }

    } else {
      const SharedSolutionRepository<int64>::Solution solution =
          response_manager->SolutionsRepository().GetSolution(0);

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
