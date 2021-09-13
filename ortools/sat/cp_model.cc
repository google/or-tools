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

#include "ortools/sat/cp_model.h"

#include <cstdint>
#include <limits>

#include "absl/strings/str_format.h"
#include "ortools/base/map_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

BoolVar::BoolVar() : cp_model_(nullptr), index_(0) {}

BoolVar::BoolVar(int index, CpModelProto* cp_model)
    : cp_model_(cp_model), index_(index) {}

BoolVar BoolVar::WithName(const std::string& name) {
  cp_model_->mutable_variables(index_)->set_name(name);
  return *this;
}

std::string BoolVar::DebugString() const {
  if (index_ < 0) {
    return absl::StrFormat("Not(%s)", Not().DebugString());
  } else {
    std::string output;
    const IntegerVariableProto& var_proto = cp_model_->variables(index_);
    // Special case for constant variables without names.
    if (var_proto.name().empty() && var_proto.domain_size() == 2 &&
        var_proto.domain(0) == var_proto.domain(1)) {
      output.append(var_proto.domain(0) == 0 ? "false" : "true");
    } else {
      if (var_proto.name().empty()) {
        absl::StrAppendFormat(&output, "BoolVar%i(", index_);
      } else {
        absl::StrAppendFormat(&output, "%s(", var_proto.name());
      }
      if (var_proto.domain(0) == var_proto.domain(1)) {
        output.append(var_proto.domain(0) == 0 ? "false)" : "true)");
      } else {
        absl::StrAppend(&output, var_proto.domain(0), ", ", var_proto.domain(1),
                        ")");
      }
    }
    return output;
  }
}

BoolVar Not(BoolVar x) { return x.Not(); }

std::ostream& operator<<(std::ostream& os, const BoolVar& var) {
  os << var.DebugString();
  return os;
}

IntVar::IntVar() : cp_model_(nullptr), index_(0) {}

IntVar::IntVar(int index, CpModelProto* cp_model)
    : cp_model_(cp_model), index_(index) {
  CHECK(RefIsPositive(index));
}

IntVar IntVar::WithName(const std::string& name) {
  cp_model_->mutable_variables(index_)->set_name(name);
  return *this;
}

IntVar::IntVar(const BoolVar& var) {
  cp_model_ = var.cp_model_;
  index_ = var.index_;
}

BoolVar IntVar::ToBoolVar() const {
  CHECK_EQ(2, Proto().domain_size());
  CHECK_GE(Proto().domain(0), 0);
  CHECK_LE(Proto().domain(1), 1);
  BoolVar var;
  var.cp_model_ = cp_model_;
  var.index_ = index();
  return var;
}

LinearExpr IntVar::AddConstant(int64_t value) const {
  return LinearExpr(*this).AddConstant(value);
}

std::string IntVar::DebugString() const {
  if (index_ < 0) {
    return absl::StrFormat("Not(%s)",
                           IntVar(NegatedRef(index_), cp_model_).DebugString());
  }
  const IntegerVariableProto& var_proto = cp_model_->variables(index_);
  // Special case for constant variables without names.
  if (var_proto.name().empty() && var_proto.domain_size() == 2 &&
      var_proto.domain(0) == var_proto.domain(1)) {
    return absl::StrCat(var_proto.domain(0));
  } else {
    std::string output;
    if (var_proto.name().empty()) {
      absl::StrAppend(&output, "IntVar", index_, "(");
    } else {
      absl::StrAppend(&output, var_proto.name(), "(");
    }
    if (var_proto.domain_size() == 2 &&
        var_proto.domain(0) == var_proto.domain(1)) {
      absl::StrAppend(&output, var_proto.domain(0), ")");
    } else {
      // TODO(user): Use domain pretty print function.
      absl::StrAppend(&output, var_proto.domain(0), ", ", var_proto.domain(1),
                      ")");
    }
    return output;
  }
}

std::ostream& operator<<(std::ostream& os, const IntVar& var) {
  os << var.DebugString();
  return os;
}

LinearExpr::LinearExpr() {}

LinearExpr::LinearExpr(BoolVar var) { AddVar(var); }

LinearExpr::LinearExpr(IntVar var) { AddVar(var); }

LinearExpr::LinearExpr(int64_t constant) { constant_ = constant; }

LinearExpr LinearExpr::Sum(absl::Span<const IntVar> vars) {
  LinearExpr result;
  for (const IntVar& var : vars) {
    result.AddVar(var);
  }
  return result;
}

