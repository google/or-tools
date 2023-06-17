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

#ifndef OR_TOOLS_ALGORITHMS_WEIGHTED_SET_COVERING_H_
#define OR_TOOLS_ALGORITHMS_WEIGHTED_SET_COVERING_H_

#include <limits>
#include <vector>

#include "ortools/algorithms/weighted_set_covering_model.h"
#include "ortools/base/adjustable_priority_queue.h"

// Solver class for the weighted set covering problem.
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
// A guided local search (GLS) metaheuristic is also provided.
//
// TODO(user): add Large Neighborhood Search that removes a collection of T_j
// (with a parameterized way to choose them), and that runs the algorithm here
// on the corresponding subproblem.
//
// TODO(user): make Large Neighborhood Search concurrent, solving independent
// subproblems in different threads.
//
// The solution procedure is based on the general scheme known as local search.
// Once a solution exists, it is improved by modifying it slightly, for example
// by flipping a binary variable, so as to minimize the cost.
// But first, we have to generate a first solution that is as good as possible.
//
// An obvious idea is to take all the T_j's (or equivalently to set all the
// x_j's to 1). It's a bit silly but fast, and we can improve on it later.
//   set_covering.UseEverything();
//
// A better idea is to use Chvatal's heuristic as described in the
// abovementioned paper: Choose the subset that covers as many remaining
// uncovered elements as possible for the least possible cost and iterate.
//   set_covering.GenerateGreedySolution();
//
// Now that we have a first solution to the problem, there may be (most often,
// there are) elements in S that are covered several times. To decrease the
// total cost, Steepest tries to eliminate some redundant T_j's from the
// solution or equivalently, to flip some x_j's from 1 to 0.
//   set_covering.Steepest(num_steepest_iterations);
//
// As usual and well-known with local search, Steepest reaches a local minimum.
// We therefore implement Guided Tabu Search, which is a crossover of
// Guided Local Search and Tabu Search.
// Guided Local Search penalizes the parts of the solution that have been often
// used, while Tabu Search makes it possible to degrade the solution temporarily
// by disallowing to go back for a certain time (changes are put in a "Tabu"
// list).
//
// C. Voudouris (1997) "Guided local search for combinatorial optimisation
// problems", PhD Thesis, University of Essex, Colchester, UK, July, 1997.
// F. Glover (1989) "Tabu Search – Part 1". ORSA Journal on Computing.
// 1 (2):190–206. doi:10.1287/ijoc.1.3.190.
// F. Glover (1990) "Tabu Search – Part 2". ORSA Journal on Computing.
// 2 (1): 4–32. doi:10.1287/ijoc.2.1.4.
// To use Guided Tabu Search, use:
//   set_covering.GuidedTabuSearch(num_guided_tabu_search_iterations);
//
// G. Cormode, H. Karloff, A.Wirth (2010) "Set Cover Algorithms for Very Large
// Datasets", CIKM'10, ACM, 2010.
// TODO(user): Add documentation for the parameters that can be set.
namespace operations_research {
// Representation of a solution, with the values of the variables, along its
// the total cost.

using ChoiceVector = glop::StrictITIVector<SubsetIndex, bool>;

class WeightedSetCoveringSolution {
 public:
  WeightedSetCoveringSolution() : cost_(0), choices_() {}
  WeightedSetCoveringSolution(Cost cost, ChoiceVector assignment)
      : cost_(cost), choices_(assignment) {}

  void StoreCostAndSolution(Cost cost, ChoiceVector assignment) {
    cost_ = cost;
    choices_ = assignment;
  }

  // Returns the cost of the solution.
  Cost cost() const { return cost_; }
  void SetCost(Cost cost) { cost_ = cost; }
  void AddToCost(Cost value) { cost_ += value; }
  void SubtractFromCost(Cost value) { cost_ -= value; }

  bool IsSet(SubsetIndex subset) const { return choices_[subset]; }
  void Set(SubsetIndex subset, bool value) { choices_[subset] = value; }

  // Returns the assignment of the solution.
  ChoiceVector choices() const { return choices_; }

  // Returns assignment as a "usable", less strongly typed vector of bool.
  // It's easier to use by the calling code.
  std::vector<bool> ChoicesAsVectorOfBooleans() const {
    std::vector<bool> assignment;
    for (int i = 0; i < choices_.size(); ++i) {
      assignment.push_back(choices_[SubsetIndex(i)]);
    }
    return assignment;
  }

