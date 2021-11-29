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

#include "ortools/sat/cp_model_search.h"

#include <cstdint>
#include <limits>
#include <random>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_format.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/util.h"

namespace operations_research {
namespace sat {

CpModelView::CpModelView(Model* model)
    : mapping_(*model->GetOrCreate<CpModelMapping>()),
      boolean_assignment_(model->GetOrCreate<Trail>()->Assignment()),
      integer_trail_(*model->GetOrCreate<IntegerTrail>()),
      integer_encoder_(*model->GetOrCreate<IntegerEncoder>()) {}

int CpModelView::NumVariables() const { return mapping_.NumProtoVariables(); }

bool CpModelView::IsFixed(int var) const {
  if (mapping_.IsBoolean(var)) {
    return boolean_assignment_.VariableIsAssigned(
        mapping_.Literal(var).Variable());
  } else if (mapping_.IsInteger(var)) {
    return integer_trail_.IsFixed(mapping_.Integer(var));
  }
  return true;  // Default.
}

bool CpModelView::IsCurrentlyFree(int var) const {
  return mapping_.IsInteger(var) &&
         integer_trail_.IsCurrentlyIgnored(mapping_.Integer(var));
}

int64_t CpModelView::Min(int var) const {
  if (mapping_.IsBoolean(var)) {
    const Literal l = mapping_.Literal(var);
    return boolean_assignment_.LiteralIsTrue(l) ? 1 : 0;
  } else if (mapping_.IsInteger(var)) {
    return integer_trail_.LowerBound(mapping_.Integer(var)).value();
  }
  return 0;  // Default.
}

int64_t CpModelView::Max(int var) const {
  if (mapping_.IsBoolean(var)) {
    const Literal l = mapping_.Literal(var);
    return boolean_assignment_.LiteralIsFalse(l) ? 0 : 1;
  } else if (mapping_.IsInteger(var)) {
    return integer_trail_.UpperBound(mapping_.Integer(var)).value();
  }
  return 0;  // Default.
}

BooleanOrIntegerLiteral CpModelView::GreaterOrEqual(int var,
                                                    int64_t value) const {
  DCHECK(!IsFixed(var));
  BooleanOrIntegerLiteral result;
  if (mapping_.IsBoolean(var)) {
    DCHECK(value == 0 || value == 1);
    if (value == 1) {
      result.boolean_literal_index = mapping_.Literal(var).Index();
    }
  } else if (mapping_.IsInteger(var)) {
    result.integer_literal = IntegerLiteral::GreaterOrEqual(
        mapping_.Integer(var), IntegerValue(value));
  }
  return result;
}

BooleanOrIntegerLiteral CpModelView::LowerOrEqual(int var,
                                                  int64_t value) const {
  DCHECK(!IsFixed(var));
  BooleanOrIntegerLiteral result;
  if (mapping_.IsBoolean(var)) {
    DCHECK(value == 0 || value == 1);
    if (value == 0) {
      result.boolean_literal_index = mapping_.Literal(var).NegatedIndex();
    }
  } else if (mapping_.IsInteger(var)) {
    result.integer_literal = IntegerLiteral::LowerOrEqual(mapping_.Integer(var),
                                                          IntegerValue(value));
  }
  return result;
}

BooleanOrIntegerLiteral CpModelView::MedianValue(int var) const {
  DCHECK(!IsFixed(var));
  BooleanOrIntegerLiteral result;
  if (mapping_.IsBoolean(var)) {
    result.boolean_literal_index = mapping_.Literal(var).NegatedIndex();
  } else if (mapping_.IsInteger(var)) {
    const IntegerVariable variable = mapping_.Integer(var);
    CHECK_NE(variable, kNoIntegerVariable);
    CHECK(integer_encoder_.VariableIsFullyEncoded(variable));
    std::vector<IntegerEncoder::ValueLiteralPair> encoding =
        integer_encoder_.RawDomainEncoding(variable);
    std::sort(encoding.begin(), encoding.end());
    std::vector<Literal> unassigned_sorted_literals;
    for (const auto& p : encoding) {
      if (!boolean_assignment_.LiteralIsAssigned(p.literal)) {
        unassigned_sorted_literals.push_back(p.literal);
      }
    }
    // 5 values -> returns the second.
    // 4 values -> returns the second too.
    // Array is 0 based.
    const int target = (unassigned_sorted_literals.size() + 1) / 2 - 1;
    result.boolean_literal_index = unassigned_sorted_literals[target].Index();
  }
  return result;
}

// Stores one variable and its strategy value.
struct VarValue {
  int ref;
  int64_t value;
};

namespace {

// TODO(user): Save this somewhere instead of recomputing it.
bool ModelHasSchedulingConstraints(const CpModelProto& cp_model_proto) {
  for (const ConstraintProto& ct : cp_model_proto.constraints()) {
    if (ct.constraint_case() == ConstraintProto::kNoOverlap) return true;
    if (ct.constraint_case() == ConstraintProto::kCumulative) return true;
  }
  return false;
}

}  // namespace

const std::function<BooleanOrIntegerLiteral()> ConstructSearchStrategyInternal(
    const std::vector<DecisionStrategyProto>& strategies, Model* model) {
  const auto& view = *model->GetOrCreate<CpModelView>();
  const auto& parameters = *model->GetOrCreate<SatParameters>();
  auto* random = model->GetOrCreate<ModelRandomGenerator>();

  // Note that we copy strategies to keep the return function validity
  // independently of the life of the passed vector.
  return [&view, &parameters, random, strategies]() {
    for (const DecisionStrategyProto& strategy : strategies) {
      int candidate;
      int64_t candidate_value = std::numeric_limits<int64_t>::max();

      // TODO(user): Improve the complexity if this becomes an issue which
      // may be the case if we do a fixed_search.

      // To store equivalent variables in randomized search.
      std::vector<VarValue> active_refs;

      int t_index = 0;  // Index in strategy.transformations().
      for (int i = 0; i < strategy.variables().size(); ++i) {
        const int ref = strategy.variables(i);
        const int var = PositiveRef(ref);
        if (view.IsFixed(var) || view.IsCurrentlyFree(var)) continue;

        int64_t coeff(1);
        int64_t offset(0);
        while (t_index < strategy.transformations().size() &&
               strategy.transformations(t_index).index() < i) {
          ++t_index;
        }
        if (t_index < strategy.transformations_size() &&
            strategy.transformations(t_index).index() == i) {
          coeff = strategy.transformations(t_index).positive_coeff();
          offset = strategy.transformations(t_index).offset();
        }

        // TODO(user): deal with integer overflow in case of wrongly specified
        // coeff? Note that if this is filled by the presolve it shouldn't
        // happen since any feasible value in the new variable domain should be
        // a feasible value of the original variable domain.
        int64_t value(0);
        int64_t lb = view.Min(var);
        int64_t ub = view.Max(var);
        if (!RefIsPositive(ref)) {
          lb = -view.Max(var);
          ub = -view.Min(var);
        }
        switch (strategy.variable_selection_strategy()) {
          case DecisionStrategyProto::CHOOSE_FIRST:
            break;
          case DecisionStrategyProto::CHOOSE_LOWEST_MIN:
            value = coeff * lb + offset;
            break;
          case DecisionStrategyProto::CHOOSE_HIGHEST_MAX:
            value = -(coeff * ub + offset);
            break;
          case DecisionStrategyProto::CHOOSE_MIN_DOMAIN_SIZE:
            value = coeff * (ub - lb + 1);
            break;
          case DecisionStrategyProto::CHOOSE_MAX_DOMAIN_SIZE:
            value = -coeff * (ub - lb + 1);
            break;
          default:
            LOG(FATAL) << "Unknown VariableSelectionStrategy "
                       << strategy.variable_selection_strategy();
        }
        if (value < candidate_value) {
          candidate = ref;
          candidate_value = value;
        }
        if (strategy.variable_selection_strategy() ==
                DecisionStrategyProto::CHOOSE_FIRST &&
            !parameters.randomize_search()) {
          break;
        } else if (parameters.randomize_search()) {
          if (value <=
              candidate_value + parameters.search_randomization_tolerance()) {
            active_refs.push_back({ref, value});
          }
        }
      }

      if (candidate_value == std::numeric_limits<int64_t>::max()) continue;
      if (parameters.randomize_search()) {
        CHECK(!active_refs.empty());
        const IntegerValue threshold(
            candidate_value + parameters.search_randomization_tolerance());
        auto is_above_tolerance = [threshold](const VarValue& entry) {
          return entry.value > threshold;
        };
        // Remove all values above tolerance.
        active_refs.erase(std::remove_if(active_refs.begin(), active_refs.end(),
                                         is_above_tolerance),
                          active_refs.end());
        const int winner = absl::Uniform<int>(*random, 0, active_refs.size());
        candidate = active_refs[winner].ref;
      }

      DecisionStrategyProto::DomainReductionStrategy selection =
          strategy.domain_reduction_strategy();
      if (!RefIsPositive(candidate)) {
        switch (selection) {
          case DecisionStrategyProto::SELECT_MIN_VALUE:
            selection = DecisionStrategyProto::SELECT_MAX_VALUE;
            break;
          case DecisionStrategyProto::SELECT_MAX_VALUE:
            selection = DecisionStrategyProto::SELECT_MIN_VALUE;
            break;
          case DecisionStrategyProto::SELECT_LOWER_HALF:
            selection = DecisionStrategyProto::SELECT_UPPER_HALF;
            break;
          case DecisionStrategyProto::SELECT_UPPER_HALF:
            selection = DecisionStrategyProto::SELECT_LOWER_HALF;
            break;
          default:
            break;
        }
      }

      const int var = PositiveRef(candidate);
      const int64_t lb = view.Min(var);
      const int64_t ub = view.Max(var);
      switch (selection) {
        case DecisionStrategyProto::SELECT_MIN_VALUE:
          return view.LowerOrEqual(var, lb);
        case DecisionStrategyProto::SELECT_MAX_VALUE:
          return view.GreaterOrEqual(var, ub);
        case DecisionStrategyProto::SELECT_LOWER_HALF:
          return view.LowerOrEqual(var, lb + (ub - lb) / 2);
        case DecisionStrategyProto::SELECT_UPPER_HALF:
          return view.GreaterOrEqual(var, ub - (ub - lb) / 2);
        case DecisionStrategyProto::SELECT_MEDIAN_VALUE:
          return view.MedianValue(var);
        default:
          LOG(FATAL) << "Unknown DomainReductionStrategy "
                     << strategy.domain_reduction_strategy();
      }
    }
    return BooleanOrIntegerLiteral();
  };
}

std::function<BooleanOrIntegerLiteral()> ConstructSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    IntegerVariable objective_var, Model* model) {
  std::vector<std::function<BooleanOrIntegerLiteral()>> heuristics;

  // We start by the user specified heuristic.
  {
    std::vector<DecisionStrategyProto> strategies;
    for (const DecisionStrategyProto& proto :
         cp_model_proto.search_strategy()) {
      strategies.push_back(proto);
    }
    heuristics.push_back(ConstructSearchStrategyInternal(strategies, model));
  }

  // If there are some scheduling constraint, we complete with a custom
  // "scheduling" strategy.
  if (ModelHasSchedulingConstraints(cp_model_proto)) {
    heuristics.push_back(SchedulingSearchHeuristic(model));
  }

  // If needed, we finish by instantiating anything left.
  if (model->GetOrCreate<SatParameters>()->instantiate_all_variables()) {
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
    heuristics.push_back(FirstUnassignedVarAtItsMinHeuristic(decisions, model));
  }

  return SequentialSearch(heuristics);
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
    new_params.set_optimize_with_lb_tree_search(true);
    new_params.set_linearization_level(2);
    strategies["lb_tree_search"] = new_params;
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

  // We only use a "fixed search" worker if some strategy is specified or
  // if we have a scheduling model.
  //
  // TODO(user): For scheduling, this is important to find good first solution
  // but afterwards it is not really great and should probably be replaced by a
  // LNS worker.
  const bool use_fixed_strategy = !cp_model.search_strategy().empty() ||
                                  ModelHasSchedulingConstraints(cp_model);

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
      names.push_back(use_fixed_strategy ? "fixed" : "pseudo_costs");
      names.push_back(cp_model.objective().vars_size() > 1 ? "core" : "no_lp");
      names.push_back("max_lp");
    } else {
      names.push_back("default_lp");
      names.push_back(use_fixed_strategy ? "fixed" : "no_lp");
      names.push_back("less_encoding");
      names.push_back("max_lp");
      names.push_back("quick_restart");
    }
  } else if (cp_model.has_objective()) {
    names.push_back("default_lp");
    if (use_fixed_strategy) {
      names.push_back("fixed");
      if (num_workers > 8) names.push_back("reduced_costs");
    } else {
      names.push_back("reduced_costs");
    }
    names.push_back("pseudo_costs");
    names.push_back("no_lp");
    names.push_back("max_lp");

    // TODO(user): Experiment with core and LP.
    if (cp_model.objective().vars_size() > 1) names.push_back("core");

    // Only add this strategy if we have enough worker left for LNS.
    if (num_workers > 8 || base_params.interleave_search()) {
      names.push_back("quick_restart");
    }
    if (num_workers > 10) {
      names.push_back("quick_restart_no_lp");
    }
  } else {
    names.push_back("default_lp");
    if (use_fixed_strategy) names.push_back("fixed");
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
    names.push_back("lb_tree_search");
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
