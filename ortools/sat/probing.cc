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
#include <functional>
#include <optional>
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
#include "ortools/sat/lrat_proof_handler.h"
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

FailedLiteralProbing::FailedLiteralProbing(Model* model)
    : sat_solver_(model->GetOrCreate<SatSolver>()),
      implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      trail_(*model->GetOrCreate<Trail>()),
      assignment_(trail_.Assignment()),
      clause_manager_(model->GetOrCreate<ClauseManager>()),
      clause_id_generator_(model->GetOrCreate<ClauseIdGenerator>()),
      lrat_proof_handler_(model->Mutable<LratProofHandler>()),
      id_(implication_graph_->PropagatorId()),
      clause_propagator_id_(clause_manager_->PropagatorId()),
      num_variables_(sat_solver_->NumVariables()) {}

// TODO(user): This might be broken if backtrack() propagates and go further
// back. Investigate and fix any issue.
bool FailedLiteralProbing::DoOneRound(ProbingOptions options) {
  WallTimer wall_timer;
  wall_timer.Start();

  options.log_info |= VLOG_IS_ON(1);

  // Reset the solver in case it was already used.
  if (!sat_solver_->ResetToLevelZero()) return false;

  // When called from Inprocessing, the implication graph should already be a
  // DAG, so these two calls should return right away. But we do need them to
  // get the topological order if this is used in isolation.
  if (!implication_graph_->DetectEquivalences()) return false;
  if (!sat_solver_->FinishPropagation()) return false;

  const int initial_num_fixed = sat_solver_->LiteralTrail().Index();
  const double initial_deterministic_time =
      time_limit_->GetElapsedDeterministicTime();
  const double limit = initial_deterministic_time + options.deterministic_limit;

  processed_.ClearAndResize(LiteralIndex(2 * num_variables_));

  if (!options.use_queue) starts_.resize(2 * num_variables_, 0);

  // Depending on the options, we do not use the same order.
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
  probing_order_ = implication_graph_->ReverseTopologicalOrder();
  if (!options.use_tree_look && !options.extract_binary_clauses) {
    std::reverse(probing_order_.begin(), probing_order_.end());
  }

  // We only use this for the queue version.
  if (options.use_queue) {
    position_in_order_.assign(2 * num_variables_, -1);
    for (int i = 0; i < probing_order_.size(); ++i) {
      position_in_order_[probing_order_[i]] = i;
    }
  }

  binary_clause_extracted_.assign(trail_.Index(), false);
  subsumed_clauses_.clear();

  while (!time_limit_->LimitReached() &&
         time_limit_->GetElapsedDeterministicTime() <= limit) {
    // We only enqueue literal at level zero if we don't use "tree look".
    if (!options.use_tree_look) {
      if (!sat_solver_->BacktrackAndPropagateReimplications(0)) return false;
    }

    // Probing works by taking a series of decisions, and by analyzing what
    // they propagate. For efficiency, we only take a new decision d' if it
    // directly implies the last one d. By doing this we know that d' directly
    // or indirectly implies all the previous decisions, which then propagate
    // all the literals on the trail up to and excluding d'. The first step is
    // to find the next_decision d', which can be done in different ways
    // depending on the options.
    LiteralIndex next_decision = kNoLiteralIndex;
    if (sat_solver_->CurrentDecisionLevel() > 0 && options.use_queue) {
      if (!ComputeNextDecisionInOrder(next_decision)) return false;
    }
    if (sat_solver_->CurrentDecisionLevel() > 0 &&
        next_decision == kNoLiteralIndex) {
      if (!GetNextDecisionInRandomOrder(next_decision)) return false;
      if (next_decision == kNoLiteralIndex) continue;
    }
    if (sat_solver_->CurrentDecisionLevel() == 0) {
      if (!GetFirstDecision(next_decision)) return false;
      // The pass is finished.
      if (next_decision == kNoLiteralIndex) break;
    }

    // We now have a next decision, enqueue it and propagate until fix point.
    int first_new_trail_index;
    if (!EnqueueDecisionAndBackjumpOnConflict(next_decision, options.use_queue,
                                              first_new_trail_index)) {
      return false;
    }

    // Inspect the newly propagated literals. Depending on the options, try to
    // extract binary clauses via hyper binary resolution and/or mark the
    // literals on the trail so that they do not need to be probed later.
    const int new_level = sat_solver_->CurrentDecisionLevel();
    if (new_level == 0) continue;
    const Literal last_decision =
        sat_solver_->Decisions()[new_level - 1].literal;
    for (int i = first_new_trail_index; i < trail_.Index(); ++i) {
      const Literal l = trail_[i];
      if (l == last_decision) continue;

      bool subsumed = false;
      if (options.subsume_with_binary_clause) {
        subsumed = MaybeSubsumeWithBinaryClause(last_decision, l);
      }
      if (options.extract_binary_clauses) {
        if (!subsumed) {
          MaybeExtractBinaryClause(last_decision, l);
        }
      } else {
        // If we don't extract binary, we don't need to explore any of
        // these literals until more variables are fixed.
        processed_.Set(l.Index());
      }
    }

    if (options.subsume_with_binary_clause) {
      SubsumeWithBinaryClauseUsingBlockingLiteral(last_decision);
    }
  }

  if (!sat_solver_->ResetToLevelZero()) return false;
  if (!ProcessLiteralsToFix()) return false;
  if (!subsumed_clauses_.empty()) {
    for (SatClause* clause : subsumed_clauses_) {
      clause_manager_->LazyDetach(clause);
    }
    clause_manager_->CleanUpWatchers();
  }

  // Display stats.
  const int num_fixed = sat_solver_->LiteralTrail().Index();
  const int num_newly_fixed = num_fixed - initial_num_fixed;
  const double time_diff =
      time_limit_->GetElapsedDeterministicTime() - initial_deterministic_time;
  const bool limit_reached = time_limit_->LimitReached() ||
                             time_limit_->GetElapsedDeterministicTime() > limit;
  LOG_IF(INFO, options.log_info)
      << "Probing. "
      << " num_probed: " << num_probed_ << "/" << probing_order_.size()
      << " num_fixed: +" << num_newly_fixed << " (" << num_fixed << "/"
      << num_variables_ << ")"
      << " explicit_fix:" << num_explicit_fix_
      << " num_conflicts:" << num_conflicts_
      << " new_binary_clauses: " << num_new_binary_
      << " subsumed: " << num_subsumed_ << " dtime: " << time_diff
      << " wtime: " << wall_timer.Get() << (limit_reached ? " (Aborted)" : "");
  return sat_solver_->FinishPropagation();
}

