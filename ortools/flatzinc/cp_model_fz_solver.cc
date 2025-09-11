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
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/btree_map.h"
#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/iterator_adaptors.h"
#include "ortools/base/stl_util.h"
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
ABSL_FLAG(bool, force_interleave_search, false,
          "If true, enable interleaved workers when num_workers is 1.");
ABSL_FLAG(bool, fz_use_light_encoding, false,
          "Use lighter encodings for the model");

// TODO(user): SetIn forces card to be > 0.

namespace operations_research {
namespace sat {

namespace {

static const int kNoVar = std::numeric_limits<int>::min();

struct VarOrValue {
  int var = kNoVar;
  int64_t value = 0;
};

struct SetVariable {
  std::vector<int> var_indices;
  std::vector<int64_t> sorted_values;
  int card_var_index = kNoVar;
  std::optional<int> fixed_card = std::nullopt;

  int find_value_index(int64_t value) const {
    const auto it =
        std::lower_bound(sorted_values.begin(), sorted_values.end(), value);
    if (it == sorted_values.end() || *it != value) return -1;
    return it - sorted_values.begin();
  }
};

// Helper class to convert a flatzinc model to a CpModelProto.
struct CpModelProtoWithMapping {
  CpModelProtoWithMapping(bool sat_enumeration)
      : arena(std::make_unique<google::protobuf::Arena>()),
        proto(*google::protobuf::Arena::Create<CpModelProto>(arena.get())),
        sat_enumeration_(sat_enumeration) {}

  // Returns the index of a new boolean variable.
  int NewBoolVar();

  // Returns the index of a new integer variable.
  int NewIntVar(int64_t min_value, int64_t max_value);

  // Returns a constant CpModelProto variable created on-demand.
  int LookupConstant(int64_t value);

  void AddImplication(absl::Span<const int> e, int l) {
    ConstraintProto* ct = proto.add_constraints();
    for (int e_lit : e) {
      ct->add_enforcement_literal(e_lit);
    }
    ct->mutable_bool_and()->add_literals(l);
  }

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

  // Checks is the domain of the variable is included in [0, 1].
  bool VariableIsBoolean(int var) const;

  // Set variables.
  void ExtractSetConstraint(const fz::Constraint& fz_ct);
  bool ConstraintContainsSetVariables(const fz::Constraint& constraint) const;
  std::shared_ptr<SetVariable> LookupSetVar(const fz::Argument& argument);
  std::shared_ptr<SetVariable> LookupSetVarAt(const fz::Argument& argument,
                                              int pos);

  // literal <=> (var op value).
  int GetOrCreateEncodingLiteral(int var, int64_t value);
  std::optional<int> GetEncodingLiteral(int var, int64_t value);
  void AddVarEqValueLiteral(int var, int64_t value, int literal);
  void AddVarGeValueLiteral(int var, int64_t value, int literal);
  void AddVarGtValueLiteral(int var, int64_t value, int literal);
  void AddVarLeValueLiteral(int var, int64_t value, int literal);
  void AddVarLtValueLiteral(int var, int64_t value, int literal);

  // literal <=> (var1 op var2).
  int GetOrCreateLiteralForVarEqVar(int var1, int var2);
  void AddVarEqVarLiteral(int var1, int var2, int literal);
  std::optional<int> GetVarEqVarLiteral(int var1, int var2);
  int GetOrCreateLiteralForVarNeVar(int var1, int var2);
  std::optional<int> GetVarNeVarLiteral(int var1, int var2);
  void AddVarNeVarLiteral(int var1, int var2, int literal);
  int GetOrCreateLiteralForVarLeVar(int var1, int var2);
  std::optional<int> GetVarLeVarLiteral(int var1, int var2);
  void AddVarLeVarLiteral(int var1, int var2, int literal);
  int GetOrCreateLiteralForVarLtVar(int var1, int var2);
  std::optional<int> GetVarLtVarLiteral(int var1, int var2);
  void AddVarLtVarLiteral(int var1, int var2, int literal);
  void AddVarOrVarLiteral(int var1, int var2, int literal);
  void AddVarAndVarLiteral(int var1, int var2, int literal);

  // Returns the list of literals corresponding to the domain of the variable.
  absl::btree_map<int64_t, int> GetFullEncoding(int var);

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

  // Adds a enforced linear constraint to the model with the given domain.
  LinearConstraintProto* AddLinearConstraint(
      absl::Span<const int> enforcement_literals, const Domain& domain,
      absl::Span<const std::pair<int, int64_t>> terms = {});

  // Adds a lex_less{eq} constraint to the model.
  void AddLexOrdering(absl::Span<const int> x, absl::Span<const int> y,
                      int enforcement_literal, bool accepts_equals);

  // Enforces that x_array != y_array.
  void AddLiteralVectorsNotEqual(absl::Span<const int> x_array,
                                 absl::Span<const int> y_array);

  // This one must be called after the domain has been set.
  void AddTermToLinearConstraint(int var, int64_t coeff,
                                 LinearConstraintProto* ct);
  void FillAMinusBInDomain(const std::vector<int64_t>& domain,
                           const fz::Constraint& fz_ct, ConstraintProto* ct);
  void FillLinearConstraintWithGivenDomain(absl::Span<const int64_t> domain,
                                           const fz::Constraint& fz_ct,
                                           ConstraintProto* ct);
  void BuildTableFromDomainIntLinEq(const fz::Constraint& fz_ct,
                                    ConstraintProto* ct);

  void FirstPass(const fz::Constraint& fz_ct);
  void FillConstraint(const fz::Constraint& fz_ct, ConstraintProto* ct);
  void FillReifiedOrImpliedConstraint(const fz::Constraint& fz_ct);
  void AddAllEncodingConstraints();

  // This function takes a list of set variables, normalize them to have all the
  // same domain then fill var_booleans[i][j] with the booleans encoding the
  // j-th possible value of the i-th set variable.
  void PutSetBooleansInCommonDomain(
      absl::Span<const std::shared_ptr<SetVariable>> set_vars,
      absl::Span<std::vector<int>* const> var_booleans);

  // Translates the flatzinc search annotations into the CpModelProto
  // search_order field.
  void TranslateSearchAnnotations(
      absl::Span<const fz::Annotation> search_annotations,
      SolverLogger* logger);

  // The output proto.
  std::unique_ptr<google::protobuf::Arena> arena;
  CpModelProto& proto;
  SatParameters parameters;
  const bool sat_enumeration_;

