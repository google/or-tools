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

#include "ortools/sat/integer_search.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/linear_propagation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/pseudo_costs.h"
#include "ortools/sat/restart.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/sat/symmetry_util.h"
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

        // This direction works better than the inverse in the benchmarks. But
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

std::function<BooleanOrIntegerLiteral()> ShaveModelIntegerVariableBounds(
    const std::vector<IntegerVariable>& vars, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  absl::BitGenRef random = *model->GetOrCreate<ModelRandomGenerator>();

  int current_index = 0;
  bool enable = true;
  return [/*copy */ vars =
              std::vector<IntegerVariable>(vars.begin(), vars.end()),
          /*copy*/ current_index, /*copy*/ enable, integer_trail, sat_solver,
          random = random]() mutable {
    const int level = sat_solver->CurrentDecisionLevel();
    if (level > 0 || !enable) return BooleanOrIntegerLiteral();

    for (int i = 0; i < vars.size(); ++i) {
      const IntegerVariable var = vars[current_index++];
      if (current_index >= vars.size()) current_index = 0;

      if (integer_trail->IsFixed(var)) continue;

      const IntegerValue var_lb = integer_trail->LowerBound(var);
      const IntegerValue var_ub = integer_trail->UpperBound(var);
      DCHECK_LT(var_lb, var_ub);

      const IntegerValue half_domain_range = (var_ub - var_lb) / 2;
      const IntegerValue new_ub =
          var_lb +
          absl::LogUniform<int64_t>(random, 0, half_domain_range.value());

      return BooleanOrIntegerLiteral(IntegerLiteral::LowerOrEqual(var, new_ub));
    }

    // We will disable this if all integer variables are fixed.
    enable = false;
    return BooleanOrIntegerLiteral();
  };
}

std::function<BooleanOrIntegerLiteral()> ShaveModelBooleanVariables(
    const std::vector<Literal>& literals, Model* model) {
  auto* sat_solver = model->GetOrCreate<SatSolver>();
  auto* binary_implication_graph = model->GetOrCreate<BinaryImplicationGraph>();
  int current_index = 0;
  bool enable = true;
  return [/*copy */ literals =
              std::vector<Literal>(literals.begin(), literals.end()),
          /*copy*/ current_index, /*copy*/ enable, sat_solver,
          binary_implication_graph]() mutable {
    const int level = sat_solver->CurrentDecisionLevel();
    if (level > 0 || !enable) return BooleanOrIntegerLiteral();

    for (int i = 0; i < literals.size(); ++i) {
      const Literal lit = literals[current_index++];
      if (current_index >= literals.size()) current_index = 0;

      if (sat_solver->Assignment().LiteralIsAssigned(lit) ||
          binary_implication_graph->IsRedundant(
              Literal(lit.Variable(), true))) {
        continue;
      }

      return BooleanOrIntegerLiteral(lit.Index());
    }

    // We will disable this if all literals are fixed.
    enable = false;
    return BooleanOrIntegerLiteral();
  };
}