 private:
  Cost cost_;
  ChoiceVector choices_;
};

template <typename T>
class TabuList {
 public:
  explicit TabuList(T size) : array_(0), fill_(0), index_(0) {
    array_.resize(size.value(), T(-1));
  }
  int size() const { return array_.size(); }
  void Init(int size) {
    array_.resize(size, T(-1));
    fill_ = 0;
    index_ = 0;
  }
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

// Element used for AdjustablePriorityQueue.
class SubsetPriority {
 public:
  SubsetPriority()
      : heap_index_(-1),
        subset_(0),
        priority_(std::numeric_limits<Cost>::infinity()) {}
  SubsetPriority(int h, SubsetIndex s, Cost cost)
      : heap_index_(h), subset_(s), priority_(cost) {}
  void SetHeapIndex(int h) { heap_index_ = h; }
  int GetHeapIndex() const { return heap_index_; }
  // The priority queue maintains the max element. This comparator breaks ties
  // between subsets using their cardinalities.
  bool operator<(const SubsetPriority& other) const {
    return priority_ < other.priority_ ||
           (priority_ == other.priority_ && subset_ < other.subset_);
  }
  SubsetIndex GetSubset() const { return subset_; }
  void SetPriority(Cost priority) { priority_ = priority; }
  Cost GetPriority() const { return priority_; }

 private:
  int heap_index_;
  SubsetIndex subset_;
  Cost priority_;
};

using SubsetCountVector = glop::StrictITIVector<SubsetIndex, int>;
using SubsetBoolVector = glop::StrictITIVector<SubsetIndex, bool>;
using SubsetPriorityVector = glop::StrictITIVector<SubsetIndex, SubsetPriority>;

class WeightedSetCoveringSolver {
 public:
  // Constructs an empty weighted set covering problem.
  explicit WeightedSetCoveringSolver(const WeightedSetCoveringModel& model)
      : model_(model),
        lagrangian_factor_(kDefaultLagrangianFactor),
        penalty_factor_(kDefaultPenaltyFactor),
        radius_factor_(kDefaultRadiusFactor),
        tabu_list_(SubsetIndex(kDefaultTabuListSize)) {}

  // Setters and getters for the Guided Tabu Search algorithm parameters.
  void SetPenaltyFactor(double factor) { penalty_factor_ = factor; }
  double GetPenaltyFactor() const { return penalty_factor_; }

  // TODO(user): re-introduce this is the code. It was used to favor
  // subsets with the same marginal costs but that would cover more elements.
  // But first, see if it makes sense to compute it.
  void SetLagrangianFactor(double factor) { lagrangian_factor_ = factor; }
  double GetLagrangianFactor() const { return lagrangian_factor_; }

  void SetRadiusFactor(double r) { radius_factor_ = r; }
  double GetRadius() const { return radius_factor_; }

  void SetTabuListSize(int size) { tabu_list_.Init(size); }
  int GetTabuListSize() const { return tabu_list_.size(); }

  // Initializes the solver once the data is set. The model cannot be changed
  // afterwards. Only lagrangian_factor_, radius_factor_ and tabu_list_size_
  // can be modified.
  void Initialize();

  // Generates a trivial solution using all the subsets.
  void GenerateTrivialSolution();

  // Generates a solution using a greedy algorithm.
  void GenerateGreedySolution();

  // Returns true if the elements selected in the current solution cover all

  // the elements of the set.
  bool CheckSolution() const;

  // Runs a steepest local search.
  void Steepest(int num_iterations);

  // Runs num_iterations of guided tabu search.
  // TODO(user): Describe what a Guided Tabu Search is.
  void GuidedTabuSearch(int num_iterations);

  // Resets the guided tabu search by clearing the penalties and the tabu list.
  // TODO(user): review whether this is public or not.
  void ResetGuidedTabuSearch();

  // Stores the solution. TODO(user): review whether this is public or not.
  void StoreSolution();

  // Restores the solution. TODO(user): review whether this is public or not.
  void RestoreSolution();

  // Returns the current solution.
  WeightedSetCoveringSolution GetSolution() const { return solution_; }

  // Returns the best solution.
  WeightedSetCoveringSolution GetBestSolution() const { return best_solution_; }

 private:
  // Computes the number of elements in subset that are already covered in the
  // current solution.
  ElementIndex ComputeNumElementsAlreadyCovered(SubsetIndex subset) const;