LinearExpr LinearExpr::ScalProd(absl::Span<const IntVar> vars,
                                absl::Span<const int64_t> coeffs) {
  CHECK_EQ(vars.size(), coeffs.size());
  LinearExpr result;
  for (int i = 0; i < vars.size(); ++i) {
    result.AddTerm(vars[i], coeffs[i]);
  }
  return result;
}

LinearExpr LinearExpr::Term(IntVar var, int64_t coefficient) {
  LinearExpr result;
  result.AddTerm(var, coefficient);
  return result;
}

LinearExpr LinearExpr::BooleanSum(absl::Span<const BoolVar> vars) {
  LinearExpr result;
  for (const BoolVar& var : vars) {
    result.AddVar(var);
  }
  return result;
}

LinearExpr LinearExpr::BooleanScalProd(absl::Span<const BoolVar> vars,
                                       absl::Span<const int64_t> coeffs) {
  CHECK_EQ(vars.size(), coeffs.size());
  LinearExpr result;
  for (int i = 0; i < vars.size(); ++i) {
    result.AddTerm(vars[i], coeffs[i]);
  }
  return result;
}

LinearExpr& LinearExpr::AddConstant(int64_t value) {
  constant_ += value;
  return *this;
}

LinearExpr& LinearExpr::AddVar(IntVar var) {
  AddTerm(var, 1);
  return *this;
}

LinearExpr& LinearExpr::AddTerm(IntVar var, int64_t coeff) {
  const int index = var.index_;
  if (RefIsPositive(index)) {
    variables_.push_back(var);
    coefficients_.push_back(coeff);
  } else {
    variables_.push_back(IntVar(PositiveRef(var.index_), var.cp_model_));
    coefficients_.push_back(-coeff);
    constant_ += coeff;
  }
  return *this;
}

LinearExpr& LinearExpr::AddExpression(const LinearExpr& expr) {
  constant_ += expr.constant_;
  variables_.insert(variables_.end(), expr.variables_.begin(),
                    expr.variables_.end());
  coefficients_.insert(coefficients_.end(), expr.coefficients_.begin(),
                       expr.coefficients_.end());
  return *this;
}

IntVar LinearExpr::Var() const {
  CHECK_EQ(constant_, 0);
  CHECK_EQ(1, variables_.size());
  CHECK_EQ(1, coefficients_[0]);
  return variables_.front();
}

int64_t LinearExpr::Value() const {
  CHECK(variables_.empty());
  return constant_;
}