  // Mapping from flatzinc variables to CpModelProto variables.
  absl::flat_hash_map<fz::Variable*, int> fz_var_to_index;
  absl::flat_hash_map<int64_t, int> constant_value_to_index;
  absl::flat_hash_map<std::tuple<int, int64_t, int, int64_t, int>, int>
      interval_key_to_index;
  absl::flat_hash_map<int, int> var_to_lit_implies_greater_than_zero;
  // Var op value encoding literals.
  absl::btree_map<int, absl::btree_map<int64_t, int>> encoding_literals;
  absl::btree_map<int, absl::btree_map<int64_t, int>> var_le_value_to_literal;
  // Var op var encoding literals.
  absl::flat_hash_map<std::pair<int, int>, int> var_eq_var_to_literal;
  absl::flat_hash_map<std::pair<int, int>, int> var_lt_var_to_literal;
  absl::flat_hash_map<std::pair<int, int>, int> var_or_var_to_literal;
  absl::flat_hash_map<std::pair<int, int>, int> var_and_var_to_literal;
  // Set variables.
  absl::flat_hash_map<fz::Variable*, std::shared_ptr<SetVariable>>
      set_variables;
  // Statistics.
  int num_var_op_value_reif_cached = 0;
  int num_var_op_var_reif_cached = 0;
  int num_var_op_var_imp_cached = 0;
  int num_full_encodings = 0;
  int num_light_encodings = 0;
  int num_partial_encodings = 0;
};

int CpModelProtoWithMapping::NewBoolVar() { return NewIntVar(0, 1); }

int CpModelProtoWithMapping::NewIntVar(int64_t min_value, int64_t max_value) {
  const int index = proto.variables_size();
  FillDomainInProto(min_value, max_value, proto.add_variables());
  return index;
}

int CpModelProtoWithMapping::LookupConstant(int64_t value) {
  if (constant_value_to_index.contains(value)) {
    return constant_value_to_index[value];
  }

  // Create the constant on the fly.
  const int index = NewIntVar(value, value);
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

bool CpModelProtoWithMapping::VariableIsBoolean(int var) const {
  if (var < 0) var = NegatedRef(var);
  const IntegerVariableProto& var_proto = proto.variables(var);
  return var_proto.domain_size() == 2 && var_proto.domain(0) >= 0 &&
         var_proto.domain(1) <= 1;
}

bool CpModelProtoWithMapping::ConstraintContainsSetVariables(
    const fz::Constraint& constraint) const {
  for (const fz::Argument& argument : constraint.arguments) {
    for (fz::Variable* var : argument.variables) {
      if (var != nullptr && var->domain.is_a_set) return true;
    }
  }
  return false;
}

std::shared_ptr<SetVariable> CpModelProtoWithMapping::LookupSetVar(
    const fz::Argument& argument) {
  if (argument.type == fz::Argument::VAR_REF) {
    CHECK(argument.Var()->domain.is_a_set);
    return set_variables[argument.Var()];
  } else {
    std::shared_ptr<SetVariable> result = std::make_shared<SetVariable>();
    const int true_literal = LookupConstant(1);
    if (argument.type == fz::Argument::INT_INTERVAL) {
      for (int64_t value = argument.values[0]; value <= argument.values[1];
           ++value) {
        result->sorted_values.push_back(value);
        result->var_indices.push_back(true_literal);
      }
    } else if (argument.type == fz::Argument::INT_LIST) {
      for (int64_t value : argument.values) {
        result->sorted_values.push_back(value);
        result->var_indices.push_back(true_literal);
      }
    } else {
      LOG(FATAL) << "LookupSetVar:Unsupported argument type '" << argument.type
                 << "'";
    }
    return result;
  }
}

std::shared_ptr<SetVariable> CpModelProtoWithMapping::LookupSetVarAt(
    const fz::Argument& argument, int pos) {
  if (argument.type == fz::Argument::VAR_REF_ARRAY) {
    CHECK(argument.variables[pos]->domain.is_a_set);
    return set_variables[argument.variables[pos]];
  } else {
    LOG(FATAL) << "LookupSetVarAt:Unsupported argument type '" << argument.type
               << "'";
  }
}

ConstraintProto* CpModelProtoWithMapping::AddEnforcedConstraint(int literal) {
  ConstraintProto* result = proto.add_constraints();
  if (literal != kNoVar) {
    result->add_enforcement_literal(literal);
  }
  return result;
}

LinearConstraintProto* CpModelProtoWithMapping::AddLinearConstraint(
    absl::Span<const int> enforcement_literals, const Domain& domain,
    absl::Span<const std::pair<int, int64_t>> terms) {
  ConstraintProto* result = proto.add_constraints();
  for (const int literal : enforcement_literals) {
    result->add_enforcement_literal(literal);
  }
  FillDomainInProto(domain, result->mutable_linear());
  for (const auto& [var, coeff] : terms) {
    AddTermToLinearConstraint(var, coeff, result->mutable_linear());
  }
  return result->mutable_linear();
}

void CpModelProtoWithMapping::AddTermToLinearConstraint(
    int var, int64_t coeff, LinearConstraintProto* ct) {
  CHECK(!ct->domain().empty());
  if (coeff == 0) return;
  if (var >= 0) {
    ct->add_vars(var);
    ct->add_coeffs(coeff);
  } else {
    ct->add_vars(NegatedRef(var));
    ct->add_coeffs(-coeff);
    // We need to update the domain of the constraint.
    for (int i = 0; i < ct->domain_size(); ++i) {
      const int64_t b = ct->domain(i);
      if (b == std::numeric_limits<int64_t>::min() ||
          b == std::numeric_limits<int64_t>::max()) {
        continue;
      }
      ct->set_domain(i, b - coeff);
    }
  }
}

void CpModelProtoWithMapping::AddLexOrdering(absl::Span<const int> x,
                                             absl::Span<const int> y,
                                             int enforcement_literal,
                                             bool accepts_equals) {
  const int min_size = std::min(x.size(), y.size());
  std::vector<int> hold(min_size + 1);  // Variables indices.
  hold[0] = enforcement_literal;
  for (int i = 1; i < min_size; ++i) hold[i] = NewBoolVar();
  const bool hold_sentinel_is_true =
      x.size() < y.size() || (x.size() == y.size() && accepts_equals);
  hold[min_size] = LookupConstant(hold_sentinel_is_true ? 1 : 0);

  if (!sat_enumeration_ && absl::GetFlag(FLAGS_fz_use_light_encoding)) {
    // Faster version, but can produce duplicate solutions.
    const Domain le_domain(std::numeric_limits<int64_t>::min(), 0);
    const Domain lt_domain(std::numeric_limits<int64_t>::min(), -1);
    for (int i = 0; i < min_size; ++i) {
      // hold[i] => x[i] <= y[i].
      AddLinearConstraint({hold[i]}, le_domain, {{x[i], 1}, {y[i], -1}});

      // hold[i] => x[i] < y[i] || hold[i + 1]
      const int is_lt = NewBoolVar();
      AddLinearConstraint({is_lt}, lt_domain, {{x[i], 1}, {y[i], -1}});

      ConstraintProto* chain = AddEnforcedConstraint(hold[i]);
      chain->mutable_bool_or()->add_literals(is_lt);
      chain->mutable_bool_or()->add_literals(hold[i + 1]);
    }
    ++num_light_encodings;
  } else {
    std::vector<int> is_lt(min_size);
    std::vector<int> is_le(min_size);
    for (int i = 0; i < min_size; ++i) {
      is_le[i] = GetOrCreateLiteralForVarLeVar(x[i], y[i]);
      is_lt[i] = GetOrCreateLiteralForVarLtVar(x[i], y[i]);
    }

    for (int i = 0; i < min_size; ++i) {
      // hold[i] => x[i] <= y[i].
      AddImplication({hold[i]}, is_le[i]);

      // hold[i] => x[i] < y[i] || hold[i + 1]
      ConstraintProto* chain = AddEnforcedConstraint(hold[i]);
      chain->mutable_bool_or()->add_literals(is_lt[i]);
      chain->mutable_bool_or()->add_literals(hold[i + 1]);

      // Optimization.
      if (i + 1 < min_size) {
        // is_lt[i] => no need to look at further hold variables.
        AddImplication({is_lt[i]}, NegatedRef(hold[i + 1]));

        // Propagate the negation of hold[i] to hold[i + 1] to avoid unnecessary
        // branching and disable the chain constraints.
        AddImplication({NegatedRef(hold[i])}, NegatedRef(hold[i + 1]));
      }
    }
  }
}

void CpModelProtoWithMapping::AddLiteralVectorsNotEqual(
    absl::Span<const int> x_array, absl::Span<const int> y_array) {
  const int size = x_array.size();
  CHECK_EQ(size, y_array.size());
  if (sat_enumeration_ || !absl::GetFlag(FLAGS_fz_use_light_encoding)) {
    BoolArgumentProto* at_least_one_different =
        proto.add_constraints()->mutable_bool_or();
    for (int i = 0; i < x_array.size(); ++i) {
      at_least_one_different->add_literals(
          GetOrCreateLiteralForVarNeVar(x_array[i], y_array[i]));
    }
    ++num_light_encodings;
  } else {
    // This version is way faster, but can produce duplicates solution when
    // enumerating solutions in a SAT model.
    std::vector<int> is_ne(size);
    for (int i = 0; i < size; ++i) {
      // Using a simple implication is faster, but it forbids the optimization
      // of the lex ordering.
      is_ne[i] = NewBoolVar();
      AddLinearConstraint({is_ne[i]}, Domain(1),
                          {{x_array[i], 1}, {y_array[i], 1}});
    }

    std::vector<int> hold(size + 1);
    hold[0] = LookupConstant(1);
    for (int i = 1; i < size; ++i) hold[i] = NewBoolVar();
    hold[size] = LookupConstant(0);

    for (int i = 0; i < size; ++i) {
      // hold[i] => x[i] != y[i] || hold[i + 1]
      ConstraintProto* chain = AddEnforcedConstraint(hold[i]);
      chain->mutable_bool_or()->add_literals(is_ne[i]);
      chain->mutable_bool_or()->add_literals(hold[i + 1]);
    }
  }
}

// lit <=> var op value

std::optional<int> CpModelProtoWithMapping::GetEncodingLiteral(int var,
                                                               int64_t value) {
  CHECK_GE(var, 0);
  const IntegerVariableProto& var_proto = proto.variables(var);
  const Domain domain = ReadDomainFromProto(var_proto);

  // We have a few shortcuts where we do not create an encoding.

  // Shortcut 1:Rule out values that are not in the domain.
  if (!domain.Contains(value)) return LookupConstant(0);

  // Shortcut 2: As we have ruled out values that are not in the domain, we can
  // return a true literal if the domain has only one value.
  if (domain.Size() == 1) return LookupConstant(1);

  // Shortcut 3: The encoding of a Boolean variable is itself.
  if (domain.Size() == 2 && domain.Min() == 0 && domain.Max() == 1) {
    return value == 1 ? var : NegatedRef(var);
  }

  const auto it = encoding_literals.find(var);
  if (it != encoding_literals.end()) {
    const auto it2 = it->second.find(value);
    if (it2 != it->second.end()) return it2->second;
  }
  return std::nullopt;
}

int CpModelProtoWithMapping::GetOrCreateEncodingLiteral(int var,
                                                        int64_t value) {
  CHECK_GE(var, 0);
  const std::optional<int> existing_literal = GetEncodingLiteral(var, value);
  if (existing_literal.has_value()) {
    CHECK_NE(existing_literal.value(), kNoVar);
    return existing_literal.value();
  }

  const IntegerVariableProto& var_proto = proto.variables(var);
  const Domain domain = ReadDomainFromProto(var_proto);
  CHECK(domain.Contains(value));
  CHECK(domain.Size() > 2 ||
        (domain.Size() == 2 && (domain.Min() != 0 || domain.Max() != 1)));

  const int lit = NewBoolVar();
  if (domain.Size() == 2) {  // We fully encode the variable.
    encoding_literals[var][domain.Min()] = NegatedRef(lit);
    encoding_literals[var][domain.Max()] = lit;
    return value == domain.Min() ? NegatedRef(lit) : lit;
  } else {
    encoding_literals[var][value] = lit;
    return lit;
  }
}

void CpModelProtoWithMapping::AddVarEqValueLiteral(int var, int64_t value,
                                                   int lit) {
  CHECK_GE(var, 0);
  const std::optional<int> existing_literal = GetEncodingLiteral(var, value);
  if (existing_literal.has_value()) {
    AddLinearConstraint({}, Domain(0),
                        {{lit, 1}, {existing_literal.value(), -1}});
    ++num_var_op_value_reif_cached;
  } else {
    const IntegerVariableProto& var_proto = proto.variables(var);
    const Domain domain = ReadDomainFromProto(var_proto);
    CHECK(domain.Contains(value));
    if (domain.Size() == 2) {
      CHECK(domain.Min() != 0 || domain.Max() != 1);
      if (value == domain.Min()) {
        encoding_literals[var][domain.Min()] = lit;
        encoding_literals[var][domain.Max()] = NegatedRef(lit);
      } else {
        encoding_literals[var][domain.Min()] = NegatedRef(lit);
        encoding_literals[var][domain.Max()] = lit;
      }
    } else {
      encoding_literals[var][value] = lit;
    }
  }
}

void CpModelProtoWithMapping::AddVarGeValueLiteral(int var, int64_t value,
                                                   int lit) {
  AddVarLeValueLiteral(var, value - 1, NegatedRef(lit));
}

void CpModelProtoWithMapping::AddVarGtValueLiteral(int var, int64_t value,
                                                   int lit) {
  AddVarLeValueLiteral(var, value, NegatedRef(lit));
}

void CpModelProtoWithMapping::AddVarLtValueLiteral(int var, int64_t value,
                                                   int lit) {
  AddVarLeValueLiteral(var, value - 1, lit);
}

void CpModelProtoWithMapping::AddVarLeValueLiteral(int var, int64_t value,
                                                   int lit) {
  const Domain domain = ReadDomainFromProto(proto.variables(var));
  const auto it = var_le_value_to_literal.find(var);
  if (it != var_le_value_to_literal.end()) {
    const auto it2 = it->second.find(value);
    if (it2 != it->second.end()) {
      AddLinearConstraint({}, Domain(0), {{lit, 1}, {it2->second, -1}});
      ++num_var_op_value_reif_cached;
      return;
    }
  }
  var_le_value_to_literal[var][value] = lit;
}

absl::btree_map<int64_t, int> CpModelProtoWithMapping::GetFullEncoding(
    int var) {
  absl::btree_map<int64_t, int> result;
  const Domain domain = ReadDomainFromProto(proto.variables(var));
  for (int64_t value : domain.Values()) {
    result[value] = GetOrCreateEncodingLiteral(var, value);
  }
  return result;
}

void CpModelProtoWithMapping::AddAllEncodingConstraints() {
  const bool light_encoding = absl::GetFlag(FLAGS_fz_use_light_encoding);
  for (const auto& [var, encoding] : encoding_literals) {
    const Domain domain = ReadDomainFromProto(proto.variables(var));
    CHECK(domain.Size() > 2 ||
          (domain.Size() == 2 && (domain.Min() != 0 || domain.Max() != 1)));

    if (domain.Size() == encoding.size()) ++num_full_encodings;

    if (domain.Size() == 2) {
      CHECK_EQ(encoding.size(), 2);
      const int lit = encoding.at(domain.Max());
      CHECK_EQ(encoding.at(domain.Min()), NegatedRef(lit));
      AddLinearConstraint({}, Domain(domain.Min()),
                          {{var, 1}, {lit, domain.Min() - domain.Max()}});
      continue;
    }

    if (domain.Size() == encoding.size() && light_encoding) {
      BoolArgumentProto* exo = proto.add_constraints()->mutable_exactly_one();
      for (const auto& [value, lit] : encoding) {
        exo->add_literals(lit);
        AddLinearConstraint({lit}, Domain(value), {{var, 1}});
      }
    } else if (encoding.size() > domain.Size() / 2 && light_encoding) {
      BoolArgumentProto* exo = proto.add_constraints()->mutable_exactly_one();
      // We iterate on the domain to make sure we visit all values.
      for (const int64_t value : domain.Values()) {
        const int lit = GetOrCreateEncodingLiteral(var, value);
        exo->add_literals(lit);
        AddLinearConstraint({lit}, Domain(value), {{var, 1}});
      }
      ++num_partial_encodings;
    } else {
      for (const auto& [value, lit] : encoding) {
        AddLinearConstraint({lit}, Domain(value), {{var, 1}});
        AddLinearConstraint({NegatedRef(lit)}, Domain(value).Complement(),
                            {{var, 1}});
      }
    }
  }

  for (const auto& [var, encoding] : var_le_value_to_literal) {
    const Domain domain = ReadDomainFromProto(proto.variables(var));
    for (const auto& [value, lit] : encoding) {
      // If the value is the min of the max of the domain, it is equivalent to
      // the encoding literal of the variable bounds. In that case, we can
      // link it to the corresponding encoding literal if it exists.
      if (value == domain.Min()) {
        const std::optional<int> encoding =
            GetEncodingLiteral(var, domain.Min());
        if (encoding.has_value()) {
          AddLinearConstraint({}, Domain(0),
                              {{lit, 1}, {encoding.value(), -1}});
          ++num_var_op_value_reif_cached;
        }
      }
      if (value == domain.Max() - 1 && !light_encoding) {
        // In the case of light encoding, !encoding_lit does not propagate.
        const std::optional<int> encoding_lit =
            GetEncodingLiteral(var, domain.Max());
        if (encoding_lit.has_value()) {
          AddLinearConstraint(
              {}, Domain(0),
              {{lit, 1}, {NegatedRef(encoding_lit.value()), -1}});
          ++num_var_op_value_reif_cached;
          continue;
        }
      }

      const Domain le_domain(std::numeric_limits<int64_t>::min(), value);
      AddLinearConstraint({lit}, le_domain, {{var, 1}});
      AddLinearConstraint({NegatedRef(lit)}, le_domain.Complement(),
                          {{var, 1}});
    }
  }
}

// literal <=> var1 op var2

int CpModelProtoWithMapping::GetOrCreateLiteralForVarEqVar(int var1, int var2) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 > var2) std::swap(var1, var2);
  if (var1 == var2) return LookupConstant(1);

