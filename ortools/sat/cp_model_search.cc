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

#include "ortools/sat/cp_model_search.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_propagation.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

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
    const std::vector<ValueLiteralPair> encoding =
        integer_encoder_.FullDomainEncoding(variable);

    // 5 values -> returns the second.
    // 4 values -> returns the second too.
    // Array is 0 based.
    const int target = (static_cast<int>(encoding.size()) + 1) / 2 - 1;
    result.boolean_literal_index = encoding[target].literal.Index();
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

void AddDualSchedulingHeuristics(SatParameters& new_params) {
  new_params.set_exploit_all_precedences(true);
  new_params.set_use_hard_precedences_in_cumulative(true);
  new_params.set_use_overload_checker_in_cumulative(true);
  new_params.set_use_strong_propagation_in_disjunctive(true);
  new_params.set_use_timetable_edge_finding_in_cumulative(true);
  new_params.set_max_pairs_pairwise_reasoning_in_no_overlap_2d(5000);
  new_params.set_use_timetabling_in_no_overlap_2d(true);
  new_params.set_use_energetic_reasoning_in_no_overlap_2d(true);
  new_params.set_use_area_energetic_reasoning_in_no_overlap_2d(true);
  new_params.set_use_conservative_scale_overload_checker(true);
}

// We want a random tie breaking among variables with equivalent values.
struct NoisyInteger {
  int64_t value;
  double noise;

  bool operator<=(const NoisyInteger& other) const {
    return value < other.value ||
           (value == other.value && noise <= other.noise);
  }
  bool operator>(const NoisyInteger& other) const {
    return value > other.value || (value == other.value && noise > other.noise);
  }
};

}  // namespace

