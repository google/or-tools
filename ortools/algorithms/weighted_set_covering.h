// Copyright 2010-2022 Google LLC
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

// Minimum cost set covering.
//
// Let S be a set, let (T_j) be a family (j in J) of subsets of S, and c_j costs
// associated to each T_j.
//
// The minimum-cost set-covering problem consists in finding K, a sub-set of J
// such that the union of all the T_k, k in K = S, with the minimal total cost
// sum c_k (k in K).
//
// TODO(user): Explain the set covering problem in matrix terms.
//
// The first solution is obtained using the Chvatal heuristic, that guarantees
// that the solution is at most 1 + log(n) times the optimal value.
// Vasek Chvatal, 1979. A greedy heuristic for the set-covering problem.
// Mathematics of Operations Research, 4(3):233-235, 1979.
// www.jstor.org/stable/3689577
//
// The idea is basically to compute the cost per element for a T_j to cover
// them, and to start by the ones with the best such amortized costs.
//
// The following paper dives into the details this class of algorithms.
// Neal E. Young, Greedy Set-Cover Algorithms (1974-1979, Chvatal, Johnson,
// Lovasz, Stein), in Kao, ed., Encyclopedia of Algorithms.
// Draft at: http://www.cs.ucr.edu/~neal/non_arxiv/Young08SetCover.pdf
//
// The first solution is then improved by using local search descent, which
// eliminates the T_j's that have no interest in the solution.
//
// Also implemented is a guided local search metaheuristic. TODO(user): Add a
// reference to guided local search.
//
// TODO(user): use "ortools/base/adjustable_priority_queue.h" to make the
// computation of the cost per element and the sorting of the columns more
// efficient.
//
// TODO(user): add Large Neighborhood Search that removes a collection of T_j
// (with a parameterized way to choose them), and that runs the algorithm here
// on the corresponding subproblem.
//
// TODO(user): represent the problem as its transpose, in order to find the
// elements whose coverage is the largest, to help choose the T_j that may
// be removed from the solution and re-optimized.
//
// TODO(user): make Large Neighborhood Search concurrent, solving independent
// subproblems in different threads.
//
// TODO(user): be able to choose the collection of T_j from the elements they
// contain so as to focus the search.

#ifndef OR_TOOLS_ALGORITHMS_WEIGHTED_SET_COVERING_H_
#define OR_TOOLS_ALGORITHMS_WEIGHTED_SET_COVERING_H_

#include <sys/types.h>

#include <limits>

#include "ortools/lp_data/lp_types.h"  // For StrictITIVector.

namespace operations_research {

typedef double Cost;

DEFINE_STRONG_INDEX_TYPE(ElementIndex);
DEFINE_STRONG_INDEX_TYPE(SubsetIndex);
DEFINE_STRONG_INDEX_TYPE(EntryIndex);

const SubsetIndex kNotFound(-1);
const Cost kMaxCost = std::numeric_limits<Cost>::infinity();
// TODO(user): This should be a parameter.
const Cost kPenaltyUpdateEpsilon = 1e-1;

// TODO(user): consider replacing with absl equivalent, which are less strict,
// unfortunately: the return type for size() is a simple size_t with absl, and
// not the Index as in StrictITIVector, which makes the code less elegant.
using SubsetCostVector = glop::StrictITIVector<SubsetIndex, Cost>;
using SparseColumn = glop::StrictITIVector<EntryIndex, ElementIndex>;
using SparseRow = glop::StrictITIVector<EntryIndex, SubsetIndex>;

template <typename T>
class TabuList {
 public:
  explicit TabuList(int size) : array_(size, T(-1)), fill_(0), index_(0) {}
  void Add(T t) {
    const int size = array_.size();
    array_[index_] = t;
    ++index_;
    if (index_ >= size) {
      index_ = 0;
    }
    if (fill_ < size) {
      ++fill_;
    }
  }
  bool Contains(T t) const {
    for (int i = 0; i < fill_; ++i) {
      if (t == array_[i]) {
        return true;
      }
    }
    return false;
  }

