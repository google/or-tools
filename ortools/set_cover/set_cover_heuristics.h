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

#ifndef OR_TOOLS_SET_COVER_SET_COVER_HEURISTICS_H_
#define OR_TOOLS_SET_COVER_SET_COVER_HEURISTICS_H_

#include <stdbool.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/algorithms/adjustable_k_ary_heap.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_invariant.h"
#include "ortools/set_cover/set_cover_model.h"

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

// Base class for all set cover solution generators. This is almost an
// interface.
class SetCoverSolutionGenerator {
 public:
  // By default, the maximum number of iterations is set to infinity, and the
  // maximum time in seconds is set to infinity as well (and the time limit is
  // not yet implemented).
  SetCoverSolutionGenerator(SetCoverInvariant* inv,
                            absl::string_view class_name,
                            absl::string_view name)
      : run_time_(absl::ZeroDuration()),
        inv_(inv),
        class_name_(class_name),
        name_(name),
        time_limit_in_seconds_(std::numeric_limits<double>::infinity()),
        max_iterations_(std::numeric_limits<int64_t>::max()) {}

  virtual ~SetCoverSolutionGenerator() = default;

  void SetName(const absl::string_view name) { name_ = name; }

  SetCoverInvariant* inv() const { return inv_; }

  // Resets the limits to their default values.
  virtual SetCoverSolutionGenerator& ResetLimits() {
    time_limit_in_seconds_ = std::numeric_limits<double>::infinity();
    max_iterations_ = std::numeric_limits<int64_t>::max();
    return *this;
  }

  // Sets the maximum number of iterations.
  SetCoverSolutionGenerator& SetMaxIterations(int64_t max_iterations) {
    max_iterations_ = max_iterations;
    return *this;
  }

  // Returns the maximum number of iterations.
  int64_t max_iterations() const { return max_iterations_; }

  // Sets the time limit in seconds.
  SetCoverSolutionGenerator& SetTimeLimitInSeconds(double seconds) {
    time_limit_in_seconds_ = seconds;
    return *this;
  }

  absl::Duration run_time() const { return run_time_; }

  // Returns the total elapsed runtime in seconds.
  double run_time_in_seconds() const {
    return absl::ToDoubleSeconds(run_time_);
  }

  // Returns the total elapsed runtime in microseconds.
  double run_time_in_microseconds() const {
    return absl::ToInt64Microseconds(run_time_);
  }

  // Returns the name of the heuristic.
  std::string name() const { return name_; }

  // Returns the name of the class.
  std::string class_name() const { return class_name_; }

  // Returns the current cost of the solution in the invariant.
  Cost cost() const { return inv_->cost(); }

  // Virtual methods that must be implemented by derived classes.

  // Computes the next full solution taking into account all the subsets.
  virtual bool NextSolution() = 0;

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  virtual bool NextSolution(absl::Span<const SubsetIndex> focus) = 0;

  // Same as above, but with a vector of Booleans as focus.
  virtual bool NextSolution(const SubsetBoolVector& in_focus) = 0;

 protected:
  // Accessors.
  SetCoverModel* model() const { return inv_->model(); }
  BaseInt num_subsets() const { return model()->num_subsets(); }

  // The time limit in seconds.
  double time_limit_in_seconds() const { return time_limit_in_seconds_; }

  // run_time_ is an abstract duration for the time spent in NextSolution().
  absl::Duration run_time_;

 private:
  // The data structure that will maintain the invariant for the model.
  SetCoverInvariant* inv_;

  // The name of the solution generator class. Cannot be changed by the user.
  std::string class_name_;

  // The name of the solution generator object. Set to the name of the class
  // by default, but can be changed by the user.
  std::string name_;

  // The time limit in seconds.
  double time_limit_in_seconds_;

  // The maximum number of iterations.
  int64_t max_iterations_;
};

// Now we define two classes that are used to implement the solution
// generators. The first one uses a vector of subset indices as focus, while
// the second one uses a vector of Booleans. This makes the declaration of the
// solution generators shorter, more readable and improves type correctness.

// The class of solution generators that use a vector of subset indices as
// focus, with a transformation from a vector of Booleans to a vector of
// subset indices if needed.
class SubsetListBasedSolutionGenerator : public SetCoverSolutionGenerator {
 public:
  explicit SubsetListBasedSolutionGenerator(SetCoverInvariant* inv,
                                            absl::string_view class_name,
                                            absl::string_view name)
      : SetCoverSolutionGenerator(inv, class_name, name) {}

  bool NextSolution(absl::Span<const SubsetIndex> _) override { return false; }

