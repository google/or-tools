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

#ifndef ORTOOLS_SAT_FEASIBILITY_JUMP_H_
#define ORTOOLS_SAT_FEASIBILITY_JUMP_H_

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/sat/constraint_violation.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_model.h"
#include "ortools/sat/restart.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/stat_tables.h"
#include "ortools/sat/subsolver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

namespace operations_research::sat {

class CompoundMoveBuilder;

// This class lazily caches the results of `compute_jump(var)` which returns a
// <delta, score> pair.
// Variables' scores can be manually modified using MutableScores (if the
// optimal jump is known not to change), or marked for recomputation on the next
// call to GetJump(var) by calling Recompute.
class JumpTable {
 public:
  JumpTable() = default;

  void SetComputeFunction(
      absl::AnyInvocable<std::pair<int64_t, double>(int) const> compute_jump);

  void RecomputeAll(int num_variables);

  // Gets the current jump delta and score, recomputing if necessary.
  std::pair<int64_t, double> GetJump(int var);

  // If the new optimum value and score is known, users can update it directly.
  // e.g. after weight rescaling, or after changing a binary variable.
  void SetJump(int var, int64_t delta, double score);

  // Recompute the jump for `var` when `GetJump(var)` is next called.
  void Recompute(int var);

  bool NeedRecomputation(int var) const { return needs_recomputation_[var]; }

  double Score(int var) const { return scores_[var]; }

  // Advanced usage, allows users to read possibly stale deltas for incremental
  // score updates.
  absl::Span<const int64_t> Deltas() const {
    return absl::MakeConstSpan(deltas_);
  }
  absl::Span<const double> Scores() const {
    return absl::MakeConstSpan(scores_);
  }

  absl::Span<double> MutableScores() { return absl::MakeSpan(scores_); }

  // For debugging and testing.
  //
  // Note if you have very high weights (e.g. when using decay), the tolerances
  // in this function are likely too tight.
  bool JumpIsUpToDate(int var) const;

 private:
  absl::AnyInvocable<std::pair<int64_t, double>(int) const> compute_jump_;

  // For each variable, we store:
  // - A jump delta which represents a change in variable value:
  //   new_value = old + delta, which is non-zero except for fixed variables.
  // - The associated weighted feasibility violation change if we take this
  //   jump.
  std::vector<int64_t> deltas_;
  std::vector<double> scores_;
  std::vector<bool> needs_recomputation_;
};

// Accessing Domain can be expensive, so we maintain vector of bool for the
// hot spots.
class VarDomainWrapper {
 public:
  explicit VarDomainWrapper(SharedBoundsManager* shared_bounds)
      : shared_bounds_id_(
            shared_bounds == nullptr ? 0 : shared_bounds->RegisterNewId()),
        shared_bounds_(shared_bounds) {}

  Domain operator[](int var) const { return domains_[var]; }
  bool HasTwoValues(int var) const { return has_two_values_[var]; }
  size_t size() const { return domains_.size(); }

  void resize(int num_vars) {
    domains_.resize(num_vars);
    has_two_values_.resize(num_vars);
    is_fixed_.resize(num_vars, false);
    objective_is_positive_.resize(num_vars, false);
    objective_is_negative_.resize(num_vars, false);
    has_better_objective_value_.resize(num_vars, false);
  }

  void Set(int var, Domain d) {
    has_two_values_[var] = d.HasTwoValues();
    if (is_fixed_[var]) {
      // The code here assume that once fixed, a variable stays that way.
      CHECK(d.IsFixed());
    } else if (d.IsFixed()) {
      is_fixed_[var] = true;
      fixed_vars_.push_back(var);
    }
    domains_[var] = std::move(d);
  }