  const std::pair<int, int> key = {var1, var2};
  const auto it = var_eq_var_to_literal.find(key);
  if (it != var_eq_var_to_literal.end()) return it->second;

  const int lit = NewBoolVar();
  AddVarEqVarLiteral(var1, var2, lit);
  return lit;
}

int CpModelProtoWithMapping::GetOrCreateLiteralForVarNeVar(int var1, int var2) {
  return NegatedRef(GetOrCreateLiteralForVarEqVar(var1, var2));
}

void CpModelProtoWithMapping::AddVarEqVarLiteral(int var1, int var2, int lit) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 == var2) {
    AddImplication({}, lit);
    return;
  }
  if (var1 > var2) std::swap(var1, var2);

  const std::pair<int, int> key = {var1, var2};
  const auto it = var_eq_var_to_literal.find(key);
  if (it != var_eq_var_to_literal.end()) {
    AddLinearConstraint({}, Domain(0), {{lit, 1}, {it->second, -1}});
    ++num_var_op_var_reif_cached;
  } else {
    if (VariableIsBoolean(var1) && VariableIsBoolean(var2)) {
      AddLinearConstraint({lit}, Domain(0), {{var1, 1}, {var2, -1}});
      AddLinearConstraint({NegatedRef(lit)}, Domain(1), {{var1, 1}, {var2, 1}});
    } else {
      AddLinearConstraint({lit}, Domain(0), {{var1, 1}, {var2, -1}});
      AddLinearConstraint({NegatedRef(lit)}, Domain(0).Complement(),
                          {{var1, 1}, {var2, -1}});
    }
    var_eq_var_to_literal[key] = lit;
  }
}

void CpModelProtoWithMapping::AddVarNeVarLiteral(int var1, int var2, int lit) {
  AddVarEqVarLiteral(var1, var2, NegatedRef(lit));
}

std::optional<int> CpModelProtoWithMapping::GetVarEqVarLiteral(int var1,
                                                               int var2) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 == var2) return LookupConstant(1);
  if (var1 > var2) std::swap(var1, var2);
  const std::pair<int, int> key = {var1, var2};
  const auto it = var_eq_var_to_literal.find(key);
  if (it != var_eq_var_to_literal.end()) return it->second;
  return std::nullopt;
}

std::optional<int> CpModelProtoWithMapping::GetVarNeVarLiteral(int var1,
                                                               int var2) {
  const std::optional<int> lit = GetVarEqVarLiteral(var1, var2);
  if (lit.has_value()) return NegatedRef(lit.value());
  return std::nullopt;
}

int CpModelProtoWithMapping::GetOrCreateLiteralForVarLtVar(int var1, int var2) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 == var2) return LookupConstant(0);

  const std::pair<int, int> key = {var1, var2};
  const auto it = var_lt_var_to_literal.find(key);
  if (it != var_lt_var_to_literal.end()) return it->second;

  const int lit = NewBoolVar();
  AddVarLtVarLiteral(var1, var2, lit);
  return lit;
}

void CpModelProtoWithMapping::AddVarLtVarLiteral(int var1, int var2, int lit) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 == var2) {
    AddImplication({}, NegatedRef(lit));
    return;
  }

  const std::pair<int, int> key = {var1, var2};
  const auto it = var_lt_var_to_literal.find(key);
  if (it != var_lt_var_to_literal.end()) {
    AddLinearConstraint({}, Domain(0), {{lit, 1}, {it->second, -1}});
    ++num_var_op_var_reif_cached;
  } else {
    var_lt_var_to_literal[key] = lit;

    if (VariableIsBoolean(var1) && VariableIsBoolean(var2)) {
      // bool_var => var1 < var2 => !var1 && var2
      BoolArgumentProto* is_lt = AddEnforcedConstraint(lit)->mutable_bool_and();
      is_lt->add_literals(NegatedRef(var1));
      is_lt->add_literals(var2);

      // !bool_var => var1 >= var2 => var1 || !var2
      BoolArgumentProto* is_ge =
          AddEnforcedConstraint(NegatedRef(lit))->mutable_bool_or();
      is_ge->add_literals(var1);
      is_ge->add_literals(NegatedRef(var2));
    } else {
      AddLinearConstraint({lit},
                          Domain(std::numeric_limits<int64_t>::min(), -1),
                          {{var1, 1}, {var2, -1}});
      AddLinearConstraint({NegatedRef(lit)},
                          Domain(0, std::numeric_limits<int64_t>::max()),
                          {{var1, 1}, {var2, -1}});
    }
  }
}

std::optional<int> CpModelProtoWithMapping::GetVarLtVarLiteral(int var1,
                                                               int var2) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 == var2) return LookupConstant(0);
  const std::pair<int, int> key = {var1, var2};
  const auto it = var_lt_var_to_literal.find(key);
  if (it != var_lt_var_to_literal.end()) return it->second;
  return std::nullopt;
}

int CpModelProtoWithMapping::GetOrCreateLiteralForVarLeVar(int var1, int var2) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 == var2) return LookupConstant(1);
  return NegatedRef(GetOrCreateLiteralForVarLtVar(var2, var1));
}

void CpModelProtoWithMapping::AddVarLeVarLiteral(int var1, int var2, int lit) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 == var2) {
    AddImplication({}, lit);
    return;
  }

  AddVarLtVarLiteral(var2, var1, NegatedRef(lit));
}

std::optional<int> CpModelProtoWithMapping::GetVarLeVarLiteral(int var1,
                                                               int var2) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  if (var1 == var2) return LookupConstant(1);

  const std::optional<int> lit = GetVarLtVarLiteral(var2, var1);
  if (lit.has_value()) return NegatedRef(lit.value());
  return std::nullopt;
}

void CpModelProtoWithMapping::AddVarOrVarLiteral(int var1, int var2, int lit) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  CHECK(VariableIsBoolean(var1));
  CHECK(VariableIsBoolean(var2));
  if (var1 == var2) {
    AddLinearConstraint({}, Domain(0), {{var1, 1}, {lit, -1}});
    return;
  }
  if (var1 > var2) std::swap(var1, var2);

  const std::pair<int, int> key = {var1, var2};
  const auto it = var_or_var_to_literal.find(key);
  if (it != var_or_var_to_literal.end()) {
    AddLinearConstraint({}, Domain(0), {{lit, 1}, {it->second, -1}});
    ++num_var_op_var_reif_cached;
  } else {
    AddImplication({var1}, lit);
    AddImplication({var2}, lit);
    AddImplication({NegatedRef(var1), NegatedRef(var2)}, NegatedRef(lit));
    var_or_var_to_literal[key] = lit;
  }
}

void CpModelProtoWithMapping::AddVarAndVarLiteral(int var1, int var2, int lit) {
  CHECK_NE(var1, kNoVar);
  CHECK_NE(var2, kNoVar);
  CHECK(VariableIsBoolean(var1));
  CHECK(VariableIsBoolean(var2));
  if (var1 == var2) {
    AddLinearConstraint({}, Domain(0), {{var1, 1}, {lit, -1}});
    return;
  }
  if (var1 > var2) std::swap(var1, var2);

  const std::pair<int, int> key = {var1, var2};
  const auto it = var_and_var_to_literal.find(key);
  if (it != var_and_var_to_literal.end()) {
    AddLinearConstraint({}, Domain(0), {{lit, 1}, {it->second, -1}});
    ++num_var_op_var_reif_cached;
  } else {
    AddImplication({lit}, var1);
    AddImplication({lit}, var2);
    AddImplication({var1, var2}, lit);
    var_and_var_to_literal[key] = lit;
  }
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
    return interval_index;
  }
}

std::vector<int> CpModelProtoWithMapping::CreateIntervals(
    absl::Span<const VarOrValue> starts, absl::Span<const VarOrValue> sizes) {
  std::vector<int> intervals;
  intervals.reserve(starts.size());
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
  CHECK_GE(domain.Min(), 0);
  if (domain.Min() > 0) return LookupConstant(1);
  if (domain.Max() == 0) {
    return LookupConstant(0);
  }

  const int var_greater_than_zero_lit = NewBoolVar();

  AddLinearConstraint({var_greater_than_zero_lit},
                      domain.IntersectionWith({1, domain.Max()}),
                      {{size.var, 1}});
  AddLinearConstraint({NegatedRef(var_greater_than_zero_lit)}, Domain(0),
                      {{size.var, 1}});

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
    AddTermToLinearConstraint(var_a, 1, arg);
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
    AddTermToLinearConstraint(var_b, 1, arg);
  } else {
    for (const int64_t domain_bound : domain) arg->add_domain(domain_bound);
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[0]), 1, arg);
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[1]), -1, arg);
  }
}

