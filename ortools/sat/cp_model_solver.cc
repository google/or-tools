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

#include "ortools/sat/cp_model_solver.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"

#if !defined(__PORTABLE_PLATFORM__)
#include "absl/synchronization/notification.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/file.h"
#endif  // __PORTABLE_PLATFORM__

#include "absl/container/flat_hash_set.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/base/status.h"
#include "ortools/base/threadpool.h"
#include "ortools/base/timer.h"
#include "ortools/graph/connectivity.h"
#include "ortools/port/proto_utils.h"
#include "ortools/sat/circuit.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_expand.h"
#include "ortools/sat/cp_model_lns.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_presolve.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/drat_checker.h"
#include "ortools/sat/drat_proof_handler.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/linear_programming_constraint.h"
#include "ortools/sat/linear_relaxation.h"
#include "ortools/sat/lns.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/probing.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/simplification.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/time_limit.h"

DEFINE_string(cp_model_dump_file, "",
              "DEBUG ONLY. When this is set to a non-empty file name, "
              "SolveCpModel() will dump its model to this file. Note that the "
              "file will be ovewritten with the last such model. "
              "TODO(fdid): dump all model to a recordio file instead?");
DEFINE_string(cp_model_params, "",
              "This is interpreted as a text SatParameters proto. The "
              "specified fields will override the normal ones for all solves.");
DEFINE_bool(cp_model_dump_lns_problems, false,
            "Useful to debug presolve issues on LNS fragments");

DEFINE_string(
    drat_output, "",
    "If non-empty, a proof in DRAT format will be written to this file. "
    "This will only be used for pure-SAT problems.");

DEFINE_bool(drat_check, false,
            "If true, a proof in DRAT format will be stored in memory and "
            "checked if the problem is UNSAT. This will only be used for "
            "pure-SAT problems.");

DEFINE_double(max_drat_time_in_seconds, std::numeric_limits<double>::infinity(),
              "Maximum time in seconds to check the DRAT proof. This will only "
              "be used is the drat_check flag is enabled.");

DEFINE_bool(cp_model_check_intermediate_solutions, false,
            "When true, all intermediate solutions found by the solver will be "
            "checked. This can be expensive, therefore it is off by default.");

namespace operations_research {
namespace sat {

namespace {

// =============================================================================
// A class that detects when variables should be fully encoded by computing a
// fixed point.
// =============================================================================

// This class is will ask the underlying Model to fully encode IntegerVariables
// of the model using constraint processors PropagateConstraintXXX(), until no
// such processor wants to fully encode a variable. The workflow is to call
// PropagateFullEncoding() on a set of constraints, then ComputeFixedPoint() to
// launch the fixed point computation.
class FullEncodingFixedPointComputer {
 public:
  explicit FullEncodingFixedPointComputer(Model* model)
      : model_(model),
        parameters_(*(model->GetOrCreate<SatParameters>())),
        mapping_(model->GetOrCreate<CpModelMapping>()),
        integer_encoder_(model->GetOrCreate<IntegerEncoder>()) {}

  // We only add to the propagation queue variable that are fully encoded.
  // Note that if a variable was already added once, we never add it again.
  void ComputeFixedPoint() {
    // Make sure all fully encoded variables of interest are in the queue.
    for (int v = 0; v < variable_watchers_.size(); v++) {
      if (!variable_watchers_[v].empty() && IsFullyEncoded(v)) {
        AddVariableToPropagationQueue(v);
      }
    }
    // Propagate until no additional variable can be fully encoded.
    while (!variables_to_propagate_.empty()) {
      const int variable = variables_to_propagate_.back();
      variables_to_propagate_.pop_back();
      for (const ConstraintProto* ct : variable_watchers_[variable]) {
        if (gtl::ContainsKey(constraint_is_finished_, ct)) continue;
        const bool finished = PropagateFullEncoding(ct);
        if (finished) constraint_is_finished_.insert(ct);
      }
    }
  }

  // Return true if the constraint is finished encoding what its wants.
  bool PropagateFullEncoding(const ConstraintProto* ct) {
    switch (ct->constraint_case()) {
      case ConstraintProto::ConstraintProto::kElement:
        return PropagateElement(ct);
      case ConstraintProto::ConstraintProto::kTable:
        return PropagateTable(ct);
      case ConstraintProto::ConstraintProto::kAutomaton:
        return PropagateAutomaton(ct);
      case ConstraintProto::ConstraintProto::kInverse:
        return PropagateInverse(ct);
      case ConstraintProto::ConstraintProto::kLinear:
        return PropagateLinear(ct);
      default:
        return true;
    }
  }

 private:
  // Constraint ct is interested by (full-encoding) state of variable.
  void Register(const ConstraintProto* ct, int variable) {
    variable = PositiveRef(variable);
    if (!gtl::ContainsKey(constraint_is_registered_, ct)) {
      constraint_is_registered_.insert(ct);
    }
    if (variable_watchers_.size() <= variable) {
      variable_watchers_.resize(variable + 1);
      variable_was_added_in_to_propagate_.resize(variable + 1);
    }
    variable_watchers_[variable].push_back(ct);
  }

  void AddVariableToPropagationQueue(int variable) {
    variable = PositiveRef(variable);
    if (variable_was_added_in_to_propagate_.size() <= variable) {
      variable_watchers_.resize(variable + 1);
      variable_was_added_in_to_propagate_.resize(variable + 1);
    }
    if (!variable_was_added_in_to_propagate_[variable]) {
      variable_was_added_in_to_propagate_[variable] = true;
      variables_to_propagate_.push_back(variable);
    }
  }

  // Note that we always consider a fixed variable to be fully encoded here.
  const bool IsFullyEncoded(int v) {
    const IntegerVariable variable = mapping_->Integer(v);
    return model_->Get(IsFixed(variable)) ||
           integer_encoder_->VariableIsFullyEncoded(variable);
  }

  void FullyEncode(int v) {
    v = PositiveRef(v);
    const IntegerVariable variable = mapping_->Integer(v);
    if (!model_->Get(IsFixed(variable))) {
      model_->Add(FullyEncodeVariable(variable));
    }
    AddVariableToPropagationQueue(v);
  }

  bool PropagateElement(const ConstraintProto* ct);
  bool PropagateTable(const ConstraintProto* ct);
  bool PropagateAutomaton(const ConstraintProto* ct);
  bool PropagateInverse(const ConstraintProto* ct);
  bool PropagateLinear(const ConstraintProto* ct);

  Model* model_;
  const SatParameters& parameters_;
  CpModelMapping* mapping_;
  IntegerEncoder* integer_encoder_;

  std::vector<bool> variable_was_added_in_to_propagate_;
  std::vector<int> variables_to_propagate_;
  std::vector<std::vector<const ConstraintProto*>> variable_watchers_;

  absl::flat_hash_set<const ConstraintProto*> constraint_is_finished_;
  absl::flat_hash_set<const ConstraintProto*> constraint_is_registered_;
};

bool FullEncodingFixedPointComputer::PropagateElement(
    const ConstraintProto* ct) {
  // Index must always be full encoded.
  FullyEncode(ct->element().index());

  // If target is a constant or fully encoded, variables must be fully encoded.
  const int target = ct->element().target();
  if (IsFullyEncoded(target)) {
    for (const int v : ct->element().vars()) FullyEncode(v);
  }

  // If all non-target variables are fully encoded, target must be too.
  bool all_variables_are_fully_encoded = true;
  for (const int v : ct->element().vars()) {
    if (v == target) continue;
    if (!IsFullyEncoded(v)) {
      all_variables_are_fully_encoded = false;
      break;
    }
  }
  if (all_variables_are_fully_encoded) {
    if (!IsFullyEncoded(target)) FullyEncode(target);
    return true;
  }

  // If some variables are not fully encoded, register on those.
  if (!gtl::ContainsKey(constraint_is_registered_, ct)) {
    for (const int v : ct->element().vars()) Register(ct, v);
    Register(ct, target);
  }
  return false;
}

// If a constraint uses its variables in a symbolic (vs. numeric) manner,
// always encode its variables.
bool FullEncodingFixedPointComputer::PropagateTable(const ConstraintProto* ct) {
  if (ct->table().negated()) return true;
  for (const int variable : ct->table().vars()) {
    FullyEncode(variable);
  }
  return true;
}

bool FullEncodingFixedPointComputer::PropagateAutomaton(
    const ConstraintProto* ct) {
  for (const int variable : ct->automaton().vars()) {
    FullyEncode(variable);
  }
  return true;
}

bool FullEncodingFixedPointComputer::PropagateInverse(
    const ConstraintProto* ct) {
  for (const int variable : ct->inverse().f_direct()) {
    FullyEncode(variable);
  }
  for (const int variable : ct->inverse().f_inverse()) {
    FullyEncode(variable);
  }
  return true;
}

bool FullEncodingFixedPointComputer::PropagateLinear(
    const ConstraintProto* ct) {
  if (parameters_.boolean_encoding_level() == 0) return true;

  // Only act when the constraint is an equality.
  if (ct->linear().domain(0) != ct->linear().domain(1)) return true;

  // If some domain is too large, abort;
  if (!gtl::ContainsKey(constraint_is_registered_, ct)) {
    for (const int v : ct->linear().vars()) {
      const IntegerVariable var = mapping_->Integer(v);
      IntegerTrail* integer_trail = model_->GetOrCreate<IntegerTrail>();
      const IntegerValue lb = integer_trail->LowerBound(var);
      const IntegerValue ub = integer_trail->UpperBound(var);
      if (ub - lb > 1024) return true;  // Arbitrary limit value.
    }
  }

  if (HasEnforcementLiteral(*ct)) {
    // Fully encode x in half-reified equality b => x == constant.
    const auto& vars = ct->linear().vars();
    if (vars.size() == 1) {
      FullyEncode(vars.Get(0));
    }
    return true;
  } else {
    // If all variables but one are fully encoded,
    // force the last one to be fully encoded.
    int variable_not_fully_encoded;
    int num_fully_encoded = 0;
    for (const int var : ct->linear().vars()) {
      if (IsFullyEncoded(var)) {
        num_fully_encoded++;
      } else {
        variable_not_fully_encoded = var;
      }
    }
    const int num_vars = ct->linear().vars_size();
    if (num_fully_encoded == num_vars - 1) {
      FullyEncode(variable_not_fully_encoded);
      return true;
    }
    if (num_fully_encoded == num_vars) return true;

    // Register on remaining variables if not already done.
    if (!gtl::ContainsKey(constraint_is_registered_, ct)) {
      for (const int var : ct->linear().vars()) {
        if (!IsFullyEncoded(var)) Register(ct, var);
      }
    }
    return false;
  }
}

// Makes the std::string fit in one line by cutting it in the middle if
// necessary.
std::string Summarize(const std::string& input) {
  if (input.size() < 105) return input;
  const int half = 50;
  return absl::StrCat(input.substr(0, half), " ... ",
                      input.substr(input.size() - half, half));
}

}  // namespace.

// =============================================================================
// Public API.
// =============================================================================

std::string CpModelStats(const CpModelProto& model_proto) {
  std::map<std::string, int> num_constraints_by_name;
  std::map<std::string, int> num_reif_constraints_by_name;
  std::map<std::string, int> name_to_num_literals;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    std::string name = ConstraintCaseName(ct.constraint_case());

    // We split the linear constraints into 3 buckets has it gives more insight
    // on the type of problem we are facing.
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
      if (ct.linear().vars_size() == 1) name += "1";
      if (ct.linear().vars_size() == 2) name += "2";
      if (ct.linear().vars_size() > 2) name += "N";
    }

    num_constraints_by_name[name]++;
    if (!ct.enforcement_literal().empty()) {
      num_reif_constraints_by_name[name]++;
    }

