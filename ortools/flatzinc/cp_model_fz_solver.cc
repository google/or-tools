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

#include "ortools/flatzinc/cp_model_fz_solver.h"

#include <atomic>
#include <cmath>
#include <limits>
#include <tuple>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/map_util.h"
#include "ortools/base/threadpool.h"
#include "ortools/base/timer.h"
#include "ortools/flatzinc/checker.h"
#include "ortools/flatzinc/logging.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_search.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/disjunctive.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_expr.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/optimization.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/table.h"

DEFINE_bool(use_flatzinc_format, true, "Output uses the flatzinc format");

namespace operations_research {
namespace sat {

namespace {

// Returns the true/false literal corresponding to a CpModelProto variable.
int TrueLiteral(int var) { return var; }
int FalseLiteral(int var) { return -var - 1; }
int NegatedCpModelVariable(int var) { return -var - 1; }

// Helper class to convert a flatzinc model to a CpModelProto.
struct CpModelProtoWithMapping {
  // Returns a constant CpModelProto variable created on-demand.
  int LookupConstant(int64 value);

  // Convert a flatzinc argument to a variable or a list of variable.
  // Note that we always encode a constant argument with a constant variable.
  int LookupVar(const fz::Argument& argument);
  std::vector<int> LookupVars(const fz::Argument& argument);

  // Create and return the indices of the IntervalConstraint corresponding
  // to the flatzinc "interval" specified by a start var and a duration var.
  // This method will cache intervals with the key <start, duration>.
  std::vector<int> CreateIntervals(const std::vector<int>& starts,
                                   const std::vector<int>& durations);

  // Create and return the index of the IntervalConstraint corresponding
  // to the flatzinc "interval" specified by a start var and a size var.
  // This method will cache intervals with the key <start_var, size_var>.
  int GetOrCreateInterval(int start_var, int size_var);

  // Create and return the index of the optional IntervalConstraint
  // corresponding to the flatzinc "interval" specified by a start var, the
  // size_var, and the Boolean opt_var. This method will cache intervals with
  // the key <start, duration, opt_var>.
  int GetOrCreateOptionalInterval(int start_var, int size_var, int opt_var);

  // Helpers to fill a ConstraintProto.
  void FillAMinusBInDomain(const std::vector<int64>& domain,
                           const fz::Constraint& fz_ct, ConstraintProto* ct);
  void FillLinearConstraintWithGivenDomain(const std::vector<int64>& domain,
                                           const fz::Constraint& fz_ct,
                                           ConstraintProto* ct);
  void FillConstraint(const fz::Constraint& fz_ct, ConstraintProto* ct);
  void FillReifOrImpliedConstraint(const fz::Constraint& fz_ct,
                                   ConstraintProto* ct);

  // Translates the flatzinc search annotations into the CpModelProto
  // search_order field.
  void TranslateSearchAnnotations(
      const std::vector<fz::Annotation>& search_annotations);

  // The output proto.
  CpModelProto proto;
  SatParameters parameters;