void CpModelProtoWithMapping::FillLinearConstraintWithGivenDomain(
    absl::Span<const int64_t> domain, const fz::Constraint& fz_ct,
    ConstraintProto* ct) {
  auto* arg = ct->mutable_linear();
  for (const int64_t domain_bound : domain) arg->add_domain(domain_bound);
  std::vector<int> vars = LookupVars(fz_ct.arguments[1]);
  for (int i = 0; i < vars.size(); ++i) {
    AddTermToLinearConstraint(vars[i], fz_ct.arguments[0].values[i], arg);
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
      arg->add_literals(var);
    }
    for (const int var : LookupVars(fz_ct.arguments[1])) {
      arg->add_literals(NegatedRef(var));
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
    FillDomainInProto(0, refute);
    AddTermToLinearConstraint(a, 1, refute);
    AddTermToLinearConstraint(b, -1, refute);

    // x => a + b == 1
    AddLinearConstraint({x}, Domain(1), {{a, 1}, {b, 1}});
  } else if (fz_ct.type == "array_bool_xor") {
    auto* arg = ct->mutable_bool_xor();
    for (const int var : LookupVars(fz_ct.arguments[0])) {
      arg->add_literals(var);
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
    FillDomainInProto(1, arg);
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[0]), 1, arg);
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[1]), 1, arg);
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
    if (fz_ct.arguments[2].IsVariable()) {
      FillDomainInProto(0, arg);
      AddTermToLinearConstraint(LookupVar(fz_ct.arguments[2]), -1, arg);
    } else {
      const int64_t v = fz_ct.arguments[2].Value();
      FillDomainInProto(v, arg);
    }
    for (int i = 0; i < vars.size(); ++i) {
      AddTermToLinearConstraint(vars[i], fz_ct.arguments[0].values[i], arg);
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
  } else if (fz_ct.type == "set_card") {
    int64_t set_size = 0;
    if (fz_ct.arguments[0].type == fz::Argument::INT_LIST) {
      set_size = fz_ct.arguments[0].values.size();
    } else if (fz_ct.arguments[0].type == fz::Argument::INT_INTERVAL) {
      set_size =
          fz_ct.arguments[0].values[1] - fz_ct.arguments[0].values[0] + 1;
    } else {
      LOG(FATAL) << "Wrong format" << fz_ct.DebugString();
    }

    auto* arg = ct->mutable_linear();
    FillDomainInProto(set_size, arg);
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[1]), 1, arg);
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
      FillDomainInProto(fz_ct.arguments[1].values[0],
                        fz_ct.arguments[1].values[1], arg);
    } else {
      LOG(FATAL) << "Wrong format";
    }
  } else if (fz_ct.type == "set_in_negated") {
    auto* arg = ct->mutable_linear();
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
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[0]), 1, arg);
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
    FillDomainInProto(0, arg);
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[0]), 1, arg);
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[1]), 1, arg);
    AddTermToLinearConstraint(LookupVar(fz_ct.arguments[2]), -1, arg);
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
  } else if (fz_ct.type == "fzn_value_precede_int") {
    const int64_t before = fz_ct.arguments[0].Value();
    const int64_t after = fz_ct.arguments[1].Value();
    const std::vector<int> x = LookupVars(fz_ct.arguments[2]);
    if (x.empty()) return;

    std::vector<int> hold(x.size());
    hold[0] = LookupConstant(0);
    for (int i = 1; i < x.size(); ++i) hold[i] = NewBoolVar();

    if (absl::GetFlag(FLAGS_fz_use_light_encoding) && !sat_enumeration_) {
      for (int i = 0; i < x.size(); ++i) {
        const int is_before = GetOrCreateEncodingLiteral(x[i], before);
        if (i + 1 < x.size()) {
          AddImplication({is_before}, hold[i + 1]);
          AddLinearConstraint({NegatedRef(is_before)}, Domain(0),
                              {{hold[i], 1}, {hold[i + 1], -1}});
        }
        AddLinearConstraint({NegatedRef(hold[i])}, Domain(after).Complement(),
                            {{x[i], 1}});
      }
      ++num_light_encodings;
    } else {
      for (int i = 0; i < x.size(); ++i) {
        const int is_before = GetOrCreateEncodingLiteral(x[i], before);
        const int is_after = GetOrCreateEncodingLiteral(x[i], after);
        if (i + 1 < x.size()) {
          AddImplication({is_before}, hold[i + 1]);
          AddLinearConstraint({NegatedRef(is_before)}, Domain(0),
                              {{hold[i], 1}, {hold[i + 1], -1}});
        }
        AddImplication({NegatedRef(hold[i])}, NegatedRef(is_after));
      }
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
      }
    }

    if (target.var == kNoVar) {
      FillDomainInProto(target.value - fixed_contributions, arg);
    } else {
      FillDomainInProto(-fixed_contributions, arg);
      AddTermToLinearConstraint(target.var, -1, arg);
    }

    for (const VarOrValue& count : counts) {
      if (count.var != kNoVar) {
        AddTermToLinearConstraint(GetOrCreateEncodingLiteral(count.var, value),
                                  1, arg);
      }
    }
  } else if (fz_ct.type == "ortools_count_eq") {
    const std::vector<VarOrValue> counts =
        LookupVarsOrValues(fz_ct.arguments[0]);
    const int var = LookupVar(fz_ct.arguments[1]);
    const VarOrValue target = LookupVarOrValue(fz_ct.arguments[2]);
    LinearConstraintProto* arg = ct->mutable_linear();
    if (target.var == kNoVar) {
      FillDomainInProto(target.value, arg);
    } else {
      FillDomainInProto(0, arg);
      AddTermToLinearConstraint(target.var, -1, arg);
    }
    for (const VarOrValue& count : counts) {
      const int bool_var = count.var == kNoVar
                               ? GetOrCreateEncodingLiteral(var, count.value)
                               : GetOrCreateLiteralForVarEqVar(var, count.var);
      AddTermToLinearConstraint(bool_var, 1, arg);
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
      domain = domain.IntersectionWith({min_index, max_index});
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
            FillDomainInProto(0, 1, new_var);
          }

          // Add the arc.
          circuit_arg->add_tails(index);
          circuit_arg->add_heads(value);
          circuit_arg->add_literals(literal);

          AddLinearConstraint({literal}, Domain(value), {{var, 1}});
          AddLinearConstraint({NegatedRef(literal)}, Domain(value).Complement(),
                              {{var, 1}});
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
  } else if (fz_ct.type == "ortools_lex_less_int" ||
             fz_ct.type == "ortools_lex_less_bool" ||
             fz_ct.type == "ortools_lex_lesseq_int" ||
             fz_ct.type == "ortools_lex_lesseq_bool") {
    const std::vector<int> x = LookupVars(fz_ct.arguments[0]);
    const std::vector<int> y = LookupVars(fz_ct.arguments[1]);
    const bool accepts_equals = fz_ct.type == "ortools_lex_lesseq_bool" ||
                                fz_ct.type == "ortools_lex_lesseq_int";
    const int false_literal = LookupConstant(0);
    AddLexOrdering(x, y, NegatedRef(false_literal), accepts_equals);
  } else if (fz_ct.type == "ortools_precede_chain_int") {
    std::vector<int64_t> values;
    if (fz_ct.arguments[0].type == fz::Argument::INT_INTERVAL) {
      values.reserve(fz_ct.arguments[0].values[1] -
                     fz_ct.arguments[0].values[0] + 1);
      for (int64_t value = fz_ct.arguments[0].values[0];
           value <= fz_ct.arguments[0].values[1]; ++value) {
        values.push_back(value);
      }
    } else if (fz_ct.arguments[0].type == fz::Argument::INT_LIST) {
      values = fz_ct.arguments[0].values;
    } else {
      LOG(FATAL) << "Unsupported argument type: " << fz_ct.arguments[0].type
                 << " for " << fz_ct.arguments[0].DebugString();
    }
    const std::vector<int> x = LookupVars(fz_ct.arguments[1]);

    if (x.empty()) return;
    if (values.size() <= 1) return;

    std::vector<int> row;
    for (int r = 0; r + 1 < values.size(); ++r) {
      row.clear();
      const int64_t before = values[r];
      const int64_t after = values[r + 1];
      row.resize(x.size());
      for (int i = 0; i < x.size(); ++i) {
        row[i] = (i <= r ? LookupConstant(0) : NewBoolVar());
      }

      for (int i = 0; i < x.size(); ++i) {
        const int is_before = GetOrCreateEncodingLiteral(x[i], before);
        const int is_after = GetOrCreateEncodingLiteral(x[i], after);
        if (i + 1 < x.size()) {
          AddImplication({is_before}, row[i + 1]);
          AddLinearConstraint({NegatedRef(is_before)}, Domain(0),
                              {{row[i], 1}, {row[i + 1], -1}});
        }
        AddImplication({NegatedRef(row[i])}, NegatedRef(is_after));
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
      auto* arg =
          AddLinearConstraint({}, Domain(fz_ct.arguments[1].values[node]));
      for (int i = 0; i < flows_per_node[node].size(); ++i) {
        AddTermToLinearConstraint(flows_per_node[node][i],
                                  coeffs_per_node[node][i], arg);
      }
    }

    if (has_cost) {
      auto* arg = AddLinearConstraint({}, Domain(0));
      for (int arc = 0; arc < num_arcs; arc++) {
        AddTermToLinearConstraint(flow[arc], fz_ct.arguments[4].values[arc],
                                  arg);
      }
      AddTermToLinearConstraint(LookupVar(fz_ct.arguments[5]), -1, arg);
    }
  } else if (fz_ct.type == "ortools_bin_packing") {
    const int capacity = fz_ct.arguments[0].Value();
    const std::vector<int> positions = LookupVars(fz_ct.arguments[1]);
    CHECK_EQ(fz_ct.arguments[2].type, fz::Argument::INT_LIST);
    const std::vector<int64_t> weights = fz_ct.arguments[2].values;

    std::vector<absl::btree_map<int64_t, int>> bin_encodings;
    absl::btree_set<int64_t> all_bin_indices;
    bin_encodings.reserve(positions.size());
    for (int p = 0; p < positions.size(); ++p) {
      absl::btree_map<int64_t, int> encoding = GetFullEncoding(positions[p]);
      for (const auto& [value, literal] : encoding) {
        all_bin_indices.insert(value);
      }
      bin_encodings.push_back(std::move(encoding));
    }

    for (const int b : all_bin_indices) {
      LinearConstraintProto* lin = AddLinearConstraint({}, {0, capacity});
      for (int p = 0; p < positions.size(); ++p) {
        const auto it = bin_encodings[p].find(b);
        if (it != bin_encodings[p].end()) {
          AddTermToLinearConstraint(it->second, weights[p], lin);
        }
      }
    }
  } else if (fz_ct.type == "ortools_bin_packing_capa") {
    const std::vector<int64_t>& capacities = fz_ct.arguments[0].values;
    const std::vector<int> bins = LookupVars(fz_ct.arguments[1]);
    CHECK_EQ(fz_ct.arguments[2].type, fz::Argument::INT_INTERVAL);
    const int first_bin = fz_ct.arguments[2].values[0];
    const int last_bin = fz_ct.arguments[2].values[1];
    CHECK_EQ(fz_ct.arguments[3].type, fz::Argument::INT_LIST);
    const std::vector<int64_t> weights = fz_ct.arguments[3].values;

    std::vector<absl::btree_map<int64_t, int>> bin_encodings;
    bin_encodings.reserve(bins.size());
    for (int bin = 0; bin < bins.size(); ++bin) {
      absl::btree_map<int64_t, int> encoding = GetFullEncoding(bins[bin]);
      for (const auto& [value, literal] : encoding) {
        if (value < first_bin || value > last_bin) {
          AddImplication({}, NegatedRef(literal));  // not a valid index.
        }
      }
      bin_encodings.push_back(std::move(encoding));
    }

    for (int b = first_bin; b <= last_bin; ++b) {
      LinearConstraintProto* lin =
          AddLinearConstraint({}, {0, capacities[b - first_bin]});
      for (int bin = 0; bin < bins.size(); ++bin) {
        const auto it = bin_encodings[bin].find(b);
        if (it != bin_encodings[bin].end()) {
          AddTermToLinearConstraint(it->second, weights[bin], lin);
        }
      }
    }
  } else if (fz_ct.type == "ortools_bin_packing_load") {
    const std::vector<int>& loads = LookupVars(fz_ct.arguments[0]);
    const std::vector<int> positions = LookupVars(fz_ct.arguments[1]);
    if (fz_ct.arguments[3].values.empty()) {  // No items.
      for (const int load : loads) {
        AddLinearConstraint({}, {0, 0}, {{load, 1}});
      }
      return;
    }
    CHECK_EQ(fz_ct.arguments[2].type, fz::Argument::INT_INTERVAL);
    const int first_bin = fz_ct.arguments[2].values[0];
    const int last_bin = fz_ct.arguments[2].values[1];
    CHECK_EQ(fz_ct.arguments[3].type, fz::Argument::INT_LIST);
    const std::vector<int64_t> weights = fz_ct.arguments[3].values;
    CHECK_EQ(weights.size(), positions.size());
    CHECK_EQ(loads.size(), last_bin - first_bin + 1);

    std::vector<absl::btree_map<int64_t, int>> bin_encodings;
    bin_encodings.reserve(positions.size());
    for (int p = 0; p < positions.size(); ++p) {
      absl::btree_map<int64_t, int> encoding = GetFullEncoding(positions[p]);
      for (const auto& [value, literal] : encoding) {
        if (value < first_bin || value > last_bin) {
          AddImplication({}, NegatedRef(literal));  // not a valid index.
        }
      }
      bin_encodings.push_back(std::move(encoding));
    }

    for (int b = first_bin; b <= last_bin; ++b) {
      LinearConstraintProto* lin = AddLinearConstraint({}, Domain(0));
      AddTermToLinearConstraint(loads[b - first_bin], -1, lin);
      for (int p = 0; p < positions.size(); ++p) {
        const auto it = bin_encodings[p].find(b);
        if (it != bin_encodings[p].end()) {
          AddTermToLinearConstraint(it->second, weights[p], lin);
        }
      }
    }

    // Redundant constraint.
    int64_t total_load = 0;
    for (int64_t weight : weights) {
      total_load += weight;
    }
    LinearConstraintProto* lin = AddLinearConstraint({}, Domain(total_load));
    for (int i = 0; i < loads.size(); ++i) {
      AddTermToLinearConstraint(loads[i], 1, lin);
    }
  } else if (fz_ct.type == "ortools_nvalue") {
    const int card = LookupVar(fz_ct.arguments[0]);
    const std::vector<int>& x = LookupVars(fz_ct.arguments[1]);

    LinearConstraintProto* global_cardinality = ct->mutable_linear();
    FillDomainInProto(x.size(), global_cardinality);

    absl::btree_map<int64_t, std::vector<int>> value_to_literals;
    for (int i = 0; i < x.size(); ++i) {
      const absl::btree_map<int64_t, int> encoding = GetFullEncoding(x[i]);
      for (const auto& [value, literal] : encoding) {
        value_to_literals[value].push_back(literal);
        AddTermToLinearConstraint(literal, 1, global_cardinality);
      }
    }

    // Constrain the range of the card variable.
    const int64_t max_size = std::min(x.size(), value_to_literals.size());
    AddLinearConstraint({}, {1, max_size}, {{card, 1}});

    LinearConstraintProto* lin =
        AddLinearConstraint({}, Domain(0), {{card, -1}});
    for (const auto& [value, literals] : value_to_literals) {
      const int is_present = NewBoolVar();
      AddTermToLinearConstraint(is_present, 1, lin);

      BoolArgumentProto* is_present_constraint =
          AddEnforcedConstraint(is_present)->mutable_bool_or();
      for (const int literal : literals) {
        AddImplication({literal}, is_present);
        is_present_constraint->add_literals(literal);
      }
    }
  } else if (fz_ct.type == "ortools_global_cardinality") {
    const std::vector<int> x = LookupVars(fz_ct.arguments[0]);
    CHECK_EQ(fz_ct.arguments[1].type, fz::Argument::INT_LIST);
    const std::vector<int64_t>& values = fz_ct.arguments[1].values;
    const std::vector<int> cards = LookupVars(fz_ct.arguments[2]);
    const bool is_closed = fz_ct.arguments[3].Value() != 0;

    const absl::flat_hash_set<int64_t> all_values(values.begin(), values.end());
    absl::flat_hash_map<int64_t, std::vector<int>> value_to_literals;
    bool exact_cover = true;

    for (const int x_var : x) {
      const absl::btree_map<int64_t, int> encoding = GetFullEncoding(x_var);
      for (const auto& [value, literal] : encoding) {
        if (all_values.contains(value)) {
          value_to_literals[value].push_back(literal);
        } else if (is_closed) {
          AddImplication({}, NegatedRef(literal));
        } else {
          exact_cover = false;
        }
      }
    }

    for (int i = 0; i < cards.size(); ++i) {
      const int64_t value = values[i];
      const int card = cards[i];
      LinearConstraintProto* lin =
          AddLinearConstraint({}, Domain(0), {{card, -1}});
      for (const int literal : value_to_literals[value]) {
        AddTermToLinearConstraint(literal, 1, lin);
      }
    }

    if (exact_cover) {
      LinearConstraintProto* cover = AddLinearConstraint({}, Domain(x.size()));
      for (const int card : cards) {
        AddTermToLinearConstraint(card, 1, cover);
      }
    }
  } else if (fz_ct.type == "ortools_global_cardinality_low_up") {
    const std::vector<int> x = LookupVars(fz_ct.arguments[0]);
    CHECK(fz_ct.arguments[1].type == fz::Argument::INT_LIST ||
          fz_ct.arguments[1].type == fz::Argument::VOID_ARGUMENT);
    const std::vector<int64_t>& values = fz_ct.arguments[1].values;
    absl::Span<const int64_t> lbs = fz_ct.arguments[2].values;
    absl::Span<const int64_t> ubs = fz_ct.arguments[3].values;
    const bool is_closed = fz_ct.arguments[4].Value() != 0;

    const absl::flat_hash_set<int64_t> all_values(values.begin(), values.end());
    absl::flat_hash_map<int64_t, std::vector<int>> value_to_literals;
    bool exact_cover = true;

    for (const int x_var : x) {
      const absl::btree_map<int64_t, int> encoding = GetFullEncoding(x_var);
      for (const auto& [value, literal] : encoding) {
        if (all_values.contains(value)) {
          value_to_literals[value].push_back(literal);
        } else if (is_closed) {
          AddImplication({}, NegatedRef(literal));
        } else {
          exact_cover = false;
        }
      }
    }

    // Optimization: if sum(lbs) == length(x), then we can reduce ubs to lbs,
    // and vice versa. the constraint is redundant.
    const int64_t sum_lbs = absl::c_accumulate(lbs, 0);
    const int64_t sum_ubs = absl::c_accumulate(ubs, 0);
    if (exact_cover && sum_lbs == x.size()) {
      ubs = lbs;
    } else if (exact_cover && sum_ubs == x.size()) {
      lbs = ubs;
    }

    for (int i = 0; i < values.size(); ++i) {
      LinearConstraintProto* lin = AddLinearConstraint({}, {lbs[i], ubs[i]});
      for (const int literal : value_to_literals[values[i]]) {
        AddTermToLinearConstraint(literal, 1, lin);
      }
    }
  } else {
    LOG(FATAL) << " Not supported " << fz_ct.type;
  }
}  // NOLINT(readability/fn_size)

