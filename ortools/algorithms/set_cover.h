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

#ifndef OR_TOOLS_ALGORITHMS_SET_COVER_H_
#define OR_TOOLS_ALGORITHMS_SET_COVER_H_

#include <vector>

#include "ortools/algorithms/set_cover_ledger.h"
#include "ortools/algorithms/set_cover_utils.h"

namespace operations_research {

// Solver classes for the weighted set covering problem.
//
// The solution procedure is based on the general scheme known as local search.
// Once a solution exists, it is improved by modifying it slightly, for example
// by flipping a binary variable, so as to minimize the cost.
// But first, we have to generate a first solution that is as good as possible.
//
// The first solution is then improved by using local search descent, which
// eliminates the T_j's that have no interest in the solution.
//
// A mix of the guided local search (GLS) and Tabu Search (TS) metaheuristic
// is also provided.
//
// The term 'focus' hereafter means a subset of the T_j's designated by their
// indices. Focus make it possible to run the algorithms on the corresponding
// subproblems.
//
// TODO(user): make the different algorithms concurrent, solving independent
// subproblems in different threads.
// TODO(user): implement Large Neighborhood Search
//
// An obvious idea is to take all the T_j's (or equivalently to set all the
// x_j's to 1). It's a bit silly but fast, and we can improve on it later using
// local search.
class TrivialSolutionGenerator {
 public:
  explicit TrivialSolutionGenerator(SetCoverLedger* ledger) : ledger_(ledger) {}

  // Returns true if a solution was found.
  // TODO(user): Add time-outs and exit with a partial solution. This seems
  // unlikely, though.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(const std::vector<SubsetIndex>& focus);

 private:
  // The ledger on which the algorithm will run.
  SetCoverLedger* ledger_;
};

// A slightly more complicated but better way to compute a first solution is to
// select columns randomly. Less silly than the previous one, and provides
// much better results.
// TODO(user): make it possible to use other random generators. Idea: bias the
// generator towards the columns with the least marginal costs.
class RandomSolutionGenerator {
 public:
  explicit RandomSolutionGenerator(SetCoverLedger* ledger) : ledger_(ledger) {}

  // Returns true if a solution was found.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(const std::vector<SubsetIndex>& focus);

 private:
  // The ledger on which the algorithm will run.
  SetCoverLedger* ledger_;
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
// The following paper dives into the details of this class of algorithms.
// Neal E. Young, Greedy Set-Cover Algorithms (1974-1979, Chvatal,
// Johnson, Lovasz, Stein), in Kao, ed., Encyclopedia of Algorithms. Draft at:
// http://www.cs.ucr.edu/~neal/non_arxiv/Young08SetCover.pdf
//
// G. Cormode, H. Karloff, A. Wirth (2010) "Set Cover Algorithms for Very Large
// Datasets", CIKM'10, ACM, 2010.
class GreedySolutionGenerator {
 public:
  explicit GreedySolutionGenerator(SetCoverLedger* ledger)
      : ledger_(ledger), pq_(ledger_) {}

  // Returns true if a solution was found.
  // TODO(user): Add time-outs and exit with a partial solution.
  bool NextSolution();

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(const std::vector<SubsetIndex>& focus);

 private:
  // Updates the priorities on the impacted_subsets.
  void UpdatePriorities(const std::vector<SubsetIndex>& impacted_subsets);

  // The ledger on which the algorithm will run.
  SetCoverLedger* ledger_;

  // The priority queue used for maintaining the subset with the lower marginal
  // cost.
  SubsetPriorityQueue pq_;
};

// Once we have a first solution to the problem, there may be (most often,
// there are) elements in S that are covered several times. To decrease the
// total cost, SteepestSearch tries to eliminate some redundant T_j's from
// the solution or equivalently, to flip some x_j's from 1 to 0. the algorithm
// gets its name because it goes in the steepest immediate direction, taking
// the T_j with the largest total cost.
class SteepestSearch {
 public:
  explicit SteepestSearch(SetCoverLedger* ledger)
      : ledger_(ledger), pq_(ledger_) {}

  // Returns true if a solution was found within num_iterations.
  // TODO(user): Add time-outs and exit with a partial solution.
  bool NextSolution(int num_iterations);

  bool NextSolution(const std::vector<SubsetIndex>& focus, int num_iterations);

 private:
  // Updates the priorities on the impacted_subsets.
  void UpdatePriorities(const std::vector<SubsetIndex>& impacted_subsets);

  // The ledger on which the algorithm will run.
  SetCoverLedger* ledger_;

  // The priority queue used for maintaining the subset with the largest total
  // cost.
  SubsetPriorityQueue pq_;
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
class GuidedTabuSearch {
 public:
  explicit GuidedTabuSearch(SetCoverLedger* ledger)
      : ledger_(ledger),
        pq_(ledger_),
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
  bool NextSolution(const std::vector<SubsetIndex>& focus, int num_iterations);

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
  void UpdatePenalties(const std::vector<SubsetIndex>& focus);

  // The ledger on which the algorithm will run.
  SetCoverLedger* ledger_;

  // The priority queue used ***
  SubsetPriorityQueue pq_;

  // Search handling variables and default parameters.
  static constexpr double kDefaultLagrangianFactor = 100.0;
  double lagrangian_factor_;

  // Guided local search-related data.
  static constexpr double kPenaltyUpdateEpsilon = 1e-1;

  // Guided Tabu Search parameters.
  static constexpr double kDefaultPenaltyFactor = 0.3;
  double penalty_factor_;

  // Tabu Search parameters.
  static constexpr double kDefaultEpsilon = 1e-8;
  double epsilon_;

  // Penalized costs for each subset as used in Guided Tabu Search.
  SubsetCostVector augmented_costs_;

  // The number of times each subset was penalized during Guided Tabu Search.
  SubsetCountVector times_penalized_;

  // TODO(user): remove and use priority_queue.
  // Utilities for the different subsets. They are updated ("penalized") costs.
  SubsetCostVector utilities_;

  // Tabu search-related data.
  static constexpr int kDefaultTabuListSize = 17;  // Nice prime number.
  TabuList<SubsetIndex> tabu_list_;
};

// Randomly clears a proportion (between 0 and 1) of the solution.
std::vector<SubsetIndex> ClearProportionRandomly(double proportion,
                                                 SetCoverLedger* ledger);

std::vector<SubsetIndex> ClearProportionRandomly(
    const std::vector<SubsetIndex>& focus, double proportion,
    SetCoverLedger* ledger);
}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_SET_COVER_H_