    // For pure Boolean constraints, we also display the total number of literal
    // involved as this gives a good idea of the problem size.
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
      name_to_num_literals[name] += ct.bool_or().literals().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kBoolAnd) {
      name_to_num_literals[name] += ct.bool_and().literals().size();
    } else if (ct.constraint_case() ==
               ConstraintProto::ConstraintCase::kAtMostOne) {
      name_to_num_literals[name] += ct.at_most_one().literals().size();
    }
  }

  int num_constants = 0;
  std::set<int64> constant_values;
  std::map<Domain, int> num_vars_per_domains;
  for (const IntegerVariableProto& var : model_proto.variables()) {
    if (var.domain_size() == 2 && var.domain(0) == var.domain(1)) {
      ++num_constants;
      constant_values.insert(var.domain(0));
    } else {
      num_vars_per_domains[ReadDomainFromProto(var)]++;
    }
  }

  std::string result;
  if (model_proto.has_objective()) {
    absl::StrAppend(&result, "Optimization model '", model_proto.name(),
                    "':\n");
  } else {
    absl::StrAppend(&result, "Satisfaction model '", model_proto.name(),
                    "':\n");
  }

  for (const DecisionStrategyProto& strategy : model_proto.search_strategy()) {
    absl::StrAppend(
        &result, "Search strategy: on ", strategy.variables_size(),
        " variables, ",
        ProtoEnumToString<DecisionStrategyProto::VariableSelectionStrategy>(
            strategy.variable_selection_strategy()),
        ", ",
        ProtoEnumToString<DecisionStrategyProto::DomainReductionStrategy>(
            strategy.domain_reduction_strategy()),
        "\n");
  }

  const std::string objective_string =
      model_proto.has_objective()
          ? absl::StrCat(" (", model_proto.objective().vars_size(),
                         " in objective)")
          : "";
  absl::StrAppend(&result, "#Variables: ", model_proto.variables_size(),
                  objective_string, "\n");
  if (num_vars_per_domains.size() < 50) {
    for (const auto& entry : num_vars_per_domains) {
      const std::string temp = absl::StrCat(" - ", entry.second, " in ",
                                            entry.first.ToString(), "\n");
      absl::StrAppend(&result, Summarize(temp));
    }
  } else {
    int64 max_complexity = 0;
    int64 min = kint64max;
    int64 max = kint64min;
    for (const auto& entry : num_vars_per_domains) {
      min = std::min(min, entry.first.Min());
      max = std::max(max, entry.first.Max());
      max_complexity = std::max(max_complexity,
                                static_cast<int64>(entry.first.NumIntervals()));
    }
    absl::StrAppend(&result, " - ", num_vars_per_domains.size(),
                    " different domains in [", min, ",", max,
                    "] with a largest complexity of ", max_complexity, ".\n");
  }

  if (num_constants > 0) {
    const std::string temp =
        absl::StrCat(" - ", num_constants, " constants in {",
                     absl::StrJoin(constant_values, ","), "} \n");
    absl::StrAppend(&result, Summarize(temp));
  }

  std::vector<std::string> constraints;
  constraints.reserve(num_constraints_by_name.size());
  for (const auto entry : num_constraints_by_name) {
    const std::string& name = entry.first;
    constraints.push_back(absl::StrCat("#", name, ": ", entry.second));
    if (gtl::ContainsKey(num_reif_constraints_by_name, name)) {
      absl::StrAppend(&constraints.back(),
                      " (#enforced: ", num_reif_constraints_by_name[name], ")");
    }
    if (gtl::ContainsKey(name_to_num_literals, name)) {
      absl::StrAppend(&constraints.back(),
                      " (#literals: ", name_to_num_literals[name], ")");
    }
  }
  std::sort(constraints.begin(), constraints.end());
  absl::StrAppend(&result, absl::StrJoin(constraints, "\n"));

  return result;
}

std::string CpSolverResponseStats(const CpSolverResponse& response) {
  std::string result;
  absl::StrAppend(&result, "CpSolverResponse:");
  absl::StrAppend(&result, "\nstatus: ",
                  ProtoEnumToString<CpSolverStatus>(response.status()));

  // We special case the pure-decision problem for clarity.
  //
  // TODO(user): This test is not ideal for the corner case where the status is
  // still UNKNOWN yet we already know that if there is a solution, then its
  // objective is zero...
  if (response.status() == CpSolverStatus::INFEASIBLE ||
      (response.status() != CpSolverStatus::OPTIMAL &&
       response.objective_value() == 0 &&
       response.best_objective_bound() == 0)) {
    absl::StrAppend(&result, "\nobjective: NA");
    absl::StrAppend(&result, "\nbest_bound: NA");
  } else {
    absl::StrAppendFormat(&result, "\nobjective: %.9g",
                          response.objective_value());
    absl::StrAppendFormat(&result, "\nbest_bound: %.9g",
                          response.best_objective_bound());
  }

  absl::StrAppend(&result, "\nbooleans: ", response.num_booleans());
  absl::StrAppend(&result, "\nconflicts: ", response.num_conflicts());
  absl::StrAppend(&result, "\nbranches: ", response.num_branches());

  // TODO(user): This is probably better named "binary_propagation", but we just
  // output "propagations" to be consistent with sat/analyze.sh.
  absl::StrAppend(&result,
                  "\npropagations: ", response.num_binary_propagations());
  absl::StrAppend(
      &result, "\ninteger_propagations: ", response.num_integer_propagations());
  absl::StrAppend(&result, "\nwalltime: ", response.wall_time());
  absl::StrAppend(&result, "\nusertime: ", response.user_time());
  absl::StrAppend(&result,
                  "\ndeterministic_time: ", response.deterministic_time());
  absl::StrAppend(&result, "\n");
  return result;
}

namespace {

void FillSolutionInResponse(const CpModelProto& model_proto, const Model& model,
                            IntegerVariable objective_var,
                            CpSolverResponse* response) {
  response->set_status(CpSolverStatus::FEASIBLE);
  response->clear_solution();
  response->clear_solution_lower_bounds();
  response->clear_solution_upper_bounds();

  auto* mapping = model.Get<CpModelMapping>();
  auto* trail = model.Get<Trail>();
  auto* integer_trail = model.Get<IntegerTrail>();

  std::vector<int64> solution;
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsInteger(i)) {
      const IntegerVariable var = mapping->Integer(i);
      if (integer_trail->IsCurrentlyIgnored(var)) {
        // This variable is "ignored" so it may not be fixed, simply use
        // the current lower bound. Any value in its domain should lead to
        // a feasible solution.
        solution.push_back(model.Get(LowerBound(var)));
      } else {
        if (model.Get(LowerBound(var)) != model.Get(UpperBound(var))) {
          solution.clear();
          break;
        }
        solution.push_back(model.Get(Value(var)));
      }
    } else {
      DCHECK(mapping->IsBoolean(i));
      const Literal literal = mapping->Literal(i);
      if (trail->Assignment().LiteralIsAssigned(literal)) {
        solution.push_back(model.Get(Value(literal)));
      } else {
        solution.clear();
        break;
      }
    }
  }

  if (!solution.empty()) {
    if (DEBUG_MODE || FLAGS_cp_model_check_intermediate_solutions) {
      // TODO(user): Checks against initial model.
      CHECK(SolutionIsFeasible(model_proto, solution));
    }
    for (const int64 value : solution) response->add_solution(value);
  } else {
    // Not all variables are fixed.
    // We fill instead the lb/ub of each variables.
    const auto& assignment = trail->Assignment();
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      if (mapping->IsBoolean(i)) {
        if (assignment.VariableIsAssigned(mapping->Literal(i).Variable())) {
          const int value = model.Get(Value(mapping->Literal(i)));
          response->add_solution_lower_bounds(value);
          response->add_solution_upper_bounds(value);
        } else {
          response->add_solution_lower_bounds(0);
          response->add_solution_upper_bounds(1);
        }
      } else {
        response->add_solution_lower_bounds(
            model.Get(LowerBound(mapping->Integer(i))));
        response->add_solution_upper_bounds(
            model.Get(UpperBound(mapping->Integer(i))));
      }
    }
  }

  // Fill the objective value and the best objective bound.
  if (model_proto.has_objective()) {
    const CpObjectiveProto& obj = model_proto.objective();
    int64 objective_value = 0;
    for (int i = 0; i < model_proto.objective().vars_size(); ++i) {
      objective_value +=
          model_proto.objective().coeffs(i) *
          model.Get(
              LowerBound(mapping->Integer(model_proto.objective().vars(i))));
    }
    response->set_objective_value(ScaleObjectiveValue(obj, objective_value));
    response->set_best_objective_bound(ScaleObjectiveValue(
        obj, integer_trail->LevelZeroLowerBound(objective_var).value()));
  } else {
    response->clear_objective_value();
    response->clear_best_objective_bound();
  }
}