  // Returns a Boolean with a .5 probability. TODO(user): remove from the class.
  bool FlipCoin() const;
  // TODO(user): remove this.
  void ToggleChoice(SubsetIndex subset, bool value);

  std::vector<SubsetIndex> Toggle(SubsetIndex subset, bool value);

  // Returns the number of elements currently covered by subset.
  ElementToSubsetVector ComputeSingleSubsetCoverage(SubsetIndex subset) const;

  // TODO(user): add a specific section in the class.
  // Checks that the value of coverage_ is correct by recomputing and comparing.
  bool CheckSingleSubsetCoverage(SubsetIndex subset) const;

  // Returns a vector containing the number of subsets covering each element.
  ElementToSubsetVector ComputeCoverage(const ChoiceVector& choices) const;

  // Update coverage_ for subset when setting choices_[subset] to value.
  void UpdateCoverage(SubsetIndex subset, bool value);

  // Returns the subsets that share at least one element with subset.
  // TODO(user): use union-find to make it faster?
  std::vector<SubsetIndex> ComputeImpactedSubsets(SubsetIndex subset) const;

  // Updates is_removable_ for each subset in impacted_subsets.
  void UpdateIsRemovable(const std::vector<SubsetIndex>& impacted_subsets);

  // Updates marginal_impacts_ for each subset in impacted_subsets.
  void UpdateMarginalImpacts(const std::vector<SubsetIndex>& impacted_subsets);

  // Checks that coverage_ is consistent with choices.
  bool CheckCoverage(const ChoiceVector& choices) const;

  // Checks that coverage_  and marginal_impacts_ are consistent with  choices.
  bool CheckCoverageAndMarginalImpacts(const ChoiceVector& choices) const;

  // Computes marginal impacts based on a coverage cvrg.
  SubsetToElementVector ComputeMarginalImpacts(
      const ElementToSubsetVector& cvrg) const;

  // Updates the priorities for the greedy first solution algorithm.
  void UpdateGreedyPriorities(const std::vector<SubsetIndex>& impacted_subsets);

  // Updates the priorities for the steepest local search algorithm.
  void UpdateSteepestPriorities(
      const std::vector<SubsetIndex>& impacted_subsets);

  // Returns true if subset can be removed from the solution, i.e. it is
  // redundant to cover all the elements.
  // This function is used to check that _is_removable_[subset] is consistent.
  bool CanBeRemoved(SubsetIndex subset) const;

  // Updates the penalties for Guided Local Search.
  void UpdatePenalties();

  // The weighted set covering model on which the solver is run.
  WeightedSetCoveringModel model_;

  // The best solution found so far for the problem.
  WeightedSetCoveringSolution best_solution_;

  // The current solution.
  WeightedSetCoveringSolution solution_;

  // Current cost_.
  Cost cost_;

  // Current assignment.
  ChoiceVector choices_;

  // Priorities for the different subsets.
  // They are similar to the marginal cost of using a particular subset,
  // defined as the cost of the subset divided by the number of elements that
  // are still not covered.
  SubsetCostVector gts_priorities_;

  // Priority queue.
  AdjustablePriorityQueue<SubsetPriority> pq_;

  // Elements in the priority queue.
  SubsetPriorityVector pq_elements_;

  SubsetToElementVector marginal_impacts_;

  // The coverage of an element is the number of used subsets which contains
  // the said element.
  ElementToSubsetVector coverage_;

  // True if the subset can be removed from the solution without making it
  // infeasible.
  SubsetBoolVector is_removable_;

  // Search handling variables and default parameters.
  static constexpr double kDefaultLagrangianFactor = 100.0;
  double lagrangian_factor_;

  // Guided local search-related data.
  static constexpr double kPenaltyUpdateEpsilon = 1e-1;

  static constexpr double kDefaultPenaltyFactor = 0.2;
  double penalty_factor_;

  static constexpr double kDefaultRadiusFactor = 1e-8;
  double radius_factor_;

  // Penalized costs for each subset as used in Guided Tabu Search.
  SubsetCostVector penalized_costs_;
  // The number of times each subset was penalized during Guided Tabu Search.
  SubsetCountVector times_penalized_;

  // Tabu search-related data.
  static constexpr int kDefaultTabuListSize = 17;  // Nice prime number.
  TabuList<SubsetIndex> tabu_list_;
};

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_WEIGHTED_SET_COVERING_H_
