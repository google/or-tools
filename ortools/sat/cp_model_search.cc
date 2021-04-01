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

#include <cstdint>
#include <random>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
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

const std::function<BooleanOrIntegerLiteral()> ConstructSearchStrategyInternal(
    const absl::flat_hash_map<int, std::pair<int64_t, int64_t>>&
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

        // TODO(user): deal with integer overflow in case of wrongly specified
        // coeff.
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
        case DecisionStrategyProto::SELECT_MEDIAN_VALUE:
          // TODO(user): Implement the correct method.
          literal = IntegerLiteral::LowerOrEqual(candidate, candidate_lb);
          break;
        default:
          LOG(FATAL) << "Unknown DomainReductionStrategy "
                     << strategy.domain_strategy;
      }
      return BooleanOrIntegerLiteral(literal);
    }
    return BooleanOrIntegerLiteral();
  };
}

std::function<BooleanOrIntegerLiteral()> ConstructSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    IntegerVariable objective_var, Model* model) {
  // Default strategy is to instantiate the IntegerVariable in order.
  std::function<BooleanOrIntegerLiteral()> default_search_strategy = nullptr;
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
  absl::flat_hash_map<int, std::pair<int64_t, int64_t>>
      var_to_coeff_offset_pair;
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
    for (const auto& transform : proto.transformations()) {
      const int ref = transform.var();
      const IntegerVariable var =
          RefIsPositive(ref) ? variable_mapping[ref]
                             : NegationOf(variable_mapping[PositiveRef(ref)]);
      if (!gtl::ContainsKey(var_to_coeff_offset_pair, var.value())) {
        var_to_coeff_offset_pair[var.value()] = {transform.positive_coeff(),
                                                 transform.offset()};
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

std::function<BooleanOrIntegerLiteral()> InstrumentSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    const std::function<BooleanOrIntegerLiteral()>& instrumented_strategy,
    Model* model) {
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

  std::vector<std::pair<int64_t, int64_t>> old_domains(variable_mapping.size());
  return [instrumented_strategy, model, variable_mapping, cp_model_proto,
          old_domains, ref_to_display]() mutable {
    const BooleanOrIntegerLiteral decision = instrumented_strategy();
    if (!decision.HasValue()) return decision;

    if (decision.boolean_literal_index != kNoLiteralIndex) {
      const Literal l = Literal(decision.boolean_literal_index);
      LOG(INFO) << "Boolean decision " << l;
      for (const IntegerLiteral i_lit :
           model->Get<IntegerEncoder>()->GetAllIntegerLiterals(l)) {
        LOG(INFO) << " - associated with " << i_lit;
      }
    } else {
      LOG(INFO) << "Integer decision " << decision.integer_literal;
    }
    const int level = model->Get<Trail>()->CurrentDecisionLevel();
    std::string to_display =
        absl::StrCat("Diff since last call, level=", level, "\n");
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    for (const int ref : ref_to_display) {
      const IntegerVariable var = variable_mapping[ref];
      const std::pair<int64_t, int64_t> new_domain(
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

// Note: in flatzinc setting, we know we always have a fixed search defined.
//
// Things to try:
//   - Specialize for purely boolean problems
//   - Disable linearization_level options for non linear problems
//   - Fast restart in randomized search
//   - Different propatation levels for scheduling constraints
std::vector<SatParameters> GetDiverseSetOfParameters(
    const SatParameters& base_params, const CpModelProto& cp_model,
    const int num_workers) {
  // Defines a set of named strategies so it is easier to read in one place
  // the one that are used. See below.
  std::map<std::string, SatParameters> strategies;

  // Lp variations only.
  {
    SatParameters new_params = base_params;
    new_params.set_linearization_level(0);
    strategies["no_lp"] = new_params;
    new_params.set_linearization_level(1);
    strategies["default_lp"] = new_params;
    new_params.set_linearization_level(2);
    new_params.set_add_lp_constraints_lazily(false);
    strategies["max_lp"] = new_params;
  }

  // Core. Note that we disable the lp here because it is faster on the minizinc
  // benchmark.
  //
  // TODO(user): Do more experiments, the LP with core could be useful, but we
  // probably need to incorporate the newly created integer variables from the
  // core algorithm into the LP.
  {
    SatParameters new_params = base_params;
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    new_params.set_optimize_with_core(true);
    new_params.set_linearization_level(0);
    strategies["core"] = new_params;
  }

  // It can be interesting to try core and lp.
  {
    SatParameters new_params = base_params;
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    new_params.set_optimize_with_core(true);
    new_params.set_linearization_level(1);
    strategies["core_default_lp"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    new_params.set_optimize_with_core(true);
    new_params.set_linearization_level(2);
    strategies["core_max_lp"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    new_params.set_use_probing_search(true);
    strategies["probing"] = new_params;
  }

  // Search variation.
  {
    SatParameters new_params = base_params;
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    strategies["auto"] = new_params;

    new_params.set_search_branching(SatParameters::FIXED_SEARCH);
    strategies["fixed"] = new_params;

    new_params.set_search_branching(
        SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH);
    strategies["quick_restart"] = new_params;

    new_params.set_search_branching(
        SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH);
    new_params.set_linearization_level(0);
    strategies["quick_restart_no_lp"] = new_params;

    // We force the max lp here too.
    new_params.set_linearization_level(2);
    new_params.set_search_branching(SatParameters::LP_SEARCH);
    strategies["reduced_costs"] = new_params;

    // For this one, we force other param too.
    new_params.set_linearization_level(2);
    new_params.set_search_branching(SatParameters::PSEUDO_COST_SEARCH);
    new_params.set_exploit_best_solution(true);
    strategies["pseudo_costs"] = new_params;
  }

  // Less encoding.
  {
    SatParameters new_params = base_params;
    new_params.set_boolean_encoding_level(0);
    strategies["less_encoding"] = new_params;
  }

  // Our current set of strategies
  //
  // TODO(user, fdid): Avoid launching two strategies if they are the same,
  // like if there is no lp, or everything is already linearized at level 1.
  std::vector<std::string> names;
  if (base_params.reduce_memory_usage_in_interleave_mode() &&
      base_params.interleave_search()) {
    // Low memory mode for interleaved search in single thread.
    if (cp_model.has_objective()) {
      names.push_back("default_lp");
      names.push_back(!cp_model.search_strategy().empty() ? "fixed"
                                                          : "pseudo_costs");
      names.push_back(cp_model.objective().vars_size() > 1 ? "core" : "no_lp");
      names.push_back("max_lp");
    } else {
      names.push_back("default_lp");
      names.push_back(cp_model.search_strategy_size() > 0 ? "fixed" : "no_lp");
      names.push_back("less_encoding");
      names.push_back("max_lp");
      names.push_back("quick_restart");
    }
  } else if (cp_model.has_objective()) {
    names.push_back("default_lp");
    names.push_back(!cp_model.search_strategy().empty() ? "fixed"
                                                        : "reduced_costs");
    names.push_back("pseudo_costs");
    names.push_back("no_lp");
    names.push_back("max_lp");
    if (cp_model.objective().vars_size() > 1) names.push_back("core");
    // TODO(user): Experiment with core and LP.

    // Only add this strategy if we have enough worker left for LNS.
    if (num_workers > 8 || base_params.interleave_search()) {
      names.push_back("quick_restart");
    }
    if (num_workers > 10) {
      names.push_back("quick_restart_no_lp");
    }
  } else {
    names.push_back("default_lp");
    if (cp_model.search_strategy_size() > 0) names.push_back("fixed");
    names.push_back("less_encoding");
    names.push_back("no_lp");
    names.push_back("max_lp");
    names.push_back("quick_restart");
    if (num_workers > 10) {
      names.push_back("quick_restart_no_lp");
    }
  }
  if (num_workers > 12) {
    names.push_back("probing");
  }

  // Creates the diverse set of parameters with names and seed. We remove the
  // last ones if needed below.
  std::vector<SatParameters> result;
  for (const std::string& name : names) {
    SatParameters new_params = strategies.at(name);
    new_params.set_name(name);
    new_params.set_random_seed(result.size() + 1);
    result.push_back(new_params);
  }

  // If there is no objective, we complete with randomized fixed search.
  // If there is an objective, the extra workers will use LNS.
  if (!cp_model.has_objective()) {
    int target = num_workers;

    // If strategies that do not require a full worker are present, leave one
    // worker for them.
    if (!base_params.interleave_search() &&
        (base_params.use_rins_lns() || base_params.use_relaxation_lns() ||
         base_params.use_feasibility_pump())) {
      target = std::max(1, num_workers - 1);
    }

    int index = 1;
    while (result.size() < target) {
      // TODO(user): This doesn't make sense if there is no fixed search.
      SatParameters new_params = base_params;
      new_params.set_search_branching(SatParameters::FIXED_SEARCH);
      new_params.set_randomize_search(true);
      new_params.set_search_randomization_tolerance(index);
      new_params.set_random_seed(result.size() + 1);
      new_params.set_name(absl::StrCat("random_", index));
      result.push_back(new_params);
      ++index;
    }
  }

  // If we are not in interleave search, we cannot run more strategies than
  // the number of worker.
  //
  // TODO(user): consider using LNS if we use a small number of workers.
  if (!base_params.interleave_search() && result.size() > num_workers) {
    result.resize(num_workers);
  }

  return result;
}

}  // namespace sat
}  // namespace operations_research
