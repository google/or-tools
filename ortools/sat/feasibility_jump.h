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

#ifndef OR_TOOLS_SAT_FEASIBILITY_JUMP_H_
#define OR_TOOLS_SAT_FEASIBILITY_JUMP_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/sat/constraint_violation.h"
#include "ortools/sat/linear_model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research::sat {

class CompoundMoveBuilder;

// Implements and heuristic similar to the one described in the paper:
// "Feasibility Jump: an LP-free Lagrangian MIP heuristic", Bjørnar
// Luteberget, Giorgio Sartor, 2023, Mathematical Programming Computation.
//
// This is basically a Guided local search (GLS) with a nice algo to know what
// value an integer variable should move to (its jump value). For binary, it
// can only be swapped, so the situation is easier.
//
// TODO(user): If we have more than one of these solver, we might want to share
// the evaluator memory between them. Right now we basically keep a copy of the
// model and its transpose for each FeasibilityJumpSolver.
class FeasibilityJumpSolver : public SubSolver {
 public:
  FeasibilityJumpSolver(const std::string name, SubSolver::SubsolverType type,
                        const LinearModel* linear_model, SatParameters params,
                        ModelSharedTimeLimit* shared_time_limit,
                        SharedResponseManager* shared_response,
                        SharedBoundsManager* shared_bounds,
                        SharedStatistics* shared_stats)
      : SubSolver(name, type),
        linear_model_(linear_model),
        params_(params),
        shared_time_limit_(shared_time_limit),
        shared_response_(shared_response),
        shared_bounds_(shared_bounds),
        shared_stats_(shared_stats),
        random_(params_) {}

  // If VLOG_IS_ON(1), it will export a bunch of statistics.
  ~FeasibilityJumpSolver() override;

  // No synchronization needed for TaskIsAvailable().
  void Synchronize() final {}

  // Note that this should only returns true if there is a need to delete this
  // subsolver early to reclaim memory, otherwise we will not properly have the
  // stats.
  //
  // TODO(user): Save the logging stats before deletion.
  bool IsDone() final {
    // Tricky: we cannot delete something if there is a task in flight, we will
    // have to wait.
    if (task_generated_.load()) return false;
    if (!model_is_supported_.load()) return true;

    // We are done after the first task is done in the FIRST_SOLUTION mode.
    return type() == SubSolver::FIRST_SOLUTION &&
           shared_response_->first_solution_solvers_should_stop()->load();
  }

  bool TaskIsAvailable() final {
    if (task_generated_.load()) return false;
    if (shared_response_->ProblemIsSolved()) return false;
    if (shared_time_limit_->LimitReached()) return false;

    return (shared_response_->SolutionsRepository().NumSolutions() > 0) ==
           (type() == SubSolver::INCOMPLETE);
  }

  std::function<void()> GenerateTask(int64_t /*task_id*/) final;

 private:
  void Initialize();
  void ResetCurrentSolution();
  void PerturbateCurrentSolution();
  std::string OneLineStats() const;

  // Linear only.
  bool JumpIsUpToDate(int var);  // Debug.
  void RecomputeJump(int var);
  void RecomputeAllJumps();
  void MarkJumpsThatNeedsToBeRecomputed(int changed_var);

  // Moves.
  bool DoSomeLinearIterations();
  bool DoSomeGeneralIterations();

  // Returns true if an improving move was found.
  bool ScanRelevantVariables(absl::Span<const double> weights,
                             int* improving_var, int64_t* improving_value,
                             double* improving_score, bool* time_limit_crossed);

  // Increases the weight of the currently infeasible constraints.
  void UpdateViolatedConstraintWeights();

  // Returns true if changing this variable to its jump value reduces weighted
  // violation, or has no impact on weighted violation but reduces the
  // objective.
  bool IsGood(int var) const;

  void RecomputeVarsToScan();
  // Returns true if it is possible that `var` may have value that reduces
  // weighted violation or improve the objective.
  // Note that this is independent of the actual weights used.
  bool ShouldScan(int var) const;
  void AddVarToScan(int var);
  bool MustRecomputeJumpOnGeneralUpdate(int var) const;

  // Resets the weights used to find compound moves. Only modifies weights that
  // have changed, but the invariant below holds for all constraints:
  // compound_weights[c] = weights_[c] if c is violated, and epsilon *
  // weights_[c] otherwise.
  void ResetChangedCompoundWeights();

  const LinearModel* linear_model_;
  SatParameters params_;
  ModelSharedTimeLimit* shared_time_limit_;
  SharedResponseManager* shared_response_;
  SharedBoundsManager* shared_bounds_ = nullptr;
  SharedStatistics* shared_stats_;
  ModelRandomGenerator random_;

  // Synchronization Booleans.
  //
  // Note that we don't fully support all type of model, and we will abort by
  // setting the model_is_supported_ bool to false when we detect this.
  bool is_initialized_ = false;
  std::atomic<bool> model_is_supported_ = true;
  std::atomic<bool> task_generated_ = false;