// Sets `next_decision` to the unassigned literal which implies the last
// decision and which comes first in the probing order (which itself can be
// the topological order of the implication graph, or the reverse).
bool FailedLiteralProbing::ComputeNextDecisionInOrder(
    LiteralIndex& next_decision) {
  // TODO(user): Instead of minimizing index in topo order (which might be
  // nice for binary extraction), we could try to maximize reusability in
  // some way.
  const Literal last_decision =
      sat_solver_->Decisions()[sat_solver_->CurrentDecisionLevel() - 1].literal;
  // If l => last_decision, then not(last_decision) => not(l). We can thus
  // find the candidates for the next decision by looking at all the
  // implications of not(last_decision).
  const absl::Span<const Literal> list =
      implication_graph_->Implications(last_decision.Negated());
  const int saved_queue_size = queue_.size();
  for (const Literal l : list) {
    const Literal candidate = l.Negated();
    if (processed_[candidate]) continue;
    if (position_in_order_[candidate] == -1) continue;
    if (assignment_.LiteralIsAssigned(candidate)) {
      // candidate => last_decision => all previous decisions, which then
      // propagate not(candidate). Hence candidate must be false.
      if (assignment_.LiteralIsFalse(candidate)) {
        AddFailedLiteralToFix(candidate);
      }
      continue;
    }
    queue_.push_back({candidate.Index(), -position_in_order_[candidate]});
  }
  // Sort all the candidates.
  std::sort(queue_.begin() + saved_queue_size, queue_.end());

  // Set next_decision to the first unassigned candidate.
  while (!queue_.empty()) {
    const LiteralIndex index = queue_.back().literal_index;
    queue_.pop_back();
    if (index == kNoLiteralIndex) {
      // This is a backtrack marker, go back one level.
      CHECK_GT(sat_solver_->CurrentDecisionLevel(), 0);
      if (!sat_solver_->BacktrackAndPropagateReimplications(
              sat_solver_->CurrentDecisionLevel() - 1))
        return false;
      continue;
    }
    const Literal candidate(index);
    if (processed_[candidate]) continue;
    if (assignment_.LiteralIsAssigned(candidate)) {
      // candidate => last_decision => all previous decisions, which then
      // propagate not(candidate). Hence candidate must be false.
      if (assignment_.LiteralIsFalse(candidate)) {
        AddFailedLiteralToFix(candidate);
      }
      continue;
    }
    next_decision = candidate.Index();
    break;
  }
  return true;
}