std::function<BooleanOrIntegerLiteral()> SequentialSearch(
    std::vector<std::function<BooleanOrIntegerLiteral()>> heuristics) {
  if (DEBUG_MODE) {
    for (const auto& h : heuristics) {
      DCHECK(h != nullptr);
    }
  }
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
  return [encoder, sat_policy,
          value_selection_heuristics = std::move(value_selection_heuristics),
          var_selection_heuristic = std::move(var_selection_heuristic)]() {
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
                var, response_manager->SolutionPool().BestSolutions(), model);
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
                                  std::move(var_selection_heuristic), model);
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
  absl::BitGenRef random = *model->GetOrCreate<ModelRandomGenerator>();

  return [obj_var, integer_trail, sat_solver, rand = random]() {
    BooleanOrIntegerLiteral result;
    const int level = sat_solver->CurrentDecisionLevel();
    if (level > 0 || obj_var == kNoIntegerVariable) return result;

    const IntegerValue obj_lb = integer_trail->LowerBound(obj_var);
    const IntegerValue obj_ub = integer_trail->UpperBound(obj_var);
    if (obj_lb == obj_ub) return result;
    const IntegerValue mid = (obj_ub - obj_lb) / 2;
    absl::BitGenRef r = rand;
    const IntegerValue new_ub =
        obj_lb + absl::LogUniform<int64_t>(r, 0, mid.value());

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

namespace {
// Detects interval precedence constraints.
class IntervalPrecedencesDetector {
 public:
  explicit IntervalPrecedencesDetector(Model* model)
      : intervals_(*model->GetOrCreate<IntervalsRepository>()),
        lin2_indices_(*model->GetOrCreate<Linear2Indices>()),
        lin2_bounds_(*model->GetOrCreate<RootLevelLinear2Bounds>()),
        conditional_bounds_(*model->GetOrCreate<ConditionalLinear2Bounds>()) {
    CompactVectorVectorBuilder<IntegerVariable, IntervalVariable>
        intervals_by_start_var_builder;
    CompactVectorVectorBuilder<IntegerVariable, IntervalVariable>
        intervals_by_end_var_builder;
    for (IntervalVariable interval_var(0);
         interval_var < intervals_.NumIntervals(); ++interval_var) {
      const AffineExpression start = intervals_.Start(interval_var);
      if (start.var != kNoIntegerVariable) {
        CHECK_GT(start.coeff, 0);
        intervals_by_start_var_builder.Add(start.var, interval_var);
      }
      const AffineExpression end = intervals_.End(interval_var);
      if (end.var != kNoIntegerVariable) {
        CHECK_GT(end.coeff, 0);
        intervals_by_end_var_builder.Add(end.var, interval_var);
      }
    }
    intervals_by_start_var_.ResetFromBuilder(intervals_by_start_var_builder,
                                             intervals_.NumIntervals());
    intervals_by_end_var_.ResetFromBuilder(intervals_by_end_var_builder,
                                           intervals_.NumIntervals());
  }

  // Detects unconditional interval precedence constraints. Returns, for each
  // interval I, the intervals J which should be fixed after it in the
  // SchedulingSearchHeuristicHelper (this can include some precedences which
  // are not in the original problem, such as artificial precedences between
  // optional and non-optional intervals).
  CompactVectorVector<IntervalVariable, IntervalVariable> DetectPrecedences() {
    CompactVectorVectorBuilder<IntervalVariable, IntervalVariable>
        interval_precedences_builder;
    for (LinearExpression2Index i(0);
         i < 2 * lin2_indices_.NumStoredPositiveLinear2(); ++i) {
      LinearExpression2 expr = lin2_indices_.GetExpression(i);
      if (expr.vars[0] == kNoIntegerVariable ||
          expr.vars[1] == kNoIntegerVariable) {
        continue;
      }
      DCHECK(expr.IsCanonicalized());
      DCHECK_GT(expr.coeffs[0], 0);
      DCHECK_GT(expr.coeffs[1], 0);
      const IntegerValue lb = -lin2_bounds_.LevelZeroUpperBound(NegationOf(i));
      for (int i = 0; i < 2; ++i) {
        // Let's look for next_interval.start >= interval.end.
        const IntegerVariable var = expr.vars[0];
        const IntegerVariable next_var = expr.vars[1];
        if (NegationOf(var) >= intervals_by_end_var_.size() ||
            next_var >= intervals_by_start_var_.size()) {
          continue;
        }
        for (const IntervalVariable interval_var :
             intervals_by_end_var_[NegationOf(var)]) {
          const AffineExpression interval_end = intervals_.End(interval_var);
          for (const IntervalVariable next_interval_var :
               intervals_by_start_var_[next_var]) {
            if (next_interval_var == interval_var) continue;
            const AffineExpression next_interval_start =
                intervals_.Start(next_interval_var);
            const std::optional<IntegerValue> delta_t =
                expr.GetDifferenceLowerBound(lb, next_interval_start,
                                             interval_end);
            if (delta_t.has_value() && delta_t.value() >= 0) {
              interval_precedences_builder.Add(interval_var, next_interval_var);
            }
          }
        }
        std::swap(expr.vars[0], expr.vars[1]);
        std::swap(expr.coeffs[0], expr.coeffs[1]);
      }
      for (int i = 0; i < 2; ++i) {
        // Let's look for next_interval.start > interval.start (the above code
        // can fail to detect some precedences, depending on how the scheduling
        // problem is modeled and/or presolved).
        const IntegerVariable var = expr.vars[0];
        const IntegerVariable next_var = expr.vars[1];
        if (next_var >= intervals_by_start_var_.size() ||
            NegationOf(var) >= intervals_by_start_var_.size()) {
          continue;
        }
        for (const IntervalVariable interval_var :
             intervals_by_start_var_[NegationOf(var)]) {
          const AffineExpression interval_start =
              intervals_.Start(interval_var);
          for (const IntervalVariable next_interval_var :
               intervals_by_start_var_[next_var]) {
            if (next_interval_var == interval_var) continue;
            const AffineExpression next_interval_start =
                intervals_.Start(next_interval_var);
            const std::optional<IntegerValue> delta_t =
                expr.GetDifferenceLowerBound(lb, next_interval_start,
                                             interval_start);
            if (delta_t.has_value() && delta_t > 0) {
              interval_precedences_builder.Add(interval_var, next_interval_var);
            }
          }
        }
        std::swap(expr.vars[0], expr.vars[1]);
        std::swap(expr.coeffs[0], expr.coeffs[1]);
      }
    }

    // Add artificial precedences to fix optional intervals before non-optional
    // ones which share the same start variable. A mandatory interval can have
    // several optional variants, with exactly one being present. In this case
    // the above code might detect precedences only between the optional
    // intervals. To avoid conflicts in the heuristics below, it is better to
    // fix the optional intervals first.
    for (const absl::Span<const IntervalVariable> intervals :
         intervals_by_start_var_) {
      for (const IntervalVariable interval_var : intervals) {
        for (const IntervalVariable other_interval_var : intervals) {
          if (interval_var == other_interval_var) continue;
          if (!intervals_.IsOptional(interval_var) &&
              intervals_.IsOptional(other_interval_var)) {
            interval_precedences_builder.Add(other_interval_var, interval_var);
          }
        }
      }
    }

    CompactVectorVector<IntervalVariable, IntervalVariable>
        interval_precedences;
    interval_precedences.ResetFromBuilder(interval_precedences_builder,
                                          intervals_.NumIntervals());
    for (int i = 0; i < interval_precedences.size(); ++i) {
      interval_precedences.SortAndRemoveDuplicateValues(IntervalVariable(i));
    }
    return interval_precedences;
  }

  // Detects conditional precedences between intervals. Returns (J, Δt) pairs
  // for each interval I such that if J.start >= I.end + Δt, then all the
  // conditional precedences we know about should be satisfied.
  CompactVectorVector<IntervalVariable,
                      std::pair<IntervalVariable, IntegerValue>>
  DetectConditionalPrecedences() {
    // For each pair of intervals (I, J), the maximum transition time from I to
    // J in the conditional bounds of the form lit => J is after I.
    absl::flat_hash_map<std::pair<IntervalVariable, IntervalVariable>,
                        IntegerValue>
        max_transition_times;
    for (int i = 0; i < conditional_bounds_.size(); ++i) {
      Relation relation = conditional_bounds_.relation(i);
      if (relation.expr.vars[0] == kNoIntegerVariable ||
          relation.expr.vars[1] == kNoIntegerVariable) {
        continue;
      }
      // There are four ways to interpret 'relation' as a precedence constraint
      // between intervals: terms can be swapped and variables can be negated.
      for (int j = 0; j < 4; ++j) {
        if (NegationOf(relation.expr.vars[0]) < intervals_by_end_var_.size() &&
            relation.expr.vars[1] < intervals_by_start_var_.size()) {
          for (IntervalVariable interval :
               intervals_by_end_var_[NegationOf(relation.expr.vars[0])]) {
            for (IntervalVariable next_interval :
                 intervals_by_start_var_[relation.expr.vars[1]]) {
              if (next_interval == interval) continue;
              const std::optional<IntegerValue> transition_time =
                  relation.expr.GetDifferenceLowerBound(
                      relation.lhs, intervals_.Start(next_interval),
                      intervals_.End(interval));
              if (transition_time.has_value() && transition_time > 0) {
                auto& current_max =
                    max_transition_times[{interval, next_interval}];
                current_max = std::max(current_max, *transition_time);
              }
            }
          }
        }
        if (j == 0 || j == 2) {
          std::swap(relation.expr.vars[0], relation.expr.vars[1]);
          std::swap(relation.expr.coeffs[0], relation.expr.coeffs[1]);
        } else if (j == 1) {
          relation.expr.vars[0] = NegationOf(relation.expr.vars[0]);
          relation.expr.vars[1] = NegationOf(relation.expr.vars[1]);
          relation.lhs = -relation.lhs;
          relation.rhs = -relation.rhs;
          std::swap(relation.lhs, relation.rhs);
        }
      }
    }
    CompactVectorVectorBuilder<IntervalVariable,
                               std::pair<IntervalVariable, IntegerValue>>
        conditional_transitions_builder;
    for (const auto& [transition_time, value] : max_transition_times) {
      conditional_transitions_builder.Add(transition_time.first,
                                          {transition_time.second, value});
    }
    CompactVectorVector<IntervalVariable,
                        std::pair<IntervalVariable, IntegerValue>>
        conditional_transitions;
    conditional_transitions.ResetFromBuilder(conditional_transitions_builder,
                                             intervals_.NumIntervals());
    return conditional_transitions;
  }

 private:
  const IntervalsRepository& intervals_;
  const Linear2Indices& lin2_indices_;
  const RootLevelLinear2Bounds& lin2_bounds_;
  const ConditionalLinear2Bounds& conditional_bounds_;

  CompactVectorVector<IntegerVariable, IntervalVariable>
      intervals_by_start_var_;
  CompactVectorVector<IntegerVariable, IntervalVariable> intervals_by_end_var_;
};

}  // namespace

class SchedulingSearchHeuristicHelper {
 public:
  explicit SchedulingSearchHeuristicHelper(Model* model)
      : fixed_search_(model->GetOrCreate<SatParameters>()->search_branching() ==
                      SatParameters::FIXED_SEARCH),
        assignment_(model->GetOrCreate<Trail>()->Assignment()),
        repo_(model->GetOrCreate<IntervalsRepository>()),
        heuristic_(model->GetOrCreate<SearchHeuristics>()),
        watcher_(model->GetOrCreate<GenericLiteralWatcher>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        random_(model->GetOrCreate<ModelRandomGenerator>()),
        rev_int_repo_(model->GetOrCreate<RevIntRepository>()) {
    if (fixed_search_) {
      IntervalPrecedencesDetector precedences_detector(model);
      successors_ = precedences_detector.DetectPrecedences();
      successors_transition_times_ =
          precedences_detector.DetectConditionalPrecedences();
    }

    // Fix the max size of random choices.
    randomization_size_ =
        std::max<int>(1, model->GetOrCreate<SatParameters>()
                             ->search_random_variable_pool_size());
  }

  BooleanOrIntegerLiteral NextDecision() {
    std::vector<ToSchedule> top_decisions;
    top_decisions.reserve(randomization_size_);
    top_decisions.resize(1);

    // Find out if there was any backtrack since the last call,
    // or since the beginning of the search.
    const bool backtrack_since_last_call = !rev_is_in_dive_;
    if (backtrack_since_last_call) {
      RecomputePredecessorCounts();
    }

    if (!first_call_ && backtrack_since_last_call) {
      no_backtrack_since_start_ = false;
      cached_start_mins_.assign(repo_->NumIntervals(), kMinIntegerValue);
    }
    first_call_ = false;
    const bool use_first_solution_heuristic =
        fixed_search_ && no_backtrack_since_start_;

    for (int index = 0; index < intervals_with_only_fixed_predecessors_.size();
         ++index) {
      const IntervalVariable interval =
          intervals_with_only_fixed_predecessors_[index];

      const ToSchedule& worst = top_decisions.back();
      if (cached_start_mins_[interval] > worst.start_min) {
        continue;
      }

      if (repo_->IsAbsent(interval)) {
        ProcessAbsentOrFixedInterval(index);
        continue;
      }

      const AffineExpression start = repo_->Start(interval);
      const AffineExpression end = repo_->End(interval);
      if (repo_->IsPresent(interval) && integer_trail_->IsFixed(start) &&
          integer_trail_->IsFixed(end)) {
        ProcessAbsentOrFixedInterval(index);
        continue;
      }

      ToSchedule candidate;
      if (repo_->IsOptional(interval)) {
        // For task whose presence is still unknown, our propagators should
        // have propagated the minimum time as if it was present. So this
        // should reflect the earliest time at which this interval can be
        // scheduled.
        const Literal lit = repo_->PresenceLiteral(interval);
        candidate.start_min = integer_trail_->ConditionalLowerBound(lit, start);
        candidate.start_max = integer_trail_->ConditionalUpperBound(lit, start);
      } else {
        candidate.start_min = integer_trail_->LowerBound(start);
        candidate.start_max = integer_trail_->UpperBound(start);
      }
      if (use_first_solution_heuristic &&
          cached_start_mins_[interval] > candidate.start_min) {
        IntegerValue delta_t =
            cached_start_mins_[interval] - candidate.start_min;
        candidate.start_min += delta_t;
        candidate.start_max += delta_t;
      } else {
        cached_start_mins_[interval] = candidate.start_min;
      }
      if (top_decisions.size() < randomization_size_ ||
          candidate.MightBeBetter(worst)) {
        // Finish filling candidate.
        //
        // For variable size, we compute the min size once the start is fixed
        // to time. This is needed to never pick the "artificial" makespan
        // interval at the end in priority compared to intervals that still
        // need to be scheduled.
        candidate.interval = interval;
        candidate.start = start;
        candidate.end = end;
        candidate.presence = repo_->IsOptional(interval)
                                 ? repo_->PresenceLiteral(interval).Index()
                                 : kNoLiteralIndex;
        candidate.size_min =
            std::max(integer_trail_->LowerBound(repo_->Size(interval)),
                     integer_trail_->LowerBound(end) - candidate.start_min);
        candidate.noise = absl::Uniform(*random_, 0.0, 1.0);

        if (top_decisions.size() == randomization_size_) {
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
    watcher_->SetUntilNextBacktrack(&rev_is_in_dive_);

    const ToSchedule best =
        top_decisions.size() == 1
            ? top_decisions.front()
            : top_decisions[absl::Uniform(
                  *random_, 0, static_cast<int>(top_decisions.size()))];
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
    heuristic_->next_decision_override = [this, use_first_solution_heuristic,
                                          best, num_times]() mutable {
      if (++num_times > 5) {
        // We have been trying to fix this interval for a while. Do we miss
        // some propagation? In any case, try to see if the heuristic above
        // would select something else.
        VLOG(3) << "Skipping ... ";
        return BooleanOrIntegerLiteral();
      }

      // First make sure the interval is present.
      if (best.presence != kNoLiteralIndex) {
        if (!assignment_.LiteralIsAssigned(Literal(best.presence))) {
          VLOG(3) << "assign " << best.presence;
          return BooleanOrIntegerLiteral(best.presence);
        }
        if (assignment_.LiteralIsFalse(Literal(best.presence))) {
          VLOG(2) << "unperformed.";
          return BooleanOrIntegerLiteral();
        }
      }

      // We assume that start_min is propagated by now.
      if (!integer_trail_->IsFixed(best.start)) {
        const IntegerValue start_min = integer_trail_->LowerBound(best.start);
        const IntegerValue cached_start_min = cached_start_mins_[best.interval];
        VLOG(3) << "start == " << start_min
                << " cached_start_min == " << cached_start_min;
        if (use_first_solution_heuristic && cached_start_min > start_min) {
          if (cached_start_min <= integer_trail_->UpperBound(best.start)) {
            return BooleanOrIntegerLiteral(
                best.start.GreaterOrEqual(cached_start_min));
          } else {
            // Our heuristic gave us a decision that is currently false! Lets
            // fall back to other heuristic until we are called again.
            return BooleanOrIntegerLiteral();
          }
        }
        return BooleanOrIntegerLiteral(best.start.LowerOrEqual(start_min));
      }

      // We assume that end_min is propagated by now.
      if (!integer_trail_->IsFixed(best.end)) {
        const IntegerValue end_min = integer_trail_->LowerBound(best.end);
        VLOG(3) << "end == " << end_min;
        return BooleanOrIntegerLiteral(best.end.LowerOrEqual(end_min));
      }

      // Everything is fixed, detach the override.
      const IntegerValue start = integer_trail_->LowerBound(best.start);
      const IntegerValue end = integer_trail_->LowerBound(best.end);
      if (use_first_solution_heuristic) {
        for (const auto& [successor, transition_time] :
             successors_transition_times_[best.interval]) {
          cached_start_mins_[successor] =
              std::max(cached_start_mins_[successor], end + transition_time);
        }
      }
      VLOG(2) << "Fixed " << best.interval << " @[" << start << ","
              << integer_trail_->LowerBound(best.end) << "]"
              << (best.presence != kNoLiteralIndex
                      ? absl::StrCat(" presence=",
                                     Literal(best.presence).DebugString())
                      : "")
              << (best.start_min < start ? absl::StrCat(" start_at_selection=",
                                                        best.start_min.value())
                                         : "");
      return BooleanOrIntegerLiteral();
    };

    return heuristic_->next_decision_override();
  }

 private:
  struct ToSchedule {
    IntervalVariable interval;
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

  void RecomputePredecessorCounts() {
    const int num_intervals = repo_->NumIntervals();
    num_non_fixed_predecessors_.assign(num_intervals, 0);
    if (!successors_.empty()) {
      for (IntervalVariable i(0); i < num_intervals; ++i) {
        if (IntervalIsAbsentOrFixed(i)) continue;
        for (const IntervalVariable j : successors_[i]) {
          num_non_fixed_predecessors_[j]++;
        }
      }
    }
    intervals_with_only_fixed_predecessors_.clear();
    for (IntervalVariable i(0); i < num_intervals; ++i) {
      if (IntervalIsAbsentOrFixed(i)) continue;
      if (num_non_fixed_predecessors_[i] == 0) {
        intervals_with_only_fixed_predecessors_.push_back(i);
      }
    }
    cached_start_mins_.assign(num_intervals, kMinIntegerValue);
  }

  // Removes the interval at the given index in
  // intervals_with_only_fixed_predecessors_ and adds the intervals whose
  // predecessors are now all fixed or absent. Decrements interval_index to
  // account for the removed interval.
  void ProcessAbsentOrFixedInterval(int& interval_index) {
    const IntervalVariable i =
        intervals_with_only_fixed_predecessors_[interval_index];
    std::swap(intervals_with_only_fixed_predecessors_[interval_index],
              intervals_with_only_fixed_predecessors_.back());
    intervals_with_only_fixed_predecessors_.pop_back();
    interval_index--;
    if (successors_.empty()) return;
    for (const IntervalVariable j : successors_[i]) {
      DCHECK_GT(num_non_fixed_predecessors_[j], 0);
      num_non_fixed_predecessors_[j]--;
      if (num_non_fixed_predecessors_[j] == 0) {
        if (IntervalIsAbsentOrFixed(j)) continue;
        DCHECK(
            !absl::c_linear_search(intervals_with_only_fixed_predecessors_, j));
        intervals_with_only_fixed_predecessors_.push_back(j);
      }
    }
  }

  bool IntervalIsAbsentOrFixed(IntervalVariable i) const {
    return repo_->IsAbsent(i) ||
           (repo_->IsPresent(i) && integer_trail_->IsFixed(repo_->Start(i)) &&
            integer_trail_->IsFixed(repo_->End(i)));
  }

  const bool fixed_search_;
  const VariablesAssignment& assignment_;
  IntervalsRepository* repo_;
  SearchHeuristics* heuristic_;
  GenericLiteralWatcher* watcher_;
  IntegerTrail* integer_trail_;
  ModelRandomGenerator* random_;
  RevIntRepository* rev_int_repo_;

  bool rev_is_in_dive_ = false;
  bool first_call_ = true;
  bool no_backtrack_since_start_ = true;
  int randomization_size_ = 1;

  // For each interval I, all the intervals J which can only be fixed once I is
  // fixed, or absent (this can include precedences which are not in the
  // original problem, such as artificial precedences between optional and
  // non-optional intervals). Empty if fixed_search_ is false.
  CompactVectorVector<IntervalVariable, IntervalVariable> successors_;
  // For each interval I, of list of (J, Δt) pairs
  // such that if J.start >= I.end + Δt, then all the conditional precedences we
  // know about should be satisfied. Empty if fixed_search_ is false.
  CompactVectorVector<IntervalVariable,
                      std::pair<IntervalVariable, IntegerValue>>
      successors_transition_times_;

  // For each interval I, the number of predecessors of I which are not absent
  // and not yet fixed.
  util_intops::StrongVector<IntervalVariable, int> num_non_fixed_predecessors_;
  // The intervals whose predecessors are all fixed or absent, and which are not
  // themselves fixed or absent.
  std::vector<IntervalVariable> intervals_with_only_fixed_predecessors_;

  util_intops::StrongVector<IntervalVariable, IntegerValue> cached_start_mins_;
};

// A simple heuristic for scheduling models.
std::function<BooleanOrIntegerLiteral()> SchedulingSearchHeuristic(
    Model* model) {
  auto* helper = model->GetOrCreate<SchedulingSearchHeuristicHelper>();
  return [helper]() { return helper->NextDecision(); };
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

        // TODO(user): Also compare the second part of the precedence in
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
      // If one of the task presence is undecided, start by making it present.
      for (const int t : {best_before, best_after}) {
        if (!best_helper->IsPresent(t)) {
          VLOG(2) << "Presence: " << best_helper->TaskDebugString(t);
          return BooleanOrIntegerLiteral(best_helper->PresenceLiteral(t));
        }
      }

      VLOG(2) << "New disjunctive precedence: "
              << best_helper->TaskDebugString(best_before) << " "
              << best_helper->TaskDebugString(best_after);
      const auto a = best_helper->GetIntervalDefinition(best_before);
      const auto b = best_helper->GetIntervalDefinition(best_after);
      return BooleanOrIntegerLiteral(
          repo->GetOrCreateDisjunctivePrecedenceLiteralIfNonTrivial(a, b));
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
      bool found = false;
      while (!found && next_end < num_tasks) {
        IntegerValue time = by_emin[next_end].time;
        if (next_start < num_tasks) {
          time = std::min(time, by_smin[next_start].time);
        }

        // Remove added task ending there.
        // Set their demand to zero.
        for (; next_end < num_tasks && by_emin[next_end].time == time;
             ++next_end) {
          const int t = by_emin[next_end].task_index;
          if (!helper->IsPresent(t)) continue;
          if (added_demand[t] > 0) {
            current_height -= added_demand[t];
            added_demand[t] = 0;
          } else {
            // Corner case if task is of duration zero.
            added_demand[t] = -1;
          }
        }

        // Add new task starting here.
        // If the task cannot be added we have a candidate for precedence.
        // TODO(user): tie-break tasks not fitting in the profile smartly.
        for (; next_start < num_tasks && by_smin[next_start].time == time;
             ++next_start) {
          const int t = by_smin[next_start].task_index;
          if (!helper->IsPresent(t)) continue;
          if (added_demand[t] == -1) continue;  // Corner case.
          const IntegerValue demand_min = h.demand_helper->DemandMin(t);
          if (current_height + demand_min <= capacity_max) {
            added_demand[t] = demand_min;
            current_height += demand_min;
          } else if (first_skipped_task == -1) {
            // We should have everything needed here to add a new precedence.
            first_skipped_task = t;
            found = true;
            break;
          }
        }
      }

      // If packing everything to the left is feasible, continue.
      if (first_skipped_task == -1) {
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
      // repo.CreateDisjunctivePrecedenceLiteralIfNonTrivial() instead.
      //
      // TODO(user): Add heuristic ordering for creating interesting precedence
      // first.
      bool found_precedence_to_add = false;
      helper->ResetReason();
      for (const int s : open_tasks) {
        for (const int t : open_tasks) {
          if (s == t) continue;

          // Make sure s could be before t.
          if (helper->TaskIsBeforeOrIsOverlapping(t, s)) {
            helper->AddReasonForBeingBeforeAssumingNoOverlap(t, s);
            continue;
          }

          // Can we add s <= t ?
          // All the considered tasks are intersecting if on the left.
          CHECK_LT(helper->StartMin(s), helper->EndMin(t));
          CHECK_LT(helper->StartMin(t), helper->EndMin(s));

          // skip if we already have a literal created and assigned to false.
          const LiteralIndex existing = repo->GetPrecedenceLiteral(
              helper->Ends()[s], helper->Starts()[t]);
          if (existing != kNoLiteralIndex) {
            if (trail->Assignment().LiteralIsTrue(Literal(existing))) {
              // In normal circumstances, this shouldn't be able to be true here
              // otherwise we will have s and t disjoint. That said, this can
              // happen if we are not in the propagation fixed point.
              return BooleanOrIntegerLiteral();
            }

            // This should always be true in normal usage after SAT search has
            // fixed all literal, but if it is not, we can just return this
            // decision.
            if (trail->Assignment().LiteralIsFalse(Literal(existing))) {
              helper->AddLiteralReason(Literal(existing));
              continue;
            }
          } else {
            // It shouldn't be able to fail since s can be before t.
            CHECK(repo->CreatePrecedenceLiteralIfNonTrivial(
                helper->Ends()[s], helper->Starts()[t]));
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
      if (!h.capacity.IsConstant()) {
        helper->AddIntegerReason(
            integer_trail->UpperBoundAsLiteral(h.capacity));
      }
      const auto& demands = h.demand_helper->Demands();
      for (const int t : open_tasks) {
        if (helper->IsOptional(t)) {
          CHECK(trail->Assignment().LiteralIsTrue(helper->PresenceLiteral(t)));
          helper->AddLiteralReason(helper->PresenceLiteral(t).Negated());
        }
        const AffineExpression d = demands[t];
        if (!d.IsConstant()) {
          helper->AddIntegerReason(integer_trail->LowerBoundAsLiteral(d));
        }
      }
      (void)helper->ReportConflict();
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
      const AffineExpression end_a = best_helper->Ends()[best_before];
      const AffineExpression start_b = best_helper->Starts()[best_after];
      repo->CreatePrecedenceLiteralIfNonTrivial(end_a, start_b);
      return BooleanOrIntegerLiteral(
          repo->GetPrecedenceLiteral(end_a, start_b));
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

  // Add model based heuristic search if present.
  if (heuristics.heuristic_search != nullptr) {
    policies.push_back(
        SequentialSearch({heuristics.heuristic_search, sat_policy,
                          heuristics.integer_completion_search}));
    weights.push_back(1);
  } else if (heuristics.user_search == nullptr) {
    // Add pseudo cost search if nothing else is present.
    policies.push_back(SequentialSearch(
        {PseudoCost(model), sat_policy, heuristics.integer_completion_search}));
    weights.push_back(1);
  }

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
              var, response_manager->SolutionPool().BestSolutions(), model);
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

std::function<bool()> RestartAfterDeterministicTime(double deterministic_time,
                                                    TimeLimit* time_limit) {
  bool reset_at_next_call = true;
  double deterministic_deadline = 0.0;
  return [=]() mutable {
    if (reset_at_next_call) {
      deterministic_deadline =
          time_limit->GetElapsedDeterministicTime() + deterministic_time;
      reset_at_next_call = false;
    } else if (time_limit->GetElapsedDeterministicTime() >=
               deterministic_deadline) {
      reset_at_next_call = true;
    }
    return reset_at_next_call;
  };
}

std::function<bool()> RestartAfterFixedDeterministicTimeOrKFailures(
    double deterministic_time, TimeLimit* time_limit, int k, SatSolver* solver,
    ModelRandomGenerator* random) {
  bool reset_at_next_call = true;
  double deterministic_deadline = 0.0;
  int next_num_failures = 0;
  bool restart_on_failures = false;
  return [=]() mutable {
    if (reset_at_next_call) {
      restart_on_failures = absl::Bernoulli(*random, 0.5);
      if (restart_on_failures) {
        next_num_failures = solver->num_failures() + k;
      } else {
        deterministic_deadline =
            time_limit->GetElapsedDeterministicTime() + deterministic_time;
      }
      reset_at_next_call = false;
    } else {
      if (restart_on_failures && solver->num_failures() >= next_num_failures) {
        reset_at_next_call = true;
      } else if (!restart_on_failures &&
                 time_limit->GetElapsedDeterministicTime() >=
                     deterministic_deadline) {
        reset_at_next_call = true;
      }
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
    case SatParameters::ROUND_ROBIN_SHAVING_SEARCH: {
      // Model objects.
      auto* mapping = model->GetOrCreate<CpModelMapping>();
      auto* integer_trail = model->GetOrCreate<IntegerTrail>();
      const auto& assignment = model->GetOrCreate<Trail>()->Assignment();

      // Temporary storage.
      CompactVectorVector<int, int> orbits;
      std::vector<int> var_to_orbit_index;
      std::vector<int> var_to_representative;
      GetOrbitsAndRepresentatives(*mapping->ModelProto(), orbits,
                                  var_to_orbit_index, var_to_representative);

      // Collect variables and literals to shave.
      const int num_model_vars = mapping->ModelProto()->variables_size();
      const int num_bool_vars = mapping->NumBooleanVariables();
      std::vector<Literal> literals;
      std::vector<IntegerVariable> integer_vars;
      literals.reserve(2 * num_bool_vars);
      integer_vars.reserve(2 * (num_model_vars - num_bool_vars));
      for (int v = 0; v < mapping->ModelProto()->variables_size(); ++v) {
        // Skip non-representative variables in symmetry orbits.
        if (!var_to_representative.empty() && var_to_representative[v] != v) {
          continue;
        }
        if (mapping->IsBoolean(v)) {
          const BooleanVariable bool_var = mapping->Literal(v).Variable();
          if (assignment.LiteralIsAssigned(Literal(bool_var, true))) continue;
          literals.push_back(Literal(bool_var, true));
          literals.push_back(Literal(bool_var, false));
        } else {
          IntegerVariable var = mapping->Integer(v);
          if (integer_trail->IsFixed(var)) continue;
          integer_vars.push_back(var);
          integer_vars.push_back(NegationOf(var));
        }
      }

      const auto search = [&parameters, model, &heuristics]() {
        return parameters.shaving_search_use_random_search()
                   ? SequentialSearch({RandomizeOnRestartHeuristic(
                                           /*lns_mode=*/false, model),
                                       heuristics.fixed_search})
                   : IntegerValueSelectionHeuristic(
                         SequentialSearch({SatSolverHeuristic(model),
                                           heuristics.fixed_search}),
                         model);
      };

      const int conflict_restart_limit =
          parameters.shaving_search_restart_limit();
      const double dtime_restart_limit =
          parameters.shaving_search_deterministic_time();
      const auto restart_policy = [conflict_restart_limit, dtime_restart_limit,
                                   model]() {
        if (conflict_restart_limit <= 0 && dtime_restart_limit <= 0.0) {
          return SatSolverRestartPolicy(model);
        } else if (dtime_restart_limit <= 0.0) {
          return RestartEveryKFailures(conflict_restart_limit,
                                       model->GetOrCreate<SatSolver>());
        } else if (conflict_restart_limit <= 0) {
          return RestartAfterDeterministicTime(dtime_restart_limit,
                                               model->GetOrCreate<TimeLimit>());
        } else {
          return RestartAfterFixedDeterministicTimeOrKFailures(
              dtime_restart_limit, model->GetOrCreate<TimeLimit>(),
              conflict_restart_limit, model->GetOrCreate<SatSolver>(),
              model->GetOrCreate<ModelRandomGenerator>());
        }
      };

      if (!integer_vars.empty()) {
        heuristics.decision_policies.push_back(SequentialSearch(
            {ShaveModelIntegerVariableBounds(integer_vars, model), search()}));
        heuristics.restart_policies.push_back(restart_policy());
      }

      if (!literals.empty()) {
        heuristics.decision_policies.push_back(SequentialSearch(
            {ShaveModelBooleanVariables(literals, model), search()}));
        heuristics.restart_policies.push_back(restart_policy());
      }

      if (heuristics.decision_policies.empty()) {
        heuristics.decision_policies.push_back(search());
        heuristics.restart_policies.push_back(SatSolverRestartPolicy(model));
      }

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
      binary_implication_graph_(model->GetOrCreate<BinaryImplicationGraph>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      implied_bounds_(model->GetOrCreate<ImpliedBounds>()),
      prober_(model->GetOrCreate<Prober>()),
      product_detector_(model->GetOrCreate<ProductDetector>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      pseudo_costs_(model->GetOrCreate<PseudoCosts>()),
      inprocessing_(model->GetOrCreate<Inprocessing>()) {}

bool IntegerSearchHelper::BeforeTakingDecision() {
  DCHECK(sat_solver_->PropagationIsDone());

  // If we pushed root level deductions, we go back to level zero and call
  // Propagate() to incorporate them. Note that the propagation is not strictly
  // needed, but it is nicer to be at fixed point when we call the level zero
  // callbacks.
  if (integer_trail_->HasPendingRootLevelDeduction()) {
    sat_solver_->Backtrack(0);
    if (!sat_solver_->Propagate()) {
      // This adds the UNSAT proof to the LRAT handler, if any.
      sat_solver_->ProcessCurrentConflict();
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }
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
    if (!sat_solver_->Propagate()) {
      // This adds the UNSAT proof to the LRAT handler, if any.
      sat_solver_->ProcessCurrentConflict();
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }
  }
  DCHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0)
      << "Level zero callback left decisions on the trail ?";

  // We apply inprocessing if we have budget.
  if (parameters_.use_sat_inprocessing() &&
      !inprocessing_->InprocessingRound()) {
    sat_solver_->NotifyThatModelIsUnsat();
    return false;
  }
  DCHECK_EQ(sat_solver_->CurrentDecisionLevel(), 0)
      << "Inprocessing left decisions on the trail ?";

  // Finally, once all the root-level work has been done, if we have
  // assumptions, we re-add them on the first decision level and continue
  // from there.
  return sat_solver_->ReapplyAssumptionsIfNeeded();
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
  do {
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
  } while (!time_limit_->LimitReached());
  return true;
}

bool IntegerSearchHelper::TakeDecision(Literal decision) {
  // If we are about to take a decision on a redundant literal, always
  // prefer to branch on the representative. This should help learn more
  // consistent conflicts.
  //
  // TODO(user): Ideally never learn anything on redundant variable. This is
  // a bit of work.
  decision = binary_implication_graph_->RepresentativeOf(decision);
  DCHECK(!sat_solver_->Assignment().LiteralIsAssigned(decision));

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
      // Note that in the presence of assumptions, BeforeTakingDecision()
      // will make sure to restore them. On restart, we always call Propagate()
      // as we might want to spent more effort on the root level LP relaxation
      // for instance.
      sat_solver_->IncreaseNumRestarts();
      sat_solver_->Backtrack(0);
      if (!sat_solver_->FinishPropagation()) {
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
      // In FIXED_SEARCH and until the first backtrack, the linear propagator
      // does not fully propagate in order to be faster. This can cause issues
      // once all the decisions are taken, because there might still be some
      // unassigned variables, or violated constraints. So do one round of full
      // propagation before accepting that solution.
      if (parameters_.search_branching() == SatParameters::FIXED_SEARCH) {
        LinearPropagator* linear_propagator =
            model_->Mutable<LinearPropagator>();
        if (linear_propagator != nullptr &&
            (!linear_propagator->PropagateAll() ||
             !sat_solver_->FinishPropagation())) {
          // Should not happen, but restart if it does (PropagateAll() disables
          // the incomplete propagation in the linear propagator, and is a no-op
          // after that; hence this can only happen at most once).
          sat_solver_->Backtrack(0);
          if (!sat_solver_->FinishPropagation()) {
            return sat_solver_->UnsatStatus();
          }
          continue;
        }
      }

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
  if (!sat_solver->ResetToLevelZero()) {
    return sat_solver->UnsatStatus();
  }

  // Sync bounds and maybe do some inprocessing.
  // We reuse the BeforeTakingDecision() code
  auto* helper = model->GetOrCreate<IntegerSearchHelper>();
  DCHECK(sat_solver->PropagationIsDone());
  if (!helper->BeforeTakingDecision()) {
    return sat_solver->UnsatStatus();
  }

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

}  // namespace sat
}  // namespace operations_research
