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

#include "ortools/sat/integer_search.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/implied_bounds.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/rins.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_inprocessing.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

IntegerLiteral AtMinValue(IntegerVariable var, IntegerTrail* integer_trail) {
  DCHECK(!integer_trail->IsCurrentlyIgnored(var));
  const IntegerValue lb = integer_trail->LowerBound(var);
  DCHECK_LE(lb, integer_trail->UpperBound(var));
  if (lb == integer_trail->UpperBound(var)) return IntegerLiteral();
  return IntegerLiteral::LowerOrEqual(var, lb);
}

IntegerLiteral ChooseBestObjectiveValue(IntegerVariable var, Model* model) {
  const auto& variables =
      model->GetOrCreate<ObjectiveDefinition>()->objective_impacting_variables;
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
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
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  auto* lp_dispatcher = model->GetOrCreate<LinearProgrammingDispatcher>();
  DCHECK(!integer_trail->IsCurrentlyIgnored(var));

  const IntegerVariable positive_var = PositiveVariable(var);
  const LinearProgrammingConstraint* lp =
      gtl::FindWithDefault(*lp_dispatcher, positive_var, nullptr);

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
    const std::vector<IntegerVariable>& vars, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [/*copy*/ vars, integer_trail]() {
    for (const IntegerVariable var : vars) {
      // Note that there is no point trying to fix a currently ignored variable.
      if (integer_trail->IsCurrentlyIgnored(var)) continue;
      const IntegerLiteral decision = AtMinValue(var, integer_trail);
      if (decision.IsValid()) return BooleanOrIntegerLiteral(decision);
    }
    return BooleanOrIntegerLiteral();
  };
}

