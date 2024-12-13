// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/integer_search.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <random>
#include <tuple>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/meta/type_traits.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/pseudo_costs.h"
#include "ortools/sat/restart.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research {
namespace sat {

IntegerLiteral AtMinValue(IntegerVariable var, IntegerTrail* integer_trail) {
  const IntegerValue lb = integer_trail->LowerBound(var);
  DCHECK_LE(lb, integer_trail->UpperBound(var));
  if (lb == integer_trail->UpperBound(var)) return IntegerLiteral();
  return IntegerLiteral::LowerOrEqual(var, lb);
}

IntegerLiteral ChooseBestObjectiveValue(
    IntegerVariable var, IntegerTrail* integer_trail,
    ObjectiveDefinition* objective_definition) {
  const auto& variables = objective_definition->objective_impacting_variables;
  if (variables.contains(var)) {
    return AtMinValue(var, integer_trail);
  } else if (variables.contains(NegationOf(var))) {
    return AtMinValue(NegationOf(var), integer_trail);
  }
  return IntegerLiteral();
}

IntegerLiteral GreaterOrEqualToMiddleValue(IntegerVariable var,
                                           IntegerTrail* integer_trail) {
  const IntegerValue var_lb = integer_trail->LowerBound(var);
  const IntegerValue var_ub = integer_trail->UpperBound(var);
  CHECK_LT(var_lb, var_ub);

  const IntegerValue chosen_value =
      var_lb + std::max(IntegerValue(1), (var_ub - var_lb) / IntegerValue(2));
  return IntegerLiteral::GreaterOrEqual(var, chosen_value);
}

IntegerLiteral SplitAroundGivenValue(IntegerVariable var, IntegerValue value,
                                     Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  const IntegerValue lb = integer_trail->LowerBound(var);
  const IntegerValue ub = integer_trail->UpperBound(var);

  const absl::flat_hash_set<IntegerVariable>& variables =
      model->GetOrCreate<ObjectiveDefinition>()->objective_impacting_variables;

  // Heuristic: Prefer the objective direction first. Reference: Conflict-Driven
  // Heuristics for Mixed Integer Programming (2019) by Jakob Witzig and Ambros
  // Gleixner.
  // NOTE: The value might be out of bounds. In that case we return
  // kNoLiteralIndex.
  const bool branch_down_feasible = value >= lb && value < ub;
  const bool branch_up_feasible = value > lb && value <= ub;
  if (variables.contains(var) && branch_down_feasible) {
    return IntegerLiteral::LowerOrEqual(var, value);
  } else if (variables.contains(NegationOf(var)) && branch_up_feasible) {
    return IntegerLiteral::GreaterOrEqual(var, value);
  } else if (branch_down_feasible) {
    return IntegerLiteral::LowerOrEqual(var, value);
  } else if (branch_up_feasible) {
    return IntegerLiteral::GreaterOrEqual(var, value);
  }
  return IntegerLiteral();
}

IntegerLiteral SplitAroundLpValue(IntegerVariable var, Model* model) {
  auto* parameters = model->GetOrCreate<SatParameters>();
  auto* lp_dispatcher = model->GetOrCreate<LinearProgrammingDispatcher>();

  const IntegerVariable positive_var = PositiveVariable(var);
  const auto& it = lp_dispatcher->find(positive_var);
  const LinearProgrammingConstraint* lp =
      it == lp_dispatcher->end() ? nullptr : it->second;

  // We only use this if the sub-lp has a solution, and depending on the value
  // of exploit_all_lp_solution() if it is a pure-integer solution.
  if (lp == nullptr || !lp->HasSolution()) return IntegerLiteral();
  if (!parameters->exploit_all_lp_solution() && !lp->SolutionIsInteger()) {
    return IntegerLiteral();
  }

  // TODO(user): Depending if we branch up or down, this might not exclude the
  // LP value, which is potentially a bad thing.
  //
  // TODO(user): Why is the reduced cost doing things differently?
  const IntegerValue value = IntegerValue(
      static_cast<int64_t>(std::round(lp->GetSolutionValue(positive_var))));

  // Because our lp solution might be from higher up in the tree, it
  // is possible that value is now outside the domain of positive_var.
  // In this case, this function will return an invalid literal.
  return SplitAroundGivenValue(positive_var, value, model);
}

IntegerLiteral SplitUsingBestSolutionValueInRepository(
    IntegerVariable var, const SharedSolutionRepository<int64_t>& solution_repo,
    Model* model) {
  if (solution_repo.NumSolutions() == 0) {
    return IntegerLiteral();
  }

  const IntegerVariable positive_var = PositiveVariable(var);
  const int proto_var =
      model->Get<CpModelMapping>()->GetProtoVariableFromIntegerVariable(
          positive_var);

  if (proto_var < 0) {
    return IntegerLiteral();
  }

  const IntegerValue value(solution_repo.GetVariableValueInSolution(
      proto_var, /*solution_index=*/0));
  return SplitAroundGivenValue(positive_var, value, model);
}

// TODO(user): the complexity caused by the linear scan in this heuristic and
// the one below is ok when search_branching is set to SAT_SEARCH because it is
// not executed often, but otherwise it is done for each search decision,
// which seems expensive. Improve.
std::function<BooleanOrIntegerLiteral()> FirstUnassignedVarAtItsMinHeuristic(
    absl::Span<const IntegerVariable> vars, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return
      [/*copy*/ vars = std::vector<IntegerVariable>(vars.begin(), vars.end()),
       integer_trail]() {
        for (const IntegerVariable var : vars) {
          const IntegerLiteral decision = AtMinValue(var, integer_trail);
          if (decision.IsValid()) return BooleanOrIntegerLiteral(decision);
        }
        return BooleanOrIntegerLiteral();
      };
}

std::function<BooleanOrIntegerLiteral()> MostFractionalHeuristic(Model* model) {
  auto* lp_values = model->GetOrCreate<ModelLpValues>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [lp_values, integer_trail, model]() {
    double best_fractionality = 0.0;
    BooleanOrIntegerLiteral decision;
    for (IntegerVariable var(0); var < lp_values->size(); var += 2) {
      if (integer_trail->IsFixed(var)) continue;
      const double lp_value = (*lp_values)[var];
      const double fractionality = std::abs(lp_value - std::round(lp_value));
      if (fractionality > best_fractionality) {
        best_fractionality = fractionality;

        // This choose <= value if possible.
        decision = BooleanOrIntegerLiteral(SplitAroundGivenValue(
            var, IntegerValue(std::floor(lp_value)), model));
      }
    }
    return decision;
  };
}

std::function<BooleanOrIntegerLiteral()> BoolPseudoCostHeuristic(Model* model) {
  auto* lp_values = model->GetOrCreate<ModelLpValues>();
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* pseudo_costs = model->GetOrCreate<PseudoCosts>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [lp_values, encoder, pseudo_costs, integer_trail]() {
    double best_score = 0.0;
    BooleanOrIntegerLiteral decision;
    for (IntegerVariable var(0); var < lp_values->size(); var += 2) {
      // Only look at non-fixed booleans.
      const IntegerValue lb = integer_trail->LowerBound(var);
      const IntegerValue ub = integer_trail->UpperBound(var);
      if (lb != 0 || ub != 1) continue;

      // Get associated literal.
      const LiteralIndex index =
          encoder->GetAssociatedLiteral(IntegerLiteral::GreaterOrEqual(var, 1));
      if (index == kNoLiteralIndex) continue;

      const double lp_value = (*lp_values)[var];
      const double score =
          pseudo_costs->BoolPseudoCost(Literal(index), lp_value);
      if (score > best_score) {
        best_score = score;
        decision = BooleanOrIntegerLiteral(Literal(index));
      }
    }
    return decision;
  };
}

std::function<BooleanOrIntegerLiteral()> LpPseudoCostHeuristic(Model* model) {
  auto* lp_values = model->GetOrCreate<ModelLpValues>();
  auto* pseudo_costs = model->GetOrCreate<PseudoCosts>();
  return [lp_values, pseudo_costs]() {
    double best_score = 0.0;
    BooleanOrIntegerLiteral decision;
    for (IntegerVariable var(0); var < lp_values->size(); var += 2) {
      const PseudoCosts::BranchingInfo info =
          pseudo_costs->EvaluateVar(var, *lp_values);
      if (info.is_fixed) continue;

      // When not reliable, we skip integer.
      //
      // TODO(user): Use strong branching when not reliable.
      // TODO(user): do not branch on integer lp? however it seems better to
      // do that !? Maybe this is because if it has a high pseudo cost
      // average, it is good anyway?
      if (!info.is_reliable && info.is_integer) continue;

      // We delay to subsequent heuristic if the score is 0.0.
      if (info.score > best_score) {
        best_score = info.score;

        // This direction works better than the inverse in the benchs. But
        // always branching up seems even better. TODO(user): investigate.
        if (info.down_score > info.up_score) {
          decision = BooleanOrIntegerLiteral(info.down_branch);
        } else {
          decision = BooleanOrIntegerLiteral(info.down_branch.Negated());
        }
      }
    }
    return decision;
  };
}

std::function<BooleanOrIntegerLiteral()>
UnassignedVarWithLowestMinAtItsMinHeuristic(
    absl::Span<const IntegerVariable> vars, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return
      [/*copy */ vars = std::vector<IntegerVariable>(vars.begin(), vars.end()),
       integer_trail]() {
        IntegerVariable candidate = kNoIntegerVariable;
        IntegerValue candidate_lb;
        for (const IntegerVariable var : vars) {
          const IntegerValue lb = integer_trail->LowerBound(var);
          if (lb < integer_trail->UpperBound(var) &&
              (candidate == kNoIntegerVariable || lb < candidate_lb)) {
            candidate = var;
            candidate_lb = lb;
          }
        }
        if (candidate == kNoIntegerVariable) return BooleanOrIntegerLiteral();
        return BooleanOrIntegerLiteral(AtMinValue(candidate, integer_trail));
      };
}

std::function<BooleanOrIntegerLiteral()> SequentialSearch(
    std::vector<std::function<BooleanOrIntegerLiteral()>> heuristics) {
  return [heuristics]() {
    for (const auto& h : heuristics) {
      const BooleanOrIntegerLiteral decision = h();
      if (decision.HasValue()) return decision;
    }
    return BooleanOrIntegerLiteral();
  };
}

std::function<BooleanOrIntegerLiteral()> SequentialValueSelection(
    std::vector<std::function<IntegerLiteral(IntegerVariable)>>
        value_selection_heuristics,
    std::function<BooleanOrIntegerLiteral()> var_selection_heuristic,
    Model* model) {
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* sat_policy = model->GetOrCreate<SatDecisionPolicy>();
  return [=]() {
    // Get the current decision.
    const BooleanOrIntegerLiteral current_decision = var_selection_heuristic();
    if (!current_decision.HasValue()) return current_decision;

    // When we are in the "stable" phase, we prefer to follow the SAT polarity
    // heuristic.
    if (current_decision.boolean_literal_index != kNoLiteralIndex &&
        sat_policy->InStablePhase()) {
      return current_decision;
    }

    // IntegerLiteral case.
    if (current_decision.boolean_literal_index == kNoLiteralIndex) {
      for (const auto& value_heuristic : value_selection_heuristics) {
        const IntegerLiteral decision =
            value_heuristic(current_decision.integer_literal.var);
        if (decision.IsValid()) return BooleanOrIntegerLiteral(decision);
      }
      return current_decision;
    }

    // Boolean case. We try to decode the Boolean decision to see if it is
    // associated with an integer variable.
    //
    // TODO(user): we will likely stop at the first non-fixed variable.
    for (const IntegerVariable var : encoder->GetAllAssociatedVariables(
             Literal(current_decision.boolean_literal_index))) {
      // Sequentially try the value selection heuristics.
      for (const auto& value_heuristic : value_selection_heuristics) {
        const IntegerLiteral decision = value_heuristic(var);
        if (decision.IsValid()) return BooleanOrIntegerLiteral(decision);
      }
    }

    return current_decision;
  };
}

bool LinearizedPartIsLarge(Model* model) {
  auto* lp_constraints =
      model->GetOrCreate<LinearProgrammingConstraintCollection>();
  int num_lp_variables = 0;
  for (LinearProgrammingConstraint* lp : *lp_constraints) {
    num_lp_variables += lp->NumVariables();
  }
  const int num_integer_variables =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value() / 2;
  return (num_integer_variables <= 2 * num_lp_variables);
}

// Note that all these heuristic do not depend on the variable being positive
// or negative.
//
// TODO(user): Experiment more with value selection heuristics.
std::function<BooleanOrIntegerLiteral()> IntegerValueSelectionHeuristic(
    std::function<BooleanOrIntegerLiteral()> var_selection_heuristic,
    Model* model) {
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  std::vector<std::function<IntegerLiteral(IntegerVariable)>>
      value_selection_heuristics;

  // LP based value.
  //
  // Note that we only do this if a big enough percentage of the problem
  // variables appear in the LP relaxation.
  if (LinearizedPartIsLarge(model) &&
      (parameters.exploit_integer_lp_solution() ||
       parameters.exploit_all_lp_solution())) {
    value_selection_heuristics.push_back([model](IntegerVariable var) {
      return SplitAroundLpValue(PositiveVariable(var), model);
    });
  }

  // Solution based value.
  if (parameters.exploit_best_solution()) {
    auto* response_manager = model->Get<SharedResponseManager>();
    if (response_manager != nullptr) {
      VLOG(3) << "Using best solution value selection heuristic.";
      value_selection_heuristics.push_back(
          [model, response_manager](IntegerVariable var) {
            return SplitUsingBestSolutionValueInRepository(
                var, response_manager->SolutionsRepository(), model);
          });
    }
  }

  // Objective based value.
  if (parameters.exploit_objective()) {
    auto* integer_trail = model->GetOrCreate<IntegerTrail>();
    auto* objective_definition = model->GetOrCreate<ObjectiveDefinition>();
    value_selection_heuristics.push_back([integer_trail, objective_definition](
                                             IntegerVariable var) {
      return ChooseBestObjectiveValue(var, integer_trail, objective_definition);
    });
  }

  return SequentialValueSelection(value_selection_heuristics,
                                  var_selection_heuristic, model);
}

std::function<BooleanOrIntegerLiteral()> SatSolverHeuristic(Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  Trail* trail = model->GetOrCreate<Trail>();
  SatDecisionPolicy* decision_policy = model->GetOrCreate<SatDecisionPolicy>();
  return [sat_solver, trail, decision_policy] {
    const bool all_assigned = trail->Index() == sat_solver->NumVariables();
    if (all_assigned) return BooleanOrIntegerLiteral();
    const Literal result = decision_policy->NextBranch();
    CHECK(!sat_solver->Assignment().LiteralIsAssigned(result));
    return BooleanOrIntegerLiteral(result.Index());
  };
}

// TODO(user): Do we need a mechanism to reduce the range of possible gaps
// when nothing gets proven? This could be a parameter or some adaptative code.
std::function<BooleanOrIntegerLiteral()> ShaveObjectiveLb(Model* model) {
  auto* objective_definition = model->GetOrCreate<ObjectiveDefinition>();
  const IntegerVariable obj_var = objective_definition->objective_var;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* random = model->GetOrCreate<ModelRandomGenerator>();

  return [obj_var, integer_trail, sat_solver, random]() {
    BooleanOrIntegerLiteral result;
    const int level = sat_solver->CurrentDecisionLevel();
    if (level > 0 || obj_var == kNoIntegerVariable) return result;

    const IntegerValue obj_lb = integer_trail->LowerBound(obj_var);
    const IntegerValue obj_ub = integer_trail->UpperBound(obj_var);
    if (obj_lb == obj_ub) return result;
    const IntegerValue mid = (obj_ub - obj_lb) / 2;
    const IntegerValue new_ub =
        obj_lb + absl::LogUniform<int64_t>(*random, 0, mid.value());

    result.integer_literal = IntegerLiteral::LowerOrEqual(obj_var, new_ub);
    return result;
  };
}

std::function<BooleanOrIntegerLiteral()> PseudoCost(Model* model) {
  auto* objective = model->Get<ObjectiveDefinition>();
  const bool has_objective =
      objective != nullptr && objective->objective_var != kNoIntegerVariable;
  if (!has_objective) {
    return []() { return BooleanOrIntegerLiteral(); };
  }

  auto* pseudo_costs = model->GetOrCreate<PseudoCosts>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [pseudo_costs, integer_trail]() {
    const IntegerVariable chosen_var = pseudo_costs->GetBestDecisionVar();
    if (chosen_var == kNoIntegerVariable) return BooleanOrIntegerLiteral();

    // TODO(user): This will be overridden by the value decision heuristic in
    // almost all cases.
    return BooleanOrIntegerLiteral(
        GreaterOrEqualToMiddleValue(chosen_var, integer_trail));
  };
}

// A simple heuristic for scheduling models.
std::function<BooleanOrIntegerLiteral()> SchedulingSearchHeuristic(
    Model* model) {
  auto* repo = model->GetOrCreate<IntervalsRepository>();
  auto* heuristic = model->GetOrCreate<SearchHeuristics>();
  auto* trail = model->GetOrCreate<Trail>();
  auto* watcher = model->GetOrCreate<GenericLiteralWatcher>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  const int64_t randomization_size = std::max<int64_t>(
      1,
      model->GetOrCreate<SatParameters>()->search_random_variable_pool_size());
  auto* random = model->GetOrCreate<ModelRandomGenerator>();

  // To avoid to scan already fixed intervals, we use a simple reversible int.
  auto* rev_int_repo = model->GetOrCreate<RevIntRepository>();
  const int num_intervals = repo->NumIntervals();
  int rev_fixed = 0;
  bool rev_is_in_dive = false;
  std::vector<IntervalVariable> intervals(num_intervals);
  std::vector<IntegerValue> cached_start_mins(num_intervals);
  for (IntervalVariable i(0); i < num_intervals; ++i) {
    intervals[i.value()] = i;
  }

  // Note(user): only the model is captured for no reason.
  return [=]() mutable {
    struct ToSchedule {
      // Variable to fix.
      LiteralIndex presence = kNoLiteralIndex;
      AffineExpression start;
      AffineExpression end;

      // Information to select best.
      IntegerValue size_min = kMaxIntegerValue;
      IntegerValue start_min = kMaxIntegerValue;
      IntegerValue start_max = kMaxIntegerValue;
      double noise = 0.5;

      // We want to pack interval to the left. If two have the same start_min,
      // we want to choose the one that will likely leave an easier problem for
      // the other tasks.
      bool operator<(const ToSchedule& other) const {
        return std::tie(start_min, start_max, size_min, noise) <
               std::tie(other.start_min, other.start_max, other.size_min,
                        other.noise);
      }

      // Generating random noise can take time, so we use this function to
      // delay it.
      bool MightBeBetter(const ToSchedule& other) const {
        return std::tie(start_min, start_max) <=
               std::tie(other.start_min, other.start_max);
      }
    };
    std::vector<ToSchedule> top_decisions;
    top_decisions.reserve(randomization_size);
    top_decisions.resize(1);

    // Save rev_fixed before we modify it.
    rev_int_repo->SaveState(&rev_fixed);

    // TODO(user): we should also precompute fixed precedences and only fix
    // interval that have all their predecessors fixed.
    for (int i = rev_fixed; i < num_intervals; ++i) {
      const ToSchedule& worst = top_decisions.back();
      if (rev_is_in_dive && cached_start_mins[i] > worst.start_min) {
        continue;
      }

      const IntervalVariable interval = intervals[i];
      if (repo->IsAbsent(interval)) {
        std::swap(intervals[i], intervals[rev_fixed]);
        std::swap(cached_start_mins[i], cached_start_mins[rev_fixed]);
        ++rev_fixed;
        continue;
      }

      const AffineExpression start = repo->Start(interval);
      const AffineExpression end = repo->End(interval);
      if (repo->IsPresent(interval) && integer_trail->IsFixed(start) &&
          integer_trail->IsFixed(end)) {
        std::swap(intervals[i], intervals[rev_fixed]);
        std::swap(cached_start_mins[i], cached_start_mins[rev_fixed]);
        ++rev_fixed;
        continue;
      }

      ToSchedule candidate;
      if (repo->IsOptional(interval)) {
        // For task whose presence is still unknown, our propagators should
        // have propagated the minimum time as if it was present. So this
        // should reflect the earliest time at which this interval can be
        // scheduled.
        const Literal lit = repo->PresenceLiteral(interval);
        candidate.start_min = integer_trail->ConditionalLowerBound(lit, start);
        candidate.start_max = integer_trail->ConditionalUpperBound(lit, start);
      } else {
        candidate.start_min = integer_trail->LowerBound(start);
        candidate.start_max = integer_trail->UpperBound(start);
      }
      cached_start_mins[i] = candidate.start_min;
      if (top_decisions.size() < randomization_size ||
          candidate.MightBeBetter(worst)) {
        // Finish filling candidate.
        //
        // For variable size, we compute the min size once the start is fixed
        // to time. This is needed to never pick the "artificial" makespan
        // interval at the end in priority compared to intervals that still
        // need to be scheduled.
        candidate.start = start;
        candidate.end = end;
        candidate.presence = repo->IsOptional(interval)
                                 ? repo->PresenceLiteral(interval).Index()
                                 : kNoLiteralIndex;
        candidate.size_min =
            std::max(integer_trail->LowerBound(repo->Size(interval)),
                     integer_trail->LowerBound(end) - candidate.start_min);
        candidate.noise = absl::Uniform(*random, 0.0, 1.0);

        if (top_decisions.size() == randomization_size) {
          // Do not replace if we have a strict inequality now.
          if (worst < candidate) continue;
          top_decisions.pop_back();
        }
        top_decisions.push_back(candidate);
        if (top_decisions.size() > 1) {
          std::sort(top_decisions.begin(), top_decisions.end());
        }
      }
    }

    // Setup rev_is_in_dive to be true on the next call only if there was no
    // backtrack since the previous call.
    watcher->SetUntilNextBacktrack(&rev_is_in_dive);

    const ToSchedule best =
        top_decisions.size() == 1
            ? top_decisions.front()
            : top_decisions[absl::Uniform(
                  *random, 0, static_cast<int>(top_decisions.size()))];
    if (top_decisions.size() > 1) {
      VLOG(2) << "Choose among " << top_decisions.size() << " "
              << best.start_min << " " << best.size_min
              << "[t=" << top_decisions.front().start_min
              << ", s=" << top_decisions.front().size_min
              << ", t=" << top_decisions.back().start_min
              << ", s=" << top_decisions.back().size_min << "]";
    }
    if (best.start_min == kMaxIntegerValue) return BooleanOrIntegerLiteral();

    // Use the next_decision_override to fix in turn all the variables from
    // the selected interval.
    int num_times = 0;
    heuristic->next_decision_override = [trail, integer_trail, best,
                                         num_times]() mutable {
      if (++num_times > 5) {
        // We have been trying to fix this interval for a while. Do we miss
        // some propagation? In any case, try to see if the heuristic above
        // would select something else.
        VLOG(3) << "Skipping ... ";
        return BooleanOrIntegerLiteral();
      }

      // First make sure the interval is present.
      if (best.presence != kNoLiteralIndex) {
        if (!trail->Assignment().LiteralIsAssigned(Literal(best.presence))) {
          VLOG(3) << "assign " << best.presence;
          return BooleanOrIntegerLiteral(best.presence);
        }
        if (trail->Assignment().LiteralIsFalse(Literal(best.presence))) {
          VLOG(2) << "unperformed.";
          return BooleanOrIntegerLiteral();
        }
      }

      // We assume that start_min is propagated by now.
      if (!integer_trail->IsFixed(best.start)) {
        const IntegerValue start_min = integer_trail->LowerBound(best.start);
        VLOG(3) << "start == " << start_min;
        return BooleanOrIntegerLiteral(best.start.LowerOrEqual(start_min));
      }

      // We assume that end_min is propagated by now.
      if (!integer_trail->IsFixed(best.end)) {
        const IntegerValue end_min = integer_trail->LowerBound(best.end);
        VLOG(3) << "end == " << end_min;
        return BooleanOrIntegerLiteral(best.end.LowerOrEqual(end_min));
      }

      // Everything is fixed, detach the override.
      const IntegerValue start = integer_trail->LowerBound(best.start);
      VLOG(2) << "Fixed @[" << start << ","
              << integer_trail->LowerBound(best.end) << "]"
              << (best.presence != kNoLiteralIndex
                      ? absl::StrCat(" presence=",
                                     Literal(best.presence).DebugString())
                      : "")
              << (best.start_min < start ? absl::StrCat(" start_at_selection=",
                                                        best.start_min.value())
                                         : "");
      return BooleanOrIntegerLiteral();
    };

    return heuristic->next_decision_override();
  };
}

namespace {

bool PrecedenceIsBetter(SchedulingConstraintHelper* helper, int a,
                        SchedulingConstraintHelper* other_helper, int other_a) {
  return std::make_tuple(helper->StartMin(a), helper->StartMax(a),
                         helper->SizeMin(a)) <
         std::make_tuple(other_helper->StartMin(other_a),
                         other_helper->StartMax(other_a),
                         other_helper->SizeMin(other_a));
}

}  // namespace

// The algo goes as follow:
// - For each disjunctive, consider the intervals by start time, consider
//   adding the first precedence between overlapping interval.
// - Take the smallest start time amongst all disjunctive.
std::function<BooleanOrIntegerLiteral()> DisjunctivePrecedenceSearchHeuristic(
    Model* model) {
  auto* repo = model->GetOrCreate<IntervalsRepository>();
  return [repo]() {
    SchedulingConstraintHelper* best_helper = nullptr;
    int best_before;
    int best_after;
    for (SchedulingConstraintHelper* helper : repo->AllDisjunctiveHelpers()) {
      if (!helper->SynchronizeAndSetTimeDirection(true)) {
        return BooleanOrIntegerLiteral();
      }

      // TODO(user): tie break by size/start-max
      // TODO(user): Use conditional lower bounds? note that in automatic search
      // all precedence will be fixed before this is called though. In fixed
      // search maybe we should use the other SchedulingSearchHeuristic().
      int a = -1;
      for (auto [b, time] : helper->TaskByIncreasingStartMin()) {
        if (helper->IsAbsent(b)) continue;
        if (a == -1 || helper->EndMin(a) <= helper->StartMin(b)) {
          a = b;
          continue;
        }

        // Swap (a,b) if they have the same start_min.
        if (PrecedenceIsBetter(helper, b, helper, a)) {
          std::swap(a, b);

          // Corner case in case b can fit before a (size zero)
          if (helper->EndMin(a) <= helper->StartMin(b)) {
            a = b;
            continue;
          }
        }

        // TODO(Fdid): Also compare the second part of the precedence in
        // PrecedenceIsBetter() and not just the interval before?
        if (best_helper == nullptr ||
            PrecedenceIsBetter(helper, a, best_helper, best_before)) {
          best_helper = helper;
          best_before = a;
          best_after = b;
        }
        break;
      }
    }

    if (best_helper != nullptr) {
      VLOG(2) << "New disjunctive precedence: "
              << best_helper->TaskDebugString(best_before) << " "
              << best_helper->TaskDebugString(best_after);
      const IntervalVariable a = best_helper->IntervalVariables()[best_before];
      const IntervalVariable b = best_helper->IntervalVariables()[best_after];
      repo->CreateDisjunctivePrecedenceLiteral(a, b);
      return BooleanOrIntegerLiteral(repo->GetPrecedenceLiteral(a, b));
    }

    return BooleanOrIntegerLiteral();
  };
}

// The algo goes as follow:
// - Build a profile of all the tasks packed to the right as long as that is
//   feasible.
// - If we can't grow the profile, we have identified a set of tasks that all
//   overlap if they are packed on the right, and whose sum of demand exceed
//   the capacity.
// - Look for two tasks in that set that can be made non-overlapping, and take
//   a "precedence" decision between them.
std::function<BooleanOrIntegerLiteral()> CumulativePrecedenceSearchHeuristic(
    Model* model) {
  auto* repo = model->GetOrCreate<IntervalsRepository>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* trail = model->GetOrCreate<Trail>();
  auto* search_helper = model->GetOrCreate<IntegerSearchHelper>();
  return [repo, integer_trail, trail, search_helper]() {
    SchedulingConstraintHelper* best_helper = nullptr;
    int best_before = 0;
    int best_after = 0;
    for (const auto h : repo->AllCumulativeHelpers()) {
      auto* helper = h.task_helper;
      if (!helper->SynchronizeAndSetTimeDirection(true)) {
        return BooleanOrIntegerLiteral();
      }

      const int num_tasks = helper->NumTasks();
      std::vector<IntegerValue> added_demand(num_tasks, 0);

      // We use a similar algo as in BuildProfile() in timetable.cc
      const auto& by_smin = helper->TaskByIncreasingStartMin();
      const auto& by_emin = helper->TaskByIncreasingEndMin();
      const IntegerValue capacity_max = integer_trail->UpperBound(h.capacity);

      // Start and height of the currently built profile rectangle.
      IntegerValue current_height = 0;
      int first_skipped_task = -1;

      int next_end = 0;
      int next_start = 0;
      int num_added = 0;
      bool found = false;
      while (!found && next_end < num_tasks) {
        IntegerValue time = by_emin[next_end].time;
        if (next_start < num_tasks) {
          time = std::min(time, by_smin[next_start].time);
        }

        // Remove added task ending there.
        // Set their demand to zero.
        while (next_end < num_tasks && by_emin[next_end].time == time) {
          const int t = by_emin[next_end].task_index;
          if (!helper->IsPresent(t)) continue;
          if (added_demand[t] > 0) {
            current_height -= added_demand[t];
            added_demand[t] = 0;
          } else {
            // Corner case if task is of duration zero.
            added_demand[t] = -1;
          }
          ++next_end;
        }

        // Add new task starting here.
        // If the task cannot be added we have a candidate for precedence.
        // TODO(user): tie-break tasks not fitting in the profile smartly.
        while (next_start < num_tasks && by_smin[next_start].time == time) {
          const int t = by_smin[next_start].task_index;
          if (!helper->IsPresent(t)) continue;
          if (added_demand[t] == -1) continue;  // Corner case.
          const IntegerValue demand_min = h.demand_helper->DemandMin(t);
          if (current_height + demand_min <= capacity_max) {
            ++num_added;
            added_demand[t] = demand_min;
            current_height += demand_min;
          } else if (first_skipped_task == -1) {
            // We should have everything needed here to add a new precedence.
            first_skipped_task = t;
            found = true;
            break;
          }
          ++next_start;
        }
      }

      // If packing everything to the left is feasible, continue.
      if (first_skipped_task == -1) {
        CHECK_EQ(num_added, num_tasks);
        continue;
      }

      // We will use a bunch of heuristic to add a new precedence. All the task
      // in open_tasks cannot share a time point since they exceed the capacity.
      // Moreover if we pack all to the left, they have an intersecting point.
      // So we should be able to make two of them disjoint
      std::vector<int> open_tasks;
      for (int t = 0; t < num_tasks; ++t) {
        if (added_demand[t] <= 0) continue;
        open_tasks.push_back(t);
      }
      open_tasks.push_back(first_skipped_task);

      // TODO(user): If the two box cannot overlap because of high demand, use
      // repo.CreateDisjunctivePrecedenceLiteral() instead.
      //
      // TODO(user): Add heuristic ordering for creating interesting precedence
      // first.
      bool found_precedence_to_add = false;
      std::vector<Literal> conflict;
      helper->ClearReason();
      for (const int s : open_tasks) {
        for (const int t : open_tasks) {
          if (s == t) continue;

          // Can we add s <= t ?
          // All the considered tasks are intersecting if on the left.
          CHECK_LT(helper->StartMin(s), helper->EndMin(t));
          CHECK_LT(helper->StartMin(t), helper->EndMin(s));

          // skip if we already have a literal created and assigned to false.
          const IntervalVariable a = helper->IntervalVariables()[s];
          const IntervalVariable b = helper->IntervalVariables()[t];
          const LiteralIndex existing = repo->GetPrecedenceLiteral(a, b);
          if (existing != kNoLiteralIndex) {
            // It shouldn't be able to be true here otherwise we will have s and
            // t disjoint.
            CHECK(!trail->Assignment().LiteralIsTrue(Literal(existing)))
                << helper->TaskDebugString(s) << " ( <= ?) "
                << helper->TaskDebugString(t);

            // This should always be true in normal usage after SAT search has
            // fixed all literal, but if it is not, we can just return this
            // decision.
            if (trail->Assignment().LiteralIsFalse(Literal(existing))) {
              conflict.push_back(Literal(existing));
              continue;
            }
          } else {
            // Make sure s could be before t.
            if (helper->EndMin(s) > helper->StartMax(t)) {
              helper->AddReasonForBeingBefore(t, s);
              continue;
            }

            // It shouldn't be able to fail since s can be before t.
            CHECK(repo->CreatePrecedenceLiteral(a, b));
          }

          // Branch on that precedence.
          best_helper = helper;
          best_before = s;
          best_after = t;
          found_precedence_to_add = true;
          break;
        }
        if (found_precedence_to_add) break;
      }
      if (found_precedence_to_add) break;

      // If no precedence can be created, and all precedence are assigned to
      // false we have a conflict since all these interval must intersect but
      // cannot fit in the capacity!
      //
      // TODO(user): We need to add the reason for demand_min and capacity_max.
      // TODO(user): unfortunately we can't report it from here.
      std::vector<IntegerLiteral> integer_reason =
          *helper->MutableIntegerReason();
      if (!h.capacity.IsConstant()) {
        integer_reason.push_back(
            integer_trail->UpperBoundAsLiteral(h.capacity));
      }
      const auto& demands = h.demand_helper->Demands();
      for (const int t : open_tasks) {
        if (helper->IsOptional(t)) {
          CHECK(trail->Assignment().LiteralIsTrue(helper->PresenceLiteral(t)));
          conflict.push_back(helper->PresenceLiteral(t).Negated());
        }
        const AffineExpression d = demands[t];
        if (!d.IsConstant()) {
          integer_reason.push_back(integer_trail->LowerBoundAsLiteral(d));
        }
      }
      integer_trail->ReportConflict(conflict, integer_reason);
      search_helper->NotifyThatConflictWasFoundDuringGetDecision();
      if (VLOG_IS_ON(2)) {
        LOG(INFO) << "Conflict between precedences !";
        for (const int t : open_tasks) LOG(INFO) << helper->TaskDebugString(t);
      }
      return BooleanOrIntegerLiteral();
    }

    // TODO(user): add heuristic criteria, right now we stop at first
    // one. See above.
    if (best_helper != nullptr) {
      VLOG(2) << "New precedence: " << best_helper->TaskDebugString(best_before)
              << " " << best_helper->TaskDebugString(best_after);
      const IntervalVariable a = best_helper->IntervalVariables()[best_before];
      const IntervalVariable b = best_helper->IntervalVariables()[best_after];
      repo->CreatePrecedenceLiteral(a, b);
      return BooleanOrIntegerLiteral(repo->GetPrecedenceLiteral(a, b));
    }

    return BooleanOrIntegerLiteral();
  };
}

std::function<BooleanOrIntegerLiteral()> RandomizeOnRestartHeuristic(
    bool lns_mode, Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  SatDecisionPolicy* decision_policy = model->GetOrCreate<SatDecisionPolicy>();
  SearchHeuristics& heuristics = *model->GetOrCreate<SearchHeuristics>();

  // TODO(user): Add other policies and perform more experiments.
  std::function<BooleanOrIntegerLiteral()> sat_policy =
      SatSolverHeuristic(model);
  std::vector<std::function<BooleanOrIntegerLiteral()>> policies;
  std::vector<double> weights;

  // Add sat search + fixed_search (to complete the search).
  policies.push_back(SequentialSearch({sat_policy, heuristics.fixed_search}));
  weights.push_back(5);

  // Adds user defined search if present.
  if (heuristics.user_search != nullptr) {
    policies.push_back(SequentialSearch(
        {heuristics.user_search, sat_policy, heuristics.fixed_search}));
    weights.push_back(1);
  }

  // Always add heuristic search.
  policies.push_back(SequentialSearch({heuristics.heuristic_search, sat_policy,
                                       heuristics.integer_completion_search}));
  weights.push_back(1);

  // The higher weight for the sat policy is because this policy actually
  // contains a lot of variation as we randomize the sat parameters.
  // TODO(user): Do more experiments to find better distribution.
  std::discrete_distribution<int> var_dist(weights.begin(), weights.end());

  // Value selection.
  std::vector<std::function<IntegerLiteral(IntegerVariable)>>
      value_selection_heuristics;
  std::vector<int> value_selection_weight;

  // LP Based value.
  const int linearization_level =
      model->GetOrCreate<SatParameters>()->linearization_level();
  if (LinearizedPartIsLarge(model)) {
    value_selection_heuristics.push_back([model](IntegerVariable var) {
      return SplitAroundLpValue(PositiveVariable(var), model);
    });
    value_selection_weight.push_back(linearization_level == 2 ? 4 : 2);
  }

  // Solution based value.
  if (!lns_mode) {
    auto* response_manager = model->Get<SharedResponseManager>();
    CHECK(response_manager != nullptr);
    value_selection_heuristics.push_back(
        [model, response_manager](IntegerVariable var) {
          return SplitUsingBestSolutionValueInRepository(
              var, response_manager->SolutionsRepository(), model);
        });
    value_selection_weight.push_back(5);
  }

  // Min value.
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  value_selection_heuristics.push_back([integer_trail](IntegerVariable var) {
    return AtMinValue(var, integer_trail);
  });
  value_selection_weight.push_back(1);

  // Special case: Don't change the decision value.
  value_selection_weight.push_back(10);

  // TODO(user): These distribution values are just guessed values. They
  // need to be tuned.
  std::discrete_distribution<int> val_dist(value_selection_weight.begin(),
                                           value_selection_weight.end());

  int policy_index = 0;
  int val_policy_index = 0;
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  auto* objective = model->Get<ObjectiveDefinition>();
  return [=]() mutable {
    if (sat_solver->CurrentDecisionLevel() == 0) {
      auto* random = model->GetOrCreate<ModelRandomGenerator>();
      RandomizeDecisionHeuristic(*random, model->GetOrCreate<SatParameters>());
      decision_policy->ResetDecisionHeuristic();

      // Set some assignment preference.
      // TODO(user): Also use LP value as assignment like in Bop.
      if (objective != nullptr && absl::Bernoulli(*random, 0.2)) {
        // Use Boolean objective as assignment preference.
        IntegerValue max_abs_weight = 0;
        for (const IntegerValue coeff : objective->coeffs) {
          max_abs_weight = std::max(max_abs_weight, IntTypeAbs(coeff));
        }
        const double max_abs_weight_double = ToDouble(max_abs_weight);

        const int objective_size = objective->vars.size();
        for (int i = 0; i < objective_size; ++i) {
          const IntegerVariable var = objective->vars[i];
          if (integer_trail->LowerBound(var) != 0) continue;
          if (integer_trail->UpperBound(var) != 1) continue;
          const LiteralIndex index = encoder->GetAssociatedLiteral(
              IntegerLiteral::GreaterOrEqual(var, 1));
          if (index == kNoLiteralIndex) continue;

          const Literal literal(index);
          const IntegerValue coeff = objective->coeffs[i];
          const double abs_weight =
              std::abs(ToDouble(objective->coeffs[i])) / max_abs_weight_double;

          // Because this is a minimization problem, we prefer to assign a
          // Boolean variable to its "low" objective value. So if a literal
          // has a positive weight when true, we want to set it to false.
          decision_policy->SetAssignmentPreference(
              coeff > 0 ? literal.Negated() : literal, abs_weight);
        }
      }

      // Select the variable selection heuristic.
      policy_index = var_dist(*(random));

      // Select the value selection heuristic.
      val_policy_index = val_dist(*(random));
    }

    // Get the current decision.
    const BooleanOrIntegerLiteral current_decision = policies[policy_index]();
    if (!current_decision.HasValue()) return current_decision;

    // Special case: Don't override the decision value.
    if (val_policy_index >= value_selection_heuristics.size()) {
      return current_decision;
    }

    if (current_decision.boolean_literal_index == kNoLiteralIndex) {
      const IntegerLiteral new_decision =
          value_selection_heuristics[val_policy_index](
              current_decision.integer_literal.var);
      if (new_decision.IsValid()) return BooleanOrIntegerLiteral(new_decision);
      return current_decision;
    }

    // Decode the decision and get the variable.
    for (const IntegerVariable var : encoder->GetAllAssociatedVariables(
             Literal(current_decision.boolean_literal_index))) {
      // Try the selected policy.
      const IntegerLiteral new_decision =
          value_selection_heuristics[val_policy_index](var);
      if (new_decision.IsValid()) return BooleanOrIntegerLiteral(new_decision);
    }

    // Selected policy failed. Revert back to original decision.
    return current_decision;
  };
}

std::function<BooleanOrIntegerLiteral()> FollowHint(
    absl::Span<const BooleanOrIntegerVariable> vars,
    absl::Span<const IntegerValue> values, Model* model) {
  auto* trail = model->GetOrCreate<Trail>();
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* rev_int_repo = model->GetOrCreate<RevIntRepository>();

  // This is not ideal as we reserve an int for the full duration of the model
  // even if we use this FollowHint() function just for a while. But it is
  // an easy solution to not have reference to deleted memory in the
  // RevIntRepository(). Note that once we backtrack, these reference will
  // disappear.
  int* rev_start_index = model->TakeOwnership(new int);
  *rev_start_index = 0;

  return [=,
          vars =
              std::vector<BooleanOrIntegerVariable>(vars.begin(), vars.end()),
          values = std::vector<IntegerValue>(values.begin(), values.end())]() {
    rev_int_repo->SaveState(rev_start_index);
    for (int i = *rev_start_index; i < vars.size(); ++i) {
      const IntegerValue value = values[i];
      if (vars[i].bool_var != kNoBooleanVariable) {
        if (trail->Assignment().VariableIsAssigned(vars[i].bool_var)) continue;

        // If we retake a decision at this level, we will restart from i.
        *rev_start_index = i;
        return BooleanOrIntegerLiteral(
            Literal(vars[i].bool_var, value == 1).Index());
      } else {
        const IntegerVariable integer_var = vars[i].int_var;
        if (integer_trail->IsFixed(integer_var)) continue;

        const IntegerVariable positive_var = PositiveVariable(integer_var);
        const IntegerLiteral decision = SplitAroundGivenValue(
            positive_var, positive_var != integer_var ? -value : value, model);
        if (decision.IsValid()) {
          // If we retake a decision at this level, we will restart from i.
          *rev_start_index = i;
          return BooleanOrIntegerLiteral(decision);
        }

        // If the value is outside the current possible domain, we skip it.
        continue;
      }
    }
    return BooleanOrIntegerLiteral();
  };
}

std::function<bool()> RestartEveryKFailures(int k, SatSolver* solver) {
  bool reset_at_next_call = true;
  int next_num_failures = 0;
  return [=]() mutable {
    if (reset_at_next_call) {
      next_num_failures = solver->num_failures() + k;
      reset_at_next_call = false;
    } else if (solver->num_failures() >= next_num_failures) {
      reset_at_next_call = true;
    }
    return reset_at_next_call;
  };
}

std::function<bool()> SatSolverRestartPolicy(Model* model) {
  auto policy = model->GetOrCreate<RestartPolicy>();
  return [policy]() { return policy->ShouldRestart(); };
}

namespace {

std::function<BooleanOrIntegerLiteral()> WrapIntegerLiteralHeuristic(
    std::function<IntegerLiteral()> f) {
  return [f]() { return BooleanOrIntegerLiteral(f()); };
}

}  // namespace

void ConfigureSearchHeuristics(Model* model) {
  SearchHeuristics& heuristics = *model->GetOrCreate<SearchHeuristics>();
  CHECK(heuristics.fixed_search != nullptr);
  heuristics.policy_index = 0;
  heuristics.decision_policies.clear();
  heuristics.restart_policies.clear();

  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  switch (parameters.search_branching()) {
    case SatParameters::AUTOMATIC_SEARCH: {
      std::function<BooleanOrIntegerLiteral()> decision_policy =
          IntegerValueSelectionHeuristic(
              SequentialSearch(
                  {SatSolverHeuristic(model), heuristics.fixed_search}),
              model);
      if (parameters.use_objective_lb_search()) {
        heuristics.decision_policies = {
            SequentialSearch({ShaveObjectiveLb(model), decision_policy})};
      } else {
        heuristics.decision_policies = {decision_policy};
      }
      heuristics.restart_policies = {SatSolverRestartPolicy(model)};
      return;
    }
    case SatParameters::FIXED_SEARCH: {
      // Not all Boolean might appear in fixed_search(), so once there is no
      // decision left, we fix all Booleans that are still undecided.
      heuristics.decision_policies = {SequentialSearch(
          {heuristics.fixed_search, SatSolverHeuristic(model)})};

      if (parameters.randomize_search()) {
        heuristics.restart_policies = {SatSolverRestartPolicy(model)};
        return;
      }

      // TODO(user): We might want to restart if external info is available.
      // Code a custom restart for this?
      auto no_restart = []() { return false; };
      heuristics.restart_policies = {no_restart};
      return;
    }
    case SatParameters::PARTIAL_FIXED_SEARCH: {
      // Push user search if present.
      if (heuristics.user_search != nullptr) {
        heuristics.decision_policies.push_back(
            SequentialSearch({heuristics.user_search, SatSolverHeuristic(model),
                              heuristics.fixed_search}));
      }

      // Do a portfolio with the default sat heuristics.
      heuristics.decision_policies.push_back(SequentialSearch(
          {SatSolverHeuristic(model), heuristics.fixed_search}));

      // Use default restart policies.
      heuristics.restart_policies.assign(heuristics.decision_policies.size(),
                                         SatSolverRestartPolicy(model));
      return;
    }
    case SatParameters::HINT_SEARCH: {
      CHECK(heuristics.hint_search != nullptr);
      heuristics.decision_policies = {
          SequentialSearch({heuristics.hint_search, SatSolverHeuristic(model),
                            heuristics.fixed_search})};
      auto no_restart = []() { return false; };
      heuristics.restart_policies = {no_restart};
      return;
    }
    case SatParameters::PORTFOLIO_SEARCH: {
      heuristics.decision_policies = {
          RandomizeOnRestartHeuristic(/*lns_mode=*/true, model)};
      heuristics.restart_policies = {SatSolverRestartPolicy(model)};
      return;
    }
    case SatParameters::LP_SEARCH: {
      std::vector<std::function<BooleanOrIntegerLiteral()>> lp_heuristics;
      for (const auto& ct :
           *(model->GetOrCreate<LinearProgrammingConstraintCollection>())) {
        lp_heuristics.push_back(WrapIntegerLiteralHeuristic(
            ct->HeuristicLpReducedCostAverageBranching()));
      }
      if (lp_heuristics.empty()) {  // Revert to fixed search.
        heuristics.decision_policies = {SequentialSearch(
            {heuristics.fixed_search, SatSolverHeuristic(model)})},
        heuristics.restart_policies = {SatSolverRestartPolicy(model)};
        return;
      }
      heuristics.decision_policies = CompleteHeuristics(
          lp_heuristics, IntegerValueSelectionHeuristic(
                             SequentialSearch({SatSolverHeuristic(model),
                                               heuristics.fixed_search}),
                             model));
      heuristics.restart_policies.assign(heuristics.decision_policies.size(),
                                         SatSolverRestartPolicy(model));
      return;
    }
    case SatParameters::PSEUDO_COST_SEARCH: {
      std::function<BooleanOrIntegerLiteral()> search =
          SequentialSearch({PseudoCost(model), SatSolverHeuristic(model),
                            heuristics.fixed_search});
      heuristics.decision_policies = {
          IntegerValueSelectionHeuristic(search, model)};
      heuristics.restart_policies = {SatSolverRestartPolicy(model)};
      return;
    }
    case SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH: {
      std::function<BooleanOrIntegerLiteral()> search = SequentialSearch(
          {RandomizeOnRestartHeuristic(/*lns_mode=*/false, model),
           heuristics.fixed_search});
      heuristics.decision_policies = {search};
      heuristics.restart_policies = {
          RestartEveryKFailures(10, model->GetOrCreate<SatSolver>())};
      return;
    }
    case SatParameters::RANDOMIZED_SEARCH: {
      heuristics.decision_policies = {
          RandomizeOnRestartHeuristic(/*lns_mode=*/false, model)};
      heuristics.restart_policies = {SatSolverRestartPolicy(model)};
      return;
    }
  }
}

std::vector<std::function<BooleanOrIntegerLiteral()>> CompleteHeuristics(
    absl::Span<const std::function<BooleanOrIntegerLiteral()>>
        incomplete_heuristics,
    const std::function<BooleanOrIntegerLiteral()>& completion_heuristic) {
  std::vector<std::function<BooleanOrIntegerLiteral()>> complete_heuristics;
  complete_heuristics.reserve(incomplete_heuristics.size());
  for (const auto& incomplete : incomplete_heuristics) {
    complete_heuristics.push_back(
        SequentialSearch({incomplete, completion_heuristic}));
  }
  return complete_heuristics;
}

IntegerSearchHelper::IntegerSearchHelper(Model* model)
    : parameters_(*model->GetOrCreate<SatParameters>()),
      model_(model),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      implied_bounds_(model->GetOrCreate<ImpliedBounds>()),
      prober_(model->GetOrCreate<Prober>()),
      product_detector_(model->GetOrCreate<ProductDetector>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      pseudo_costs_(model->GetOrCreate<PseudoCosts>()),
      inprocessing_(model->GetOrCreate<Inprocessing>()) {}

bool IntegerSearchHelper::BeforeTakingDecision() {
  // If we pushed root level deductions, we restart to incorporate them.
  // Note that in the present of assumptions, it is important to return to
  // the level zero first ! otherwise, the new deductions will not be
  // incorporated and the solver will loop forever.
  if (integer_trail_->HasPendingRootLevelDeduction()) {
    sat_solver_->Backtrack(0);
  }

  // The rest only trigger at level zero.
  if (sat_solver_->CurrentDecisionLevel() != 0) return true;

  // Rather than doing it in each callback, we detect newly fixed variables or
  // tighter bounds, and propagate just once when everything was added.
  const int saved_bool_index = sat_solver_->LiteralTrail().Index();
  const int saved_integer_index = integer_trail_->num_enqueues();

  auto* level_zero_callbacks = model_->GetOrCreate<LevelZeroCallbackHelper>();
  for (const auto& cb : level_zero_callbacks->callbacks) {
    if (!cb()) {
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }
  }

  // We propagate if needed.
  if (sat_solver_->LiteralTrail().Index() > saved_bool_index ||
      integer_trail_->num_enqueues() > saved_integer_index ||
      integer_trail_->HasPendingRootLevelDeduction()) {
    if (!sat_solver_->ResetToLevelZero()) return false;
  }

  if (parameters_.use_sat_inprocessing() &&
      !inprocessing_->InprocessingRound()) {
    sat_solver_->NotifyThatModelIsUnsat();
    return false;
  }

  return true;
}

LiteralIndex IntegerSearchHelper::GetDecisionLiteral(
    const BooleanOrIntegerLiteral& decision) {
  DCHECK(decision.HasValue());

  // Convert integer decision to literal one if needed.
  //
  // TODO(user): Ideally it would be cool to delay the creation even more
  // until we have a conflict with these decisions, but it is currently
  // hard to do so.
  const Literal literal =
      decision.boolean_literal_index != kNoLiteralIndex
          ? Literal(decision.boolean_literal_index)
          : encoder_->GetOrCreateAssociatedLiteral(decision.integer_literal);
  if (sat_solver_->Assignment().LiteralIsAssigned(literal)) {
    // TODO(user): It would be nicer if this can never happen. For now, it
    // does because of the Propagate() not reaching the fixed point as
    // mentioned in a TODO above. As a work-around, we display a message
    // but do not crash and recall the decision heuristic.
    VLOG(1) << "Trying to take a decision that is already assigned!"
            << " Fix this. Continuing for now...";
    return kNoLiteralIndex;
  }
  return literal.Index();
}

bool IntegerSearchHelper::GetDecision(
    const std::function<BooleanOrIntegerLiteral()>& f, LiteralIndex* decision) {
  *decision = kNoLiteralIndex;
  while (!time_limit_->LimitReached()) {
    BooleanOrIntegerLiteral new_decision;
    if (integer_trail_->InPropagationLoop()) {
      const IntegerVariable var =
          integer_trail_->NextVariableToBranchOnInPropagationLoop();
      if (var != kNoIntegerVariable) {
        new_decision.integer_literal =
            GreaterOrEqualToMiddleValue(var, integer_trail_);
      }
    }
    if (!new_decision.HasValue()) {
      new_decision = f();
      if (must_process_conflict_) {
        must_process_conflict_ = false;
        sat_solver_->ProcessCurrentConflict();
        (void)sat_solver_->FinishPropagation();
        return false;
      }
    }
    if (!new_decision.HasValue() &&
        integer_trail_->CurrentBranchHadAnIncompletePropagation()) {
      const IntegerVariable var = integer_trail_->FirstUnassignedVariable();
      if (var != kNoIntegerVariable) {
        new_decision.integer_literal = AtMinValue(var, integer_trail_);
      }
    }
    if (!new_decision.HasValue()) break;

    *decision = GetDecisionLiteral(new_decision);
    if (*decision != kNoLiteralIndex) break;
  }
  return true;
}

bool IntegerSearchHelper::TakeDecision(Literal decision) {
  pseudo_costs_->BeforeTakingDecision(decision);

  // Note that kUnsatTrailIndex might also mean ASSUMPTIONS_UNSAT.
  //
  // TODO(user): on some problems, this function can be quite long. Expand
  // so that we can check the time limit at each step?
  const int old_level = sat_solver_->CurrentDecisionLevel();
  const int index = sat_solver_->EnqueueDecisionAndBackjumpOnConflict(decision);
  if (index == kUnsatTrailIndex) return false;

  // Update the implied bounds each time we enqueue a literal at level zero.
  // This is "almost free", so we might as well do it.
  if (old_level == 0 && sat_solver_->CurrentDecisionLevel() == 1) {
    if (!implied_bounds_->ProcessIntegerTrail(decision)) return false;
    product_detector_->ProcessTrailAtLevelOne();
  }

  // Update the pseudo costs.
  pseudo_costs_->AfterTakingDecision(
      /*conflict=*/sat_solver_->CurrentDecisionLevel() <= old_level);

  sat_solver_->AdvanceDeterministicTime(time_limit_);
  return sat_solver_->ReapplyAssumptionsIfNeeded();
}

SatSolver::Status IntegerSearchHelper::SolveIntegerProblem() {
  if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;

  SearchHeuristics& heuristics = *model_->GetOrCreate<SearchHeuristics>();
  const int num_policies = heuristics.decision_policies.size();
  CHECK_NE(num_policies, 0);
  CHECK_EQ(num_policies, heuristics.restart_policies.size());

  // Note that it is important to do the level-zero propagation if it wasn't
  // already done because EnqueueDecisionAndBackjumpOnConflict() assumes that
  // the solver is in a "propagated" state.
  //
  // TODO(user): We have the issue that at level zero. calling the propagation
  // loop more than once can propagate more! This is because we call the LP
  // again and again on each level zero propagation. This is causing some
  // CHECKs() to fail in multithread (rarely) because when we associate new
  // literals to integer ones, Propagate() is indirectly called. Not sure yet
  // how to fix.
  if (!sat_solver_->FinishPropagation()) return sat_solver_->UnsatStatus();

  // Main search loop.
  const int64_t old_num_conflicts = sat_solver_->num_failures();
  const int64_t conflict_limit = parameters_.max_number_of_conflicts();
  int64_t num_decisions_since_last_lp_record_ = 0;
  while (!time_limit_->LimitReached() &&
         (sat_solver_->num_failures() - old_num_conflicts < conflict_limit)) {
    // If needed, restart and switch decision_policy.
    if (heuristics.restart_policies[heuristics.policy_index]()) {
      if (!sat_solver_->RestoreSolverToAssumptionLevel()) {
        return sat_solver_->UnsatStatus();
      }
      heuristics.policy_index = (heuristics.policy_index + 1) % num_policies;
    }

    if (!BeforeTakingDecision()) return sat_solver_->UnsatStatus();

    LiteralIndex decision = kNoLiteralIndex;
    while (true) {
      if (sat_solver_->ModelIsUnsat()) return sat_solver_->UnsatStatus();
      if (heuristics.next_decision_override != nullptr) {
        // Note that to properly count the num_times, we do not want to move
        // this function, but actually call that copy.
        if (!GetDecision(heuristics.next_decision_override, &decision)) {
          continue;
        }
        if (decision == kNoLiteralIndex) {
          heuristics.next_decision_override = nullptr;
        }
      }
      if (decision == kNoLiteralIndex) {
        if (!GetDecision(heuristics.decision_policies[heuristics.policy_index],
                         &decision)) {
          continue;
        }
      }
      break;
    }

    // No decision means that we reached a leave of the search tree and that
    // we have a feasible solution.
    //
    // Tricky: If the time limit is reached during the final propagation when
    // all variables are fixed, there is no guarantee that the propagation
    // responsible for testing the validity of the solution was run to
    // completion. So we cannot report a feasible solution.
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;
    if (decision == kNoLiteralIndex) {
      // Save the current polarity of all Booleans in the solution. It will be
      // followed for the next SAT decisions. This is known to be a good policy
      // for optimization problem. Note that for decision problem we don't care
      // since we are just done as soon as a solution is found.
      //
      // This idea is kind of "well known", see for instance the "LinSBPS"
      // submission to the maxSAT 2018 competition by Emir Demirovic and Peter
      // Stuckey where they show it is a good idea and provide more references.
      if (parameters_.use_optimization_hints()) {
        auto* sat_decision = model_->GetOrCreate<SatDecisionPolicy>();
        const auto& trail = *model_->GetOrCreate<Trail>();
        for (int i = 0; i < trail.Index(); ++i) {
          sat_decision->SetAssignmentPreference(trail[i], 0.0);
        }
      }
      return SatSolver::FEASIBLE;
    }

    if (!TakeDecision(Literal(decision))) {
      return sat_solver_->UnsatStatus();
    }

    // In multi-thread, we really only want to save the LP relaxation for thread
    // with high linearization level to avoid to pollute the repository with
    // sub-par lp solutions.
    //
    // TODO(user): Experiment more around dynamically changing the
    // threshold for storing LP solutions in the pool. Alternatively expose
    // this as parameter so this can be tuned later.
    //
    // TODO(user): Avoid adding the same solution many time if the LP didn't
    // change. Avoid adding solution that are too deep in the tree (most
    // variable fixed). Also use a callback rather than having this here, we
    // don't want this file to depend on cp_model.proto.
    if (model_->Get<SharedLPSolutionRepository>() != nullptr &&
        parameters_.linearization_level() >= 2) {
      num_decisions_since_last_lp_record_++;
      if (num_decisions_since_last_lp_record_ >= 100) {
        // NOTE: We can actually record LP solutions more frequently. However
        // this process is time consuming and workers waste a lot of time doing
        // this. To avoid this we don't record solutions after each decision.
        RecordLPRelaxationValues(model_);
        num_decisions_since_last_lp_record_ = 0;
      }
    }
  }
  return SatSolver::Status::LIMIT_REACHED;
}

SatSolver::Status ResetAndSolveIntegerProblem(
    const std::vector<Literal>& assumptions, Model* model) {
  // Backtrack to level zero.
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  if (!sat_solver->ResetToLevelZero()) return sat_solver->UnsatStatus();

  // Sync bounds and maybe do some inprocessing.
  // We reuse the BeforeTakingDecision() code
  auto* helper = model->GetOrCreate<IntegerSearchHelper>();
  if (!helper->BeforeTakingDecision()) return sat_solver->UnsatStatus();

  // Add the assumptions if any and solve.
  if (!sat_solver->ResetWithGivenAssumptions(assumptions)) {
    return sat_solver->UnsatStatus();
  }
  return helper->SolveIntegerProblem();
}

SatSolver::Status SolveIntegerProblemWithLazyEncoding(Model* model) {
  const IntegerVariable num_vars =
      model->GetOrCreate<IntegerTrail>()->NumIntegerVariables();
  std::vector<IntegerVariable> all_variables;
  for (IntegerVariable var(0); var < num_vars; ++var) {
    all_variables.push_back(var);
  }

  SearchHeuristics& heuristics = *model->GetOrCreate<SearchHeuristics>();
  heuristics.policy_index = 0;
  heuristics.decision_policies = {SequentialSearch(
      {SatSolverHeuristic(model),
       FirstUnassignedVarAtItsMinHeuristic(all_variables, model)})};
  heuristics.restart_policies = {SatSolverRestartPolicy(model)};
  return ResetAndSolveIntegerProblem(/*assumptions=*/{}, model);
}

#define RETURN_IF_NOT_FEASIBLE(test)       \
  const SatSolver::Status status = (test); \
  if (status != SatSolver::FEASIBLE) return status;

ContinuousProber::ContinuousProber(const CpModelProto& model_proto,
                                   Model* model)
    : model_(model),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      binary_implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      clause_manager_(model->GetOrCreate<ClauseManager>()),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      inprocessing_(model->GetOrCreate<Inprocessing>()),
      parameters_(*(model->GetOrCreate<SatParameters>())),
      level_zero_callbacks_(model->GetOrCreate<LevelZeroCallbackHelper>()),
      prober_(model->GetOrCreate<Prober>()),
      shared_response_manager_(model->Mutable<SharedResponseManager>()),
      shared_bounds_manager_(model->Mutable<SharedBoundsManager>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      active_limit_(parameters_.shaving_search_deterministic_time()) {
  auto* mapping = model_->GetOrCreate<CpModelMapping>();
  absl::flat_hash_set<BooleanVariable> visited;

  trail_index_at_start_of_iteration_ = trail_->Index();
  integer_trail_index_at_start_of_iteration_ = integer_trail_->Index();

  // Build variable lists.
  // TODO(user): Ideally, we should scan the internal model. But there can be
  // a large blowup of variables during loading, which slows down the probing
  // part. Using model variables is a good heuristic to select 'impactful'
  // Boolean variables.
  for (int v = 0; v < model_proto.variables_size(); ++v) {
    if (mapping->IsBoolean(v)) {
      const BooleanVariable bool_var = mapping->Literal(v).Variable();
      const auto [_, inserted] = visited.insert(bool_var);
      if (inserted) {
        bool_vars_.push_back(bool_var);
      }
    } else {
      IntegerVariable var = mapping->Integer(v);
      if (integer_trail_->IsFixed(var)) continue;
      int_vars_.push_back(var);
    }
  }

  VLOG(2) << "Start continuous probing with " << bool_vars_.size()
          << " Boolean variables,  " << int_vars_.size()
          << " integer variables, deterministic time limit = "
          << time_limit_->GetDeterministicLimit() << " on " << model_->Name();
}

// Continuous probing procedure.
// TODO(user):
//   - sort variables before the iteration (statically or dynamically)
//   - compress clause databases regularly (especially the implication graph)
//   - better interleaving of the probing and shaving phases
//   - move the shaving code directly in the probing class
//   - probe all variables and not just the model ones
SatSolver::Status ContinuousProber::Probe() {
  // Backtrack to level 0 in case we are not there.
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;

  while (!time_limit_->LimitReached()) {
    if (parameters_.use_sat_inprocessing() &&
        !inprocessing_->InprocessingRound()) {
      sat_solver_->NotifyThatModelIsUnsat();
      return sat_solver_->UnsatStatus();
    }

    // Store current statistics to detect an iteration without any improvement.
    const int64_t initial_num_literals_fixed =
        prober_->num_new_literals_fixed();
    const int64_t initial_num_bounds_shaved = num_bounds_shaved_;
    const auto& assignment = sat_solver_->Assignment();

    // Probe variable bounds.
    // TODO(user): Probe optional variables.
    for (; current_int_var_ < int_vars_.size(); ++current_int_var_) {
      const IntegerVariable int_var = int_vars_[current_int_var_];
      if (integer_trail_->IsFixed(int_var)) continue;

      const Literal shave_lb_literal =
          encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::LowerOrEqual(
              int_var, integer_trail_->LowerBound(int_var)));
      const BooleanVariable shave_lb_var = shave_lb_literal.Variable();
      const auto [_lb, lb_inserted] = probed_bool_vars_.insert(shave_lb_var);
      if (lb_inserted) {
        if (!prober_->ProbeOneVariable(shave_lb_var)) {
          return SatSolver::INFEASIBLE;
        }
        num_literals_probed_++;
      }

      const Literal shave_ub_literal =
          encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
              int_var, integer_trail_->UpperBound(int_var)));
      const BooleanVariable shave_ub_var = shave_ub_literal.Variable();
      const auto [_ub, ub_inserted] = probed_bool_vars_.insert(shave_ub_var);
      if (ub_inserted) {
        if (!prober_->ProbeOneVariable(shave_ub_var)) {
          return SatSolver::INFEASIBLE;
        }
        num_literals_probed_++;
      }

      if (use_shaving_) {
        const SatSolver::Status lb_status = ShaveLiteral(shave_lb_literal);
        if (ReportStatus(lb_status)) return lb_status;

        const SatSolver::Status ub_status = ShaveLiteral(shave_ub_literal);
        if (ReportStatus(ub_status)) return ub_status;
      }

      RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
    }

    // Probe Boolean variables from the model.
    for (; current_bool_var_ < bool_vars_.size(); ++current_bool_var_) {
      const BooleanVariable& bool_var = bool_vars_[current_bool_var_];

      if (assignment.VariableIsAssigned(bool_var)) continue;

      const auto [_, inserted] = probed_bool_vars_.insert(bool_var);
      if (!inserted) continue;

      if (!prober_->ProbeOneVariable(bool_var)) {
        return SatSolver::INFEASIBLE;
      }
      num_literals_probed_++;

      const Literal literal(bool_var, true);
      if (use_shaving_ && !assignment.LiteralIsAssigned(literal)) {
        const SatSolver::Status true_status = ShaveLiteral(literal);
        if (ReportStatus(true_status)) return true_status;
        if (true_status == SatSolver::ASSUMPTIONS_UNSAT) continue;

        const SatSolver::Status false_status = ShaveLiteral(literal.Negated());
        if (ReportStatus(false_status)) return false_status;
      }

      RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
    }

    if (parameters_.use_extended_probing()) {
      const auto at_least_one_literal_is_true =
          [&assignment](absl::Span<const Literal> literals) {
            for (const Literal literal : literals) {
              if (assignment.LiteralIsTrue(literal)) {
                return true;
              }
            }
            return false;
          };

      // Probe clauses of the SAT model.
      for (;;) {
        const SatClause* clause = clause_manager_->NextClauseToProbe();
        if (clause == nullptr) break;
        if (at_least_one_literal_is_true(clause->AsSpan())) continue;

        tmp_dnf_.clear();
        for (const Literal literal : clause->AsSpan()) {
          if (assignment.LiteralIsAssigned(literal)) continue;
          tmp_dnf_.push_back({literal});
        }
        ++num_at_least_one_probed_;
        if (!prober_->ProbeDnf("at_least_one", tmp_dnf_)) {
          return SatSolver::INFEASIBLE;
        }

        RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
      }

      // Probe at_most_ones of the SAT model.
      for (;;) {
        const absl::Span<const Literal> at_most_one =
            binary_implication_graph_->NextAtMostOne();
        if (at_most_one.empty()) break;
        if (at_least_one_literal_is_true(at_most_one)) continue;

        tmp_dnf_.clear();
        tmp_literals_.clear();
        for (const Literal literal : at_most_one) {
          if (assignment.LiteralIsAssigned(literal)) continue;
          tmp_dnf_.push_back({literal});
          tmp_literals_.push_back(literal.Negated());
        }
        tmp_dnf_.push_back(tmp_literals_);
        ++num_at_most_one_probed_;
        if (!prober_->ProbeDnf("at_most_one", tmp_dnf_)) {
          return SatSolver::INFEASIBLE;
        }

        RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
      }

      // Probe combinations of Booleans variables.
      const int limit = parameters_.probing_num_combinations_limit();
      const bool max_num_bool_vars_for_pairs_probing =
          static_cast<int>(std::sqrt(2 * limit));
      const int num_bool_vars = bool_vars_.size();

      if (num_bool_vars < max_num_bool_vars_for_pairs_probing) {
        for (; current_bv1_ + 1 < bool_vars_.size(); ++current_bv1_) {
          const BooleanVariable bv1 = bool_vars_[current_bv1_];
          if (assignment.VariableIsAssigned(bv1)) continue;
          current_bv2_ = std::max(current_bv1_ + 1, current_bv2_);
          for (; current_bv2_ < bool_vars_.size(); ++current_bv2_) {
            const BooleanVariable& bv2 = bool_vars_[current_bv2_];
            if (assignment.VariableIsAssigned(bv2)) continue;
            if (!prober_->ProbeDnf(
                    "pair_of_bool_vars",
                    {{Literal(bv1, true), Literal(bv2, true)},
                     {Literal(bv1, true), Literal(bv2, false)},
                     {Literal(bv1, false), Literal(bv2, true)},
                     {Literal(bv1, false), Literal(bv2, false)}})) {
              return SatSolver::INFEASIBLE;
            }
            RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
          }
          current_bv2_ = 0;
        }
      } else {
        for (; random_pair_of_bool_vars_probed_ < 10000;
             ++random_pair_of_bool_vars_probed_) {
          const BooleanVariable bv1 =
              bool_vars_[absl::Uniform<int>(*random_, 0, bool_vars_.size())];
          if (assignment.VariableIsAssigned(bv1)) continue;
          const BooleanVariable bv2 =
              bool_vars_[absl::Uniform<int>(*random_, 0, bool_vars_.size())];
          if (assignment.VariableIsAssigned(bv2) || bv1 == bv2) {
            continue;
          }
          if (!prober_->ProbeDnf(
                  "rnd_pair_of_bool_vars",
                  {{Literal(bv1, true), Literal(bv2, true)},
                   {Literal(bv1, true), Literal(bv2, false)},
                   {Literal(bv1, false), Literal(bv2, true)},
                   {Literal(bv1, false), Literal(bv2, false)}})) {
            return SatSolver::INFEASIBLE;
          }

          RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
        }
      }

      // Note that the product is always >= 0.
      const bool max_num_bool_vars_for_triplet_probing =
          static_cast<int>(std::cbrt(2 * limit));
      // We use a limit to make sure we do not overflow.
      const int loop_limit =
          num_bool_vars < max_num_bool_vars_for_triplet_probing
              ? num_bool_vars * (num_bool_vars - 1) * (num_bool_vars - 2) / 2
              : limit;
      for (; random_triplet_of_bool_vars_probed_ < loop_limit;
           ++random_triplet_of_bool_vars_probed_) {
        const BooleanVariable bv1 =
            bool_vars_[absl::Uniform<int>(*random_, 0, bool_vars_.size())];
        if (assignment.VariableIsAssigned(bv1)) continue;
        const BooleanVariable bv2 =
            bool_vars_[absl::Uniform<int>(*random_, 0, bool_vars_.size())];
        if (assignment.VariableIsAssigned(bv2) || bv1 == bv2) {
          continue;
        }
        const BooleanVariable bv3 =
            bool_vars_[absl::Uniform<int>(*random_, 0, bool_vars_.size())];
        if (assignment.VariableIsAssigned(bv3) || bv1 == bv3 || bv2 == bv3) {
          continue;
        }
        tmp_dnf_.clear();
        for (int i = 0; i < 8; ++i) {
          tmp_dnf_.push_back({Literal(bv1, (i & 1) > 0),
                              Literal(bv2, (i & 2) > 0),
                              Literal(bv3, (i & 4) > 0)});
        }

        if (!prober_->ProbeDnf("rnd_triplet_of_bool_vars", tmp_dnf_)) {
          return SatSolver::INFEASIBLE;
        }

        RETURN_IF_NOT_FEASIBLE(PeriodicSyncAndCheck());
      }
    }

    // Adjust the active_limit.
    if (use_shaving_) {
      const double deterministic_time =
          parameters_.shaving_search_deterministic_time();
      const bool something_has_been_detected =
          num_bounds_shaved_ != initial_num_bounds_shaved ||
          prober_->num_new_literals_fixed() != initial_num_literals_fixed;
      if (something_has_been_detected) {  // Reset the limit.
        active_limit_ = deterministic_time;
      } else if (active_limit_ < 25 * deterministic_time) {  // Bump the limit.
        active_limit_ += deterministic_time;
      }
    }

    // Reset all counters.
    ++iteration_;
    current_bool_var_ = 0;
    current_int_var_ = 0;
    current_bv1_ = 0;
    current_bv2_ = 1;
    random_pair_of_bool_vars_probed_ = 0;
    random_triplet_of_bool_vars_probed_ = 0;
    binary_implication_graph_->ResetAtMostOneIterator();
    clause_manager_->ResetToProbeIndex();
    probed_bool_vars_.clear();
    shaved_literals_.clear();

    const int new_trail_index = trail_->Index();
    const int new_integer_trail_index = integer_trail_->Index();

    // Update the use_shaving_ parameter.
    // TODO(user): Currently, the heuristics is that we alternate shaving and
    // not shaving, unless use_shaving_in_probing_search is false.
    use_shaving_ =
        parameters_.use_shaving_in_probing_search() ? !use_shaving_ : false;
    trail_index_at_start_of_iteration_ = new_trail_index;
    integer_trail_index_at_start_of_iteration_ = new_integer_trail_index;

    // Remove fixed Boolean variables.
    int new_size = 0;
    for (int i = 0; i < bool_vars_.size(); ++i) {
      if (!sat_solver_->Assignment().VariableIsAssigned(bool_vars_[i])) {
        bool_vars_[new_size++] = bool_vars_[i];
      }
    }
    bool_vars_.resize(new_size);

    // Remove fixed integer variables.
    new_size = 0;
    for (int i = 0; i < int_vars_.size(); ++i) {
      if (!integer_trail_->IsFixed(int_vars_[i])) {
        int_vars_[new_size++] = int_vars_[i];
      }
    }
    int_vars_.resize(new_size);
  }
  return SatSolver::LIMIT_REACHED;
}