std::string LinearExpr::DebugString() const {
  std::string result;
  for (int i = 0; i < variables_.size(); ++i) {
    const int64_t coeff = coefficients_[i];
    if (i == 0) {
      if (coeff == 1) {
        absl::StrAppend(&result, variables_[i].DebugString());
      } else if (coeff == -1) {
        absl::StrAppend(&result, "-", variables_[i].DebugString());
      } else if (coeff != 0) {
        absl::StrAppend(&result, coeff, " * ", variables_[i].DebugString());
      }
    } else if (coeff == 1) {
      absl::StrAppend(&result, " + ", variables_[i].DebugString());
    } else if (coeff > 0) {
      absl::StrAppend(&result, " + ", coeff, " * ",
                      variables_[i].DebugString());
    } else if (coeff == -1) {
      absl::StrAppend(&result, " - ", variables_[i].DebugString());
    } else if (coeff < 0) {
      absl::StrAppend(&result, " - ", -coeff, " * ",
                      variables_[i].DebugString());
    }
  }

  if (constant_ != 0) {
    if (variables_.empty()) {
      return absl::StrCat(constant_);
    } else if (constant_ > 0) {
      absl::StrAppend(&result, " + ", constant_);
    } else {
      absl::StrAppend(&result, " - ", -constant_);
    }
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, const LinearExpr& e) {
  os << e.DebugString();
  return os;
}

Constraint::Constraint(ConstraintProto* proto) : proto_(proto) {}

Constraint Constraint::WithName(const std::string& name) {
  proto_->set_name(name);
  return *this;
}

const std::string& Constraint::Name() const { return proto_->name(); }

Constraint Constraint::OnlyEnforceIf(absl::Span<const BoolVar> literals) {
  for (const BoolVar& var : literals) {
    proto_->add_enforcement_literal(var.index_);
  }
  return *this;
}

Constraint Constraint::OnlyEnforceIf(BoolVar literal) {
  proto_->add_enforcement_literal(literal.index_);
  return *this;
}

void CircuitConstraint::AddArc(int tail, int head, BoolVar literal) {
  proto_->mutable_circuit()->add_tails(tail);
  proto_->mutable_circuit()->add_heads(head);
  proto_->mutable_circuit()->add_literals(literal.index_);
}

void MultipleCircuitConstraint::AddArc(int tail, int head, BoolVar literal) {
  proto_->mutable_routes()->add_tails(tail);
  proto_->mutable_routes()->add_heads(head);
  proto_->mutable_routes()->add_literals(literal.index_);
}

void TableConstraint::AddTuple(absl::Span<const int64_t> tuple) {
  CHECK_EQ(tuple.size(), proto_->table().vars_size());
  for (const int64_t t : tuple) {
    proto_->mutable_table()->add_values(t);
  }
}

ReservoirConstraint::ReservoirConstraint(ConstraintProto* proto,
                                         CpModelBuilder* builder)
    : Constraint(proto), builder_(builder) {}

void ReservoirConstraint::AddEvent(IntVar time, int64_t demand) {
  proto_->mutable_reservoir()->add_times(
      builder_->GetOrCreateIntegerIndex(time.index_));
  proto_->mutable_reservoir()->add_demands(demand);
  proto_->mutable_reservoir()->add_actives(builder_->IndexFromConstant(1));
}

void ReservoirConstraint::AddOptionalEvent(IntVar time, int64_t demand,
                                           BoolVar is_active) {
  proto_->mutable_reservoir()->add_times(
      builder_->GetOrCreateIntegerIndex(time.index_));
  proto_->mutable_reservoir()->add_demands(demand);
  proto_->mutable_reservoir()->add_actives(is_active.index_);
}

void AutomatonConstraint::AddTransition(int tail, int head,
                                        int64_t transition_label) {
  proto_->mutable_automaton()->add_transition_tail(tail);
  proto_->mutable_automaton()->add_transition_head(head);
  proto_->mutable_automaton()->add_transition_label(transition_label);
}

void NoOverlap2DConstraint::AddRectangle(IntervalVar x_coordinate,
                                         IntervalVar y_coordinate) {
  proto_->mutable_no_overlap_2d()->add_x_intervals(x_coordinate.index_);
  proto_->mutable_no_overlap_2d()->add_y_intervals(y_coordinate.index_);
}

CumulativeConstraint::CumulativeConstraint(ConstraintProto* proto,
                                           CpModelBuilder* builder)
    : Constraint(proto), builder_(builder) {}

void CumulativeConstraint::AddDemand(IntervalVar interval, IntVar demand) {
  proto_->mutable_cumulative()->add_intervals(interval.index_);
  proto_->mutable_cumulative()->add_demands(
      builder_->GetOrCreateIntegerIndex(demand.index_));
}

IntervalVar::IntervalVar() : cp_model_(nullptr), index_() {}

IntervalVar::IntervalVar(int index, CpModelProto* cp_model)
    : cp_model_(cp_model), index_(index) {}

IntervalVar IntervalVar::WithName(const std::string& name) {
  cp_model_->mutable_constraints(index_)->set_name(name);
  return *this;
}

LinearExpr IntervalVar::StartExpr() const {
  return CpModelBuilder::LinearExprFromProto(Proto().start_view(), cp_model_);
}

LinearExpr IntervalVar::SizeExpr() const {
  return CpModelBuilder::LinearExprFromProto(Proto().size_view(), cp_model_);
}

LinearExpr IntervalVar::EndExpr() const {
  return CpModelBuilder::LinearExprFromProto(Proto().end_view(), cp_model_);
}

BoolVar IntervalVar::PresenceBoolVar() const {
  return BoolVar(cp_model_->constraints(index_).enforcement_literal(0),
                 cp_model_);
}

std::string IntervalVar::Name() const {
  return cp_model_->constraints(index_).name();
}

std::string IntervalVar::DebugString() const {
  CHECK_GE(index_, 0);
  const ConstraintProto& ct_proto = cp_model_->constraints(index_);
  std::string output;
  if (ct_proto.name().empty()) {
    absl::StrAppend(&output, "IntervalVar", index_, "(");
  } else {
    absl::StrAppend(&output, ct_proto.name(), "(");
  }
  absl::StrAppend(&output, StartExpr().DebugString(), ", ",
                  SizeExpr().DebugString(), ", ", EndExpr().DebugString(), ", ",
                  PresenceBoolVar().DebugString(), ")");
  return output;
}

std::ostream& operator<<(std::ostream& os, const IntervalVar& var) {
  os << var.DebugString();
  return os;
}

int CpModelBuilder::IndexFromConstant(int64_t value) {
  if (!gtl::ContainsKey(constant_to_index_map_, value)) {
    const int index = cp_model_.variables_size();
    IntegerVariableProto* const var_proto = cp_model_.add_variables();
    var_proto->add_domain(value);
    var_proto->add_domain(value);
    constant_to_index_map_[value] = index;
  }
  return constant_to_index_map_[value];
}

int CpModelBuilder::GetOrCreateIntegerIndex(int index) {
  if (index >= 0) {
    return index;
  }
  if (!gtl::ContainsKey(bool_to_integer_index_map_, index)) {
    const int var = PositiveRef(index);
    const IntegerVariableProto& old_var = cp_model_.variables(var);
    const int new_index = cp_model_.variables_size();
    IntegerVariableProto* const new_var = cp_model_.add_variables();
    new_var->add_domain(0);
    new_var->add_domain(1);
    if (!old_var.name().empty()) {
      new_var->set_name(absl::StrCat("Not(", old_var.name(), ")"));
    }
    AddEquality(IntVar(new_index, &cp_model_), BoolVar(index, &cp_model_));
    bool_to_integer_index_map_[index] = new_index;
    return new_index;
  }
  return bool_to_integer_index_map_[index];
}

IntVar CpModelBuilder::NewIntVar(const Domain& domain) {
  const int index = cp_model_.variables_size();
  IntegerVariableProto* const var_proto = cp_model_.add_variables();
  for (const auto& interval : domain) {
    var_proto->add_domain(interval.start);
    var_proto->add_domain(interval.end);
  }
  return IntVar(index, &cp_model_);
}

BoolVar CpModelBuilder::NewBoolVar() {
  const int index = cp_model_.variables_size();
  IntegerVariableProto* const var_proto = cp_model_.add_variables();
  var_proto->add_domain(0);
  var_proto->add_domain(1);
  return BoolVar(index, &cp_model_);
}

IntVar CpModelBuilder::NewConstant(int64_t value) {
  return IntVar(IndexFromConstant(value), &cp_model_);
}

BoolVar CpModelBuilder::TrueVar() {
  return BoolVar(IndexFromConstant(1), &cp_model_);
}

BoolVar CpModelBuilder::FalseVar() {
  return BoolVar(IndexFromConstant(0), &cp_model_);
}

IntervalVar CpModelBuilder::NewIntervalVar(const LinearExpr& start,
                                           const LinearExpr& size,
                                           const LinearExpr& end) {
  return NewOptionalIntervalVar(start, size, end, TrueVar());
}

IntervalVar CpModelBuilder::NewFixedSizeIntervalVar(const LinearExpr& start,
                                                    int64_t size) {
  return NewOptionalFixedSizeIntervalVar(start, size, TrueVar());
}

IntervalVar CpModelBuilder::NewOptionalIntervalVar(const LinearExpr& start,
                                                   const LinearExpr& size,
                                                   const LinearExpr& end,
                                                   BoolVar presence) {
  AddEquality(LinearExpr(start).AddExpression(size), end)
      .OnlyEnforceIf(presence);

  const int index = cp_model_.constraints_size();
  ConstraintProto* const ct = cp_model_.add_constraints();
  ct->add_enforcement_literal(presence.index_);
  IntervalConstraintProto* const interval = ct->mutable_interval();
  LinearExprToProto(start, interval->mutable_start_view());
  LinearExprToProto(size, interval->mutable_size_view());
  LinearExprToProto(end, interval->mutable_end_view());
  return IntervalVar(index, &cp_model_);
}

IntervalVar CpModelBuilder::NewOptionalFixedSizeIntervalVar(
    const LinearExpr& start, int64_t size, BoolVar presence) {
  const int index = cp_model_.constraints_size();
  ConstraintProto* const ct = cp_model_.add_constraints();
  ct->add_enforcement_literal(presence.index_);
  IntervalConstraintProto* const interval = ct->mutable_interval();
  LinearExprToProto(start, interval->mutable_start_view());
  interval->mutable_size_view()->set_offset(size);
  LinearExprToProto(start, interval->mutable_end_view());
  interval->mutable_end_view()->set_offset(interval->end_view().offset() +
                                           size);
  return IntervalVar(index, &cp_model_);
}

Constraint CpModelBuilder::AddBoolOr(absl::Span<const BoolVar> literals) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const BoolVar& lit : literals) {
    proto->mutable_bool_or()->add_literals(lit.index_);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddBoolAnd(absl::Span<const BoolVar> literals) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const BoolVar& lit : literals) {
    proto->mutable_bool_and()->add_literals(lit.index_);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddBoolXor(absl::Span<const BoolVar> literals) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const BoolVar& lit : literals) {
    proto->mutable_bool_xor()->add_literals(lit.index_);
  }
  return Constraint(proto);
}