  std::unique_ptr<LsEvaluator> evaluator_;
  std::vector<Domain> var_domains_;
  std::vector<bool> var_has_two_values_;

  // For each variable, we store:
  // - A jump delta which represent a change in variable value:
  //   new_value = old + delta, which is non-zero except for fixed variable.
  // - The associated weighted feasibility violation change if we take this
  //   jump.
  std::vector<bool> jump_need_recomputation_;
  std::vector<int64_t> jump_deltas_;
  std::vector<double> jump_scores_;
  std::vector<double> for_weight_update_;

  // The score of a solution is just the sum of infeasibility of each
  // constraint weighted by these scores.
  std::vector<double> weights_;
  // If using compound moves, these will be discounted on a new incumbent then
  // re-converge to `weights_` after some exploration.
  // Search will repeatedly pick moves with negative WeightedViolationDelta
  // using these weights.
  std::vector<double> compound_weights_;

  std::vector<bool> in_compound_weight_changed_;
  std::vector<int> compound_weight_changed_;

  // Depending on the options, we use an exponentially decaying constraint
  // weight like for SAT activities.
  double bump_value_ = 1.0;

  // Sparse list of jumps with a potential delta < 0.0.
  // If jump_need_recomputation_[var] is true, we lazily recompute the exact
  // delta as we randomly pick variables from here.
  std::vector<bool> in_good_jumps_;
  std::vector<int> good_jumps_;

  // We restart each time our local deterministic time crosses this.
  double dtime_restart_threshold_ = 0.0;
  int64_t update_restart_threshold_ = 0;
  int num_batches_before_perturbation_;

  std::vector<int64_t> tmp_breakpoints_;

  // Each time we reset the weight, we might randomly change this to update
  // them with decay or not.
  bool use_decay_ = true;

  // Each batch, randomly decide if we will use compound moves or not.
  bool use_compound_moves_ = false;

  // Statistics
  int64_t num_batches_ = 0;
  int64_t num_linear_evals_ = 0;
  int64_t num_general_evals_ = 0;
  int64_t num_general_moves_ = 0;
  int64_t num_compound_moves_ = 0;
  int64_t num_linear_moves_ = 0;
  int64_t num_perturbations_ = 0;
  int64_t num_restarts_ = 0;
  int64_t num_solutions_imported_ = 0;
  int64_t num_weight_updates_ = 0;

  // A list of variables that might be relevant to check in general iterations.
  std::vector<bool> in_vars_to_scan_;
  std::vector<int> vars_to_scan_;
  std::unique_ptr<CompoundMoveBuilder> move_;

  // Counts the number of violated constraints each var is in.
  // Only maintained in general iterations as jump_score_ is used for filtering
  // moves in linear iterations.
  std::vector<int> num_violated_constraints_per_var_;

  // Info on the last solution loaded.
  int64_t last_solution_rank_ = std::numeric_limits<int64_t>::max();
};

// This class helps keep track of moves that change more than one variable.
// Mainly this class keeps track of how to backtrack back to the local minimum
// as you make a sequence of exploratory moves, so in order to commit a compound
// move, you just need to call `Clear` instead of Backtracking over the changes.
class CompoundMoveBuilder {
 public:
  CompoundMoveBuilder(LsEvaluator* evaluator, int num_variables)
      : evaluator_(evaluator), var_on_stack_(num_variables, false) {}

  // Adds an atomic move to the stack.
  // `var` must not be on the stack (this is DCHECKed).
  void Push(int var, int64_t prev_value, double score);

  // Sets var, val and score to a move that will revert the most recent atomic
  // move on the stack, and pops this move from the stack.
  // Returns false if the stack is empty.
  bool Backtrack(int* var, int64_t* value, double* score);

  // Removes all moves on the stack.
  void Clear();

  // Returns the number of variables in the move.
  int Size() const { return stack_.size(); }

  // Returns true if this var has been set in this move already,
  bool OnStack(int var) const;

  // Returns the sum of scores of atomic moved pushed to this compound move.
  double Score() const {
    return stack_.empty() ? 0.0 : stack_.back().cumulative_score;
  }

  // Returns the sum of objective deltas from the root.
  double ObjectiveDelta() const {
    return stack_.empty() ? 0.0 : stack_.back().cumulative_objective_delta;
  }

  // Returns true if this move reduces weighted violation, or improves the
  // objective without increasing violation.
  bool IsImproving() const;

  // Returns the number of backtracking moves that have been applied.
  int NumBacktracks() const { return num_backtracks_; }

 private:
  struct UnitMove {
    int var;
    int64_t prev_value;
    // Note that this stores the score of reverting to prev_value.
    double score;
    // Instead of tracking this on the compound move, we store the partial sums
    // to avoid numerical issues causing negative scores after backtracking.
    double cumulative_score;
    double cumulative_objective_delta;
  };
  LsEvaluator* evaluator_;
  std::vector<bool> var_on_stack_;
  std::vector<UnitMove> stack_;

  int64_t num_backtracks_ = 0;
};

}  // namespace operations_research::sat

#endif  // OR_TOOLS_SAT_FEASIBILITY_JUMP_H_
