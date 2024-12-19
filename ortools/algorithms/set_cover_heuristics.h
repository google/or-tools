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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_HEURISTICS_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_HEURISTICS_H_

#include <vector>

#include "absl/types/span.h"
#include "ortools/algorithms/adjustable_k_ary_heap.h"
#include "ortools/algorithms/set_cover_invariant.h"
#include "ortools/algorithms/set_cover_model.h"

namespace operations_research {

// Solver classes for the weighted set covering problem.
//
// The solution procedure is based on the general scheme known as local search.
// Once a solution exists, it is improved by modifying it slightly, for example
// by flipping a binary variable, so as to minimize the cost.
// But first, we have to generate a first solution that is as good as possible.
//
// The first solution is then improved by using local search descent, which
// eliminates the S_j's that have no interest in the solution.
//
// A mix of the guided local search (GLS) and Tabu Search (TS) metaheuristic
// is also provided.
//
// The term 'focus' hereafter means a subset of the S_j's designated by their
// indices. Focus make it possible to run the algorithms on the corresponding
// subproblems.
//
// TODO(user): make the different algorithms concurrent, solving independent
// subproblems in different threads.
//

// An obvious idea is to take all the S_j's (or equivalently to set all the
// x_j's to 1). It's very silly but fast, and we can improve on it later using
// local search.

// The consistency level is maintained up to kFreeAndUncovered.
class TrivialSolutionGenerator {
 public:
  explicit TrivialSolutionGenerator(SetCoverInvariant* inv) : inv_(inv) {}

  // Returns true if a solution was found.
  // TODO(user): Add time-outs and exit with a partial solution. This seems
  // unlikely, though.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus);

 private:
  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;
};

// A slightly more complicated but better way to compute a first solution is to
// select columns randomly. Less silly than the previous one, and provides
// much better results.
// TODO(user): make it possible to use other random generators. Idea: bias the
// generator towards the columns with the least marginal costs.

// The consistency level is maintained up to kFreeAndUncovered.
class RandomSolutionGenerator {
 public:
  explicit RandomSolutionGenerator(SetCoverInvariant* inv) : inv_(inv) {}

  // Returns true if a solution was found.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(const std::vector<SubsetIndex>& focus);

 private:
  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;
};

// The first solution is obtained using the Chvatal heuristic, that guarantees
// that the solution is at most 1 + log(n) times the optimal value.
// Vasek Chvatal, 1979. A greedy heuristic for the set-covering problem.
// Mathematics of Operations Research, 4(3):233-235, 1979.
// http://www.jstor.org/stable/3689577
//
// Chvatal's heuristic works as follows: Choose the subset that covers as many
// remaining uncovered elements as possible for the least possible cost per
// element and iterate.
//
// The following papers dive into the details of this class of algorithms.
//
// Young, Neal E. 2008. “Greedy Set-Cover Algorithms.” In Encyclopedia of
// Algorithms, 379–81. Boston, MA: Springer US. Draft at:
// http://www.cs.ucr.edu/~neal/non_arxiv/Young08SetCover.pdf
//
// Cormode, Graham, Howard Karloff, and Anthony Wirth. 2010. “Set Cover
// Algorithms for Very Large Datasets.” In CIKM ’10. ACM Press.
// https://doi.org/10.1145/1871437.1871501.

// The consistency level is maintained up to kFreeAndUncovered.
class GreedySolutionGenerator {
 public:
  explicit GreedySolutionGenerator(SetCoverInvariant* inv) : inv_(inv) {}

  // Returns true if a solution was found.
  // TODO(user): Add time-outs and exit with a partial solution.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus);

  // Same with a different set of costs.
  bool NextSolution(absl::Span<const SubsetIndex> focus,
                    const SubsetCostVector& costs);