  bool NextSolution() final { return NextSolution(model()->all_subsets()); }

  bool NextSolution(const SubsetBoolVector& in_focus) final {
    return NextSolution(MakeSubsetIndexSpan(in_focus));
  }

 private:
  // Converts a vector of Booleans to a vector of subset indices.
  // TODO(user): this should not be, but a better iterator system should be
  // implemented.
  absl::Span<const SubsetIndex> MakeSubsetIndexSpan(
      const SubsetBoolVector& in_focus) {
    std::vector<SubsetIndex> result;
    result.reserve(in_focus.size());
    SubsetIndex i(0);
    for (const auto bit : in_focus) {
      if (bit) {
        result.push_back(i);
      }
    }
    return absl::MakeConstSpan(result);
  }
};

// The class of solution generators that use a vector of Booleans as focus,
// with a transformation from a vector of subset indices to a vector of
// Booleans if needed.
class BoolVectorBasedSolutionGenerator : public SetCoverSolutionGenerator {
 public:
  explicit BoolVectorBasedSolutionGenerator(SetCoverInvariant* inv,
                                            absl::string_view class_name,
                                            absl::string_view name)
      : SetCoverSolutionGenerator(inv, class_name, name) {}

  bool NextSolution(const SubsetBoolVector& _) override { return false; }

  bool NextSolution(absl::Span<const SubsetIndex> focus) final {
    return NextSolution(MakeBoolVector(focus, num_subsets()));
  }

  bool NextSolution() final {
    return NextSolution(SubsetBoolVector(num_subsets(), true));
  }

 private:
  // Converts a vector of subset indices to a vector of Booleans.
  // TODO(user): this should not be, but a better iterator system should be
  // implemented.
  SubsetBoolVector MakeBoolVector(absl::Span<const SubsetIndex> focus,
                                  BaseInt size) {
    SubsetBoolVector result(SubsetIndex(size), false);
    for (const SubsetIndex subset : focus) {
      result[subset] = true;
    }
    return result;
  }
};

// An obvious idea is to take all the S_j's (or equivalently to set all the
// x_j's to 1). It's very silly but fast, and we can improve on it later
// using local search.

// The consistency level is maintained up to kFreeAndUncovered.
class TrivialSolutionGenerator : public SubsetListBasedSolutionGenerator {
 public:
  explicit TrivialSolutionGenerator(SetCoverInvariant* inv)
      : TrivialSolutionGenerator(inv, "TrivialGenerator") {}

  TrivialSolutionGenerator(SetCoverInvariant* inv, absl::string_view name)
      : SubsetListBasedSolutionGenerator(inv, "TrivialGenerator", name) {}

  using SubsetListBasedSolutionGenerator::NextSolution;
  bool NextSolution(absl::Span<const SubsetIndex> focus) final;
};

// A slightly more complicated but better way to compute a first solution is
// to select columns randomly. Less silly than the previous one, and
// provides much better results.
// TODO(user): make it possible to use other random generators. Idea: bias
// the generator towards the columns with the least marginal costs.

// The consistency level is maintained up to kFreeAndUncovered.
class RandomSolutionGenerator : public SubsetListBasedSolutionGenerator {
 public:
  explicit RandomSolutionGenerator(SetCoverInvariant* inv)
      : RandomSolutionGenerator(inv, "RandomGenerator") {}

  RandomSolutionGenerator(SetCoverInvariant* inv, absl::string_view name)
      : SubsetListBasedSolutionGenerator(inv, "RandomGenerator", name) {}

  using SubsetListBasedSolutionGenerator::NextSolution;
  bool NextSolution(absl::Span<const SubsetIndex> focus) final;
};

// The first solution is obtained using the Chvatal heuristic, that
// guarantees that the solution is at most 1 + log(n) times the optimal
// value. Vasek Chvatal, 1979. A greedy heuristic for the set-covering
// problem. Mathematics of Operations Research, 4(3):233-235, 1979.
// http://www.jstor.org/stable/3689577
//
// Chvatal's heuristic works as follows: Choose the subset that covers as
// many remaining uncovered elements as possible for the least possible cost
// per element and iterate.
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
class GreedySolutionGenerator : public SubsetListBasedSolutionGenerator {
 public:
  explicit GreedySolutionGenerator(SetCoverInvariant* inv)
      : GreedySolutionGenerator(inv, "GreedyGenerator") {}

  GreedySolutionGenerator(SetCoverInvariant* inv, absl::string_view name)
      : SubsetListBasedSolutionGenerator(inv, "GreedyGenerator", name) {}

