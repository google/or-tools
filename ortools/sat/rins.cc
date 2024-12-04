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

#include "ortools/sat/rins.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {

void RecordLPRelaxationValues(Model* model) {
  auto* lp_solutions = model->Mutable<SharedLPSolutionRepository>();
  if (lp_solutions == nullptr) return;

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  auto* lp_values = model->GetOrCreate<ModelLpValues>();

  // TODO(user): The default of ::infinity() for variable for which we do not
  // have any LP solution is weird and inconsistent with ModelLpValues default
  // which is zero. Fix. Note that in practice, at linearization level 2, all
  // variable will eventually have an lp relaxation value, so it shoulnd't
  // matter much to just use zero in RINS/RENS.
  std::vector<double> relaxation_values(
      mapping->NumProtoVariables(), std::numeric_limits<double>::infinity());

  // We only loop over the positive variables.
  const int size = lp_values->size();
  for (IntegerVariable var(0); var < size; var += 2) {
    const int proto_var = mapping->GetProtoVariableFromIntegerVariable(var);
    if (proto_var != -1) {
      relaxation_values[proto_var] = (*lp_values)[var];
    }
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

  const SharedSolutionRepository<double>::Solution lp_solution =
      lp_solutions->GetRandomBiasedSolution(random);

  for (int model_var = 0; model_var < lp_solution.variable_values.size();
       ++model_var) {
    relaxation_values.push_back(lp_solution.variable_values[model_var]);
  }
  return relaxation_values;
}

std::vector<double> GetIncompleteSolutionValues(
    SharedIncompleteSolutionManager* incomplete_solutions) {
  std::vector<double> empty_solution_values;

  if (incomplete_solutions == nullptr || !incomplete_solutions->HasSolution()) {
    return empty_solution_values;
  }

  return incomplete_solutions->PopLast();
}

static double kEpsilon = 1e-7;

struct VarWeight {
  int model_var;
  // Variables with minimum weight will be fixed in the neighborhood.
  double weight;

  // Comparator with tolerance and random tie breaking.
  bool operator<(const VarWeight& o) const { return weight < o.weight; }
};

void FillRinsNeighborhood(absl::Span<const int64_t> solution,
                          absl::Span<const double> relaxation_values,
                          double difficulty, absl::BitGenRef random,
                          ReducedDomainNeighborhood& reduced_domains) {
  std::vector<VarWeight> var_lp_gap_pairs;
  for (int model_var = 0; model_var < relaxation_values.size(); ++model_var) {
    const double relaxation_value = relaxation_values[model_var];
    if (relaxation_value == std::numeric_limits<double>::infinity()) continue;

    const int64_t best_solution_value = solution[model_var];
    const double pertubation = absl::Uniform(random, -kEpsilon, kEpsilon);
    var_lp_gap_pairs.push_back({
        model_var,
        std::abs(relaxation_value - static_cast<double>(best_solution_value)) +
            pertubation,
    });
  }
  std::sort(var_lp_gap_pairs.begin(), var_lp_gap_pairs.end());

  const int target_size = std::min(
      static_cast<int>(std::round(
          static_cast<double>(relaxation_values.size()) * (1.0 - difficulty))),
      static_cast<int>(var_lp_gap_pairs.size()));
  for (int i = 0; i < target_size; ++i) {
    const int model_var = var_lp_gap_pairs[i].model_var;
    reduced_domains.fixed_vars.push_back({model_var, solution[model_var]});
  }
}

void FillRensNeighborhood(absl::Span<const double> relaxation_values,
                          double difficulty, absl::BitGenRef random,
                          ReducedDomainNeighborhood& reduced_domains) {
  std::vector<VarWeight> var_fractionality_pairs;
  for (int model_var = 0; model_var < relaxation_values.size(); ++model_var) {
    const double relaxation_value = relaxation_values[model_var];
    if (relaxation_value == std::numeric_limits<double>::infinity()) continue;

    const double pertubation = absl::Uniform(random, -kEpsilon, kEpsilon);
    var_fractionality_pairs.push_back(
        {model_var, std::abs(std::round(relaxation_value) - relaxation_value) +
                        pertubation});
  }
  std::sort(var_fractionality_pairs.begin(), var_fractionality_pairs.end());
  const int target_size = static_cast<int>(std::round(
      static_cast<double>(relaxation_values.size()) * (1.0 - difficulty)));
  for (int i = 0; i < var_fractionality_pairs.size(); ++i) {
    const int model_var = var_fractionality_pairs[i].model_var;
    const double relaxation_value = relaxation_values[model_var];
    if (i < target_size) {
      // Fix the variable.
      reduced_domains.fixed_vars.push_back(
          {model_var, static_cast<int64_t>(std::round(relaxation_value))});
    } else {
      // Important: the LP relaxation doesn't know about holes in the variable
      // domains, so the intersection of [domain_lb, domain_ub] with the
      // initial variable domain might be empty.
      const int64_t domain_lb =
          static_cast<int64_t>(std::floor(relaxation_value));
      // TODO(user): Use the domain here.
      reduced_domains.reduced_domain_vars.push_back(
          {model_var, {domain_lb, domain_lb + 1}});
    }
  }
}

}  // namespace

ReducedDomainNeighborhood GetRinsRensNeighborhood(
    const SharedResponseManager* response_manager,
    const SharedLPSolutionRepository* lp_solutions,
    SharedIncompleteSolutionManager* incomplete_solutions, double difficulty,
    absl::BitGenRef random) {
  ReducedDomainNeighborhood reduced_domains;
  CHECK(lp_solutions != nullptr);
  CHECK(incomplete_solutions != nullptr);
  const bool lp_solution_available = lp_solutions->NumSolutions() > 0;
  const bool incomplete_solution_available =
      incomplete_solutions->HasSolution();

  if (!lp_solution_available && !incomplete_solution_available) {
    return reduced_domains;  // Not generated.
  }

  // Using a partial LP relaxation computed by feasibility_pump, and a full lp
  // relaxation periodically dumped by linearization=2 workers is equiprobable.
  std::bernoulli_distribution random_bool(0.5);

  const bool use_lp_relaxation =
      lp_solution_available && incomplete_solution_available
          ? random_bool(random)
          : lp_solution_available;

  const std::vector<double> relaxation_values =
      use_lp_relaxation ? GetLPRelaxationValues(lp_solutions, random)
                        : GetIncompleteSolutionValues(incomplete_solutions);
  if (relaxation_values.empty()) return reduced_domains;  // Not generated.

  std::bernoulli_distribution three_out_of_four(0.75);

  if (response_manager != nullptr &&
      response_manager->SolutionsRepository().NumSolutions() > 0 &&
      three_out_of_four(random)) {  // Rins.
    const std::vector<int64_t> solution =
        response_manager->SolutionsRepository()
            .GetRandomBiasedSolution(random)
            .variable_values;
    FillRinsNeighborhood(solution, relaxation_values, difficulty, random,
                         reduced_domains);
    reduced_domains.source_info = "rins_";
  } else {  // Rens.
    FillRensNeighborhood(relaxation_values, difficulty, random,
                         reduced_domains);
    reduced_domains.source_info = "rens_";
  }

  absl::StrAppend(&reduced_domains.source_info,
                  use_lp_relaxation ? "lp" : "pump", "_lns");
  return reduced_domains;
}

}  // namespace sat
}  // namespace operations_research
