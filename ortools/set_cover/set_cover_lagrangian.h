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

#ifndef OR_TOOLS_SET_COVER_SET_COVER_LAGRANGIAN_H_
#define OR_TOOLS_SET_COVER_SET_COVER_LAGRANGIAN_H_

#include <memory>
#include <new>
#include <tuple>
#include <vector>

#include "ortools/base/threadpool.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research {

// The SetCoverLagrangian class implements the Lagrangian relaxation of the set
// cover problem.

// In the following, we refer to the following articles:
// [1] Caprara, Alberto, Matteo Fischetti, and Paolo Toth. 1999. “A Heuristic
// Method for the Set Covering Problem.” Operations Research 47 (5): 730–43.
// https://www.jstor.org/stable/223097
// [2] Fisher, Marshall L. 1981. “The Lagrangian Relaxation Method for Solving
// Integer Programming Problems.” Management Science 27 (1): 1–18.
// https://www.jstor.org/stable/2631139
// [3] Held, M., Karp, R.M. The traveling-salesman problem and minimum spanning
// trees: Part II. Mathematical Programming 1, 6–25 (1971).
// https://link.springer.com/article/10.1007/BF01584070
// [4] Williamson, David P. 2002. “The Primal-Dual Method for Approximation
// Algorithms.” Mathematical Programming, 91 (3): 447–78.
// https://link.springer.com/article/10.1007/s101070100262

class SetCoverLagrangian {
 public:
  explicit SetCoverLagrangian(SetCoverInvariant* inv, int num_threads = 1)
      : inv_(inv),
        model_(*inv->model()),
        num_threads_(num_threads),
        thread_pool_(new ThreadPool(num_threads)) {
    thread_pool_->StartWorkers();
  }

  // Returns true if a solution was found.
  // TODO(user): Add time-outs and exit with a partial solution. This seems
  // unlikely, though.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(const std::vector<SubsetIndex>& focus);

  // Initializes the multipliers vector (u) based on the cost per subset.
  ElementCostVector InitializeLagrangeMultipliers() const;

  // Computes the Lagrangian (row-)cost vector.
  // For a subset j, c_j(u) = c_j - sum_{i \in I_j} u_i.
  // I_j denotes the indices of elements present in subset j.
  SubsetCostVector ComputeReducedCosts(
      const SubsetCostVector& costs,
      const ElementCostVector& multipliers) const;

  // Same as above, but parallelized, using the number of threads specified in
  // the constructor.
  SubsetCostVector ParallelComputeReducedCosts(
      const SubsetCostVector& costs,
      const ElementCostVector& multipliers) const;

  // Computes the subgradient (column-)cost vector.
  // For all element indices i, s_i(u) = 1 - sum_{j \in J_i} x_j(u),
  // where J_i denotes the set of indices of subsets j covering element i.
  ElementCostVector ComputeSubgradient(
      const SubsetCostVector& reduced_costs) const;

  // Same as above, but parallelized, using the number of threads specified in
  // the constructor.
  ElementCostVector ParallelComputeSubgradient(
      const SubsetCostVector& reduced_costs) const;

  // Computes the value of the Lagrangian L(u).
  // L(u) = min \sum_{j \in N} c_j(u) x_j + \sum{i \in M} u_i.
  // If c_j(u) < 0: x_j(u) = 1, if c_j(u) > 0: x_j(u) = 0,
  // otherwise x_j(u) is unbound, it can take any value in {0, 1}.
  Cost ComputeLagrangianValue(const SubsetCostVector& reduced_costs,
                              const ElementCostVector& multipliers) const;

  // Same as above, but parallelized, using the number of threads specified in
  // the constructor.
  Cost ParallelComputeLagrangianValue(
      const SubsetCostVector& reduced_costs,
      const ElementCostVector& multipliers) const;

  // Updates the multipliers vector (u) with the given step size.
  // Following [3], each of the coordinates is defined as: u^{k+1}_i =
  //     max(u^k_i + lambda_k * (UB - L(u^k)) / |s^k(u)|^2 * s^k_i(u), 0).
  // lambda_k is step_size in the function signature below. UB is upper_bound.
  void UpdateMultipliers(double step_size, Cost lagrangian_value,
                         Cost upper_bound,
                         const SubsetCostVector& reduced_costs,
                         ElementCostVector* multipliers) const;

  // Same as above, but parallelized, using the number of threads specified in
  // the constructor.
  void ParallelUpdateMultipliers(double step_size, Cost lagrangian_value,
                                 Cost upper_bound,
                                 const SubsetCostVector& reduced_costs,
                                 ElementCostVector* multipliers) const;

  // Computes the gap between the current solution and the optimal solution.
  // This is the sum of the multipliers for the elements that are not covered
  // by the current solution.
  Cost ComputeGap(const SubsetCostVector& reduced_costs,
                  const SubsetBoolVector& solution,
                  const ElementCostVector& multipliers) const;

  // Performs the three-phase algorithm.
  void ThreePhase(Cost upper_bound);

  // Computes a lower bound on the optimal cost.
  // The returned value is the lower bound, the reduced costs, and the
  // multipliers.
  std::tuple<Cost, SubsetCostVector, ElementCostVector> ComputeLowerBound(
      const SubsetCostVector& costs, Cost upper_bound);

 private:
  // The invariant on which the algorithm will run.
  SetCoverInvariant* inv_;

  // The model on which the invariant is defined.
  const SetCoverModel& model_;

  // The number of threads to use for parallelization.
  int num_threads_;

  // The thread pool used for parallelization.
  std::unique_ptr<ThreadPool> thread_pool_;

  // Total (scalar) Lagrangian cost.
  Cost lagrangian_;

  // Lagrangian cost vector, per subset.
  SubsetCostVector lagrangians_;

  // Computes the delta vector.
  // This is definition (9) in [1].
  SubsetCostVector ComputeDelta(const SubsetCostVector& reduced_costs,
                                const ElementCostVector& multipliers) const;
};

}  // namespace operations_research

#endif  // OR_TOOLS_SET_COVER_SET_COVER_LAGRANGIAN_H_