std::function<BooleanOrIntegerLiteral()> ConstructUserSearchStrategy(
    const CpModelProto& cp_model_proto, Model* model) {
  if (cp_model_proto.search_strategy().empty()) return nullptr;

  std::vector<DecisionStrategyProto> strategies;
  for (const DecisionStrategyProto& proto : cp_model_proto.search_strategy()) {
    strategies.push_back(proto);
  }
  const auto& view = *model->GetOrCreate<CpModelView>();
  const auto& parameters = *model->GetOrCreate<SatParameters>();
  auto* random = model->GetOrCreate<ModelRandomGenerator>();

  // Note that we copy strategies to keep the return function validity
  // independently of the life of the passed vector.
  return [&view, &parameters, random, strategies]() {
    for (const DecisionStrategyProto& strategy : strategies) {
      int candidate_ref = -1;
      int64_t candidate_value = std::numeric_limits<int64_t>::max();

      // TODO(user): Improve the complexity if this becomes an issue which
      // may be the case if we do a fixed_search.

      // To store equivalent variables in randomized search.
      const bool randomize_decision =
          parameters.search_random_variable_pool_size() > 1;
      TopN<int, NoisyInteger> top_variables(
          randomize_decision ? parameters.search_random_variable_pool_size()
                             : 1);

      for (const LinearExpressionProto& expr : strategy.exprs()) {
        const int var = expr.vars(0);
        if (view.IsFixed(var)) continue;

        int64_t coeff = expr.coeffs(0);
        int64_t offset = expr.offset();
        int64_t lb = view.Min(var);
        int64_t ub = view.Max(var);
        int ref = var;
        if (coeff < 0) {
          lb = -view.Max(var);
          ub = -view.Min(var);
          coeff = -coeff;
          ref = NegatedRef(var);
        }

        int64_t value(0);
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
            // The size of the domain is not multiplied by the coeff.
            value = ub - lb + 1;
            break;
          case DecisionStrategyProto::CHOOSE_MAX_DOMAIN_SIZE:
            // The size of the domain is not multiplied by the coeff.
            value = -(ub - lb + 1);
            break;
          default:
            LOG(FATAL) << "Unknown VariableSelectionStrategy "
                       << strategy.variable_selection_strategy();
        }

        if (randomize_decision) {
          // We need to use -value as we want the minimum valued variables.
          // We add a random noise to get improve the entropy.
          const double noise = absl::Uniform(*random, 0., 1.0);
          top_variables.Add(ref, {-value, noise});
          candidate_value = std::min(candidate_value, value);
        } else if (value < candidate_value) {
          candidate_ref = ref;
          candidate_value = value;
        }

        // We can stop scanning if the variable selection strategy is to use the
        // first unbound variable and no randomization is needed.
        if (strategy.variable_selection_strategy() ==
                DecisionStrategyProto::CHOOSE_FIRST &&
            !randomize_decision) {
          break;
        }
      }

      // Check if one active variable has been found.
      if (candidate_value == std::numeric_limits<int64_t>::max()) continue;

      // Pick the winner when decisions are randomized.
      if (randomize_decision) {
        const std::vector<int>& candidates = top_variables.UnorderedElements();
        candidate_ref = candidates[absl::Uniform(
            *random, 0, static_cast<int>(candidates.size()))];
      }

      DecisionStrategyProto::DomainReductionStrategy selection =
          strategy.domain_reduction_strategy();
      if (!RefIsPositive(candidate_ref)) {
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

      const int var = PositiveRef(candidate_ref);
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

// TODO(user): Implement a routing search.
std::function<BooleanOrIntegerLiteral()> ConstructHeuristicSearchStrategy(
    const CpModelProto& cp_model_proto, Model* model) {
  if (ModelHasSchedulingConstraints(cp_model_proto)) {
    std::vector<std::function<BooleanOrIntegerLiteral()>> heuristics;
    const auto& params = *model->GetOrCreate<SatParameters>();
    bool possible_new_constraints = false;
    if (params.use_dynamic_precedence_in_disjunctive()) {
      possible_new_constraints = true;
      heuristics.push_back(DisjunctivePrecedenceSearchHeuristic(model));
    }
    if (params.use_dynamic_precedence_in_cumulative()) {
      possible_new_constraints = true;
      heuristics.push_back(CumulativePrecedenceSearchHeuristic(model));
    }

    // Tricky: we need to create this at level zero in case there are no linear
    // constraint in the model at the beginning.
    //
    // TODO(user): Alternatively, support creation of SatPropagator at positive
    // level.
    if (possible_new_constraints && params.new_linear_propagation()) {
      model->GetOrCreate<LinearPropagator>();
    }

    heuristics.push_back(SchedulingSearchHeuristic(model));
    return SequentialSearch(std::move(heuristics));
  }
  return PseudoCost(model);
}

std::function<BooleanOrIntegerLiteral()>
ConstructIntegerCompletionSearchStrategy(
    const std::vector<IntegerVariable>& variable_mapping,
    IntegerVariable objective_var, Model* model) {
  const auto& params = *model->GetOrCreate<SatParameters>();
  if (!params.instantiate_all_variables()) {
    return []() { return BooleanOrIntegerLiteral(); };
  }

  std::vector<IntegerVariable> decisions;
  for (const IntegerVariable var : variable_mapping) {
    if (var == kNoIntegerVariable) continue;

    // Make sure we try to fix the objective to its lowest value first.
    // TODO(user): we could also fix terms of the objective in the right
    // direction.
    if (var == NegationOf(objective_var)) {
      decisions.push_back(objective_var);
    } else {
      decisions.push_back(var);
    }
  }
  return FirstUnassignedVarAtItsMinHeuristic(decisions, model);
}

// Constructs a search strategy that follow the hint from the model.
std::function<BooleanOrIntegerLiteral()> ConstructHintSearchStrategy(
    const CpModelProto& cp_model_proto, CpModelMapping* mapping, Model* model) {
  std::vector<BooleanOrIntegerVariable> vars;
  std::vector<IntegerValue> values;
  for (int i = 0; i < cp_model_proto.solution_hint().vars_size(); ++i) {
    const int ref = cp_model_proto.solution_hint().vars(i);
    CHECK(RefIsPositive(ref));
    BooleanOrIntegerVariable var;
    if (mapping->IsBoolean(ref)) {
      var.bool_var = mapping->Literal(ref).Variable();
    } else {
      var.int_var = mapping->Integer(ref);
    }
    vars.push_back(var);
    values.push_back(IntegerValue(cp_model_proto.solution_hint().values(i)));
  }
  return FollowHint(vars, values, model);
}

std::function<BooleanOrIntegerLiteral()> ConstructFixedSearchStrategy(
    std::function<BooleanOrIntegerLiteral()> user_search,
    std::function<BooleanOrIntegerLiteral()> heuristic_search,
    std::function<BooleanOrIntegerLiteral()> integer_completion) {
  // We start by the user specified heuristic.
  std::vector<std::function<BooleanOrIntegerLiteral()>> heuristics;
  if (user_search != nullptr) {
    heuristics.push_back(user_search);
  }
  if (heuristic_search != nullptr) {
    heuristics.push_back(heuristic_search);
  }
  if (integer_completion != nullptr) {
    heuristics.push_back(integer_completion);
  }

  return SequentialSearch(heuristics);
}

std::function<BooleanOrIntegerLiteral()> InstrumentSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    std::function<BooleanOrIntegerLiteral()> instrumented_strategy,
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
  return [instrumented_strategy, model, &variable_mapping, &cp_model_proto,
          old_domains, ref_to_display]() mutable {
    const BooleanOrIntegerLiteral decision = instrumented_strategy();
    if (!decision.HasValue()) return decision;

    if (decision.boolean_literal_index != kNoLiteralIndex) {
      const Literal l = Literal(decision.boolean_literal_index);
      LOG(INFO) << "Boolean decision " << l;
      const auto& encoder = model->Get<IntegerEncoder>();
      for (const IntegerLiteral i_lit : encoder->GetIntegerLiterals(l)) {
        LOG(INFO) << " - associated with " << i_lit;
      }
      for (const auto [var, value] : encoder->GetEqualityLiterals(l)) {
        LOG(INFO) << " - associated with " << var << " == " << value;
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

absl::flat_hash_map<std::string, SatParameters> GetNamedParameters(
    SatParameters base_params) {
  absl::flat_hash_map<std::string, SatParameters> strategies;

  // By default we disable the logging when we generate a set of parameter. It
  // is possible to force it by setting it in the corresponding named parameter
  // via the subsolver_params field.
  base_params.set_log_search_progress(false);

  // The "default" name can be used for the base_params unchanged.
  strategies["default"] = base_params;

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
    new_params.set_use_symmetry_in_lp(true);
    strategies["max_lp_sym"] = new_params;
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
    new_params.set_optimize_with_core(true);
    new_params.set_optimize_with_max_hs(true);
    strategies["max_hs"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_optimize_with_lb_tree_search(true);
    // We do not want to change the objective_var lb from outside as it gives
    // better result to only use locally derived reason in that algo.
    new_params.set_share_objective_bounds(false);

    new_params.set_linearization_level(0);
    strategies["lb_tree_search_no_lp"] = new_params;

    new_params.set_linearization_level(2);
    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }
    // We want to spend more time on the LP here.
    new_params.set_add_lp_constraints_lazily(false);
    new_params.set_root_lp_iterations(100'000);
    strategies["lb_tree_search"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_use_objective_lb_search(true);

    new_params.set_linearization_level(0);
    strategies["objective_lb_search_no_lp"] = new_params;

    new_params.set_linearization_level(1);
    strategies["objective_lb_search"] = new_params;

    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }
    new_params.set_linearization_level(2);
    strategies["objective_lb_search_max_lp"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_use_objective_shaving_search(true);
    new_params.set_cp_model_presolve(true);
    new_params.set_cp_model_probing_level(0);
    new_params.set_symmetry_level(0);
    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }

    strategies["objective_shaving"] = new_params;

    new_params.set_linearization_level(0);
    strategies["objective_shaving_no_lp"] = new_params;

    new_params.set_linearization_level(2);
    strategies["objective_shaving_max_lp"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_use_variables_shaving_search(true);
    new_params.set_cp_model_presolve(true);
    new_params.set_cp_model_probing_level(0);
    new_params.set_symmetry_level(0);
    new_params.set_share_objective_bounds(false);
    new_params.set_share_level_zero_bounds(false);

    strategies["variables_shaving"] = new_params;

    new_params.set_linearization_level(0);
    strategies["variables_shaving_no_lp"] = new_params;

    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }
    new_params.set_linearization_level(2);
    strategies["variables_shaving_max_lp"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    new_params.set_use_probing_search(true);
    new_params.set_at_most_one_max_expansion_size(2);
    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }
    strategies["probing"] = new_params;

    new_params.set_linearization_level(0);
    strategies["probing_no_lp"] = new_params;

    new_params.set_linearization_level(2);
    // We want to spend more time on the LP here.
    new_params.set_add_lp_constraints_lazily(false);
    new_params.set_root_lp_iterations(100'000);
    strategies["probing_max_lp"] = new_params;
  }

  // Search variation.
  {
    SatParameters new_params = base_params;
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    strategies["auto"] = new_params;

    new_params.set_search_branching(SatParameters::FIXED_SEARCH);
    new_params.set_use_dynamic_precedence_in_disjunctive(false);
    new_params.set_use_dynamic_precedence_in_cumulative(false);
    strategies["fixed"] = new_params;
  }

  // Quick restart.
  {
    // TODO(user): Experiment with search_random_variable_pool_size.
    SatParameters new_params = base_params;
    new_params.set_search_branching(
        SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH);
    strategies["quick_restart"] = new_params;

    new_params.set_linearization_level(0);
    strategies["quick_restart_no_lp"] = new_params;

    new_params.set_linearization_level(2);
    strategies["quick_restart_max_lp"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_linearization_level(2);
    new_params.set_search_branching(SatParameters::LP_SEARCH);
    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }
    strategies["reduced_costs"] = new_params;
  }

  {
    // Note: no dual scheduling heuristics.
    SatParameters new_params = base_params;
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

  // Base parameters for shared tree worker.
  {
    SatParameters new_params = base_params;
    new_params.set_use_shared_tree_search(true);
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);

    // These settings don't make sense with shared tree search, turn them off as
    // they can break things.
    new_params.set_optimize_with_core(false);
    new_params.set_optimize_with_lb_tree_search(false);
    new_params.set_optimize_with_max_hs(false);

    strategies["shared_tree"] = new_params;
  }

  // Base parameters for LNS worker.
  {
    SatParameters new_params = base_params;
    new_params.set_stop_after_first_solution(false);
    new_params.set_cp_model_presolve(true);

    // We disable costly presolve/inprocessing.
    new_params.set_use_sat_inprocessing(false);
    new_params.set_cp_model_probing_level(0);
    new_params.set_symmetry_level(0);
    new_params.set_find_big_linear_overlap(false);

    new_params.set_log_search_progress(false);
    new_params.set_debug_crash_on_bad_hint(false);  // Can happen in lns.
    new_params.set_solution_pool_size(1);  // Keep the best solution found.
    strategies["lns"] = new_params;
  }

  // Add user defined ones.
  // Note that this might be merged to our default ones.
  for (const SatParameters& params : base_params.subsolver_params()) {
    auto it = strategies.find(params.name());
    if (it != strategies.end()) {
      it->second.MergeFrom(params);
    } else {
      // Merge the named parameters with the base parameters to create the new
      // parameters.
      SatParameters new_params = base_params;
      new_params.MergeFrom(params);
      strategies[params.name()] = new_params;
    }
  }

  // Fix names (we don't set them above).
  for (auto& [name, params] : strategies) {
    params.set_name(name);
  }

  return strategies;
}

// Note: in flatzinc setting, we know we always have a fixed search defined.
//
// Things to try:
//   - Specialize for purely boolean problems
//   - Disable linearization_level options for non linear problems
//   - Fast restart in randomized search
//   - Different propatation levels for scheduling constraints
std::vector<SatParameters> GetFullWorkerParameters(
    const SatParameters& base_params, const CpModelProto& cp_model,
    int num_already_present, SubsolverNameFilter* filter) {
  // Defines a set of named strategies so it is easier to read in one place
  // the one that are used. See below.
  const auto strategies = GetNamedParameters(base_params);

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
  // TODO(user): Avoid launching two strategies if they are the same,
  // like if there is no lp, or everything is already linearized at level 1.
  std::vector<std::string> names;

  // Starts by adding user specified ones.
  for (const std::string& name : base_params.extra_subsolvers()) {
    names.push_back(name);
  }

  // We use the default if empty.
  if (base_params.subsolvers().empty()) {
    // Note that the order is important as the list can be truncated.
    names.push_back("default_lp");
    names.push_back("fixed");
    names.push_back("core");
    names.push_back("no_lp");
    if (cp_model.has_symmetry()) {
      names.push_back("max_lp_sym");
    } else {
      // If there is no symmetry, max_lp_sym and max_lp are the same, but
      // we prefer the less confusing name.
      names.push_back("max_lp");
    }
    names.push_back("quick_restart");
    names.push_back("reduced_costs");
    names.push_back("quick_restart_no_lp");
    names.push_back("pseudo_costs");
    names.push_back("lb_tree_search");
    names.push_back("probing");
    names.push_back("objective_lb_search");
    names.push_back("objective_shaving_no_lp");
    names.push_back("objective_shaving_max_lp");
    names.push_back("probing_max_lp");
    names.push_back("probing_no_lp");
    names.push_back("objective_lb_search_no_lp");
    names.push_back("objective_lb_search_max_lp");
    if (cp_model.has_symmetry()) {
      names.push_back("max_lp");
    }
  } else {
    for (const std::string& name : base_params.subsolvers()) {
      // Hack for flatzinc. At the time of parameter setting, the objective is
      // not expanded. So we do not know if core is applicable or not.
      if (name == "core_or_no_lp") {
        if (!cp_model.has_objective() ||
            cp_model.objective().vars_size() <= 1) {
          names.push_back("no_lp");
        } else {
          names.push_back("core");
        }
      } else {
        names.push_back(name);
      }
    }
  }

  // Remove the names that should be ignored.
  int new_size = 0;
  for (const std::string& name : names) {
    if (filter->Keep(name)) {
      names[new_size++] = name;
    }
  }
  names.resize(new_size);

  // Creates the diverse set of parameters with names and seed.
  std::vector<SatParameters> result;
  for (const std::string& name : names) {
    SatParameters params = strategies.at(name);

    // Do some filtering.
    if (!use_fixed_strategy &&
        params.search_branching() == SatParameters::FIXED_SEARCH) {
      continue;
    }

    // TODO(user): Enable probing_search in deterministic mode.
    // Currently it timeouts on small problems as the deterministic time limit
    // never hits the sharding limit.
    if (params.use_probing_search() && params.interleave_search()) continue;

    // TODO(user): Enable shaving search in interleave mode.
    // Currently it do not respect ^C, and has no per chunk time limit.
    if ((params.use_objective_shaving_search() ||
         params.use_variables_shaving_search()) &&
        params.interleave_search()) {
      continue;
    }

    // In the corner case of empty variable, lets not schedule the probing as
    // it currently just loop forever instead of returning right away.
    if (params.use_probing_search() && cp_model.variables().empty()) continue;

    if (cp_model.has_objective() && !cp_model.objective().vars().empty()) {
      // Disable core search if there is only 1 term in the objective.
      if (cp_model.objective().vars().size() == 1 &&
          params.optimize_with_core()) {
        continue;
      }

      if (name == "less_encoding") continue;

      // Disable subsolvers that do not implement the deterministic mode.
      //
      // TODO(user): Enable lb_tree_search in deterministic mode.
      if (params.interleave_search() &&
          (params.optimize_with_lb_tree_search() ||
           params.use_objective_lb_search())) {
        continue;
      }
    } else {
      // Remove subsolvers that require an objective.
      if (params.optimize_with_lb_tree_search()) continue;
      if (params.optimize_with_core()) continue;
      if (params.use_objective_lb_search()) continue;
      if (params.use_objective_shaving_search()) continue;
      if (params.search_branching() == SatParameters::LP_SEARCH) continue;
      if (params.search_branching() == SatParameters::PSEUDO_COST_SEARCH) {
        continue;
      }
    }

    // Add this strategy.
    params.set_name(name);
    params.set_random_seed(CombineSeed(
        base_params.random_seed(), static_cast<int64_t>(result.size()) + 1));
    result.push_back(params);
  }

  // In interleaved mode, we run all of them.
  //
  // TODO(user): Actually make sure the gap num_workers <-> num_heuristics is
  // contained.
  if (base_params.interleave_search()) return result;

  // Apply the logic for how many we keep.
  int num_to_keep = base_params.num_full_subsolvers();
  if (num_to_keep == 0) {
    // Derive some automatic number to leave room for LS/LNS and other
    // strategies not taken into account here.
    const int num_available =
        std::max(0, base_params.num_workers() - num_already_present);

    const auto heuristic_num_workers = [](int num_workers) {
      DCHECK_GE(num_workers, 0);
      if (num_workers == 1) return 1;
      if (num_workers <= 4) return num_workers - 1;
      if (num_workers <= 8) return num_workers - 2;
      if (num_workers <= 16) return num_workers - (num_workers / 4 + 1);
      return num_workers - (num_workers / 2 - 3);
    };

    num_to_keep = heuristic_num_workers(num_available);
  }

  if (result.size() > num_to_keep) {
    result.resize(std::max(0, num_to_keep));
  }
  return result;
}

std::vector<SatParameters> GetFirstSolutionBaseParams(
    const SatParameters& base_params) {
  std::vector<SatParameters> result;

  const auto get_base = [&result, &base_params](bool fj) {
    SatParameters new_params = base_params;
    new_params.set_log_search_progress(false);
    new_params.set_use_feasibility_jump(fj);

    const int base_seed = base_params.random_seed();
    new_params.set_random_seed(CombineSeed(base_seed, result.size()));
    return new_params;
  };

  // Add one feasibility jump.
  if (base_params.use_feasibility_jump()) {
    SatParameters new_params = get_base(true);
    new_params.set_name("fj");
    new_params.set_feasibility_jump_linearization_level(0);
    result.push_back(new_params);
  }

  // Random search.
  for (int i = 0; i < 2; ++i) {
    SatParameters new_params = get_base(false);
    new_params.set_search_random_variable_pool_size(5);
    new_params.set_search_branching(SatParameters::RANDOMIZED_SEARCH);
    if (i % 2 == 0) {
      new_params.set_name("fs_random_no_lp");
      new_params.set_linearization_level(0);
    } else {
      new_params.set_name("fs_random");
    }
    result.push_back(new_params);
  }

  // Add a second feasibility jump.
  if (base_params.use_feasibility_jump()) {
    SatParameters new_params = get_base(true);
    new_params.set_name("fj");
    new_params.set_feasibility_jump_linearization_level(0);
    result.push_back(new_params);
  }

  // Random quick restart.
  for (int i = 0; i < 2; ++i) {
    SatParameters new_params = get_base(false);
    new_params.set_search_random_variable_pool_size(5);
    new_params.set_search_branching(
        SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH);
    if (i % 2 == 0) {
      new_params.set_name("fs_random_quick_restart_no_lp");
      new_params.set_linearization_level(0);
    } else {
      new_params.set_name("fs_random_quick_restart");
    }
    result.push_back(new_params);
  }

  // Add a linear feasibility jump.
  // This one seems to perform worse, so we add only 1 for 2 normal LS, and we
  // add this late.
  if (base_params.use_feasibility_jump()) {
    SatParameters new_params = get_base(true);
    new_params.set_name("fj_lin");
    new_params.set_feasibility_jump_linearization_level(2);
    result.push_back(new_params);
  }

  return result;
}

std::vector<SatParameters> RepeatParameters(
    absl::Span<const SatParameters> base_params, int num_params_to_generate) {
  // Return if we are done.
  std::vector<SatParameters> result;
  result.assign(base_params.begin(), base_params.end());
  if (result.empty()) return result;
  if (result.size() >= num_params_to_generate) {
    result.resize(num_params_to_generate);
    return result;
  }

  // Repeat parameters until we have enough.
  int i = 0;
  const int base_size = result.size();
  while (result.size() < num_params_to_generate) {
    result.push_back(result[i % base_size]);
    result.back().set_random_seed(CombineSeed(result.back().random_seed(), i));
    ++i;
  }
  return result;
}

SubsolverNameFilter::SubsolverNameFilter(const SatParameters& params) {
  for (const auto& pattern : params.filter_subsolvers()) {
    filter_patterns_.push_back(pattern);
  }
  for (const auto& pattern : params.ignore_subsolvers()) {
    ignore_patterns_.push_back(pattern);
  }

  // Hack for backward compatibility and easy of use.
  if (params.use_ls_only()) {
    filter_patterns_.push_back("ls*");
    filter_patterns_.push_back("fj*");
  }

  if (params.use_lns_only()) {
    // Still add first solution solvers.
    filter_patterns_.push_back("fj*");
    filter_patterns_.push_back("fs*");
    filter_patterns_.push_back("*lns");
  }
}

bool SubsolverNameFilter::Keep(absl::string_view name) {
  last_name_ = name;
  if (!filter_patterns_.empty()) {
    bool keep = false;
    for (const absl::string_view pattern : filter_patterns_) {
      if (FNMatch(pattern, name)) {
        keep = true;
        break;
      }
    }
    if (!keep) {
      ignored_.emplace_back(name);
      return false;
    }
  }
  for (const absl::string_view pattern : ignore_patterns_) {
    if (FNMatch(pattern, name)) {
      ignored_.emplace_back(name);
      return false;
    }
  }
  return true;
}

bool SubsolverNameFilter::FNMatch(absl::string_view pattern,
                                  absl::string_view str) {
  bool in_wildcard_match = false;
  while (true) {
    if (pattern.empty()) {
      return in_wildcard_match || str.empty();
    }
    if (str.empty()) {
      return pattern.find_first_not_of('*') == pattern.npos;
    }
    switch (pattern.front()) {
      case '*':
        pattern.remove_prefix(1);
        in_wildcard_match = true;
        break;
      case '?':
        pattern.remove_prefix(1);
        str.remove_prefix(1);
        break;
      default:
        if (in_wildcard_match) {
          absl::string_view fixed_portion = pattern;
          const size_t end = fixed_portion.find_first_of("*?");
          if (end != fixed_portion.npos) {
            fixed_portion = fixed_portion.substr(0, end);
          }
          const size_t match = str.find(fixed_portion);
          if (match == str.npos) {
            return false;
          }
          pattern.remove_prefix(fixed_portion.size());
          str.remove_prefix(match + fixed_portion.size());
          in_wildcard_match = false;
        } else {
          if (pattern.front() != str.front()) {
            return false;
          }
          pattern.remove_prefix(1);
          str.remove_prefix(1);
        }
        break;
    }
  }
}

}  // namespace sat
}  // namespace operations_research