namespace {

IntegerVariable GetOrCreateVariableWithTightBound(
    const std::vector<std::pair<IntegerVariable, int64>>& terms, Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  int64 sum_min = 0;
  int64 sum_max = 0;
  for (const std::pair<IntegerVariable, int64> var_coeff : terms) {
    const int64 min_domain = model->Get(LowerBound(var_coeff.first));
    const int64 max_domain = model->Get(UpperBound(var_coeff.first));
    const int64 coeff = var_coeff.second;
    const int64 prod1 = min_domain * coeff;
    const int64 prod2 = max_domain * coeff;
    sum_min += std::min(prod1, prod2);
    sum_max += std::max(prod1, prod2);
  }
  return model->Add(NewIntegerVariable(sum_min, sum_max));
}

IntegerVariable GetOrCreateVariableGreaterOrEqualToSumOf(
    const std::vector<std::pair<IntegerVariable, int64>>& terms, Model* model) {
  if (terms.empty()) return model->Add(ConstantIntegerVariable(0));
  if (terms.size() == 1 && terms.front().second == 1) {
    return terms.front().first;
  }
  if (terms.size() == 1 && terms.front().second == -1) {
    return NegationOf(terms.front().first);
  }

  // Create a new variable and link it with the linear terms.
  const IntegerVariable new_var =
      GetOrCreateVariableWithTightBound(terms, model);
  std::vector<IntegerVariable> vars;
  std::vector<int64> coeffs;
  for (const auto& term : terms) {
    vars.push_back(term.first);
    coeffs.push_back(term.second);
  }
  vars.push_back(new_var);
  coeffs.push_back(-1);
  model->Add(WeightedSumLowerOrEqual(vars, coeffs, 0));
  return new_var;
}

// Add a linear relaxation of the CP constraint to the set of linear
// constraints. The highest linearization_level is, the more types of constraint
// we encode. At level zero, we only encode non-reified linear constraints.
//
// TODO(user): In full generality, we could encode all the constraint as an LP.
void TryToLinearizeConstraint(const CpModelProto& model_proto,
                              const ConstraintProto& ct, Model* m,
                              int linearization_level,
                              LinearRelaxation* relaxation) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kBoolOr) {
    if (linearization_level < 2) return;
    LinearConstraintBuilder lc(m, IntegerValue(1), kMaxIntegerValue);
    for (const int enforcement_ref : ct.enforcement_literal()) {
      CHECK(lc.AddLiteralTerm(mapping->Literal(NegatedRef(enforcement_ref)),
                              IntegerValue(1)));
    }
    for (const int ref : ct.bool_or().literals()) {
      CHECK(lc.AddLiteralTerm(mapping->Literal(ref), IntegerValue(1)));
    }
    relaxation->linear_constraints.push_back(lc.Build());
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kBoolAnd) {
    // TODO(user): These constraints can be many, and if they are not regrouped
    // in big at most ones, then they should probably only added lazily as cuts.
    // Regroup this with future clique-cut separation logic.
    if (linearization_level < 2) return;
    if (!HasEnforcementLiteral(ct)) return;
    if (ct.enforcement_literal().size() == 1) {
      const Literal enforcement = mapping->Literal(ct.enforcement_literal(0));
      for (const int ref : ct.bool_and().literals()) {
        relaxation->at_most_ones.push_back(
            {enforcement, mapping->Literal(ref).Negated()});
      }
      return;
    }
    for (const int ref : ct.bool_and().literals()) {
      // Same as the clause linearization above.
      LinearConstraintBuilder lc(m, IntegerValue(1), kMaxIntegerValue);
      for (const int enforcement_ref : ct.enforcement_literal()) {
        CHECK(lc.AddLiteralTerm(mapping->Literal(NegatedRef(enforcement_ref)),
                                IntegerValue(1)));
      }
      CHECK(lc.AddLiteralTerm(mapping->Literal(ref), IntegerValue(1)));
      relaxation->linear_constraints.push_back(lc.Build());
    }
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kAtMostOne) {
    if (HasEnforcementLiteral(ct)) return;
    std::vector<Literal> at_most_one;
    for (const int ref : ct.at_most_one().literals()) {
      at_most_one.push_back(mapping->Literal(ref));
    }
    relaxation->at_most_ones.push_back(at_most_one);
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMax) {
    if (HasEnforcementLiteral(ct)) return;
    const int target = ct.int_max().target();
    for (const int var : ct.int_max().vars()) {
      // This deal with the corner case X = max(X, Y, Z, ..) !
      // Note that this can be presolved into X >= Y, X >= Z, ...
      if (target == var) continue;
      LinearConstraintBuilder lc(m, kMinIntegerValue, IntegerValue(0));
      lc.AddTerm(mapping->Integer(var), IntegerValue(1));
      lc.AddTerm(mapping->Integer(target), IntegerValue(-1));
      relaxation->linear_constraints.push_back(lc.Build());
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kIntMin) {
    if (HasEnforcementLiteral(ct)) return;
    const int target = ct.int_min().target();
    for (const int var : ct.int_min().vars()) {
      if (target == var) continue;
      LinearConstraintBuilder lc(m, kMinIntegerValue, IntegerValue(0));
      lc.AddTerm(mapping->Integer(target), IntegerValue(1));
      lc.AddTerm(mapping->Integer(var), IntegerValue(-1));
      relaxation->linear_constraints.push_back(lc.Build());
    }
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kIntProd) {
    if (HasEnforcementLiteral(ct)) return;
    const int target = ct.int_prod().target();
    const int size = ct.int_prod().vars_size();

    // We just linearize x = y^2 by x >= y which is far from ideal but at
    // least pushes x when y moves away from zero. Note that if y is negative,
    // we should probably also add x >= -y, but then this do not happen in
    // our test set.
    if (size == 2 && ct.int_prod().vars(0) == ct.int_prod().vars(1)) {
      LinearConstraintBuilder lc(m, kMinIntegerValue, IntegerValue(0));
      lc.AddTerm(mapping->Integer(ct.int_prod().vars(0)), IntegerValue(1));
      lc.AddTerm(mapping->Integer(target), IntegerValue(-1));
      relaxation->linear_constraints.push_back(lc.Build());
    }
  } else if (ct.constraint_case() == ConstraintProto::ConstraintCase::kLinear) {
    AppendLinearConstraintRelaxation(ct, linearization_level, *m, relaxation);
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kCircuit) {
    if (HasEnforcementLiteral(ct)) return;
    const int num_arcs = ct.circuit().literals_size();
    CHECK_EQ(num_arcs, ct.circuit().tails_size());
    CHECK_EQ(num_arcs, ct.circuit().heads_size());

    // Each node must have exactly one incoming and one outgoing arc (note that
    // it can be the unique self-arc of this node too).
    std::map<int, std::vector<Literal>> incoming_arc_constraints;
    std::map<int, std::vector<Literal>> outgoing_arc_constraints;
    for (int i = 0; i < num_arcs; i++) {
      const Literal arc = mapping->Literal(ct.circuit().literals(i));
      const int tail = ct.circuit().tails(i);
      const int head = ct.circuit().heads(i);

      // Make sure this literal has a view.
      m->Add(NewIntegerVariableFromLiteral(arc));
      outgoing_arc_constraints[tail].push_back(arc);
      incoming_arc_constraints[head].push_back(arc);
    }
    for (const auto* node_map :
         {&outgoing_arc_constraints, &incoming_arc_constraints}) {
      for (const auto& entry : *node_map) {
        const std::vector<Literal>& exactly_one = entry.second;
        if (exactly_one.size() > 1) {
          LinearConstraintBuilder at_least_one_lc(m, IntegerValue(1),
                                                  kMaxIntegerValue);
          for (const Literal l : exactly_one) {
            CHECK(at_least_one_lc.AddLiteralTerm(l, IntegerValue(1)));
          }

          // We separate the two constraints.
          relaxation->at_most_ones.push_back(exactly_one);
          relaxation->linear_constraints.push_back(at_least_one_lc.Build());
        }
      }
    }
  } else if (ct.constraint_case() ==
             ConstraintProto::ConstraintCase::kElement) {
    const IntegerVariable index = mapping->Integer(ct.element().index());
    const IntegerVariable target = mapping->Integer(ct.element().target());
    const std::vector<IntegerVariable> vars =
        mapping->Integers(ct.element().vars());

    // We only relax the case where all the vars are constant.
    // target = sum (index == i) * fixed_vars[i].
    LinearConstraintBuilder constraint(m, IntegerValue(0), IntegerValue(0));
    constraint.AddTerm(target, IntegerValue(-1));
    IntegerTrail* integer_trail = m->GetOrCreate<IntegerTrail>();
    for (const auto literal_value : m->Add(FullyEncodeVariable((index)))) {
      const IntegerVariable var = vars[literal_value.value.value()];
      if (!m->Get(IsFixed(var))) return;

      // Make sure this literal has a view.
      m->Add(NewIntegerVariableFromLiteral(literal_value.literal));
      CHECK(constraint.AddLiteralTerm(literal_value.literal,
                                      integer_trail->LowerBound(var)));
    }

    relaxation->linear_constraints.push_back(constraint.Build());
  }
}

void TryToAddCutGenerators(const CpModelProto& model_proto,
                           const ConstraintProto& ct, Model* m,
                           LinearRelaxation* relaxation) {
  auto* mapping = m->GetOrCreate<CpModelMapping>();
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kCircuit) {
    std::vector<int> tails(ct.circuit().tails().begin(),
                           ct.circuit().tails().end());
    std::vector<int> heads(ct.circuit().heads().begin(),
                           ct.circuit().heads().end());
    std::vector<Literal> literals = mapping->Literals(ct.circuit().literals());
    const int num_nodes = ReindexArcs(&tails, &heads, &literals);

    std::vector<IntegerVariable> vars;
    vars.reserve(literals.size());
    for (const Literal& literal : literals) {
      vars.push_back(m->Add(NewIntegerVariableFromLiteral(literal)));
    }

    relaxation->cut_generators.push_back(
        CreateStronglyConnectedGraphCutGenerator(num_nodes, tails, heads,
                                                 vars));
  }
  if (ct.constraint_case() == ConstraintProto::ConstraintCase::kRoutes) {
    std::vector<int> tails;
    std::vector<int> heads;
    std::vector<IntegerVariable> vars;
    int num_nodes = 0;
    auto* encoder = m->GetOrCreate<IntegerEncoder>();
    for (int i = 0; i < ct.routes().tails_size(); ++i) {
      const IntegerVariable var =
          encoder->GetLiteralView(mapping->Literal(ct.routes().literals(i)));
      if (var == kNoIntegerVariable) return;

      vars.push_back(var);
      tails.push_back(ct.routes().tails(i));
      heads.push_back(ct.routes().heads(i));
      num_nodes = std::max(num_nodes, 1 + ct.routes().tails(i));
      num_nodes = std::max(num_nodes, 1 + ct.routes().heads(i));
    }
    if (ct.routes().demands().empty() || ct.routes().capacity() == 0) {
      relaxation->cut_generators.push_back(
          CreateStronglyConnectedGraphCutGenerator(num_nodes, tails, heads,
                                                   vars));
    } else {
      const std::vector<int64> demands(ct.routes().demands().begin(),
                                       ct.routes().demands().end());
      relaxation->cut_generators.push_back(CreateCVRPCutGenerator(
          num_nodes, tails, heads, vars, demands, ct.routes().capacity()));
    }
  }
}

}  // namespace

