// Copyright 2010-2018 Google LLC
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

#include "ortools/sat/cp_model_search.h"

#include <random>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "ortools/base/cleanup.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

// The function responsible for implementing the chosen search strategy.
//
// TODO(user): expose and unit-test, it seems easy to get the order wrong, and
// that would not change the correctness.
struct Strategy {
  std::vector<IntegerVariable> variables;
  DecisionStrategyProto::VariableSelectionStrategy var_strategy;
  DecisionStrategyProto::DomainReductionStrategy domain_strategy;
};

// Stores one variable and its strategy value.
struct VarValue {
  IntegerVariable var;
  IntegerValue value;
};

const std::function<LiteralIndex()> ConstructSearchStrategyInternal(
    const absl::flat_hash_map<int, std::pair<int64, int64>>&
        var_to_coeff_offset_pair,
    const std::vector<Strategy>& strategies, Model* model) {
  IntegerEncoder* const integer_encoder = model->GetOrCreate<IntegerEncoder>();
  IntegerTrail* const integer_trail = model->GetOrCreate<IntegerTrail>();

  // Note that we copy strategies to keep the return function validity
  // independently of the life of the passed vector.
  return [integer_encoder, integer_trail, strategies, var_to_coeff_offset_pair,
          model]() {
    const SatParameters* const parameters = model->GetOrCreate<SatParameters>();

    for (const Strategy& strategy : strategies) {
      IntegerVariable candidate = kNoIntegerVariable;
      IntegerValue candidate_value = kMaxIntegerValue;
      IntegerValue candidate_lb;
      IntegerValue candidate_ub;

      // TODO(user): Improve the complexity if this becomes an issue which
      // may be the case if we do a fixed_search.

      // To store equivalent variables in randomized search.
      std::vector<VarValue> active_vars;

      for (const IntegerVariable var : strategy.variables) {
        if (integer_trail->IsCurrentlyIgnored(var)) continue;
        const IntegerValue lb = integer_trail->LowerBound(var);
        const IntegerValue ub = integer_trail->UpperBound(var);
        if (lb == ub) continue;
        IntegerValue value(0);
        IntegerValue coeff(1);
        IntegerValue offset(0);
        if (gtl::ContainsKey(var_to_coeff_offset_pair, var.value())) {
          const auto coeff_offset =
              gtl::FindOrDie(var_to_coeff_offset_pair, var.value());
          coeff = coeff_offset.first;
          offset = coeff_offset.second;
        }
        DCHECK_GT(coeff, 0);
        switch (strategy.var_strategy) {
          case DecisionStrategyProto::CHOOSE_FIRST:
            break;
          case DecisionStrategyProto::CHOOSE_LOWEST_MIN:
            value = coeff * lb + offset;
            break;
          case DecisionStrategyProto::CHOOSE_HIGHEST_MAX:
            value = -(coeff * ub + offset);
            break;
          case DecisionStrategyProto::CHOOSE_MIN_DOMAIN_SIZE:
            // TODO(user): Evaluate an exact domain computation.
            value = coeff * (ub - lb + 1);
            break;
          case DecisionStrategyProto::CHOOSE_MAX_DOMAIN_SIZE:
            // TODO(user): Evaluate an exact domain computation.
            value = -coeff * (ub - lb + 1);
            break;
          default:
            LOG(FATAL) << "Unknown VariableSelectionStrategy "
                       << strategy.var_strategy;
        }
        if (value < candidate_value) {
          candidate = var;
          candidate_lb = lb;
          candidate_ub = ub;
          candidate_value = value;
        }
        if (strategy.var_strategy == DecisionStrategyProto::CHOOSE_FIRST &&
            !parameters->randomize_search()) {
          break;
        } else if (parameters->randomize_search()) {
          if (active_vars.empty() ||
              value <= candidate_value +
                           parameters->search_randomization_tolerance()) {
            active_vars.push_back({var, value});
          }
        }
      }
      if (candidate == kNoIntegerVariable) continue;
      if (parameters->randomize_search()) {
        CHECK(!active_vars.empty());
        const IntegerValue threshold(
            candidate_value + parameters->search_randomization_tolerance());
        auto is_above_tolerance = [threshold](const VarValue& entry) {
          return entry.value > threshold;
        };
        // Remove all values above tolerance.
        active_vars.erase(std::remove_if(active_vars.begin(), active_vars.end(),
                                         is_above_tolerance),
                          active_vars.end());
        const int winner =
            std::uniform_int_distribution<int>(0, active_vars.size() - 1)(
                *model->GetOrCreate<ModelRandomGenerator>());
        candidate = active_vars[winner].var;
        candidate_lb = integer_trail->LowerBound(candidate);
        candidate_ub = integer_trail->UpperBound(candidate);
      }

      IntegerLiteral literal;
      switch (strategy.domain_strategy) {
        case DecisionStrategyProto::SELECT_MIN_VALUE:
          literal = IntegerLiteral::LowerOrEqual(candidate, candidate_lb);
          break;
        case DecisionStrategyProto::SELECT_MAX_VALUE:
          literal = IntegerLiteral::GreaterOrEqual(candidate, candidate_ub);
          break;
        case DecisionStrategyProto::SELECT_LOWER_HALF:
          literal = IntegerLiteral::LowerOrEqual(
              candidate, candidate_lb + (candidate_ub - candidate_lb) / 2);
          break;
        case DecisionStrategyProto::SELECT_UPPER_HALF:
          literal = IntegerLiteral::GreaterOrEqual(
              candidate, candidate_ub - (candidate_ub - candidate_lb) / 2);
          break;
        default:
          LOG(FATAL) << "Unknown DomainReductionStrategy "
                     << strategy.domain_strategy;
      }
      return integer_encoder->GetOrCreateAssociatedLiteral(literal).Index();
    }
    return kNoLiteralIndex;
  };
}

