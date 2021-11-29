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

#include "ortools/flatzinc/cp_model_fz_solver.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/map_util.h"
#include "ortools/base/threadpool.h"
#include "ortools/base/timer.h"
#include "ortools/flatzinc/checker.h"
#include "ortools/flatzinc/model.h"
#include "ortools/sat/cp_constraints.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
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
#include "ortools/util/logging.h"

ABSL_FLAG(int64_t, fz_int_max, int64_t{1} << 50,
          "Default max value for unbounded integer variables.");

namespace operations_research {
namespace sat {

namespace {

static const int kNoVar = std::numeric_limits<int>::min();

struct VarOrValue {
  int var = kNoVar;
  int64_t value = 0;
};

// Returns the true/false literal corresponding to a CpModelProto variable.
int TrueLiteral(int var) { return var; }
int FalseLiteral(int var) { return -var - 1; }
int NegatedCpModelVariable(int var) { return -var - 1; }

// Helper class to convert a flatzinc model to a CpModelProto.
struct CpModelProtoWithMapping {
  // Returns a constant CpModelProto variable created on-demand.
  int LookupConstant(int64_t value);

  // Convert a flatzinc argument to a variable or a list of variable.
  // Note that we always encode a constant argument with a constant variable.
  int LookupVar(const fz::Argument& argument);
  std::vector<int> LookupVars(const fz::Argument& argument);
  std::vector<VarOrValue> LookupVarsOrValues(const fz::Argument& argument);

  // Create and return the indices of the IntervalConstraint corresponding
  // to the flatzinc "interval" specified by a start var and a size var.
  // This method will cache intervals with the key <start, size>.
  std::vector<int> CreateIntervals(const std::vector<int>& starts,
                                   const std::vector<VarOrValue>& sizes);

  // Create and return the index of the optional IntervalConstraint
  // corresponding to the flatzinc "interval" specified by a start var, the
  // size_var, and the Boolean opt_var. This method will cache intervals with
  // the key <start, size, opt_var>. If opt_var == kNoVar, the interval will not
  // be optional.
  int GetOrCreateOptionalInterval(int start_var, VarOrValue size, int opt_var);

  // Adds a constraint to the model, add the enforcement literal if it is
  // different from kNoVar, and returns a ptr to the ConstraintProto.
  ConstraintProto* AddEnforcedConstraint(int literal);