  // Mapping from flatzinc variables to CpModelProto variables.
  absl::flat_hash_map<fz::IntegerVariable*, int> fz_var_to_index;
  absl::flat_hash_map<int64, int> constant_value_to_index;
  absl::flat_hash_map<std::tuple<int, int, int>, int>
      start_size_opt_tuple_to_interval;
};

int CpModelProtoWithMapping::LookupConstant(int64 value) {
  if (gtl::ContainsKey(constant_value_to_index, value)) {
    return constant_value_to_index[value];
  }

  // Create the constant on the fly.
  const int index = proto.variables_size();
  IntegerVariableProto* var_proto = proto.add_variables();
  var_proto->add_domain(value);
  var_proto->add_domain(value);
  constant_value_to_index[value] = index;
  return index;
}

int CpModelProtoWithMapping::LookupVar(const fz::Argument& argument) {
  if (argument.HasOneValue()) return LookupConstant(argument.Value());
  CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF);
  return fz_var_to_index[argument.Var()];
}

std::vector<int> CpModelProtoWithMapping::LookupVars(
    const fz::Argument& argument) {
  std::vector<int> result;
  if (argument.type == fz::Argument::VOID_ARGUMENT) return result;
  if (argument.type == fz::Argument::INT_LIST) {
    for (int64 value : argument.values) {
      result.push_back(LookupConstant(value));
    }
  } else {
    CHECK_EQ(argument.type, fz::Argument::INT_VAR_REF_ARRAY);
    for (fz::IntegerVariable* var : argument.variables) {
      CHECK(var != nullptr);
      result.push_back(fz_var_to_index[var]);
    }
  }
  return result;
}

int CpModelProtoWithMapping::GetOrCreateInterval(int start_var, int size_var) {
  return GetOrCreateOptionalInterval(start_var, size_var, kint32max);
}

int CpModelProtoWithMapping::GetOrCreateOptionalInterval(int start_var,
                                                         int size_var,
                                                         int opt_var) {
  const std::tuple<int, int, int> key =
      std::make_tuple(start_var, size_var, opt_var);
  if (gtl::ContainsKey(start_size_opt_tuple_to_interval, key)) {
    return start_size_opt_tuple_to_interval[key];
  }
  const int interval_index = proto.constraints_size();

  auto* ct = proto.add_constraints();
  if (opt_var != kint32max) {
    ct->add_enforcement_literal(opt_var);
  }
  auto* interval = ct->mutable_interval();
  interval->set_start(start_var);
  interval->set_size(size_var);

  interval->set_end(proto.variables_size());
  auto* end_var = proto.add_variables();
  const auto start_proto = proto.variables(start_var);
  const auto size_proto = proto.variables(size_var);
  end_var->add_domain(start_proto.domain(0) + size_proto.domain(0));
  end_var->add_domain(start_proto.domain(start_proto.domain_size() - 1) +
                      size_proto.domain(size_proto.domain_size() - 1));
  start_size_opt_tuple_to_interval[key] = interval_index;
  return interval_index;
}

std::vector<int> CpModelProtoWithMapping::CreateIntervals(
    const std::vector<int>& starts, const std::vector<int>& durations) {
  std::vector<int> intervals;
  for (int i = 0; i < starts.size(); ++i) {
    intervals.push_back(GetOrCreateInterval(starts[i], durations[i]));
  }
  return intervals;
}

void CpModelProtoWithMapping::FillAMinusBInDomain(
    const std::vector<int64>& domain, const fz::Constraint& fz_ct,
    ConstraintProto* ct) {
  auto* arg = ct->mutable_linear();
  for (const int64 domain_bound : domain) arg->add_domain(domain_bound);
  arg->add_vars(LookupVar(fz_ct.arguments[0]));
  arg->add_coeffs(1);
  arg->add_vars(LookupVar(fz_ct.arguments[1]));
  arg->add_coeffs(-1);
}

void CpModelProtoWithMapping::FillLinearConstraintWithGivenDomain(
    const std::vector<int64>& domain, const fz::Constraint& fz_ct,
    ConstraintProto* ct) {
  auto* arg = ct->mutable_linear();
  for (const int64 domain_bound : domain) arg->add_domain(domain_bound);
  std::vector<int> vars = LookupVars(fz_ct.arguments[1]);
  for (int i = 0; i < vars.size(); ++i) {
    arg->add_vars(vars[i]);
    arg->add_coeffs(fz_ct.arguments[0].values[i]);
  }
}

void CpModelProtoWithMapping::FillConstraint(const fz::Constraint& fz_ct,
                                             ConstraintProto* ct) {
  if (fz_ct.type == "false_constraint") {
    // An empty clause is always false.
    ct->mutable_bool_or();
  } else if (fz_ct.type == "bool_clause") {
    auto* arg = ct->mutable_bool_or();
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      arg->add_literals(TrueLiteral(var));
    }
    for (const int var : LookupVars(fz_ct.arguments[1])) {
      arg->add_literals(FalseLiteral(var));
    }
  } else if (fz_ct.type == "bool_xor") {
    auto* arg = ct->mutable_bool_xor();
    arg->add_literals(TrueLiteral(LookupVar(fz_ct.arguments[0])));
    arg->add_literals(TrueLiteral(LookupVar(fz_ct.arguments[1])));
  } else if (fz_ct.type == "array_bool_or") {
    auto* arg = ct->mutable_bool_or();
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      arg->add_literals(TrueLiteral(var));
    }
  } else if (fz_ct.type == "array_bool_or_negated") {
    auto* arg = ct->mutable_bool_and();
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      arg->add_literals(FalseLiteral(var));
    }
  } else if (fz_ct.type == "array_bool_and") {
    auto* arg = ct->mutable_bool_and();
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      arg->add_literals(TrueLiteral(var));
    }
  } else if (fz_ct.type == "array_bool_and_negated") {
    auto* arg = ct->mutable_bool_or();
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      arg->add_literals(FalseLiteral(var));
    }
  } else if (fz_ct.type == "array_bool_xor") {
    auto* arg = ct->mutable_bool_xor();
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      arg->add_literals(TrueLiteral(var));
    }
  } else if (fz_ct.type == "bool_le" || fz_ct.type == "int_le") {
    FillAMinusBInDomain({kint64min, 0}, fz_ct, ct);
  } else if (fz_ct.type == "bool_ge" || fz_ct.type == "int_ge") {
    FillAMinusBInDomain({0, kint64max}, fz_ct, ct);
  } else if (fz_ct.type == "bool_lt" || fz_ct.type == "int_lt") {
    FillAMinusBInDomain({kint64min, -1}, fz_ct, ct);
  } else if (fz_ct.type == "bool_gt" || fz_ct.type == "int_gt") {
    FillAMinusBInDomain({1, kint64max}, fz_ct, ct);
  } else if (fz_ct.type == "bool_eq" || fz_ct.type == "int_eq" ||
             fz_ct.type == "bool2int") {
    FillAMinusBInDomain({0, 0}, fz_ct, ct);
  } else if (fz_ct.type == "bool_ne" || fz_ct.type == "bool_not") {
    auto* arg = ct->mutable_linear();
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_coeffs(1);
    arg->add_vars(LookupVar(fz_ct.arguments[1]));
    arg->add_coeffs(1);
    arg->add_domain(1);
    arg->add_domain(1);
  } else if (fz_ct.type == "int_ne") {
    FillAMinusBInDomain({kint64min, -1, 1, kint64max}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_eq") {
    const int64 rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain({rhs, rhs}, fz_ct, ct);
  } else if (fz_ct.type == "bool_lin_eq") {
    auto* arg = ct->mutable_linear();
    const std::vector<int> vars = LookupVars(fz_ct.arguments[1]);
    for (int i = 0; i < vars.size(); ++i) {
      arg->add_vars(vars[i]);
      arg->add_coeffs(fz_ct.arguments[0].values[i]);
    }
    if (fz_ct.arguments[2].IsVariable()) {
      arg->add_vars(LookupVar(fz_ct.arguments[2]));
      arg->add_coeffs(-1);
      arg->add_domain(0);
      arg->add_domain(0);
    } else {
      const int64 v = fz_ct.arguments[2].Value();
      arg->add_domain(v);
      arg->add_domain(v);
    }
  } else if (fz_ct.type == "int_lin_le" || fz_ct.type == "bool_lin_le") {
    const int64 rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain({kint64min, rhs}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_lt") {
    const int64 rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain({kint64min, rhs - 1}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_ge") {
    const int64 rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain({rhs, kint64max}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_gt") {
    const int64 rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain({rhs + 1, kint64max}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_ne") {
    const int64 rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain(
        {kint64min, rhs - 1, rhs + 1, kint64max}, fz_ct, ct);
  } else if (fz_ct.type == "set_in") {
    auto* arg = ct->mutable_linear();
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_coeffs(1);
    if (fz_ct.arguments[1].type == fz::Argument::INT_LIST) {
      FillDomainInProto(Domain::FromValues(std::vector<int64>{
                            fz_ct.arguments[1].values.begin(),
                            fz_ct.arguments[1].values.end()}),
                        arg);
    } else if (fz_ct.arguments[1].type == fz::Argument::INT_INTERVAL) {
      FillDomainInProto(
          Domain(fz_ct.arguments[1].values[0], fz_ct.arguments[1].values[1]),
          arg);
    } else {
      LOG(FATAL) << "Wrong format";
    }
  } else if (fz_ct.type == "set_in_negated") {
    auto* arg = ct->mutable_linear();
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_coeffs(1);
    if (fz_ct.arguments[1].type == fz::Argument::INT_LIST) {
      FillDomainInProto(
          Domain::FromValues(
              std::vector<int64>{fz_ct.arguments[1].values.begin(),
                                 fz_ct.arguments[1].values.end()})
              .Complement(),
          arg);
    } else if (fz_ct.arguments[1].type == fz::Argument::INT_INTERVAL) {
      FillDomainInProto(
          Domain(fz_ct.arguments[1].values[0], fz_ct.arguments[1].values[1])
              .Complement(),
          arg);
    } else {
      LOG(FATAL) << "Wrong format";
    }
  } else if (fz_ct.type == "int_min") {
    auto* arg = ct->mutable_int_min();
    arg->set_target(LookupVar(fz_ct.arguments[2]));
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_vars(LookupVar(fz_ct.arguments[1]));
  } else if (fz_ct.type == "array_int_minimum" || fz_ct.type == "minimum_int") {
    auto* arg = ct->mutable_int_min();
    arg->set_target(LookupVar(fz_ct.arguments[0]));
    for (const int var : LookupVars(fz_ct.arguments[1])) arg->add_vars(var);
  } else if (fz_ct.type == "int_max") {
    auto* arg = ct->mutable_int_max();
    arg->set_target(LookupVar(fz_ct.arguments[2]));
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_vars(LookupVar(fz_ct.arguments[1]));
  } else if (fz_ct.type == "array_int_maximum" || fz_ct.type == "maximum_int") {
    auto* arg = ct->mutable_int_max();
    arg->set_target(LookupVar(fz_ct.arguments[0]));
    for (const int var : LookupVars(fz_ct.arguments[1])) arg->add_vars(var);
  } else if (fz_ct.type == "int_times") {
    auto* arg = ct->mutable_int_prod();
    arg->set_target(LookupVar(fz_ct.arguments[2]));
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_vars(LookupVar(fz_ct.arguments[1]));
  } else if (fz_ct.type == "int_abs") {
    auto* arg = ct->mutable_int_max();
    arg->set_target(LookupVar(fz_ct.arguments[1]));
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_vars(-LookupVar(fz_ct.arguments[0]) - 1);
  } else if (fz_ct.type == "int_plus") {
    auto* arg = ct->mutable_linear();
    FillDomainInProto(Domain(0, 0), arg);
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_coeffs(1);
    arg->add_vars(LookupVar(fz_ct.arguments[1]));
    arg->add_coeffs(1);
    arg->add_vars(LookupVar(fz_ct.arguments[2]));
    arg->add_coeffs(-1);
  } else if (fz_ct.type == "int_div") {
    auto* arg = ct->mutable_int_div();
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_vars(LookupVar(fz_ct.arguments[1]));
    arg->set_target(LookupVar(fz_ct.arguments[2]));
  } else if (fz_ct.type == "int_mod") {
    auto* arg = ct->mutable_int_mod();
    arg->set_target(LookupVar(fz_ct.arguments[2]));
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_vars(LookupVar(fz_ct.arguments[1]));
  } else if (fz_ct.type == "array_int_element" ||
             fz_ct.type == "array_bool_element" ||
             fz_ct.type == "array_var_int_element" ||
             fz_ct.type == "array_var_bool_element" ||
             fz_ct.type == "array_int_element_nonshifted") {
    if (fz_ct.arguments[0].type == fz::Argument::INT_VAR_REF) {
      auto* arg = ct->mutable_element();
      arg->set_index(LookupVar(fz_ct.arguments[0]));
      arg->set_target(LookupVar(fz_ct.arguments[2]));

      if (!absl::EndsWith(fz_ct.type, "_nonshifted")) {
        // Add a dummy variable at position zero because flatzinc index start at
        // 1.
        // TODO(user): Make sure that zero is not in the index domain...
        arg->add_vars(arg->target());
      }
      for (const int var : LookupVars(fz_ct.arguments[1])) arg->add_vars(var);
    } else {
      // Special case added by the presolve or in flatzinc. We encode this
      // as a table constraint.
      CHECK(!absl::EndsWith(fz_ct.type, "_nonshifted"));
      auto* arg = ct->mutable_table();

      // the constraint is:
      //   values[coeff1 * vars[0] + coeff2 * vars[1] + offset] == target.
      for (const int var : LookupVars(fz_ct.arguments[0])) arg->add_vars(var);
      arg->add_vars(LookupVar(fz_ct.arguments[2]));  // the target

      const std::vector<int64>& values = fz_ct.arguments[1].values;
      const int64 coeff1 = fz_ct.arguments[3].values[0];
      const int64 coeff2 = fz_ct.arguments[3].values[1];
      const int64 offset = fz_ct.arguments[4].values[0] - 1;

      for (const int64 a : AllValuesInDomain(proto.variables(arg->vars(0)))) {
        for (const int64 b : AllValuesInDomain(proto.variables(arg->vars(1)))) {
          const int index = coeff1 * a + coeff2 * b + offset;
          CHECK_GE(index, 0);
          CHECK_LT(index, values.size());
          arg->add_values(a);
          arg->add_values(b);
          arg->add_values(values[index]);
        }
      }
    }
  } else if (fz_ct.type == "table_int") {
    auto* arg = ct->mutable_table();
    for (const int var : LookupVars(fz_ct.arguments[0])) arg->add_vars(var);
    for (const int64 value : fz_ct.arguments[1].values) arg->add_values(value);
  } else if (fz_ct.type == "regular") {
    auto* arg = ct->mutable_automaton();
    for (const int var : LookupVars(fz_ct.arguments[0])) arg->add_vars(var);

    int count = 0;
    const int num_states = fz_ct.arguments[1].Value();
    const int num_values = fz_ct.arguments[2].Value();
    for (int i = 1; i <= num_states; ++i) {
      for (int j = 1; j <= num_values; ++j) {
        CHECK_LT(count, fz_ct.arguments[3].values.size());
        const int next = fz_ct.arguments[3].values[count++];
        if (next == 0) continue;  // 0 is a failing state.
        arg->add_transition_tail(i);
        arg->add_transition_label(j);
        arg->add_transition_head(next);
      }
    }

    arg->set_starting_state(fz_ct.arguments[4].Value());
    switch (fz_ct.arguments[5].type) {
      case fz::Argument::INT_VALUE: {
        arg->add_final_states(fz_ct.arguments[5].values[0]);
        break;
      }
      case fz::Argument::INT_INTERVAL: {
        for (int v = fz_ct.arguments[5].values[0];
             v <= fz_ct.arguments[5].values[1]; ++v) {
          arg->add_final_states(v);
        }
        break;
      }
      case fz::Argument::INT_LIST: {
        for (const int v : fz_ct.arguments[5].values) {
          arg->add_final_states(v);
        }
        break;
      }
      default: {
        LOG(FATAL) << "Wrong constraint " << fz_ct.DebugString();
      }
    }
  } else if (fz_ct.type == "all_different_int") {
    auto* arg = ct->mutable_all_diff();
    for (const int var : LookupVars(fz_ct.arguments[0])) arg->add_vars(var);
  } else if (fz_ct.type == "circuit" || fz_ct.type == "subcircuit") {
    // Try to auto-detect if it is zero or one based.
    bool found_zero = false;
    bool found_size = false;
    const int size = fz_ct.arguments[0].variables.size();
    for (fz::IntegerVariable* const var : fz_ct.arguments[0].variables) {
      if (var->domain.Min() == 0) found_zero = true;
      if (var->domain.Max() == size) found_size = true;
    }
    const bool is_one_based = !found_zero || found_size;
    const int min_index = is_one_based ? 1 : 0;
    const int max_index = min_index + fz_ct.arguments[0].variables.size() - 1;

    // The arc-based mutable circuit.
    auto* circuit_arg = ct->mutable_circuit();

    // We fully encode all variables so we can use the literal based circuit.
    // TODO(user): avoid fully encoding more than once?
    int index = min_index;
    const bool is_circuit = (fz_ct.type == "circuit");
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      Domain domain = ReadDomainFromProto(proto.variables(var));

      // Restrict the domain of var to [min_index, max_index]
      domain = domain.IntersectionWith(Domain(min_index, max_index));
      if (is_circuit) {
        // We simply make sure that the variable cannot take the value index.
        domain = domain.IntersectionWith(Domain::FromIntervals(
            {{kint64min, index - 1}, {index + 1, kint64max}}));
      }
      FillDomainInProto(domain, proto.mutable_variables(var));

      for (const ClosedInterval interval : domain.intervals()) {
        for (int64 value = interval.start; value <= interval.end; ++value) {
          // Create one Boolean variable for this arc.
          const int literal = proto.variables_size();
          {
            auto* new_var = proto.add_variables();
            new_var->add_domain(0);
            new_var->add_domain(1);
          }

          // Add the arc.
          circuit_arg->add_tails(index);
          circuit_arg->add_heads(value);
          circuit_arg->add_literals(literal);

          // literal => var == value.
          {
            auto* ct = proto.add_constraints();
            ct->add_enforcement_literal(literal);
            ct->mutable_linear()->add_coeffs(1);
            ct->mutable_linear()->add_vars(var);
            ct->mutable_linear()->add_domain(value);
            ct->mutable_linear()->add_domain(value);
          }

          // not(literal) => var != value
          {
            auto* ct = proto.add_constraints();
            ct->add_enforcement_literal(NegatedRef(literal));
            ct->mutable_linear()->add_coeffs(1);
            ct->mutable_linear()->add_vars(var);
            ct->mutable_linear()->add_domain(kint64min);
            ct->mutable_linear()->add_domain(value - 1);
            ct->mutable_linear()->add_domain(value + 1);
            ct->mutable_linear()->add_domain(kint64max);
          }
        }
      }

      ++index;
    }
  } else if (fz_ct.type == "inverse") {
    auto* arg = ct->mutable_inverse();

    const auto direct_variables = LookupVars(fz_ct.arguments[0]);
    const auto inverse_variables = LookupVars(fz_ct.arguments[1]);

    const int num_variables =
        std::min(direct_variables.size(), inverse_variables.size());

    // Try to auto-detect if it is zero or one based.
    bool found_zero = false;
    bool found_size = false;
    for (fz::IntegerVariable* const var : fz_ct.arguments[0].variables) {
      if (var->domain.Min() == 0) found_zero = true;
      if (var->domain.Max() == num_variables) found_size = true;
    }
    for (fz::IntegerVariable* const var : fz_ct.arguments[1].variables) {
      if (var->domain.Min() == 0) found_zero = true;
      if (var->domain.Max() == num_variables) found_size = true;
    }

    // Add a dummy constant variable at zero if the indexing is one based.
    const bool is_one_based = !found_zero || found_size;
    const int offset = is_one_based ? 1 : 0;

    if (is_one_based) arg->add_f_direct(LookupConstant(0));
    for (const int var : direct_variables) {
      arg->add_f_direct(var);
      // Intersect domains with offset + [0, num_variables).
      FillDomainInProto(
          ReadDomainFromProto(proto.variables(var))
              .IntersectionWith(Domain(offset, num_variables - 1 + offset)),
          proto.mutable_variables(var));
    }

    if (is_one_based) arg->add_f_inverse(LookupConstant(0));
    for (const int var : inverse_variables) {
      arg->add_f_inverse(var);
      // Intersect domains with offset + [0, num_variables).
      FillDomainInProto(
          ReadDomainFromProto(proto.variables(var))
              .IntersectionWith(Domain(offset, num_variables - 1 + offset)),
          proto.mutable_variables(var));
    }
  } else if (fz_ct.type == "cumulative") {
    const std::vector<int> starts = LookupVars(fz_ct.arguments[0]);
    const std::vector<int> durations = LookupVars(fz_ct.arguments[1]);
    const std::vector<int> demands = LookupVars(fz_ct.arguments[2]);
    const int capacity = LookupVar(fz_ct.arguments[3]);

    auto* arg = ct->mutable_cumulative();
    arg->set_capacity(capacity);
    for (int i = 0; i < starts.size(); ++i) {
      // Special case for a 0-1 demand, we mark the interval as optional instead
      // and fix the demand to 1.
      if (proto.variables(demands[i]).domain().size() == 2 &&
          proto.variables(demands[i]).domain(0) == 0 &&
          proto.variables(demands[i]).domain(1) == 1 &&
          proto.variables(capacity).domain(1) == 1) {
        arg->add_intervals(
            GetOrCreateOptionalInterval(starts[i], durations[i], demands[i]));
        arg->add_demands(LookupConstant(1));
      } else {
        arg->add_intervals(GetOrCreateInterval(starts[i], durations[i]));
        arg->add_demands(demands[i]);
      }
    }
  } else if (fz_ct.type == "diffn") {
    const std::vector<int> x = LookupVars(fz_ct.arguments[0]);
    const std::vector<int> y = LookupVars(fz_ct.arguments[1]);
    const std::vector<int> dx = LookupVars(fz_ct.arguments[2]);
    const std::vector<int> dy = LookupVars(fz_ct.arguments[3]);
    const std::vector<int> x_intervals = CreateIntervals(x, dx);
    const std::vector<int> y_intervals = CreateIntervals(y, dy);
    auto* arg = ct->mutable_no_overlap_2d();
    for (int i = 0; i < x.size(); ++i) {
      arg->add_x_intervals(x_intervals[i]);
      arg->add_y_intervals(y_intervals[i]);
    }
  } else if (fz_ct.type == "network_flow" ||
             fz_ct.type == "network_flow_cost") {
    // Note that we leave ct empty here (with just the name set).
    // We simply do a linear encoding of this constraint.
    const bool has_cost = fz_ct.type == "network_flow_cost";
    const std::vector<int> flow = LookupVars(fz_ct.arguments[has_cost ? 3 : 2]);

    // Flow conservation constraints.
    const int num_nodes = fz_ct.arguments[1].values.size();
    std::vector<std::vector<int>> flows_per_node(num_nodes);
    std::vector<std::vector<int>> coeffs_per_node(num_nodes);
    const int num_arcs = fz_ct.arguments[0].values.size() / 2;
    for (int arc = 0; arc < num_arcs; arc++) {
      const int tail = fz_ct.arguments[0].values[2 * arc] - 1;
      const int head = fz_ct.arguments[0].values[2 * arc + 1] - 1;
      if (tail == head) continue;

      flows_per_node[tail].push_back(flow[arc]);
      coeffs_per_node[tail].push_back(1);
      flows_per_node[head].push_back(flow[arc]);
      coeffs_per_node[head].push_back(-1);
    }
    for (int node = 0; node < num_nodes; node++) {
      auto* arg = proto.add_constraints()->mutable_linear();
      arg->add_domain(fz_ct.arguments[1].values[node]);
      arg->add_domain(fz_ct.arguments[1].values[node]);
      for (int i = 0; i < flows_per_node[node].size(); ++i) {
        arg->add_vars(flows_per_node[node][i]);
        arg->add_coeffs(coeffs_per_node[node][i]);
      }
    }

    if (has_cost) {
      auto* arg = proto.add_constraints()->mutable_linear();
      arg->add_domain(0);
      arg->add_domain(0);
      for (int arc = 0; arc < num_arcs; arc++) {
        const int64 weight = fz_ct.arguments[2].values[arc];
        if (weight != 0) {
          arg->add_vars(flow[arc]);
          arg->add_coeffs(weight);
        }
      }
      arg->add_vars(LookupVar(fz_ct.arguments[4]));
      arg->add_coeffs(-1);
    }
  } else {
    LOG(FATAL) << " Not supported " << fz_ct.type;
  }
}

void CpModelProtoWithMapping::FillReifOrImpliedConstraint(
    const fz::Constraint& fz_ct, ConstraintProto* ct) {
  // Start by adding a non-reified version of the same constraint.
  const bool is_implication = absl::EndsWith(fz_ct.type, "_imp");

  std::string simplified_type;
  if (absl::EndsWith(fz_ct.type, "_reif")) {
    // Remove _reif.
    simplified_type = fz_ct.type.substr(0, fz_ct.type.size() - 5);
  } else if (absl::EndsWith(fz_ct.type, "_imp")) {
    // Remove _imp.
    simplified_type = fz_ct.type.substr(0, fz_ct.type.size() - 4);
  } else {
    // Keep name as it is an implicit reified constraint.
    simplified_type = fz_ct.type;
  }

  // We need a copy to be able to change the type of the constraint.
  fz::Constraint copy = fz_ct;
  copy.type = simplified_type;

  // Create the CP-SAT constraint.
  FillConstraint(copy, ct);

  // In case of reified constraints, the type of the opposite constraint.
  std::string negated_type;

  // Fill enforcement_literal and set copy.type to the negated constraint.
  if (simplified_type == "array_bool_or") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[1])));
    negated_type = "array_bool_or_negated";
  } else if (simplified_type == "array_bool_and") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[1])));
    negated_type = "array_bool_and_negated";
  } else if (simplified_type == "set_in") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[2])));
    negated_type = "set_in_negated";
  } else if (simplified_type == "bool_eq" || simplified_type == "int_eq") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[2])));
    negated_type = "int_ne";
  } else if (simplified_type == "bool_ne" || simplified_type == "int_ne") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[2])));
    negated_type = "int_eq";
  } else if (simplified_type == "bool_le" || simplified_type == "int_le") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[2])));
    negated_type = "int_gt";
  } else if (simplified_type == "bool_lt" || simplified_type == "int_lt") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[2])));
    negated_type = "int_ge";
  } else if (simplified_type == "bool_ge" || simplified_type == "int_ge") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[2])));
    negated_type = "int_lt";
  } else if (simplified_type == "bool_gt" || simplified_type == "int_gt") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[2])));
    negated_type = "int_le";
  } else if (simplified_type == "int_lin_eq") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[3])));
    negated_type = "int_lin_ne";
  } else if (simplified_type == "int_lin_ne") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[3])));
    negated_type = "int_lin_eq";
  } else if (simplified_type == "int_lin_le") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[3])));
    negated_type = "int_lin_gt";
  } else if (simplified_type == "int_lin_ge") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[3])));
    negated_type = "int_lin_lt";
  } else if (simplified_type == "int_lin_lt") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[3])));
    negated_type = "int_lin_ge";
  } else if (simplified_type == "int_lin_gt") {
    ct->add_enforcement_literal(TrueLiteral(LookupVar(fz_ct.arguments[3])));
    negated_type = "int_lin_le";
  } else {
    LOG(FATAL) << "Unsupported " << simplified_type;
  }

  // One way implication. We can stop here.
  if (is_implication) return;

  // Add the other side of the reification because CpModelProto only support
  // half reification.
  ConstraintProto* negated_ct = proto.add_constraints();
  negated_ct->set_name(fz_ct.type + " (negated)");
  negated_ct->add_enforcement_literal(
      sat::NegatedRef(ct->enforcement_literal(0)));
  copy.type = negated_type;
  FillConstraint(copy, negated_ct);
}