// Sets `next_decision` to the first unassigned literal we find which implies
// the last decision, in no particular order.
bool FailedLiteralProbing::GetNextDecisionInRandomOrder(
    LiteralIndex& next_decision) {
  const int level = sat_solver_->CurrentDecisionLevel();
  const Literal last_decision = sat_solver_->Decisions()[level - 1].literal;
  const absl::Span<const Literal> list =
      implication_graph_->Implications(last_decision.Negated());

  // If l => last_decision, then not(last_decision) => not(l). We can thus
  // find the candidates for the next decision by looking at all the
  // implications of not(last_decision).
  int j = starts_[last_decision.NegatedIndex()];
  for (int i = 0; i < list.size(); ++i, ++j) {
    j %= list.size();
    const Literal candidate = Literal(list[j]).Negated();
    if (processed_[candidate]) continue;
    if (assignment_.LiteralIsFalse(candidate)) {
      // candidate => last_decision => all previous decisions, which then
      // propagate not(candidate). Hence candidate must be false.
      AddFailedLiteralToFix(candidate);
      continue;
    }
    // This shouldn't happen if extract_binary_clauses is false.
    // We have an equivalence.
    if (assignment_.LiteralIsTrue(candidate)) continue;
    next_decision = candidate.Index();
    break;
  }
  starts_[last_decision.NegatedIndex()] = j;
  if (next_decision == kNoLiteralIndex) {
    if (!sat_solver_->BacktrackAndPropagateReimplications(level - 1)) {
      return false;
    }
  }
  return true;
}

// Sets `next_decision` to the first unassigned literal in probing_order (if
// there is no last decision we can use any literal as first decision).
bool FailedLiteralProbing::GetFirstDecision(LiteralIndex& next_decision) {
  // Fix any delayed fixed literal.
  if (!ProcessLiteralsToFix()) return false;

  // Probe an unexplored node.
  for (; order_index_ < probing_order_.size(); ++order_index_) {
    const Literal candidate(probing_order_[order_index_]);
    if (processed_[candidate]) continue;
    if (assignment_.LiteralIsAssigned(candidate)) continue;
    next_decision = candidate.Index();
    break;
  }
  return true;
}