 private:
  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;
};

// Solution generator based on the degree of elements.
// The degree of an element is the number of subsets covering it.
// The generator consists in iteratively choosing a non-covered element with the
// smallest degree, and selecting a subset that covers it with the least
// ratio cost / number of uncovered elements. The number of uncovered elements
// are updated for each impacted subset. The newly-covered elements degree
// are also updated and set to zero.

// The consistency level is maintained up to kFreeAndUncovered.
class ElementDegreeSolutionGenerator {
 public:
  explicit ElementDegreeSolutionGenerator(SetCoverInvariant* inv) : inv_(inv) {}

  // Returns true if a solution was found.
  // TODO(user): Add time-outs and exit with a partial solution.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus);

  // Same with a different set of costs.
  bool NextSolution(absl::Span<const SubsetIndex> focus,
                    const SubsetCostVector& costs);

 private:
  // Same with a different set of costs, and the focus defined as a vector of
  // Booleans. This is the actual implementation of NextSolution.
  bool NextSolution(const SubsetBoolVector& in_focus,
                    const SubsetCostVector& costs);

  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;
};

// Solution generator based on the degree of elements.
// The heuristic is the same as ElementDegreeSolutionGenerator, but the number
// of uncovered elements for a subset is computed on-demand. In empirical
// tests, this seems to be faster than ElementDegreeSolutionGenerator because
// a very small percentage of need to be computed, and even fewer among them
// need to be computed again later on.

// Because the number of uncovered elements is computed on-demand, the
// consistency level only needs to be set to kCostAndCoverage.
class LazyElementDegreeSolutionGenerator {
 public:
  explicit LazyElementDegreeSolutionGenerator(SetCoverInvariant* inv)
      : inv_(inv) {}

  // Returns true if a solution was found.
  // TODO(user): Add time-outs and exit with a partial solution.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus);

  // Same with a different set of costs.
  bool NextSolution(absl::Span<const SubsetIndex> focus,
                    const SubsetCostVector& costs);

 private:
  // Same with a different set of costs, and the focus defined as a vector of
  // Booleans. This is the actual implementation of NextSolution.
  bool NextSolution(const SubsetBoolVector& in_focus,
                    const SubsetCostVector& costs);

  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;
};

// Once we have a first solution to the problem, there may be (most often,
// there are) elements in E that are covered several times. To decrease the
// total cost, SteepestSearch tries to eliminate some redundant S_j's from
// the solution or equivalently, to flip some x_j's from 1 to 0. the algorithm
// gets its name because it goes in the steepest immediate direction, taking
// the S_j with the largest total cost.

// The consistency level is maintained up to kFreeAndUncovered.
class SteepestSearch {
 public:
  explicit SteepestSearch(SetCoverInvariant* inv) : inv_(inv) {}

  // Returns true if a solution was found within num_iterations.
  // TODO(user): Add time-outs and exit with a partial solution.
  bool NextSolution(int num_iterations);

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus, int num_iterations);

  // Same as above, with a different set of costs.
  bool NextSolution(absl::Span<const SubsetIndex> focus,
                    const SubsetCostVector& costs, int num_iterations);

 private:
  // Same with a different set of costs, and the focus defined as a vector of
  // Booleans. This is the actual implementation of NextSolution.
  bool NextSolution(const SubsetBoolVector& in_focus,
                    const SubsetCostVector& costs, int num_iterations);

  // Updates the priorities on the impacted_subsets.
  void UpdatePriorities(absl::Span<const SubsetIndex> impacted_subsets);

  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;
};

// A Tabu list is a fixed-sized set with FIFO replacement. It is expected to
// be of small size, usually a few dozens of elements.
template <typename T>
class TabuList {
 public:
  explicit TabuList(T size) : array_(0), fill_(0), index_(0) {
    array_.resize(size.value(), T(-1));
  }

  // Returns the size of the array.
  int size() const { return array_.size(); }

  // Initializes the array of the Tabu list.
  void Init(int size) {
    array_.resize(size, T(-1));
    fill_ = 0;
    index_ = 0;
  }

  // Adds t to the array. When the end of the array is reached, re-start at 0.
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