// Adds one LinearProgrammingConstraint per connected component of the model.
IntegerVariable AddLPConstraints(const CpModelProto& model_proto,
                                 int linearization_level, Model* m) {
  LinearRelaxation relaxation;

  // Linearize the constraints.
  absl::flat_hash_set<int> used_integer_variable;

  auto* mapping = m->GetOrCreate<CpModelMapping>();
  auto* encoder = m->GetOrCreate<IntegerEncoder>();
  for (const auto& ct : model_proto.constraints()) {
    // Make sure the literal from a circuit constraint always have a view.
    if (ct.constraint_case() == ConstraintProto::ConstraintCase::kCircuit) {
      for (const int ref : ct.circuit().literals()) {
        m->Add(NewIntegerVariableFromLiteral(mapping->Literal(ref)));
      }
    }

    // For now, we skip any constraint with literals that do not have an integer
    // view. Ideally it should be up to the constraint to decide if creating a
    // view is worth it.
    //
    // TODO(user): It should be possible to speed this up if needed.
    const IndexReferences refs = GetReferencesUsedByConstraint(ct);
    bool ok = true;
    for (const int literal_ref : refs.literals) {
      const Literal literal = mapping->Literal(literal_ref);
      if (encoder->GetLiteralView(literal) == kNoIntegerVariable &&
          encoder->GetLiteralView(literal.Negated()) == kNoIntegerVariable) {
        ok = false;
        break;
      }
    }
    if (!ok) continue;

    TryToLinearizeConstraint(model_proto, ct, m, linearization_level,
                             &relaxation);

    // For now these are only useful on problem with circuit. They can help
    // a lot on complex problems, but they also slow down simple ones.
    if (linearization_level > 1) {
      TryToAddCutGenerators(model_proto, ct, m, &relaxation);
    }
  }

  // Linearize the encoding of variable that are fully encoded in the proto.
  int num_full_encoding_relaxations = 0;
  int num_partial_encoding_relaxations = 0;
  for (int i = 0; i < model_proto.variables_size(); ++i) {
    if (mapping->IsBoolean(i)) continue;

    const IntegerVariable var = mapping->Integer(i);
    if (m->Get(IsFixed(var))) continue;

    // TODO(user): This different encoding for the partial variable might be
    // better (less LP constraints), but we do need more investigation to
    // decide.
    if (/* DISABLES CODE */ (false)) {
      AppendPartialEncodingRelaxation(var, *m, &relaxation);
      continue;
    }

    if (encoder->VariableIsFullyEncoded(var)) {
      if (AppendFullEncodingRelaxation(var, *m, &relaxation)) {
        ++num_full_encoding_relaxations;
        continue;
      }
    }

    // Even if the variable is fully encoded, sometimes not all its associated
    // literal have a view (if they are not part of the original model for
    // instance).
    //
    // TODO(user): Should we add them to the LP anyway? this isn't clear as
    // we can sometimes create a lot of Booleans like this.
    const int old = relaxation.linear_constraints.size();
    AppendPartialGreaterThanEncodingRelaxation(var, *m, &relaxation);
    if (relaxation.linear_constraints.size() > old) {
      ++num_partial_encoding_relaxations;
    }
  }

  // Linearize the at most one constraints. Note that we transform them
  // into maximum "at most one" first and we removes redundant ones.
  m->GetOrCreate<BinaryImplicationGraph>()->TransformIntoMaxCliques(
      &relaxation.at_most_ones);
  for (const std::vector<Literal>& at_most_one : relaxation.at_most_ones) {
    if (at_most_one.empty()) continue;
    LinearConstraintBuilder lc(m, kMinIntegerValue, IntegerValue(1));
    for (const Literal literal : at_most_one) {
      // Note that it is okay to simply ignore the literal if it has no
      // integer view.
      const bool unused ABSL_ATTRIBUTE_UNUSED =
          lc.AddLiteralTerm(literal, IntegerValue(1));
    }
    relaxation.linear_constraints.push_back(lc.Build());
  }

  // Remove size one LP constraints, they are not useful.
  {
    int new_size = 0;
    for (int i = 0; i < relaxation.linear_constraints.size(); ++i) {
      if (relaxation.linear_constraints[i].vars.size() <= 1) continue;
      std::swap(relaxation.linear_constraints[new_size++],
                relaxation.linear_constraints[i]);
    }
    relaxation.linear_constraints.resize(new_size);
  }

  VLOG(2) << "num_full_encoding_relaxations: " << num_full_encoding_relaxations;
  VLOG(2) << "num_partial_encoding_relaxations: "
          << num_partial_encoding_relaxations;
  VLOG(2) << relaxation.linear_constraints.size()
          << " constraints in the LP relaxation.";
  VLOG(2) << relaxation.cut_generators.size() << " cuts generators.";

  // The bipartite graph of LP constraints might be disconnected:
  // make a partition of the variables into connected components.
  // Constraint nodes are indexed by [0..num_lp_constraints),
  // variable nodes by [num_lp_constraints..num_lp_constraints+num_variables).
  //
  // TODO(user): look into biconnected components.
  const int num_lp_constraints = relaxation.linear_constraints.size();
  const int num_lp_cut_generators = relaxation.cut_generators.size();
  const int num_integer_variables =
      m->GetOrCreate<IntegerTrail>()->NumIntegerVariables().value();
  ConnectedComponents<int, int> components;
  components.Init(num_lp_constraints + num_lp_cut_generators +
                  num_integer_variables);
  auto get_constraint_index = [](int ct_index) { return ct_index; };
  auto get_cut_generator_index = [num_lp_constraints](int cut_index) {
    return num_lp_constraints + cut_index;
  };
  auto get_var_index = [num_lp_constraints,
                        num_lp_cut_generators](IntegerVariable var) {
    return num_lp_constraints + num_lp_cut_generators + var.value();
  };
  for (int i = 0; i < num_lp_constraints; i++) {
    for (const IntegerVariable var : relaxation.linear_constraints[i].vars) {
      components.AddArc(get_constraint_index(i), get_var_index(var));
    }
  }
  for (int i = 0; i < num_lp_cut_generators; ++i) {
    for (const IntegerVariable var : relaxation.cut_generators[i].vars) {
      components.AddArc(get_cut_generator_index(i), get_var_index(var));
    }
  }

  std::map<int, int> components_to_size;
  for (int i = 0; i < num_lp_constraints; i++) {
    const int id = components.GetClassRepresentative(get_constraint_index(i));
    components_to_size[id] += 1;
  }
  for (int i = 0; i < num_lp_cut_generators; i++) {
    const int id =
        components.GetClassRepresentative(get_cut_generator_index(i));
    components_to_size[id] += 1;
  }

  // Make sure any constraint that touch the objective is not discarded even
  // if it is the only one in its component. This is important to propagate
  // as much as possible the objective bound by using any bounds the LP give
  // us on one of its components. This is critical on the zephyrus problems for
  // instance.
  for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
    const IntegerVariable var =
        mapping->Integer(model_proto.objective().vars(i));
    const int id = components.GetClassRepresentative(get_var_index(var));
    components_to_size[id] += 1;
  }

  // Dispatch every constraint to its LinearProgrammingConstraint.
  std::map<int, LinearProgrammingConstraint*> representative_to_lp_constraint;
  std::vector<LinearProgrammingConstraint*> lp_constraints;
  std::map<int, std::vector<LinearConstraint>> id_to_constraints;
  for (int i = 0; i < num_lp_constraints; i++) {
    const int id = components.GetClassRepresentative(get_constraint_index(i));
    if (components_to_size[id] <= 1) continue;
    id_to_constraints[id].push_back(relaxation.linear_constraints[i]);
    if (!gtl::ContainsKey(representative_to_lp_constraint, id)) {
      auto* lp = m->Create<LinearProgrammingConstraint>();
      representative_to_lp_constraint[id] = lp;
      lp_constraints.push_back(lp);
    }

    // Load the constraint.
    gtl::FindOrDie(representative_to_lp_constraint, id)
        ->AddLinearConstraint(relaxation.linear_constraints[i]);
  }

  // Dispatch every cut generator to its LinearProgrammingConstraint.
  for (int i = 0; i < num_lp_cut_generators; i++) {
    const int id =
        components.GetClassRepresentative(get_cut_generator_index(i));
    if (!gtl::ContainsKey(representative_to_lp_constraint, id)) {
      auto* lp = m->Create<LinearProgrammingConstraint>();
      representative_to_lp_constraint[id] = lp;
      lp_constraints.push_back(lp);
    }
    LinearProgrammingConstraint* lp = representative_to_lp_constraint[id];
    lp->AddCutGenerator(std::move(relaxation.cut_generators[i]));
  }

  const SatParameters& params = *(m->GetOrCreate<SatParameters>());
  if (params.add_knapsack_cuts()) {
    for (const auto entry : id_to_constraints) {
      const int id = entry.first;
      LinearProgrammingConstraint* lp =
          gtl::FindOrDie(representative_to_lp_constraint, id);
      lp->AddCutGenerator(CreateKnapsackCoverCutGenerator(
          id_to_constraints[id], lp->integer_variables(), m));
    }
  }

  // Add the objective.
  std::map<int, std::vector<std::pair<IntegerVariable, int64>>>
      representative_to_cp_terms;
  std::vector<std::pair<IntegerVariable, int64>> top_level_cp_terms;
  int num_components_containing_objective = 0;
  if (model_proto.has_objective()) {
    // First pass: set objective coefficients on the lp constraints, and store
    // the cp terms in one vector per component.
    for (int i = 0; i < model_proto.objective().coeffs_size(); ++i) {
      const IntegerVariable var =
          mapping->Integer(model_proto.objective().vars(i));
      const int64 coeff = model_proto.objective().coeffs(i);
      const int id = components.GetClassRepresentative(get_var_index(var));
      if (gtl::ContainsKey(representative_to_lp_constraint, id)) {
        representative_to_lp_constraint[id]->SetObjectiveCoefficient(
            var, IntegerValue(coeff));
        representative_to_cp_terms[id].push_back(std::make_pair(var, coeff));
      } else {
        // Component is too small. We still need to store the objective term.
        top_level_cp_terms.push_back(std::make_pair(var, coeff));
      }
    }
    // Second pass: Build the cp sub-objectives per component.
    for (const auto& it : representative_to_cp_terms) {
      const int id = it.first;
      LinearProgrammingConstraint* lp =
          gtl::FindOrDie(representative_to_lp_constraint, id);
      const std::vector<std::pair<IntegerVariable, int64>>& terms = it.second;
      const IntegerVariable sub_obj_var =
          GetOrCreateVariableGreaterOrEqualToSumOf(terms, m);
      top_level_cp_terms.push_back(std::make_pair(sub_obj_var, 1));
      lp->SetMainObjectiveVariable(sub_obj_var);
      num_components_containing_objective++;
    }
  }

  const IntegerVariable main_objective_var =
      model_proto.has_objective()
          ? GetOrCreateVariableGreaterOrEqualToSumOf(top_level_cp_terms, m)
          : kNoIntegerVariable;

  // Register LP constraints. Note that this needs to be done after all the
  // constraints have been added.
  for (auto* lp_constraint : lp_constraints) {
    lp_constraint->RegisterWith(m);
    VLOG(2) << "LP constraint: " << lp_constraint->DimensionString() << ".";
  }

  VLOG(2) << top_level_cp_terms.size()
          << " terms in the main objective linear equation ("
          << num_components_containing_objective << " from LP constraints).";
  return main_objective_var;
}

void ExtractLinearObjective(const CpModelProto& model_proto,
                            const CpModelMapping& mapping,
                            std::vector<IntegerVariable>* linear_vars,
                            std::vector<IntegerValue>* linear_coeffs) {
  CHECK(model_proto.has_objective());
  const CpObjectiveProto& obj = model_proto.objective();
  linear_vars->reserve(obj.vars_size());
  linear_coeffs->reserve(obj.vars_size());
  for (int i = 0; i < obj.vars_size(); ++i) {
    linear_vars->push_back(mapping.Integer(obj.vars(i)));
    linear_coeffs->push_back(IntegerValue(obj.coeffs(i)));
  }
}

}  // namespace

// Used by NewFeasibleSolutionObserver to register observers.
struct SolutionObservers {
  explicit SolutionObservers(Model* model) {}
  std::vector<std::function<void(const CpSolverResponse& response)>> observers;
};

std::function<void(Model*)> NewFeasibleSolutionObserver(
    const std::function<void(const CpSolverResponse& response)>& observer) {
  return [=](Model* model) {
    model->GetOrCreate<SolutionObservers>()->observers.push_back(observer);
  };
}

struct SynchronizationFunction {
  std::function<CpSolverResponse()> f;
};

void SetSynchronizationFunction(std::function<CpSolverResponse()> f,
                                Model* model) {
  model->GetOrCreate<SynchronizationFunction>()->f = std::move(f);
}

#if !defined(__PORTABLE_PLATFORM__)
// TODO(user): Support it on android.
std::function<SatParameters(Model*)> NewSatParameters(
    const std::string& params) {
  sat::SatParameters parameters;
  if (!params.empty()) {
    CHECK(google::protobuf::TextFormat::ParseFromString(params, &parameters))
        << params;
  }
  return NewSatParameters(parameters);
}
#endif  // __PORTABLE_PLATFORM__

std::function<SatParameters(Model*)> NewSatParameters(
    const sat::SatParameters& parameters) {
  return [=](Model* model) {
    // Tricky: It is important to initialize the model parameters before any
    // of the solver object are created, so that by default they use the given
    // parameters.
    *model->GetOrCreate<SatParameters>() = parameters;
    model->GetOrCreate<SatSolver>()->SetParameters(parameters);
    return parameters;
  };
}

