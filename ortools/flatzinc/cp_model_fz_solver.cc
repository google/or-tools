// Copyright 2010-2025 Google LLC
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

#include <algorithm>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/flatzinc/checker.h"
#include "ortools/flatzinc/model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/logging.h"
#include "ortools/util/sorted_interval_list.h"

ABSL_FLAG(int64_t, fz_int_max, int64_t{1} << 40,
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

// Helper class to convert a flatzinc model to a CpModelProto.
struct CpModelProtoWithMapping {
  CpModelProtoWithMapping()
      : arena(std::make_unique<google::protobuf::Arena>()),
        proto(*google::protobuf::Arena::Create<CpModelProto>(arena.get())) {}

  // Returns a constant CpModelProto variable created on-demand.
  int LookupConstant(int64_t value);

  // Convert a flatzinc argument to a variable or a list of variable.
  // Note that we always encode a constant argument with a constant variable.
  int LookupVar(const fz::Argument& argument);
  LinearExpressionProto LookupExpr(const fz::Argument& argument,
                                   bool negate = false);
  LinearExpressionProto LookupExprAt(const fz::Argument& argument, int pos,
                                     bool negate = false);
  std::vector<int> LookupVars(const fz::Argument& argument);
  VarOrValue LookupVarOrValue(const fz::Argument& argument);
  std::vector<VarOrValue> LookupVarsOrValues(const fz::Argument& argument);

  // Get or create a literal that is equivalent tovar == value.
  int GetOrCreateLiteralForVarEqValue(int var, int64_t value);

  // Get or create a literal that is equivalent to var1 == var2.
  int GetOrCreateLiteralForVarEqVar(int var1, int var2);

  // Create and return the indices of the IntervalConstraint corresponding
  // to the flatzinc "interval" specified by a start var and a size var.
  // This method will cache intervals with the key <start, size>.
  std::vector<int> CreateIntervals(absl::Span<const VarOrValue> starts,
                                   absl::Span<const VarOrValue> sizes);

  // Create and return the indices of the IntervalConstraint corresponding
  // to the flatzinc "interval" specified by a start var and a size var.
  // This method will cache intervals with the key <start, size>.
  // This interval will be optional if the size can be 0.
  // It also adds a constraint linking the enforcement literal with the size,
  // stating that the interval will be performed if and only if the size is
  // greater than 0.
  std::vector<int> CreateNonZeroOrOptionalIntervals(
      absl::Span<const VarOrValue> starts, absl::Span<const VarOrValue> sizes);

  // Create and return the index of the optional IntervalConstraint
  // corresponding to the flatzinc "interval" specified by a start var, the
  // size_var, and the Boolean opt_var. This method will cache intervals with
  // the key <start, size, opt_var>. If opt_var == kNoVar, the interval will not
  // be optional.
  int GetOrCreateOptionalInterval(VarOrValue start_var, VarOrValue size,
                                  int opt_var);

  // Get or Create a literal that implies the variable is > 0.
  // Its negation implies the variable is == 0.
  int NonZeroLiteralFrom(VarOrValue size);

  // Adds a constraint to the model, add the enforcement literal if it is
  // different from kNoVar, and returns a ptr to the ConstraintProto.
  ConstraintProto* AddEnforcedConstraint(int literal);

  // Helpers to fill a ConstraintProto.
  void FillAMinusBInDomain(const std::vector<int64_t>& domain,
                           const fz::Constraint& fz_ct, ConstraintProto* ct);
  void FillLinearConstraintWithGivenDomain(absl::Span<const int64_t> domain,
                                           const fz::Constraint& fz_ct,
                                           ConstraintProto* ct);
  void FillConstraint(const fz::Constraint& fz_ct, ConstraintProto* ct);
  void FillReifOrImpliedConstraint(const fz::Constraint& fz_ct,
                                   ConstraintProto* ct);
  void BuildTableFromDomainIntLinEq(const fz::Constraint& fz_ct,
                                    ConstraintProto* ct);

  // Translates the flatzinc search annotations into the CpModelProto
  // search_order field.
  void TranslateSearchAnnotations(
      absl::Span<const fz::Annotation> search_annotations,
      SolverLogger* logger);

  // The output proto.
  std::unique_ptr<google::protobuf::Arena> arena;
  CpModelProto& proto;
  SatParameters parameters;

  // Mapping from flatzinc variables to CpModelProto variables.
  absl::flat_hash_map<fz::Variable*, int> fz_var_to_index;
  absl::flat_hash_map<int64_t, int> constant_value_to_index;
  absl::flat_hash_map<std::tuple<int, int64_t, int, int64_t, int>, int>
      interval_key_to_index;
  absl::flat_hash_map<int, int> var_to_lit_implies_greater_than_zero;
  absl::flat_hash_map<std::pair<int, int64_t>, int> var_eq_value_to_literal;
  absl::flat_hash_map<std::pair<int, int>, int> var_eq_var_to_literal;
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

LinearExpressionProto CpModelProtoWithMapping::LookupExpr(
    const fz::Argument& argument, bool negate) {
  LinearExpressionProto expr;
  if (argument.HasOneValue()) {
    const int64_t value = argument.Value();
    expr.set_offset(negate ? -value : value);
  } else {
    expr.add_vars(LookupVar(argument));
    expr.add_coeffs(negate ? -1 : 1);
  }
  return expr;
}

LinearExpressionProto CpModelProtoWithMapping::LookupExprAt(
    const fz::Argument& argument, int pos, bool negate) {
  LinearExpressionProto expr;
  if (argument.HasOneValueAt(pos)) {
    const int64_t value = argument.ValueAt(pos);
    expr.set_offset(negate ? -value : value);
  } else {
    expr.add_vars(fz_var_to_index[argument.VarAt(pos)]);
    expr.add_coeffs(negate ? -1 : 1);
  }
  return expr;
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

VarOrValue CpModelProtoWithMapping::LookupVarOrValue(
    const fz::Argument& argument) {
  if (argument.type == fz::Argument::INT_VALUE) {
    return {kNoVar, argument.Value()};
  } else {
    CHECK_EQ(argument.type, fz::Argument::VAR_REF);
    fz::Variable* var = argument.Var();
    CHECK(var != nullptr);
    if (var->domain.HasOneValue()) {
      return {kNoVar, var->domain.Value()};
    } else {
      return {fz_var_to_index[var], 0};
    }
  }
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

int CpModelProtoWithMapping::GetOrCreateLiteralForVarEqValue(int var,
                                                             int64_t value) {
  const std::pair<int, int64_t> key = {var, value};
  const auto it = var_eq_value_to_literal.find(key);
  if (it != var_eq_value_to_literal.end()) return it->second;
  const int bool_var = proto.variables_size();
  IntegerVariableProto* var_proto = proto.add_variables();
  var_proto->add_domain(0);
  var_proto->add_domain(1);

  ConstraintProto* is_eq = AddEnforcedConstraint(TrueLiteral(bool_var));
  is_eq->mutable_linear()->add_vars(var);
  is_eq->mutable_linear()->add_coeffs(1);
  is_eq->mutable_linear()->add_domain(value);
  is_eq->mutable_linear()->add_domain(value);

  ConstraintProto* is_not_eq = AddEnforcedConstraint(FalseLiteral(bool_var));
  is_not_eq->mutable_linear()->add_vars(var);
  is_not_eq->mutable_linear()->add_coeffs(1);
  is_not_eq->mutable_linear()->add_domain(std::numeric_limits<int64_t>::min());
  is_not_eq->mutable_linear()->add_domain(value - 1);
  is_not_eq->mutable_linear()->add_domain(value + 1);
  is_not_eq->mutable_linear()->add_domain(std::numeric_limits<int64_t>::max());

  var_eq_value_to_literal[key] = bool_var;
  return bool_var;
}

int CpModelProtoWithMapping::GetOrCreateLiteralForVarEqVar(int var1, int var2) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 > var2) std::swap(var1, var2);
  if (var1 == var2) return LookupConstant(1);

  const std::pair<int, int> key = {var1, var2};
  const auto it = var_eq_var_to_literal.find(key);
  if (it != var_eq_var_to_literal.end()) return it->second;

  const int bool_var = proto.variables_size();
  IntegerVariableProto* var_proto = proto.add_variables();
  var_proto->add_domain(0);
  var_proto->add_domain(1);

  ConstraintProto* is_eq = AddEnforcedConstraint(TrueLiteral(bool_var));
  is_eq->mutable_linear()->add_vars(var1);
  is_eq->mutable_linear()->add_coeffs(1);
  is_eq->mutable_linear()->add_vars(var2);
  is_eq->mutable_linear()->add_coeffs(-1);
  is_eq->mutable_linear()->add_domain(0);
  is_eq->mutable_linear()->add_domain(0);

  ConstraintProto* is_not_eq = AddEnforcedConstraint(FalseLiteral(bool_var));
  is_not_eq->mutable_linear()->add_vars(var1);
  is_not_eq->mutable_linear()->add_coeffs(1);
  is_not_eq->mutable_linear()->add_vars(var2);
  is_not_eq->mutable_linear()->add_coeffs(-1);
  is_not_eq->mutable_linear()->add_domain(std::numeric_limits<int64_t>::min());
  is_not_eq->mutable_linear()->add_domain(-1);
  is_not_eq->mutable_linear()->add_domain(1);
  is_not_eq->mutable_linear()->add_domain(std::numeric_limits<int64_t>::max());

  var_eq_var_to_literal[key] = bool_var;
  return bool_var;
}

int CpModelProtoWithMapping::GetOrCreateOptionalInterval(VarOrValue start,
                                                         VarOrValue size,
                                                         int opt_var) {
  const int interval_index = proto.constraints_size();
  const std::tuple<int, int64_t, int, int64_t, int> key =
      std::make_tuple(start.var, start.value, size.var, size.value, opt_var);
  const auto [it, inserted] =
      interval_key_to_index.insert({key, interval_index});
  if (!inserted) {
    return it->second;
  }

  if (size.var == kNoVar || start.var == kNoVar) {  // Size or start fixed.
    auto* interval = AddEnforcedConstraint(opt_var)->mutable_interval();
    if (start.var != kNoVar) {
      interval->mutable_start()->add_vars(start.var);
      interval->mutable_start()->add_coeffs(1);
      interval->mutable_end()->add_vars(start.var);
      interval->mutable_end()->add_coeffs(1);
    } else {
      interval->mutable_start()->set_offset(start.value);
      interval->mutable_end()->set_offset(start.value);
    }

    if (size.var != kNoVar) {
      interval->mutable_size()->add_vars(size.var);
      interval->mutable_size()->add_coeffs(1);
      interval->mutable_end()->add_vars(size.var);
      interval->mutable_end()->add_coeffs(1);
    } else {
      interval->mutable_size()->set_offset(size.value);
      interval->mutable_end()->set_offset(size.value +
                                          interval->end().offset());
    }
    return interval_index;
  } else {  // start and size are variable.
    const int end_var = proto.variables_size();
    FillDomainInProto(
        ReadDomainFromProto(proto.variables(start.var))
            .AdditionWith(ReadDomainFromProto(proto.variables(size.var))),
        proto.add_variables());

    // Create the interval.
    auto* interval = AddEnforcedConstraint(opt_var)->mutable_interval();
    interval->mutable_start()->add_vars(start.var);
    interval->mutable_start()->add_coeffs(1);
    interval->mutable_size()->add_vars(size.var);
    interval->mutable_size()->add_coeffs(1);
    interval->mutable_end()->add_vars(end_var);
    interval->mutable_end()->add_coeffs(1);

    // Add the linear constraint (after the interval constraint as we have
    // stored its index).
    auto* lin = AddEnforcedConstraint(opt_var)->mutable_linear();
    lin->add_vars(start.var);
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
    absl::Span<const VarOrValue> starts, absl::Span<const VarOrValue> sizes) {
  std::vector<int> intervals;
  for (int i = 0; i < starts.size(); ++i) {
    intervals.push_back(
        GetOrCreateOptionalInterval(starts[i], sizes[i], kNoVar));
  }
  return intervals;
}

std::vector<int> CpModelProtoWithMapping::CreateNonZeroOrOptionalIntervals(
    absl::Span<const VarOrValue> starts, absl::Span<const VarOrValue> sizes) {
  std::vector<int> intervals;
  for (int i = 0; i < starts.size(); ++i) {
    const int opt_var = NonZeroLiteralFrom(sizes[i]);
    intervals.push_back(
        GetOrCreateOptionalInterval(starts[i], sizes[i], opt_var));
  }
  return intervals;
}

int CpModelProtoWithMapping::NonZeroLiteralFrom(VarOrValue size) {
  if (size.var == kNoVar) {
    return LookupConstant(size.value > 0);
  }
  const auto it = var_to_lit_implies_greater_than_zero.find(size.var);
  if (it != var_to_lit_implies_greater_than_zero.end()) {
    return it->second;
  }

  const IntegerVariableProto& var_proto = proto.variables(size.var);
  const Domain domain = ReadDomainFromProto(var_proto);
  DCHECK_GE(domain.Min(), 0);
  if (domain.Min() > 0) return LookupConstant(1);
  if (domain.Max() == 0) {
    return LookupConstant(0);
  }

  const int var_greater_than_zero_lit = proto.variables_size();
  IntegerVariableProto* lit_proto = proto.add_variables();
  lit_proto->add_domain(0);
  lit_proto->add_domain(1);

  ConstraintProto* is_greater =
      AddEnforcedConstraint(TrueLiteral(var_greater_than_zero_lit));
  is_greater->mutable_linear()->add_vars(size.var);
  is_greater->mutable_linear()->add_coeffs(1);
  const Domain positive = domain.IntersectionWith({1, domain.Max()});
  FillDomainInProto(positive, is_greater->mutable_linear());

  ConstraintProto* is_null =
      AddEnforcedConstraint(FalseLiteral(var_greater_than_zero_lit));
  is_null->mutable_linear()->add_vars(size.var);
  is_null->mutable_linear()->add_coeffs(1);
  is_null->mutable_linear()->add_domain(0);
  is_null->mutable_linear()->add_domain(0);

  var_to_lit_implies_greater_than_zero[size.var] = var_greater_than_zero_lit;
  return var_greater_than_zero_lit;
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
    absl::Span<const int64_t> domain, const fz::Constraint& fz_ct,
    ConstraintProto* ct) {
  auto* arg = ct->mutable_linear();
  for (const int64_t domain_bound : domain) arg->add_domain(domain_bound);
  std::vector<int> vars = LookupVars(fz_ct.arguments[1]);
  for (int i = 0; i < vars.size(); ++i) {
    arg->add_vars(vars[i]);
    arg->add_coeffs(fz_ct.arguments[0].values[i]);
  }
}

void CpModelProtoWithMapping::BuildTableFromDomainIntLinEq(
    const fz::Constraint& fz_ct, ConstraintProto* ct) {
  const std::vector<int64_t>& coeffs = fz_ct.arguments[0].values;
  const std::vector<int> vars = LookupVars(fz_ct.arguments[1]);
  const int rhs = fz_ct.arguments[2].Value();
  CHECK_EQ(coeffs.back(), -1);
  for (const int var : vars) {
    LinearExpressionProto* expr = ct->mutable_table()->add_exprs();
    expr->add_vars(var);
    expr->add_coeffs(1);
  }

  switch (vars.size()) {
    case 3: {
      const Domain domain0 = ReadDomainFromProto(proto.variables(vars[0]));
      const Domain domain1 = ReadDomainFromProto(proto.variables(vars[1]));
      for (const int64_t v0 : domain0.Values()) {
        for (const int64_t v1 : domain1.Values()) {
          const int64_t v2 = coeffs[0] * v0 + coeffs[1] * v1 - rhs;
          ct->mutable_table()->add_values(v0);
          ct->mutable_table()->add_values(v1);
          ct->mutable_table()->add_values(v2);
        }
      }
      break;
    }
    case 4: {
      const Domain domain0 = ReadDomainFromProto(proto.variables(vars[0]));
      const Domain domain1 = ReadDomainFromProto(proto.variables(vars[1]));
      const Domain domain2 = ReadDomainFromProto(proto.variables(vars[2]));
      for (const int64_t v0 : domain0.Values()) {
        for (const int64_t v1 : domain1.Values()) {
          for (const int64_t v2 : domain2.Values()) {
            const int64_t v3 =
                coeffs[0] * v0 + coeffs[1] * v1 + coeffs[2] * v2 - rhs;
            ct->mutable_table()->add_values(v0);
            ct->mutable_table()->add_values(v1);
            ct->mutable_table()->add_values(v2);
            ct->mutable_table()->add_values(v3);
          }
        }
      }
      break;
    }
    default:
      LOG(FATAL) << "Unsupported size";
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
    // Special case for the index of element 2D and element 3D constraints.
    if (fz_ct.strong_propagation && fz_ct.arguments[0].Size() >= 3 &&
        fz_ct.arguments[0].Size() <= 4 &&
        fz_ct.arguments[0].values.back() == -1) {
      BuildTableFromDomainIntLinEq(fz_ct, ct);
    } else {
      const int64_t rhs = fz_ct.arguments[2].values[0];
      FillLinearConstraintWithGivenDomain({rhs, rhs}, fz_ct, ct);
    }
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
    auto* arg = ct->mutable_lin_max();
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[0], /*negate=*/true);
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[1], /*negate=*/true);
    *arg->mutable_target() = LookupExpr(fz_ct.arguments[2], /*negate=*/true);
  } else if (fz_ct.type == "array_int_minimum" || fz_ct.type == "minimum_int") {
    auto* arg = ct->mutable_lin_max();
    *arg->mutable_target() = LookupExpr(fz_ct.arguments[0], /*negate=*/true);
    for (int i = 0; i < fz_ct.arguments[1].Size(); ++i) {
      *arg->add_exprs() = LookupExprAt(fz_ct.arguments[1], i, /*negate=*/true);
    }
  } else if (fz_ct.type == "int_max") {
    auto* arg = ct->mutable_lin_max();
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[0]);
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[1]);
    *arg->mutable_target() = LookupExpr(fz_ct.arguments[2]);
  } else if (fz_ct.type == "array_int_maximum" || fz_ct.type == "maximum_int") {
    auto* arg = ct->mutable_lin_max();
    *arg->mutable_target() = LookupExpr(fz_ct.arguments[0]);
    for (int i = 0; i < fz_ct.arguments[1].Size(); ++i) {
      *arg->add_exprs() = LookupExprAt(fz_ct.arguments[1], i);
    }
  } else if (fz_ct.type == "int_times") {
    auto* arg = ct->mutable_int_prod();
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[0]);
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[1]);
    *arg->mutable_target() = LookupExpr(fz_ct.arguments[2]);
  } else if (fz_ct.type == "int_abs") {
    auto* arg = ct->mutable_lin_max();
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[0]);
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[0], /*negate=*/true);
    *arg->mutable_target() = LookupExpr(fz_ct.arguments[1]);
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
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[0]);
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[1]);
    *arg->mutable_target() = LookupExpr(fz_ct.arguments[2]);
  } else if (fz_ct.type == "int_mod") {
    auto* arg = ct->mutable_int_mod();
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[0]);
    *arg->add_exprs() = LookupExpr(fz_ct.arguments[1]);
    *arg->mutable_target() = LookupExpr(fz_ct.arguments[2]);
  } else if (fz_ct.type == "array_int_element" ||
             fz_ct.type == "array_bool_element" ||
             fz_ct.type == "array_var_int_element" ||
             fz_ct.type == "array_var_bool_element" ||
             fz_ct.type == "array_int_element_nonshifted") {
    auto* arg = ct->mutable_element();
    *arg->mutable_linear_index() = LookupExpr(fz_ct.arguments[0]);
    if (!absl::EndsWith(fz_ct.type, "_nonshifted")) {
      arg->mutable_linear_index()->set_offset(arg->linear_index().offset() - 1);
    }
    *arg->mutable_linear_target() = LookupExpr(fz_ct.arguments[2]);

    for (const VarOrValue elem : LookupVarsOrValues(fz_ct.arguments[1])) {
      auto* elem_proto = ct->mutable_element()->add_exprs();
      if (elem.var != kNoVar) {
        elem_proto->add_vars(elem.var);
        elem_proto->add_coeffs(1);
      } else {
        elem_proto->set_offset(elem.value);
      }
    }
  } else if (fz_ct.type == "ortools_array_int_element" ||
             fz_ct.type == "ortools_array_bool_element" ||
             fz_ct.type == "ortools_array_var_int_element" ||
             fz_ct.type == "ortools_array_var_bool_element") {
    auto* arg = ct->mutable_element();

    // Index.
    *arg->mutable_linear_index() = LookupExpr(fz_ct.arguments[0]);
    const int64_t index_min = fz_ct.arguments[1].values[0];
    arg->mutable_linear_index()->set_offset(arg->linear_index().offset() -
                                            index_min);

    // Target.
    *arg->mutable_linear_target() = LookupExpr(fz_ct.arguments[3]);

    // Expressions.
    for (const VarOrValue elem : LookupVarsOrValues(fz_ct.arguments[2])) {
      auto* elem_proto = ct->mutable_element()->add_exprs();
      if (elem.var != kNoVar) {
        elem_proto->add_vars(elem.var);
        elem_proto->add_coeffs(1);
      } else {
        elem_proto->set_offset(elem.value);
      }
    }
  } else if (fz_ct.type == "ortools_table_int") {
    auto* arg = ct->mutable_table();
    for (const VarOrValue v : LookupVarsOrValues(fz_ct.arguments[0])) {
      LinearExpressionProto* expr = arg->add_exprs();
      if (v.var != kNoVar) {
        expr->add_vars(v.var);
        expr->add_coeffs(1);
      } else {
        expr->set_offset(v.value);
      }
    }
    for (const int64_t value : fz_ct.arguments[1].values)
      arg->add_values(value);
  } else if (fz_ct.type == "ortools_regular") {
    auto* arg = ct->mutable_automaton();
    for (const VarOrValue v : LookupVarsOrValues(fz_ct.arguments[0])) {
      LinearExpressionProto* expr = arg->add_exprs();
      if (v.var != kNoVar) {
        expr->add_vars(v.var);
        expr->add_coeffs(1);
      } else {
        expr->set_offset(v.value);
      }
    }

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
    for (int i = 0; i < fz_ct.arguments[0].Size(); ++i) {
      *arg->add_exprs() = LookupExprAt(fz_ct.arguments[0], i);
    }
  } else if (fz_ct.type == "ortools_count_eq_cst") {
    const std::vector<VarOrValue> counts =
        LookupVarsOrValues(fz_ct.arguments[0]);
    const int64_t value = fz_ct.arguments[1].Value();
    const VarOrValue target = LookupVarOrValue(fz_ct.arguments[2]);
    LinearConstraintProto* arg = ct->mutable_linear();
    int64_t fixed_contributions = 0;
    for (const VarOrValue& count : counts) {
      if (count.var == kNoVar) {
        fixed_contributions += count.value == value ? 1 : 0;
      } else {
        const int boolvar = GetOrCreateLiteralForVarEqValue(count.var, value);
        CHECK_GE(boolvar, 0);
        arg->add_vars(boolvar);
        arg->add_coeffs(1);
      }
    }
    if (target.var == kNoVar) {
      arg->add_domain(target.value - fixed_contributions);
      arg->add_domain(target.value - fixed_contributions);
    } else {
      arg->add_vars(target.var);
      arg->add_coeffs(-1);
      arg->add_domain(-fixed_contributions);
      arg->add_domain(-fixed_contributions);
    }
  } else if (fz_ct.type == "ortools_count_eq") {
    const std::vector<VarOrValue> counts =
        LookupVarsOrValues(fz_ct.arguments[0]);
    const int var = LookupVar(fz_ct.arguments[1]);
    const VarOrValue target = LookupVarOrValue(fz_ct.arguments[2]);
    LinearConstraintProto* arg = ct->mutable_linear();
    for (const VarOrValue& count : counts) {
      if (count.var == kNoVar) {
        const int boolvar = GetOrCreateLiteralForVarEqValue(var, count.value);
        CHECK_GE(boolvar, 0);
        arg->add_vars(boolvar);
        arg->add_coeffs(1);
      } else {
        const int boolvar = GetOrCreateLiteralForVarEqVar(var, count.var);
        CHECK_GE(boolvar, 0);
        arg->add_vars(boolvar);
        arg->add_coeffs(1);
      }
    }
    if (target.var == kNoVar) {
      arg->add_domain(target.value);
      arg->add_domain(target.value);
    } else {
      arg->add_vars(target.var);
      arg->add_coeffs(-1);
      arg->add_domain(0);
      arg->add_domain(0);
    }
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

      for (const ClosedInterval interval : domain) {
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
  } else if (fz_ct.type == "fzn_disjunctive") {
    const std::vector<VarOrValue> starts =
        LookupVarsOrValues(fz_ct.arguments[0]);
    const std::vector<VarOrValue> sizes =
        LookupVarsOrValues(fz_ct.arguments[1]);

    auto* arg = ct->mutable_cumulative();
    arg->mutable_capacity()->set_offset(1);
    for (int i = 0; i < starts.size(); ++i) {
      arg->add_intervals(
          GetOrCreateOptionalInterval(starts[i], sizes[i], kNoVar));
      arg->add_demands()->set_offset(1);
    }
  } else if (fz_ct.type == "fzn_disjunctive_strict") {
    const std::vector<VarOrValue> starts =
        LookupVarsOrValues(fz_ct.arguments[0]);
    const std::vector<VarOrValue> sizes =
        LookupVarsOrValues(fz_ct.arguments[1]);

    auto* arg = ct->mutable_no_overlap();
    for (int i = 0; i < starts.size(); ++i) {
      arg->add_intervals(
          GetOrCreateOptionalInterval(starts[i], sizes[i], kNoVar));
    }
  } else if (fz_ct.type == "fzn_cumulative") {
    const std::vector<VarOrValue> starts =
        LookupVarsOrValues(fz_ct.arguments[0]);
    const std::vector<VarOrValue> sizes =
        LookupVarsOrValues(fz_ct.arguments[1]);
    const std::vector<VarOrValue> demands =
        LookupVarsOrValues(fz_ct.arguments[2]);

    auto* arg = ct->mutable_cumulative();
    if (fz_ct.arguments[3].HasOneValue()) {
      arg->mutable_capacity()->set_offset(fz_ct.arguments[3].Value());
    } else {
      arg->mutable_capacity()->add_vars(LookupVar(fz_ct.arguments[3]));
      arg->mutable_capacity()->add_coeffs(1);
    }
    for (int i = 0; i < starts.size(); ++i) {
      // Special case for a 0-1 demand, we mark the interval as optional
      // instead and fix the demand to 1.
      if (demands[i].var != kNoVar &&
          proto.variables(demands[i].var).domain().size() == 2 &&
          proto.variables(demands[i].var).domain(0) == 0 &&
          proto.variables(demands[i].var).domain(1) == 1 &&
          fz_ct.arguments[3].HasOneValue() && fz_ct.arguments[3].Value() == 1) {
        arg->add_intervals(
            GetOrCreateOptionalInterval(starts[i], sizes[i], demands[i].var));
        arg->add_demands()->set_offset(1);
      } else {
        arg->add_intervals(
            GetOrCreateOptionalInterval(starts[i], sizes[i], kNoVar));
        LinearExpressionProto* demand = arg->add_demands();
        if (demands[i].var == kNoVar) {
          demand->set_offset(demands[i].value);
        } else {
          demand->add_vars(demands[i].var);
          demand->add_coeffs(1);
        }
      }
    }
  } else if (fz_ct.type == "ortools_cumulative_opt") {
    const std::vector<int> occurs = LookupVars(fz_ct.arguments[0]);
    const std::vector<VarOrValue> starts =
        LookupVarsOrValues(fz_ct.arguments[1]);
    const std::vector<VarOrValue> durations =
        LookupVarsOrValues(fz_ct.arguments[2]);
    const std::vector<VarOrValue> demands =
        LookupVarsOrValues(fz_ct.arguments[3]);

    auto* arg = ct->mutable_cumulative();
    if (fz_ct.arguments[3].HasOneValue()) {
      arg->mutable_capacity()->set_offset(fz_ct.arguments[4].Value());
    } else {
      arg->mutable_capacity()->add_vars(LookupVar(fz_ct.arguments[4]));
      arg->mutable_capacity()->add_coeffs(1);
    }
    for (int i = 0; i < starts.size(); ++i) {
      arg->add_intervals(
          GetOrCreateOptionalInterval(starts[i], durations[i], occurs[i]));
      LinearExpressionProto* demand = arg->add_demands();
      if (demands[i].var == kNoVar) {
        demand->set_offset(demands[i].value);
      } else {
        demand->add_vars(demands[i].var);
        demand->add_coeffs(1);
      }
    }
  } else if (fz_ct.type == "ortools_disjunctive_strict_opt") {
    const std::vector<int> occurs = LookupVars(fz_ct.arguments[0]);
    const std::vector<VarOrValue> starts =
        LookupVarsOrValues(fz_ct.arguments[1]);
    const std::vector<VarOrValue> durations =
        LookupVarsOrValues(fz_ct.arguments[2]);

    auto* arg = ct->mutable_no_overlap();
    for (int i = 0; i < starts.size(); ++i) {
      arg->add_intervals(
          GetOrCreateOptionalInterval(starts[i], durations[i], occurs[i]));
    }
  } else if (fz_ct.type == "fzn_diffn" || fz_ct.type == "fzn_diffn_nonstrict") {
    const bool is_strict = fz_ct.type == "fzn_diffn";
    const std::vector<VarOrValue> x = LookupVarsOrValues(fz_ct.arguments[0]);
    const std::vector<VarOrValue> y = LookupVarsOrValues(fz_ct.arguments[1]);
    const std::vector<VarOrValue> dx = LookupVarsOrValues(fz_ct.arguments[2]);
    const std::vector<VarOrValue> dy = LookupVarsOrValues(fz_ct.arguments[3]);
    const std::vector<int> x_intervals =
        is_strict ? CreateIntervals(x, dx)
                  : CreateNonZeroOrOptionalIntervals(x, dx);
    const std::vector<int> y_intervals =
        is_strict ? CreateIntervals(y, dy)
                  : CreateNonZeroOrOptionalIntervals(y, dy);
    auto* arg = ct->mutable_no_overlap_2d();
    for (int i = 0; i < x.size(); ++i) {
      arg->add_x_intervals(x_intervals[i]);
      arg->add_y_intervals(y_intervals[i]);
    }
  } else if (fz_ct.type == "ortools_network_flow" ||
             fz_ct.type == "ortools_network_flow_cost") {
    // Note that we leave ct empty here (with just the name set).
    // We simply do a linear encoding of this constraint.
    const bool has_cost = fz_ct.type == "ortools_network_flow_cost";
    const std::vector<int> flow = LookupVars(fz_ct.arguments[3]);

    // Flow conservation constraints.
    const int num_nodes = fz_ct.arguments[1].values.size();
    const int base_node = fz_ct.arguments[2].Value();
    std::vector<std::vector<int>> flows_per_node(num_nodes);
    std::vector<std::vector<int>> coeffs_per_node(num_nodes);

    const int num_arcs = fz_ct.arguments[0].values.size() / 2;
    for (int arc = 0; arc < num_arcs; arc++) {
      const int tail = fz_ct.arguments[0].values[2 * arc] - base_node;
      const int head = fz_ct.arguments[0].values[2 * arc + 1] - base_node;
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
        const int64_t weight = fz_ct.arguments[4].values[arc];
        if (weight != 0) {
          arg->add_vars(flow[arc]);
          arg->add_coeffs(weight);
        }
      }
      arg->add_vars(LookupVar(fz_ct.arguments[5]));
      arg->add_coeffs(-1);
    }
  } else {
    LOG(FATAL) << " Not supported " << fz_ct.type;
  }
}  // NOLINT(readability/fn_size)

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
    absl::Span<const fz::Annotation> search_annotations, SolverLogger* logger) {
  std::vector<fz::Annotation> flat_annotations;
  for (const fz::Annotation& annotation : search_annotations) {
    fz::FlattenAnnotations(annotation, &flat_annotations);
  }

  // CP-SAT rejects models containing variables duplicated in hints.
  absl::flat_hash_set<int> hinted_vars;

  for (const fz::Annotation& annotation : flat_annotations) {
    if (annotation.IsFunctionCallWithIdentifier("warm_start")) {
      CHECK_EQ(2, annotation.annotations.size());
      const fz::Annotation& vars = annotation.annotations[0];
      const fz::Annotation& values = annotation.annotations[1];
      if (vars.type != fz::Annotation::VAR_REF_ARRAY ||
          values.type != fz::Annotation::INT_LIST) {
        continue;
      }
      for (int i = 0; i < vars.variables.size(); ++i) {
        fz::Variable* fz_var = vars.variables[i];
        const int var = fz_var_to_index.at(fz_var);
        const int64_t value = values.values[i];
        if (hinted_vars.insert(var).second) {
          proto.mutable_solution_hint()->add_vars(var);
          proto.mutable_solution_hint()->add_values(value);
        }
      }
    } else if (annotation.IsFunctionCallWithIdentifier("int_search") ||
               annotation.IsFunctionCallWithIdentifier("bool_search")) {
      const std::vector<fz::Annotation>& args = annotation.annotations;
      std::vector<fz::Variable*> vars;
      args[0].AppendAllVariables(&vars);

      DecisionStrategyProto* strategy = proto.add_search_strategy();
      for (fz::Variable* v : vars) {
        strategy->add_variables(fz_var_to_index.at(v));
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
        SOLVER_LOG(logger, "Unsupported variable selection strategy '",
                   choose.id, "', falling back to 'smallest'");
        strategy->set_variable_selection_strategy(
            DecisionStrategyProto::CHOOSE_LOWEST_MIN);
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
        SOLVER_LOG(logger, "Unsupported value selection strategy '", select.id,
                   "', falling back to 'indomain_min'");
        strategy->set_domain_reduction_strategy(
            DecisionStrategyProto::SELECT_MIN_VALUE);
      }
    }
  }
}

