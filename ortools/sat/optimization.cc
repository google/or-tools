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

#include "ortools/sat/optimization.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/boolean_problem.pb.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/encoding.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

void MinimizeCoreWithPropagation(TimeLimit* limit, SatSolver* solver,
                                 std::vector<Literal>* core) {
  if (solver->ModelIsUnsat()) return;
  absl::btree_set<LiteralIndex> moved_last;
  std::vector<Literal> candidate(core->begin(), core->end());

  solver->Backtrack(0);
  solver->SetAssumptionLevel(0);
  if (!solver->FinishPropagation()) return;
  while (!limit->LimitReached()) {
    // We want each literal in candidate to appear last once in our propagation
    // order. We want to do that while maximizing the reutilization of the
    // current assignment prefix, that is minimizing the number of
    // decision/progagation we need to perform.
    const int target_level = MoveOneUnprocessedLiteralLast(
        moved_last, solver->CurrentDecisionLevel(), &candidate);
    if (target_level == -1) break;
    solver->Backtrack(target_level);
    while (!solver->ModelIsUnsat() && !limit->LimitReached() &&
           solver->CurrentDecisionLevel() < candidate.size()) {
      const Literal decision = candidate[solver->CurrentDecisionLevel()];
      if (solver->Assignment().LiteralIsTrue(decision)) {
        candidate.erase(candidate.begin() + solver->CurrentDecisionLevel());
        continue;
      } else if (solver->Assignment().LiteralIsFalse(decision)) {
        // This is a "weird" API to get the subset of decisions that caused
        // this literal to be false with reason analysis.
        solver->EnqueueDecisionAndBacktrackOnConflict(decision);
        candidate = solver->GetLastIncompatibleDecisions();
        break;
      } else {
        solver->EnqueueDecisionAndBackjumpOnConflict(decision);
      }
    }
    if (candidate.empty() || solver->ModelIsUnsat()) return;
    moved_last.insert(candidate.back().Index());
  }

  solver->Backtrack(0);
  solver->SetAssumptionLevel(0);
  if (candidate.size() < core->size()) {
    VLOG(1) << "minimization with propag " << core->size() << " -> "
            << candidate.size();

    // We want to preserve the order of literal in the response.
    absl::flat_hash_set<LiteralIndex> set;
    for (const Literal l : candidate) set.insert(l.Index());
    int new_size = 0;
    for (const Literal l : *core) {
      if (set.contains(l.Index())) {
        (*core)[new_size++] = l;
      }
    }
    core->resize(new_size);
  }
}

void MinimizeCoreWithSearch(TimeLimit* limit, SatSolver* solver,
                            std::vector<Literal>* core) {
  if (solver->ModelIsUnsat()) return;

  // TODO(user): tune.
  if (core->size() > 100 || core->size() == 1) return;

  const bool old_log_state = solver->mutable_logger()->LoggingIsEnabled();
  solver->mutable_logger()->EnableLogging(false);

  const int old_size = core->size();
  std::vector<Literal> assumptions;
  absl::flat_hash_set<LiteralIndex> removed_once;
  while (true) {
    if (limit->LimitReached()) break;

    // Find a not yet removed literal to remove.
    // We prefer to remove high indices since these are more likely to be of
    // high depth.
    //
    // TODO(user): Properly use the node depth instead.
    int to_remove = -1;
    for (int i = core->size(); --i >= 0;) {
      const Literal l = (*core)[i];
      const auto [_, inserted] = removed_once.insert(l.Index());
      if (inserted) {
        to_remove = i;
        break;
      }
    }
    if (to_remove == -1) break;

    assumptions.clear();
    for (int i = 0; i < core->size(); ++i) {
      if (i == to_remove) continue;
      assumptions.push_back((*core)[i]);
    }

    const auto status = solver->ResetAndSolveWithGivenAssumptions(
        assumptions, /*max_number_of_conflicts=*/1000);
    if (status == SatSolver::ASSUMPTIONS_UNSAT) {
      *core = solver->GetLastIncompatibleDecisions();
    }
  }

  if (core->size() < old_size) {
    VLOG(1) << "minimization with search " << old_size << " -> "
            << core->size();
  }

  (void)solver->ResetToLevelZero();
  solver->mutable_logger()->EnableLogging(old_log_state);
}

bool ProbeLiteral(Literal assumption, SatSolver* solver) {
  if (solver->ModelIsUnsat()) return false;

  const bool old_log_state = solver->mutable_logger()->LoggingIsEnabled();
  solver->mutable_logger()->EnableLogging(false);

  // Note that since we only care about Booleans here, even if we have a
  // feasible solution, it might not be feasible for the full cp_model.
  //
  // TODO(user): Still use it if the problem is Boolean only.
  const auto status = solver->ResetAndSolveWithGivenAssumptions(
      {assumption}, /*max_number_of_conflicts=*/1'000);
  if (!solver->ResetToLevelZero()) return false;
  if (status == SatSolver::ASSUMPTIONS_UNSAT) {
    if (!solver->AddUnitClause(assumption.Negated())) {
      return false;
    }
    if (!solver->Propagate()) {
      solver->NotifyThatModelIsUnsat();
      return false;
    }
  }

  solver->mutable_logger()->EnableLogging(old_log_state);
  return solver->Assignment().LiteralIsAssigned(assumption);
}

// A core cannot be all true.
void FilterAssignedLiteral(const VariablesAssignment& assignment,
                           std::vector<Literal>* core) {
  int new_size = 0;
  for (const Literal lit : *core) {
    if (assignment.LiteralIsTrue(lit)) continue;
    if (assignment.LiteralIsFalse(lit)) {
      (*core)[0] = lit;
      core->resize(1);
      return;
    }
    (*core)[new_size++] = lit;
  }
  core->resize(new_size);
}

SatSolver::Status MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
    IntegerVariable objective_var,
    const std::function<void()>& feasible_solution_observer, Model* model) {
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* search = model->GetOrCreate<IntegerSearchHelper>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());

  // Simple linear scan algorithm to find the optimal.
  if (!sat_solver->ResetToLevelZero()) return SatSolver::INFEASIBLE;
  while (true) {
    const SatSolver::Status result = search->SolveIntegerProblem();
    if (result != SatSolver::FEASIBLE) return result;

    // The objective is the current lower bound of the objective_var.
    const IntegerValue objective = integer_trail->LowerBound(objective_var);

    // We have a solution!
    if (feasible_solution_observer != nullptr) {
      feasible_solution_observer();
    }
    if (parameters.stop_after_first_solution()) {
      return SatSolver::LIMIT_REACHED;
    }

    // Restrict the objective.
    sat_solver->Backtrack(0);
    if (!integer_trail->Enqueue(
            IntegerLiteral::LowerOrEqual(objective_var, objective - 1), {},
            {})) {
      return SatSolver::INFEASIBLE;
    }
  }
}