void CpModelBuilder::FillLinearTerms(const LinearExpr& left,
                                     const LinearExpr& right,
                                     LinearConstraintProto* proto) {
  for (const IntVar& x : left.variables()) {
    proto->add_vars(x.index_);
  }
  for (const int64_t coeff : left.coefficients()) {
    proto->add_coeffs(coeff);
  }
  for (const IntVar& x : right.variables()) {
    proto->add_vars(x.index_);
  }
  for (const int64_t coeff : right.coefficients()) {
    proto->add_coeffs(-coeff);
  }
}

Constraint CpModelBuilder::AddEquality(const LinearExpr& left,
                                       const LinearExpr& right) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  FillLinearTerms(left, right, proto->mutable_linear());
  const int64_t rhs = right.constant() - left.constant();
  proto->mutable_linear()->add_domain(rhs);
  proto->mutable_linear()->add_domain(rhs);
  return Constraint(proto);
}

Constraint CpModelBuilder::AddGreaterOrEqual(const LinearExpr& left,
                                             const LinearExpr& right) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  FillLinearTerms(left, right, proto->mutable_linear());
  const int64_t rhs = right.constant() - left.constant();
  proto->mutable_linear()->add_domain(rhs);
  proto->mutable_linear()->add_domain(std::numeric_limits<int64_t>::max());
  return Constraint(proto);
}