void CpModelProtoWithMapping::PutSetBooleansInCommonDomain(
    absl::Span<const std::shared_ptr<SetVariable>> set_vars,
    absl::Span<std::vector<int>* const> var_booleans) {
  CHECK_EQ(set_vars.size(), var_booleans.size());
  std::vector<int64_t> all_values;
  for (const std::shared_ptr<SetVariable>& set_var : set_vars) {
    all_values.insert(all_values.end(), set_var->sorted_values.begin(),
                      set_var->sorted_values.end());
  }
  gtl::STLSortAndRemoveDuplicates(&all_values);
  absl::flat_hash_map<int64_t, int> value_to_index;
  for (int i = 0; i < all_values.size(); ++i) {
    value_to_index[all_values[i]] = i;
  }
  const int false_literal = LookupConstant(0);
  for (int set_var_index = 0; set_var_index < var_booleans.size();
       ++set_var_index) {
    std::vector<int>* literals = var_booleans[set_var_index];
    std::shared_ptr<SetVariable> set_var = set_vars[set_var_index];
    *literals = std::vector<int>(all_values.size(), false_literal);
    for (int i = 0; i < set_var->sorted_values.size(); ++i) {
      (*literals)[value_to_index[set_var->sorted_values[i]]] =
          set_var->var_indices[i];
    }
  }
}

void CpModelProtoWithMapping::FirstPass(const fz::Constraint& fz_ct) {
  if (fz_ct.type == "set_card") {
    if (!fz_ct.arguments[0].IsVariable() ||
        !fz_ct.arguments[0].Var()->domain.is_a_set ||
        !fz_ct.arguments[1].HasOneValue()) {
      return;
    }
    std::shared_ptr<SetVariable> set_var = LookupSetVar(fz_ct.arguments[0]);
    const int64_t fixed_card = fz_ct.arguments[1].Value();
    set_var->fixed_card = fixed_card;
  }
}

