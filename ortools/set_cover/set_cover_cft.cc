// Copyright 2025 Francesco Cavaliere
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

#include "ortools/set_cover/set_cover_cft.h"

#include <absl/algorithm/container.h>
#include <absl/random/random.h>
#include <absl/status/status.h>
#include <absl/types/span.h>

#include "ortools/base/accurate_sum.h"
#include "ortools/base/status_macros.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/util/fp_utils.h"

namespace operations_research::scp {

////////////////////////////////////////////////////////////////////////
////////////////////////// COMMON DEFINITIONS //////////////////////////
////////////////////////////////////////////////////////////////////////
namespace {
class CoverCounters {
 public:
  CoverCounters(BaseInt nelems = 0) : cov_counters(nelems, 0) {}
  void Reset(BaseInt nelems) { cov_counters.assign(nelems, 0); }
  BaseInt Size() const { return cov_counters.size(); }
  BaseInt operator[](ElementIndex i) const {
    assert(i < cov_counters.size());
    return cov_counters[i];
  }

  template <typename IterableT>
  BaseInt Cover(IterableT const& subset) {
    BaseInt covered = 0;
    for (ElementIndex i : subset) {
      covered += cov_counters[i] == 0 ? 1ULL : 0ULL;
      cov_counters[i]++;
    }
    return covered;
  }

  template <typename IterableT>
  BaseInt Uncover(IterableT const& subset) {
    BaseInt uncovered = 0;
    for (ElementIndex i : subset) {
      --cov_counters[i];
      uncovered += cov_counters[i] == 0 ? 1ULL : 0ULL;
    }
    return uncovered;
  }

  // Check if all the elements of a subset are already covered.
  template <typename IterableT>
  bool IsRedundantCover(IterableT const& subset) const {
    return absl::c_all_of(subset,
                          [&](ElementIndex i) { return cov_counters[i] > 0; });
  }

  // Check if all the elements would still be covered if the subset was removed.
  template <typename IterableT>
  bool IsRedundantUncover(IterableT const& subset) const {
    return absl::c_all_of(subset,
                          [&](ElementIndex i) { return cov_counters[i] > 1; });
  }

 private:
  ElementToIntVector cov_counters;
};

}  // namespace

absl::Status ValidateModel(const Model& model) {
  if (model.rows().size() != model.num_elements()) {
    return absl::InvalidArgumentError("Model has no rows.");
  }
  if (model.columns().size() != model.num_subsets()) {
    return absl::InvalidArgumentError("Model has no columns.");
  }
  if (model.subset_costs().size() != model.num_subsets()) {
    return absl::InvalidArgumentError("Model has no subset costs.");
  }
  if (model.num_elements() <= 0) {
    return absl::InvalidArgumentError("Model has no elements.");
  }
  if (model.num_subsets() <= 0) {
    return absl::InvalidArgumentError("Model has no subsets.");
  }
  if (!model.ComputeFeasibility()) {
    return absl::InvalidArgumentError("Model is infeasible.");
  }
  return absl::OkStatus();
}

absl::Status ValidateFeasibleSolution(const Model& model,
                                      const Solution& solution,
                                      Cost tolerance) {
  AccurateSum<Cost> solution_cost;
  CoverCounters coverage(model.num_elements());
  const SparseColumnView& columns = model.columns();
  const SubsetCostVector& subset_costs = model.subset_costs();
  for (SubsetIndex subset : solution.subsets()) {
    solution_cost.Add(subset_costs[subset]);
    coverage.Cover(columns[subset]);
  }
  if (!AreWithinAbsoluteOrRelativeTolerances(
          solution_cost.Value(), solution.cost(), tolerance, tolerance))
    return absl::InvalidArgumentError("Solution cost is incorrect.");
  for (ElementIndex element : model.ElementRange()) {
    if (coverage[element] == 0) {
      return absl::InvalidArgumentError("Solution is infeasible.");
    }
  }
  return absl::OkStatus();
}

///////////////////////////////////////////////////////////////////////
//////////////////////// THREE PHASE ALGORITHM ////////////////////////
///////////////////////////////////////////////////////////////////////
namespace {

void FixColumns(CoreModel& model, const std::vector<SubsetIndex>& cols_to_fix,
                ElementMapVector& new_to_old_map);
void FixBestColumns(CoreModel& model, PrimalDualState& state);
void RandomizeDualState(const Model& model, DualState& dual_state,
                        absl::BitGen& rnd);
}  // namespace

absl::StatusOr<PrimalDualState> RunThreePhase(CoreModel& model,
                                              const Solution& init_solution) {
  model.CreateSparseRowView();
  RETURN_IF_ERROR(ValidateModel(model));

  PrimalDualState best_state = {.solution = init_solution,
                                .dual_state = DualState(model)};
  if (best_state.solution.Empty()) {
    best_state.solution =
        RunMultiplierBasedGreedy(model, best_state.dual_state);
  }
  RETURN_IF_ERROR(ValidateFeasibleSolution(model, best_state.solution));
  std::cout << "Initial lower bound: " << best_state.dual_state.lower_bound()
            << "\nInitial solution cost: " << best_state.solution.cost()
            << "\nStarting 3-phase algorithm\n";

  PrimalDualState curr_state = best_state;
  BoundCBs dual_bound_cbs;
  HeuristicCBs heuristic_cbs;
  BaseInt iter_count = 0;
  absl::BitGen rnd;
  while (model.num_elements() > 0) {
    ++iter_count;

    // Phase 1: refine the current dual_state and model
    SubgradientOptimization(model, dual_bound_cbs, curr_state);
    if (iter_count == 1) {
      best_state.dual_state = curr_state.dual_state;
    }
    // Phase 2: search for good solutions
    SubgradientOptimization(model, heuristic_cbs, curr_state);
    if (curr_state.solution.cost() < best_state.solution.cost()) {
      best_state = curr_state;
    }
    std::cout << "Iterartion " << iter_count
              << "\n - Lower bound: " << curr_state.dual_state.lower_bound()
              << "\n - Solution cost: " << curr_state.solution.cost() << '\n';

    // Phase 3: Fix the best columns (diving)
    FixBestColumns(model, curr_state);
    RandomizeDualState(model, curr_state.dual_state, rnd);
  }

  return best_state;
}

}  // namespace operations_research::scp