Constraint CpModelBuilder::AddLessOrEqual(const LinearExpr& left,
                                          const LinearExpr& right) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  FillLinearTerms(left, right, proto->mutable_linear());
  const int64_t rhs = right.constant() - left.constant();
  proto->mutable_linear()->add_domain(std::numeric_limits<int64_t>::min());
  proto->mutable_linear()->add_domain(rhs);
  return Constraint(proto);
}

Constraint CpModelBuilder::AddGreaterThan(const LinearExpr& left,
                                          const LinearExpr& right) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  FillLinearTerms(left, right, proto->mutable_linear());
  const int64_t rhs = right.constant() - left.constant();
  proto->mutable_linear()->add_domain(rhs + 1);
  proto->mutable_linear()->add_domain(std::numeric_limits<int64_t>::max());
  return Constraint(proto);
}

Constraint CpModelBuilder::AddLessThan(const LinearExpr& left,
                                       const LinearExpr& right) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  FillLinearTerms(left, right, proto->mutable_linear());
  const int64_t rhs = right.constant() - left.constant();
  proto->mutable_linear()->add_domain(std::numeric_limits<int64_t>::min());
  proto->mutable_linear()->add_domain(rhs - 1);
  return Constraint(proto);
}

Constraint CpModelBuilder::AddLinearConstraint(const LinearExpr& expr,
                                               const Domain& domain) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntVar& x : expr.variables()) {
    proto->mutable_linear()->add_vars(x.index_);
  }
  for (const int64_t coeff : expr.coefficients()) {
    proto->mutable_linear()->add_coeffs(coeff);
  }
  const int64_t cst = expr.constant();
  for (const auto& i : domain) {
    proto->mutable_linear()->add_domain(i.start - cst);
    proto->mutable_linear()->add_domain(i.end - cst);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddNotEqual(const LinearExpr& left,
                                       const LinearExpr& right) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  FillLinearTerms(left, right, proto->mutable_linear());
  const int64_t rhs = right.constant() - left.constant();
  proto->mutable_linear()->add_domain(std::numeric_limits<int64_t>::min());
  proto->mutable_linear()->add_domain(rhs - 1);
  proto->mutable_linear()->add_domain(rhs + 1);
  proto->mutable_linear()->add_domain(std::numeric_limits<int64_t>::max());
  return Constraint(proto);
}