  // Return false if one of the domain becomes empty (UNSAT). This might happen
  // while we are cleaning up all workers at the end of a search.
  bool UpdateFromSharedBounds() {
    if (shared_bounds_ == nullptr) return true;
    shared_bounds_->GetChangedBounds(shared_bounds_id_, &tmp_variables_,
                                     &tmp_new_lower_bounds_,
                                     &tmp_new_upper_bounds_);
    for (int i = 0; i < tmp_variables_.size(); ++i) {
      const int var = tmp_variables_[i];
      const Domain new_domain = domains_[var].IntersectionWith(
          Domain(tmp_new_lower_bounds_[i], tmp_new_upper_bounds_[i]));
      if (new_domain.IsEmpty()) return false;
      Set(var, new_domain);
    }
    return true;
  }

  absl::Span<const Domain> AsSpan() const { return domains_; }

  void InitializeObjective(const CpModelProto& cp_model_proto) {
    if (!cp_model_proto.has_objective()) return;
    const int num_terms = cp_model_proto.objective().vars().size();
    for (int i = 0; i < num_terms; ++i) {
      const int var = cp_model_proto.objective().vars(i);
      const int coeff = cp_model_proto.objective().coeffs(i);
      objective_is_positive_[var] = coeff > 0;
      objective_is_negative_[var] = coeff < 0;
    }
  }

  bool IsFixed(int var) const { return is_fixed_[var]; }

  bool HasBetterObjectiveValue(int var) const {
    return has_better_objective_value_[var];
  }

  // Tricky: this must be called on solution value change or domains update.
  void OnValueChange(int var, int64_t value) {
    has_better_objective_value_[var] =
        (objective_is_positive_[var] && value > domains_[var].Min()) ||
        (objective_is_negative_[var] && value < domains_[var].Max());
  }

  absl::Span<const int> FixedVariables() const { return fixed_vars_; }

 private:
  const int shared_bounds_id_;
  SharedBoundsManager* shared_bounds_;

  // Basically fixed once and for all.
  std::vector<bool> objective_is_positive_;
  std::vector<bool> objective_is_negative_;

  // Depends on domain updates.
  std::vector<Domain> domains_;
  std::vector<bool> has_two_values_;
  std::vector<bool> is_fixed_;
  std::vector<int> fixed_vars_;

  // This is the only one that depends on the current solution value.
  std::vector<bool> has_better_objective_value_;

  // Temporary data for UpdateFromSharedBounds()
  std::vector<int> tmp_variables_;
  std::vector<int64_t> tmp_new_lower_bounds_;
  std::vector<int64_t> tmp_new_upper_bounds_;
};

// Local search counters. This can either be the stats of one run without
// restart or some aggregation of such runs.
struct LsCounters {
  int64_t num_batches = 0;
  int64_t num_perturbations = 0;
  int64_t num_linear_evals = 0;
  int64_t num_linear_moves = 0;
  int64_t num_general_evals = 0;
  int64_t num_general_moves = 0;
  int64_t num_backtracks = 0;
  int64_t num_compound_moves = 0;
  int64_t num_weight_updates = 0;
  int64_t num_scores_computed = 0;

  void AddFrom(const LsCounters& o) {
    num_batches += o.num_batches;
    num_perturbations += o.num_perturbations;
    num_linear_evals += o.num_linear_evals;
    num_linear_moves += o.num_linear_moves;
    num_general_evals += o.num_general_evals;
    num_general_moves += o.num_general_moves;
    num_backtracks += o.num_backtracks;
    num_compound_moves += o.num_compound_moves;
    num_weight_updates += o.num_weight_updates;
    num_scores_computed += o.num_scores_computed;
  }
};

// The parameters used by the local search code.
struct LsOptions {
  // This one never changes.
  // - If true, each restart is independent from the other. This is nice because
  //   it plays well with the theoretical Luby restart sequence.
  // - If false, we always "restart" from the current state, but we perturb it
  //   or just reset the constraint weight. We currently use this one way less
  //   often.
  bool use_restart = true;

