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
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/functional/bind_front.h"
#include "absl/types/span.h"
#include "ortools/sat/constraint_violation.h"
#include "ortools/sat/linear_model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/stat_tables.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research::sat {

class CompoundMoveBuilder;

// This class lazily caches the results of `compute_jump(var)` which returns a
// <delta, score> pair.
// Variables' scores can be manually modified using MutableScores (if the
// optimal jump is known not to change), or marked for recomputation on the next
// call to GetJump(var) by calling Recompute.
class JumpTable {
 public:
  explicit JumpTable(
      absl::AnyInvocable<std::pair<int64_t, double>(int)> compute_jump);
  void RecomputeAll(int num_variables);

  // Gets the current jump delta and score, recomputing if necessary.
  std::pair<int64_t, double> GetJump(int var);
  // If the new optimum value and score is known, users can update it directly.
  // e.g. after weight rescaling, or after changing a binary variable.
  void SetJump(int var, int64_t delta, double score);
  // Recompute the jump for `var` when `GetJump(var)` is next called.
  void Recompute(int var);
  // Returns true if the jump score for `var` might be negative.
  bool PossiblyGood(int var) const;

  // Advanced usage, allows users to read possibly stale deltas for incremental
  // score updates.
  absl::Span<const int64_t> Deltas() const {
    return absl::MakeConstSpan(deltas_);
  }
  absl::Span<const double> Scores() const {
    return absl::MakeConstSpan(scores_);
  }

  absl::Span<double> MutableScores() { return absl::MakeSpan(scores_); }

  // Note if you have very high weights (e.g. when using decay), the tolerances
  // in this function are likely too tight.
  bool JumpIsUpToDate(int var);  // For debugging and testing.

 private:
  absl::AnyInvocable<std::pair<int64_t, double>(int)> compute_jump_;