bool FailedLiteralProbing::EnqueueDecisionAndBackjumpOnConflict(
    LiteralIndex next_decision, bool use_queue, int& first_new_trail_index) {
  ++num_probed_;
  processed_.Set(next_decision);
  CHECK_NE(next_decision, kNoLiteralIndex);
  queue_.push_back({kNoLiteralIndex, 0});  // Backtrack marker.
  const int level = sat_solver_->CurrentDecisionLevel();

  // The unit clause ID that fixes next_decision to false, if it causes a
  // conflict.
  ClauseId fixed_decision_unit_id = kNoClauseId;
  auto conflict_callback = [&](ClauseId conflict_id,
                               absl::Span<const Literal> conflict_clause) {
    if (fixed_decision_unit_id != kNoClauseId) return;
    ComputeDecisionImplicationIds(Literal(next_decision), level,
                                  /*end_level=*/0, tmp_clause_ids_);
    sat_solver_->AppendClausesFixing(conflict_clause, &tmp_clause_ids_);
    tmp_clause_ids_.push_back(conflict_id);
    fixed_decision_unit_id = clause_id_generator_->GetNextId();
    lrat_proof_handler_->AddInferredClause(fixed_decision_unit_id,
                                           {Literal(next_decision).Negated()},
                                           tmp_clause_ids_);
  };
  first_new_trail_index = sat_solver_->EnqueueDecisionAndBackjumpOnConflict(
      Literal(next_decision),
      lrat_proof_handler_ != nullptr
          ? conflict_callback
          : std::optional<SatSolver::ConflictCallback>());

  if (first_new_trail_index == kUnsatTrailIndex) return false;
  binary_clause_extracted_.resize(first_new_trail_index);
  binary_clause_extracted_.resize(trail_.Index(), false);

  // This is tricky, depending on the parameters, and for integer problem,
  // EnqueueDecisionAndBackjumpOnConflict() might create new Booleans.
  if (sat_solver_->NumVariables() > num_variables_) {
    num_variables_ = sat_solver_->NumVariables();
    processed_.Resize(LiteralIndex(2 * num_variables_));
    if (!use_queue) {
      starts_.resize(2 * num_variables_, 0);
    } else {
      position_in_order_.resize(2 * num_variables_, -1);
    }
  }

  const int new_level = sat_solver_->CurrentDecisionLevel();
  sat_solver_->AdvanceDeterministicTime(time_limit_);
  if (sat_solver_->ModelIsUnsat()) return false;
  if (new_level <= level) {
    ++num_conflicts_;

    // Sync the queue with the new level.
    if (use_queue) {
      if (new_level == 0) {
        queue_.clear();
      } else {
        int queue_level = level + 1;
        while (queue_level > new_level) {
          CHECK(!queue_.empty());
          if (queue_.back().literal_index == kNoLiteralIndex) --queue_level;
          queue_.pop_back();
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
    if (sat_solver_->CurrentDecisionLevel() != 0 ||
        assignment_.LiteralIsFalse(Literal(next_decision))) {
      to_fix_.push_back(Literal(next_decision).Negated());
      if (lrat_proof_handler_ != nullptr) {
        to_fix_unit_id_.push_back(fixed_decision_unit_id);
      }
    }
  }
  return true;
}

// If we can extract a binary clause that subsume the reason clause, we do add
// the binary and remove the subsumed clause.
//
// TODO(user): We could be slightly more generic and subsume some clauses that
// do not contain last_decision.Negated().
bool FailedLiteralProbing::MaybeSubsumeWithBinaryClause(
    const Literal last_decision, const Literal l) {
  const int trail_index = trail_.Info(l.Variable()).trail_index;
  if (binary_clause_extracted_[trail_index] ||
      trail_.AssignmentType(l.Variable()) != clause_propagator_id_) {
    return false;
  }
  bool subsumed = false;
  for (const Literal lit : trail_.Reason(l.Variable())) {
    if (lit == last_decision.Negated()) {
      subsumed = true;
      break;
    }
  }
  if (!subsumed) return false;
  ++num_subsumed_;
  ++num_new_binary_;
  // TODO(user): add LRAT proof.
  CHECK(implication_graph_->AddBinaryClause(last_decision.Negated(), l));
  binary_clause_extracted_[trail_index] = true;

  int test = 0;
  for (const Literal lit :
       clause_manager_->ReasonClause(trail_index)->AsSpan()) {
    if (lit == l) ++test;
    if (lit == last_decision.Negated()) ++test;
  }
  CHECK_EQ(test, 2);
  subsumed_clauses_.push_back(clause_manager_->ReasonClause(trail_index));
  return true;
}

void FailedLiteralProbing::MaybeExtractBinaryClause(const Literal last_decision,
                                                    const Literal l) {
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
  // this clause subsumes the reason.
  if (trail_.AssignmentType(l.Variable()) != id_) {
    ++num_new_binary_;
    // TODO(user): add LRAT proof.
    CHECK(implication_graph_->AddBinaryClause(last_decision.Negated(), l));
  }
}

// Inspect the watcher list for last_decision, If we have a blocking
// literal at true (implied by last decision), then we have subsumptions.
//
// The intuition behind this is that if a binary clause (a,b) subsumes a
// clause, and we watch a.Negated() for this clause with a blocking
// literal b, then this watch entry will never change because we always
// propagate binary clauses first and the blocking literal will always be
// true. So after many propagations, we hope to have such configuration
// which is quite cheap to test here.
void FailedLiteralProbing::SubsumeWithBinaryClauseUsingBlockingLiteral(
    const Literal last_decision) {
  for (const auto& w :
       clause_manager_->WatcherListOnFalse(last_decision.Negated())) {
    if (assignment_.LiteralIsTrue(w.blocking_literal)) {
      if (w.clause->IsRemoved()) continue;
      const auto& info = trail_.Info(w.blocking_literal.Variable());
      CHECK_NE(w.blocking_literal, last_decision.Negated());

      // Add the binary clause if not already done. If the variable was true at
      // level zero, there is no point adding the clause.
      if (info.level > 0 && !binary_clause_extracted_[info.trail_index]) {
        ++num_new_binary_;
        // TODO(user): add LRAT proof.
        CHECK(implication_graph_->AddBinaryClause(last_decision.Negated(),
                                                  w.blocking_literal));
        binary_clause_extracted_[info.trail_index] = true;
      }

      ++num_subsumed_;
      subsumed_clauses_.push_back(w.clause);
    }
  }
}

// Sets `clause_ids` to the IDs of the implications from `l` to the decisions
// from `start_level` to `end_level` (included, going backwards).
void FailedLiteralProbing::ComputeDecisionImplicationIds(
    Literal l, int start_level, int end_level,
    std::vector<ClauseId>& clause_ids) {
  clause_ids.clear();
  int level = start_level;
  while (level != end_level) {
    const Literal decision = sat_solver_->Decisions()[--level].literal;
    clause_ids.push_back(
        implication_graph_->GetClauseId(l.Negated(), decision));
    l = decision;
  }
}

// Adds 'not(literal)' to `to_fix_`, assuming that 'literal' directly implies
// the current decision, which itself implies all the previous decisions, with
// some of them propagating 'not(literal)'.
void FailedLiteralProbing::AddFailedLiteralToFix(const Literal literal) {
  // TODO(user): skip if literal.Negated() is already in to_fix?
  to_fix_.push_back(literal.Negated());
  if (lrat_proof_handler_ == nullptr) return;

  ClauseId unit_id = trail_.GetUnitClauseId(literal.Variable());
  if (unit_id == kNoClauseId) {
    // TODO(user): it should be possible to stop at the level of the
    // first decision necessary to fix 'literal'. A first attempt at this
    // failed.
    ComputeDecisionImplicationIds(literal, sat_solver_->CurrentDecisionLevel(),
                                  /*end_level=*/0, tmp_clause_ids_);
    sat_solver_->AppendClausesFixing({literal.Negated()}, &tmp_clause_ids_);
    unit_id = clause_id_generator_->GetNextId();
    lrat_proof_handler_->AddInferredClause(unit_id, {literal.Negated()},
                                           tmp_clause_ids_);
  }
  to_fix_unit_id_.push_back({unit_id});
}

// Fixes all the literals in to_fix_, and finish propagation.
bool FailedLiteralProbing::ProcessLiteralsToFix() {
  for (int i = 0; i < to_fix_.size(); ++i) {
    const Literal literal = to_fix_[i];
    if (assignment_.LiteralIsTrue(literal)) continue;
    ++num_explicit_fix_;
    const ClauseId clause_id =
        lrat_proof_handler_ != nullptr ? to_fix_unit_id_[i] : kNoClauseId;
    if (!clause_manager_->InprocessingAddUnitClause(clause_id, literal)) {
      return false;
    }
  }
  to_fix_.clear();
  to_fix_unit_id_.clear();
  return sat_solver_->FinishPropagation();
}

bool FailedLiteralProbingRound(ProbingOptions options, Model* model) {
  return FailedLiteralProbing(model).DoOneRound(options);
}

}  // namespace sat
}  // namespace operations_research