Constraint CpModelBuilder::AddAllDifferent(absl::Span<const IntVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntVar& var : vars) {
    proto->mutable_all_diff()->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddVariableElement(
    IntVar index, absl::Span<const IntVar> variables, IntVar target) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_element()->set_index(GetOrCreateIntegerIndex(index.index_));
  proto->mutable_element()->set_target(GetOrCreateIntegerIndex(target.index_));
  for (const IntVar& var : variables) {
    proto->mutable_element()->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddElement(IntVar index,
                                      absl::Span<const int64_t> values,
                                      IntVar target) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_element()->set_index(GetOrCreateIntegerIndex(index.index_));
  proto->mutable_element()->set_target(GetOrCreateIntegerIndex(target.index_));
  for (int64_t value : values) {
    proto->mutable_element()->add_vars(IndexFromConstant(value));
  }
  return Constraint(proto);
}

CircuitConstraint CpModelBuilder::AddCircuitConstraint() {
  return CircuitConstraint(cp_model_.add_constraints());
}

MultipleCircuitConstraint CpModelBuilder::AddMultipleCircuitConstraint() {
  return MultipleCircuitConstraint(cp_model_.add_constraints());
}

TableConstraint CpModelBuilder::AddAllowedAssignments(
    absl::Span<const IntVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntVar& var : vars) {
    proto->mutable_table()->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  return TableConstraint(proto);
}

TableConstraint CpModelBuilder::AddForbiddenAssignments(
    absl::Span<const IntVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntVar& var : vars) {
    proto->mutable_table()->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  proto->mutable_table()->set_negated(true);
  return TableConstraint(proto);
}

Constraint CpModelBuilder::AddInverseConstraint(
    absl::Span<const IntVar> variables,
    absl::Span<const IntVar> inverse_variables) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntVar& var : variables) {
    proto->mutable_inverse()->add_f_direct(GetOrCreateIntegerIndex(var.index_));
  }
  for (const IntVar& var : inverse_variables) {
    proto->mutable_inverse()->add_f_inverse(
        GetOrCreateIntegerIndex(var.index_));
  }
  return Constraint(proto);
}

ReservoirConstraint CpModelBuilder::AddReservoirConstraint(int64_t min_level,
                                                           int64_t max_level) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_reservoir()->set_min_level(min_level);
  proto->mutable_reservoir()->set_max_level(max_level);
  return ReservoirConstraint(proto, this);
}

AutomatonConstraint CpModelBuilder::AddAutomaton(
    absl::Span<const IntVar> transition_variables, int starting_state,
    absl::Span<const int> final_states) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntVar& var : transition_variables) {
    proto->mutable_automaton()->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  proto->mutable_automaton()->set_starting_state(starting_state);
  for (const int final_state : final_states) {
    proto->mutable_automaton()->add_final_states(final_state);
  }
  return AutomatonConstraint(proto);
}