void CpModelProtoWithMapping::TranslateSearchAnnotations(
    const std::vector<fz::Annotation>& search_annotations) {
  std::vector<fz::Annotation> flat_annotations;
  for (const fz::Annotation& annotation : search_annotations) {
    fz::FlattenAnnotations(annotation, &flat_annotations);
  }

  for (const fz::Annotation& annotation : flat_annotations) {
    if (annotation.IsFunctionCallWithIdentifier("int_search") ||
        annotation.IsFunctionCallWithIdentifier("bool_search")) {
      const std::vector<fz::Annotation>& args = annotation.annotations;
      std::vector<fz::IntegerVariable*> vars;
      args[0].AppendAllIntegerVariables(&vars);

      DecisionStrategyProto* strategy = proto.add_search_strategy();
      for (fz::IntegerVariable* v : vars) {
        strategy->add_variables(gtl::FindOrDie(fz_var_to_index, v));
      }

      const fz::Annotation& choose = args[1];
      if (choose.id == "input_order") {
        strategy->set_variable_selection_strategy(
            DecisionStrategyProto::CHOOSE_FIRST);
      } else if (choose.id == "first_fail") {
        strategy->set_variable_selection_strategy(
            DecisionStrategyProto::CHOOSE_MIN_DOMAIN_SIZE);
      } else if (choose.id == "anti_first_fail") {
        strategy->set_variable_selection_strategy(
            DecisionStrategyProto::CHOOSE_MAX_DOMAIN_SIZE);
      } else if (choose.id == "smallest") {
        strategy->set_variable_selection_strategy(
            DecisionStrategyProto::CHOOSE_LOWEST_MIN);
      } else if (choose.id == "largest") {
        strategy->set_variable_selection_strategy(
            DecisionStrategyProto::CHOOSE_HIGHEST_MAX);
      } else {
        LOG(FATAL) << "Unsupported order: " << choose.id;
      }

      const fz::Annotation& select = args[2];
      if (select.id == "indomain_min" || select.id == "indomain") {
        strategy->set_domain_reduction_strategy(
            DecisionStrategyProto::SELECT_MIN_VALUE);
      } else if (select.id == "indomain_max") {
        strategy->set_domain_reduction_strategy(
            DecisionStrategyProto::SELECT_MAX_VALUE);
      } else if (select.id == "indomain_split") {
        strategy->set_domain_reduction_strategy(
            DecisionStrategyProto::SELECT_LOWER_HALF);
      } else if (select.id == "indomain_reverse_split") {
        strategy->set_domain_reduction_strategy(
            DecisionStrategyProto::SELECT_UPPER_HALF);
      } else {
        LOG(FATAL) << "Unsupported select: " << select.id;
      }
    }
  }
}