namespace {
// Because we also use this function for postsolve, we call it with
// is_real_solve set to true and avoid doing non-useful work in this case.
CpSolverResponse SolveCpModelInternal(
    const CpModelProto& model_proto, bool is_real_solve,
    const std::function<void(const CpSolverResponse&)>&
        external_solution_observer,
    SharedBoundsManager* shared_bounds_manager, WallTimer* wall_timer,
    Model* model) {
  const bool worker_is_in_parallel_search = model->Get<WorkerInfo>() != nullptr;
  const bool is_lns = model->GetOrCreate<SatParameters>()->use_lns();

  if (!worker_is_in_parallel_search && is_real_solve && !is_lns) {
    VLOG(1) << absl::StrFormat("*** starting to load the model at %.2fs",
                               wall_timer->Get());
  }
  // Initialize a default invalid response.
  CpSolverResponse response;
  response.set_status(CpSolverStatus::MODEL_INVALID);

  auto fill_response_statistics = [&response, model, wall_timer]() {
    auto* sat_solver = model->Get<SatSolver>();
    response.set_num_booleans(sat_solver->NumVariables());
    response.set_num_branches(sat_solver->num_branches());
    response.set_num_conflicts(sat_solver->num_failures());
    response.set_num_binary_propagations(sat_solver->num_propagations());
    response.set_num_integer_propagations(
        model->Get<IntegerTrail>() == nullptr
            ? 0
            : model->Get<IntegerTrail>()->num_enqueues());
    response.set_wall_time(wall_timer->Get());
    response.set_deterministic_time(
        model->Get<TimeLimit>()->GetElapsedDeterministicTime());
  };

  // We will add them all at once after model_proto is loaded.
  model->GetOrCreate<IntegerEncoder>()->DisableImplicationBetweenLiteral();

  auto* mapping = model->GetOrCreate<CpModelMapping>();
  const SatParameters& parameters = *(model->GetOrCreate<SatParameters>());
  const bool view_all_booleans_as_integers =
      (parameters.linearization_level() >= 2) ||
      (parameters.search_branching() == SatParameters::FIXED_SEARCH &&
       model_proto.search_strategy().empty());
  mapping->CreateVariables(model_proto, view_all_booleans_as_integers, model);
  mapping->DetectOptionalVariables(model_proto, model);
  mapping->ExtractEncoding(model_proto, model);

  // Force some variables to be fully encoded.
  FullEncodingFixedPointComputer fixpoint(model);
  for (const ConstraintProto& ct : model_proto.constraints()) {
    fixpoint.PropagateFullEncoding(&ct);
  }
  fixpoint.ComputeFixedPoint();

  // Load the constraints.
  std::set<std::string> unsupported_types;
  int num_ignored_constraints = 0;
  for (const ConstraintProto& ct : model_proto.constraints()) {
    if (mapping->ConstraintIsAlreadyLoaded(&ct)) {
      ++num_ignored_constraints;
      continue;
    }

    if (!LoadConstraint(ct, model)) {
      unsupported_types.insert(ConstraintCaseName(ct.constraint_case()));
      continue;
    }

    // We propagate after each new Boolean constraint but not the integer
    // ones. So we call Propagate() manually here. Note that we do not do
    // that in the postsolve as there is some corner case where propagating
    // after each new constraint can have a quadratic behavior.
    //
    // Note that we only do that in debug mode as this can be really slow on
    // certain types of problems with millions of constraints.
    if (DEBUG_MODE) {
      model->GetOrCreate<SatSolver>()->Propagate();
      Trail* trail = model->GetOrCreate<Trail>();
      const int old_num_fixed = trail->Index();
      if (trail->Index() > old_num_fixed) {
        VLOG(2) << "Constraint fixed " << trail->Index() - old_num_fixed
                << " Boolean variable(s): " << ProtobufDebugString(ct);
      }
    }
    if (model->GetOrCreate<SatSolver>()->IsModelUnsat()) {
      VLOG(2) << "UNSAT during extraction (after adding '"
              << ConstraintCaseName(ct.constraint_case()) << "'). "
              << ProtobufDebugString(ct);
      break;
    }
  }
  if (num_ignored_constraints > 0) {
    VLOG(2) << num_ignored_constraints << " constraints were skipped.";
  }
  if (!unsupported_types.empty()) {
    VLOG(1) << "There is unsuported constraints types in this model: ";
    for (const std::string& type : unsupported_types) {
      VLOG(1) << " - " << type;
    }
    return response;
  }

  model->GetOrCreate<IntegerEncoder>()
      ->AddAllImplicationsBetweenAssociatedLiterals();
  model->GetOrCreate<SatSolver>()->Propagate();

  // Auto detect "at least one of" constraints in the PrecedencesPropagator.
  // Note that we do that before we finish loading the problem (objective and
  // LP relaxation), because propagation will be faster at this point and it
  // should be enough for the purpose of this auto-detection.
  if (model->Mutable<PrecedencesPropagator>() != nullptr &&
      parameters.auto_detect_greater_than_at_least_one_of()) {
    model->Mutable<PrecedencesPropagator>()
        ->AddGreaterThanAtLeastOneOfConstraints(model);
    model->GetOrCreate<SatSolver>()->Propagate();
  }

  // TODO(user): This should be done in the presolve instead.
  // TODO(user): We don't have a good deterministic time on all constraints,
  // so this might take more time than wanted.
  if (is_real_solve && parameters.cp_model_probing_level() > 1) {
    ProbeBooleanVariables(/*deterministic_time_limit=*/1.0, model);
    if (!model->GetOrCreate<BinaryImplicationGraph>()
             ->ComputeTransitiveReduction()) {
      model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
    }
  }

  // Create an objective variable and its associated linear constraint if
  // needed.
  IntegerVariable objective_var = kNoIntegerVariable;
  if (is_real_solve && parameters.linearization_level() > 0) {
    // Linearize some part of the problem and register LP constraint(s).
    objective_var =
        AddLPConstraints(model_proto, parameters.linearization_level(), model);
  } else if (model_proto.has_objective()) {
    const CpObjectiveProto& obj = model_proto.objective();
    std::vector<std::pair<IntegerVariable, int64>> terms;
    terms.reserve(obj.vars_size());
    for (int i = 0; i < obj.vars_size(); ++i) {
      terms.push_back(
          std::make_pair(mapping->Integer(obj.vars(i)), obj.coeffs(i)));
    }
    if (parameters.optimize_with_core()) {
      objective_var = GetOrCreateVariableWithTightBound(terms, model);
    } else {
      objective_var = GetOrCreateVariableGreaterOrEqualToSumOf(terms, model);
    }
  }

  if (objective_var != kNoIntegerVariable) {
    // Fill the ObjectiveSynchronizationHelper.
    ObjectiveSynchronizationHelper* helper =
        model->GetOrCreate<ObjectiveSynchronizationHelper>();
    helper->scaling_factor = model_proto.objective().scaling_factor();
    if (helper->scaling_factor == 0.0) {
      helper->scaling_factor = 1.0;
    }
    helper->offset = model_proto.objective().offset();
    helper->objective_var = objective_var;
  }

  // Intersect the objective domain with the given one if any.
  if (!model_proto.objective().domain().empty()) {
    const Domain user_domain = ReadDomainFromProto(model_proto.objective());
    const Domain automatic_domain =
        model->GetOrCreate<IntegerTrail>()->InitialVariableDomain(
            objective_var);
    VLOG(2) << "Objective offset:" << model_proto.objective().offset()
            << " scaling_factor:" << model_proto.objective().scaling_factor();
    VLOG(2) << "Automatic internal objective domain: " << automatic_domain;
    VLOG(2) << "User specified internal objective domain: " << user_domain;
    CHECK_NE(objective_var, kNoIntegerVariable);
    const bool ok = model->GetOrCreate<IntegerTrail>()->UpdateInitialDomain(
        objective_var, user_domain);
    if (!ok) {
      VLOG(2) << "UNSAT due to the objective domain.";
      model->GetOrCreate<SatSolver>()->NotifyThatModelIsUnsat();
    }

    // Make sure the sum take a value inside the objective domain by adding
    // the other side: objective <= sum terms.
    //
    // TODO(user): Use a better condition to detect when this is not useful.
    if (user_domain != automatic_domain) {
      std::vector<IntegerVariable> vars;
      std::vector<int64> coeffs;
      const CpObjectiveProto& obj = model_proto.objective();
      for (int i = 0; i < obj.vars_size(); ++i) {
        vars.push_back(mapping->Integer(obj.vars(i)));
        coeffs.push_back(obj.coeffs(i));
      }
      vars.push_back(objective_var);
      coeffs.push_back(-1);
      model->Add(WeightedSumGreaterOrEqual(vars, coeffs, 0));
    }
  }

  // Note that we do one last propagation at level zero once all the
  // constraints were added.
  model->GetOrCreate<SatSolver>()->Propagate();

  // Initialize the search strategy function.
  std::function<LiteralIndex()> next_decision = ConstructSearchStrategy(
      model_proto, mapping->GetVariableMapping(), objective_var, model);
  if (VLOG_IS_ON(3)) {
    next_decision = InstrumentSearchStrategy(
        model_proto, mapping->GetVariableMapping(), next_decision, model);
  }

  // Solve.
  int num_solutions = 0;
  SatSolver::Status status;

  // TODO(user): remove argument as it isn't used. Pass solution info instead?
  std::string solution_info;
  const auto solution_observer = [&model_proto, &response, &num_solutions,
                                  &model, &solution_info,
                                  &external_solution_observer, objective_var,
                                  &fill_response_statistics](const Model&) {
    num_solutions++;
    FillSolutionInResponse(model_proto, *model, objective_var, &response);
    fill_response_statistics();
    response.set_solution_info(
        absl::StrCat("num_bool:", model->Get<SatSolver>()->NumVariables(), " ",
                     solution_info));
    external_solution_observer(response);
  };

  // Objective bounds reporting and sharing.
  ObjectiveSynchronizationHelper* helper =
      model->GetOrCreate<ObjectiveSynchronizationHelper>();

  if (!is_lns && model_proto.has_objective()) {
    // Detect sequential mode, register callbacks in that case.
    if (!worker_is_in_parallel_search) {
      model->GetOrCreate<WorkerInfo>()->worker_id = 0;
      auto* integer_trail = model->Get<IntegerTrail>();
      const CpObjectiveProto& obj = model_proto.objective();

      helper->get_external_best_objective = [&response, integer_trail, &obj,
                                             objective_var]() {
        return response.status() != CpSolverStatus::MODEL_INVALID
                   ? response.objective_value()
                   : ScaleObjectiveValue(
                         obj, integer_trail->LevelZeroUpperBound(objective_var)
                                      .value() +
                                  1);
      };

      helper->get_external_best_bound = [&response, integer_trail, &obj,
                                         objective_var]() {
        return response.status() != CpSolverStatus::MODEL_INVALID
                   ? response.best_objective_bound()
                   : ScaleObjectiveValue(
                         obj, integer_trail->LevelZeroLowerBound(objective_var)
                                  .value());
      };

      helper->set_external_best_bound =
          [&response](double objective_value, double objective_best_bound) {
            response.set_best_objective_bound(objective_best_bound);
          };
    }

    // Watch improved objective best bounds in regular search, or core based
    // search. It should be disabled for LNS.
    RegisterObjectiveBestBoundExport(model_proto, VLOG_IS_ON(1), objective_var,
                                     wall_timer, model);
  }

  // Import objective bounds.
  // TODO(user): Support bounds import in LNS and Core based search.
  if (model->GetOrCreate<SatParameters>()->share_objective_bounds() &&
      worker_is_in_parallel_search && model_proto.has_objective()) {
    RegisterObjectiveBoundsImport(model);
  }

  // Level zero variable bounds sharing.
  if (shared_bounds_manager != nullptr) {
    RegisterVariableBoundsLevelZeroExport(model_proto, shared_bounds_manager,
                                          model);

    RegisterVariableBoundsLevelZeroImport(model_proto, shared_bounds_manager,
                                          model);
  }

  if (!worker_is_in_parallel_search && is_real_solve && !is_lns) {
    VLOG(1) << absl::StrFormat("*** starting sequential search at %.2fs",
                               wall_timer->Get());
  }

  // Load solution hint.
  // We follow it and allow for a tiny number of conflicts before giving up.
  //
  // TODO(user): When we get a feasible solution here, even with phase saving
  // it seems we don't get the same solution while launching the optimization
  // below. Understand why. Also just constrain the solver to find a better
  // solution right away, it should be more efficient. Note that this is kind
  // of already done when get_objective_value() is registered above.
  if (model_proto.has_solution_hint()) {
    const int64 old_conflict_limit = parameters.max_number_of_conflicts();
    model->GetOrCreate<SatParameters>()->set_max_number_of_conflicts(10);
    std::vector<BooleanOrIntegerVariable> vars;
    std::vector<IntegerValue> values;
    for (int i = 0; i < model_proto.solution_hint().vars_size(); ++i) {
      const int ref = model_proto.solution_hint().vars(i);
      CHECK(RefIsPositive(ref));
      BooleanOrIntegerVariable var;
      if (mapping->IsBoolean(ref)) {
        var.bool_var = mapping->Literal(ref).Variable();
      } else {
        var.int_var = mapping->Integer(ref);
      }
      vars.push_back(var);
      values.push_back(IntegerValue(model_proto.solution_hint().values(i)));
    }
    std::vector<std::function<LiteralIndex()>> decision_policies = {
        SequentialSearch({FollowHint(vars, values, model),
                          SatSolverHeuristic(model), next_decision})};
    auto no_restart = []() { return false; };
    status =
        SolveProblemWithPortfolioSearch(decision_policies, {no_restart}, model);
    if (status == SatSolver::Status::FEASIBLE) {
      solution_info = "hint";
      solution_observer(*model);
      CHECK(SolutionIsFeasible(model_proto,
                               std::vector<int64>(response.solution().begin(),
                                                  response.solution().end())));
      if (!model_proto.has_objective()) {
        if (parameters.enumerate_all_solutions()) {
          model->Add(
              ExcludeCurrentSolutionWithoutIgnoredVariableAndBacktrack());
        } else {
          return response;
        }
      } else {
        IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
        const IntegerValue current_internal_objective =
            integer_trail->LowerBound(objective_var);
        // Restrict the objective.
        model->GetOrCreate<SatSolver>()->Backtrack(0);
        if (!integer_trail->Enqueue(
                IntegerLiteral::LowerOrEqual(objective_var,
                                             current_internal_objective - 1),
                {}, {})) {
          response.set_best_objective_bound(response.objective_value());
          response.set_status(CpSolverStatus::OPTIMAL);
          fill_response_statistics();
          return response;
        }
      }
    }
    // TODO(user): Remove conflicts used during hint search.
    model->GetOrCreate<SatParameters>()->set_max_number_of_conflicts(
        old_conflict_limit);
  }

  solution_info = "";
  if (!model_proto.has_objective()) {
    while (true) {
      status = SolveIntegerProblemWithLazyEncoding(
          /*assumptions=*/{}, next_decision, model);
      if (status != SatSolver::Status::FEASIBLE) break;
      solution_observer(*model);
      if (!parameters.enumerate_all_solutions()) break;
      model->Add(ExcludeCurrentSolutionWithoutIgnoredVariableAndBacktrack());
    }
    if (num_solutions > 0) {
      if (status == SatSolver::Status::INFEASIBLE) {
        response.set_all_solutions_were_found(true);
      }
      status = SatSolver::Status::FEASIBLE;
    }
  } else {
    // Optimization problem.
    const CpObjectiveProto& obj = model_proto.objective();
    VLOG(2) << obj.vars_size() << " terms in the proto objective.";
    VLOG(2) << "Initial num_bool: " << model->Get<SatSolver>()->NumVariables();

    if (parameters.optimize_with_core()) {
      std::vector<IntegerVariable> linear_vars;
      std::vector<IntegerValue> linear_coeffs;
      ExtractLinearObjective(model_proto, *model->GetOrCreate<CpModelMapping>(),
                             &linear_vars, &linear_coeffs);
      if (parameters.optimize_with_max_hs()) {
        status = MinimizeWithHittingSetAndLazyEncoding(
            VLOG_IS_ON(2), objective_var, linear_vars, linear_coeffs,
            next_decision, solution_observer, model);
      } else {
        status = MinimizeWithCoreAndLazyEncoding(
            VLOG_IS_ON(2), objective_var, linear_vars, linear_coeffs,
            next_decision, solution_observer, model);
      }
    } else {
      if (parameters.binary_search_num_conflicts() >= 0) {
        RestrictObjectiveDomainWithBinarySearch(objective_var, next_decision,
                                                solution_observer, model);
      }
      status = MinimizeIntegerVariableWithLinearScanAndLazyEncoding(
          /*log_info=*/false, objective_var, next_decision, solution_observer,
          model);
      if (num_solutions > 0 && status == SatSolver::INFEASIBLE) {
        status = SatSolver::FEASIBLE;
      }
    }

    if (status == SatSolver::LIMIT_REACHED) {
      model->GetOrCreate<SatSolver>()->Backtrack(0);
      if (num_solutions == 0) {
        response.set_objective_value(
            ScaleObjectiveValue(obj, model->Get(UpperBound(objective_var))));
      }
      response.set_best_objective_bound(
          ScaleObjectiveValue(obj, model->Get(LowerBound(objective_var))));
    } else if (status == SatSolver::FEASIBLE) {
      // Optimal!
      response.set_best_objective_bound(response.objective_value());
    }
  }

  // Fill response.
  switch (status) {
    case SatSolver::LIMIT_REACHED: {
      response.set_status(num_solutions != 0 ? CpSolverStatus::FEASIBLE
                                             : CpSolverStatus::UNKNOWN);
      break;
    }
    case SatSolver::FEASIBLE: {
      response.set_status(model_proto.has_objective()
                              ? CpSolverStatus::OPTIMAL
                              : CpSolverStatus::FEASIBLE);
      break;
    }
    case SatSolver::INFEASIBLE: {
      response.set_status(CpSolverStatus::INFEASIBLE);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected SatSolver::Status " << status;
  }
  fill_response_statistics();
  return response;
}

// TODO(user): If this ever shows up in the profile, we could avoid copying
// the mapping_proto if we are careful about how we modify the variable domain
// before postsolving it.
void PostsolveResponse(const CpModelProto& model_proto,
                       CpModelProto mapping_proto,
                       const std::vector<int>& postsolve_mapping,
                       WallTimer* wall_timer, CpSolverResponse* response) {
  if (response->status() != CpSolverStatus::FEASIBLE &&
      response->status() != CpSolverStatus::OPTIMAL) {
    return;
  }
  // Postsolve.
  for (int i = 0; i < response->solution_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    var_proto->clear_domain();
    var_proto->add_domain(response->solution(i));
    var_proto->add_domain(response->solution(i));
  }
  for (int i = 0; i < response->solution_lower_bounds_size(); ++i) {
    auto* var_proto = mapping_proto.mutable_variables(postsolve_mapping[i]);
    FillDomainInProto(
        ReadDomainFromProto(*var_proto)
            .IntersectionWith({response->solution_lower_bounds(i),
                               response->solution_upper_bounds(i)}),
        var_proto);
  }

  // Postosolve parameters.
  // TODO(user): this problem is usually trivial, but we may still want to
  // impose a time limit or copy some of the parameters passed by the user.
  Model postsolve_model;
  {
    SatParameters params;
    params.set_linearization_level(0);
    postsolve_model.Add(operations_research::sat::NewSatParameters(params));
  }
  const CpSolverResponse postsolve_response = SolveCpModelInternal(
      mapping_proto, false, [](const CpSolverResponse&) {},
      /*shared_bounds_manager=*/nullptr, wall_timer, &postsolve_model);
  CHECK_EQ(postsolve_response.status(), CpSolverStatus::FEASIBLE);

  // We only copy the solution from the postsolve_response to the response.
  response->clear_solution();
  response->clear_solution_lower_bounds();
  response->clear_solution_upper_bounds();
  if (!postsolve_response.solution().empty()) {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response->add_solution(postsolve_response.solution(i));
    }
    CHECK(SolutionIsFeasible(model_proto,
                             std::vector<int64>(response->solution().begin(),
                                                response->solution().end())));
  } else {
    for (int i = 0; i < model_proto.variables_size(); ++i) {
      response->add_solution_lower_bounds(
          postsolve_response.solution_lower_bounds(i));
      response->add_solution_upper_bounds(
          postsolve_response.solution_upper_bounds(i));
    }
  }
}

CpSolverResponse SolvePureSatModel(const CpModelProto& model_proto,
                                   WallTimer* wall_timer, Model* model) {
  std::unique_ptr<SatSolver> solver(new SatSolver());
  SatParameters parameters = *model->GetOrCreate<SatParameters>();
  parameters.set_log_search_progress(true);
  solver->SetParameters(parameters);
  model->GetOrCreate<TimeLimit>()->ResetLimitFromParameters(parameters);

  // Create a DratProofHandler?
  std::unique_ptr<DratProofHandler> drat_proof_handler;
#if !defined(__PORTABLE_PLATFORM__)
  if (!FLAGS_drat_output.empty() || FLAGS_drat_check) {
    if (!FLAGS_drat_output.empty()) {
      File* output;
      CHECK_OK(file::Open(FLAGS_drat_output, "w", &output, file::Defaults()));
      drat_proof_handler = absl::make_unique<DratProofHandler>(
          /*in_binary_format=*/false, output, FLAGS_drat_check);
    } else {
      drat_proof_handler = absl::make_unique<DratProofHandler>();
    }
    solver->SetDratProofHandler(drat_proof_handler.get());
  }
#endif  // __PORTABLE_PLATFORM__

  auto get_literal = [](int ref) {
    if (ref >= 0) return Literal(BooleanVariable(ref), true);
    return Literal(BooleanVariable(NegatedRef(ref)), false);
  };

  std::vector<Literal> temp;
  const int num_variables = model_proto.variables_size();
  solver->SetNumVariables(num_variables);
  if (drat_proof_handler != nullptr) {
    drat_proof_handler->SetNumVariables(num_variables);
  }

  for (const ConstraintProto& ct : model_proto.constraints()) {
    switch (ct.constraint_case()) {
      case ConstraintProto::ConstraintCase::kBoolAnd: {
        if (ct.enforcement_literal_size() == 0) {
          for (const int ref : ct.bool_and().literals()) {
            const Literal b = get_literal(ref);
            solver->AddUnitClause(b);
          }
        } else {
          // a => b
          const Literal not_a =
              get_literal(ct.enforcement_literal(0)).Negated();
          for (const int ref : ct.bool_and().literals()) {
            const Literal b = get_literal(ref);
            solver->AddProblemClause({not_a, b});
            if (drat_proof_handler != nullptr) {
              drat_proof_handler->AddProblemClause({not_a, b});
            }
          }
        }
        break;
      }
      case ConstraintProto::ConstraintCase::kBoolOr:
        temp.clear();
        for (const int ref : ct.bool_or().literals()) {
          temp.push_back(get_literal(ref));
        }
        solver->AddProblemClause(temp);
        if (drat_proof_handler != nullptr) {
          drat_proof_handler->AddProblemClause(temp);
        }
        break;
      default:
        LOG(FATAL) << "Not supported";
    }
  }

  // Deal with fixed variables.
  for (int ref = 0; ref < num_variables; ++ref) {
    const Domain domain = ReadDomainFromProto(model_proto.variables(ref));
    if (domain.Min() == domain.Max()) {
      const Literal ref_literal =
          domain.Min() == 0 ? get_literal(ref).Negated() : get_literal(ref);
      solver->AddUnitClause(ref_literal);
      if (drat_proof_handler != nullptr) {
        drat_proof_handler->AddProblemClause({ref_literal});
      }
    }
  }

  SatSolver::Status status;
  CpSolverResponse response;
  if (parameters.cp_model_presolve()) {
    std::vector<bool> solution;
    status = SolveWithPresolve(&solver, model->GetOrCreate<TimeLimit>(),
                               &solution, drat_proof_handler.get());
    if (status == SatSolver::FEASIBLE) {
      response.clear_solution();
      for (int ref = 0; ref < num_variables; ++ref) {
        response.add_solution(solution[ref]);
      }
    }
  } else {
    status = solver->SolveWithTimeLimit(model->GetOrCreate<TimeLimit>());
    if (status == SatSolver::FEASIBLE) {
      response.clear_solution();
      for (int ref = 0; ref < num_variables; ++ref) {
        response.add_solution(
            solver->Assignment().LiteralIsTrue(get_literal(ref)) ? 1 : 0);
      }
    }
  }

  switch (status) {
    case SatSolver::LIMIT_REACHED: {
      response.set_status(CpSolverStatus::UNKNOWN);
      break;
    }
    case SatSolver::FEASIBLE: {
      CHECK(SolutionIsFeasible(model_proto,
                               std::vector<int64>(response.solution().begin(),
                                                  response.solution().end())));
      response.set_status(CpSolverStatus::FEASIBLE);
      break;
    }
    case SatSolver::INFEASIBLE: {
      response.set_status(CpSolverStatus::INFEASIBLE);
      break;
    }
    default:
      LOG(FATAL) << "Unexpected SatSolver::Status " << status;
  }
  response.set_num_booleans(solver->NumVariables());
  response.set_num_branches(solver->num_branches());
  response.set_num_conflicts(solver->num_failures());
  response.set_num_binary_propagations(solver->num_propagations());
  response.set_num_integer_propagations(0);
  response.set_wall_time(wall_timer->Get());
  response.set_deterministic_time(
      model->Get<TimeLimit>()->GetElapsedDeterministicTime());

  if (status == SatSolver::INFEASIBLE && drat_proof_handler != nullptr) {
    WallTimer drat_timer;
    drat_timer.Start();
    DratChecker::Status drat_status =
        drat_proof_handler->Check(FLAGS_max_drat_time_in_seconds);
    switch (drat_status) {
      case DratChecker::UNKNOWN:
        LOG(INFO) << "DRAT status: UNKNOWN";
        break;
      case DratChecker::VALID:
        LOG(INFO) << "DRAT status: VALID";
        break;
      case DratChecker::INVALID:
        LOG(ERROR) << "DRAT status: INVALID";
        break;
      default:
        // Should not happen.
        break;
    }
    LOG(INFO) << "DRAT wall time: " << drat_timer.Get();
  } else if (drat_proof_handler != nullptr) {
    // Always log a DRAT status to make it easier to extract it from a multirun
    // result with awk.
    LOG(INFO) << "DRAT status: NA";
    LOG(INFO) << "DRAT wall time: NA";
    LOG(INFO) << "DRAT user time: NA";
  }
  return response;
}

CpSolverResponse SolveCpModelWithLNS(
    const CpModelProto& model_proto,
    const std::function<void(const CpSolverResponse&)>& observer,
    int num_workers, int worker_id, WallTimer* wall_timer, Model* model) {
  SatParameters* parameters = model->GetOrCreate<SatParameters>();
  parameters->set_stop_after_first_solution(true);
  CpSolverResponse response;
  auto* synchro = model->Get<SynchronizationFunction>();
  if (synchro != nullptr && synchro->f != nullptr) {
    response = synchro->f();
  } else {
    response = SolveCpModelInternal(
        model_proto, /*is_real_solve=*/true, observer,
        /*shared_bounds_manager=*/nullptr, wall_timer, model);
  }
  if (response.status() != CpSolverStatus::FEASIBLE) {
    return response;
  }
  const bool focus_on_decision_variables =
      parameters->lns_focus_on_decision_variables();

  // TODO(user): Find a way to propagate the level zero bounds from the other
  // worker inside this base LNS problem.
  const NeighborhoodGeneratorHelper helper(&model_proto,
                                           focus_on_decision_variables);

  // For now we will just alternate between our possible neighborhoods.
  //
  // TODO(user): select with higher probability the one that seems to work best?
  // We can evaluate this compared to the best solution at the time of the
  // neighborhood creation.
  std::vector<std::unique_ptr<NeighborhoodGenerator>> generators;
  generators.push_back(
      absl::make_unique<SimpleNeighborhoodGenerator>(&helper, "rnd_lns"));
  generators.push_back(absl::make_unique<VariableGraphNeighborhoodGenerator>(
      &helper, "var_lns"));
  generators.push_back(absl::make_unique<ConstraintGraphNeighborhoodGenerator>(
      &helper, "cst_lns"));
  if (!helper.TypeToConstraints(ConstraintProto::kNoOverlap).empty()) {
    generators.push_back(
        absl::make_unique<SchedulingTimeWindowNeighborhoodGenerator>(
            &helper, "scheduling_time_window_lns"));
    generators.push_back(absl::make_unique<SchedulingNeighborhoodGenerator>(
        &helper, "scheduling_random_lns"));
  }
  if (parameters->use_rins_lns()) {
    // TODO(user): Only do it if we have a linear relaxation.
    generators.push_back(
        absl::make_unique<RelaxationInducedNeighborhoodGenerator>(
            &helper, *model, "rins"));
  }

  // The "optimal" difficulties do not have to be the same for different
  // generators.
  //
  // TODO(user): This should be shared across LNS threads! create a thread
  // safe class that accept signals of the form (difficulty and result) and
  // properly update the difficulties.
  std::vector<AdaptiveParameterValue> difficulties(generators.size(),
                                                   AdaptiveParameterValue(0.5));

  TimeLimit* limit = model->GetOrCreate<TimeLimit>();
  double deterministic_time = 0.1;
  int num_no_progress = 0;

  const int num_threads = std::max(1, parameters->lns_num_threads());
  OptimizeWithLNS(
      num_threads,
      [&]() {
        // Synchronize with external world.
        auto* synchro = model->Get<SynchronizationFunction>();
        if (synchro != nullptr && synchro->f != nullptr) {
          const CpSolverResponse candidate_response = synchro->f();
          if (!candidate_response.solution().empty()) {
            double coeff = model_proto.objective().scaling_factor();
            if (coeff == 0.0) coeff = 1.0;
            if (candidate_response.objective_value() * coeff <
                response.objective_value() * coeff) {
              response = candidate_response;
            }
          }
        }

        // If we didn't see any progress recently, bump the time limit.
        // TODO(user): Tune the logic and expose the parameters.
        if (num_no_progress > 100) {
          deterministic_time *= 1.1;
          num_no_progress = 0;
        }
        return limit->LimitReached() ||
               response.objective_value() == response.best_objective_bound();
      },
      [&](int64 seed) {
        AdaptiveParameterValue& difficulty =
            difficulties[seed % generators.size()];
        const double saved_difficulty = difficulty.value();
        const int selected_generator = seed % generators.size();
        Neighborhood neighborhood = generators[selected_generator]->Generate(
            response, num_workers * seed + worker_id, saved_difficulty);
        CpModelProto& local_problem = neighborhood.cp_model;
        const std::string solution_info = absl::StrFormat(
            "%s(d=%0.2f s=%i t=%0.2f)", generators[selected_generator]->name(),
            saved_difficulty, seed, deterministic_time);

#if !defined(__PORTABLE_PLATFORM__)
        if (FLAGS_cp_model_dump_lns_problems) {
          const int id = num_workers * seed + worker_id;
          const std::string name = absl::StrCat("/tmp/lns_", id, ".pb.txt");
          LOG(INFO) << "Dumping LNS model to '" << name << "'.";
          CHECK_OK(file::SetTextProto(name, local_problem, file::Defaults()));
        }
#endif  // __PORTABLE_PLATFORM__

        Model local_model;
        {
          SatParameters local_parameters;
          local_parameters = *parameters;
          local_parameters.set_max_deterministic_time(deterministic_time);
          local_parameters.set_stop_after_first_solution(false);
          local_model.Add(NewSatParameters(local_parameters));
        }
        local_model.GetOrCreate<TimeLimit>()->MergeWithGlobalTimeLimit(limit);

        // Presolve and solve the LNS fragment.
        CpSolverResponse local_response;
        {
          CpModelProto mapping_proto;
          std::vector<int> postsolve_mapping;
          PresolveOptions options;
          options.log_info = VLOG_IS_ON(2);
          options.parameters = *local_model.GetOrCreate<SatParameters>();
          options.time_limit = local_model.GetOrCreate<TimeLimit>();
          PresolveCpModel(options, &local_problem, &mapping_proto,
                          &postsolve_mapping);
          local_response = SolveCpModelInternal(
              local_problem, /*is_real_solve=*/true,
              [](const CpSolverResponse& response) {},
              /*shared_bounds_manager=*/nullptr, wall_timer, &local_model);
          PostsolveResponse(model_proto, mapping_proto, postsolve_mapping,
                            wall_timer, &local_response);
        }

        const bool neighborhood_is_reduced = neighborhood.is_reduced;
        return
            [neighborhood_is_reduced, &num_no_progress, &model_proto, &response,
             &difficulty, local_response, &observer, limit, solution_info]() {
              // TODO(user): This is not ideal in multithread because even
              // though the saved_difficulty will be the same for all thread, we
              // will Increase()/Decrease() the difficuty sequentially more than
              // once.
              if (local_response.status() == CpSolverStatus::OPTIMAL ||
                  local_response.status() == CpSolverStatus::INFEASIBLE) {
                if (neighborhood_is_reduced) {
                  difficulty.Increase();
                } else {
                  // We solved the full model here.
                  response = local_response;
                }
              } else {
                difficulty.Decrease();
              }
              if (local_response.status() == CpSolverStatus::FEASIBLE ||
                  local_response.status() == CpSolverStatus::OPTIMAL) {
                // If the objective are the same, we override the solution,
                // otherwise we just ignore this local solution and increment
                // num_no_progress.
                double coeff = model_proto.objective().scaling_factor();
                if (coeff == 0.0) coeff = 1.0;
                if (local_response.objective_value() * coeff >=
                    response.objective_value() * coeff) {
                  if (local_response.objective_value() * coeff >
                      response.objective_value() * coeff) {
                    return;
                  }
                  ++num_no_progress;
                } else {
                  num_no_progress = 0;
                }

                // Update the global response.
                *(response.mutable_solution()) = local_response.solution();
                response.set_objective_value(local_response.objective_value());
                response.set_wall_time(limit->GetElapsedTime());
                response.set_user_time(response.user_time() +
                                       local_response.user_time());
                response.set_deterministic_time(
                    response.deterministic_time() +
                    local_response.deterministic_time());
                if (DEBUG_MODE || FLAGS_cp_model_check_intermediate_solutions) {
                  CHECK(SolutionIsFeasible(
                      model_proto,
                      std::vector<int64>(local_response.solution().begin(),
                                         local_response.solution().end())));
                }
                if (num_no_progress == 0) {  // Improving solution.
                  response.set_solution_info(solution_info);
                  observer(response);
                }
              }
            };
      });

  if (response.status() == CpSolverStatus::FEASIBLE) {
    if (response.objective_value() == response.best_objective_bound()) {
      response.set_status(CpSolverStatus::OPTIMAL);
    }
  }

  return response;
}

#if !defined(__PORTABLE_PLATFORM__)

CpSolverResponse SolveCpModelParallel(
    const CpModelProto& model_proto,
    const std::function<void(const CpSolverResponse&)>& observer,
    WallTimer* wall_timer, Model* model) {
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  const int random_seed = params.random_seed();
  CHECK(!params.enumerate_all_solutions())
      << "Enumerating all solutions in parallel is not supported.";

  // This is a bit hacky. If the provided TimeLimit as a "stop" Boolean, we
  // use this one instead.
  std::atomic<bool> stopped_boolean(false);
  std::atomic<bool>* stopped = &stopped_boolean;
  if (model->GetOrCreate<TimeLimit>()->ExternalBooleanAsLimit() != nullptr) {
    stopped = model->GetOrCreate<TimeLimit>()->ExternalBooleanAsLimit();
  }

  const bool maximize = model_proto.objective().scaling_factor() < 0.0;

  CpSolverResponse best_response;
  if (model_proto.has_objective()) {
    const double kInfinity = std::numeric_limits<double>::infinity();
    if (maximize) {
      best_response.set_objective_value(-kInfinity);
      best_response.set_best_objective_bound(kInfinity);
    } else {
      best_response.set_objective_value(kInfinity);
      best_response.set_best_objective_bound(-kInfinity);
    }
  }

  absl::Mutex mutex;

  // In the LNS threads, we wait for this notification before starting work.
  absl::Notification first_solution_found_or_search_finished;

  const int num_search_workers = params.num_search_workers();

  // Collect per-worker parameters and names.
  std::vector<SatParameters> worker_parameters;
  std::vector<std::string> worker_names;
  for (int worker_id = 0; worker_id < num_search_workers; ++worker_id) {
    std::string worker_name;
    const SatParameters local_params =
        DiversifySearchParameters(params, model_proto, worker_id, &worker_name);
    worker_names.push_back(worker_name);
    worker_parameters.push_back(local_params);
  }

  VLOG(1) << absl::StrFormat(
      "*** starting parallel search at %.2fs with %i workers: [ %s ]",
      wall_timer->Get(), num_search_workers, absl::StrJoin(worker_names, ", "));

  if (!model_proto.has_objective()) {
    {
      ThreadPool pool("Parallel_search", num_search_workers);
      pool.StartWorkers();

      for (int worker_id = 0; worker_id < num_search_workers; ++worker_id) {
        const std::string worker_name = worker_names[worker_id];
        const SatParameters local_params = worker_parameters[worker_id];

        pool.Schedule([&model_proto, stopped, local_params, &best_response,
                       &mutex, worker_name, worker_id, wall_timer]() {
          Model local_model;
          local_model.Add(NewSatParameters(local_params));
          local_model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(
              stopped);

          // Stores info that will be used for logs in the local model.
          WorkerInfo* worker_info = local_model.GetOrCreate<WorkerInfo>();
          worker_info->worker_name = worker_name;
          worker_info->worker_id = worker_id;

          const CpSolverResponse local_response = SolveCpModelInternal(
              model_proto, true, [](const CpSolverResponse& response) {},
              /*shared_bounds_manager=*/nullptr, wall_timer, &local_model);

          absl::MutexLock lock(&mutex);
          if (best_response.status() == CpSolverStatus::UNKNOWN) {
            best_response = local_response;
          }
          if (local_response.status() != CpSolverStatus::UNKNOWN) {
            CHECK_EQ(local_response.status(), best_response.status());
            VLOG(1) << absl::StrFormat("#1      %6.2fs  %s", wall_timer->Get(),
                                       worker_name);
            *stopped = true;
          }
        });
      }
    }  // for the dtor of the threadpool (join workers) before returning.
    return best_response;
  }

  // Optimization problem.
  const auto get_objective_value = [&mutex, &best_response]() {
    absl::MutexLock lock(&mutex);
    return best_response.objective_value();
  };
  const auto get_objective_best_bound = [&mutex, &best_response]() {
    absl::MutexLock lock(&mutex);
    return best_response.best_objective_bound();
  };
  const auto set_objective_best_bound = [&mutex, maximize, &best_response](
                                            double objective_value,
                                            double objective_best_bound) {
    // Broadcast a best bound improving solution.
    absl::MutexLock lock(&mutex);
    CpSolverResponse lb_response;
    lb_response.set_status(CpSolverStatus::UNKNOWN);
    lb_response.set_objective_value(objective_value);
    lb_response.set_best_objective_bound(objective_best_bound);
    CHECK(!MergeOptimizationSolution(lb_response, maximize, &best_response));
  };
  const auto solution_synchronization = [&mutex, &best_response]() {
    absl::MutexLock lock(&mutex);
    return best_response;
  };

  std::unique_ptr<SharedBoundsManager> shared_bounds_manager;
  if (model->GetOrCreate<SatParameters>()->share_level_zero_bounds()) {
    shared_bounds_manager = absl::make_unique<SharedBoundsManager>(
        num_search_workers, model_proto.variables_size());
  }

  {
    ThreadPool pool("Parallel_search", num_search_workers);
    pool.StartWorkers();

    for (int worker_id = 0; worker_id < num_search_workers; ++worker_id) {
      const std::string worker_name = worker_names[worker_id];
      const SatParameters local_params = worker_parameters[worker_id];

      const auto solution_observer = [maximize, worker_name, &mutex,
                                      &best_response, &observer,
                                      &first_solution_found_or_search_finished](
                                         const CpSolverResponse& r) {
        absl::MutexLock lock(&mutex);

        // Check is the new solution is actually improving upon the best
        // solution found so far.
        if (MergeOptimizationSolution(r, maximize, &best_response)) {
          // Checks that r is not a pure best-bound improving solution.
          CHECK_EQ(r.status(), CpSolverStatus::FEASIBLE);

          best_response.set_solution_info(
              absl::StrCat(worker_name, " ", r.solution_info()));
          observer(best_response);

          // We have potentially displayed the improving solution, and updated
          // the best_response. We can awaken sleeping LNS threads.
          if (!first_solution_found_or_search_finished.HasBeenNotified()) {
            first_solution_found_or_search_finished.Notify();
          }
        }
      };

      pool.Schedule([&model_proto, solution_observer, solution_synchronization,
                     get_objective_value, get_objective_best_bound,
                     set_objective_best_bound, stopped, local_params, worker_id,
                     &mutex, &best_response, num_search_workers, random_seed,
                     wall_timer, &first_solution_found_or_search_finished,
                     &shared_bounds_manager, maximize, worker_name]() {
        Model local_model;
        local_model.Add(NewSatParameters(local_params));
        local_model.GetOrCreate<TimeLimit>()->RegisterExternalBooleanAsLimit(
            stopped);

        // Stores info that will be used for logs in the local model.
        WorkerInfo* worker_info = local_model.GetOrCreate<WorkerInfo>();
        worker_info->worker_name = worker_name;
        worker_info->worker_id = worker_id;

        SetSynchronizationFunction(std::move(solution_synchronization),
                                   &local_model);

        ObjectiveSynchronizationHelper* helper =
            local_model.GetOrCreate<ObjectiveSynchronizationHelper>();
        helper->get_external_best_objective = std::move(get_objective_value);
        helper->get_external_best_bound = std::move(get_objective_best_bound);
        if (local_model.GetOrCreate<SatParameters>()
                ->share_objective_bounds()) {
          helper->set_external_best_bound = std::move(set_objective_best_bound);
        }
        helper->parallel_mode = true;
        CpSolverResponse thread_response;
        if (local_params.use_lns()) {
          first_solution_found_or_search_finished.WaitForNotification();
          // TODO(user,user): Provide a better diversification for different
          // seeds.
          thread_response = SolveCpModelWithLNS(
              model_proto, solution_observer, num_search_workers,
              worker_id + random_seed, wall_timer, &local_model);
        } else {
          thread_response = SolveCpModelInternal(
              model_proto, true, solution_observer, shared_bounds_manager.get(),
              wall_timer, &local_model);
        }

        // Process final solution. Decide which worker has the 'best'
        // solution. Note that the solution observer may or may not have been
        // called.
        {
          absl::MutexLock lock(&mutex);
          VLOG(1) << "*** worker '" << worker_name
                  << "' terminates with status "
                  << ProtoEnumToString<CpSolverStatus>(thread_response.status())
                  << " and an objective value of "
                  << thread_response.objective_value();

          MergeOptimizationSolution(thread_response, maximize, &best_response);

          // TODO(user): For now we assume that each worker only terminate when
          // the time limit is reached or when the problem is solved, so we just
          // abort all other threads and return.
          *stopped = true;
          if (!first_solution_found_or_search_finished.HasBeenNotified()) {
            first_solution_found_or_search_finished.Notify();
          }
        }
      });
    }
  }  // for the dtor of the threadpool (join workers) before returning.
  return best_response;
}

#endif  // __PORTABLE_PLATFORM__

}  // namespace