Constraint CpModelBuilder::AddMinEquality(IntVar target,
                                          absl::Span<const IntVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_int_min()->set_target(GetOrCreateIntegerIndex(target.index_));
  for (const IntVar& var : vars) {
    proto->mutable_int_min()->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  return Constraint(proto);
}

void CpModelBuilder::LinearExprToProto(const LinearExpr& expr,
                                       LinearExpressionProto* expr_proto) {
  for (const IntVar var : expr.variables()) {
    expr_proto->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  for (const int64_t coeff : expr.coefficients()) {
    expr_proto->add_coeffs(coeff);
  }
  expr_proto->set_offset(expr.constant());
}

LinearExpr CpModelBuilder::LinearExprFromProto(
    const LinearExpressionProto& expr_proto, CpModelProto* model_proto) {
  LinearExpr result(expr_proto.offset());
  for (int i = 0; i < expr_proto.vars_size(); ++i) {
    result.AddTerm(IntVar(expr_proto.vars(i), model_proto),
                   expr_proto.coeffs(i));
  }
  return result;
}

Constraint CpModelBuilder::AddLinMinEquality(
    const LinearExpr& target, absl::Span<const LinearExpr> exprs) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  LinearExprToProto(target, proto->mutable_lin_min()->mutable_target());
  for (const LinearExpr& expr : exprs) {
    LinearExpressionProto* expr_proto = proto->mutable_lin_min()->add_exprs();
    LinearExprToProto(expr, expr_proto);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddMaxEquality(IntVar target,
                                          absl::Span<const IntVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_int_max()->set_target(GetOrCreateIntegerIndex(target.index_));
  for (const IntVar& var : vars) {
    proto->mutable_int_max()->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddLinMaxEquality(
    const LinearExpr& target, absl::Span<const LinearExpr> exprs) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  LinearExprToProto(target, proto->mutable_lin_max()->mutable_target());
  for (const LinearExpr& expr : exprs) {
    LinearExpressionProto* expr_proto = proto->mutable_lin_max()->add_exprs();
    LinearExprToProto(expr, expr_proto);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddDivisionEquality(IntVar target, IntVar numerator,
                                               IntVar denominator) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_int_div()->set_target(GetOrCreateIntegerIndex(target.index_));
  proto->mutable_int_div()->add_vars(GetOrCreateIntegerIndex(numerator.index_));
  proto->mutable_int_div()->add_vars(
      GetOrCreateIntegerIndex(denominator.index_));
  return Constraint(proto);
}

Constraint CpModelBuilder::AddAbsEquality(IntVar target, IntVar var) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_int_max()->set_target(GetOrCreateIntegerIndex(target.index_));
  proto->mutable_int_max()->add_vars(GetOrCreateIntegerIndex(var.index_));
  proto->mutable_int_max()->add_vars(
      NegatedRef(GetOrCreateIntegerIndex(var.index_)));
  return Constraint(proto);
}

Constraint CpModelBuilder::AddModuloEquality(IntVar target, IntVar var,
                                             IntVar mod) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_int_mod()->set_target(GetOrCreateIntegerIndex(target.index_));
  proto->mutable_int_mod()->add_vars(GetOrCreateIntegerIndex(var.index_));
  proto->mutable_int_mod()->add_vars(GetOrCreateIntegerIndex(mod.index_));
  return Constraint(proto);
}

Constraint CpModelBuilder::AddProductEquality(IntVar target,
                                              absl::Span<const IntVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_int_prod()->set_target(GetOrCreateIntegerIndex(target.index_));
  for (const IntVar& var : vars) {
    proto->mutable_int_prod()->add_vars(GetOrCreateIntegerIndex(var.index_));
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddNoOverlap(absl::Span<const IntervalVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntervalVar& var : vars) {
    proto->mutable_no_overlap()->add_intervals(
        GetOrCreateIntegerIndex(var.index_));
  }
  return Constraint(proto);
}

NoOverlap2DConstraint CpModelBuilder::AddNoOverlap2D() {
  return NoOverlap2DConstraint(cp_model_.add_constraints());
}

CumulativeConstraint CpModelBuilder::AddCumulative(IntVar capacity) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_cumulative()->set_capacity(
      GetOrCreateIntegerIndex(capacity.index_));
  return CumulativeConstraint(proto, this);
}

void CpModelBuilder::Minimize(const LinearExpr& expr) {
  cp_model_.mutable_objective()->Clear();
  for (const IntVar& x : expr.variables()) {
    cp_model_.mutable_objective()->add_vars(x.index_);
  }
  for (const int64_t coeff : expr.coefficients()) {
    cp_model_.mutable_objective()->add_coeffs(coeff);
  }
  cp_model_.mutable_objective()->set_offset(expr.constant());
}

void CpModelBuilder::Maximize(const LinearExpr& expr) {
  cp_model_.mutable_objective()->Clear();
  for (const IntVar& x : expr.variables()) {
    cp_model_.mutable_objective()->add_vars(x.index_);
  }
  for (const int64_t coeff : expr.coefficients()) {
    cp_model_.mutable_objective()->add_coeffs(-coeff);
  }
  cp_model_.mutable_objective()->set_offset(-expr.constant());
  cp_model_.mutable_objective()->set_scaling_factor(-1.0);
}

void CpModelBuilder::ScaleObjectiveBy(double scaling) {
  CHECK(cp_model_.has_objective());
  cp_model_.mutable_objective()->set_scaling_factor(
      scaling * cp_model_.objective().scaling_factor());
}

void CpModelBuilder::AddDecisionStrategy(
    absl::Span<const IntVar> variables,
    DecisionStrategyProto::VariableSelectionStrategy var_strategy,
    DecisionStrategyProto::DomainReductionStrategy domain_strategy) {
  DecisionStrategyProto* const proto = cp_model_.add_search_strategy();
  for (const IntVar& var : variables) {
    proto->add_variables(var.index_);
  }
  proto->set_variable_selection_strategy(var_strategy);
  proto->set_domain_reduction_strategy(domain_strategy);
}

void CpModelBuilder::AddDecisionStrategy(
    absl::Span<const BoolVar> variables,
    DecisionStrategyProto::VariableSelectionStrategy var_strategy,
    DecisionStrategyProto::DomainReductionStrategy domain_strategy) {
  DecisionStrategyProto* const proto = cp_model_.add_search_strategy();
  for (const BoolVar& var : variables) {
    proto->add_variables(var.index_);
  }
  proto->set_variable_selection_strategy(var_strategy);
  proto->set_domain_reduction_strategy(domain_strategy);
}

void CpModelBuilder::AddHint(IntVar var, int64_t value) {
  cp_model_.mutable_solution_hint()->add_vars(
      GetOrCreateIntegerIndex(var.index_));
  cp_model_.mutable_solution_hint()->add_values(value);
}

void CpModelBuilder::ClearHints() {
  cp_model_.mutable_solution_hint()->Clear();
}

void CpModelBuilder::AddAssumption(BoolVar lit) {
  cp_model_.mutable_assumptions()->Add(lit.index_);
}

void CpModelBuilder::AddAssumptions(absl::Span<const BoolVar> literals) {
  for (const BoolVar& lit : literals) {
    cp_model_.mutable_assumptions()->Add(lit.index_);
  }
}

void CpModelBuilder::ClearAssumptions() {
  cp_model_.mutable_assumptions()->Clear();
}

void CpModelBuilder::CopyFrom(const CpModelProto& model_proto) {
  cp_model_ = model_proto;
  // Rebuild constant to index map.
  constant_to_index_map_.clear();
  for (int i = 0; i < cp_model_.variables_size(); ++i) {
    const IntegerVariableProto& var = cp_model_.variables(i);
    if (var.domain_size() == 2 && var.domain(0) == var.domain(1)) {
      constant_to_index_map_[var.domain(0)] = i;
    }
  }
  // This one would be more complicated to rebuild. Let's just clear it.
  bool_to_integer_index_map_.clear();
}

BoolVar CpModelBuilder::GetBoolVarFromProtoIndex(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, cp_model_.variables_size());
  const IntegerVariableProto& proto = cp_model_.variables(index);
  CHECK_EQ(2, proto.domain_size())
      << "CpModelBuilder::GetBoolVarFromProtoIndex: The domain of the variable "
         "is not Boolean";
  CHECK_GE(0, proto.domain(0))
      << "CpModelBuilder::GetBoolVarFromProtoIndex: The domain of the variable "
         "is not Boolean";
  CHECK_LE(1, proto.domain(1))
      << "CpModelBuilder::GetBoolVarFromProtoIndex: The domain of the variable "
         "is not Boolean";
  return BoolVar(index, &cp_model_);
}

IntVar CpModelBuilder::GetIntVarFromProtoIndex(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, cp_model_.variables_size());
  return IntVar(index, &cp_model_);
}

IntervalVar CpModelBuilder::GetIntervalVarFromProtoIndex(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, cp_model_.constraints_size());
  const ConstraintProto& ct = cp_model_.constraints(index);
  CHECK_EQ(ct.constraint_case(), ConstraintProto::kInterval)
      << "CpModelBuilder::GetIntervalVarFromProtoIndex: the referenced "
         "object is not an interval variable";
  return IntervalVar(index, &cp_model_);
}

int64_t SolutionIntegerValue(const CpSolverResponse& r,
                             const LinearExpr& expr) {
  int64_t result = expr.constant();
  const std::vector<IntVar>& variables = expr.variables();
  const std::vector<int64_t>& coefficients = expr.coefficients();
  for (int i = 0; i < variables.size(); ++i) {
    result += r.solution(variables[i].index_) * coefficients[i];
  }
  return result;
}

int64_t SolutionIntegerMin(const CpSolverResponse& r, IntVar x) {
  if (r.solution_size() > 0) {
    return r.solution(x.index_);
  } else {
    return r.solution_lower_bounds(x.index_);
  }
}

int64_t SolutionIntegerMax(const CpSolverResponse& r, IntVar x) {
  if (r.solution_size() > 0) {
    return r.solution(x.index_);
  } else {
    return r.solution_upper_bounds(x.index_);
  }
}

bool SolutionBooleanValue(const CpSolverResponse& r, BoolVar x) {
  const int ref = x.index_;
  if (RefIsPositive(ref)) {
    return r.solution(ref) == 1;
  } else {
    return r.solution(PositiveRef(ref)) == 0;
  }
}

}  // namespace sat
}  // namespace operations_research