std::function<BooleanOrIntegerLiteral()>
UnassignedVarWithLowestMinAtItsMinHeuristic(
    const std::vector<IntegerVariable>& vars, Model* model) {
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [/*copy */ vars, integer_trail]() {
    IntegerVariable candidate = kNoIntegerVariable;
    IntegerValue candidate_lb;
    for (const IntegerVariable var : vars) {
      if (integer_trail->IsCurrentlyIgnored(var)) continue;
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
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
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
    for (const IntegerLiteral l : encoder->GetAllIntegerLiterals(
             Literal(current_decision.boolean_literal_index))) {
      if (integer_trail->IsCurrentlyIgnored(l.var)) continue;

      // Sequentially try the value selection heuristics.
      for (const auto& value_heuristic : value_selection_heuristics) {
        const IntegerLiteral decision = value_heuristic(l.var);
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

  // Relaxation Solution based value.
  if (parameters.exploit_relaxation_solution()) {
    auto* relaxation_solutions =
        model->Get<SharedRelaxationSolutionRepository>();
    if (relaxation_solutions != nullptr) {
      value_selection_heuristics.push_back(
          [model, relaxation_solutions](IntegerVariable var) {
            VLOG(3) << "Using relaxation solution value selection heuristic.";
            return SplitUsingBestSolutionValueInRepository(
                var, *relaxation_solutions, model);
          });
    }
  }

  // Objective based value.
  if (parameters.exploit_objective()) {
    value_selection_heuristics.push_back([model](IntegerVariable var) {
      return ChooseBestObjectiveValue(var, model);
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

    // TODO(user): This will be overidden by the value decision heuristic in
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
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [repo, heuristic, trail, integer_trail]() {
    struct ToSchedule {
      // Variable to fix.
      LiteralIndex presence = kNoLiteralIndex;
      AffineExpression start;
      AffineExpression end;

      // Information to select best.
      IntegerValue size_min = kMaxIntegerValue;
      IntegerValue time = kMaxIntegerValue;
    };
    ToSchedule best;

    // TODO(user): we should also precompute fixed precedences and only fix
    // interval that have all their predecessors fixed.
    const int num_intervals = repo->NumIntervals();
    for (IntervalVariable i(0); i < num_intervals; ++i) {
      if (repo->IsAbsent(i)) continue;
      if (!repo->IsPresent(i) || !integer_trail->IsFixed(repo->Start(i)) ||
          !integer_trail->IsFixed(repo->End(i))) {
        IntegerValue time = integer_trail->LowerBound(repo->Start(i));
        if (repo->IsOptional(i)) {
          // For task whose presence is still unknown, our propagators should
          // have propagated the minimium time as if it was present. So this
          // should reflect the earliest time at which this interval can be
          // scheduled.
          time = std::max(time, integer_trail->ConditionalLowerBound(
                                    repo->PresenceLiteral(i), repo->Start(i)));
        }

        // For variable size, we compute the min size once the start is fixed
        // to time. This is needed to never pick the "artificial" makespan
        // interval at the end in priority compared to intervals that still
        // need to be scheduled.
        const IntegerValue size_min =
            std::max(integer_trail->LowerBound(repo->Size(i)),
                     integer_trail->LowerBound(repo->End(i)) - time);
        if (time < best.time ||
            (time == best.time && size_min < best.size_min)) {
          best.presence = repo->IsOptional(i) ? repo->PresenceLiteral(i).Index()
                                              : kNoLiteralIndex;
          best.start = repo->Start(i);
          best.end = repo->End(i);
          best.time = time;
          best.size_min = size_min;
        }
      }
    }
    if (best.time == kMaxIntegerValue) return BooleanOrIntegerLiteral();

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

      // Everything is fixed, dettach the override.
      const IntegerValue start = integer_trail->LowerBound(best.start);
      VLOG(2) << "Fixed @[" << start << ","
              << integer_trail->LowerBound(best.end) << "]"
              << (best.presence != kNoLiteralIndex
                      ? absl::StrCat(" presence=",
                                     Literal(best.presence).DebugString())
                      : "")
              << (best.time < start
                      ? absl::StrCat(" start_at_selection=", best.time.value())
                      : "");
      return BooleanOrIntegerLiteral();
    };

    return heuristic->next_decision_override();
  };
}

std::function<BooleanOrIntegerLiteral()> RandomizeOnRestartHeuristic(
    Model* model) {
  SatSolver* sat_solver = model->GetOrCreate<SatSolver>();
  SatDecisionPolicy* decision_policy = model->GetOrCreate<SatDecisionPolicy>();

  // TODO(user): Add other policy and perform more experiments.
  std::function<BooleanOrIntegerLiteral()> sat_policy =
      SatSolverHeuristic(model);
  std::vector<std::function<BooleanOrIntegerLiteral()>> policies{
      sat_policy, SequentialSearch({PseudoCost(model), sat_policy})};
  // The higher weight for the sat policy is because this policy actually
  // contains a lot of variation as we randomize the sat parameters.
  // TODO(user): Do more experiments to find better distribution.
  std::discrete_distribution<int> var_dist{3 /*sat_policy*/, 1 /*Pseudo cost*/};

  // Value selection.
  std::vector<std::function<IntegerLiteral(IntegerVariable)>>
      value_selection_heuristics;
  std::vector<int> value_selection_weight;

  // LP Based value.
  value_selection_heuristics.push_back([model](IntegerVariable var) {
    return SplitAroundLpValue(PositiveVariable(var), model);
  });
  value_selection_weight.push_back(8);

  // Solution based value.
  auto* response_manager = model->Get<SharedResponseManager>();
  if (response_manager != nullptr) {
    value_selection_heuristics.push_back(
        [model, response_manager](IntegerVariable var) {
          return SplitUsingBestSolutionValueInRepository(
              var, response_manager->SolutionsRepository(), model);
        });
    value_selection_weight.push_back(5);
  }

  // Relaxation solution based value.
  auto* relaxation_solutions = model->Get<SharedRelaxationSolutionRepository>();
  if (relaxation_solutions != nullptr) {
    value_selection_heuristics.push_back(
        [model, relaxation_solutions](IntegerVariable var) {
          return SplitUsingBestSolutionValueInRepository(
              var, *relaxation_solutions, model);
        });
    value_selection_weight.push_back(3);
  }

  // Middle value.
  auto* integer_trail = model->GetOrCreate<IntegerTrail>();
  value_selection_heuristics.push_back([integer_trail](IntegerVariable var) {
    return GreaterOrEqualToMiddleValue(var, integer_trail);
  });
  value_selection_weight.push_back(1);

  // Min value.
  value_selection_heuristics.push_back([integer_trail](IntegerVariable var) {
    return AtMinValue(var, integer_trail);
  });
  value_selection_weight.push_back(1);

  // Special case: Don't change the decision value.
  value_selection_weight.push_back(10);

  // TODO(user): These distribution values are just guessed values. They need
  // to be tuned.
  std::discrete_distribution<int> val_dist(value_selection_weight.begin(),
                                           value_selection_weight.end());

  int policy_index = 0;
  int val_policy_index = 0;
  auto* encoder = model->GetOrCreate<IntegerEncoder>();
  return [=]() mutable {
    if (sat_solver->CurrentDecisionLevel() == 0) {
      auto* random = model->GetOrCreate<ModelRandomGenerator>();
      RandomizeDecisionHeuristic(*random, model->GetOrCreate<SatParameters>());
      decision_policy->ResetDecisionHeuristic();

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
    for (const IntegerLiteral l : encoder->GetAllIntegerLiterals(
             Literal(current_decision.boolean_literal_index))) {
      if (integer_trail->IsCurrentlyIgnored(l.var)) continue;

      // Try the selected policy.
      const IntegerLiteral new_decision =
          value_selection_heuristics[val_policy_index](l.var);
      if (new_decision.IsValid()) return BooleanOrIntegerLiteral(new_decision);
    }

    // Selected policy failed. Revert back to original decision.
    return current_decision;
  };
}

// TODO(user): Avoid the quadratic algorithm!!
std::function<BooleanOrIntegerLiteral()> FollowHint(
    const std::vector<BooleanOrIntegerVariable>& vars,
    const std::vector<IntegerValue>& values, Model* model) {
  const Trail* trail = model->GetOrCreate<Trail>();
  const IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  return [=] {  // copy
    for (int i = 0; i < vars.size(); ++i) {
      const IntegerValue value = values[i];
      if (vars[i].bool_var != kNoBooleanVariable) {
        if (trail->Assignment().VariableIsAssigned(vars[i].bool_var)) continue;
        return BooleanOrIntegerLiteral(
            Literal(vars[i].bool_var, value == 1).Index());
      } else {
        const IntegerVariable integer_var = vars[i].int_var;
        if (integer_trail->IsCurrentlyIgnored(integer_var)) continue;
        if (integer_trail->IsFixed(integer_var)) continue;

        const IntegerVariable positive_var = PositiveVariable(integer_var);
        const IntegerLiteral decision = SplitAroundGivenValue(
            positive_var, positive_var != integer_var ? -value : value, model);
        if (decision.IsValid()) return BooleanOrIntegerLiteral(decision);

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
      std::function<BooleanOrIntegerLiteral()> decision_policy;
      if (parameters.randomize_search()) {
        decision_policy = RandomizeOnRestartHeuristic(model);
      } else {
        decision_policy = SatSolverHeuristic(model);
      }
      decision_policy =
          SequentialSearch({decision_policy, heuristics.fixed_search});
      decision_policy = IntegerValueSelectionHeuristic(decision_policy, model);
      heuristics.decision_policies = {decision_policy};
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
      // TODO(user): This is not used in any of our default config. remove?
      // It make also no sense to choose a value in the LP heuristic and then
      // override it with IntegerValueSelectionHeuristic(), clean that up.
      std::vector<std::function<BooleanOrIntegerLiteral()>> base_heuristics;
      base_heuristics.push_back(heuristics.fixed_search);
      for (const auto& ct :
           *(model->GetOrCreate<LinearProgrammingConstraintCollection>())) {
        base_heuristics.push_back(WrapIntegerLiteralHeuristic(
            ct->HeuristicLpReducedCostBinary(model)));
        base_heuristics.push_back(WrapIntegerLiteralHeuristic(
            ct->HeuristicLpMostInfeasibleBinary(model)));
      }
      heuristics.decision_policies = CompleteHeuristics(
          base_heuristics, SequentialSearch({SatSolverHeuristic(model),
                                             heuristics.fixed_search}));
      for (auto& ref : heuristics.decision_policies) {
        ref = IntegerValueSelectionHeuristic(ref, model);
      }
      heuristics.restart_policies.assign(heuristics.decision_policies.size(),
                                         SatSolverRestartPolicy(model));
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
          lp_heuristics, SequentialSearch({SatSolverHeuristic(model),
                                           heuristics.fixed_search}));
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
          {RandomizeOnRestartHeuristic(model), heuristics.fixed_search});
      heuristics.decision_policies = {search};
      heuristics.restart_policies = {
          RestartEveryKFailures(10, model->GetOrCreate<SatSolver>())};
      return;
    }
  }
}

std::vector<std::function<BooleanOrIntegerLiteral()>> CompleteHeuristics(
    const std::vector<std::function<BooleanOrIntegerLiteral()>>&
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
    : model_(model),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      implied_bounds_(model->GetOrCreate<ImpliedBounds>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      pseudo_costs_(model->GetOrCreate<PseudoCosts>()) {
  // This is needed for recording the pseudo-costs.
  const ObjectiveDefinition* objective = model->Get<ObjectiveDefinition>();
  if (objective != nullptr) objective_var_ = objective->objective_var;
}

bool IntegerSearchHelper::BeforeTakingDecision() {
  // If we pushed root level deductions, we restart to incorporate them.
  // Note that in the present of assumptions, it is important to return to
  // the level zero first ! otherwise, the new deductions will not be
  // incorporated and the solver will loop forever.
  if (integer_trail_->HasPendingRootLevelDeduction()) {
    sat_solver_->Backtrack(0);
    if (!sat_solver_->RestoreSolverToAssumptionLevel()) {
      return false;
    }
  }

  if (sat_solver_->CurrentDecisionLevel() == 0) {
    if (!implied_bounds_->EnqueueNewDeductions()) {
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }

    auto* level_zero_callbacks = model_->GetOrCreate<LevelZeroCallbackHelper>();
    for (const auto& cb : level_zero_callbacks->callbacks) {
      if (!cb()) {
        sat_solver_->NotifyThatModelIsUnsat();
        return false;
      }
    }

    if (model_->GetOrCreate<SatParameters>()->use_sat_inprocessing() &&
        !model_->GetOrCreate<Inprocessing>()->InprocessingRound()) {
      sat_solver_->NotifyThatModelIsUnsat();
      return false;
    }
  }
  return true;
}

LiteralIndex IntegerSearchHelper::GetDecision(
    const std::function<BooleanOrIntegerLiteral()>& f) {
  LiteralIndex decision = kNoLiteralIndex;
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
    }
    if (!new_decision.HasValue() &&
        integer_trail_->CurrentBranchHadAnIncompletePropagation()) {
      const IntegerVariable var = integer_trail_->FirstUnassignedVariable();
      if (var != kNoIntegerVariable) {
        new_decision.integer_literal = AtMinValue(var, integer_trail_);
      }
    }
    if (!new_decision.HasValue()) break;

    // Convert integer decision to literal one if needed.
    //
    // TODO(user): Ideally it would be cool to delay the creation even more
    // until we have a conflict with these decisions, but it is currrently
    // hard to do so.
    if (new_decision.boolean_literal_index != kNoLiteralIndex) {
      decision = new_decision.boolean_literal_index;
    } else {
      decision =
          encoder_->GetOrCreateAssociatedLiteral(new_decision.integer_literal)
              .Index();
    }
    if (sat_solver_->Assignment().LiteralIsAssigned(Literal(decision))) {
      // TODO(user): It would be nicer if this can never happen. For now, it
      // does because of the Propagate() not reaching the fixed point as
      // mentionned in a TODO above. As a work-around, we display a message
      // but do not crash and recall the decision heuristic.
      VLOG(1) << "Trying to take a decision that is already assigned!"
              << " Fix this. Continuing for now...";
      continue;
    }
    break;
  }
  return decision;
}

bool IntegerSearchHelper::TakeDecision(Literal decision) {
  // Record the changelist and objective bounds for updating pseudo costs.
  const std::vector<PseudoCosts::VariableBoundChange> bound_changes =
      GetBoundChanges(decision.Index(), model_);
  IntegerValue old_obj_lb = kMinIntegerValue;
  IntegerValue old_obj_ub = kMaxIntegerValue;
  if (objective_var_ != kNoIntegerVariable) {
    old_obj_lb = integer_trail_->LowerBound(objective_var_);
    old_obj_ub = integer_trail_->UpperBound(objective_var_);
  }
  const int old_level = sat_solver_->CurrentDecisionLevel();

  // TODO(user): on some problems, this function can be quite long. Expand
  // so that we can check the time limit at each step?
  sat_solver_->EnqueueDecisionAndBackjumpOnConflict(decision);

  // Update the implied bounds each time we enqueue a literal at level zero.
  // This is "almost free", so we might as well do it.
  if (old_level == 0 && sat_solver_->CurrentDecisionLevel() == 1) {
    implied_bounds_->ProcessIntegerTrail(decision);
  }

  // Update the pseudo costs.
  if (sat_solver_->CurrentDecisionLevel() > old_level &&
      objective_var_ != kNoIntegerVariable) {
    const IntegerValue new_obj_lb = integer_trail_->LowerBound(objective_var_);
    const IntegerValue new_obj_ub = integer_trail_->UpperBound(objective_var_);
    const IntegerValue objective_bound_change =
        (new_obj_lb - old_obj_lb) + (old_obj_ub - new_obj_ub);
    pseudo_costs_->UpdateCost(bound_changes, objective_bound_change);
  }

  sat_solver_->AdvanceDeterministicTime(time_limit_);
  return sat_solver_->ReapplyAssumptionsIfNeeded();
}

SatSolver::Status SolveIntegerProblem(Model* model) {
  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();
  if (time_limit->LimitReached()) return SatSolver::LIMIT_REACHED;

  SearchHeuristics& heuristics = *model->GetOrCreate<SearchHeuristics>();
  const int num_policies = heuristics.decision_policies.size();
  CHECK_NE(num_policies, 0);
  CHECK_EQ(num_policies, heuristics.restart_policies.size());

  auto* helper = model->GetOrCreate<IntegerSearchHelper>();

  // This is needed for recording the pseudo-costs.
  IntegerVariable objective_var = kNoIntegerVariable;
  {
    const ObjectiveDefinition* objective = model->Get<ObjectiveDefinition>();
    if (objective != nullptr) objective_var = objective->objective_var;
  }

  // Note that it is important to do the level-zero propagation if it wasn't
  // already done because EnqueueDecisionAndBackjumpOnConflict() assumes that
  // the solver is in a "propagated" state.
  SatSolver* const sat_solver = model->GetOrCreate<SatSolver>();

  // TODO(user): We have the issue that at level zero. calling the propagation
  // loop more than once can propagate more! This is because we call the LP
  // again and again on each level zero propagation. This is causing some
  // CHECKs() to fail in multithread (rarely) because when we associate new
  // literals to integer ones, Propagate() is indirectly called. Not sure yet
  // how to fix.
  if (!sat_solver->FinishPropagation()) return sat_solver->UnsatStatus();

  auto* prober = model->GetOrCreate<Prober>();

  const SatParameters& sat_parameters = *(model->GetOrCreate<SatParameters>());

  // Main search loop.
  const int64_t old_num_conflicts = sat_solver->num_failures();
  const int64_t conflict_limit = sat_parameters.max_number_of_conflicts();
  int64_t num_decisions_since_last_lp_record_ = 0;
  int64_t num_decisions_without_probing = 0;
  while (!time_limit->LimitReached() &&
         (sat_solver->num_failures() - old_num_conflicts < conflict_limit)) {
    // If needed, restart and switch decision_policy.
    if (heuristics.restart_policies[heuristics.policy_index]()) {
      if (!sat_solver->RestoreSolverToAssumptionLevel()) {
        return sat_solver->UnsatStatus();
      }
      heuristics.policy_index = (heuristics.policy_index + 1) % num_policies;
    }

    if (!helper->BeforeTakingDecision()) return sat_solver->UnsatStatus();

    LiteralIndex decision = kNoLiteralIndex;
    while (true) {
      if (heuristics.next_decision_override != nullptr) {
        // Note that to properly count the num_times, we do not want to move
        // this function, but actually call that copy.
        decision = helper->GetDecision(heuristics.next_decision_override);
        if (decision == kNoLiteralIndex) {
          heuristics.next_decision_override = nullptr;
        }
      }
      if (decision == kNoLiteralIndex) {
        decision = helper->GetDecision(
            heuristics.decision_policies[heuristics.policy_index]);
      }

      // Probing?
      //
      // TODO(user): Be smarter about what variables we probe, we can
      // also do more than one.
      if (decision != kNoLiteralIndex &&
          sat_solver->CurrentDecisionLevel() == 0 &&
          sat_parameters.probing_period_at_root() > 0 &&
          ++num_decisions_without_probing >=
              sat_parameters.probing_period_at_root()) {
        num_decisions_without_probing = 0;
        if (!prober->ProbeOneVariable(Literal(decision).Variable())) {
          return SatSolver::INFEASIBLE;
        }
        DCHECK_EQ(sat_solver->CurrentDecisionLevel(), 0);

        // We need to check after the probing that the literal is not fixed,
        // otherwise we just go to the next decision.
        if (sat_solver->Assignment().LiteralIsAssigned(Literal(decision))) {
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
    if (time_limit->LimitReached()) return SatSolver::LIMIT_REACHED;
    if (decision == kNoLiteralIndex) {
      // Save the current polarity of all Booleans in the solution. It will be
      // followed for the next SAT decisions. This is known to be a good policy
      // for optimization problem. Note that for decision problem we don't care
      // since we are just done as soon as a solution is found.
      //
      // This idea is kind of "well known", see for instance the "LinSBPS"
      // submission to the maxSAT 2018 competition by Emir Demirovic and Peter
      // Stuckey where they show it is a good idea and provide more references.
      if (model->GetOrCreate<SatParameters>()->use_optimization_hints()) {
        auto* sat_decision = model->GetOrCreate<SatDecisionPolicy>();
        const auto& trail = *model->GetOrCreate<Trail>();
        for (int i = 0; i < trail.Index(); ++i) {
          sat_decision->SetAssignmentPreference(trail[i], 0.0);
        }
      }
      return SatSolver::FEASIBLE;
    }

    if (!helper->TakeDecision(Literal(decision))) {
      return sat_solver->UnsatStatus();
    }

    // TODO(user): Experiment more around dynamically changing the
    // threshold for storing LP solutions in the pool. Alternatively expose
    // this as parameter so this can be tuned later.
    //
    // TODO(user): Avoid adding the same solution many time if the LP didn't
    // change. Avoid adding solution that are too deep in the tree (most
    // variable fixed). Also use a callback rather than having this here, we
    // don't want this file to depend on cp_model.proto.
    if (model->Get<SharedLPSolutionRepository>() != nullptr) {
      num_decisions_since_last_lp_record_++;
      if (num_decisions_since_last_lp_record_ >= 100) {
        // NOTE: We can actually record LP solutions more frequently. However
        // this process is time consuming and workers waste a lot of time doing
        // this. To avoid this we don't record solutions after each decision.
        RecordLPRelaxationValues(model);
        num_decisions_since_last_lp_record_ = 0;
      }
    }
  }
  return SatSolver::Status::LIMIT_REACHED;
}

SatSolver::Status ResetAndSolveIntegerProblem(
    const std::vector<Literal>& assumptions, Model* model) {
  SatSolver* const solver = model->GetOrCreate<SatSolver>();

  // Sync the bound first.
  if (!solver->ResetToLevelZero()) return solver->UnsatStatus();
  auto* level_zero_callbacks = model->GetOrCreate<LevelZeroCallbackHelper>();
  for (const auto& cb : level_zero_callbacks->callbacks) {
    if (!cb()) {
      solver->NotifyThatModelIsUnsat();
      return solver->UnsatStatus();
    }
  }

  // Add the assumptions if any and solve.
  if (!solver->ResetWithGivenAssumptions(assumptions)) {
    return solver->UnsatStatus();
  }
  return SolveIntegerProblem(model);
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

SatSolver::Status ContinuousProbing(
    const std::vector<BooleanVariable>& bool_vars,
    const std::vector<IntegerVariable>& int_vars,
    const std::function<void()>& feasible_solution_observer, Model* model) {
  VLOG(2) << "Start continuous probing with " << bool_vars.size()
          << " Boolean variables, and " << int_vars.size()
          << " integer variables";

  SatSolver* solver = model->GetOrCreate<SatSolver>();
  TimeLimit* time_limit = model->GetOrCreate<TimeLimit>();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
  const SatParameters& sat_parameters = *(model->GetOrCreate<SatParameters>());
  auto* level_zero_callbacks = model->GetOrCreate<LevelZeroCallbackHelper>();
  Prober* prober = model->GetOrCreate<Prober>();

  std::vector<BooleanVariable> active_vars;
  std::vector<BooleanVariable> integer_bounds;
  absl::flat_hash_set<BooleanVariable> integer_bounds_set;

  int loop = 0;
  while (!time_limit->LimitReached()) {
    VLOG(2) << "Probing loop " << loop++;

    // Sync the bounds first.
    auto SyncBounds = [solver, &level_zero_callbacks]() {
      if (!solver->ResetToLevelZero()) return false;
      for (const auto& cb : level_zero_callbacks->callbacks) {
        if (!cb()) {
          solver->NotifyThatModelIsUnsat();
          return false;
        }
      }
      return true;
    };
    if (!SyncBounds()) {
      return SatSolver::INFEASIBLE;
    }

    // Run sat in-processing to reduce the size of the clause database.
    if (sat_parameters.use_sat_inprocessing() &&
        !model->GetOrCreate<Inprocessing>()->InprocessingRound()) {
      return SatSolver::INFEASIBLE;
    }

    // TODO(user): Explore fast probing methods.

    // Probe each Boolean variable at most once per loop.
    absl::flat_hash_set<BooleanVariable> probed;

    // Probe variable bounds.
    // TODO(user): Probe optional variables.
    for (const IntegerVariable int_var : int_vars) {
      if (integer_trail->IsFixed(int_var) ||
          integer_trail->IsOptional(int_var)) {
        continue;
      }

      const BooleanVariable shave_lb =
          encoder
              ->GetOrCreateAssociatedLiteral(IntegerLiteral::LowerOrEqual(
                  int_var, integer_trail->LowerBound(int_var)))
              .Variable();
      if (!probed.contains(shave_lb)) {
        probed.insert(shave_lb);
        if (!prober->ProbeOneVariable(shave_lb)) {
          return SatSolver::INFEASIBLE;
        }
      }

      const BooleanVariable shave_ub =
          encoder
              ->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
                  int_var, integer_trail->UpperBound(int_var)))
              .Variable();
      if (!probed.contains(shave_ub)) {
        probed.insert(shave_ub);
        if (!prober->ProbeOneVariable(shave_ub)) {
          return SatSolver::INFEASIBLE;
        }
      }

      if (!SyncBounds()) {
        return SatSolver::INFEASIBLE;
      }
      if (time_limit->LimitReached()) {
        return SatSolver::LIMIT_REACHED;
      }
    }

    // Probe Boolean variables from the model.
    for (const BooleanVariable& bool_var : bool_vars) {
      if (solver->Assignment().VariableIsAssigned(bool_var)) continue;
      if (time_limit->LimitReached()) {
        return SatSolver::LIMIT_REACHED;
      }
      if (!SyncBounds()) {
        return SatSolver::INFEASIBLE;
      }
      if (!probed.contains(bool_var)) {
        probed.insert(bool_var);
        if (!prober->ProbeOneVariable(bool_var)) {
          return SatSolver::INFEASIBLE;
        }
      }
    }
  }
  return SatSolver::LIMIT_REACHED;
}

}  // namespace sat
}  // namespace operations_research