  using SubsetListBasedSolutionGenerator::NextSolution;
  bool NextSolution(absl::Span<const SubsetIndex> focus) final;
};

// Solution generator based on the degree of elements.
// The degree of an element is the number of subsets covering it.
// The generator consists in iteratively choosing a non-covered element with
// the smallest degree, and selecting a subset that covers it with the least
// ratio cost / number of uncovered elements. The number of uncovered elements
// are updated for each impacted subset. The newly-covered elements degrees
// are also updated and set to zero.

// The consistency level is maintained up to kFreeAndUncovered.
class ElementDegreeSolutionGenerator : public BoolVectorBasedSolutionGenerator {
 public:
  explicit ElementDegreeSolutionGenerator(SetCoverInvariant* inv)
      : ElementDegreeSolutionGenerator(inv, "ElementDegreeGenerator") {}

  ElementDegreeSolutionGenerator(SetCoverInvariant* inv, absl::string_view name)
      : BoolVectorBasedSolutionGenerator(inv, "ElementDegreeGenerator", name) {}

  using BoolVectorBasedSolutionGenerator::NextSolution;
  bool NextSolution(const SubsetBoolVector& in_focus) final;
};

// Solution generator based on the degree of elements.
// The heuristic is the same as ElementDegreeSolutionGenerator, but the number
// of uncovered elements for a subset is computed on-demand. In empirical
// tests, this is faster than ElementDegreeSolutionGenerator because a very
// small percentage of need to be computed, and even fewer among them need to
// be computed again later on.

// Because the number of uncovered elements is computed on-demand, the
// consistency level only needs to be set to kCostAndCoverage.
class LazyElementDegreeSolutionGenerator
    : public BoolVectorBasedSolutionGenerator {
 public:
  explicit LazyElementDegreeSolutionGenerator(SetCoverInvariant* inv)
      : LazyElementDegreeSolutionGenerator(inv, "LazyElementDegreeGenerator") {}

  LazyElementDegreeSolutionGenerator(SetCoverInvariant* inv,
                                     absl::string_view name)
      : BoolVectorBasedSolutionGenerator(inv, "LazyElementDegreeGenerator",
                                         name) {}

  using BoolVectorBasedSolutionGenerator::NextSolution;
  bool NextSolution(const SubsetBoolVector& in_focus) final;
};

// Once we have a first solution to the problem, there may be (most often,
// there are) elements in E that are covered several times. To decrease the
// total cost, SteepestSearch tries to eliminate some redundant S_j's from
// the solution or equivalently, to flip some x_j's from 1 to 0. the
// algorithm gets its name because it goes in the steepest immediate
// direction, taking the S_j with the largest total cost.

// The consistency level is maintained up to kFreeAndUncovered.
class SteepestSearch : public BoolVectorBasedSolutionGenerator {
 public:
  explicit SteepestSearch(SetCoverInvariant* inv)
      : SteepestSearch(inv, "SteepestSearch") {}

  SteepestSearch(SetCoverInvariant* inv, absl::string_view name)
      : BoolVectorBasedSolutionGenerator(inv, "SteepestSearch", name) {}

  using BoolVectorBasedSolutionGenerator::NextSolution;
  bool NextSolution(const SubsetBoolVector& in_focus) final;
};

// Lazy Steepest Search is a variant of Steepest Search that does not use
// any priority queue to update the priorities of the subsets. The
// priorities are computed when needed. It is faster to compute because
// there are relatively few subsets in the solution, because the cardinality
// of the solution is bounded by the number of elements.
class LazySteepestSearch : public BoolVectorBasedSolutionGenerator {
 public:
  explicit LazySteepestSearch(SetCoverInvariant* inv)
      : LazySteepestSearch(inv, "LazySteepestSearch") {}

  LazySteepestSearch(SetCoverInvariant* inv, absl::string_view name)
      : BoolVectorBasedSolutionGenerator(inv, "LazySteepestSearch", name) {}

