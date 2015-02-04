// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_BOP_BOP_PORTFOLIO_H_
#define OR_TOOLS_BOP_BOP_PORTFOLIO_H_

#include "bop/bop_base.h"
#include "bop/bop_lns.h"
#include "bop/bop_parameters.pb.h"
#include "bop/bop_solution.h"
#include "bop/bop_types.h"
#include "glop/lp_solver.h"
#include "sat/boolean_problem.pb.h"
#include "sat/sat_solver.h"
#include "util/stats.h"
#include "util/time_limit.h"

namespace operations_research {
namespace bop {
// Forward declaration.
class AdaptativeItemSelector;

// This class implements a portfolio optimizer.
// The portfolio currently includes all the following optimizers:
//   - SatCoreBasedOptimizer
//   - LinearRelaxation
//   - LocalSearchOptimizer
//   - BopRandomFirstSolutionGenerator
//   - BopRandomConstraintLNSOptimizer
//   - BopRandomLNSOptimizer.
// At each call of Optimize(), the portfolio optimizer selects the next
// optimizer to run and runs it. The selection is auto-adaptative, meaning that
// optimizers that succeeded more in the previous calls to Optimizer() are more
// likely to be selected.
class PortfolioOptimizer : public BopOptimizerBase {
 public:
  explicit PortfolioOptimizer(const ProblemState& problem_state,
                              const BopParameters& parameters,
                              const BopSolverOptimizerSet& optimizer_set,
                              const std::string& name);
  virtual ~PortfolioOptimizer();

  virtual bool RunOncePerSolution() const { return false; }
  virtual bool NeedAFeasibleSolution() const { return false; }
  virtual Status Optimize(const BopParameters& parameters,
                          const ProblemState& problem_state,
                          LearnedInfo* learned_info, TimeLimit* time_limit);

 private:
  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);
  void AddOptimizer(const LinearBooleanProblem& problem,
                    const BopParameters& parameters,
                    const BopOptimizerMethod& optimizer_method);
  void CreateOptimizers(const LinearBooleanProblem& problem,
                        const BopParameters& parameters,
                        const BopSolverOptimizerSet& optimizer_set);

  int64 state_update_stamp_;
  BopConstraintTerms objective_terms_;
  std::unique_ptr<AdaptativeItemSelector> selector_;
  std::vector<std::unique_ptr<BopOptimizerBase>> optimizers_;
  std::vector<double> optimizer_initial_scores_;
  SatPropagator sat_propagator_;
  bool feasible_solution_;
  double lower_bound_;
  double upper_bound_;
};

// This class provides a way to iteratively select an item among n items in
// an adaptative way.
// TODO(user): Document and move to util?
class AdaptativeItemSelector {
 public:
  AdaptativeItemSelector(int random_seed, const std::vector<double>& initial_scores);

  static const int kNoSelection;
  static const double kErosion;
  static const double kScoreMin;

  void StartNewRoundOfSelections();

  bool SelectItem();

  bool has_selected_element() const {
    return selected_item_id_ != kNoSelection;
  }
  int selected_item_id() const { return selected_item_id_; }

  void UpdateSelectedItem(bool success, bool can_still_be_selected);

  void MarkItemNonSelectable(int item);

 private:
  struct Item {
    explicit Item(double initial_score)
        : round_score(std::max(kScoreMin, initial_score)),
          current_score(std::max(kScoreMin, initial_score)),
          can_be_selected(true),
          num_selections(0),
          num_successes(0) {}

    double round_score;
    double current_score;
    bool can_be_selected;
    int num_selections;
    int num_successes;
  };

  MTRandom random_;
  int selected_item_id_;
  std::vector<Item> items_;
};

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_PORTFOLIO_H_
