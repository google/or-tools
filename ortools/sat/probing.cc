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

#include "ortools/sat/probing.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

Prober::Prober(Model* model)
    : trail_(*model->GetOrCreate<Trail>()),
      assignment_(model->GetOrCreate<SatSolver>()->Assignment()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      implied_bounds_(model->GetOrCreate<ImpliedBounds>()),
      product_detector_(model->GetOrCreate<ProductDetector>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      logger_(model->GetOrCreate<SolverLogger>()) {}

bool Prober::ProbeBooleanVariables(const double deterministic_time_limit) {
  const int num_variables = sat_solver_->NumVariables();
  const VariablesAssignment& assignment = sat_solver_->Assignment();
  std::vector<BooleanVariable> bool_vars;
  for (BooleanVariable b(0); b < num_variables; ++b) {
    if (assignment.VariableIsAssigned(b)) continue;
    const Literal literal(b, true);
    if (implication_graph_->RepresentativeOf(literal) != literal) {
      continue;
    }
    bool_vars.push_back(b);
  }
  return ProbeBooleanVariables(deterministic_time_limit, bool_vars);
}

bool Prober::ProbeOneVariableInternal(BooleanVariable b) {
  new_integer_bounds_.clear();
  propagated_.ResetAllToFalse();
  for (const Literal decision : {Literal(b, true), Literal(b, false)}) {
    if (assignment_.LiteralIsAssigned(decision)) continue;

    ++num_decisions_;
    CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
    const int saved_index = trail_.Index();
    if (sat_solver_->EnqueueDecisionAndBackjumpOnConflict(decision) ==
        kUnsatTrailIndex) {
      return false;
    }
    sat_solver_->AdvanceDeterministicTime(time_limit_);

    if (sat_solver_->ModelIsUnsat()) return false;
    if (sat_solver_->CurrentDecisionLevel() == 0) continue;
    if (trail_.Index() > saved_index) {
      if (callback_ != nullptr) callback_(decision);
    }

    if (!implied_bounds_->ProcessIntegerTrail(decision)) return false;
    product_detector_->ProcessTrailAtLevelOne();
    integer_trail_->AppendNewBounds(&new_integer_bounds_);
    for (int i = saved_index + 1; i < trail_.Index(); ++i) {
      const Literal l = trail_[i];

      // We mark on the first run (b.IsPositive()) and check on the second.
      if (decision.IsPositive()) {
        propagated_.Set(l.Index());
      } else {
        if (propagated_[l]) {
          to_fix_at_true_.push_back(l);
        }
      }

      // Anything not propagated by the BinaryImplicationGraph is a "new"
      // binary clause. This is because the BinaryImplicationGraph has the
      // highest priority of all propagators.
      if (trail_.AssignmentType(l.Variable()) !=
          implication_graph_->PropagatorId()) {
        new_binary_clauses_.push_back({decision.Negated(), l});
      }
    }

    // Fix variable and add new binary clauses.
    if (!sat_solver_->ResetToLevelZero()) return false;
    for (const Literal l : to_fix_at_true_) {
      if (!sat_solver_->AddUnitClause(l)) return false;
    }
    to_fix_at_true_.clear();
    if (!sat_solver_->FinishPropagation()) return false;
    num_new_binary_ += new_binary_clauses_.size();
    for (auto binary : new_binary_clauses_) {
      sat_solver_->AddBinaryClause(binary.first, binary.second);
    }
    new_binary_clauses_.clear();
    if (!sat_solver_->FinishPropagation()) return false;
  }

  // We have at most two lower bounds for each variables (one for b==0 and one
  // for b==1), so the min of the two is a valid level zero bound! More
  // generally, the domain of a variable can be intersected with the union
  // of the two propagated domains. This also allow to detect "holes".
  //
  // TODO(user): More generally, for any clauses (b or not(b) is one), we
  // could probe all the literal inside, and for any integer variable, we can
  // take the union of the propagated domain as a new domain.
  //
  // TODO(user): fix binary variable in the same way? It might not be as
  // useful since probing on such variable will also fix it. But then we might
  // abort probing early, so it might still be good.
  std::sort(new_integer_bounds_.begin(), new_integer_bounds_.end(),
            [](IntegerLiteral a, IntegerLiteral b) { return a.var < b.var; });

  // This is used for the hole detection.
  IntegerVariable prev_var = kNoIntegerVariable;
  IntegerValue lb_max = kMinIntegerValue;
  IntegerValue ub_min = kMaxIntegerValue;
  new_integer_bounds_.push_back(IntegerLiteral());  // Sentinel.

  const int limit = new_integer_bounds_.size();
  for (int i = 0; i < limit; ++i) {
    const IntegerVariable var = new_integer_bounds_[i].var;

    // Hole detection.
    if (i > 0 && PositiveVariable(var) != prev_var) {
      if (ub_min + 1 < lb_max) {
        // The variable cannot take value in (ub_min, lb_max) !
        //
        // TODO(user): do not create domain with a complexity that is too
        // large?
        const Domain old_domain =
            integer_trail_->InitialVariableDomain(prev_var);
        const Domain new_domain = old_domain.IntersectionWith(
            Domain(ub_min.value() + 1, lb_max.value() - 1).Complement());
        if (new_domain != old_domain) {
          ++num_new_holes_;
          if (!integer_trail_->UpdateInitialDomain(prev_var, new_domain)) {
            return false;
          }
        }
      }

      // Reinitialize.
      lb_max = kMinIntegerValue;
      ub_min = kMaxIntegerValue;
    }

    prev_var = PositiveVariable(var);
    if (VariableIsPositive(var)) {
      lb_max = std::max(lb_max, new_integer_bounds_[i].bound);
    } else {
      ub_min = std::min(ub_min, -new_integer_bounds_[i].bound);
    }

    // Bound tightening.
    if (i == 0 || new_integer_bounds_[i - 1].var != var) continue;
    const IntegerValue new_bound = std::min(new_integer_bounds_[i - 1].bound,
                                            new_integer_bounds_[i].bound);
    if (new_bound > integer_trail_->LowerBound(var)) {
      ++num_new_integer_bounds_;
      if (!integer_trail_->Enqueue(
              IntegerLiteral::GreaterOrEqual(var, new_bound), {}, {})) {
        return false;
      }
    }
  }

  // We might have updated some integer domain, let's propagate.
  return sat_solver_->FinishPropagation();
}

bool Prober::ProbeOneVariable(BooleanVariable b) {
  // Resize the propagated sparse bitset.
  const int num_variables = sat_solver_->NumVariables();
  propagated_.ClearAndResize(LiteralIndex(2 * num_variables));

  // Reset the solver in case it was already used.
  if (!sat_solver_->ResetToLevelZero()) return false;

  const int initial_num_fixed = sat_solver_->LiteralTrail().Index();
  if (!ProbeOneVariableInternal(b)) return false;

  // Statistics
  const int num_fixed = sat_solver_->LiteralTrail().Index();
  num_new_literals_fixed_ += num_fixed - initial_num_fixed;
  return true;
}

bool Prober::ProbeBooleanVariables(
    const double deterministic_time_limit,
    absl::Span<const BooleanVariable> bool_vars) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Reset statistics.
  num_decisions_ = 0;
  num_new_binary_ = 0;
  num_new_holes_ = 0;
  num_new_integer_bounds_ = 0;
  num_new_literals_fixed_ = 0;

  // Resize the propagated sparse bitset.
  const int num_variables = sat_solver_->NumVariables();
  propagated_.ClearAndResize(LiteralIndex(2 * num_variables));

  // Reset the solver in case it was already used.
  if (!sat_solver_->ResetToLevelZero()) return false;

  const int initial_num_fixed = sat_solver_->LiteralTrail().Index();
  const double initial_deterministic_time =
      time_limit_->GetElapsedDeterministicTime();
  const double limit = initial_deterministic_time + deterministic_time_limit;

  bool limit_reached = false;
  int num_probed = 0;

  for (const BooleanVariable b : bool_vars) {
    const Literal literal(b, true);
    if (implication_graph_->RepresentativeOf(literal) != literal) {
      continue;
    }

    // TODO(user): Instead of an hard deterministic limit, we should probably
    // use a lower one, but reset it each time we have found something useful.
    if (time_limit_->LimitReached() ||
        time_limit_->GetElapsedDeterministicTime() > limit) {
      limit_reached = true;
      break;
    }

    // Propagate b=1 and then b=0.
    ++num_probed;
    if (!ProbeOneVariableInternal(b)) {
      return false;
    }
  }

  // Update stats.
  const int num_fixed = sat_solver_->LiteralTrail().Index();
  num_new_literals_fixed_ = num_fixed - initial_num_fixed;

  // Display stats.
  if (logger_->LoggingIsEnabled()) {
    const double time_diff =
        time_limit_->GetElapsedDeterministicTime() - initial_deterministic_time;
    SOLVER_LOG(logger_, "[Probing] deterministic_time: ", time_diff,
               " (limit: ", deterministic_time_limit,
               ") wall_time: ", wall_timer.Get(), " (",
               (limit_reached ? "Aborted " : ""), num_probed, "/",
               bool_vars.size(), ")");
    if (num_new_literals_fixed_ > 0) {
      SOLVER_LOG(logger_,
                 "[Probing]  - new fixed Boolean: ", num_new_literals_fixed_,
                 " (", FormatCounter(num_fixed), "/",
                 FormatCounter(sat_solver_->NumVariables()), ")");
    }
    if (num_new_holes_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new integer holes: ",
                 FormatCounter(num_new_holes_));
    }
    if (num_new_integer_bounds_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new integer bounds: ",
                 FormatCounter(num_new_integer_bounds_));
    }
    if (num_new_binary_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new binary clause: ",
                 FormatCounter(num_new_binary_));
    }
  }

  return true;
}