void RestrictObjectiveDomainWithBinarySearch(
    IntegerVariable objective_var,
    const std::function<void()>& feasible_solution_observer, Model* model) {
  const SatParameters old_params = *model->GetOrCreate<SatParameters>();
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntegerEncoder* integer_encoder = model->GetOrCreate<IntegerEncoder>();

  // Set the requested conflict limit.
  {
    SatParameters new_params = old_params;
    new_params.set_max_number_of_conflicts(
        old_params.binary_search_num_conflicts());
    *model->GetOrCreate<SatParameters>() = new_params;
  }

  // The assumption (objective <= value) for values in
  // [unknown_min, unknown_max] reached the conflict limit.
  bool loop = true;
  IntegerValue unknown_min = integer_trail->UpperBound(objective_var);
  IntegerValue unknown_max = integer_trail->LowerBound(objective_var);
  while (loop) {
    sat_solver->Backtrack(0);
    const IntegerValue lb = integer_trail->LowerBound(objective_var);
    const IntegerValue ub = integer_trail->UpperBound(objective_var);
    unknown_min = std::min(unknown_min, ub);
    unknown_max = std::max(unknown_max, lb);

    // We first refine the lower bound and then the upper bound.
    IntegerValue target;
    if (lb < unknown_min) {
      target = lb + (unknown_min - lb) / 2;
    } else if (unknown_max < ub) {
      target = ub - (ub - unknown_max) / 2;
    } else {
      VLOG(1) << "Binary-search, done.";
      break;
    }
    VLOG(1) << "Binary-search, objective: [" << lb << "," << ub << "]"
            << " tried: [" << unknown_min << "," << unknown_max << "]"
            << " target: obj<=" << target;
    SatSolver::Status result;
    if (target < ub) {
      const Literal assumption = integer_encoder->GetOrCreateAssociatedLiteral(
          IntegerLiteral::LowerOrEqual(objective_var, target));
      result = ResetAndSolveIntegerProblem({assumption}, model);
    } else {
      result = ResetAndSolveIntegerProblem({}, model);
    }

    switch (result) {
      case SatSolver::INFEASIBLE: {
        loop = false;
        break;
      }
      case SatSolver::ASSUMPTIONS_UNSAT: {
        // Update the objective lower bound.
        sat_solver->Backtrack(0);
        if (!integer_trail->Enqueue(
                IntegerLiteral::GreaterOrEqual(objective_var, target + 1), {},
                {})) {
          loop = false;
        }
        break;
      }
      case SatSolver::FEASIBLE: {
        // The objective is the current lower bound of the objective_var.
        const IntegerValue objective = integer_trail->LowerBound(objective_var);
        if (feasible_solution_observer != nullptr) {
          feasible_solution_observer();
        }

        // We have a solution, restrict the objective upper bound to only look
        // for better ones now.
        sat_solver->Backtrack(0);
        if (!integer_trail->Enqueue(
                IntegerLiteral::LowerOrEqual(objective_var, objective - 1), {},
                {})) {
          loop = false;
        }
        break;
      }
      case SatSolver::LIMIT_REACHED: {
        unknown_min = std::min(target, unknown_min);
        unknown_max = std::max(target, unknown_max);
        break;
      }
    }
  }

  sat_solver->Backtrack(0);
  *model->GetOrCreate<SatParameters>() = old_params;
}

namespace {

// If the given model is unsat under the given assumptions, returns one or more
// non-overlapping set of assumptions, each set making the problem infeasible on
// its own (the cores).
//
// In presence of weights, we "generalize" the notions of disjoints core using
// the WCE idea describe in "Weight-Aware Core Extraction in SAT-Based MaxSAT
// solving" Jeremias Berg And Matti Jarvisalo.
//
// The returned status can be either:
// - ASSUMPTIONS_UNSAT if the set of returned core perfectly cover the given
//   assumptions, in this case, we don't bother trying to find a SAT solution
//   with no assumptions.
// - FEASIBLE if after finding zero or more core we have a solution.
// - LIMIT_REACHED if we reached the time-limit before one of the two status
//   above could be decided.
//
// TODO(user): There is many way to combine the WCE and stratification
// heuristics. I didn't had time to properly compare the different approach. See
// the WCE papers for some ideas, but there is many more ways to try to find a
// lot of core at once and try to minimize the minimum weight of each of the
// cores.
SatSolver::Status FindCores(std::vector<Literal> assumptions,
                            std::vector<IntegerValue> assumption_weights,
                            IntegerValue stratified_threshold, Model* model,
                            std::vector<std::vector<Literal>>* cores) {
  cores->clear();
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  TimeLimit* limit = model->GetOrCreate<TimeLimit>();
  do {
    if (limit->LimitReached()) return SatSolver::LIMIT_REACHED;

    const SatSolver::Status result =
        ResetAndSolveIntegerProblem(assumptions, model);
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;
    std::vector<Literal> core = sat_solver->GetLastIncompatibleDecisions();
    if (sat_solver->parameters().core_minimization_level() > 0) {
      MinimizeCoreWithPropagation(limit, sat_solver, &core);
    }
    if (core.size() == 1) {
      if (!sat_solver->AddUnitClause(core[0].Negated())) {
        return SatSolver::INFEASIBLE;
      }
    }
    if (core.empty()) return sat_solver->UnsatStatus();
    cores->push_back(core);
    if (!sat_solver->parameters().find_multiple_cores()) break;

    // Recover the original indices of the assumptions that are part of the
    // core.
    std::vector<int> indices;
    {
      absl::btree_set<Literal> temp(core.begin(), core.end());
      for (int i = 0; i < assumptions.size(); ++i) {
        if (temp.contains(assumptions[i])) {
          indices.push_back(i);
        }
      }
    }

    // Remove min_weight from the weights of all the assumptions in the core.
    //
    // TODO(user): push right away the objective bound by that much? This should
    // be better in a multi-threading context as we can share more quickly the
    // better bound.
    IntegerValue min_weight = assumption_weights[indices.front()];
    for (const int i : indices) {
      min_weight = std::min(min_weight, assumption_weights[i]);
    }
    for (const int i : indices) {
      assumption_weights[i] -= min_weight;
    }

    // Remove from assumptions all the one with a new weight smaller than the
    // current stratification threshold and see if we can find another core.
    int new_size = 0;
    for (int i = 0; i < assumptions.size(); ++i) {
      if (assumption_weights[i] < stratified_threshold) continue;
      assumptions[new_size] = assumptions[i];
      assumption_weights[new_size] = assumption_weights[i];
      ++new_size;
    }
    assumptions.resize(new_size);
    assumption_weights.resize(new_size);
  } while (!assumptions.empty());
  return SatSolver::ASSUMPTIONS_UNSAT;
}

}  // namespace

