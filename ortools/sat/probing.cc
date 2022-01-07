// Copyright 2010-2021 Google LLC
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

#include <cstdint>
#include <set>

#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/strong_vector.h"
#include "ortools/base/timer.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/util.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

Prober::Prober(Model* model)
    : trail_(*model->GetOrCreate<Trail>()),
      assignment_(model->GetOrCreate<SatSolver>()->Assignment()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      implied_bounds_(model->GetOrCreate<ImpliedBounds>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      logger_(model->GetOrCreate<SolverLogger>()) {}

bool Prober::ProbeBooleanVariables(const double deterministic_time_limit) {
  const int num_variables = sat_solver_->NumVariables();
  std::vector<BooleanVariable> bool_vars;
  for (BooleanVariable b(0); b < num_variables; ++b) {
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
  propagated_.SparseClearAll();
  for (const Literal decision : {Literal(b, true), Literal(b, false)}) {
    if (assignment_.LiteralIsAssigned(decision)) continue;

    CHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0);
    const int saved_index = trail_.Index();
    sat_solver_->EnqueueDecisionAndBackjumpOnConflict(decision);
    sat_solver_->AdvanceDeterministicTime(time_limit_);

    if (sat_solver_->IsModelUnsat()) return false;
    if (sat_solver_->CurrentDecisionLevel() == 0) continue;

    implied_bounds_->ProcessIntegerTrail(decision);
    integer_trail_->AppendNewBounds(&new_integer_bounds_);
    for (int i = saved_index + 1; i < trail_.Index(); ++i) {
      const Literal l = trail_[i];

      // We mark on the first run (b.IsPositive()) and check on the second.
      if (decision.IsPositive()) {
        propagated_.Set(l.Index());
      } else {
        if (propagated_[l.Index()]) {
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
    if (!sat_solver_->RestoreSolverToAssumptionLevel()) return false;
    for (const Literal l : to_fix_at_true_) {
      sat_solver_->AddUnitClause(l);
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

  for (int i = 0; i < new_integer_bounds_.size(); ++i) {
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
  // Reset statistics.
  num_new_binary_ = 0;
  num_new_holes_ = 0;
  num_new_integer_bounds_ = 0;

  // Resize the propagated sparse bitset.
  const int num_variables = sat_solver_->NumVariables();
  propagated_.ClearAndResize(LiteralIndex(2 * num_variables));

  // Reset the solver in case it was already used.
  sat_solver_->SetAssumptionLevel(0);
  if (!sat_solver_->RestoreSolverToAssumptionLevel()) return false;

  return ProbeOneVariableInternal(b);
}

bool Prober::ProbeBooleanVariables(
    const double deterministic_time_limit,
    absl::Span<const BooleanVariable> bool_vars) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Reset statistics.
  num_new_binary_ = 0;
  num_new_holes_ = 0;
  num_new_integer_bounds_ = 0;

  // Resize the propagated sparse bitset.
  const int num_variables = sat_solver_->NumVariables();
  propagated_.ClearAndResize(LiteralIndex(2 * num_variables));

  // Reset the solver in case it was already used.
  sat_solver_->SetAssumptionLevel(0);
  if (!sat_solver_->RestoreSolverToAssumptionLevel()) return false;

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

  // Display stats.
  if (logger_->LoggingIsEnabled()) {
    const double time_diff =
        time_limit_->GetElapsedDeterministicTime() - initial_deterministic_time;
    const int num_fixed = sat_solver_->LiteralTrail().Index();
    const int num_newly_fixed = num_fixed - initial_num_fixed;
    SOLVER_LOG(logger_, "[Probing] deterministic_time: ", time_diff,
               " (limit: ", deterministic_time_limit,
               ") wall_time: ", wall_timer.Get(), " (",
               (limit_reached ? "Aborted " : ""), num_probed, "/",
               bool_vars.size(), ")");
    if (num_newly_fixed > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new fixed Boolean: ", num_newly_fixed,
                 " (", num_fixed, "/", sat_solver_->NumVariables(), ")");
    }
    if (num_new_holes_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new integer holes: ", num_new_holes_);
    }
    if (num_new_integer_bounds_ > 0) {
      SOLVER_LOG(logger_,
                 "[Probing]  - new integer bounds: ", num_new_integer_bounds_);
    }
    if (num_new_binary_ > 0) {
      SOLVER_LOG(logger_, "[Probing]  - new binary clause: ", num_new_binary_);
    }
  }

  return true;
}

bool LookForTrivialSatSolution(double deterministic_time_limit, Model* model) {
  WallTimer wall_timer;
  wall_timer.Start();

  // Reset the solver in case it was already used.
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  sat_solver->SetAssumptionLevel(0);
  if (!sat_solver->RestoreSolverToAssumptionLevel()) return false;

  auto* time_limit = model->GetOrCreate<TimeLimit>();
  const int initial_num_fixed = sat_solver->LiteralTrail().Index();
  auto* logger = model->GetOrCreate<SolverLogger>();

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

    if (!sat_solver->RestoreSolverToAssumptionLevel()) {
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
  if (!sat_solver->RestoreSolverToAssumptionLevel()) return false;

  if (logger->LoggingIsEnabled()) {
    const int num_fixed = sat_solver->LiteralTrail().Index();
    const int num_newly_fixed = num_fixed - initial_num_fixed;
    const int num_variables = sat_solver->NumVariables();
    SOLVER_LOG(logger, "Random exploration.", " num_fixed: +", num_newly_fixed,
               " (", num_fixed, "/", num_variables, ")",
               " dtime: ", elapsed_dtime, "/", deterministic_time_limit,
               " wtime: ", wall_timer.Get(),
               (limit_reached ? " (Aborted)" : ""));
  }
  return sat_solver->FinishPropagation();
}

bool FailedLiteralProbingRound(ProbingOptions options, Model* model) {
  WallTimer wall_timer;
  wall_timer.Start();
  options.log_info |= VLOG_IS_ON(1);

  // Reset the solver in case it was already used.
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  sat_solver->SetAssumptionLevel(0);
  if (!sat_solver->RestoreSolverToAssumptionLevel()) return false;

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

  const int num_variables = sat_solver->NumVariables();
  SparseBitset<LiteralIndex> processed(LiteralIndex(2 * num_variables));

  int64_t num_probed = 0;
  int64_t num_explicit_fix = 0;
  int64_t num_conflicts = 0;
  int64_t num_new_binary = 0;
  int64_t num_subsumed = 0;

  const auto& trail = *(model->Get<Trail>());
  const auto& assignment = trail.Assignment();
  auto* clause_manager = model->GetOrCreate<LiteralWatchers>();
  const int id = implication_graph->PropagatorId();
  const int clause_id = clause_manager->PropagatorId();

  // This is only needed when options.use_queue is true.
  struct SavedNextLiteral {
    LiteralIndex literal_index;  // kNoLiteralIndex if we need to backtrack.
    int rank;  // Cached position_in_order, we prefer lower positions.

    bool operator<(const SavedNextLiteral& o) const { return rank < o.rank; }
  };
  std::vector<SavedNextLiteral> queue;
  absl::StrongVector<LiteralIndex, int> position_in_order;

  // This is only needed when options use_queue is false;
  absl::StrongVector<LiteralIndex, int> starts;
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
    if (!options.use_tree_look) sat_solver->Backtrack(0);

    LiteralIndex next_decision = kNoLiteralIndex;
    if (options.use_queue && sat_solver->CurrentDecisionLevel() > 0) {
      // TODO(user): Instead of minimizing index in topo order (which might be
      // nice for binary extraction), we could try to maximize reusability in
      // some way.
      const Literal prev_decision =
          sat_solver->Decisions()[sat_solver->CurrentDecisionLevel() - 1]
              .literal;
      const auto& list =
          implication_graph->Implications(prev_decision.Negated());
      const int saved_queue_size = queue.size();
      for (const Literal l : list) {
        const Literal candidate = l.Negated();
        if (processed[candidate.Index()]) continue;
        if (position_in_order[candidate.Index()] == -1) continue;
        if (assignment.LiteralIsAssigned(candidate)) {
          if (assignment.LiteralIsFalse(candidate)) {
            to_fix.push_back(Literal(candidate.Negated()));
          }
          continue;
        }
        queue.push_back(
            {candidate.Index(), -position_in_order[candidate.Index()]});
      }
      std::sort(queue.begin() + saved_queue_size, queue.end());

      // Probe a literal that implies previous decision.
      while (!queue.empty()) {
        const LiteralIndex index = queue.back().literal_index;
        queue.pop_back();
        if (index == kNoLiteralIndex) {
          // This is a backtrack marker, go back one level.
          CHECK_GT(sat_solver->CurrentDecisionLevel(), 0);
          sat_solver->Backtrack(sat_solver->CurrentDecisionLevel() - 1);
          continue;
        }
        const Literal candidate(index);
        if (processed[candidate.Index()]) continue;
        if (assignment.LiteralIsAssigned(candidate)) {
          if (assignment.LiteralIsFalse(candidate)) {
            to_fix.push_back(Literal(candidate.Negated()));
          }
          continue;
        }
        next_decision = candidate.Index();
        break;
      }
    }

    if (sat_solver->CurrentDecisionLevel() == 0) {
      // Fix any delayed fixed literal.
      for (const Literal literal : to_fix) {
        if (!assignment.LiteralIsTrue(literal)) {
          ++num_explicit_fix;
          sat_solver->AddUnitClause(literal);
        }
      }
      to_fix.clear();
      if (!sat_solver->FinishPropagation()) return false;

      // Probe an unexplored node.
      for (; order_index < probing_order.size(); ++order_index) {
        const Literal candidate(probing_order[order_index]);
        if (processed[candidate.Index()]) continue;
        if (assignment.LiteralIsAssigned(candidate)) continue;
        next_decision = candidate.Index();
        break;
      }

      // The pass is finished.
      if (next_decision == kNoLiteralIndex) break;
    } else if (next_decision == kNoLiteralIndex) {
      const int level = sat_solver->CurrentDecisionLevel();
      const Literal prev_decision = sat_solver->Decisions()[level - 1].literal;
      const auto& list =
          implication_graph->Implications(prev_decision.Negated());

      // Probe a literal that implies previous decision.
      //
      // Note that contrary to the queue based implementation, this do not
      // process them in a particular order.
      int j = starts[prev_decision.NegatedIndex()];
      for (int i = 0; i < list.size(); ++i, ++j) {
        j %= list.size();
        const Literal candidate = Literal(list[j]).Negated();
        if (processed[candidate.Index()]) continue;
        if (assignment.LiteralIsFalse(candidate)) {
          // candidate => previous => not(candidate), so we can fix it.
          to_fix.push_back(Literal(candidate.Negated()));
          continue;
        }
        // This shouldn't happen if extract_binary_clauses is false.
        // We have an equivalence.
        if (assignment.LiteralIsTrue(candidate)) continue;
        next_decision = candidate.Index();
        break;
      }
      starts[prev_decision.NegatedIndex()] = j;
      if (next_decision == kNoLiteralIndex) {
        sat_solver->Backtrack(level - 1);
        continue;
      }
    }

    ++num_probed;
    processed.Set(next_decision);
    CHECK_NE(next_decision, kNoLiteralIndex);
    queue.push_back({kNoLiteralIndex, 0});  // Backtrack marker.
    const int level = sat_solver->CurrentDecisionLevel();
    const int first_new_trail_index =
        sat_solver->EnqueueDecisionAndBackjumpOnConflict(
            Literal(next_decision));
    const int new_level = sat_solver->CurrentDecisionLevel();
    sat_solver->AdvanceDeterministicTime(time_limit);
    if (sat_solver->IsModelUnsat()) return false;
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

      // Fix next_decision to false if not already done.
      //
      // Even if we fixed something at evel zero, next_decision might not be
      // fixed! But we can fix it. It can happen because when we propagate
      // with clauses, we might have a => b but not not(b) => not(a). Like a
      // => b and clause (not(a), not(b), c), propagating a will set c, but
      // propagating not(c) will not do anything.
      //
      // We "delay" the fixing if we are not at level zero so that we can
      // still reuse the current propagation work via tree look.
      //
      // TODO(user): Can we be smarter here? Maybe we can still fix the
      // literal without going back to level zero by simply enqueing it with
      // no reason? it will be bactracked over, but we will still lazily fix
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
          implication_graph->AddBinaryClause(last_decision.Negated(), l);
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
          implication_graph->AddBinaryClause(last_decision.Negated(), l);
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
      for (const auto& w :
           clause_manager->WatcherListOnFalse(last_decision.Negated())) {
        if (assignment.LiteralIsTrue(w.blocking_literal)) {
          if (w.clause->empty()) continue;
          CHECK_NE(w.blocking_literal, last_decision.Negated());

          // Add the binary clause if needed. Note that we change the reason
          // to a binary one so that we never add the same clause twice.
          //
          // Tricky: while last_decision would be a valid reason, we need a
          // reason that was assigned before this literal, so we use the
          // decision at the level where this literal was assigne which is an
          // even better reasony. Maybe it is just better to change all the
          // reason above to a binary one so we don't have an issue here.
          if (trail.AssignmentType(w.blocking_literal.Variable()) != id) {
            // If the variable was true at level zero, there is no point
            // adding the clause.
            const auto& info = trail.Info(w.blocking_literal.Variable());
            if (info.level > 0) {
              ++num_new_binary;
              implication_graph->AddBinaryClause(last_decision.Negated(),
                                                 w.blocking_literal);

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
    sat_solver->AddUnitClause(literal);
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
      << " num_probed: " << num_probed << " num_fixed: +" << num_newly_fixed
      << " (" << num_fixed << "/" << num_variables << ")"
      << " explicit_fix:" << num_explicit_fix
      << " num_conflicts:" << num_conflicts
      << " new_binary_clauses: " << num_new_binary
      << " subsumed: " << num_subsumed << " dtime: " << time_diff
      << " wtime: " << wall_timer.Get() << (limit_reached ? " (Aborted)" : "");
  return sat_solver->FinishPropagation();
}

}  // namespace sat
}  // namespace operations_research