// The format is fixed in the flatzinc specification.
std::string SolutionString(
    const fz::SolutionOutputSpecs& output,
    const std::function<int64(fz::IntegerVariable*)>& value_func) {
  if (output.variable != nullptr) {
    const int64 value = value_func(output.variable);
    if (output.display_as_boolean) {
      return absl::StrCat(output.name, " = ", value == 1 ? "true" : "false",
                          ";");
    } else {
      return absl::StrCat(output.name, " = ", value, ";");
    }
  } else {
    const int bound_size = output.bounds.size();
    std::string result =
        absl::StrCat(output.name, " = array", bound_size, "d(");
    for (int i = 0; i < bound_size; ++i) {
      if (output.bounds[i].max_value != 0) {
        absl::StrAppend(&result, output.bounds[i].min_value, "..",
                        output.bounds[i].max_value, ", ");
      } else {
        result.append("{},");
      }
    }
    result.append("[");
    for (int i = 0; i < output.flat_variables.size(); ++i) {
      const int64 value = value_func(output.flat_variables[i]);
      if (output.display_as_boolean) {
        result.append(value ? "true" : "false");
      } else {
        absl::StrAppend(&result, value);
      }
      if (i != output.flat_variables.size() - 1) {
        result.append(", ");
      }
    }
    result.append("]);");
    return result;
  }
  return "";
}