CoreBasedOptimizer::CoreBasedOptimizer(
    IntegerVariable objective_var, absl::Span<const IntegerVariable> variables,
    absl::Span<const IntegerValue> coefficients,
    std::function<void()> feasible_solution_observer, Model* model)
    : parameters_(model->GetOrCreate<SatParameters>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      clauses_(model->GetOrCreate<ClauseManager>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      implications_(model->GetOrCreate<BinaryImplicationGraph>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
      model_(model),
      objective_var_(objective_var),
      feasible_solution_observer_(std::move(feasible_solution_observer)) {
  CHECK_EQ(variables.size(), coefficients.size());
  for (int i = 0; i < variables.size(); ++i) {
    if (coefficients[i] > 0) {
      terms_.push_back({variables[i], coefficients[i]});
    } else if (coefficients[i] < 0) {
      terms_.push_back({NegationOf(variables[i]), -coefficients[i]});
    } else {
      continue;  // coefficients[i] == 0
    }
    terms_.back().depth = 0;
  }

  // This is used by the "stratified" approach. We will only consider terms with
  // a weight not lower than this threshold. The threshold will decrease as the
  // algorithm progress.
  stratification_threshold_ = parameters_->max_sat_stratification() ==
                                      SatParameters::STRATIFICATION_NONE
                                  ? IntegerValue(1)
                                  : kMaxIntegerValue;
}

bool CoreBasedOptimizer::ProcessSolution() {
  // We don't assume that objective_var is linked with its linear term, so
  // we recompute the objective here.
  IntegerValue objective(0);
  for (ObjectiveTerm& term : terms_) {
    const IntegerValue value = integer_trail_->LowerBound(term.var);
    objective += term.weight * value;

    // Also keep in term.cover_ub the minimum value for term.var that we have
    // seens amongst all the feasible solutions found so far.
    term.cover_ub = std::min(term.cover_ub, value);
  }

  // Test that the current objective value fall in the requested objective
  // domain, which could potentially have holes.
  if (!integer_trail_->InitialVariableDomain(objective_var_)
           .Contains(objective.value())) {
    return true;
  }

  if (feasible_solution_observer_ != nullptr) {
    feasible_solution_observer_();
  }
  if (parameters_->stop_after_first_solution()) {
    stop_ = true;
  }

  // Constrain objective_var. This has a better result when objective_var is
  // used in an LP relaxation for instance.
  sat_solver_->Backtrack(0);
  sat_solver_->SetAssumptionLevel(0);
  return integer_trail_->Enqueue(
      IntegerLiteral::LowerOrEqual(objective_var_, objective - 1), {}, {});
}

bool CoreBasedOptimizer::PropagateObjectiveBounds() {
  // We assumes all terms (modulo stratification) at their lower-bound.
  bool some_bound_were_tightened = true;
  while (some_bound_were_tightened) {
    some_bound_were_tightened = false;
    if (!sat_solver_->ResetToLevelZero()) return false;
    if (time_limit_->LimitReached()) return true;

    // Compute implied lb.
    IntegerValue implied_objective_lb(0);
    for (ObjectiveTerm& term : terms_) {
      const IntegerValue var_lb = integer_trail_->LowerBound(term.var);
      term.old_var_lb = var_lb;
      implied_objective_lb += term.weight * var_lb.value();
    }

    // Update the objective lower bound with our current bound.
    if (implied_objective_lb > integer_trail_->LowerBound(objective_var_)) {
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(
                                       objective_var_, implied_objective_lb),
                                   {}, {})) {
        return false;
      }

      some_bound_were_tightened = true;
    }

    // The gap is used to propagate the upper-bound of all variable that are
    // in the current objective (Exactly like done in the propagation of a
    // linear constraint with the slack). When this fix a variable to its
    // lower bound, it is called "hardening" in the max-sat literature. This
    // has a really beneficial effect on some weighted max-sat problems like
    // the haplotyping-pedigrees ones.
    const IntegerValue gap =
        integer_trail_->UpperBound(objective_var_) - implied_objective_lb;

    for (const ObjectiveTerm& term : terms_) {
      if (term.weight == 0) continue;
      const IntegerValue var_lb = integer_trail_->LowerBound(term.var);
      const IntegerValue var_ub = integer_trail_->UpperBound(term.var);
      if (var_lb == var_ub) continue;

      // Hardening. This basically just propagate the implied upper bound on
      // term.var from the current best solution. Note that the gap is
      // non-negative and the weight positive here. The test is done in order
      // to avoid any integer overflow provided (ub - lb) do not overflow, but
      // this is a precondition in our cp-model.
      if (gap / term.weight < var_ub - var_lb) {
        some_bound_were_tightened = true;
        const IntegerValue new_ub = var_lb + gap / term.weight;
        DCHECK_LT(new_ub, var_ub);
        if (!integer_trail_->Enqueue(
                IntegerLiteral::LowerOrEqual(term.var, new_ub), {}, {})) {
          return false;
        }
      }
    }
  }
  return true;
}

// A basic algorithm is to take the next one, or at least the next one
// that invalidate the current solution. But to avoid corner cases for
// problem with a lot of terms all with different objective weights (in
// which case we will kind of introduce only one assumption per loop
// which is little), we use an heuristic and take the 90% percentile of
// the unique weights not yet included.
//
// TODO(user): There is many other possible heuristics here, and I
// didn't have the time to properly compare them.
void CoreBasedOptimizer::ComputeNextStratificationThreshold() {
  std::vector<IntegerValue> weights;
  for (ObjectiveTerm& term : terms_) {
    if (term.weight >= stratification_threshold_) continue;
    if (term.weight == 0) continue;

    const IntegerValue var_lb = integer_trail_->LevelZeroLowerBound(term.var);
    const IntegerValue var_ub = integer_trail_->LevelZeroUpperBound(term.var);
    if (var_lb == var_ub) continue;

    weights.push_back(term.weight);
  }
  if (weights.empty()) {
    stratification_threshold_ = IntegerValue(0);
    return;
  }

  gtl::STLSortAndRemoveDuplicates(&weights);
  stratification_threshold_ =
      weights[static_cast<int>(std::floor(0.9 * weights.size()))];
}

bool CoreBasedOptimizer::CoverOptimization() {
  if (!sat_solver_->ResetToLevelZero()) return false;

  // We set a fix deterministic time limit per all sub-solves and skip to the
  // next core if the sum of the sub-solves is also over this limit.
  constexpr double max_dtime_per_core = 0.5;
  const double old_time_limit = parameters_->max_deterministic_time();
  parameters_->set_max_deterministic_time(max_dtime_per_core);
  auto cleanup = ::absl::MakeCleanup([old_time_limit, this]() {
    parameters_->set_max_deterministic_time(old_time_limit);
  });

  for (const ObjectiveTerm& term : terms_) {
    // We currently skip the initial objective terms as there could be many
    // of them. TODO(user): provide an option to cover-optimize them? I
    // fear that this will slow down the solver too much though.
    if (term.depth == 0) continue;

    // Find out the true lower bound of var. This is called "cover
    // optimization" in some of the max-SAT literature. It can helps on some
    // problem families and hurt on others, but the overall impact is
    // positive.
    const IntegerVariable var = term.var;
    IntegerValue best =
        std::min(term.cover_ub, integer_trail_->UpperBound(var));

    // Note(user): this can happen in some corner case because each time we
    // find a solution, we constrain the objective to be smaller than it, so
    // it is possible that a previous best is now infeasible.
    if (best <= integer_trail_->LowerBound(var)) continue;

    // Compute the global deterministic time for this core cover
    // optimization.
    const double deterministic_limit =
        time_limit_->GetElapsedDeterministicTime() + max_dtime_per_core;

    // Simple linear scan algorithm to find the optimal of var.
    SatSolver::Status result;
    while (best > integer_trail_->LowerBound(var)) {
      const Literal assumption = integer_encoder_->GetOrCreateAssociatedLiteral(
          IntegerLiteral::LowerOrEqual(var, best - 1));
      result = ResetAndSolveIntegerProblem({assumption}, model_);
      if (result != SatSolver::FEASIBLE) break;

      best = integer_trail_->LowerBound(var);
      VLOG(1) << "cover_opt var:" << var << " domain:["
              << integer_trail_->LevelZeroLowerBound(var) << "," << best << "]";
      if (!ProcessSolution()) return false;
      if (!sat_solver_->ResetToLevelZero()) return false;
      if (stop_ ||
          time_limit_->GetElapsedDeterministicTime() > deterministic_limit) {
        break;
      }
    }
    if (result == SatSolver::INFEASIBLE) return false;
    if (result == SatSolver::ASSUMPTIONS_UNSAT) {
      if (!sat_solver_->ResetToLevelZero()) return false;

      // TODO(user): If we improve the lower bound of var, we should check
      // if our global lower bound reached our current best solution in
      // order to abort early if the optimal is proved.
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(var, best),
                                   {}, {})) {
        return false;
      }
    }
  }

  return PropagateObjectiveBounds();
}