bool Prober::ProbeDnf(absl::string_view name,
                      absl::Span<const std::vector<Literal>> dnf) {
  if (dnf.size() <= 1) return true;

  // Reset the solver in case it was already used.
  if (!sat_solver_->ResetToLevelZero()) return false;

  always_propagated_bounds_.clear();
  always_propagated_literals_.clear();
  int num_valid_conjunctions = 0;
  for (const std::vector<Literal>& conjunction : dnf) {
    if (!sat_solver_->ResetToLevelZero()) return false;
    if (num_valid_conjunctions > 0 && always_propagated_bounds_.empty() &&
        always_propagated_literals_.empty()) {
      // We can exit safely as nothing will be propagated.
      return true;
    }

    bool conjunction_is_valid = true;
    int num_literals_enqueued = 0;
    const int root_trail_index = trail_.Index();
    const int root_integer_trail_index = integer_trail_->Index();
    for (const Literal& lit : conjunction) {
      if (assignment_.LiteralIsAssigned(lit)) {
        if (assignment_.LiteralIsTrue(lit)) continue;
        conjunction_is_valid = false;
        break;
      }
      ++num_literals_enqueued;
      const int decision_level_before_enqueue =
          sat_solver_->CurrentDecisionLevel();
      sat_solver_->EnqueueDecisionAndBackjumpOnConflict(lit);
      sat_solver_->AdvanceDeterministicTime(time_limit_);
      const int decision_level_after_enqueue =
          sat_solver_->CurrentDecisionLevel();
      ++num_decisions_;

      if (sat_solver_->ModelIsUnsat()) return false;
      // If the literal has been pushed without any conflict, the level should
      // have been increased.
      if (decision_level_after_enqueue <= decision_level_before_enqueue) {
        conjunction_is_valid = false;
        break;
      }
      // TODO(user): Can we use the callback_?
    }
    if (conjunction_is_valid) {
      ++num_valid_conjunctions;
    } else {
      continue;
    }

    // Process propagated literals.
    new_propagated_literals_.clear();
    for (int i = root_trail_index; i < trail_.Index(); ++i) {
      const LiteralIndex literal_index = trail_[i].Index();
      if (num_valid_conjunctions == 1 ||
          always_propagated_literals_.contains(literal_index)) {
        new_propagated_literals_.insert(literal_index);
      }
    }
    std::swap(new_propagated_literals_, always_propagated_literals_);

    // Process propagated integer bounds.
    new_integer_bounds_.clear();
    integer_trail_->AppendNewBoundsFrom(root_integer_trail_index,
                                        &new_integer_bounds_);
    new_propagated_bounds_.clear();
    for (const IntegerLiteral entry : new_integer_bounds_) {
      const auto it = always_propagated_bounds_.find(entry.var);
      if (num_valid_conjunctions == 1) {  // First loop.
        new_propagated_bounds_[entry.var] = entry.bound;
      } else if (it != always_propagated_bounds_.end()) {
        new_propagated_bounds_[entry.var] = std::min(entry.bound, it->second);
      }
    }
    std::swap(new_propagated_bounds_, always_propagated_bounds_);
  }

  if (!sat_solver_->ResetToLevelZero()) return false;
  // Fix literals implied by the dnf.
  const int previous_num_literals_fixed = num_new_literals_fixed_;
  for (const LiteralIndex literal_index : always_propagated_literals_) {
    const Literal lit(literal_index);
    if (assignment_.LiteralIsTrue(lit)) continue;
    ++num_new_literals_fixed_;
    if (!sat_solver_->AddUnitClause(lit)) return false;
  }

  // Fix integer bounds implied by the dnf.
  int previous_num_integer_bounds = num_new_integer_bounds_;
  for (const auto& [var, bound] : always_propagated_bounds_) {
    if (bound > integer_trail_->LowerBound(var)) {
      ++num_new_integer_bounds_;
      if (!integer_trail_->Enqueue(IntegerLiteral::GreaterOrEqual(var, bound),
                                   {}, {})) {
        return false;
      }
    }
  }

  if (!sat_solver_->FinishPropagation()) return false;

  if (num_new_integer_bounds_ > previous_num_integer_bounds ||
      num_new_literals_fixed_ > previous_num_literals_fixed) {
    VLOG(1) << "ProbeDnf(" << name << ", num_fixed_literals="
            << num_new_literals_fixed_ - previous_num_literals_fixed
            << ", num_pushed_integer_bounds="
            << num_new_integer_bounds_ - previous_num_integer_bounds
            << ", num_valid_conjunctions=" << num_valid_conjunctions << "/"
            << dnf.size() << ")";
  }

  return true;
}