  using BoolVectorBasedSolutionGenerator::NextSolution;
  bool NextSolution(const SubsetBoolVector& in_focus) final;
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
// minimum. We therefore implement Guided Tabu Search, which is a crossover
// of Guided Local Search and Tabu Search.
//
// Guided Local Search penalizes the parts of the solution that have been
// often used. It behaves as a long-term memory which "learns" the most used
// features and introduces some diversification in the search.
//
// C. Voudouris (1997) "Guided local search for combinatorial optimisation
// problems", PhD Thesis, University of Essex, Colchester, UK, July, 1997.
//
// Tabu Search makes it possible to degrade the solution temporarily
// by disallowing to go back for a certain time (changes are put in a "Tabu"
// list).
//
// Tabu behaves like a short-term memory and is the intensification part of
// the local search metaheuristic.
//
// F. Glover (1989) "Tabu Search – Part 1". ORSA Journal on Computing.
// 1 (2):190–206. doi:10.1287/ijoc.1.3.190.
// F. Glover (1990) "Tabu Search – Part 2". ORSA Journal on Computing.
// 2 (1): 4–32. doi:10.1287/ijoc.2.1.4.

// The consistency level is maintained up to kFreeAndUncovered.
class GuidedTabuSearch : public SubsetListBasedSolutionGenerator {
 public:
  explicit GuidedTabuSearch(SetCoverInvariant* inv)
      : GuidedTabuSearch(inv, "GuidedTabuSearch") {}

  GuidedTabuSearch(SetCoverInvariant* inv, absl::string_view name)
      : SubsetListBasedSolutionGenerator(inv, "GuidedTabuSearch", name),
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

  using SubsetListBasedSolutionGenerator::NextSolution;
  bool NextSolution(absl::Span<const SubsetIndex> focus) final;

  // TODO(user): re-introduce this is the code. It was used to favor
  // subsets with the same marginal costs but that would cover more
  // elements. But first, see if it makes sense to compute it.
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

  // The number of times each subset was penalized during Guided Tabu
  // Search.
  SubsetToIntVector times_penalized_;

  // TODO(user): remove and use priority_queue.
  // Utilities for the different subsets. They are updated ("penalized")
  // costs.
  SubsetCostVector utilities_;

  // Tabu search-related data.
  static constexpr int kDefaultTabuListSize = 17;  // Nice prime number.
  TabuList<SubsetIndex> tabu_list_;
};

// Guided Local Search penalizes the parts of the solution that have been
// often used. It behaves as a long-term memory which "learns" the most used
// features and introduces some diversification in the search.
// At each iteration, the algorithm selects a subset from the focus with
// maximum utility of penalization and penalizes it.

// It has been observed that good values for the penalisation factor can be
// found by dividing the value of the objective function of a local minimum
// with the number of features present in it [1]. In our case, the
// penalisation factor is the sum of the costs of the subsets selected in
// the focus divided by the number of subsets in the focus times a tunable
// factor alpha_. [1] C. Voudouris (1997) "Guided local search for
// combinatorial optimisation problems", PhD Thesis, University of Essex,
// Colchester, UK, July, 1997.

// The consistency level is maintained up to kRedundancy.
class GuidedLocalSearch : public SubsetListBasedSolutionGenerator {
 public:
  explicit GuidedLocalSearch(SetCoverInvariant* inv)
      : GuidedLocalSearch(inv, "GuidedLocalSearch") {}

  GuidedLocalSearch(SetCoverInvariant* inv, absl::string_view name)
      : SubsetListBasedSolutionGenerator(inv, "GuidedLocalSearch", name),
        epsilon_(kDefaultEpsilon),
        alpha_(kDefaultAlpha) {
    Initialize();
  }

  // Initializes the Guided Local Search algorithm.
  void Initialize();

  using SubsetListBasedSolutionGenerator::NextSolution;
  bool NextSolution(absl::Span<const SubsetIndex> focus) final;

 private:
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

  // The priority heap used to select the subset with the maximum priority
  // to be updated.
  AdjustableKAryHeap<float, SubsetIndex::ValueType, 2, true> priority_heap_;

  // The utility heap used to select the subset with the maximum utility to
  // be penalized.
  AdjustableKAryHeap<float, SubsetIndex::ValueType, 2, true> utility_heap_;
};

// Randomly clears a proportion num_subsets variables in the solution.
// Returns a list of subset indices to be potentially reused as a focus.
// Randomly clears at least num_subsets variables in the
// solution. There can be more than num_subsets variables cleared because
// the intersecting subsets are also removed from the solution. Returns a
// list of subset indices that can be reused as a focus.

// The consistency level is maintained up to kCostAndCoverage.
std::vector<SubsetIndex> ClearRandomSubsets(BaseInt num_subsets,
                                            SetCoverInvariant* inv);

// Same as above, but clears the subset indices in focus.
std::vector<SubsetIndex> ClearRandomSubsets(absl::Span<const SubsetIndex> focus,
                                            BaseInt num_subsets,
                                            SetCoverInvariant* inv);

// Clears the variables (subsets) that cover the most covered elements. This
// is capped by num_subsets. If the cap is reached, the subsets are chosen
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

#endif  // OR_TOOLS_SET_COVER_SET_COVER_HEURISTICS_H_