SatSolver::Status CoreBasedOptimizer::OptimizeWithSatEncoding(
    absl::Span<const Literal> literals, absl::Span<const IntegerVariable> vars,
    absl::Span<const Coefficient> coefficients, Coefficient offset) {
  // Create one initial nodes per variables with cost.
  // TODO(user): We could create EncodingNode out of IntegerVariable.
  //
  // Note that the nodes order and assumptions extracted from it will be stable.
  // In particular, new nodes will be appended at the end, which make the solver
  // more likely to find core involving only the first assumptions. This is
  // important at the beginning so the solver as a chance to find a lot of
  // non-overlapping small cores without the need to have dedicated
  // non-overlapping core finder.
  // TODO(user): It could still be beneficial to add one. Experiments.
  ObjectiveEncoder encoder(model_);
  if (vars.empty()) {
    // All Booleans.
    for (int i = 0; i < literals.size(); ++i) {
      CHECK_GT(coefficients[i], 0);
      encoder.AddBaseNode(
          EncodingNode::LiteralNode(literals[i], coefficients[i]));
    }
  } else {
    // Use integer encoding.
    CHECK_EQ(vars.size(), coefficients.size());
    for (int i = 0; i < vars.size(); ++i) {
      CHECK_GT(coefficients[i], 0);
      const IntegerVariable var = vars[i];
      const IntegerValue var_lb = integer_trail_->LowerBound(var);
      const IntegerValue var_ub = integer_trail_->UpperBound(var);
      if (var_ub - var_lb == 1) {
        const Literal lit = integer_encoder_->GetOrCreateAssociatedLiteral(
            IntegerLiteral::GreaterOrEqual(var, var_ub));
        encoder.AddBaseNode(EncodingNode::LiteralNode(lit, coefficients[i]));
      } else {
        // TODO(user): This might not be ideal if there are holes in the domain.
        // It should work by adding duplicates literal, but we should be able to
        // be more efficient.
        const int lb = 0;
        const int ub = static_cast<int>(var_ub.value() - var_lb.value());
        encoder.AddBaseNode(EncodingNode::GenericNode(
            lb, ub,
            [var, var_lb, this](int x) {
              return integer_encoder_->GetOrCreateAssociatedLiteral(
                  IntegerLiteral::GreaterOrEqual(var,
                                                 var_lb + IntegerValue(x + 1)));
            },
            coefficients[i]));
      }
    }
  }

  // Initialize the bounds.
  // This is in term of number of variables not at their minimal value.
  Coefficient lower_bound(0);

  // This is used by the "stratified" approach.
  Coefficient stratified_lower_bound(0);
  if (parameters_->max_sat_stratification() !=
      SatParameters::STRATIFICATION_NONE) {
    for (EncodingNode* n : encoder.nodes()) {
      stratified_lower_bound = std::max(stratified_lower_bound, n->weight());
    }
  }

  // Start the algorithm.
  int max_depth = 0;
  std::string previous_core_info = "";
  for (int iter = 0;;) {
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

    // Note that the objective_var_ upper bound is the one from the "improving"
    // problem, so if we have a feasible solution, it will be the best solution
    // objective value - 1.
    const Coefficient upper_bound(
        integer_trail_->UpperBound(objective_var_).value() - offset.value());
    ReduceNodes(upper_bound, &lower_bound, encoder.mutable_nodes(),
                sat_solver_);
    const IntegerValue new_obj_lb(lower_bound.value() + offset.value());
    if (new_obj_lb > integer_trail_->LowerBound(objective_var_)) {
      if (!integer_trail_->Enqueue(
              IntegerLiteral::GreaterOrEqual(objective_var_, new_obj_lb), {},
              {})) {
        return SatSolver::INFEASIBLE;
      }

      // Report the improvement.
      // Note that we have a callback that will do the same, but doing it
      // earlier allow us to add more information.
      const int num_bools = sat_solver_->NumVariables();
      const int num_fixed = sat_solver_->NumFixedVariables();
      model_->GetOrCreate<SharedResponseManager>()->UpdateInnerObjectiveBounds(
          absl::StrFormat("bool_%s (num_cores=%d [%s] a=%u d=%d "
                          "fixed=%d/%d clauses=%s)",
                          model_->Name(), iter, previous_core_info,
                          encoder.nodes().size(), max_depth, num_fixed,
                          num_bools, FormatCounter(clauses_->num_clauses())),
          new_obj_lb, integer_trail_->LevelZeroUpperBound(objective_var_));
    }

    if (parameters_->cover_optimization() && encoder.nodes().size() > 1) {
      if (ProbeLiteral(
              encoder.mutable_nodes()->back()->GetAssumption(sat_solver_),
              sat_solver_)) {
        previous_core_info = "cover";
        continue;
      }
    }

    // We adapt the stratified lower bound when the gap is small. All literals
    // with such weight will be in an at_most_one relationship, which will lead
    // to a nice encoding if we find a core.
    const Coefficient gap = upper_bound - lower_bound;
    if (stratified_lower_bound > (gap + 2) / 2) {
      stratified_lower_bound = (gap + 2) / 2;
    }
    std::vector<Literal> assumptions;
    while (true) {
      assumptions = ExtractAssumptions(stratified_lower_bound, encoder.nodes(),
                                       sat_solver_);
      if (!assumptions.empty()) break;

      stratified_lower_bound =
          MaxNodeWeightSmallerThan(encoder.nodes(), stratified_lower_bound);
      if (stratified_lower_bound > 0) continue;

      // We do not have any assumptions anymore, but we still need to see
      // if the problem is feasible or not!
      break;
    }
    VLOG(2) << "[Core] #nodes " << encoder.nodes().size()
            << " #assumptions:" << assumptions.size()
            << " stratification:" << stratified_lower_bound << " gap:" << gap;

    // Solve under the assumptions.
    //
    // TODO(user): Find multiple core like in the "main" algorithm. This is just
    // trying to solve with assumptions not involving the newly found core.
    //
    // TODO(user): With stratification, sometime we just spend too much time
    // trying to find a feasible solution/prove infeasibility and we could
    // instead just use stratification=0 to find easty core and improve lower
    // bound.
    const SatSolver::Status result =
        ResetAndSolveIntegerProblem(assumptions, model_);
    if (result == SatSolver::FEASIBLE) {
      if (!ProcessSolution()) return SatSolver::INFEASIBLE;
      if (stop_) return SatSolver::LIMIT_REACHED;

      // If not all assumptions were taken, continue with a lower stratified
      // bound. Otherwise we have an optimal solution.
      stratified_lower_bound =
          MaxNodeWeightSmallerThan(encoder.nodes(), stratified_lower_bound);
      if (stratified_lower_bound > 0) continue;
      return SatSolver::INFEASIBLE;
    }
    if (result != SatSolver::ASSUMPTIONS_UNSAT) return result;

    // We have a new core.
    std::vector<Literal> core = sat_solver_->GetLastIncompatibleDecisions();
    if (parameters_->core_minimization_level() > 0) {
      MinimizeCoreWithPropagation(time_limit_, sat_solver_, &core);
    }
    if (parameters_->core_minimization_level() > 1) {
      MinimizeCoreWithSearch(time_limit_, sat_solver_, &core);
    }
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;
    FilterAssignedLiteral(sat_solver_->Assignment(), &core);
    if (core.empty()) return SatSolver::INFEASIBLE;

    // Compute the min weight of all the nodes in the core.
    // The lower bound will be increased by that much.
    const Coefficient min_weight = ComputeCoreMinWeight(encoder.nodes(), core);
    previous_core_info =
        absl::StrFormat("size:%u mw:%d", core.size(), min_weight.value());

    // We only count an iter when we found a core.
    ++iter;
    if (!encoder.ProcessCore(core, min_weight, gap, &previous_core_info)) {
      return SatSolver::INFEASIBLE;
    }
    max_depth = std::max(max_depth, encoder.nodes().back()->depth());
  }

  return SatSolver::FEASIBLE;  // shouldn't reach here.
}