  // These are randomized each restart by Randomize().
  double perturbation_probability = 0.0;
  bool use_decay = true;
  bool use_compound_moves = true;
  bool use_objective = true;  // No effect if there are no objective.

  // Allows to identify which options worked well.
  std::string name() const {
    std::vector<absl::string_view> parts;
    parts.reserve(5);
    if (use_restart) parts.push_back("restart");
    if (use_decay) parts.push_back("decay");
    if (use_compound_moves) parts.push_back("compound");
    if (perturbation_probability > 0) parts.push_back("perturb");
    if (use_objective) parts.push_back("obj");
    return absl::StrJoin(parts, "_");
  }

  // In order to collect statistics by options.
  template <typename H>
  friend H AbslHashValue(H h, const LsOptions& o) {
    return H::combine(std::move(h), o.use_restart, o.perturbation_probability,
                      o.use_decay, o.use_compound_moves, o.use_objective);
  }

  bool operator==(const LsOptions& o) const {
    return use_restart == o.use_restart &&
           perturbation_probability == o.perturbation_probability &&
           use_decay == o.use_decay &&
           use_compound_moves == o.use_compound_moves &&
           use_objective == o.use_objective;
  }

  void Randomize(const SatParameters& params, ModelRandomGenerator* random) {
    perturbation_probability =
        absl::Bernoulli(*random, 0.5)
            ? 0.0
            : params.feasibility_jump_var_randomization_probability();
    use_decay = absl::Bernoulli(*random, 0.5);
    use_compound_moves = absl::Bernoulli(*random, 0.5);
    use_objective = absl::Bernoulli(*random, 0.5);
  }
};

// Each FeasibilityJumpSolver work on many LsState in an interleaved parallel
// fashion. Each "batch of moves" will update one of these state. Restart
// heuristic are also on a per state basis.
//
// This allows to not use O(problem size) per state while having a more
// diverse set of heuristics.
struct LsState {
  // The score of a solution is just the sum of infeasibility of each
  // constraint weighted by these weights.
  std::vector<int64_t> solution;
  std::vector<double> weights;
  std::shared_ptr<const SharedSolutionRepository<int64_t>::Solution>
      base_solution;

  // Depending on the options, we use an exponentially decaying constraint
  // weight like for SAT activities.
  double bump_value = 1.0;

  // If using compound moves, these will be discounted on a new incumbent then
  // re-converge to weights after some exploration. Search will repeatedly pick
  // moves with negative WeightedViolationDelta using these weights.
  //
  // We limit the discrepancy in compound move search (i.e. limit the number of
  // backtracks to any ancestor of the current leaf). This is set to 0 whenever
  // a new incumbent is found or weights are updated, and increased at fixed
  // point. Weights are only increased if no moves are found with discrepancy 2.
  // Empirically we have seen very few moves applied with discrepancy > 2.
  int compound_move_max_discrepancy = 0;
  std::vector<double> compound_weights;
  std::vector<bool> in_compound_weight_changed;
  std::vector<int> compound_weight_changed;
  std::unique_ptr<CompoundMoveBuilder> move;

  // Counters for a "non-restarted" run.
  LsCounters counters;

  // Strategy
  LsOptions options;

  // Global counters, incremented across restart.
  int64_t num_restarts = 0;
  int64_t num_solutions_imported = 0;

  // When this reach zero, we restart / perturbate or trigger something.
  int64_t num_batches_before_change = 0;

  // Used by LS to know the rank of the starting solution for this state.
  int64_t last_solution_rank = std::numeric_limits<int64_t>::max();

  // Tricky: If this changed since last time, we need to recompute the
  // compound moves as the objective constraint bound changed.
  IntegerValue saved_inner_objective_lb = 0;
  IntegerValue saved_inner_objective_ub = 0;
};

// Shared set of local search states that we work on.
class SharedLsStates {
 public:
  // Important: max_parallelism should be greater or equal than the actual
  // number of thread sharing this class, otherwise the code will break.
  SharedLsStates(absl::string_view name, const SatParameters& params,
                 SharedStatTables* stat_tables)
      : name_(name), params_(params), stat_tables_(stat_tables) {
    // We always start with at least 8 states.
    // We will create more if there are more parallel workers as needed.
    for (int i = 0; i < 8; ++i) CreateNewState();
  }

