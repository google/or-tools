// Copyright 2010-2017 Google
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
#include <unordered_map>

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
    const std::unordered_map<int, std::pair<int64, int64>>&
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
  if (cp_model_proto.search_strategy().empty()) {
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
    return FirstUnassignedVarAtItsMinHeuristic(decisions, model);
  }

  std::vector<Strategy> strategies;
  std::unordered_map<int, std::pair<int64, int64>> var_to_coeff_offset_pair;
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
  return ConstructSearchStrategyInternal(var_to_coeff_offset_pair, strategies,
                                         model);
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
        old_domains[ref] = new_domain;
        absl::StrAppend(&to_display, cp_model_proto.variables(ref).name(), " [",
                        new_domain.first, ",", new_domain.second, "]\n");
      }
    }
    LOG(INFO) << to_display;
    return decision;
  };
}

}  // namespace sat
}  // namespace operations_research