bool LookForTrivialSatSolution(double deterministic_time_limit, Model* model,
                               SolverLogger* logger) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Hack to not have empty logger.
  if (logger == nullptr) logger = model->GetOrCreate<SolverLogger>();

  // Reset the solver in case it was already used.
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  if (!sat_solver->ResetToLevelZero()) return false;

  auto* time_limit = model->GetOrCreate<TimeLimit>();
  const int initial_num_fixed = sat_solver->LiteralTrail().Index();

  // Note that this code do not care about the non-Boolean part and just try to
  // assign the existing Booleans.
  SatParameters initial_params = *model->GetOrCreate<SatParameters>();
  SatParameters new_params = initial_params;
  new_params.set_log_search_progress(false);
  new_params.set_max_number_of_conflicts(1);
  new_params.set_max_deterministic_time(deterministic_time_limit);

  double elapsed_dtime = 0.0;

  const int num_times = 1000;
  bool limit_reached = false;
  auto* random = model->GetOrCreate<ModelRandomGenerator>();
  for (int i = 0; i < num_times; ++i) {
    if (time_limit->LimitReached() ||
        elapsed_dtime > deterministic_time_limit) {
      limit_reached = true;
      break;
    }

    // SetParameters() reset the deterministic time to zero inside time_limit.
    sat_solver->SetParameters(new_params);
    sat_solver->ResetDecisionHeuristic();
    const SatSolver::Status result = sat_solver->SolveWithTimeLimit(time_limit);
    elapsed_dtime += time_limit->GetElapsedDeterministicTime();

    if (result == SatSolver::FEASIBLE) {
      SOLVER_LOG(logger, "Trivial exploration found feasible solution!");
      time_limit->AdvanceDeterministicTime(elapsed_dtime);
      return true;
    }

    if (!sat_solver->ResetToLevelZero()) {
      SOLVER_LOG(logger, "UNSAT during trivial exploration heuristic.");
      time_limit->AdvanceDeterministicTime(elapsed_dtime);
      return false;
    }

    // We randomize at the end so that the default params is executed
    // at least once.
    RandomizeDecisionHeuristic(*random, &new_params);
    new_params.set_random_seed(i);
    new_params.set_max_deterministic_time(deterministic_time_limit -
                                          elapsed_dtime);
  }

  // Restore the initial parameters.
  sat_solver->SetParameters(initial_params);
  sat_solver->ResetDecisionHeuristic();
  time_limit->AdvanceDeterministicTime(elapsed_dtime);
  if (!sat_solver->ResetToLevelZero()) return false;

  if (logger->LoggingIsEnabled()) {
    const int num_fixed = sat_solver->LiteralTrail().Index();
    const int num_newly_fixed = num_fixed - initial_num_fixed;
    const int num_variables = sat_solver->NumVariables();
    SOLVER_LOG(logger, "[Random exploration]", " num_fixed: +",
               FormatCounter(num_newly_fixed), " (", FormatCounter(num_fixed),
               "/", FormatCounter(num_variables), ")",
               " dtime: ", elapsed_dtime, "/", deterministic_time_limit,
               " wtime: ", wall_timer.Get(),
               (limit_reached ? " (Aborted)" : ""));
  }
  return sat_solver->FinishPropagation();
}