  ~SharedLsStates();

  void CreateNewState() {
    const int index = states_.size();
    states_.emplace_back(new LsState());
    taken_.push_back(false);
    num_selected_.push_back(0);

    // We add one no-restart per 16 states and put it last.
    states_.back()->options.use_restart = (index % 16 != 15);
  }

  // Returns the next available state in round-robin fashion.
  // This is thread safe. If we respect the max_parallelism guarantee, then
  // all states should be independent.
  LsState* GetNextState() {
    absl::MutexLock mutex_lock(mutex_);
    int next = -1;
    const int num_states = states_.size();
    for (int i = 0; i < num_states; ++i) {
      const int index = round_robin_index_;
      round_robin_index_ = (round_robin_index_ + 1) % num_states;
      if (taken_[index]) continue;
      if (next == -1 || num_selected_[index] < num_selected_[next]) {
        next = index;
      }
    }

    if (next == -1) {
      // We need more parallelism and create a new state.
      next = num_states;
      CreateNewState();
    }

    --states_[next]->num_batches_before_change;
    taken_[next] = true;
    num_selected_[next]++;
    return states_[next].get();
  }

  void Release(LsState* state) {
    absl::MutexLock mutex_lock(mutex_);
    for (int i = 0; i < states_.size(); ++i) {
      if (state == states_[i].get()) {
        taken_[i] = false;
        break;
      }
    }
  }

  void ResetLubyCounter() {
    absl::MutexLock mutex_lock(mutex_);
    luby_counter_ = 0;
  }

  // We share a global running Luby sequence for all the "restart" state.
  // Note that we randomize the parameters on each restart.
  //
  // Hack: options.use_restart is constant, so we are free to inspect it.
  // Also if options.use_restart, then num_batches_before_change is only
  // modified under lock, so this code should be thread safe.
  void ConfigureNextLubyRestart(LsState* state) {
    absl::MutexLock mutex_lock(mutex_);
    const int factor = std::max(1, params_.feasibility_jump_restart_factor());
    CHECK(state->options.use_restart);
    const int64_t next = factor * SUniv(++luby_counter_);
    state->num_batches_before_change = next;
  }

  // Accumulate in the relevant bucket the counters of the given states.
  void CollectStatistics(const LsState& state) {
    if (state.counters.num_batches == 0) return;

    absl::MutexLock mutex_lock(mutex_);
    options_to_stats_[state.options].AddFrom(state.counters);
    options_to_num_restarts_[state.options]++;
  }

 private:
  const std::string name_;
  const SatParameters& params_;
  SharedStatTables* stat_tables_;

  mutable absl::Mutex mutex_;
  int round_robin_index_ = 0;
  std::vector<std::unique_ptr<LsState>> states_;
  std::vector<bool> taken_;
  std::vector<int> num_selected_;
  int luby_counter_ = 0;

  absl::flat_hash_map<LsOptions, LsCounters> options_to_stats_;
  absl::flat_hash_map<LsOptions, int> options_to_num_restarts_;
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
  FeasibilityJumpSolver(const absl::string_view name,
                        SubSolver::SubsolverType type,
                        const LinearModel* linear_model, SatParameters params,
                        std::shared_ptr<SharedLsStates> ls_states,
                        ModelSharedTimeLimit* shared_time_limit,
                        SharedResponseManager* shared_response,
                        SharedBoundsManager* shared_bounds,
                        SharedLsSolutionRepository* shared_hints,
                        SharedStatistics* shared_stats,
                        SharedStatTables* stat_tables)
      : SubSolver(name, type),
        linear_model_(linear_model),
        params_(params),
        states_(std::move(ls_states)),
        shared_time_limit_(shared_time_limit),
        shared_response_(shared_response),
        shared_hints_(shared_hints),
        stat_tables_(stat_tables),
        random_(params_),
        var_domains_(shared_bounds) {
    shared_time_limit_->UpdateLocalLimit(&time_limit_);
  }

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