void PresolveBooleanLinearExpression(std::vector<Literal>* literals,
                                     std::vector<Coefficient>* coefficients,
                                     Coefficient* offset) {
  // Sorting by literal index regroup duplicate or negated literal together.
  std::vector<std::pair<LiteralIndex, Coefficient>> pairs;
  const int size = literals->size();
  for (int i = 0; i < size; ++i) {
    pairs.push_back({(*literals)[i].Index(), (*coefficients)[i]});
  }
  std::sort(pairs.begin(), pairs.end());

  // Merge terms if needed.
  int new_size = 0;
  for (const auto& [index, coeff] : pairs) {
    if (new_size > 0) {
      if (pairs[new_size - 1].first == index) {
        pairs[new_size - 1].second += coeff;
        continue;
      } else if (pairs[new_size - 1].first == Literal(index).NegatedIndex()) {
        // The term is coeff *( 1 - X).
        pairs[new_size - 1].second -= coeff;
        *offset += coeff;
        continue;
      }
    }
    pairs[new_size++] = {index, coeff};
  }
  pairs.resize(new_size);

  // Rebuild with positive coeff.
  literals->clear();
  coefficients->clear();
  for (const auto& [index, coeff] : pairs) {
    if (coeff > 0) {
      literals->push_back(Literal(index));
      coefficients->push_back(coeff);
    } else if (coeff < 0) {
      // coeff * X = coeff - coeff * (1 - X).
      *offset += coeff;
      literals->push_back(Literal(index).Negated());
      coefficients->push_back(-coeff);
    }
  }
}