 private:
  std::vector<T> array_;
  int fill_;
  int index_;
};

// TODO(user): Expand documentation here.
// The solving procedure looks like:
// set_covering.GenerateGreedySolution();
// set_covering.Steepest(num_steepest_iterations);
// set_covering.GuidedTabuSearch(num_guided_tabu_search_iterations);

// TODO(user): Explain set covering in matrix terms.

class WeightedSetCovering {
 public:
  // Constructs an empty weighted set covering problem.
  // TODO(user): find a meaningful way to set the Tabu list size, if ever
  // it is useful.
  WeightedSetCovering() : iteration_(0), tabu_list_(17) {}

  WeightedSetCovering(const WeightedSetCovering&) = default;
  WeightedSetCovering(WeightedSetCovering&&) = default;
  WeightedSetCovering& operator=(const WeightedSetCovering&) = default;
  WeightedSetCovering& operator=(WeightedSetCovering&&) = default;
  // Adds an empty subset with a cost to the problem. In matrix terms, this
  // adds a column to the matrix.
  void AddEmptySubset(Cost cost);

  // Adds an element to the last subset created. In matrix terms, this adds a
  // 1 on row 'element' of the current last column of the matrix.
  void AddElementToLastSubset(int element);

  void SetCostOfSubset(Cost cost, int subset);

  void AddElementToSubset(int element, int subset);

  // Initializes the solver once the data is set.
  void Init();

  // Generates a solution using a greedy algorithm.
  void GenerateGreedySolution();
  void UseEverything();

  // Returns true if the current solution is indeed a solution, i.e. all the
  // elements of the set are covered by the selected subsets.
  bool CheckSolution() const;

  // Returns true if the problem is feasible. The problem is trivially
  // infeasible if one element does not appear in any of the subsets.
  bool CheckFeasibility() const;

  // Runs a steepest local search.
  void Steepest(int num_iterations);

  // Runs a guided tabu search.
  // TODO(user): Describe what a Guided Tabu Search is.
  void GuidedTabuSearch(int num_iterations);

  // Resets the guided tabu search by clearing the penalties and the tabu list.
  void ResetGuidedTabuSearch();

  // Stores the solution.
  void StoreSolution();

  // Restores the solution.
  void RestoreSolution();

  // TODO(user) P0: add accessors to current and best costs, and assignments.

 private:
  ElementIndex ComputeNumElementsCovered(SubsetIndex subset) const;
  bool Probability() const;
  void Flip(SubsetIndex subset);
  bool CanBeRemoved(SubsetIndex subset) const;
  void UpdatePenalties();

  ElementIndex num_elements_;

  // Input data.
  SubsetCostVector subset_cost_;

  // Input data.
  glop::StrictITIVector<SubsetIndex, SparseColumn> columns_;

  glop::StrictITIVector<ElementIndex, SparseRow> rows_;

  // Reduced costs.
  glop::StrictITIVector<SubsetIndex, Cost> reduced_cost_;

  // Current assignment.
  Cost cost_;
  glop::StrictITIVector<SubsetIndex, bool> assignment_;

  // Best solution.
  Cost best_cost_;
  glop::StrictITIVector<SubsetIndex, bool> best_assignment_;

  // Search handling data.
  int iteration_;
  glop::StrictITIVector<ElementIndex, SubsetIndex>
      num_subsets_covering_element_;

  double lagrangian_factor_;

  // Guided local search-related data.
  double penalty_factor_;
  SubsetCostVector penalized_cost_;
  SubsetCostVector priority_;
  glop::StrictITIVector<SubsetIndex, int> times_penalized_;
  // Tabu-search related data.
  TabuList<SubsetIndex> tabu_list_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_WEIGHTED_SET_COVERING_H_