std::function<LiteralIndex()> ConstructSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    IntegerVariable objective_var, Model* model) {
  // Default strategy is to instantiate the IntegerVariable in order.
  std::function<LiteralIndex()> default_search_strategy = nullptr;
  const bool instantiate_all_variables =
      model->GetOrCreate<SatParameters>()->instantiate_all_variables();

  if (instantiate_all_variables) {
    std::vector<IntegerVariable> decisions;
    for (const IntegerVariable var : variable_mapping) {
      if (var == kNoIntegerVariable) continue;

      // Make sure we try to fix the objective to its lowest value first.
      if (var == NegationOf(objective_var)) {
        decisions.push_back(objective_var);
      } else {
        decisions.push_back(var);
      }
    }
    default_search_strategy =
        FirstUnassignedVarAtItsMinHeuristic(decisions, model);
  }

  std::vector<Strategy> strategies;
  absl::flat_hash_map<int, std::pair<int64, int64>> var_to_coeff_offset_pair;
  for (const DecisionStrategyProto& proto : cp_model_proto.search_strategy()) {
    strategies.push_back(Strategy());
    Strategy& strategy = strategies.back();
    for (const int ref : proto.variables()) {
      strategy.variables.push_back(
          RefIsPositive(ref) ? variable_mapping[ref]
                             : NegationOf(variable_mapping[PositiveRef(ref)]));
    }
    strategy.var_strategy = proto.variable_selection_strategy();
    strategy.domain_strategy = proto.domain_reduction_strategy();
    for (const auto& tranform : proto.transformations()) {
      const int ref = tranform.var();
      const IntegerVariable var =
          RefIsPositive(ref) ? variable_mapping[ref]
                             : NegationOf(variable_mapping[PositiveRef(ref)]);
      if (!gtl::ContainsKey(var_to_coeff_offset_pair, var.value())) {
        var_to_coeff_offset_pair[var.value()] = {tranform.positive_coeff(),
                                                 tranform.offset()};
      }
    }
  }
  if (instantiate_all_variables) {
    return SequentialSearch({ConstructSearchStrategyInternal(
                                 var_to_coeff_offset_pair, strategies, model),
                             default_search_strategy});
  } else {
    return ConstructSearchStrategyInternal(var_to_coeff_offset_pair, strategies,
                                           model);
  }
}