    return shared_response_->HasFeasibleSolution() ==
           (type() == SubSolver::INCOMPLETE);
  }

  std::function<void()> GenerateTask(int64_t /*task_id*/) final;

 private:
  void ImportState();
  void ReleaseState();

  // Return false if we could not initialize the evaluator in the time limit.
  bool Initialize();
  void ResetCurrentSolution(bool use_hint, bool use_objective,
                            double perturbation_probability);
  void PerturbateCurrentSolution(double perturbation_probability);
  std::string OneLineStats() const;

  absl::Span<const double> ScanWeights() const {
    return absl::MakeConstSpan(state_->options.use_compound_moves
                                   ? state_->compound_weights
                                   : state_->weights);
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
  void MarkJumpsThatNeedToBeRecomputed(int changed_var);

  // Moves.
  bool DoSomeLinearIterations();
  bool DoSomeGeneralIterations();

  // Returns true if an improving move was found.
  bool ScanRelevantVariables(int num_to_scan, int* var, int64_t* value,
                             double* score);

  // Increases the weight of the currently infeasible constraints.
  // Ensures jumps remains consistent.
  void UpdateViolatedConstraintWeights();

  // Returns true if it is possible that `var` may have value that reduces
  // weighted violation or improve the objective.
  // Note that this is independent of the actual weights used.
  bool ShouldScan(int var) const;

  void AddVarToScan(int var);
  void RecomputeVarsToScan();

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

  double DeterministicTime() const {
    return evaluator_->DeterministicTime() + num_ops_ * 1e-8;
  }

  const LinearModel* linear_model_;
  SatParameters params_;
  std::shared_ptr<SharedLsStates> states_;
  ModelSharedTimeLimit* shared_time_limit_;
  TimeLimit time_limit_;
  SharedResponseManager* shared_response_;
  SharedLsSolutionRepository* shared_hints_;
  SharedStatTables* stat_tables_;
  ModelRandomGenerator random_;

  VarDomainWrapper var_domains_;

  // Synchronization Booleans.
  //
  // Note that we don't fully support all type of model, and we will abort by
  // setting the model_is_supported_ bool to false when we detect this.
  bool is_initialized_ = false;
  std::atomic<bool> model_is_supported_ = true;
  std::atomic<bool> task_generated_ = false;
  bool time_limit_crossed_ = false;

  std::unique_ptr<LsEvaluator> evaluator_;
  std::vector<bool> var_occurs_in_non_linear_constraint_;

  JumpTable jumps_;
  std::vector<double> for_weight_update_;

  // The current sate we work on.
  LsState* state_;

  // A list of variables that might be relevant to check for improving jumps.
  std::vector<bool> in_vars_to_scan_;
  FixedCapacityVector<int> vars_to_scan_;

  std::vector<int64_t> tmp_breakpoints_;

  // For counting the dtime. See DeterministicTime().
  int64_t num_ops_ = 0;
};

// This class helps keep track of moves that change more than one variable.
// Mainly this class keeps track of how to backtrack back to the local minimum
// as you make a sequence of exploratory moves, so in order to commit a compound
// move, you just need to call `Clear` instead of Backtracking over the changes.
class CompoundMoveBuilder {
 public:
  explicit CompoundMoveBuilder(int num_variables)
      : var_on_stack_(num_variables, false) {}

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
  std::vector<bool> var_on_stack_;
  std::vector<UnitMove> stack_;
};

}  // namespace operations_research::sat

#endif  // ORTOOLS_SAT_FEASIBILITY_JUMP_H_