std::string SolutionString(
    const fz::Model& model,
    const std::function<int64(fz::IntegerVariable*)>& value_func) {
  std::string solution_string;
  for (const auto& output_spec : model.output()) {
    solution_string.append(SolutionString(output_spec, value_func));
    solution_string.append("\n");
  }
  return solution_string;
}

void LogInFlatzincFormat(const std::string& multi_line_input) {
  std::vector<std::string> lines =
      absl::StrSplit(multi_line_input, '\n', absl::SkipEmpty());
  for (const std::string& line : lines) {
    FZLOG << line << FZENDL;
  }
}

void OutputFlatzincStats(const CpSolverResponse& response) {
  std::cout << "%%%mzn-stat: objective=" << response.objective_value()
            << std::endl;
  std::cout << "%%%mzn-stat: objectiveBound=" << response.best_objective_bound()
            << std::endl;
  std::cout << "%%%mzn-stat: boolVariables=" << response.num_booleans()
            << std::endl;
  std::cout << "%%%mzn-stat: failures=" << response.num_conflicts()
            << std::endl;
  std::cout << "%%%mzn-stat: propagations="
            << response.num_binary_propagations() +
                   response.num_integer_propagations()
            << std::endl;
  std::cout << "%%%mzn-stat: solveTime=" << response.wall_time() << std::endl;
}

}  // namespace