#undef RETURN_IF_NOT_FEASIBLE

SatSolver::Status ContinuousProber::PeriodicSyncAndCheck() {
  // Check limit.
  if (--num_test_limit_remaining_ <= 0) {
    num_test_limit_remaining_ = kTestLimitPeriod;
    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;
  }

  // Log the state of the prober.
  if (--num_logs_remaining_ <= 0) {
    num_logs_remaining_ = kLogPeriod;
    LogStatistics();
  }

  // Sync with shared storage.
  if (--num_syncs_remaining_ <= 0) {
    num_syncs_remaining_ = kSyncPeriod;
    if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;
    for (const auto& cb : level_zero_callbacks_->callbacks) {
      if (!cb()) {
        sat_solver_->NotifyThatModelIsUnsat();
        return SatSolver::INFEASIBLE;
      }
    }
  }

  return SatSolver::FEASIBLE;
}

SatSolver::Status ContinuousProber::ShaveLiteral(Literal literal) {
  const auto [_, inserted] = shaved_literals_.insert(literal.Index());
  if (trail_->Assignment().LiteralIsAssigned(literal) || !inserted) {
    return SatSolver::LIMIT_REACHED;
  }
  num_bounds_tried_++;

  const double original_dtime_limit = time_limit_->GetDeterministicLimit();
  time_limit_->ChangeDeterministicLimit(
      std::min(original_dtime_limit,
               time_limit_->GetElapsedDeterministicTime() + active_limit_));
  const SatSolver::Status status =
      ResetAndSolveIntegerProblem({literal}, model_);
  time_limit_->ChangeDeterministicLimit(original_dtime_limit);
  if (ReportStatus(status)) return status;

  if (status == SatSolver::ASSUMPTIONS_UNSAT) {
    num_bounds_shaved_++;
  }

  // Important: we want to reset the solver right away, as we check for
  // fixed variable in the main loop!
  if (!sat_solver_->ResetToLevelZero()) return SatSolver::INFEASIBLE;
  return status;
}