void CpModelProtoWithMapping::ExtractSetConstraint(
    const fz::Constraint& fz_ct) {
  if (fz_ct.type == "array_set_element") {
    const int index = LookupVar(fz_ct.arguments[0]);
    const Domain index_domain = ReadDomainFromProto(proto.variables(index));
    const std::shared_ptr<SetVariable> target_var =
        LookupSetVar(fz_ct.arguments[2]);
    const absl::flat_hash_set<int64_t> target_values(
        target_var->sorted_values.begin(), target_var->sorted_values.end());
    absl::flat_hash_map<int64_t, std::vector<int64_t>> value_to_supports;
    const int64_t min_index = index_domain.Min();
    for (const int64_t value : index_domain.Values()) {
      const int64_t offset = value - min_index;
      bool index_is_valid = true;
      const fz::Domain& set_domain = fz_ct.arguments[1].domains[offset];
      if (set_domain.is_interval) {
        for (int64_t v = set_domain.values[0]; v <= set_domain.values[1]; ++v) {
          if (!target_values.contains(v)) {
            index_is_valid = false;
            break;
          }
        }
        if (!index_is_valid) continue;

        for (int64_t v = set_domain.values[0]; v <= set_domain.values[1]; ++v) {
          value_to_supports[v].push_back(value);
        }
      } else {
        for (const int64_t v : set_domain.values) {
          if (!target_values.contains(v)) {
            index_is_valid = false;
            break;
          }
        }
        if (!index_is_valid) continue;

        for (const int64_t v : set_domain.values) {
          value_to_supports[v].push_back(value);
        }
      }
    }

    for (int i = 0; i < target_var->sorted_values.size(); ++i) {
      const int64_t target_value = target_var->sorted_values[i];
      const int target_literal = target_var->var_indices[i];
      const auto it = value_to_supports.find(target_value);
      if (it == value_to_supports.end()) {
        proto.add_constraints()->mutable_bool_and()->add_literals(
            NegatedRef(target_literal));
      } else if (it->second.size() == index_domain.Size()) {
        proto.add_constraints()->mutable_bool_and()->add_literals(
            target_literal);
      } else {
        const Domain true_indices = Domain::FromValues(it->second);
        AddLinearConstraint({target_literal}, true_indices, {{index, 1}});
        AddLinearConstraint({NegatedRef(target_literal)},
                            true_indices.Complement(), {{index, 1}});
      }
    }
  } else if (fz_ct.type == "array_var_set_element") {
    const int index = LookupVar(fz_ct.arguments[0]);
    const Domain index_domain = ReadDomainFromProto(proto.variables(index));
    const int64_t min_index = index_domain.Min();

    const std::shared_ptr<SetVariable> target_var =
        LookupSetVar(fz_ct.arguments[2]);
    absl::btree_map<int64_t, int> target_values_to_literals;
    for (int i = 0; i < target_var->sorted_values.size(); ++i) {
      target_values_to_literals[target_var->sorted_values[i]] =
          target_var->var_indices[i];
    }

    BoolArgumentProto* exactly_one =
        proto.add_constraints()->mutable_exactly_one();
    for (const int64_t value : index_domain.Values()) {
      const int index_literal = GetOrCreateEncodingLiteral(index, value);
      exactly_one->add_literals(index_literal);

      std::shared_ptr<SetVariable> set_var =
          LookupSetVarAt(fz_ct.arguments[1], value - min_index);
      absl::btree_map<int64_t, int> set_values_to_literals;
      for (int i = 0; i < set_var->sorted_values.size(); ++i) {
        set_values_to_literals[set_var->sorted_values[i]] =
            set_var->var_indices[i];
      }

      for (const auto& [value, set_literal] : set_values_to_literals) {
        const auto it = target_values_to_literals.find(value);
        if (it == target_values_to_literals.end()) {
          // index is selected => set_literal[value] is false.
          AddImplication({index_literal}, NegatedRef(set_literal));
        } else {
          // index is selected => set_literal[value] == target_literal[value].
          AddImplication({index_literal, set_literal}, it->second);
          AddImplication({index_literal, it->second}, set_literal);
        }
      }

      // Properly handle the case where not all target literals are reached.
      for (const auto& [value, target_literal] : target_values_to_literals) {
        if (!set_values_to_literals.contains(value)) {
          AddImplication({index_literal}, NegatedRef(target_literal));
        }
      }
    }
  } else if (fz_ct.type == "fzn_all_different_set") {
    const int num_vars = fz_ct.arguments[0].Size();
    if (num_vars == 0) return;

    std::vector<std::shared_ptr<SetVariable>> set_vars;
    set_vars.reserve(num_vars);
    for (int i = 0; i < num_vars; ++i) {
      set_vars.push_back(LookupSetVarAt(fz_ct.arguments[0], i));
    }
    std::vector<std::vector<int>> var_booleans(num_vars);
    std::vector<std::vector<int>*> var_booleans_ptrs(num_vars);
    for (int i = 0; i < set_vars.size(); ++i) {
      var_booleans_ptrs[i] = &var_booleans[i];
    }
    PutSetBooleansInCommonDomain(set_vars, var_booleans_ptrs);
    for (int i = 0; i + 1 < num_vars; ++i) {
      for (int j = i + 1; j < num_vars; ++j) {
        AddLiteralVectorsNotEqual(var_booleans[i], var_booleans[j]);
      }
    }
  } else if (fz_ct.type == "fzn_all_disjoint") {
    const int num_vars = fz_ct.arguments[0].Size();
    if (num_vars == 0) return;

    absl::btree_map<int64_t, std::vector<int>> value_to_candidates;
    for (int i = 0; i < num_vars; ++i) {
      const std::shared_ptr<SetVariable> set_var =
          LookupSetVarAt(fz_ct.arguments[0], i);
      for (int j = 0; j < set_var->sorted_values.size(); ++j) {
        const int64_t value = set_var->sorted_values[j];
        const int literal = set_var->var_indices[j];
        value_to_candidates[value].push_back(literal);
      }
    }
    for (const auto& [value, candidates] : value_to_candidates) {
      BoolArgumentProto* amo = proto.add_constraints()->mutable_at_most_one();
      for (const int literal : candidates) {
        amo->add_literals(literal);
      }
    }
  } else if (fz_ct.type == "fzn_disjoint") {
    std::vector<int> x_literals, y_literals;
    PutSetBooleansInCommonDomain(
        {LookupSetVar(fz_ct.arguments[0]), LookupSetVar(fz_ct.arguments[1])},
        {&x_literals, &y_literals});
    for (int i = 0; i < x_literals.size(); ++i) {
      BoolArgumentProto* at_least_one_false =
          proto.add_constraints()->mutable_bool_or();
      at_least_one_false->add_literals(NegatedRef(x_literals[i]));
      at_least_one_false->add_literals(NegatedRef(y_literals[i]));
    }
  } else if (fz_ct.type == "fzn_partition_set") {
    const int num_vars = fz_ct.arguments[0].Size();
    if (num_vars == 0) return;

    absl::flat_hash_set<int64_t> universe;
    if (fz_ct.arguments[1].type == fz::Argument::INT_INTERVAL) {
      for (int64_t v = fz_ct.arguments[1].values[0];
           v <= fz_ct.arguments[1].values[1]; ++v) {
        universe.insert(v);
      }
    } else {
      CHECK_EQ(fz_ct.arguments[1].type, fz::Argument::INT_LIST);
      universe = absl::flat_hash_set<int64_t>(fz_ct.arguments[1].values.begin(),
                                              fz_ct.arguments[1].values.end());
    }

    absl::btree_map<int64_t, std::vector<int>> value_to_candidates;
    for (int i = 0; i < num_vars; ++i) {
      const std::shared_ptr<SetVariable> set_var =
          LookupSetVarAt(fz_ct.arguments[0], i);
      for (int j = 0; j < set_var->sorted_values.size(); ++j) {
        const int64_t value = set_var->sorted_values[j];
        const int literal = set_var->var_indices[j];
        if (!universe.contains(value)) {
          AddImplication({}, NegatedRef(literal));
        } else {
          value_to_candidates[value].push_back(literal);
        }
      }
    }
    for (const auto& [value, candidates] : value_to_candidates) {
      BoolArgumentProto* ex1 = proto.add_constraints()->mutable_exactly_one();
      for (const int literal : candidates) {
        ex1->add_literals(literal);
      }
    }
  } else if (fz_ct.type == "set_card") {
    std::shared_ptr<SetVariable> set_var = LookupSetVar(fz_ct.arguments[0]);
    VarOrValue var_or_value = LookupVarOrValue(fz_ct.arguments[1]);
    if (set_var->card_var_index == kNoVar) {
      ConstraintProto* ct = proto.add_constraints();
      if (var_or_value.var == kNoVar) {
        set_var->card_var_index = LookupConstant(var_or_value.value);
        FillDomainInProto(var_or_value.value, ct->mutable_linear());
      } else {
        set_var->card_var_index = var_or_value.var;
        FillDomainInProto(0, ct->mutable_linear());
        AddTermToLinearConstraint(var_or_value.var, -1, ct->mutable_linear());
      }
      for (const int bool_var : set_var->var_indices) {
        AddTermToLinearConstraint(bool_var, 1, ct->mutable_linear());
      }
    } else {
      if (var_or_value.var == kNoVar) {
        AddLinearConstraint({}, Domain(var_or_value.value),
                            {{set_var->card_var_index, 1}});
      } else {
        AddLinearConstraint(
            {}, Domain(0),
            {{set_var->card_var_index, 1}, {var_or_value.var, -1}});
      }
    }
  } else if (fz_ct.type == "set_in") {
    const VarOrValue var_or_value = LookupVarOrValue(fz_ct.arguments[0]);
    std::shared_ptr<SetVariable> set_var = LookupSetVar(fz_ct.arguments[1]);
    if (var_or_value.var == kNoVar) {
      const int index = set_var->find_value_index(var_or_value.value);
      // TODO(user): Improve behavior and reporting when failing.
      CHECK_NE(index, -1);
      AddImplication({}, set_var->var_indices[index]);
    } else if (set_var->fixed_card.has_value() &&
               set_var->fixed_card.value() == 1) {
      // We have a one to one mapping between the set_var and the full encoding
      // of the variable.
      //
      // Cache the mapping of set_var values to literals.
      absl::flat_hash_map<int64_t, int> set_value_to_literal;
      for (int i = 0; i < set_var->sorted_values.size(); ++i) {
        set_value_to_literal[set_var->sorted_values[i]] =
            set_var->var_indices[i];
      }

      // Reduce the domain of the target variable.
      AddLinearConstraint({}, Domain::FromValues(set_var->sorted_values),
                          {{var_or_value.var, 1}});

      absl::flat_hash_set<int> var_values;
      const Domain var_domain =
          ReadDomainFromProto(proto.variables(var_or_value.var));
      for (const int64_t value : var_domain.Values()) {
        const auto it = set_value_to_literal.find(value);
        // Non-covered values are fixed to 0 by the above domain reduction.
        if (it == set_value_to_literal.end()) continue;
        AddVarEqValueLiteral(var_or_value.var, value, it->second);
        var_values.insert(value);
      }

      // Zero out set literals not covered by the full encoding.
      for (int i = 0; i < set_var->sorted_values.size(); ++i) {
        if (!var_values.contains(set_var->sorted_values[i])) {
          AddImplication({}, NegatedRef(set_var->var_indices[i]));
        }
      }
    } else {
      // We express `v \in set({v_i}, {b_i})` by N+1 constraints:
      // v \in {v_i}
      // (v == v_i) => b_i
      //
      // Reduce the domain of the target variable.
      AddLinearConstraint({}, Domain::FromValues(set_var->sorted_values),
                          {{var_or_value.var, 1}});

      // Then propagate any remove from the set domain to the variable domain.
      for (int i = 0; i < set_var->sorted_values.size(); ++i) {
        AddLinearConstraint({NegatedRef(set_var->var_indices[i])},
                            {Domain(set_var->sorted_values[i]).Complement()},
                            {{var_or_value.var, 1}});
      }
    }
  } else if (fz_ct.type == "set_in_reif") {
    VarOrValue var_or_value = LookupVarOrValue(fz_ct.arguments[0]);
    std::shared_ptr<const SetVariable> set_var =
        LookupSetVar(fz_ct.arguments[1]);
    const int enforcement_literal = LookupVar(fz_ct.arguments[2]);
    if (var_or_value.var == kNoVar) {
      const int index = set_var->find_value_index(var_or_value.value);
      if (index == -1) {
        AddImplication({}, NegatedRef(enforcement_literal));
      } else {
        AddLinearConstraint(
            {}, Domain(0),
            {{set_var->var_indices[index], 1}, {enforcement_literal, -1}});
      }
    } else {
      // Reduce the domain of the target variable.
      Domain set_domain = Domain::FromValues(set_var->sorted_values);
      AddLinearConstraint({enforcement_literal}, set_domain,
                          {{var_or_value.var, 1}});

      // Then propagate any remove from the set domain to the variable domain.
      // Note that the reif part is an equivalence. We do not want false
      // implies var in set (which contains the value of the var).
      for (int i = 0; i < set_var->sorted_values.size(); ++i) {
        const int64_t value = set_var->sorted_values[i];
        const int set_value_literal = set_var->var_indices[i];
        // Let's create the literal as we are going to reuse it.
        const int not_var_value_literal =
            NegatedRef(GetOrCreateEncodingLiteral(var_or_value.var, value));

        AddImplication({enforcement_literal, NegatedRef(set_value_literal)},
                       not_var_value_literal);
        AddImplication({NegatedRef(enforcement_literal), set_value_literal},
                       not_var_value_literal);
      }
    }
  } else if (fz_ct.type == "set_intersect" || fz_ct.type == "set_union" ||
             fz_ct.type == "set_symdiff" || fz_ct.type == "set_diff") {
    std::vector<int> x_literals, y_literals, r_literals;
    PutSetBooleansInCommonDomain(
        {LookupSetVar(fz_ct.arguments[0]), LookupSetVar(fz_ct.arguments[1]),
         LookupSetVar(fz_ct.arguments[2])},
        {&x_literals, &y_literals, &r_literals});
    if (fz_ct.type == "set_intersect") {
      for (int i = 0; i < x_literals.size(); ++i) {
        AddVarAndVarLiteral(x_literals[i], y_literals[i], r_literals[i]);
      }
    } else if (fz_ct.type == "set_union") {
      for (int i = 0; i < x_literals.size(); ++i) {
        AddVarOrVarLiteral(x_literals[i], y_literals[i], r_literals[i]);
      }
    } else if (fz_ct.type == "set_symdiff") {
      for (int i = 0; i < x_literals.size(); ++i) {
        AddVarEqVarLiteral(x_literals[i], y_literals[i],
                           NegatedRef(r_literals[i]));
      }
    } else if (fz_ct.type == "set_diff") {
      for (int i = 0; i < x_literals.size(); ++i) {
        AddVarLtVarLiteral(y_literals[i], x_literals[i], r_literals[i]);
      }
    }
  } else if (fz_ct.type == "set_subset" || fz_ct.type == "set_superset" ||
             fz_ct.type == "set_eq") {
    std::vector<int> x_literals, y_literals;
    PutSetBooleansInCommonDomain(
        {LookupSetVar(fz_ct.arguments[0]), LookupSetVar(fz_ct.arguments[1])},
        {&x_literals, &y_literals});

    // Implement the comparison logic.
    for (int i = 0; i < x_literals.size(); ++i) {
      const int x_lit = x_literals[i];
      const int y_lit = y_literals[i];
      if (fz_ct.type == "set_subset") {
        AddImplication({x_lit}, y_lit);
      } else if (fz_ct.type == "set_superset") {
        AddImplication({y_lit}, x_lit);
      } else if (fz_ct.type == "set_eq") {
        // We use the linear as it is easier for the presolve.
        AddLinearConstraint({}, Domain(0), {{x_lit, 1}, {y_lit, -1}});
      } else {
        LOG(FATAL) << "Unsupported " << fz_ct.type;
      }
    }
  } else if (fz_ct.type == "set_ne") {
    std::vector<int> x_literals, y_literals;
    PutSetBooleansInCommonDomain(
        {LookupSetVar(fz_ct.arguments[0]), LookupSetVar(fz_ct.arguments[1])},
        {&x_literals, &y_literals});
    AddLiteralVectorsNotEqual(x_literals, y_literals);
  } else if (fz_ct.type == "set_eq_reif" || fz_ct.type == "set_ne_reif" ||
             fz_ct.type == "set_subset_reif" ||
             fz_ct.type == "set_superset_reif") {
    std::vector<int> x_literals, y_literals;
    PutSetBooleansInCommonDomain(
        {LookupSetVar(fz_ct.arguments[0]), LookupSetVar(fz_ct.arguments[1])},
        {&x_literals, &y_literals});
    const int enforcement_literal = LookupVar(fz_ct.arguments[2]);
    const int num_values = x_literals.size();

    int e = enforcement_literal;
    if (fz_ct.type == "set_ne_reif") {
      e = NegatedRef(enforcement_literal);
    }
    BoolArgumentProto* all_true = AddEnforcedConstraint(e)->mutable_bool_and();
    BoolArgumentProto* one_false =
        AddEnforcedConstraint(NegatedRef(e))->mutable_bool_or();
    for (int i = 0; i < num_values; ++i) {
      int lit;
      if (fz_ct.type == "set_eq_reif" || fz_ct.type == "set_ne_reif") {
        lit = GetOrCreateLiteralForVarEqVar(x_literals[i], y_literals[i]);
      } else if (fz_ct.type == "set_subset_reif") {
        lit = GetOrCreateLiteralForVarLeVar(x_literals[i], y_literals[i]);
      } else {
        DCHECK_EQ(fz_ct.type, "set_superset_reif");
        lit = GetOrCreateLiteralForVarLeVar(y_literals[i], x_literals[i]);
      }
      all_true->add_literals(lit);
      one_false->add_literals(NegatedRef(lit));
    }
  } else if (fz_ct.type == "set_le" || fz_ct.type == "set_lt" ||
             fz_ct.type == "set_le_reif" || fz_ct.type == "set_lt_reif") {
    // set_le is tricky. Let's see all possible sets of size four in their
    // lexicographical order and their bit representation:
    // {}        0000
    // {1}       1000
    // {1,2}     1100
    // {1,2,3}   1110
    // {1,2,3,4} 1111
    // {1,2,4}   1101
    // {1,3}     1010
    // {1,3,4}   1011
    // {1,4}     1001
    // {2}       0100
    // {2,3}     0110
    // {2,3,4}   0111
    // {2,4}     0101
    // {3}       0010
    // {3,4}     0011
    // {4}       0001
    //
    // The example above clearly show that we cannot simply force the bit
    // representation to be in lexicographical order, which would be
    // relatively easy to do. The underlying reason is that the empty
    // (sub-)set compares before other sets. To work-around this, we define a
    // larger bit representation where between each two bits we add a new bit
    // saying whether all the bits to its right are zero or not. This way the
    // empty set is mapped from 0000 to 10101010, since every time the bits to
    // the right are zero. For the example above, we get:
    // {}        10101010
    // {1}       01101010
    // {1,2}     01011010
    // {1,2,3}   01010110
    // {1,2,3,4} 01010101
    // {1,2,4}   01010001
    // {1,3}     01000110
    // {1,3,4}   01000101
    // {1,4}     01000001
    // {2}       00011010
    // {2,3}     00010110
    // {2,3,4}   00010101
    // {2,4}     00010001
    // {3}       00000110
    // {3,4}     00000101
    // {4}       00000001
    //
    // After this trick, we can just apply the reverse lexicographic ordering
    // on the bit representation.

    std::vector<int> orig_x_literals, orig_y_literals;
    PutSetBooleansInCommonDomain(
        {LookupSetVar(fz_ct.arguments[0]), LookupSetVar(fz_ct.arguments[1])},
        {&orig_x_literals, &orig_y_literals});

    std::vector<int> x_literals, y_literals;
    for (int set_idx = 0; set_idx < 2; ++set_idx) {
      const std::vector<int>& orig_literals =
          set_idx == 0 ? orig_x_literals : orig_y_literals;
      std::vector<int>& literals = set_idx == 0 ? x_literals : y_literals;
      literals.reserve(2 * orig_literals.size());

      for (int i = 0; i < orig_literals.size(); ++i) {
        const int empty_suffix_lit = NewBoolVar();
        literals.push_back(empty_suffix_lit);
        literals.push_back(orig_literals[i]);

        ConstraintProto* empty_suffix_ct = proto.add_constraints();
        ConstraintProto* empty_suffix_ct_rev = proto.add_constraints();
        empty_suffix_ct->add_enforcement_literal(empty_suffix_lit);
        empty_suffix_ct_rev->mutable_bool_and()->add_literals(empty_suffix_lit);
        for (int j = i; j < orig_literals.size(); ++j) {
          empty_suffix_ct->mutable_bool_and()->add_literals(
              NegatedRef(orig_literals[j]));
          empty_suffix_ct_rev->add_enforcement_literal(
              NegatedRef(orig_literals[j]));
        }
      }
    }

    const bool accept_equals =
        fz_ct.type == "set_le" || fz_ct.type == "set_le_reif";
    const bool is_reif =
        fz_ct.type == "set_le_reif" || fz_ct.type == "set_lt_reif";
    const int enforcement_literal =
        is_reif ? LookupVar(fz_ct.arguments[2]) : LookupConstant(1);
    AddLexOrdering(y_literals, x_literals, enforcement_literal, accept_equals);
    if (is_reif) {
      // set_le_reif:
      //    -  l => x <= y
      //    - ~l => y < x
      // set_lt_reif:
      //    -  l => x < y
      //    - ~l => y <= x
      AddLexOrdering(x_literals, y_literals, NegatedRef(enforcement_literal),
                     !accept_equals);
    }
  } else {
    LOG(FATAL) << "Not supported " << fz_ct.DebugString();
  }
}  // NOLINT(readability/fn_size)