  // For each variable, we store:
  // - A jump delta which represents a change in variable value:
  //   new_value = old + delta, which is non-zero except for fixed variables.
  // - The associated weighted feasibility violation change if we take this
  //   jump.
  std::vector<int64_t> deltas_;
  std::vector<double> scores_;
  std::vector<bool> needs_recomputation_;
};

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
                        SharedStatistics* shared_stats,
                        SharedStatTables* stat_tables)
      : SubSolver(name, type),
        linear_model_(linear_model),
        params_(params),
        shared_time_limit_(shared_time_limit),
        shared_response_(shared_response),
        shared_bounds_(shared_bounds),
        shared_stats_(shared_stats),
        stat_tables_(stat_tables),
        random_(params_),
        linear_jumps_(
            absl::bind_front(&FeasibilityJumpSolver::ComputeLinearJump, this)),
        general_jumps_(absl::bind_front(
            &FeasibilityJumpSolver::ComputeGeneralJump, this)) {}

  // If VLOG_IS_ON(1), it will export a bunch of statistics.
  ~FeasibilityJumpSolver() override;

  // No synchronization needed for TaskIsAvailable().
  void Synchronize() final {}

  // Shall we delete this subsolver?
  bool IsDone() final {
    if (!model_is_supported_.load()) return true;

    // We are done after the first task is done in the FIRST_SOLUTION mode.
    return type() == SubSolver::FIRST_SOLUTION &&
           shared_response_->first_solution_solvers_should_stop()->load();
  }

  bool TaskIsAvailable() final {
    if (IsDone()) return false;
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

  absl::Span<double> ScanWeights() {
    return absl::MakeSpan(use_compound_moves_ ? compound_weights_ : weights_);
  }
  absl::Span<const double> ScanWeights() const {
    return absl::MakeConstSpan(use_compound_moves_ ? compound_weights_
                                                   : weights_);
  }

  // Returns the weighted violation delta plus epsilon * the objective delta.
  double ComputeScore(absl::Span<const double> weights, int var, int64_t delta,
                      bool linear_only);

  // Computes the optimal value for variable v, considering only the violation
  // of linear constraints.
  std::pair<int64_t, double> ComputeLinearJump(int var);

  // Computes the optimal value for variable v, considering all constraints
  // (assuming violation functions are convex).
  std::pair<int64_t, double> ComputeGeneralJump(int var);

  // Marks all variables whose jump value may have changed due to the last
  // update, except for `changed var`.
  void MarkJumpsThatNeedToBeRecomputed(int changed_var, JumpTable& jumps);

  // Moves.
  bool DoSomeLinearIterations();
  bool DoSomeGeneralIterations();

  // Returns true if an improving move was found.
  bool ScanRelevantVariables(int num_to_scan, JumpTable& jumps, int* var,
                             int64_t* value, double* score);

  // Increases the weight of the currently infeasible constraints.
  // Ensures jumps remains consistent.
  void UpdateViolatedConstraintWeights(JumpTable& jumps);

  void UpdateNumViolatedConstraintsPerVar();

  void RecomputeVarsToScan(JumpTable&);

  // Returns true if it is possible that `var` may have value that reduces
  // weighted violation or improve the objective.
  // Note that this is independent of the actual weights used.
  bool ShouldScan(const JumpTable& jumps, int var) const;
  void AddVarToScan(const JumpTable&, int var);

  // Resets the weights used to find compound moves.
  // Ensures the following invariant holds afterwards:
  // compound_weights[c] = weights_[c] if c is violated, and epsilon *
  // weights_[c] otherwise.
  void ResetChangedCompoundWeights();

  // Returns true if we should push this change to move_.
  // `novelty` is the total discount applied to the score due to using
  // `cumulative_weights_`, should always be positive (modulo floating-point
  // errors).
  bool ShouldExtendCompoundMove(double score, double novelty);

  // Validates each element in num_violated_constraints_per_var_ against
  // evaluator_->ViolatedConstraints.
  bool SlowCheckNumViolatedConstraints() const;

  const LinearModel* linear_model_;
  SatParameters params_;
  ModelSharedTimeLimit* shared_time_limit_;
  SharedResponseManager* shared_response_;
  SharedBoundsManager* shared_bounds_ = nullptr;
  SharedStatistics* shared_stats_;
  SharedStatTables* stat_tables_;
  ModelRandomGenerator random_;

  // Synchronization Booleans.
  //
  // Note that we don't fully support all type of model, and we will abort by
  // setting the model_is_supported_ bool to false when we detect this.
  bool is_initialized_ = false;
  std::atomic<bool> model_is_supported_ = true;
  std::atomic<bool> task_generated_ = false;
  bool time_limit_crossed_ = false;

  std::unique_ptr<LsEvaluator> evaluator_;
  std::vector<Domain> var_domains_;
  std::vector<bool> var_has_two_values_;
  std::vector<bool> var_occurs_in_non_linear_constraint_;

  JumpTable linear_jumps_;
  JumpTable general_jumps_;
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

  // A list of variables that might be relevant to check for improving jumps.
  std::vector<bool> in_vars_to_scan_;
  std::vector<int> vars_to_scan_;

  // We restart each time our local deterministic time crosses this.
  double dtime_restart_threshold_ = 0.0;
  int64_t update_restart_threshold_ = 0;
  int num_batches_before_perturbation_;

  std::vector<int64_t> tmp_breakpoints_;

  // Each time we reset the weights, randomly change this to update them with
  // decay or not.
  bool use_decay_ = true;

  // Each time we reset the weights, randomly decide if we will use compound
  // moves or not.
  bool use_compound_moves_ = false;

  // Limit the discrepancy in compound move search (i.e. limit the number of
  // backtracks to any ancestor of the current leaf). This is set to 0 whenever
  // a new incumbent is found or weights are updated, and increased at fixed
  // point.
  // Weights are only increased if no moves are found with discrepancy 2.
  // Empirically we have seen very few moves applied with discrepancy > 2.
  int compound_move_max_discrepancy_ = 0;

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
  int64_t num_scores_computed_ = 0;

  std::unique_ptr<CompoundMoveBuilder> move_;

  // Counts the number of violated constraints each var is in.
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

  double BestChildScore() const {
    return stack_.empty() ? 0.0 : stack_.back().best_child_score;
  }

  // Returns the number of backtracks to any ancestor of the current leaf.
  int Discrepancy() const {
    return stack_.empty() ? 0 : stack_.back().discrepancy;
  }

  // Returns the number of backtracking moves that have been applied.
  int NumBacktracks() const { return num_backtracks_; }

  // Returns true if all prev_values on the stack are in the appropriate domain.
  bool StackValuesInDomains(absl::Span<const Domain> var_domains) const;

 private:
  struct UnitMove {
    int var;
    int64_t prev_value;
    // Note that this stores the score of reverting to prev_value.
    double score;
    // Instead of tracking this on the compound move, we store the partial sums
    // to avoid numerical issues causing negative scores after backtracking.
    double cumulative_score;

    // Used to avoid infinite loops, this tracks the best score of any immediate
    // children (and not deeper descendants) to avoid re-exploring the same
    // prefix.
    double best_child_score = 0.0;
    int discrepancy = 0;
  };
  LsEvaluator* evaluator_;
  std::vector<bool> var_on_stack_;
  std::vector<UnitMove> stack_;

  int64_t num_backtracks_ = 0;
};

}  // namespace operations_research::sat

#endif  // OR_TOOLS_SAT_FEASIBILITY_JUMP_H_