  // Returns true if t is in the array. This is O(size), but small.
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

// As usual and well-known with local search, SteepestSearch reaches a local
// minimum. We therefore implement Guided Tabu Search, which is a crossover of
// Guided Local Search and Tabu Search.
//
// Guided Local Search penalizes the parts of the solution that have been often
// used. It behaves as a long-term memory which "learns" the most used
// features and introduces some diversification in the search.
//
// C. Voudouris (1997) "Guided local search for combinatorial optimisation
// problems", PhD Thesis, University of Essex, Colchester, UK, July, 1997.
//
// Tabu Search makes it possible to degrade the solution temporarily
// by disallowing to go back for a certain time (changes are put in a "Tabu"
// list).
//
// Tabu behaves like a short-term memory and is the intensification part of the
// local search metaheuristic.
//
// F. Glover (1989) "Tabu Search – Part 1". ORSA Journal on Computing.
// 1 (2):190–206. doi:10.1287/ijoc.1.3.190.
// F. Glover (1990) "Tabu Search – Part 2". ORSA Journal on Computing.
// 2 (1): 4–32. doi:10.1287/ijoc.2.1.4.

// The consistency level is maintained up to kFreeAndUncovered.
class GuidedTabuSearch {
 public:
  explicit GuidedTabuSearch(SetCoverInvariant* inv)
      : inv_(inv),
        lagrangian_factor_(kDefaultLagrangianFactor),
        penalty_factor_(kDefaultPenaltyFactor),
        epsilon_(kDefaultEpsilon),
        augmented_costs_(),
        times_penalized_(),
        tabu_list_(SubsetIndex(kDefaultTabuListSize)) {
    Initialize();
  }

  // Initializes the Guided Tabu Search algorithm.
  void Initialize();

  // Returns the next solution by running the Tabu Search algorithm for maximum
  // num_iterations iterations.
  bool NextSolution(int num_iterations);

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus, int num_iterations);

  bool NextSolution(const SubsetBoolVector& in_focus, int num_iterations);

  // TODO(user): re-introduce this is the code. It was used to favor
  // subsets with the same marginal costs but that would cover more elements.
  // But first, see if it makes sense to compute it.
  void SetLagrangianFactor(double factor) { lagrangian_factor_ = factor; }
  double GetLagrangianFactor() const { return lagrangian_factor_; }

  void SetEpsilon(double r) { epsilon_ = r; }
  double GetEpsilon() const { return epsilon_; }

  // Setters and getters for the Guided Tabu Search algorithm parameters.
  void SetPenaltyFactor(double factor) { penalty_factor_ = factor; }
  double GetPenaltyFactor() const { return penalty_factor_; }

  void SetTabuListSize(int size) { tabu_list_.Init(size); }
  int GetTabuListSize() const { return tabu_list_.size(); }

 private:
  // Updates the penalties on the subsets in focus.
  void UpdatePenalties(absl::Span<const SubsetIndex> focus);

  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;

  // Search handling variables and default parameters.
  static constexpr double kDefaultLagrangianFactor = 100.0;
  double lagrangian_factor_;

  // Guided local search-related data.
  static constexpr double kPenaltyUpdateEpsilon = 1e-1;

  // Guided Tabu Search parameters.
  static constexpr double kDefaultPenaltyFactor = 0.3;
  double penalty_factor_;

  // Tabu Search parameters.
  static constexpr double kDefaultEpsilon = 1e-6;
  double epsilon_;

  // Penalized costs for each subset as used in Guided Tabu Search.
  SubsetCostVector augmented_costs_;

  // The number of times each subset was penalized during Guided Tabu Search.
  SubsetToIntVector times_penalized_;

  // TODO(user): remove and use priority_queue.
  // Utilities for the different subsets. They are updated ("penalized") costs.
  SubsetCostVector utilities_;

  // Tabu search-related data.
  static constexpr int kDefaultTabuListSize = 17;  // Nice prime number.
  TabuList<SubsetIndex> tabu_list_;
};