CpSolverResponse SolveCpModel(const CpModelProto& model_proto, Model* model) {
  WallTimer wall_timer;
  UserTimer user_timer;
  wall_timer.Start();
  user_timer.Start();

  // Validate model_proto.
  // TODO(user): provide an option to skip this step for speed?
  {
    const std::string error = ValidateCpModel(model_proto);
    if (!error.empty()) {
      VLOG(1) << error;
      CpSolverResponse response;
      response.set_status(CpSolverStatus::MODEL_INVALID);
      return response;
    }
  }

#if !defined(__PORTABLE_PLATFORM__)
  // Dump?
  if (!FLAGS_cp_model_dump_file.empty()) {
    LOG(INFO) << "Dumping cp model proto to '" << FLAGS_cp_model_dump_file
              << "'.";
    CHECK_OK(file::SetTextProto(FLAGS_cp_model_dump_file, model_proto,
                                file::Defaults()));
  }

  // Override parameters?
  if (!FLAGS_cp_model_params.empty()) {
    SatParameters params = *model->GetOrCreate<SatParameters>();
    SatParameters flag_params;
    CHECK(google::protobuf::TextFormat::ParseFromString(FLAGS_cp_model_params,
                                                        &flag_params));
    params.MergeFrom(flag_params);
    model->Add(NewSatParameters(params));
    LOG(INFO) << "Parameters: " << params.ShortDebugString();
  }
#endif  // __PORTABLE_PLATFORM__

  // Special case for pure-sat problem.
  // TODO(user): improve the normal presolver to do the same thing.
  // TODO(user): Support solution hint, but then the first TODO will make it
  // automatic.
  const SatParameters& params = *model->GetOrCreate<SatParameters>();
  if (!model_proto.has_objective() && !model_proto.has_solution_hint() &&
      !params.enumerate_all_solutions() && !params.use_lns()) {
    bool is_pure_sat = true;
    for (const IntegerVariableProto& var : model_proto.variables()) {
      if (var.domain_size() != 2 || var.domain(0) < 0 || var.domain(1) > 1) {
        is_pure_sat = false;
        break;
      }
    }
    if (is_pure_sat) {
      for (const ConstraintProto& ct : model_proto.constraints()) {
        if (ct.constraint_case() != ConstraintProto::ConstraintCase::kBoolOr &&
            ct.constraint_case() != ConstraintProto::ConstraintCase::kBoolAnd) {
          is_pure_sat = false;
          break;
        }
      }
    }
    if (is_pure_sat) return SolvePureSatModel(model_proto, &wall_timer, model);
  }

  // Starts by expanding some constraints if needed.
  VLOG(1) << absl::StrFormat("*** starting model expansion at %.2fs",
                             wall_timer.Get());
  CpModelProto new_model = model_proto;  // Copy.
  PresolveOptions options;
  options.log_info = VLOG_IS_ON(1);
  ExpandCpModel(&new_model, options);

  // Presolve?
  std::function<void(CpSolverResponse * response)> postprocess_solution;
  if (params.cp_model_presolve()) {
    VLOG(1) << absl::StrFormat("*** starting model presolve at %.2fs",
                               wall_timer.Get());
    // Do the actual presolve.
    CpModelProto mapping_proto;
    std::vector<int> postsolve_mapping;
    PresolveOptions options;
    options.log_info = VLOG_IS_ON(1);
    options.parameters = *model->GetOrCreate<SatParameters>();
    options.time_limit = model->GetOrCreate<TimeLimit>();
    const bool ok = PresolveCpModel(options, &new_model, &mapping_proto,
                                    &postsolve_mapping);
    if (!ok) {
      LOG(ERROR) << "Error while presolving, likely due to integer overflow.";
      CpSolverResponse response;
      response.set_status(CpSolverStatus::MODEL_INVALID);
      return response;
    }
    VLOG(1) << CpModelStats(new_model);
    postprocess_solution = [&model_proto, mapping_proto, postsolve_mapping,
                            &wall_timer](CpSolverResponse* response) {
      // Note that it is okay to use the initial model_proto in the postsolve
      // even though we called PresolveCpModel() on the expanded proto. This is
      // because PostsolveResponse() only use the proto to known the number of
      // variables to fill in the response and to check the solution feasibility
      // of these variables.
      PostsolveResponse(model_proto, mapping_proto, postsolve_mapping,
                        &wall_timer, response);
    };
  } else {
    const int initial_size = model_proto.variables_size();
    postprocess_solution = [initial_size](CpSolverResponse* response) {
      // Truncate the solution in case model expansion added more variables.
      if (response->solution_size() > 0) {
        response->mutable_solution()->Truncate(initial_size);
      } else if (response->solution_lower_bounds_size() > 0) {
        response->mutable_solution_lower_bounds()->Truncate(initial_size);
        response->mutable_solution_upper_bounds()->Truncate(initial_size);
      }
    };
  }

  const auto& observers = model->GetOrCreate<SolutionObservers>()->observers;
  int num_solutions = 0;
  std::function<void(const CpSolverResponse&)> observer_function =
      [&model_proto, &observers, &num_solutions, &wall_timer, &user_timer,
       &postprocess_solution](const CpSolverResponse& response) {
        if (VLOG_IS_ON(1)) {
          if (model_proto.has_objective()) {
            const bool maximize =
                model_proto.objective().scaling_factor() < 0.0;
            LogNewSolution(absl::StrCat(++num_solutions), wall_timer.Get(),
                           maximize ? response.objective_value()
                                    : response.best_objective_bound(),
                           maximize ? response.best_objective_bound()
                                    : response.objective_value(),
                           response.solution_info());
          } else {
            LogNewSatSolution(absl::StrCat(++num_solutions), wall_timer.Get(),
                              response.solution_info());
          }
        }

        if (observers.empty()) return;
        CpSolverResponse copy = response;
        postprocess_solution(&copy);
        if (!copy.solution().empty()) {
          if (DEBUG_MODE || FLAGS_cp_model_check_intermediate_solutions) {
            CHECK(SolutionIsFeasible(
                model_proto, std::vector<int64>(copy.solution().begin(),
                                                copy.solution().end())));
          }
        }

        copy.set_wall_time(wall_timer.Get());
        copy.set_user_time(user_timer.Get());
        for (const auto& observer : observers) {
          observer(copy);
        }
      };

  CpSolverResponse response;

#if defined(__PORTABLE_PLATFORM__)
  if (/* DISABLES CODE */ (false)) {
    // We ignore the multithreading parameter in this case.
#else   // __PORTABLE_PLATFORM__
  if (params.num_search_workers() > 1) {
    response =
        SolveCpModelParallel(new_model, observer_function, &wall_timer, model);
#endif  // __PORTABLE_PLATFORM__
  } else if (params.use_lns() && new_model.has_objective() &&
             !params.enumerate_all_solutions()) {
    // TODO(user,user): Provide a better diversification for different
    // seeds.
    const int random_seed = model->GetOrCreate<SatParameters>()->random_seed();
    response = SolveCpModelWithLNS(new_model, observer_function, 1, random_seed,
                                   &wall_timer, model);
  } else {  // Normal sequential run.
    response = SolveCpModelInternal(
        new_model, /*is_real_solve=*/true, observer_function,
        /*shared_bounds_manager=*/nullptr, &wall_timer, model);
  }

  postprocess_solution(&response);
  if (!response.solution().empty()) {
    CHECK(SolutionIsFeasible(model_proto,
                             std::vector<int64>(response.solution().begin(),
                                                response.solution().end())));
  }

  // Fix the timing information before returning the response.
  response.set_wall_time(wall_timer.Get());
  response.set_user_time(user_timer.Get());
  return response;
}

}  // namespace sat
}  // namespace operations_research