// TODO(user): This might be broken if backtrack() propagates and go further
// back. Investigate and fix any issue.
bool FailedLiteralProbingRound(ProbingOptions options, Model* model) {
  WallTimer wall_timer;
  wall_timer.Start();
  options.log_info |= VLOG_IS_ON(1);

  // Reset the solver in case it was already used.
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  if (!sat_solver->ResetToLevelZero()) return false;

  // When called from Inprocessing, the implication graph should already be a
  // DAG, so these two calls should return right away. But we do need them to
  // get the topological order if this is used in isolation.
  auto* implication_graph = model->GetOrCreate<BinaryImplicationGraph>();
  if (!implication_graph->DetectEquivalences()) return false;
  if (!sat_solver->FinishPropagation()) return false;

  auto* time_limit = model->GetOrCreate<TimeLimit>();
  const int initial_num_fixed = sat_solver->LiteralTrail().Index();
  const double initial_deterministic_time =
      time_limit->GetElapsedDeterministicTime();
  const double limit = initial_deterministic_time + options.deterministic_limit;

  int num_variables = sat_solver->NumVariables();
  SparseBitset<LiteralIndex> processed(LiteralIndex(2 * num_variables));

  int64_t num_probed = 0;
  int64_t num_explicit_fix = 0;
  int64_t num_conflicts = 0;
  int64_t num_new_binary = 0;
  int64_t num_subsumed = 0;

  const auto& trail = *(model->Get<Trail>());
  const auto& assignment = trail.Assignment();
  auto* clause_manager = model->GetOrCreate<ClauseManager>();
  const int id = implication_graph->PropagatorId();
  const int clause_id = clause_manager->PropagatorId();

  // This is only needed when options.use_queue is true.
  struct SavedNextLiteral {
    LiteralIndex literal_index;  // kNoLiteralIndex if we need to backtrack.
    int rank;  // Cached position_in_order, we prefer lower positions.

    bool operator<(const SavedNextLiteral& o) const { return rank < o.rank; }
  };
  std::vector<SavedNextLiteral> queue;
  util_intops::StrongVector<LiteralIndex, int> position_in_order;

  // This is only needed when options use_queue is false;
  util_intops::StrongVector<LiteralIndex, int> starts;
  if (!options.use_queue) starts.resize(2 * num_variables, 0);

  // We delay fixing of already assigned literal once we go back to level
  // zero.
  std::vector<Literal> to_fix;

  // Depending on the options. we do not use the same order.
  // With tree look, it is better to start with "leaf" first since we try
  // to reuse propagation as much as possible. This is also interesting to
  // do when extracting binary clauses since we will need to propagate
  // everyone anyway, and this should result in less clauses that can be
  // removed later by transitive reduction.
  //
  // However, without tree-look and without the need to extract all binary
  // clauses, it is better to just probe the root of the binary implication
  // graph. This is exactly what happen when we probe using the topological
  // order.
  int order_index(0);
  std::vector<LiteralIndex> probing_order =
      implication_graph->ReverseTopologicalOrder();
  if (!options.use_tree_look && !options.extract_binary_clauses) {
    std::reverse(probing_order.begin(), probing_order.end());
  }

  // We only use this for the queue version.
  if (options.use_queue) {
    position_in_order.assign(2 * num_variables, -1);
    for (int i = 0; i < probing_order.size(); ++i) {
      position_in_order[probing_order[i]] = i;
    }
  }

  while (!time_limit->LimitReached() &&
         time_limit->GetElapsedDeterministicTime() <= limit) {
    // We only enqueue literal at level zero if we don't use "tree look".
    if (!options.use_tree_look) {
      if (!sat_solver->BacktrackAndPropagateReimplications(0)) return false;
    }

    // Probing works by taking a series of decisions, and by analyzing what they
    // propagate. For efficiency, we only take a new decision d' if it directly
    // implies the last one d. By doing this we know that d' directly or
    // indirectly implies all the previous decisions, which then propagate all
    // the literals on the trail up to and excluding d'. The first step is to
    // find the next_decision d', which can be done in different ways depending
    // on the options.
    LiteralIndex next_decision = kNoLiteralIndex;

    // A first option is to use an unassigned literal which implies the last
    // decision and which comes first in the probing order (which itself can be
    // the topological order of the implication graph, or the reverse).
    if (sat_solver->CurrentDecisionLevel() > 0 && options.use_queue) {
      // TODO(user): Instead of minimizing index in topo order (which might be
      // nice for binary extraction), we could try to maximize reusability in
      // some way.
      const Literal last_decision =
          sat_solver->Decisions()[sat_solver->CurrentDecisionLevel() - 1]
              .literal;
      // If l => last_decision, then not(last_decision) => not(l). We can thus
      // find the candidates for the next decision by looking at all the
      // implications of not(last_decision).
      const absl::Span<const Literal> list =
          implication_graph->Implications(last_decision.Negated());
      const int saved_queue_size = queue.size();
      for (const Literal l : list) {
        const Literal candidate = l.Negated();
        if (processed[candidate]) continue;
        if (position_in_order[candidate] == -1) continue;
        if (assignment.LiteralIsAssigned(candidate)) {
          // candidate => last_decision => all previous decisions, which then
          // propagate not(candidate). Hence candidate must be false.
          if (assignment.LiteralIsFalse(candidate)) {
            to_fix.push_back(Literal(candidate.Negated()));
          }
          continue;
        }
        queue.push_back({candidate.Index(), -position_in_order[candidate]});
      }
      // Sort all the candidates.
      std::sort(queue.begin() + saved_queue_size, queue.end());

      // Set next_decision to the first unassigned candidate.
      while (!queue.empty()) {
        const LiteralIndex index = queue.back().literal_index;
        queue.pop_back();
        if (index == kNoLiteralIndex) {
          // This is a backtrack marker, go back one level.
          CHECK_GT(sat_solver->CurrentDecisionLevel(), 0);
          if (!sat_solver->BacktrackAndPropagateReimplications(
                  sat_solver->CurrentDecisionLevel() - 1))
            return false;
          continue;
        }
        const Literal candidate(index);
        if (processed[candidate]) continue;
        if (assignment.LiteralIsAssigned(candidate)) {
          // candidate => last_decision => all previous decisions, which then
          // propagate not(candidate). Hence candidate must be false.
          if (assignment.LiteralIsFalse(candidate)) {
            to_fix.push_back(Literal(candidate.Negated()));
          }
          continue;
        }
        next_decision = candidate.Index();
        break;
      }
    }
    // A second option to find the next decision is to use the first unassigned
    // literal we find which implies the last decision, in no particular
    // order.
    if (sat_solver->CurrentDecisionLevel() > 0 &&
        next_decision == kNoLiteralIndex) {
      const int level = sat_solver->CurrentDecisionLevel();
      const Literal last_decision = sat_solver->Decisions()[level - 1].literal;
      const absl::Span<const Literal> list =
          implication_graph->Implications(last_decision.Negated());

      // If l => last_decision, then not(last_decision) => not(l). We can thus
      // find the candidates for the next decision by looking at all the
      // implications of not(last_decision).
      int j = starts[last_decision.NegatedIndex()];
      for (int i = 0; i < list.size(); ++i, ++j) {
        j %= list.size();
        const Literal candidate = Literal(list[j]).Negated();
        if (processed[candidate]) continue;
        if (assignment.LiteralIsFalse(candidate)) {
          // candidate => last_decision => all previous decisions, which then
          // propagate not(candidate). Hence candidate must be false.
          to_fix.push_back(Literal(candidate.Negated()));
          continue;
        }
        // This shouldn't happen if extract_binary_clauses is false.
        // We have an equivalence.
        if (assignment.LiteralIsTrue(candidate)) continue;
        next_decision = candidate.Index();
        break;
      }
      starts[last_decision.NegatedIndex()] = j;
      if (next_decision == kNoLiteralIndex) {
        if (!sat_solver->BacktrackAndPropagateReimplications(level - 1)) {
          return false;
        }
        continue;
      }
    }
    // If there is no last decision we can use any literal as first decision.
    // We use the first unassigned literal in probing_order.
    if (sat_solver->CurrentDecisionLevel() == 0) {
      // Fix any delayed fixed literal.
      for (const Literal literal : to_fix) {
        if (!assignment.LiteralIsTrue(literal)) {
          ++num_explicit_fix;
          if (!sat_solver->AddUnitClause(literal)) return false;
        }
      }
      to_fix.clear();
      if (!sat_solver->FinishPropagation()) return false;

      // Probe an unexplored node.
      for (; order_index < probing_order.size(); ++order_index) {
        const Literal candidate(probing_order[order_index]);
        if (processed[candidate]) continue;
        if (assignment.LiteralIsAssigned(candidate)) continue;
        next_decision = candidate.Index();
        break;
      }

      // The pass is finished.
      if (next_decision == kNoLiteralIndex) break;
    }

    // We now have a next decision, enqueue it and propagate until fix point.
    ++num_probed;
    processed.Set(next_decision);
    CHECK_NE(next_decision, kNoLiteralIndex);
    queue.push_back({kNoLiteralIndex, 0});  // Backtrack marker.
    const int level = sat_solver->CurrentDecisionLevel();
    const int first_new_trail_index =
        sat_solver->EnqueueDecisionAndBackjumpOnConflict(
            Literal(next_decision));

    // This is tricky, depending on the parameters, and for integer problem,
    // EnqueueDecisionAndBackjumpOnConflict() might create new Booleans.
    if (sat_solver->NumVariables() > num_variables) {
      num_variables = sat_solver->NumVariables();
      processed.Resize(LiteralIndex(2 * num_variables));
      if (!options.use_queue) {
        starts.resize(2 * num_variables, 0);
      } else {
        position_in_order.resize(2 * num_variables, -1);
      }
    }

    const int new_level = sat_solver->CurrentDecisionLevel();
    sat_solver->AdvanceDeterministicTime(time_limit);
    if (sat_solver->ModelIsUnsat()) return false;
    if (new_level <= level) {
      ++num_conflicts;

      // Sync the queue with the new level.
      if (options.use_queue) {
        if (new_level == 0) {
          queue.clear();
        } else {
          int queue_level = level + 1;
          while (queue_level > new_level) {
            CHECK(!queue.empty());
            if (queue.back().literal_index == kNoLiteralIndex) --queue_level;
            queue.pop_back();
          }
        }
      }

      // Fix `next_decision` to `false` if not already done.
      //
      // Even if we fixed something at level zero, next_decision might not be
      // fixed! But we can fix it. It can happen because when we propagate
      // with clauses, we might have `a => b` but not `not(b) => not(a)`. Like
      // `a => b` and clause `(not(a), not(b), c)`, propagating `a` will set
      // `c`, but propagating `not(c)` will not do anything.
      //
      // We "delay" the fixing if we are not at level zero so that we can
      // still reuse the current propagation work via tree look.
      //
      // TODO(user): Can we be smarter here? Maybe we can still fix the
      // literal without going back to level zero by simply enqueuing it with
      // no reason? it will be backtracked over, but we will still lazily fix
      // it later.
      if (sat_solver->CurrentDecisionLevel() != 0 ||
          assignment.LiteralIsFalse(Literal(next_decision))) {
        to_fix.push_back(Literal(next_decision).Negated());
      }
    }

    // Inspect the newly propagated literals. Depending on the options, try to
    // extract binary clauses via hyper binary resolution and/or mark the
    // literals on the trail so that they do not need to be probed later.
    if (new_level == 0) continue;
    const Literal last_decision =
        sat_solver->Decisions()[new_level - 1].literal;
    int num_new_subsumed = 0;
    for (int i = first_new_trail_index; i < trail.Index(); ++i) {
      const Literal l = trail[i];
      if (l == last_decision) continue;

      // If we can extract a binary clause that subsume the reason clause, we
      // do add the binary and remove the subsumed clause.
      //
      // TODO(user): We could be slightly more generic and subsume some
      // clauses that do not contains last_decision.Negated().
      bool subsumed = false;
      if (options.subsume_with_binary_clause &&
          trail.AssignmentType(l.Variable()) == clause_id) {
        for (const Literal lit : trail.Reason(l.Variable())) {
          if (lit == last_decision.Negated()) {
            subsumed = true;
            break;
          }
        }
        if (subsumed) {
          ++num_new_subsumed;
          ++num_new_binary;
          CHECK(implication_graph->AddBinaryClause(last_decision.Negated(), l));
          const int trail_index = trail.Info(l.Variable()).trail_index;

          int test = 0;
          for (const Literal lit :
               clause_manager->ReasonClause(trail_index)->AsSpan()) {
            if (lit == l) ++test;
            if (lit == last_decision.Negated()) ++test;
          }
          CHECK_EQ(test, 2);
          clause_manager->LazyDetach(clause_manager->ReasonClause(trail_index));

          // We need to change the reason now that the clause is cleared.
          implication_graph->ChangeReason(trail_index, last_decision);
        }
      }

      if (options.extract_binary_clauses) {
        // Anything not propagated by the BinaryImplicationGraph is a "new"
        // binary clause. This is because the BinaryImplicationGraph has the
        // highest priority of all propagators.
        //
        // Note(user): This is not 100% true, since when we launch the clause
        // propagation for one literal we do finish it before calling again
        // the binary propagation.
        //
        // TODO(user): Think about trying to extract clause that will not
        // get removed by transitive reduction later. If we can both extract
        // a => c and b => c , ideally we don't want to extract a => c first
        // if we already know that a => b.
        //
        // TODO(user): Similar to previous point, we could find the LCA
        // of all literals in the reason for this propagation. And use this
        // as a reason for later hyber binary resolution. Like we do when
        // this clause subsume the reason.
        if (!subsumed && trail.AssignmentType(l.Variable()) != id) {
          ++num_new_binary;
          CHECK(implication_graph->AddBinaryClause(last_decision.Negated(), l));
        }
      } else {
        // If we don't extract binary, we don't need to explore any of
        // these literal until more variables are fixed.
        processed.Set(l.Index());
      }
    }

    // Inspect the watcher list for last_decision, If we have a blocking
    // literal at true (implied by last decision), then we have subsumptions.
    //
    // The intuition behind this is that if a binary clause (a,b) subsume a
    // clause, and we watch a.Negated() for this clause with a blocking
    // literal b, then this watch entry will never change because we always
    // propagate binary clauses first and the blocking literal will always be
    // true. So after many propagations, we hope to have such configuration
    // which is quite cheap to test here.
    if (options.subsume_with_binary_clause) {
      // Tricky: If we have many "decision" and we do not extract the binary
      // clause, then the fact that last_decision => literal might not be
      // currently encoded in the problem clause, so if we use that relation
      // to subsume, we should make sure it is added.
      //
      // Note that it is okay to add duplicate binary clause, we will clean that
      // later.
      const bool always_add_binary = sat_solver->CurrentDecisionLevel() > 1 &&
                                     !options.extract_binary_clauses;

      for (const auto& w :
           clause_manager->WatcherListOnFalse(last_decision.Negated())) {
        if (assignment.LiteralIsTrue(w.blocking_literal)) {
          if (w.clause->IsRemoved()) continue;
          CHECK_NE(w.blocking_literal, last_decision.Negated());

          // Add the binary clause if needed. Note that we change the reason
          // to a binary one so that we never add the same clause twice.
          //
          // Tricky: while last_decision would be a valid reason, we need a
          // reason that was assigned before this literal, so we use the
          // decision at the level where this literal was assigned which is an
          // even better reason. Maybe it is just better to change all the
          // reason above to a binary one so we don't have an issue here.
          if (always_add_binary ||
              trail.AssignmentType(w.blocking_literal.Variable()) != id) {
            // If the variable was true at level zero, there is no point
            // adding the clause.
            const auto& info = trail.Info(w.blocking_literal.Variable());
            if (info.level > 0) {
              ++num_new_binary;
              CHECK(implication_graph->AddBinaryClause(last_decision.Negated(),
                                                       w.blocking_literal));

              const Literal d = sat_solver->Decisions()[info.level - 1].literal;
              if (d != w.blocking_literal) {
                implication_graph->ChangeReason(info.trail_index, d);
              }
            }
          }

          ++num_new_subsumed;
          clause_manager->LazyDetach(w.clause);
        }
      }
    }

    if (num_new_subsumed > 0) {
      // TODO(user): We might just want to do that even more lazily by
      // checking for detached clause while propagating here? and do a big
      // cleanup at the end.
      clause_manager->CleanUpWatchers();
      num_subsumed += num_new_subsumed;
    }
  }

  if (!sat_solver->ResetToLevelZero()) return false;
  for (const Literal literal : to_fix) {
    ++num_explicit_fix;
    if (!sat_solver->AddUnitClause(literal)) return false;
  }
  to_fix.clear();
  if (!sat_solver->FinishPropagation()) return false;

  // Display stats.
  const int num_fixed = sat_solver->LiteralTrail().Index();
  const int num_newly_fixed = num_fixed - initial_num_fixed;
  const double time_diff =
      time_limit->GetElapsedDeterministicTime() - initial_deterministic_time;
  const bool limit_reached = time_limit->LimitReached() ||
                             time_limit->GetElapsedDeterministicTime() > limit;
  LOG_IF(INFO, options.log_info)
      << "Probing. "
      << " num_probed: " << num_probed << "/" << probing_order.size()
      << " num_fixed: +" << num_newly_fixed << " (" << num_fixed << "/"
      << num_variables << ")"
      << " explicit_fix:" << num_explicit_fix
      << " num_conflicts:" << num_conflicts
      << " new_binary_clauses: " << num_new_binary
      << " subsumed: " << num_subsumed << " dtime: " << time_diff
      << " wtime: " << wall_timer.Get() << (limit_reached ? " (Aborted)" : "");
  return sat_solver->FinishPropagation();
}

}  // namespace sat
}  // namespace operations_research