bool ContinuousProber::ReportStatus(const SatSolver::Status status) {
  return status == SatSolver::INFEASIBLE || status == SatSolver::FEASIBLE;
}

void ContinuousProber::LogStatistics() {
  if (shared_response_manager_ == nullptr ||
      shared_bounds_manager_ == nullptr) {
    return;
  }
  if (VLOG_IS_ON(1)) {
    shared_response_manager_->LogMessageWithThrottling(
        "Probe",
        absl::StrCat(
            " (iterations=", iteration_,
            " linearization_level=", parameters_.linearization_level(),
            " shaving=", use_shaving_, " active_bool_vars=", bool_vars_.size(),
            " active_int_vars=", integer_trail_->NumIntegerVariables(),
            " literals fixed/probed=", prober_->num_new_literals_fixed(), "/",
            num_literals_probed_, " bounds shaved/tried=", num_bounds_shaved_,
            "/", num_bounds_tried_, " new_integer_bounds=",
            shared_bounds_manager_->NumBoundsExported("probing"),
            " new_binary_clause=", prober_->num_new_binary_clauses(),
            " num_at_least_one_probed=", num_at_least_one_probed_,
            " num_at_most_one_probed=", num_at_most_one_probed_, ")"));
  }
}

}  // namespace sat
}  // namespace operations_research