// The format is fixed in the flatzinc specification.
std::string SolutionString(
    const fz::SolutionOutputSpecs& output,
    const std::function<int64_t(fz::Variable*)>& value_func,
    double objective_value) {
  if (output.variable != nullptr && !output.variable->domain.is_float) {
    const int64_t value = value_func(output.variable);
    if (output.display_as_boolean) {
      return absl::StrCat(output.name, " = ", value == 1 ? "true" : "false",
                          ";");
    } else {
      return absl::StrCat(output.name, " = ", value, ";");
    }
  } else if (output.variable != nullptr && output.variable->domain.is_float) {
    return absl::StrCat(output.name, " = ", objective_value, ";");
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
    const std::function<int64_t(fz::Variable*)>& value_func,
    double objective_value) {
  std::string solution_string;
  for (const auto& output_spec : model.output()) {
    solution_string.append(
        SolutionString(output_spec, value_func, objective_value));
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

void ProcessFloatingPointOVariablesAndObjective(fz::Model* fz_model) {
  // Scan the model, rename int2float to int_eq, change type of the floating
  // point variables to integer.
  for (fz::Constraint* ct : fz_model->constraints()) {
    if (!ct->active) continue;
    if (ct->type == "int2float") {
      ct->type = "int_eq";
      fz::Domain& float_domain = ct->arguments[1].variables[0]->domain;
      float_domain.is_float = false;
      for (const double float_value : float_domain.float_values) {
        float_domain.values.push_back(static_cast<int64_t>(float_value));
      }
      float_domain.float_values.clear();
    }
  }

  // Scan the model to find the float objective variable and the float objective
  // constraint if defined.
  fz::Variable* float_objective_var = nullptr;
  for (fz::Variable* var : fz_model->variables()) {
    if (!var->active) continue;
    if (var->domain.is_float) {
      CHECK(float_objective_var == nullptr);
      float_objective_var = var;
    }
  }

  fz::Constraint* float_objective_ct = nullptr;
  if (float_objective_var != nullptr) {
    for (fz::Constraint* ct : fz_model->constraints()) {
      if (!ct->active) continue;
      if (ct->type == "float_lin_eq") {
        CHECK(float_objective_ct == nullptr);
        float_objective_ct = ct;
        break;
      }
    }
  }

  if (float_objective_ct != nullptr || float_objective_var != nullptr) {
    CHECK(float_objective_ct != nullptr);
    CHECK(float_objective_var != nullptr);
    const int arity = float_objective_ct->arguments[0].Size();
    CHECK_EQ(float_objective_ct->arguments[1].variables[arity - 1],
             float_objective_var);
    CHECK_EQ(float_objective_ct->arguments[0].floats[arity - 1], -1.0);
    for (int i = 0; i + 1 < arity; ++i) {
      fz_model->AddFloatingPointObjectiveTerm(
          float_objective_ct->arguments[1].variables[i],
          float_objective_ct->arguments[0].floats[i]);
    }
    fz_model->SetFloatingPointObjectiveOffset(
        -float_objective_ct->arguments[2].floats[0]);
    fz_model->ClearObjective();
    float_objective_var->active = false;
    float_objective_ct->active = false;
  }
}

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
    if (fz_model.maximize()) {
      objective->set_scaling_factor(-1);
      objective->add_coeffs(-1);
      objective->add_vars(m.fz_var_to_index[fz_model.objective()]);
    } else {
      objective->add_coeffs(1);
      objective->add_vars(m.fz_var_to_index[fz_model.objective()]);
    }
  } else if (!fz_model.float_objective_variables().empty()) {
    FloatObjectiveProto* objective = m.proto.mutable_floating_point_objective();
    for (int i = 0; i < fz_model.float_objective_variables().size(); ++i) {
      objective->add_vars(
          m.fz_var_to_index[fz_model.float_objective_variables()[i]]);
      objective->add_coeffs(fz_model.float_objective_coefficients()[i]);
    }
    objective->set_offset(fz_model.float_objective_offset());
    objective->set_maximize(fz_model.maximize());
  }

  // Fill the search order.
  m.TranslateSearchAnnotations(fz_model.search_annotations(), logger);

  if (p.search_all_solutions && !m.proto.has_objective()) {
    // Enumerate all sat solutions.
    m.parameters.set_enumerate_all_solutions(true);
  }

  m.parameters.set_log_search_progress(p.log_search_progress);
  m.parameters.set_log_to_stdout(!p.ortools_mode);

  // Helps with challenge unit tests.
  m.parameters.set_max_domain_size_when_encoding_eq_neq_constraints(32);

  // Computes the number of workers.
  int num_workers = 1;
  if (p.search_all_solutions && fz_model.objective() == nullptr) {
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

  // Specifies single thread specific search modes.
  if (num_workers == 1 && !p.use_free_search) {  // Fixed search.
    m.parameters.set_search_branching(SatParameters::FIXED_SEARCH);
    m.parameters.set_keep_all_feasible_solutions_in_presolve(true);
  } else if (num_workers == 1 && p.use_free_search) {  // Free search.
    m.parameters.set_search_branching(SatParameters::AUTOMATIC_SEARCH);
    if (!p.search_all_solutions && p.ortools_mode) {
      m.parameters.set_interleave_search(true);
      if (fz_model.objective() != nullptr) {
        m.parameters.add_subsolvers("default_lp");
        m.parameters.add_subsolvers(
            m.proto.search_strategy().empty() ? "quick_restart" : "fixed");
        m.parameters.add_subsolvers("core_or_no_lp"),
            m.parameters.add_subsolvers("max_lp");

      } else {
        m.parameters.add_subsolvers("default_lp");
        m.parameters.add_subsolvers(
            m.proto.search_strategy().empty() ? "probing" : "fixed");
        m.parameters.add_subsolvers("no_lp");
        m.parameters.add_subsolvers("max_lp");
        m.parameters.add_subsolvers("quick_restart");
      }
    }
  } else if (num_workers > 1 && num_workers < 8 && p.ortools_mode) {
    SOLVER_LOG(logger, "Bumping number of workers from ", num_workers, " to 8");
    num_workers = 8;
  }
  m.parameters.set_num_workers(num_workers);

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

  // We only need an observer if 'p.display_all_solutions' or
  // 'p.search_all_solutions' are true.
  std::function<void(const CpSolverResponse&)> solution_observer = nullptr;
  if (p.display_all_solutions || p.search_all_solutions) {
    solution_observer = [&fz_model, &m, &p,
                         solution_logger](const CpSolverResponse& r) {
      const std::string solution_string = SolutionString(
          fz_model,
          [&m, &r](fz::Variable* v) {
            return r.solution(m.fz_var_to_index.at(v));
          },
          r.objective_value());
      SOLVER_LOG(solution_logger, solution_string);
      if (p.display_statistics) {
        OutputFlatzincStats(r, solution_logger);
      }
      SOLVER_LOG(solution_logger, "----------");
    };
  }

  Model sat_model;

  // Setup logging.
  // Note that we need to do that before we start calling the sat functions
  // below that might create a SolverLogger() themselves.
  sat_model.Register<SolverLogger>(logger);

  sat_model.Add(NewSatParameters(m.parameters));
  if (solution_observer != nullptr) {
    sat_model.Add(NewFeasibleSolutionObserver(solution_observer));
  }

  const CpSolverResponse response = SolveCpModel(m.proto, &sat_model);

  // Check the returned solution with the fz model checker.
  if (response.status() == CpSolverStatus::FEASIBLE ||
      response.status() == CpSolverStatus::OPTIMAL) {
    CHECK(CheckSolution(
        fz_model,
        [&response, &m](fz::Variable* v) {
          return response.solution(m.fz_var_to_index.at(v));
        },
        logger));
  }

  // Output the solution in the flatzinc official format.
  if (p.ortools_mode) {
    if (response.status() == CpSolverStatus::FEASIBLE ||
        response.status() == CpSolverStatus::OPTIMAL) {
      if (!p.display_all_solutions && !p.search_all_solutions) {
        // Already printed otherwise.
        const std::string solution_string = SolutionString(
            fz_model,
            [&response, &m](fz::Variable* v) {
              return response.solution(m.fz_var_to_index.at(v));
            },
            response.objective_value());
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