void CoreBasedOptimizer::PresolveObjectiveWithAtMostOne(
    std::vector<Literal>* literals, std::vector<Coefficient>* coefficients,
    Coefficient* offset) {
  // This contains non-negative value. If a literal has negative weight, then
  // we just put a positive weight on its negation and update the offset.
  const int num_literals = implications_->literal_size();
  util_intops::StrongVector<LiteralIndex, Coefficient> weights(num_literals);
  util_intops::StrongVector<LiteralIndex, bool> is_candidate(num_literals);

  // For now, we do not use weight. Note that finding the at most on in the
  // creation order of the variable make a HUGE difference on the max-sat frb
  // family.
  //
  // TODO(user): We can assign preferences to literals to favor certain at most
  // one instead of other. For now we don't, so ExpandAtMostOneWithWeight() will
  // kind of randomize the expansion amongst possible choices.
  util_intops::StrongVector<LiteralIndex, double> preferences;

  // Collect all literals with "negative weights", we will try to find at most
  // one between them.
  std::vector<Literal> candidates;
  const int num_terms = literals->size();
  for (int i = 0; i < num_terms; ++i) {
    const Literal lit = (*literals)[i];
    const Coefficient coeff = (*coefficients)[i];

    // For now we know the input only has positive weight, but it is easy to
    // adapt if needed.
    CHECK_GT(coeff, 0);
    weights[lit] = coeff;

    candidates.push_back(lit.Negated());
    is_candidate[lit.NegatedIndex()] = true;
  }

  int num_at_most_ones = 0;
  Coefficient overall_lb_increase(0);

  std::vector<Literal> at_most_one;
  std::vector<std::pair<Literal, Coefficient>> new_obj_terms;
  implications_->ResetWorkDone();
  for (const Literal root : candidates) {
    if (weights[root.NegatedIndex()] == 0) continue;
    if (implications_->WorkDone() > 1e8) continue;

    // We never put weight on both a literal and its negation.
    CHECK_EQ(weights[root], 0);

    // Note that for this to be as exhaustive as possible, the probing needs
    // to have added binary clauses corresponding to lvl0 propagation.
    at_most_one =
        implications_->ExpandAtMostOneWithWeight</*use_weight=*/false>(
            {root}, is_candidate, preferences);
    if (at_most_one.size() <= 1) continue;
    ++num_at_most_ones;

    // Change the objective weights. Note that all the literal in the at most
    // one will not be processed again since the weight of their negation will
    // be zero after this step.
    Coefficient max_coeff(0);
    Coefficient lb_increase(0);
    for (const Literal lit : at_most_one) {
      const Coefficient coeff = weights[lit.NegatedIndex()];
      lb_increase += coeff;
      max_coeff = std::max(max_coeff, coeff);
    }
    lb_increase -= max_coeff;

    *offset += lb_increase;
    overall_lb_increase += lb_increase;

    for (const Literal lit : at_most_one) {
      is_candidate[lit] = false;
      const Coefficient new_weight = max_coeff - weights[lit.NegatedIndex()];
      CHECK_EQ(weights[lit], 0);
      weights[lit] = new_weight;
      weights[lit.NegatedIndex()] = 0;
      if (new_weight > 0) {
        // TODO(user): While we autorize this to be in future at most one, it
        // will not appear in the "literal" list. We might also want to continue
        // until we reached the fix point.
        is_candidate[lit.NegatedIndex()] = true;
      }
    }

    // Create a new Boolean with weight max_coeff.
    const Literal new_lit(sat_solver_->NewBooleanVariable(), true);
    new_obj_terms.push_back({new_lit, max_coeff});

    // The new boolean is true only if all the one in the at most one are false.
    at_most_one.push_back(new_lit);
    sat_solver_->AddProblemClause(at_most_one);
    is_candidate.resize(implications_->literal_size(), false);
    preferences.resize(implications_->literal_size(), 1.0);
  }

  if (overall_lb_increase > 0) {
    // Report new bounds right away with extra information.
    model_->GetOrCreate<SharedResponseManager>()->UpdateInnerObjectiveBounds(
        absl::StrFormat("am1_presolve (num_literals=%d num_am1=%d "
                        "increase=%lld work_done=%lld)",
                        (int)candidates.size(), num_at_most_ones,
                        overall_lb_increase.value(), implications_->WorkDone()),
        IntegerValue(offset->value()),
        integer_trail_->LevelZeroUpperBound(objective_var_));
  }

  // Reconstruct the objective.
  literals->clear();
  coefficients->clear();
  for (const Literal root : candidates) {
    if (weights[root] > 0) {
      CHECK_EQ(weights[root.NegatedIndex()], 0);
      literals->push_back(root);
      coefficients->push_back(weights[root]);
    }
    if (weights[root.NegatedIndex()] > 0) {
      CHECK_EQ(weights[root], 0);
      literals->push_back(root.Negated());
      coefficients->push_back(weights[root.NegatedIndex()]);
    }
  }
  for (const auto& [lit, coeff] : new_obj_terms) {
    literals->push_back(lit);
    coefficients->push_back(coeff);
  }
}