// Guided Local Search penalizes the parts of the solution that have been often
// used. It behaves as a long-term memory which "learns" the most used
// features and introduces some diversification in the search.
// At each iteration, the algorithm selects a subset from the focus with maximum
// utility of penalization and penalizes it.

// It has been observed that good values for the penalisation factor can be
// found by dividing the value of the objective function of a local minimum
// with the number of features present in it [1]. In our case, the penalisation
// factor is the sum of the costs of the subsets selected in the focus divided
// by the number of subsets in the focus times a tunable factor alpha_.
// [1] C. Voudouris (1997) "Guided local search for combinatorial optimisation
// problems", PhD Thesis, University of Essex, Colchester, UK, July, 1997.

// The consistency level is maintained up to kRedundancy.
class GuidedLocalSearch {
 public:
  explicit GuidedLocalSearch(SetCoverInvariant* inv)
      : inv_(inv), epsilon_(kDefaultEpsilon), alpha_(kDefaultAlpha) {
    Initialize();
  }

  // Initializes the Guided Local Search algorithm.
  void Initialize();

  // Returns the next solution by running the Guided Local  algorithm for
  // maximum num_iterations iterations.
  bool NextSolution(int num_iterations);

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus, int num_iterations);

  bool NextSolution(const SubsetBoolVector& in_focus, int num_iterations);

 private:
  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;

  // Setters and getters for the Guided Local Search algorithm parameters.
  void SetEpsilon(double r) { epsilon_ = r; }

  double GetEpsilon() const { return epsilon_; }

  void SetAlpha(double r) { alpha_ = r; }

  double GetAlpha() const { return alpha_; }

  // The epsilon value for the Guided Local Search algorithm.
  // Used to penalize the subsets within epsilon of the maximum utility.
  static constexpr double kDefaultEpsilon = 1e-8;
  double epsilon_;

  // The alpha value for the Guided Local Search algorithm.
  // Tunable factor used to penalize the subsets.
  static constexpr double kDefaultAlpha = 0.5;
  double alpha_;

  // The penalization value for the Guided Local Search algorithm.
  double penalization_factor_;

  // The penalties of each feature during Guided Local Search.
  SubsetToIntVector penalties_;

  // Computes the delta of the cost of the solution if subset state changed.
  Cost ComputeDelta(SubsetIndex subset) const;

  // The priority heap used to select the subset with the maximum priority to be
  // updated.
  AdjustableKAryHeap<float, SubsetIndex::ValueType, 2, true> priority_heap_;

  // The utility heap used to select the subset with the maximum utility to be
  // penalized.
  AdjustableKAryHeap<float, SubsetIndex::ValueType, 2, true> utility_heap_;
};

// Randomly clears a proportion num_subsets variables in the solution.
// Returns a list of subset indices to be potentially reused as a focus.
// Randomly clears at least num_subsets variables in the
// solution. There can be more than num_subsets variables cleared because the
// intersecting subsets are also removed from the solution. Returns a list of
// subset indices that can be reused as a focus.

// The consistency level is maintained up to kCostAndCoverage.
std::vector<SubsetIndex> ClearRandomSubsets(BaseInt num_subsets,
                                            SetCoverInvariant* inv);

// Same as above, but clears the subset indices in focus.
std::vector<SubsetIndex> ClearRandomSubsets(absl::Span<const SubsetIndex> focus,
                                            BaseInt num_subsets,
                                            SetCoverInvariant* inv);

// Clears the variables (subsets) that cover the most covered elements. This is
// capped by num_subsets. If the cap is reached, the subsets are chosen
// randomly.
// Returns the list of the chosen subset indices.
// This indices can then be used ax a focus.

// The consistency level is maintained up to kCostAndCoverage.
std::vector<SubsetIndex> ClearMostCoveredElements(BaseInt num_subsets,
                                                  SetCoverInvariant* inv);

// Same as above, but clears the subset indices in focus.
std::vector<SubsetIndex> ClearMostCoveredElements(
    absl::Span<const SubsetIndex> focus, BaseInt num_subsets,
    SetCoverInvariant* inv);
}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_HEURISTICS_H_
