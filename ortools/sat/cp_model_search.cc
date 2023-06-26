// Copyright 2010-2022 Google LLC
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
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"

// TODO(user): remove this when the code is stable and does not use SCIP
// anymore.
ABSL_FLAG(bool, cp_model_use_max_hs, false, "Use max_hs in search portfolio.");

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
  new_params.set_use_pairwise_reasoning_in_no_overlap_2d(true);
  new_params.set_use_timetabling_in_no_overlap_2d(true);
  new_params.set_use_energetic_reasoning_in_no_overlap_2d(true);
}

}  // namespace

std::function<BooleanOrIntegerLiteral()> ConstructSearchStrategyInternal(
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
        const int winner =
            absl::Uniform(*random, 0, static_cast<int>(active_refs.size()));
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

std::function<BooleanOrIntegerLiteral()> ConstructUserSearchStrategy(
    const CpModelProto& cp_model_proto, Model* model) {
  std::vector<DecisionStrategyProto> strategies;
  for (const DecisionStrategyProto& proto : cp_model_proto.search_strategy()) {
    strategies.push_back(proto);
  }
  return ConstructSearchStrategyInternal(strategies, model);
}

std::function<BooleanOrIntegerLiteral()> ConstructFixedSearchStrategy(
    const CpModelProto& cp_model_proto,
    const std::vector<IntegerVariable>& variable_mapping,
    IntegerVariable objective_var, Model* model) {
  std::vector<std::function<BooleanOrIntegerLiteral()>> heuristics;

  // We start by the user specified heuristic.
  const auto& params = *model->GetOrCreate<SatParameters>();
  if (params.search_branching() != SatParameters::PARTIAL_FIXED_SEARCH) {
    heuristics.push_back(ConstructUserSearchStrategy(cp_model_proto, model));
  }

  // If there are some scheduling constraint, we complete with a custom
  // "scheduling" strategy.
  if (ModelHasSchedulingConstraints(cp_model_proto)) {
    heuristics.push_back(SchedulingSearchHeuristic(model));
  }

  // If needed, we finish by instantiating anything left.
  if (params.instantiate_all_variables()) {
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

// This generates a valid random seed (base_seed + delta) without overflow.
// We assume |delta| is small.
int ValidSumSeed(int base_seed, int delta) {
  CHECK_GE(delta, 0);
  int64_t result = int64_t{base_seed} + int64_t{delta};
  const int64_t int32max = int64_t{std::numeric_limits<int>::max()};
  while (result > int32max) {
    result -= int32max;
  }
  return static_cast<int>(result);
}

absl::flat_hash_map<std::string, SatParameters> GetNamedParameters(
    const SatParameters& base_params) {
  absl::flat_hash_map<std::string, SatParameters> strategies;

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
    new_params.set_linearization_level(2);
    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }

    // We want to spend more time on the LP here.
    new_params.set_add_lp_constraints_lazily(false);
    new_params.set_root_lp_iterations(100'000);

    // We do not want to change the objective_var lb from outside as it gives
    // better result to only use locally derived reason in that algo.
    new_params.set_share_objective_bounds(false);
    strategies["lb_tree_search"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_linearization_level(1);
    new_params.set_use_objective_lb_search(true);
    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }
    strategies["objective_lb_search"] = new_params;

    new_params.set_linearization_level(0);
    strategies["objective_lb_search_no_lp"] = new_params;

    new_params.set_linearization_level(2);
    strategies["objective_lb_search_max_lp"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_linearization_level(2);
    new_params.set_use_objective_shaving_search(true);
    new_params.set_cp_model_presolve(true);
    new_params.set_cp_model_probing_level(0);
    new_params.set_symmetry_level(0);
    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }
    strategies["objective_shaving_search"] = new_params;
  }

  {
    SatParameters new_params = base_params;
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    new_params.set_use_probing_search(true);
    if (base_params.use_dual_scheduling_heuristics()) {
      AddDualSchedulingHeuristics(new_params);
    }
    strategies["probing"] = new_params;

    new_params.set_linearization_level(0);
    strategies["probing_no_lp"] = new_params;

    new_params.set_linearization_level(2);
    strategies["probing_max_lp"] = new_params;
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

    new_params.set_search_branching(
        SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH);
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

  // Add user defined ones.
  // Note that this might overwrite our default ones.
  for (const SatParameters& params : base_params.subsolver_params()) {
    strategies[params.name()] = params;
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
std::vector<SatParameters> GetDiverseSetOfParameters(
    const SatParameters& base_params, const CpModelProto& cp_model) {
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

  const int num_workers_to_generate =
      base_params.num_workers() - base_params.shared_tree_num_workers();
  // We use the default if empty.
  if (base_params.subsolvers().empty()) {
    names.push_back("default_lp");
    names.push_back("fixed");
    names.push_back("less_encoding");

    names.push_back("no_lp");
    names.push_back("max_lp");
    names.push_back("core");

    names.push_back("reduced_costs");
    names.push_back("pseudo_costs");

    names.push_back("quick_restart");
    names.push_back("quick_restart_no_lp");
    names.push_back("lb_tree_search");
    // Do not add objective_lb_search if core is active and num_workers <= 16.
    if (cp_model.has_objective() &&
        (cp_model.objective().vars().size() == 1 ||  // core is not active
         num_workers_to_generate > 16)) {
      names.push_back("objective_lb_search");
    }
    names.push_back("probing");
    if (num_workers_to_generate >= 20) {
      names.push_back("probing_max_lp");
    }
    if (num_workers_to_generate >= 24) {
      names.push_back("objective_shaving_search");
    }
#if !defined(__PORTABLE_PLATFORM__) && defined(USE_SCIP)
    if (absl::GetFlag(FLAGS_cp_model_use_max_hs)) names.push_back("max_hs");
#endif  // !defined(__PORTABLE_PLATFORM__) && defined(USE_SCIP)
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
  absl::flat_hash_set<std::string> to_ignore;
  for (const std::string& name : base_params.ignore_subsolvers()) {
    to_ignore.insert(name);
  }
  int new_size = 0;
  for (const std::string& name : names) {
    if (to_ignore.contains(name)) continue;
    names[new_size++] = name;
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

    // In the corner case of empty variable, lets not schedule the probing as
    // it currently just loop forever instead of returning right away.
    if (params.use_probing_search() && cp_model.variables().empty()) continue;

    if (cp_model.has_objective() && !cp_model.objective().vars().empty()) {
      // Disable core search if only 1 term in the objective.
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
    //
    // TODO(user): Find a better randomization for the seed so that changing
    // random_seed() has more impact?
    params.set_name(name);
    params.set_random_seed(ValidSumSeed(base_params.random_seed(),
                                        static_cast<int>(result.size()) + 1));
    result.push_back(params);
  }

  if (cp_model.has_objective() && !cp_model.objective().vars().empty()) {
    // If there is an objective, the extra workers will use LNS.
    // Make sure we have at least min_num_lns_workers() of them.
    const int target =
        std::max(base_params.shared_tree_num_workers() > 0 ? 0 : 1,
                 num_workers_to_generate - base_params.min_num_lns_workers());
    if (!base_params.interleave_search() && result.size() > target) {
      result.resize(target);
    }
  } else {
    // If strategies that do not require a full worker are present, leave a
    // few workers for them.
    const bool need_extra_workers =
        !base_params.interleave_search() &&
        (base_params.use_rins_lns() || base_params.use_feasibility_pump());
    int target = num_workers_to_generate;
    if (need_extra_workers && target > 4) {
      if (target <= 8) {
        target -= 1;
      } else if (target == 9) {
        target -= 2;
      } else {
        target -= 3;
      }
    }
    if (!base_params.interleave_search() && result.size() > target) {
      result.resize(target);
    }
  }
  return result;
}

std::vector<SatParameters> GetFirstSolutionParams(
    const SatParameters& base_params, const CpModelProto& cp_model,
    int num_params_to_generate) {
  std::vector<SatParameters> result;
  if (num_params_to_generate <= 0) return result;
  int num_random = 0;
  int num_random_qr = 0;
  while (result.size() < num_params_to_generate) {
    SatParameters new_params = base_params;
    const int base_seed = base_params.random_seed();
    if (num_random <= num_random_qr) {  // Random search.
      // Alternate between automatic search and fixed search (if defined).
      //
      // TODO(user): Maybe alternate between more search types.
      // TODO(user): Check the randomization tolerance.
      if (cp_model.search_strategy().empty() && num_random % 2 == 0) {
        new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
      } else {
        new_params.set_search_branching(SatParameters::FIXED_SEARCH);
      }
      new_params.set_randomize_search(true);
      new_params.set_search_randomization_tolerance(num_random + 1);
      new_params.set_random_seed(ValidSumSeed(base_seed, 2 * num_random + 1));
      new_params.set_name("random");
      num_random++;
    } else {  // Random quick restart.
      new_params.set_search_branching(
          SatParameters::PORTFOLIO_WITH_QUICK_RESTART_SEARCH);
      new_params.set_randomize_search(true);
      new_params.set_search_randomization_tolerance(num_random_qr + 1);
      new_params.set_random_seed(ValidSumSeed(base_seed, 2 * num_random_qr));
      new_params.set_name("random_quick_restart");
      num_random_qr++;
    }
    result.push_back(new_params);
  }
  return result;
}

std::vector<SatParameters> GetWorkSharingParams(
    const SatParameters& base_params, const CpModelProto& cp_model,
    int num_params_to_generate) {
  std::vector<SatParameters> result;
  // TODO(user): We could support assumptions, it's just not implemented.
  if (!cp_model.assumptions().empty()) return result;
  if (num_params_to_generate <= 0) return result;
  int num_workers = 0;
  while (result.size() < num_params_to_generate) {
    // TODO(user): Make the base parameters configurable.
    SatParameters new_params = base_params;
    std::string name = "shared_";
    const int base_seed = base_params.random_seed();
    new_params.set_random_seed(ValidSumSeed(base_seed, 2 * num_workers + 1));
    new_params.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    new_params.set_use_shared_tree_search(true);

    // These settings don't make sense with shared tree search, turn them off as
    // they can break things.
    new_params.set_optimize_with_core(false);
    new_params.set_optimize_with_lb_tree_search(false);
    new_params.set_optimize_with_max_hs(false);

    std::string lp_tags[] = {"no", "default", "max"};
    absl::StrAppend(&name,
                    lp_tags[std::min(new_params.linearization_level(), 2)],
                    "_lp_", num_workers);
    new_params.set_name(name);
    num_workers++;
    result.push_back(new_params);
  }
  return result;
}
}  // namespace sat
}  // namespace operations_research