std::function<LiteralIndex()> InstrumentSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    const std::function<LiteralIndex()>& instrumented_strategy, Model* model) {
  std::vector<int> ref_to_display;
  for (int i = 0; i < cp_model_proto.variables_size(); ++i) {
    if (variable_mapping[i] == kNoIntegerVariable) continue;
    if (cp_model_proto.variables(i).name().empty()) continue;
    ref_to_display.push_back(i);
  }
  std::sort(ref_to_display.begin(), ref_to_display.end(), [&](int i, int j) {
    return cp_model_proto.variables(i).name() <
           cp_model_proto.variables(j).name();
  });

  std::vector<std::pair<int64, int64>> old_domains(variable_mapping.size());
  return [=]() mutable {
    const LiteralIndex decision = instrumented_strategy();
    if (decision == kNoLiteralIndex) return decision;

    for (const IntegerLiteral i_lit :
         model->Get<IntegerEncoder>()->GetAllIntegerLiterals(
             Literal(decision))) {
      LOG(INFO) << "decision " << i_lit;
    }
    const int level = model->Get<Trail>()->CurrentDecisionLevel();
    std::string to_display =
        absl::StrCat("Diff since last call, level=", level, "\n");
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    for (const int ref : ref_to_display) {
      const IntegerVariable var = variable_mapping[ref];
      const std::pair<int64, int64> new_domain(
          integer_trail->LowerBound(var).value(),
          integer_trail->UpperBound(var).value());
      if (new_domain != old_domains[ref]) {
        absl::StrAppend(&to_display, cp_model_proto.variables(ref).name(), " [",
                        old_domains[ref].first, ",", old_domains[ref].second,
                        "] -> [", new_domain.first, ",", new_domain.second,
                        "]\n");
        old_domains[ref] = new_domain;
      }
    }
    LOG(INFO) << to_display;
    return decision;
  };
}

SatParameters DiversifySearchParameters(const SatParameters& params,
                                        const CpModelProto& cp_model,
                                        const int worker_id,
                                        std::string* name) {
  // Note: in the flatzinc setting, we know we always have a fixed search
  //       defined.
  // Things to try:
  //   - Specialize for purely boolean problems
  //   - Disable linearization_level options for non linear problems
  //   - Fast restart in randomized search
  //   - Different propatation levels for scheduling constraints
  SatParameters new_params = params;
  new_params.set_random_seed(params.random_seed() + worker_id);
  int index = worker_id;

  if (cp_model.has_objective()) {
    if (index == 0) {  // Use default parameters and automatic search.
      new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
      new_params.set_linearization_level(1);
      *name = "auto";
      return new_params;
    }

    if (cp_model.search_strategy_size() > 0) {
      if (--index == 0) {  // Use default parameters and fixed search.
        new_params.set_search_branching(SatParameters::FIXED_SEARCH);
        *name = "fixed";
        return new_params;
      }
    } else {
      // TODO(user): Disable lp_br if linear part is small or empty.
      if (--index == 0) {
        new_params.set_search_branching(SatParameters::LP_SEARCH);
        *name = "lp_br";
        return new_params;
      }
    }

    if (--index == 0) {
      new_params.set_search_branching(SatParameters::PSEUDO_COST_SEARCH);
      *name = "pseudo_cost";
      return new_params;
    }

    // TODO(user): Disable no_lp if linear part is small.
    if (--index == 0) {  // Remove LP relaxation.
      new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
      new_params.set_linearization_level(0);
      *name = "no_lp";
      return new_params;
    }

    // TODO(user): Disable max_lp if no change in linearization against auto.
    if (--index == 0) {  // Reinforce LP relaxation.
      new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
      new_params.set_linearization_level(2);
      new_params.set_add_cg_cuts(true);
      *name = "max_lp";
      return new_params;
    }

    // Only add this strategy if we have enough worker left for LNS.
    if (params.num_search_workers() > 8 && --index == 0) {
      new_params.set_search_branching(
          SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH);
      *name = "quick_restart";
      return new_params;
    }

    if (cp_model.objective().vars_size() > 1) {
      if (--index == 0) {  // Core based approach.
        new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
        new_params.set_optimize_with_core(true);
        new_params.set_linearization_level(0);
        *name = "core";
        return new_params;
      }
    }

    // Use LNS for the remaining workers.
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    new_params.set_use_lns(true);
    new_params.set_lns_num_threads(1);
    *name = absl::StrFormat("lns_%i", index);
    return new_params;
  } else {
    // The goal here is to try fixed and free search on the first two threads.
    // Then maximize diversity on the extra threads.
    int index = worker_id;

    if (index == 0) {  // Default automatic search.
      new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
      *name = "auto";
      return new_params;
    }

    if (cp_model.search_strategy_size() > 0) {  // Use predefined search.
      if (--index == 0) {
        new_params.set_search_branching(SatParameters::FIXED_SEARCH);
        *name = "fixed";
        return new_params;
      }
    }

    if (--index == 0) {  // Reduce boolean encoding.
      new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
      new_params.set_boolean_encoding_level(0);
      *name = "less encoding";
      return new_params;
    }

    if (--index == 0) {
      new_params.set_search_branching(
          SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH);
      *name = "random";
      return new_params;
    }

    // Randomized fixed search.
    new_params.set_search_branching(SatParameters::FIXED_SEARCH);
    new_params.set_randomize_search(true);
    new_params.set_search_randomization_tolerance(index);
    *name = absl::StrFormat("random_%i", index);
    return new_params;
  }
}