void CpModelProtoWithMapping::FillReifiedOrImpliedConstraint(
    const fz::Constraint& fz_ct) {
  // Start by processing the constraints that are cached.
  if (fz_ct.type == "int_eq_reif" || fz_ct.type == "bool_eq_reif") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    if (left.var == kNoVar) std::swap(left, right);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      CHECK_EQ(right.var, kNoVar);
      if (left.value == right.value) {
        AddImplication({}, e);
      } else {
        AddImplication({}, NegatedRef(e));
      }
    } else if (right.var == kNoVar) {
      AddVarEqValueLiteral(left.var, right.value, e);
    } else {
      AddVarEqVarLiteral(left.var, right.var, e);
    }
  } else if (fz_ct.type == "int_eq_imp" || fz_ct.type == "bool_eq_imp") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    if (left.var == kNoVar) std::swap(left, right);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      CHECK_EQ(right.var, kNoVar);
      if (left.value != right.value) {
        AddImplication({}, NegatedRef(e));
      }
    } else if (right.var == kNoVar) {
      const std::optional<int> lit = GetEncodingLiteral(left.var, right.value);
      if (lit.has_value()) {
        AddImplication({e}, lit.value());
        ++num_var_op_var_imp_cached;
      } else {
        AddLinearConstraint({e}, Domain(right.value), {{left.var, 1}});
      }
    } else {
      const std::optional<int> lit = GetVarEqVarLiteral(left.var, right.var);
      if (lit.has_value()) {
        AddImplication({e}, lit.value());
        ++num_var_op_var_imp_cached;
      } else {
        AddLinearConstraint({e}, Domain(0), {{left.var, 1}, {right.var, -1}});
      }
    }
  } else if (fz_ct.type == "int_ne_reif" || fz_ct.type == "bool_ne_reif") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    if (left.var == kNoVar) std::swap(left, right);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      CHECK_EQ(right.var, kNoVar);
      if (left.value != right.value) {
        AddImplication({}, e);
      } else {
        AddImplication({}, NegatedRef(e));
      }
    } else if (right.var == kNoVar) {
      AddVarEqValueLiteral(left.var, right.value, NegatedRef(e));
    } else {
      AddVarEqVarLiteral(left.var, right.var, NegatedRef(e));
    }
  } else if (fz_ct.type == "int_ne_imp" || fz_ct.type == "bool_ne_imp") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    if (left.var == kNoVar) std::swap(left, right);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      CHECK_EQ(right.var, kNoVar);
      if (left.value == right.value) {
        AddImplication({}, NegatedRef(e));
      }
    } else if (right.var == kNoVar) {
      const std::optional<int> lit = GetEncodingLiteral(left.var, right.value);
      if (lit.has_value()) {
        AddImplication({e}, NegatedRef(lit.value()));
        ++num_var_op_var_imp_cached;
      } else {
        AddLinearConstraint({e}, Domain(right.value).Complement(),
                            {{left.var, 1}});
      }
    } else {
      const std::optional<int> lit = GetVarNeVarLiteral(left.var, right.var);
      if (lit.has_value()) {
        AddImplication({e}, lit.value());
        ++num_var_op_var_imp_cached;
      } else {
        AddLinearConstraint({e}, Domain(0).Complement(),
                            {{left.var, 1}, {right.var, -1}});
      }
    }
  } else if (fz_ct.type == "int_le_reif" || fz_ct.type == "bool_le_reif") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      if (right.var == kNoVar) {
        if (left.value <= right.value) {
          AddImplication({}, e);
        } else {
          AddImplication({}, NegatedRef(e));
        }
      } else {
        AddVarGeValueLiteral(right.var, left.value, e);
      }
    } else if (right.var == kNoVar) {
      AddVarLeValueLiteral(left.var, right.value, e);
    } else {
      AddVarLeVarLiteral(left.var, right.var, e);
    }
  } else if (fz_ct.type == "int_le_imp" || fz_ct.type == "bool_le_imp") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      if (right.var == kNoVar) {
        if (left.value > right.value) {
          AddImplication({}, NegatedRef(e));
        }
      } else {
        AddLinearConstraint({e},
                            {left.value, std::numeric_limits<int64_t>::max()},
                            {{right.var, 1}});
      }
    } else if (right.var == kNoVar) {
      AddLinearConstraint({e},
                          {std::numeric_limits<int64_t>::min(), right.value},
                          {{left.var, 1}});
    } else {
      const std::optional<int> lit = GetVarLeVarLiteral(left.var, right.var);
      if (lit.has_value()) {
        AddImplication({e}, lit.value());
        ++num_var_op_var_imp_cached;
      } else {
        AddLinearConstraint({e}, {std::numeric_limits<int64_t>::min(), 0},
                            {{left.var, 1}, {right.var, -1}});
      }
    }
  } else if (fz_ct.type == "int_ge_reif" || fz_ct.type == "bool_ge_reif") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      if (right.var == kNoVar) {
        if (left.value >= right.value) {
          AddImplication({}, e);
        } else {
          AddImplication({}, NegatedRef(e));
        }
      } else {
        AddVarLeValueLiteral(right.var, left.value, e);
      }
    } else if (right.var == kNoVar) {
      AddVarGeValueLiteral(left.var, right.value, e);
    } else {
      AddVarLeVarLiteral(right.var, left.var, e);
    }
  } else if (fz_ct.type == "int_lt_reif" || fz_ct.type == "bool_lt_reif") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      if (right.var == kNoVar) {
        if (left.value < right.value) {
          AddImplication({}, e);
        } else {
          AddImplication({}, NegatedRef(e));
        }
      } else {
        AddVarGtValueLiteral(right.var, left.value, e);
      }
    } else if (right.var == kNoVar) {
      AddVarLtValueLiteral(left.var, right.value, e);
    } else {
      AddVarLtVarLiteral(left.var, right.var, e);
    }
  } else if (fz_ct.type == "int_gt_reif" || fz_ct.type == "bool_gt_reif") {
    VarOrValue left = LookupVarOrValue(fz_ct.arguments[0]);
    VarOrValue right = LookupVarOrValue(fz_ct.arguments[1]);
    const int e = LookupVar(fz_ct.arguments[2]);
    if (left.var == kNoVar) {
      if (right.var == kNoVar) {
        if (left.value < right.value) {
          AddImplication({}, e);
        } else {
          AddImplication({}, NegatedRef(e));
        }
      } else {
        AddVarLtValueLiteral(right.var, left.value, e);
      }
    } else if (right.var == kNoVar) {
      AddVarGtValueLiteral(left.var, right.value, e);
    } else {
      AddVarLtVarLiteral(right.var, left.var, e);
    }
  } else if (fz_ct.type == "array_bool_or") {
    const std::vector<int> literals = LookupVars(fz_ct.arguments[0]);
    const int e = LookupVar(fz_ct.arguments[1]);
    switch (literals.size()) {
      case 0: {
        AddImplication({}, NegatedRef(e));
        break;
      }
      case 1: {
        AddLinearConstraint({}, Domain(0), {{literals[0], 1}, {e, -1}});
        break;
      }
      case 2: {
        AddVarOrVarLiteral(literals[0], literals[1], e);
        break;
      }
      default: {
        ConstraintProto* imply = AddEnforcedConstraint(e);
        for (int i = 0; i < literals.size(); ++i) {
          imply->mutable_bool_or()->add_literals(literals[i]);
        }
        ConstraintProto* refute = AddEnforcedConstraint(NegatedRef(e));
        for (int i = 0; i < literals.size(); ++i) {
          refute->mutable_bool_and()->add_literals(NegatedRef(literals[i]));
        }
      }
    }
  } else if (fz_ct.type == "array_bool_and") {
    const std::vector<int> literals = LookupVars(fz_ct.arguments[0]);
    const int e = LookupVar(fz_ct.arguments[1]);
    switch (literals.size()) {
      case 0: {
        AddImplication({}, e);
        break;
      }
      case 1: {
        AddLinearConstraint({}, Domain(0), {{literals[0], 1}, {e, -1}});
        break;
      }
      case 2: {
        AddVarAndVarLiteral(literals[0], literals[1], e);
        break;
      }
      default: {
        ConstraintProto* imply = AddEnforcedConstraint(e);
        for (int i = 0; i < literals.size(); ++i) {
          imply->mutable_bool_and()->add_literals(literals[i]);
        }
        ConstraintProto* refute = AddEnforcedConstraint(NegatedRef(e));
        for (int i = 0; i < literals.size(); ++i) {
          refute->mutable_bool_or()->add_literals(NegatedRef(literals[i]));
        }
      }
    }
  } else {
    // Start by adding a non-reified version of the same constraint.
    ConstraintProto* ct = proto.add_constraints();
    ct->set_name(fz_ct.type);
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
    if (simplified_type == "set_in") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[2]));
      negated_type = "set_in_negated";
    } else if (simplified_type == "bool_eq" || simplified_type == "int_eq") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[2]));
      negated_type = "int_ne";
    } else if (simplified_type == "bool_ne" || simplified_type == "int_ne") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[2]));
      negated_type = "int_eq";
    } else if (simplified_type == "bool_le" || simplified_type == "int_le") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[2]));
      negated_type = "int_gt";
    } else if (simplified_type == "bool_lt" || simplified_type == "int_lt") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[2]));
      negated_type = "int_ge";
    } else if (simplified_type == "bool_ge" || simplified_type == "int_ge") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[2]));
      negated_type = "int_lt";
    } else if (simplified_type == "bool_gt" || simplified_type == "int_gt") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[2]));
      negated_type = "int_le";
    } else if (simplified_type == "int_lin_eq") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[3]));
      negated_type = "int_lin_ne";
    } else if (simplified_type == "int_lin_ne") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[3]));
      negated_type = "int_lin_eq";
    } else if (simplified_type == "int_lin_le") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[3]));
      negated_type = "int_lin_gt";
    } else if (simplified_type == "int_lin_ge") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[3]));
      negated_type = "int_lin_lt";
    } else if (simplified_type == "int_lin_lt") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[3]));
      negated_type = "int_lin_ge";
    } else if (simplified_type == "int_lin_gt") {
      ct->add_enforcement_literal(LookupVar(fz_ct.arguments[3]));
      negated_type = "int_lin_le";
    } else {
      LOG(FATAL) << "Unsupported " << simplified_type;
    }

    // One way implication. We can stop here.
    if (absl::EndsWith(fz_ct.type, "_imp")) return;

    // Add the other side of the reification because CpModelProto only support
    // half reification.
    ConstraintProto* negated_ct =
        AddEnforcedConstraint(NegatedRef(ct->enforcement_literal(0)));
    negated_ct->set_name(fz_ct.type + " (negated)");
    copy.type = negated_type;
    FillConstraint(copy, negated_ct);
  }
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
    } else if (annotation.IsFunctionCallWithIdentifier("set_search")) {
      const std::vector<fz::Annotation>& args = annotation.annotations;
      std::vector<fz::Variable*> vars;
      args[0].AppendAllVariables(&vars);

      DecisionStrategyProto* strategy = proto.add_search_strategy();
      for (fz::Variable* v : vars) {
        std::shared_ptr<SetVariable> set_var = set_variables.at(v);
        strategy->mutable_variables()->Add(set_var->var_indices.begin(),
                                           set_var->var_indices.end());
      }

      const fz::Annotation& choose = args[1];
      if (choose.id == "input_order") {
        strategy->set_variable_selection_strategy(
            DecisionStrategyProto::CHOOSE_FIRST);
      } else {
        SOLVER_LOG(logger, "Unsupported set variable selection strategy '",
                   choose.id, "', falling back to 'input_order'");
        strategy->set_variable_selection_strategy(
            DecisionStrategyProto::CHOOSE_FIRST);
      }

      const fz::Annotation& select = args[2];
      if (select.id == "indomain_min" || select.id == "indomain") {
        strategy->set_domain_reduction_strategy(
            DecisionStrategyProto::SELECT_MIN_VALUE);
      } else if (select.id == "indomain_max") {
        strategy->set_domain_reduction_strategy(
            DecisionStrategyProto::SELECT_MAX_VALUE);
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
    const std::function<std::vector<int64_t>(fz::Variable*)>& set_evaluator,
    double objective_value) {
  if (output.variable != nullptr && !output.variable->domain.is_float &&
      !output.variable->domain.is_a_set) {
    const int64_t value = value_func(output.variable);
    if (output.display_as_boolean) {
      return absl::StrCat(output.name, " = ", value == 1 ? "true" : "false",
                          ";");
    } else {
      return absl::StrCat(output.name, " = ", value, ";");
    }
  } else if (output.variable != nullptr && output.variable->domain.is_a_set) {
    const std::vector<int64_t> values = set_evaluator(output.variable);
    return absl::StrCat(output.name, " = {", absl::StrJoin(values, ","), "};");
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
      if (output.flat_variables[i]->domain.is_a_set) {
        const std::vector<int64_t> values =
            set_evaluator(output.flat_variables[i]);
        absl::StrAppend(&result, "{", absl::StrJoin(values, ","), "}");
      } else {
        const int64_t value = value_func(output.flat_variables[i]);
        if (output.display_as_boolean) {
          result.append(value ? "true" : "false");
        } else {
          absl::StrAppend(&result, value);
        }
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
    const std::function<std::vector<int64_t>(fz::Variable*)>& set_evaluator,
    double objective_value) {
  std::string solution_string;
  for (const auto& output_spec : model.output()) {
    solution_string.append(SolutionString(output_spec, value_func,
                                          set_evaluator, objective_value));
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
  const bool enumerate_all_solutions_of_a_sat_problem =
      p.search_all_solutions && fz_model.objective() == nullptr;
  CpModelProtoWithMapping m(enumerate_all_solutions_of_a_sat_problem);
  m.proto.set_name(fz_model.name());

  // The translation is easy, we create one variable
  // per flatzinc variable, plus eventually a bunch
  // of constant variables that will be created
  // lazily.
  for (fz::Variable* fz_var : fz_model.variables()) {
    if (!fz_var->active) continue;
    CHECK(!fz_var->domain.is_float) << "CP-SAT does not support float "
                                       "variables";

    if (fz_var->domain.is_a_set) {
      std::shared_ptr<SetVariable> set_var = std::make_shared<SetVariable>();

      // Fill values.
      if (fz_var->domain.is_interval) {
        set_var->sorted_values.reserve(fz_var->domain.values[1] -
                                       fz_var->domain.values[0] + 1);
        for (int64_t value = fz_var->domain.values[0];
             value <= fz_var->domain.values[1]; ++value) {
          set_var->sorted_values.push_back(value);
        }
      } else {
        set_var->sorted_values.reserve(fz_var->domain.values.size());
        for (int64_t value : fz_var->domain.values) {
          set_var->sorted_values.push_back(value);
        }
      }

      // Add Boolean variables.
      for (int i = 0; i < set_var->sorted_values.size(); ++i) {
        if (fz_var->domain.is_fixed_set) {
          set_var->var_indices.push_back(m.LookupConstant(1));
        } else {
          set_var->var_indices.push_back(m.NewBoolVar());
        }
      }
      m.set_variables[fz_var] = set_var;
    } else {
      m.fz_var_to_index[fz_var] = m.proto.variables_size();
      IntegerVariableProto* var = m.proto.add_variables();
      var->set_name(fz_var->name);
      if (fz_var->domain.is_interval) {
        if (fz_var->domain.values.empty()) {
          // The CP-SAT solver checks that
          // constraints cannot overflow during
          // their propagation. Because of that, we
          // trim undefined variable domains (i.e.
          // int in minizinc) to something hopefully
          // large enough.
          LOG_FIRST_N(WARNING, 1) << "Using flag --fz_int_max for "
                                     "unbounded integer variables.";
          LOG_FIRST_N(WARNING, 1)
              << "    actual domain is [" << -absl::GetFlag(FLAGS_fz_int_max)
              << ".." << absl::GetFlag(FLAGS_fz_int_max) << "]";
          FillDomainInProto(-absl::GetFlag(FLAGS_fz_int_max),
                            absl::GetFlag(FLAGS_fz_int_max), var);
        } else {
          FillDomainInProto(fz_var->domain.values[0], fz_var->domain.values[1],
                            var);
        }
      } else {
        FillDomainInProto(Domain::FromValues(fz_var->domain.values), var);
      }
    }
  }

  // Translate the constraints.

  // First pass: extract useful information.
  for (fz::Constraint* fz_ct : fz_model.constraints()) {
    m.FirstPass(*fz_ct);
  }

  // Second pass: translate the constraints.
  for (fz::Constraint* fz_ct : fz_model.constraints()) {
    if (fz_ct == nullptr || !fz_ct->active) continue;
    if (m.ConstraintContainsSetVariables(*fz_ct)) {
      m.ExtractSetConstraint(*fz_ct);
    } else {
      if (absl::EndsWith(fz_ct->type, "_reif") ||
          absl::EndsWith(fz_ct->type, "_imp") ||
          fz_ct->type == "array_bool_or" || fz_ct->type == "array_bool_and") {
        m.FillReifiedOrImpliedConstraint(*fz_ct);
      } else {
        ConstraintProto* ct = m.proto.add_constraints();
        ct->set_name(fz_ct->type);
        m.FillConstraint(*fz_ct, ct);
      }
    }
  }

  // Third pass: Implement the encoding constraints.
  m.AddAllEncodingConstraints();

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

  // Extra statistics.
  SOLVER_LOG(logger,
             "  - #var_op_value_reif cached: ", m.num_var_op_value_reif_cached);
  SOLVER_LOG(logger,
             "  - #var_op_var_reif cached: ", m.num_var_op_var_reif_cached);
  SOLVER_LOG(logger,
             "  - #var_op_var_imp cached: ", m.num_var_op_var_reif_cached);
  SOLVER_LOG(logger, "  - #full domain encodings: ", m.num_full_encodings);
  if (absl::GetFlag(FLAGS_fz_use_light_encoding)) {
    SOLVER_LOG(logger,
               "  - #partial domain encodings: ", m.num_partial_encodings);
    SOLVER_LOG(logger,
               "  - #light constraint encodings: ", m.num_light_encodings);
  }

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
    if (!p.search_all_solutions &&
        (absl::GetFlag(FLAGS_force_interleave_search) || p.ortools_mode)) {
      m.parameters.set_interleave_search(true);
      m.parameters.set_use_rins_lns(false);
      m.parameters.add_subsolvers("default_lp");
      m.parameters.add_subsolvers("max_lp");
      m.parameters.add_subsolvers("quick_restart");
      m.parameters.add_subsolvers("core_or_no_lp");  // no_lp if no objective.
      m.parameters.set_num_violation_ls(1);          // Off if no objective.
      if (!m.proto.search_strategy().empty()) {
        m.parameters.add_subsolvers("fixed");
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
          [&m, &r](fz::Variable* v) {
            std::vector<int64_t> values;
            const std::shared_ptr<SetVariable> set_var = m.set_variables.at(v);
            for (int i = 0; i < set_var->sorted_values.size(); ++i) {
              if (r.solution(set_var->var_indices[i]) == 1) {
                values.push_back(set_var->sorted_values[i]);
              }
            }
            return values;
          },
          r.objective_value());
      SOLVER_LOG(solution_logger, solution_string);
      if (p.check_all_solutions) {
        CHECK(CheckSolution(
            fz_model,
            [&r, &m](fz::Variable* v) {
              return r.solution(m.fz_var_to_index.at(v));
            },
            [&r, &m](fz::Variable* v) {
              std::vector<int64_t> values;
              const std::shared_ptr<SetVariable> set_var =
                  m.set_variables.at(v);
              for (int i = 0; i < set_var->sorted_values.size(); ++i) {
                if (r.solution(set_var->var_indices[i]) == 1) {
                  values.push_back(set_var->sorted_values[i]);
                }
              }
              return values;
            },
            solution_logger));
      }
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
        [&response, &m](fz::Variable* v) {
          std::vector<int64_t> values;
          const std::shared_ptr<SetVariable> set_var = m.set_variables.at(v);
          for (int i = 0; i < set_var->sorted_values.size(); ++i) {
            if (response.solution(set_var->var_indices[i]) == 1) {
              values.push_back(set_var->sorted_values[i]);
            }
          }
          return values;
        },
        logger));
  }

  // Output the solution in the flatzinc official format.
  if (p.ortools_mode) {
    if (response.status() == CpSolverStatus::FEASIBLE ||
        response.status() == CpSolverStatus::OPTIMAL) {
      // Display the solution if it is not already displayed.
      if (!p.display_all_solutions && !p.search_all_solutions) {
        // Already printed otherwise.
        const std::string solution_string = SolutionString(
            fz_model,
            [&response, &m](fz::Variable* v) {
              return response.solution(m.fz_var_to_index.at(v));
            },
            [&response, &m](fz::Variable* v) {
              std::vector<int64_t> values;
              const std::shared_ptr<SetVariable> set_var =
                  m.set_variables.at(v);
              for (int i = 0; i < set_var->sorted_values.size(); ++i) {
                if (response.solution(set_var->var_indices[i]) == 1) {
                  values.push_back(set_var->sorted_values[i]);
                }
              }
              return values;
            },
            response.objective_value());
        SOLVER_LOG(solution_logger, solution_string);
        SOLVER_LOG(solution_logger, "----------");
      }
      const bool should_print_optimal =
          response.status() == CpSolverStatus::OPTIMAL &&
          (fz_model.objective() != nullptr || p.search_all_solutions);

      if (should_print_optimal) {
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
