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

#include "ortools/bop/bop_base.h"
#include "ortools/bop/bop_lns.h"
#include "ortools/bop/bop_parameters.pb.h"
#include "ortools/bop/bop_solution.h"
#include "ortools/bop/bop_types.h"
#include "ortools/glop/lp_solver.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/stats.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace bop {

DEFINE_INT_TYPE(OptimizerIndex, int);
const OptimizerIndex kInvalidOptimizerIndex(-1);

// Forward declaration.
class OptimizerSelector;

// This class implements a portfolio optimizer.
// The portfolio currently includes all the following optimizers:
//   - SAT_CORE_BASED
//   - SAT_LINEAR_SEARCH
//   - LINEAR_RELAXATION
//   - LOCAL_SEARCH
//   - RANDOM_FIRST_SOLUTION
//   - RANDOM_CONSTRAINT_LNS
//   - RANDOM_VARIABLE_LNS
//   - COMPLETE_LNS
//   - LP_FIRST_SOLUTION
//   - OBJECTIVE_FIRST_SOLUTION
//   - USER_GUIDED_FIRST_SOLUTION
//   - FEASIBILITY_PUMP_FIRST_SOLUTION
//   - RANDOM_CONSTRAINT_LNS_GUIDED_BY_LP
//   - RANDOM_VARIABLE_LNS_GUIDED_BY_LP
//   - RELATION_GRAPH_LNS
//   - RELATION_GRAPH_LNS_GUIDED_BY_LP
//
// At each call of Optimize(), the portfolio optimizer selects the next
// optimizer to run and runs it. The selection is auto-adaptative, meaning that
// optimizers that succeeded more in the previous calls to Optimizer() are more
// likely to be selected.
class PortfolioOptimizer : public BopOptimizerBase {
 public:
  PortfolioOptimizer(const ProblemState& problem_state,
                     const BopParameters& parameters,
                     const BopSolverOptimizerSet& optimizer_set,
                     const std::string& name);
  ~PortfolioOptimizer() override;

  bool ShouldBeRun(const ProblemState& problem_state) const override {
    return true;
  }
  Status Optimize(const BopParameters& parameters,
                  const ProblemState& problem_state, LearnedInfo* learned_info,
                  TimeLimit* time_limit) override;

 private:
  BopOptimizerBase::Status SynchronizeIfNeeded(
      const ProblemState& problem_state);
  void AddOptimizer(const LinearBooleanProblem& problem,
                    const BopParameters& parameters,
                    const BopOptimizerMethod& optimizer_method);
  void CreateOptimizers(const LinearBooleanProblem& problem,
                        const BopParameters& parameters,
                        const BopSolverOptimizerSet& optimizer_set);

  std::unique_ptr<MTRandom> random_;
  int64 state_update_stamp_;
  BopConstraintTerms objective_terms_;
  std::unique_ptr<OptimizerSelector> selector_;
  ITIVector<OptimizerIndex, BopOptimizerBase*> optimizers_;
  sat::SatSolver sat_propagator_;
  BopParameters parameters_;
  double lower_bound_;
  double upper_bound_;
  int number_of_consecutive_failing_optimizers_;
};

// This class is providing an adaptative selector for optimizers based on
// their past successes and deterministic time spent.
class OptimizerSelector {
 public:
  // Note that the list of optimizers is only used to get the names for
  // debug purposes, the ownership of the optimizers is not transfered.
  explicit OptimizerSelector(
      const ITIVector<OptimizerIndex, BopOptimizerBase*>& optimizers);

  // Selects the next optimizer to run based on the user defined order and
  // history of success. Returns kInvalidOptimizerIndex if no optimizer is
  // selectable and runnable (see the functions below).
  //
  // The optimizer is selected using the following algorithm (L being the
  // sorted list of optimizers, and l the position of the last selected
  // optimizer):
  //   a- If a new solution has been found by optimizer l, select the first
  //      optimizer l' in L, l' >= 0, that can run.
  //   b- If optimizer l didn't find a new solution, select the first
  //      optimizer l', with l' > l, such that its deterministic time spent
  //      since last solution is smaller than the deterministic time spent
  //      by any runnable optimizer in 1..l since last solution.
  //      If no such optimizer is available, go to option a.
  OptimizerIndex SelectOptimizer();

  // Updates the internal metrics to decide which optimizer to select.
  // This method should be called each time the selected optimizer is run.
  //
  // The gain corresponds to the reward to assign to the solver; It could for
  // instance be the difference in cost between the last and the current
  // solution.
  //
  // The time spent corresponds to the time the optimizer spent; To make the
  // behavior deterministic, it is recommanded to use the deterministic time
  // instead of the elapsed time.
  //
  // The optimizers are sorted based on their score each time a new solution is
  // found.
  void UpdateScore(int64 gain, double time_spent);

  // Marks the given optimizer as not selectable until UpdateScore() is called
  // with a positive gain. In which case, all optimizer will become selectable
  // again.
  void TemporarilyMarkOptimizerAsUnselectable(OptimizerIndex optimizer_index);

  // Sets whether or not an optimizer is "runnable". Like a non-selectable one,
  // a non-runnable optimizer will never be returned by SelectOptimizer().
  //
  // TODO(user): Maybe we should simply have the notion of selectability here
  // and let the client handle the logic to decide what optimizer are selectable
  // or not.
  void SetOptimizerRunnability(OptimizerIndex optimizer_index, bool runable);

  // Returns statistics about the given optimizer.
  std::string PrintStats(OptimizerIndex optimizer_index) const;
  int NumCallsForOptimizer(OptimizerIndex optimizer_index) const;

  // Prints some debug information. Should not be used in production.
  void DebugPrint() const;

 private:
  // Updates internals when a solution has been found using the selected
  // optimizer.
  void NewSolutionFound(int64 gain);

  // Updates the deterministic time spent by the selected optimizer.
  void UpdateDeterministicTime(double time_spent);

  // Sorts optimizers based on their scores.
  void UpdateOrder();

  struct RunInfo {
    RunInfo(OptimizerIndex i, const std::string& n)
        : optimizer_index(i),
          name(n),
          num_successes(0),
          num_calls(0),
          total_gain(0),
          time_spent(0.0),
          time_spent_since_last_solution(0),
          runnable(true),
          selectable(true),
          score(0.0) {}

    bool RunnableAndSelectable() const { return runnable && selectable; }

    OptimizerIndex optimizer_index;
    std::string name;
    int num_successes;
    int num_calls;
    int64 total_gain;
    double time_spent;
    double time_spent_since_last_solution;
    bool runnable;
    bool selectable;
    double score;
  };

  std::vector<RunInfo> run_infos_;
  ITIVector<OptimizerIndex, int> info_positions_;
  int selected_index_;
};

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_PORTFOLIO_H_