void SolveFzWithCpModelProto(const fz::Model& fz_model,
                             const fz::FlatzincSatParameters& p,
                             const std::string& sat_params) {
  if (!FLAGS_use_flatzinc_format) {
    LOG(INFO) << "*** Starting translation to CP-SAT";
  } else if (p.verbose_logging) {
    FZLOG << "*** Starting translation to CP-SAT" << FZENDL;
  }

  CpModelProtoWithMapping m;
  m.proto.set_name(fz_model.name());

  // The translation is easy, we create one variable per flatzinc variable, plus
  // eventually a bunch of constant variables that will be created lazily.
  int num_variables = 0;
  for (fz::IntegerVariable* fz_var : fz_model.variables()) {
    if (!fz_var->active) continue;
    m.fz_var_to_index[fz_var] = num_variables++;
    IntegerVariableProto* var = m.proto.add_variables();
    var->set_name(fz_var->name);
    if (fz_var->domain.is_interval) {
      if (fz_var->domain.values.empty()) {
        var->add_domain(kint64min);
        var->add_domain(kint64max);
      } else {
        var->add_domain(fz_var->domain.values[0]);
        var->add_domain(fz_var->domain.values[1]);
      }
    } else {
      FillDomainInProto(Domain::FromValues(fz_var->domain.values), var);
    }

    // Some variables in flatzinc have large domain and we don't really support
    // that in cp_model (where all the constraint checks that they cannot
    // overflow during their propagation). Because of that, we intersect the
    // variable domains with [kint32min, kint32max].
    //
    // TODO(user): Warn when we reduce the domain.
    FillDomainInProto(ReadDomainFromProto(*var).IntersectionWith(
                          Domain(kint32min, kint32max)),
                      var);
  }

  // Translate the constraints.
  for (fz::Constraint* fz_ct : fz_model.constraints()) {
    if (fz_ct == nullptr || !fz_ct->active) continue;
    ConstraintProto* ct = m.proto.add_constraints();
    ct->set_name(fz_ct->type);
    if (absl::EndsWith(fz_ct->type, "_reif") ||
        absl::EndsWith(fz_ct->type, "_imp") || fz_ct->type == "array_bool_or" ||
        fz_ct->type == "array_bool_and") {
      m.FillReifOrImpliedConstraint(*fz_ct, ct);
    } else {
      m.FillConstraint(*fz_ct, ct);
    }
  }

  // Fill the objective.
  if (fz_model.objective() != nullptr) {
    CpObjectiveProto* objective = m.proto.mutable_objective();
    objective->add_coeffs(1);
    if (fz_model.maximize()) {
      objective->set_scaling_factor(-1);
      objective->add_vars(
          NegatedCpModelVariable(m.fz_var_to_index[fz_model.objective()]));
    } else {
      objective->add_vars(m.fz_var_to_index[fz_model.objective()]);
    }
  }

  // Fill the search order.
  m.TranslateSearchAnnotations(fz_model.search_annotations());

  // Print model statistics.
  if (FLAGS_use_flatzinc_format && p.verbose_logging) {
    LogInFlatzincFormat(CpModelStats(m.proto));
  }

  if (p.display_all_solutions && !m.proto.has_objective()) {
    // Enumerate all sat solutions.
    m.parameters.set_enumerate_all_solutions(true);
  }
  if (p.use_free_search) {
    m.parameters.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
  } else {
    m.parameters.set_search_branching(SatParameters::FIXED_SEARCH);
  }
  if (p.max_time_in_seconds > 0) {
    m.parameters.set_max_time_in_seconds(p.max_time_in_seconds);
  }

  // We don't support enumerating all solution in parallel for a SAT problem.
  // But note that we do support it for an optimization problem since the
  // meaning of p.all_solutions is not the same in this case.
  if (p.display_all_solutions && fz_model.objective() == nullptr) {
    m.parameters.set_num_search_workers(1);
  } else {
    m.parameters.set_num_search_workers(std::max(1, p.number_of_threads));
  }

  // The order is important, we want the flag parameters to overwrite anything
  // set in m.parameters.
  sat::SatParameters flag_parameters;
  CHECK(google::protobuf::TextFormat::ParseFromString(sat_params,
                                                      &flag_parameters))
      << sat_params;
  m.parameters.MergeFrom(flag_parameters);

  // We only need an observer if 'p.all_solutions' is true.
  std::function<void(const CpSolverResponse&)> solution_observer = nullptr;
  if (p.display_all_solutions && FLAGS_use_flatzinc_format) {
    solution_observer = [&fz_model, &m, &p](const CpSolverResponse& r) {
      const std::string solution_string =
          SolutionString(fz_model, [&m, &r](fz::IntegerVariable* v) {
            return r.solution(gtl::FindOrDie(m.fz_var_to_index, v));
          });
      std::cout << solution_string << std::endl;
      if (p.display_statistics && FLAGS_use_flatzinc_format) {
        OutputFlatzincStats(r);
      }
      std::cout << "----------" << std::endl;
    };
  }

  Model sat_model;
  sat_model.Add(NewSatParameters(m.parameters));
  if (solution_observer != nullptr) {
    sat_model.Add(NewFeasibleSolutionObserver(solution_observer));
  }
  const CpSolverResponse response = SolveCpModel(m.proto, &sat_model);

  // Check the returned solution with the fz model checker.
  if (response.status() == CpSolverStatus::FEASIBLE ||
      response.status() == CpSolverStatus::OPTIMAL) {
    CHECK(CheckSolution(fz_model, [&response, &m](fz::IntegerVariable* v) {
      return response.solution(gtl::FindOrDie(m.fz_var_to_index, v));
    }));
  }

  // Output the solution in the flatzinc official format.
  if (FLAGS_use_flatzinc_format) {
    if (response.status() == CpSolverStatus::FEASIBLE ||
        response.status() == CpSolverStatus::OPTIMAL) {
      if (!p.display_all_solutions) {  // Already printed otherwise.
        const std::string solution_string =
            SolutionString(fz_model, [&response, &m](fz::IntegerVariable* v) {
              return response.solution(gtl::FindOrDie(m.fz_var_to_index, v));
            });
        std::cout << solution_string << std::endl;
        std::cout << "----------" << std::endl;
      }
      if (response.status() == CpSolverStatus::OPTIMAL) {
        std::cout << "==========" << std::endl;
      }
    } else if (response.status() == CpSolverStatus::INFEASIBLE) {
      std::cout << "=====UNSATISFIABLE=====" << std::endl;
    } else {
      std::cout << "%% TIMEOUT" << std::endl;
    }
    if (p.display_statistics && FLAGS_use_flatzinc_format) {
      OutputFlatzincStats(response);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