  // Helpers to fill a ConstraintProto.
  void FillAMinusBInDomain(const std::vector<int64_t>& domain,
                           const fz::Constraint& fz_ct, ConstraintProto* ct);
  void FillLinearConstraintWithGivenDomain(const std::vector<int64_t>& domain,
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
  absl::flat_hash_map<fz::Variable*, int> fz_var_to_index;
  absl::flat_hash_map<int64_t, int> constant_value_to_index;
  absl::flat_hash_map<std::tuple<int, int, int>, int>
      start_size_opt_tuple_to_interval;
  absl::flat_hash_map<std::tuple<int, int64_t, int>, int>
      start_fixed_size_opt_tuple_to_interval;
};

int CpModelProtoWithMapping::LookupConstant(int64_t value) {
  if (constant_value_to_index.contains(value)) {
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
  CHECK_EQ(argument.type, fz::Argument::VAR_REF);
  return fz_var_to_index[argument.Var()];
}

std::vector<int> CpModelProtoWithMapping::LookupVars(
    const fz::Argument& argument) {
  std::vector<int> result;
  if (argument.type == fz::Argument::VOID_ARGUMENT) return result;
  if (argument.type == fz::Argument::INT_LIST) {
    for (int64_t value : argument.values) {
      result.push_back(LookupConstant(value));
    }
  } else if (argument.type == fz::Argument::INT_VALUE) {
    result.push_back(LookupConstant(argument.Value()));
  } else {
    CHECK_EQ(argument.type, fz::Argument::VAR_REF_ARRAY);
    for (fz::Variable* var : argument.variables) {
      CHECK(var != nullptr);
      result.push_back(fz_var_to_index[var]);
    }
  }
  return result;
}

std::vector<VarOrValue> CpModelProtoWithMapping::LookupVarsOrValues(
    const fz::Argument& argument) {
  std::vector<VarOrValue> result;
  const int no_var = kNoVar;
  if (argument.type == fz::Argument::VOID_ARGUMENT) return result;
  if (argument.type == fz::Argument::INT_LIST) {
    for (int64_t value : argument.values) {
      result.push_back({no_var, value});
    }
  } else if (argument.type == fz::Argument::INT_VALUE) {
    result.push_back({no_var, argument.Value()});
  } else {
    CHECK_EQ(argument.type, fz::Argument::VAR_REF_ARRAY);
    for (fz::Variable* var : argument.variables) {
      CHECK(var != nullptr);
      if (var->domain.HasOneValue()) {
        result.push_back({no_var, var->domain.Value()});
      } else {
        result.push_back({fz_var_to_index[var], 0});
      }
    }
  }
  return result;
}

ConstraintProto* CpModelProtoWithMapping::AddEnforcedConstraint(int literal) {
  ConstraintProto* result = proto.add_constraints();
  if (literal != kNoVar) {
    result->add_enforcement_literal(literal);
  }
  return result;
}

int CpModelProtoWithMapping::GetOrCreateOptionalInterval(int start_var,
                                                         VarOrValue size,
                                                         int opt_var) {
  const int interval_index = proto.constraints_size();
  if (size.var == kNoVar) {  // Size is fixed.
    const std::tuple<int, int64_t, int> key =
        std::make_tuple(start_var, size.value, opt_var);
    const auto [it, inserted] =
        start_fixed_size_opt_tuple_to_interval.insert({key, interval_index});
    if (!inserted) {
      return it->second;
    }

    auto* interval = AddEnforcedConstraint(opt_var)->mutable_interval();
    interval->mutable_start_view()->add_vars(start_var);
    interval->mutable_start_view()->add_coeffs(1);
    interval->mutable_size_view()->set_offset(size.value);
    interval->mutable_end_view()->add_vars(start_var);
    interval->mutable_end_view()->add_coeffs(1);
    interval->mutable_end_view()->set_offset(size.value);

    return interval_index;
  } else {  // Size is variable.
    const std::tuple<int, int, int> key =
        std::make_tuple(start_var, size.var, opt_var);
    const auto [it, inserted] =
        start_size_opt_tuple_to_interval.insert({key, interval_index});
    if (!inserted) {
      return it->second;
    }

    const int end_var = proto.variables_size();
    FillDomainInProto(
        ReadDomainFromProto(proto.variables(start_var))
            .AdditionWith(ReadDomainFromProto(proto.variables(size.var))),
        proto.add_variables());

    // Create the interval.
    auto* interval = AddEnforcedConstraint(opt_var)->mutable_interval();
    interval->mutable_start_view()->add_vars(start_var);
    interval->mutable_start_view()->add_coeffs(1);
    interval->mutable_size_view()->add_vars(size.var);
    interval->mutable_size_view()->add_coeffs(1);
    interval->mutable_end_view()->add_vars(end_var);
    interval->mutable_end_view()->add_coeffs(1);

    // Add the linear constraint (after the interval constraint as we have
    // stored its index).
    auto* lin = AddEnforcedConstraint(opt_var)->mutable_linear();
    lin->add_vars(start_var);
    lin->add_coeffs(1);
    lin->add_vars(size.var);
    lin->add_coeffs(1);
    lin->add_vars(end_var);
    lin->add_coeffs(-1);
    lin->add_domain(0);
    lin->add_domain(0);

    return interval_index;
  }
}

std::vector<int> CpModelProtoWithMapping::CreateIntervals(
    const std::vector<int>& starts, const std::vector<VarOrValue>& sizes) {
  std::vector<int> intervals;
  for (int i = 0; i < starts.size(); ++i) {
    intervals.push_back(
        GetOrCreateOptionalInterval(starts[i], sizes[i], kNoVar));
  }
  return intervals;
}

void CpModelProtoWithMapping::FillAMinusBInDomain(
    const std::vector<int64_t>& domain, const fz::Constraint& fz_ct,
    ConstraintProto* ct) {
  auto* arg = ct->mutable_linear();
  if (fz_ct.arguments[1].type == fz::Argument::INT_VALUE) {
    const int64_t value = fz_ct.arguments[1].Value();
    const int var_a = LookupVar(fz_ct.arguments[0]);
    for (const int64_t domain_bound : domain) {
      if (domain_bound == std::numeric_limits<int64_t>::min() ||
          domain_bound == std::numeric_limits<int64_t>::max()) {
        arg->add_domain(domain_bound);
      } else {
        arg->add_domain(domain_bound + value);
      }
    }
    arg->add_vars(var_a);
    arg->add_coeffs(1);
  } else if (fz_ct.arguments[0].type == fz::Argument::INT_VALUE) {
    const int64_t value = fz_ct.arguments[0].Value();
    const int var_b = LookupVar(fz_ct.arguments[1]);
    for (int64_t domain_bound : gtl::reversed_view(domain)) {
      if (domain_bound == std::numeric_limits<int64_t>::min()) {
        arg->add_domain(std::numeric_limits<int64_t>::max());
      } else if (domain_bound == std::numeric_limits<int64_t>::max()) {
        arg->add_domain(std::numeric_limits<int64_t>::min());
      } else {
        arg->add_domain(value - domain_bound);
      }
    }
    arg->add_vars(var_b);
    arg->add_coeffs(1);
  } else {
    for (const int64_t domain_bound : domain) arg->add_domain(domain_bound);
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_coeffs(1);
    arg->add_vars(LookupVar(fz_ct.arguments[1]));
    arg->add_coeffs(-1);
  }
}

void CpModelProtoWithMapping::FillLinearConstraintWithGivenDomain(
    const std::vector<int64_t>& domain, const fz::Constraint& fz_ct,
    ConstraintProto* ct) {
  auto* arg = ct->mutable_linear();
  for (const int64_t domain_bound : domain) arg->add_domain(domain_bound);
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
    // This is not the same semantics as the array_bool_xor as this constraint
    // is actually a fully reified xor(a, b) <==> x.
    const int a = LookupVar(fz_ct.arguments[0]);
    const int b = LookupVar(fz_ct.arguments[1]);
    const int x = LookupVar(fz_ct.arguments[2]);

    // not(x) => a == b
    ct->add_enforcement_literal(NegatedRef(x));
    auto* const refute = ct->mutable_linear();
    refute->add_vars(a);
    refute->add_coeffs(1);
    refute->add_vars(b);
    refute->add_coeffs(-1);
    refute->add_domain(0);
    refute->add_domain(0);

    // x => a + b == 1
    auto* enforce = AddEnforcedConstraint(x)->mutable_linear();
    enforce->add_vars(a);
    enforce->add_coeffs(1);
    enforce->add_vars(b);
    enforce->add_coeffs(1);
    enforce->add_domain(1);
    enforce->add_domain(1);
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
    FillAMinusBInDomain({std::numeric_limits<int64_t>::min(), 0}, fz_ct, ct);
  } else if (fz_ct.type == "bool_ge" || fz_ct.type == "int_ge") {
    FillAMinusBInDomain({0, std::numeric_limits<int64_t>::max()}, fz_ct, ct);
  } else if (fz_ct.type == "bool_lt" || fz_ct.type == "int_lt") {
    FillAMinusBInDomain({std::numeric_limits<int64_t>::min(), -1}, fz_ct, ct);
  } else if (fz_ct.type == "bool_gt" || fz_ct.type == "int_gt") {
    FillAMinusBInDomain({1, std::numeric_limits<int64_t>::max()}, fz_ct, ct);
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
    FillAMinusBInDomain({std::numeric_limits<int64_t>::min(), -1, 1,
                         std::numeric_limits<int64_t>::max()},
                        fz_ct, ct);
  } else if (fz_ct.type == "int_lin_eq") {
    const int64_t rhs = fz_ct.arguments[2].values[0];
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
      const int64_t v = fz_ct.arguments[2].Value();
      arg->add_domain(v);
      arg->add_domain(v);
    }
  } else if (fz_ct.type == "int_lin_le" || fz_ct.type == "bool_lin_le") {
    const int64_t rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain(
        {std::numeric_limits<int64_t>::min(), rhs}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_lt") {
    const int64_t rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain(
        {std::numeric_limits<int64_t>::min(), rhs - 1}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_ge") {
    const int64_t rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain(
        {rhs, std::numeric_limits<int64_t>::max()}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_gt") {
    const int64_t rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain(
        {rhs + 1, std::numeric_limits<int64_t>::max()}, fz_ct, ct);
  } else if (fz_ct.type == "int_lin_ne") {
    const int64_t rhs = fz_ct.arguments[2].values[0];
    FillLinearConstraintWithGivenDomain(
        {std::numeric_limits<int64_t>::min(), rhs - 1, rhs + 1,
         std::numeric_limits<int64_t>::max()},
        fz_ct, ct);
  } else if (fz_ct.type == "set_in") {
    auto* arg = ct->mutable_linear();
    arg->add_vars(LookupVar(fz_ct.arguments[0]));
    arg->add_coeffs(1);
    if (fz_ct.arguments[1].type == fz::Argument::INT_LIST) {
      FillDomainInProto(Domain::FromValues(std::vector<int64_t>{
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
              std::vector<int64_t>{fz_ct.arguments[1].values.begin(),
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
    if (fz_ct.arguments[0].type == fz::Argument::VAR_REF ||
        fz_ct.arguments[0].type == fz::Argument::INT_VALUE) {
      auto* arg = ct->mutable_element();
      arg->set_index(LookupVar(fz_ct.arguments[0]));
      arg->set_target(LookupVar(fz_ct.arguments[2]));

      if (!absl::EndsWith(fz_ct.type, "_nonshifted")) {
        // Add a dummy variable at position zero because flatzinc index start
        // at 1.
        // TODO(user): Make sure that zero is not in the index domain...
        arg->add_vars(LookupConstant(0));
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

      const std::vector<int64_t>& values = fz_ct.arguments[1].values;
      const int64_t coeff1 = fz_ct.arguments[3].values[0];
      const int64_t coeff2 = fz_ct.arguments[3].values[1];
      const int64_t offset = fz_ct.arguments[4].values[0] - 1;

      for (const int64_t a : AllValuesInDomain(proto.variables(arg->vars(0)))) {
        for (const int64_t b :
             AllValuesInDomain(proto.variables(arg->vars(1)))) {
          const int index = coeff1 * a + coeff2 * b + offset;
          CHECK_GE(index, 0);
          CHECK_LT(index, values.size());
          arg->add_values(a);
          arg->add_values(b);
          arg->add_values(values[index]);
        }
      }
    }
  } else if (fz_ct.type == "ortools_table_int") {
    auto* arg = ct->mutable_table();
    for (const int var : LookupVars(fz_ct.arguments[0])) arg->add_vars(var);
    for (const int64_t value : fz_ct.arguments[1].values)
      arg->add_values(value);
  } else if (fz_ct.type == "ortools_regular") {
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
  } else if (fz_ct.type == "fzn_all_different_int") {
    auto* arg = ct->mutable_all_diff();
    for (const int var : LookupVars(fz_ct.arguments[0])) arg->add_vars(var);
  } else if (fz_ct.type == "ortools_circuit" ||
             fz_ct.type == "ortools_subcircuit") {
    const int64_t min_index = fz_ct.arguments[1].Value();
    const int size = std::max(fz_ct.arguments[0].values.size(),
                              fz_ct.arguments[0].variables.size());

    const int64_t max_index = min_index + size - 1;
    // The arc-based mutable circuit.
    auto* circuit_arg = ct->mutable_circuit();

    // We fully encode all variables so we can use the literal based circuit.
    // TODO(user): avoid fully encoding more than once?
    int64_t index = min_index;
    const bool is_circuit = (fz_ct.type == "ortools_circuit");
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      Domain domain = ReadDomainFromProto(proto.variables(var));

      // Restrict the domain of var to [min_index, max_index]
      domain = domain.IntersectionWith(Domain(min_index, max_index));
      if (is_circuit) {
        // We simply make sure that the variable cannot take the value index.
        domain = domain.IntersectionWith(Domain::FromIntervals(
            {{std::numeric_limits<int64_t>::min(), index - 1},
             {index + 1, std::numeric_limits<int64_t>::max()}}));
      }
      FillDomainInProto(domain, proto.mutable_variables(var));

      for (const ClosedInterval interval : domain.intervals()) {
        for (int64_t value = interval.start; value <= interval.end; ++value) {
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
            auto* lin = AddEnforcedConstraint(literal)->mutable_linear();
            lin->add_coeffs(1);
            lin->add_vars(var);
            lin->add_domain(value);
            lin->add_domain(value);
          }

          // not(literal) => var != value
          {
            auto* lin =
                AddEnforcedConstraint(NegatedRef(literal))->mutable_linear();
            lin->add_coeffs(1);
            lin->add_vars(var);
            lin->add_domain(std::numeric_limits<int64_t>::min());
            lin->add_domain(value - 1);
            lin->add_domain(value + 1);
            lin->add_domain(std::numeric_limits<int64_t>::max());
          }
        }
      }

      ++index;
    }
  } else if (fz_ct.type == "ortools_inverse") {
    auto* arg = ct->mutable_inverse();

    const auto direct_variables = LookupVars(fz_ct.arguments[0]);
    const auto inverse_variables = LookupVars(fz_ct.arguments[1]);
    const int base_direct = fz_ct.arguments[2].Value();
    const int base_inverse = fz_ct.arguments[3].Value();

    CHECK_EQ(direct_variables.size(), inverse_variables.size());
    const int num_variables = direct_variables.size();
    const int end_direct = base_direct + num_variables;
    const int end_inverse = base_inverse + num_variables;

    // Any convention that maps the "fixed values" to the one of the inverse and
    // back works. We decided to follow this one:
    // There are 3 cases:
    // (A) base_direct == base_inverse, we fill the arrays
    //     direct  = [0, .., base_direct - 1] U [direct_vars]
    //     inverse = [0, .., base_direct - 1] U [inverse_vars]
    // (B) base_direct == base_inverse + offset (> 0), we fill the arrays
    //     direct  = [0, .., base_inverse - 1] U
    //               [end_inverse, .., end_inverse + offset - 1] U
    //               [direct_vars]
    //     inverse = [0, .., base_inverse - 1] U
    //               [inverse_vars] U
    //               [base_inverse, .., base_base_inverse + offset - 1]
    // (C): base_inverse == base_direct + offset (> 0), we fill the arrays
    //     direct =  [0, .., base_direct - 1] U
    //               [direct_vars] U
    //               [base_direct, .., base_direct + offset - 1]
    //     inverse   [0, .., base_direct - 1] U
    //               [end_direct, .., end_direct + offset - 1] U
    //               [inverse_vars]
    const int arity = std::max(base_inverse, base_direct) + num_variables;
    for (int i = 0; i < arity; ++i) {
      // Fill the direct array.
      if (i < base_direct) {
        if (i < base_inverse) {
          arg->add_f_direct(LookupConstant(i));
        } else if (i >= base_inverse) {
          arg->add_f_direct(LookupConstant(i + num_variables));
        }
      } else if (i >= base_direct && i < end_direct) {
        arg->add_f_direct(direct_variables[i - base_direct]);
      } else {
        arg->add_f_direct(LookupConstant(i - num_variables));
      }

      // Fill the inverse array.
      if (i < base_inverse) {
        if (i < base_direct) {
          arg->add_f_inverse(LookupConstant(i));
        } else if (i >= base_direct) {
          arg->add_f_inverse(LookupConstant(i + num_variables));
        }
      } else if (i >= base_inverse && i < end_inverse) {
        arg->add_f_inverse(inverse_variables[i - base_inverse]);
      } else {
        arg->add_f_inverse(LookupConstant(i - num_variables));
      }
    }
  } else if (fz_ct.type == "fzn_cumulative") {
    const std::vector<int> starts = LookupVars(fz_ct.arguments[0]);
    const std::vector<VarOrValue> sizes =
        LookupVarsOrValues(fz_ct.arguments[1]);
    const std::vector<int> demands = LookupVars(fz_ct.arguments[2]);
    const int capacity = LookupVar(fz_ct.arguments[3]);

    auto* arg = ct->mutable_cumulative();
    arg->set_capacity(capacity);
    for (int i = 0; i < starts.size(); ++i) {
      // Special case for a 0-1 demand, we mark the interval as optional
      // instead and fix the demand to 1.
      if (proto.variables(demands[i]).domain().size() == 2 &&
          proto.variables(demands[i]).domain(0) == 0 &&
          proto.variables(demands[i]).domain(1) == 1 &&
          proto.variables(capacity).domain(1) == 1) {
        arg->add_intervals(
            GetOrCreateOptionalInterval(starts[i], sizes[i], demands[i]));
        arg->add_demands(LookupConstant(1));
      } else {
        arg->add_intervals(
            GetOrCreateOptionalInterval(starts[i], sizes[i], kNoVar));
        arg->add_demands(demands[i]);
      }
    }
  } else if (fz_ct.type == "fzn_diffn" || fz_ct.type == "fzn_diffn_nonstrict") {
    const std::vector<int> x = LookupVars(fz_ct.arguments[0]);
    const std::vector<int> y = LookupVars(fz_ct.arguments[1]);
    const std::vector<VarOrValue> dx = LookupVarsOrValues(fz_ct.arguments[2]);
    const std::vector<VarOrValue> dy = LookupVarsOrValues(fz_ct.arguments[3]);
    const std::vector<int> x_intervals = CreateIntervals(x, dx);
    const std::vector<int> y_intervals = CreateIntervals(y, dy);
    auto* arg = ct->mutable_no_overlap_2d();
    for (int i = 0; i < x.size(); ++i) {
      arg->add_x_intervals(x_intervals[i]);
      arg->add_y_intervals(y_intervals[i]);
    }
    arg->set_boxes_with_null_area_can_overlap(fz_ct.type ==
                                              "fzn_diffn_nonstrict");
  } else if (fz_ct.type == "ortools_network_flow" ||
             fz_ct.type == "ortools_network_flow_cost") {
    // Note that we leave ct empty here (with just the name set).
    // We simply do a linear encoding of this constraint.
    const bool has_cost = fz_ct.type == "ortools_network_flow_cost";
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
        const int64_t weight = fz_ct.arguments[2].values[arc];
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
  if (absl::EndsWith(fz_ct.type, "_imp")) return;

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
      std::vector<fz::Variable*> vars;
      args[0].AppendAllVariables(&vars);

      DecisionStrategyProto* strategy = proto.add_search_strategy();
      for (fz::Variable* v : vars) {
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
      } else if (select.id == "indomain_median") {
        strategy->set_domain_reduction_strategy(
            DecisionStrategyProto::SELECT_MEDIAN_VALUE);
      } else {
        LOG(FATAL) << "Unsupported select: " << select.id;
      }
    }
  }
}

// The format is fixed in the flatzinc specification.
std::string SolutionString(
    const fz::SolutionOutputSpecs& output,
    const std::function<int64_t(fz::Variable*)>& value_func) {
  if (output.variable != nullptr) {
    const int64_t value = value_func(output.variable);
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
      if (output.bounds[i].max_value >= output.bounds[i].min_value) {
        absl::StrAppend(&result, output.bounds[i].min_value, "..",
                        output.bounds[i].max_value, ", ");
      } else {
        result.append("{},");
      }
    }
    result.append("[");
    for (int i = 0; i < output.flat_variables.size(); ++i) {
      const int64_t value = value_func(output.flat_variables[i]);
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
    const std::function<int64_t(fz::Variable*)>& value_func) {
  std::string solution_string;
  for (const auto& output_spec : model.output()) {
    solution_string.append(SolutionString(output_spec, value_func));
    solution_string.append("\n");
  }
  return solution_string;
}

void OutputFlatzincStats(const CpSolverResponse& response,
                         SolverLogger* solution_logger) {
  SOLVER_LOG(solution_logger,
             "%%%mzn-stat: objective=", response.objective_value());
  SOLVER_LOG(solution_logger,
             "%%%mzn-stat: objectiveBound=", response.best_objective_bound());
  SOLVER_LOG(solution_logger,
             "%%%mzn-stat: boolVariables=", response.num_booleans());
  SOLVER_LOG(solution_logger,
             "%%%mzn-stat: failures=", response.num_conflicts());
  SOLVER_LOG(
      solution_logger, "%%%mzn-stat: propagations=",
      response.num_binary_propagations() + response.num_integer_propagations());
  SOLVER_LOG(solution_logger, "%%%mzn-stat: solveTime=", response.wall_time());
}

}  // namespace

void SolveFzWithCpModelProto(const fz::Model& fz_model,
                             const fz::FlatzincSatParameters& p,
                             const std::string& sat_params,
                             SolverLogger* logger,
                             SolverLogger* solution_logger) {
  CpModelProtoWithMapping m;
  m.proto.set_name(fz_model.name());

  // The translation is easy, we create one variable per flatzinc variable,
  // plus eventually a bunch of constant variables that will be created
  // lazily.
  int num_variables = 0;
  for (fz::Variable* fz_var : fz_model.variables()) {
    if (!fz_var->active) continue;
    CHECK(!fz_var->domain.is_float)
        << "CP-SAT does not support float variables";

    m.fz_var_to_index[fz_var] = num_variables++;
    IntegerVariableProto* var = m.proto.add_variables();
    var->set_name(fz_var->name);
    if (fz_var->domain.is_interval) {
      if (fz_var->domain.values.empty()) {
        // The CP-SAT solver checks that constraints cannot overflow during
        // their propagation. Because of that, we trim undefined variable
        // domains (i.e. int in minizinc) to something hopefully large enough.
        LOG_FIRST_N(WARNING, 1)
            << "Using flag --fz_int_max for unbounded integer variables.";
        LOG_FIRST_N(WARNING, 1)
            << "    actual domain is [" << -absl::GetFlag(FLAGS_fz_int_max)
            << ".." << absl::GetFlag(FLAGS_fz_int_max) << "]";
        var->add_domain(-absl::GetFlag(FLAGS_fz_int_max));
        var->add_domain(absl::GetFlag(FLAGS_fz_int_max));
      } else {
        var->add_domain(fz_var->domain.values[0]);
        var->add_domain(fz_var->domain.values[1]);
      }
    } else {
      FillDomainInProto(Domain::FromValues(fz_var->domain.values), var);
    }
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

  if (p.display_all_solutions && !m.proto.has_objective()) {
    // Enumerate all sat solutions.
    m.parameters.set_enumerate_all_solutions(true);
  }

  m.parameters.set_log_search_progress(p.log_search_progress);

  // Helps with challenge unit tests.
  m.parameters.set_max_domain_size_when_encoding_eq_neq_constraints(32);

  // Computes the number of workers.
  int num_workers = 1;
  if (p.display_all_solutions && fz_model.objective() == nullptr) {
    if (p.number_of_threads > 1) {
      // We don't support enumerating all solution in parallel for a SAT
      // problem. But note that we do support it for an optimization problem
      // since the meaning of p.all_solutions is not the same in this case.
      SOLVER_LOG(logger,
                 "Search for all solutions of a SAT problem in parallel is not "
                 "supported. Switching back to sequential search.");
    }
  } else if (p.number_of_threads <= 0) {
    // TODO(user): Supports setting the number of workers to 0, which will
    //     then query the number of cores available. This is complex now as we
    //     need to still support the expected behabior (no flags -> 1 thread
    //     fixed search, -f -> 1 thread free search).
    SOLVER_LOG(logger,
               "The number of search workers, is not specified. For better "
               "performances, please set the number of workers to 8, 16, or "
               "more depending on the number of cores of your computer.");
  } else {
    num_workers = p.number_of_threads;
  }
  m.parameters.set_num_search_workers(num_workers);

  // Specifies single thread specific search modes.
  if (num_workers == 1) {
    if (p.use_free_search) {
      m.parameters.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
      m.parameters.set_interleave_search(true);
      m.parameters.set_reduce_memory_usage_in_interleave_mode(true);
    } else {
      m.parameters.set_search_branching(SatParameters::FIXED_SEARCH);
      m.parameters.set_keep_all_feasible_solutions_in_presolve(true);
    }
  }

  // Time limit.
  if (p.max_time_in_seconds > 0) {
    m.parameters.set_max_time_in_seconds(p.max_time_in_seconds);
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
  if (p.display_all_solutions) {
    solution_observer = [&fz_model, &m, &p,
                         solution_logger](const CpSolverResponse& r) {
      const std::string solution_string =
          SolutionString(fz_model, [&m, &r](fz::Variable* v) {
            return r.solution(gtl::FindOrDie(m.fz_var_to_index, v));
          });
      SOLVER_LOG(solution_logger, solution_string);
      if (p.display_statistics) {
        OutputFlatzincStats(r, solution_logger);
      }
      SOLVER_LOG(solution_logger, "----------");
    };
  }

  Model sat_model;
  sat_model.Add(NewSatParameters(m.parameters));
  if (solution_observer != nullptr) {
    sat_model.Add(NewFeasibleSolutionObserver(solution_observer));
  }
  // Setup logging.
  sat_model.GetOrCreate<SatParameters>()->set_log_to_stdout(false);
  sat_model.Register<SolverLogger>(logger);

  const CpSolverResponse response = SolveCpModel(m.proto, &sat_model);

  // Check the returned solution with the fz model checker.
  if (response.status() == CpSolverStatus::FEASIBLE ||
      response.status() == CpSolverStatus::OPTIMAL) {
    CHECK(CheckSolution(
        fz_model,
        [&response, &m](fz::Variable* v) {
          return response.solution(gtl::FindOrDie(m.fz_var_to_index, v));
        },
        logger));
  }

  // Output the solution in the flatzinc official format.
  if (solution_logger->LoggingIsEnabled()) {
    if (response.status() == CpSolverStatus::FEASIBLE ||
        response.status() == CpSolverStatus::OPTIMAL) {
      if (!p.display_all_solutions) {  // Already printed otherwise.
        const std::string solution_string =
            SolutionString(fz_model, [&response, &m](fz::Variable* v) {
              return response.solution(gtl::FindOrDie(m.fz_var_to_index, v));
            });
        SOLVER_LOG(solution_logger, solution_string);
        SOLVER_LOG(solution_logger, "----------");
      }
      if (response.status() == CpSolverStatus::OPTIMAL) {
        SOLVER_LOG(solution_logger, "==========");
      }
    } else if (response.status() == CpSolverStatus::INFEASIBLE) {
      SOLVER_LOG(solution_logger, "=====UNSATISFIABLE=====");
    } else if (response.status() == CpSolverStatus::MODEL_INVALID) {
      const std::string error_message = ValidateCpModel(m.proto);
      VLOG(1) << "%% Error message = '" << error_message << "'";
      if (absl::StrContains(error_message, "overflow")) {
        SOLVER_LOG(solution_logger, "=====OVERFLOW=====");
      } else {
        SOLVER_LOG(solution_logger, "=====MODEL INVALID=====");
      }
    } else {
      SOLVER_LOG(solution_logger, "%% TIMEOUT");
    }
    if (p.display_statistics) {
      OutputFlatzincStats(response, solution_logger);
    }
  }
}

}  // namespace sat
}  // namespace operations_research