// TODO(user): Better stats in the multi thread case.
//                Should we cumul conflicts, branches... ?
bool MergeOptimizationSolution(const CpSolverResponse& response, bool maximize,
                               CpSolverResponse* best) {
  // In all cases, we always update the best objective bound similarly.
  const double current_best_bound = response.best_objective_bound();
  const double previous_best_bound = best->best_objective_bound();
  const double new_best_objective_bound =
      maximize ? std::min(previous_best_bound, current_best_bound)
               : std::max(previous_best_bound, current_best_bound);
  const auto cleanup = ::gtl::MakeCleanup([&best, new_best_objective_bound]() {
    if (best->status() != OPTIMAL) {
      best->set_best_objective_bound(new_best_objective_bound);
    } else {
      best->set_best_objective_bound(best->objective_value());
    }
  });

  switch (response.status()) {
    case CpSolverStatus::FEASIBLE: {
      const bool is_improving =
          maximize ? response.objective_value() > best->objective_value()
                   : response.objective_value() < best->objective_value();
      // TODO(user): return OPTIMAL if objective is tight.
      if (is_improving) {
        // Overwrite solution and fix best_objective_bound.
        *best = response;
        return true;
      }
      if (new_best_objective_bound != previous_best_bound) {
        // The new solution has a worse objective value, but a better
        // best_objective_bound.
        return true;
      }
      return false;
    }
    case CpSolverStatus::INFEASIBLE: {
      if (best->status() == CpSolverStatus::UNKNOWN ||
          best->status() == CpSolverStatus::INFEASIBLE) {
        // Stores the unsat solution.
        *best = response;
        return true;
      } else {
        // It can happen that the LNS finds the best solution, but does
        // not prove it. Then another worker pulls in the best solution,
        // does not improve upon it, returns UNSAT if it has not found a
        // previous solution, or OPTIMAL with a bad objective value, and
        // stops all other workers. In that case, if the last solution
        // found has a FEASIBLE status, it is indeed optimal, and
        // should be marked as thus.
        best->set_status(CpSolverStatus::OPTIMAL);
        return false;
      }
      break;
    }
    case CpSolverStatus::OPTIMAL: {
      const double previous = best->objective_value();
      const double current = response.objective_value();
      if ((maximize && current >= previous) ||
          (!maximize && current <= previous)) {
        // We always overwrite the best solution with an at-least as good
        // optimal solution.
        *best = response;
        return true;
      } else {
        // We are in the same case as the INFEASIBLE above.  Solution
        // synchronization has forced the solver to exit with a sub-optimal
        // solution, believing it was optimal.
        best->set_status(CpSolverStatus::OPTIMAL);
        return false;
      }
      break;
    }
    case CpSolverStatus::UNKNOWN: {
      if (best->status() == CpSolverStatus::UNKNOWN) {
        if (!std::isfinite(best->objective_value())) {
          // TODO(user): Find a better test for never updated response.
          *best = response;
        } else if (maximize) {
          // Update objective_value and best_objective_bound.
          best->set_objective_value(
              std::max(best->objective_value(), response.objective_value()));
        } else {
          best->set_objective_value(
              std::min(best->objective_value(), response.objective_value()));
        }
      }
      return false;
      break;
    }
    default: {
      return false;
    }
  }
}

}  // namespace sat
}  // namespace operations_research