SatSolver::Status CoreBasedOptimizer::Optimize() {
  // Hack: If the objective is fully Boolean, we use the
  // OptimizeWithSatEncoding() version as it seems to be better.
  //
  // TODO(user): Try to understand exactly why and merge both code path.
  if (!parameters_->interleave_search()) {
    Coefficient offset(0);
    std::vector<Literal> literals;
    std::vector<IntegerVariable> vars;
    std::vector<Coefficient> coefficients;
    bool all_booleans = true;
    IntegerValue range(0);
    for (const ObjectiveTerm& term : terms_) {
      const IntegerVariable var = term.var;
      const IntegerValue coeff = term.weight;
      const IntegerValue lb = integer_trail_->LowerBound(var);
      const IntegerValue ub = integer_trail_->UpperBound(var);
      offset += Coefficient((lb * coeff).value());
      if (lb == ub) continue;

      vars.push_back(var);
      coefficients.push_back(Coefficient(coeff.value()));
      if (ub - lb == 1) {
        literals.push_back(integer_encoder_->GetOrCreateAssociatedLiteral(
            IntegerLiteral::GreaterOrEqual(var, ub)));
      } else {
        all_booleans = false;
        range += ub - lb;
      }
    }
    if (all_booleans) {
      // In some corner case, it is possible the GetOrCreateAssociatedLiteral()
      // returns identical or negated literal of another term. We don't support
      // this below, so we need to make sure this is not the case.
      PresolveBooleanLinearExpression(&literals, &coefficients, &offset);

      // TODO(user): It might be interesting to redo this kind of presolving
      // once high cost booleans have been fixed as we might have more at most
      // one between literal in the objective by then.
      //
      // Or alternatively, we could try this or something like it on the
      // literals from the cores as they are found. We should probably make
      // sure that if it exist, a core of size two is always added. And for
      // such core, we can always try to see if the "at most one" can be
      // extended.
      PresolveObjectiveWithAtMostOne(&literals, &coefficients, &offset);
      return OptimizeWithSatEncoding(literals, {}, coefficients, offset);
    }
    if (range < 1e8) {
      return OptimizeWithSatEncoding({}, vars, coefficients, offset);
    }
  }

  // TODO(user): The core is returned in the same order as the assumptions,
  // so we don't really need this map, we could just do a linear scan to
  // recover which node are part of the core. This however needs to be properly
  // unit tested before usage.
  absl::btree_map<LiteralIndex, int> literal_to_term_index;

  // Start the algorithm.
  stop_ = false;
  while (true) {
    // TODO(user): This always resets the solver to level zero.
    // Because of that we don't resume a solve in "chunk" perfectly. Fix.
    if (!PropagateObjectiveBounds()) return SatSolver::INFEASIBLE;
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;

    // Bulk cover optimization.
    //
    // TODO(user): If the search is aborted during this phase and we solve in
    // "chunk", we don't resume perfectly from where it was. Fix.
    if (parameters_->cover_optimization()) {
      if (!CoverOptimization()) return SatSolver::INFEASIBLE;
      if (stop_) return SatSolver::LIMIT_REACHED;
    }

    // We assumes all terms (modulo stratification) at their lower-bound.
    std::vector<int> term_indices;
    std::vector<IntegerLiteral> integer_assumptions;
    std::vector<IntegerValue> assumption_weights;
    IntegerValue objective_offset(0);
    bool some_assumptions_were_skipped = false;
    for (int i = 0; i < terms_.size(); ++i) {
      const ObjectiveTerm term = terms_[i];

      // TODO(user): These can be simply removed from the list.
      if (term.weight == 0) continue;

      // Skip fixed terms.
      // We still keep them around for a proper lower-bound computation.
      //
      // TODO(user): we could keep an objective offset instead.
      const IntegerValue var_lb = integer_trail_->LowerBound(term.var);
      const IntegerValue var_ub = integer_trail_->UpperBound(term.var);
      if (var_lb == var_ub) {
        objective_offset += term.weight * var_lb.value();
        continue;
      }

      // Only consider the terms above the threshold.
      if (term.weight >= stratification_threshold_) {
        integer_assumptions.push_back(
            IntegerLiteral::LowerOrEqual(term.var, var_lb));
        assumption_weights.push_back(term.weight);
        term_indices.push_back(i);
      } else {
        some_assumptions_were_skipped = true;
      }
    }

    // No assumptions with the current stratification? use the next one.
    if (term_indices.empty() && some_assumptions_were_skipped) {
      ComputeNextStratificationThreshold();
      continue;
    }

    // If there is only one or two assumptions left, we switch the algorithm.
    if (term_indices.size() <= 2 && !some_assumptions_were_skipped) {
      VLOG(1) << "Switching to linear scan...";
      if (!already_switched_to_linear_scan_) {
        already_switched_to_linear_scan_ = true;
        std::vector<IntegerVariable> constraint_vars;
        std::vector<int64_t> constraint_coeffs;
        for (const int index : term_indices) {
          constraint_vars.push_back(terms_[index].var);
          constraint_coeffs.push_back(terms_[index].weight.value());
        }
        constraint_vars.push_back(objective_var_);
        constraint_coeffs.push_back(-1);
        model_->Add(WeightedSumLowerOrEqual(constraint_vars, constraint_coeffs,
                                            -objective_offset.value()));
      }

      return MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          objective_var_, feasible_solution_observer_, model_);
    }

    // Display the progress.
    if (VLOG_IS_ON(1)) {
      int max_depth = 0;
      for (const ObjectiveTerm& term : terms_) {
        max_depth = std::max(max_depth, term.depth);
      }
      const int64_t lb = integer_trail_->LowerBound(objective_var_).value();
      const int64_t ub = integer_trail_->UpperBound(objective_var_).value();
      const int gap =
          lb == ub
              ? 0
              : static_cast<int>(std::ceil(
                    100.0 * (ub - lb) / std::max(std::abs(ub), std::abs(lb))));
      VLOG(1) << absl::StrCat("unscaled_next_obj_range:[", lb, ",", ub,
                              "]"
                              " gap:",
                              gap, "%", " assumptions:", term_indices.size(),
                              " strat:", stratification_threshold_.value(),
                              " depth:", max_depth,
                              " bool: ", sat_solver_->NumVariables());
    }

    // Convert integer_assumptions to Literals.
    std::vector<Literal> assumptions;
    literal_to_term_index.clear();
    for (int i = 0; i < integer_assumptions.size(); ++i) {
      assumptions.push_back(integer_encoder_->GetOrCreateAssociatedLiteral(
          integer_assumptions[i]));

      // Tricky: In some rare case, it is possible that the same literal
      // correspond to more that one assumptions. In this case, we can just
      // pick one of them when converting back a core to term indices.
      //
      // TODO(user): We can probably be smarter about the cost of the
      // assumptions though.
      literal_to_term_index[assumptions.back()] = term_indices[i];
    }

    // Solve under the assumptions.
    //
    // TODO(user): If the "search" is interrupted while computing cores, we
    // currently do not resume it flawlessly. We however add any cores we found
    // before aborting.
    std::vector<std::vector<Literal>> cores;
    const SatSolver::Status result =
        FindCores(assumptions, assumption_weights, stratification_threshold_,
                  model_, &cores);
    if (result == SatSolver::INFEASIBLE) return SatSolver::INFEASIBLE;
    if (result == SatSolver::FEASIBLE) {
      if (!ProcessSolution()) return SatSolver::INFEASIBLE;
      if (stop_) return SatSolver::LIMIT_REACHED;
      if (cores.empty()) {
        ComputeNextStratificationThreshold();
        if (stratification_threshold_ == 0) return SatSolver::INFEASIBLE;
        continue;
      }
    }

    // Process the cores by creating new variables and transferring the minimum
    // weight of each core to it.
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;
    for (const std::vector<Literal>& core : cores) {
      // This just increase the lower-bound of the corresponding node.
      // TODO(user): Maybe the solver should do it right away.
      if (core.size() == 1) {
        if (!sat_solver_->AddUnitClause(core[0].Negated())) {
          return SatSolver::INFEASIBLE;
        }
        continue;
      }

      // Compute the min weight of all the terms in the core. The lower bound
      // will be increased by that much because at least one assumption in the
      // core must be true. This is also why we can start at 1 for new_var_lb.
      bool ignore_this_core = false;
      IntegerValue min_weight = kMaxIntegerValue;
      IntegerValue max_weight(0);
      IntegerValue new_var_lb(1);
      IntegerValue new_var_ub(0);
      int new_depth = 0;
      for (const Literal lit : core) {
        const int index = literal_to_term_index.at(lit.Index());

        // When this happen, the core is now trivially "minimized" by the new
        // bound on this variable, so there is no point in adding it.
        if (terms_[index].old_var_lb <
            integer_trail_->LowerBound(terms_[index].var)) {
          ignore_this_core = true;
          break;
        }

        const IntegerValue weight = terms_[index].weight;
        min_weight = std::min(min_weight, weight);
        max_weight = std::max(max_weight, weight);
        new_depth = std::max(new_depth, terms_[index].depth + 1);
        new_var_lb += integer_trail_->LowerBound(terms_[index].var);
        new_var_ub += integer_trail_->UpperBound(terms_[index].var);
      }
      if (ignore_this_core) continue;

      VLOG(1) << absl::StrFormat(
          "core:%u weight:[%d,%d] domain:[%d,%d] depth:%d", core.size(),
          min_weight.value(), max_weight.value(), new_var_lb.value(),
          new_var_ub.value(), new_depth);

      // We will "transfer" min_weight from all the variables of the core
      // to a new variable.
      const IntegerVariable new_var =
          integer_trail_->AddIntegerVariable(new_var_lb, new_var_ub);
      terms_.push_back({new_var, min_weight, new_depth});
      terms_.back().cover_ub = new_var_ub;

      // Sum variables in the core <= new_var.
      {
        std::vector<IntegerVariable> constraint_vars;
        std::vector<int64_t> constraint_coeffs;
        for (const Literal lit : core) {
          const int index = literal_to_term_index.at(lit.Index());
          terms_[index].weight -= min_weight;
          constraint_vars.push_back(terms_[index].var);
          constraint_coeffs.push_back(1);
        }
        constraint_vars.push_back(new_var);
        constraint_coeffs.push_back(-1);
        model_->Add(
            WeightedSumLowerOrEqual(constraint_vars, constraint_coeffs, 0));
      }
    }

    // Abort if we reached the time limit. Note that we still add any cores we
    // found in case the solve is split in "chunk".
    if (result == SatSolver::LIMIT_REACHED) return result;
  }
}

}  // namespace sat
}  // namespace operations_research
