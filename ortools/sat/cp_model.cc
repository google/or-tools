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
#include <initializer_list>
#include <limits>

#include "absl/strings/str_format.h"
#include "ortools/base/map_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

BoolVar::BoolVar() : builder_(nullptr), index_(0) {}

BoolVar::BoolVar(int index, CpModelBuilder* builder)
    : builder_(builder), index_(index) {}

BoolVar BoolVar::WithName(const std::string& name) {
  DCHECK(builder_ != nullptr);
  if (builder_ == nullptr) return *this;
  builder_->MutableProto()
      ->mutable_variables(PositiveRef(index_))
      ->set_name(name);
  return *this;
}

std::string BoolVar::Name() const {
  if (builder_ == nullptr) return "null";
  const std::string& name =
      builder_->Proto().variables(PositiveRef(index_)).name();
  if (RefIsPositive(index_)) {
    return name;
  } else {
    return absl::StrCat("Not(", name, ")");
  }
}

std::string BoolVar::DebugString() const {
  if (builder_ == nullptr) return "null";
  if (index_ < 0) {
    return absl::StrFormat("Not(%s)", Not().DebugString());
  } else {
    std::string output;
    const IntegerVariableProto& var_proto = builder_->Proto().variables(index_);
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

IntVar::IntVar() : builder_(nullptr), index_(0) {}

IntVar::IntVar(int index, CpModelBuilder* builder)
    : builder_(builder), index_(index) {
  DCHECK(RefIsPositive(index_));
}

IntVar::IntVar(const BoolVar& var) {
  if (var.builder_ == nullptr) {
    *this = IntVar();
    return;
  }
  builder_ = var.builder_;
  index_ = builder_->GetOrCreateIntegerIndex(var.index_);
  DCHECK(RefIsPositive(index_));
}

BoolVar IntVar::ToBoolVar() const {
  if (builder_ != nullptr) {
    const IntegerVariableProto& proto = builder_->Proto().variables(index_);
    DCHECK_EQ(2, proto.domain_size());
    DCHECK_GE(proto.domain(0), 0);
    DCHECK_LE(proto.domain(1), 1);
  }
  return BoolVar(index_, builder_);
}

IntVar IntVar::WithName(const std::string& name) {
  DCHECK(builder_ != nullptr);
  if (builder_ == nullptr) return *this;
  builder_->MutableProto()->mutable_variables(index_)->set_name(name);
  return *this;
}

std::string IntVar::Name() const {
  if (builder_ == nullptr) return "null";
  return builder_->Proto().variables(index_).name();
}

LinearExpr IntVar::AddConstant(int64_t value) const {
  return LinearExpr(*this).AddConstant(value);
}

  ::operations_research::Domain IntVar::Domain() const {
  if (builder_ == nullptr) return Domain();
  return ReadDomainFromProto(builder_->Proto().variables(index_));
}

std::string IntVar::DebugString() const {
  if (builder_ == nullptr) return "null";
  return VarDebugString(builder_->Proto(), index_);
}

// TODO(user): unfortunately, we need this indirection to get a DebugString()
// in a const way from an index. Because building an IntVar is non-const.
std::string VarDebugString(const CpModelProto& proto, int index) {
  std::string output;

  // Special case for constant variables without names.
  const IntegerVariableProto& var_proto = proto.variables(index);
  if (var_proto.name().empty() && var_proto.domain_size() == 2 &&
      var_proto.domain(0) == var_proto.domain(1)) {
    absl::StrAppend(&output, var_proto.domain(0));
  } else {
    if (var_proto.name().empty()) {
      absl::StrAppend(&output, "V", index, "(");
    } else {
      absl::StrAppend(&output, var_proto.name(), "(");
    }

    // TODO(user): Use domain pretty print function.
    if (var_proto.domain_size() == 2 &&
        var_proto.domain(0) == var_proto.domain(1)) {
      absl::StrAppend(&output, var_proto.domain(0), ")");
    } else {
      absl::StrAppend(&output, var_proto.domain(0), ", ", var_proto.domain(1),
                      ")");
    }
  }

  return output;
}

std::ostream& operator<<(std::ostream& os, const IntVar& var) {
  os << var.DebugString();
  return os;
}

LinearExpr::LinearExpr(BoolVar var) { AddVar(var); }

LinearExpr::LinearExpr(IntVar var) { AddVar(var); }

LinearExpr::LinearExpr(int64_t constant) { constant_ = constant; }

LinearExpr LinearExpr::FromProto(const LinearExpressionProto& expr_proto) {
  LinearExpr result(expr_proto.offset());
  for (int i = 0; i < expr_proto.vars_size(); ++i) {
    result.variables_.push_back(expr_proto.vars(i));
    result.coefficients_.push_back(expr_proto.coeffs(i));
  }
  return result;
}

LinearExpr LinearExpr::Sum(absl::Span<const IntVar> vars) {
  LinearExpr result;
  for (const IntVar& var : vars) {
    result.AddVar(var);
  }
  return result;
}

LinearExpr LinearExpr::Sum(absl::Span<const BoolVar> vars) {
  LinearExpr result;
  for (const BoolVar& var : vars) {
    result.AddVar(var);
  }
  return result;
}

LinearExpr LinearExpr::Sum(std::initializer_list<IntVar> vars) {
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

LinearExpr LinearExpr::ScalProd(absl::Span<const BoolVar> vars,
                                absl::Span<const int64_t> coeffs) {
  CHECK_EQ(vars.size(), coeffs.size());
  LinearExpr result;
  for (int i = 0; i < vars.size(); ++i) {
    result.AddTerm(vars[i], coeffs[i]);
  }
  return result;
}

LinearExpr LinearExpr::ScalProd(std::initializer_list<IntVar> vars,
                                absl::Span<const int64_t> coeffs) {
  CHECK_EQ(vars.size(), coeffs.size());
  LinearExpr result;
  int count = 0;
  for (const IntVar& var : vars) {
    result.AddTerm(var, coeffs[count++]);
  }
  return result;
}

LinearExpr LinearExpr::Term(IntVar var, int64_t coefficient) {
  LinearExpr result;
  result.AddTerm(var, coefficient);
  return result;
}

LinearExpr LinearExpr::Term(BoolVar var, int64_t coefficient) {
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

LinearExpr& LinearExpr::AddVar(IntVar var) { return AddTerm(var, 1); }

LinearExpr& LinearExpr::AddVar(BoolVar var) { return AddTerm(var, 1); }

LinearExpr& LinearExpr::AddTerm(IntVar var, int64_t coeff) {
  DCHECK(var.builder_ != nullptr);
  variables_.push_back(var.index_);
  coefficients_.push_back(coeff);
  return *this;
}

LinearExpr& LinearExpr::AddTerm(BoolVar var, int64_t coeff) {
  DCHECK(var.builder_ != nullptr);
  const int index = var.index_;
  if (RefIsPositive(index)) {
    variables_.push_back(index);
    coefficients_.push_back(coeff);
  } else {
    // We add 1 - var instead.
    variables_.push_back(PositiveRef(index));
    coefficients_.push_back(-coeff);
    constant_ += coeff;
  }
  return *this;
}

LinearExpr& LinearExpr::operator+=(const LinearExpr& other) {
  constant_ += other.constant_;
  variables_.insert(variables_.end(), other.variables_.begin(),
                    other.variables_.end());
  coefficients_.insert(coefficients_.end(), other.coefficients_.begin(),
                       other.coefficients_.end());
  return *this;
}

LinearExpr& LinearExpr::operator-=(const LinearExpr& other) {
  constant_ -= other.constant_;
  variables_.insert(variables_.end(), other.variables_.begin(),
                    other.variables_.end());
  for (const int64_t coeff : other.coefficients_) {
    coefficients_.push_back(-coeff);
  }
  return *this;
}

LinearExpr& LinearExpr::operator*=(int64_t factor) {
  constant_ *= factor;
  for (int64_t& coeff : coefficients_) coeff *= factor;
  return *this;
}

std::string LinearExpr::DebugString(const CpModelProto* proto) const {
  std::string result;
  for (int i = 0; i < variables_.size(); ++i) {
    const int64_t coeff = coefficients_[i];
    const std::string var_string = proto == nullptr
                                       ? absl::StrCat("V", variables_[i])
                                       : VarDebugString(*proto, variables_[i]);
    if (i == 0) {
      if (coeff == 1) {
        absl::StrAppend(&result, var_string);
      } else if (coeff == -1) {
        absl::StrAppend(&result, "-", var_string);
      } else if (coeff != 0) {
        absl::StrAppend(&result, coeff, " * ", var_string);
      }
    } else if (coeff == 1) {
      absl::StrAppend(&result, " + ", var_string);
    } else if (coeff > 0) {
      absl::StrAppend(&result, " + ", coeff, " * ", var_string);
    } else if (coeff == -1) {
      absl::StrAppend(&result, " - ", var_string);
    } else if (coeff < 0) {
      absl::StrAppend(&result, " - ", -coeff, " * ", var_string);
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

DoubleLinearExpr::DoubleLinearExpr() {}

DoubleLinearExpr::DoubleLinearExpr(BoolVar var) { AddTerm(var, 1); }

DoubleLinearExpr::DoubleLinearExpr(IntVar var) { AddTerm(var, 1); }

DoubleLinearExpr::DoubleLinearExpr(double constant) { constant_ = constant; }

DoubleLinearExpr DoubleLinearExpr::Sum(absl::Span<const IntVar> vars) {
  DoubleLinearExpr result;
  for (const IntVar& var : vars) {
    result.AddTerm(var, 1.0);
  }
  return result;
}

DoubleLinearExpr DoubleLinearExpr::Sum(absl::Span<const BoolVar> vars) {
  DoubleLinearExpr result;
  for (const BoolVar& var : vars) {
    result.AddTerm(var, 1.0);
  }
  return result;
}

DoubleLinearExpr DoubleLinearExpr::Sum(std::initializer_list<IntVar> vars) {
  DoubleLinearExpr result;
  for (const IntVar& var : vars) {
    result.AddTerm(var, 1.0);
  }
  return result;
}

DoubleLinearExpr DoubleLinearExpr::ScalProd(absl::Span<const IntVar> vars,
                                            absl::Span<const double> coeffs) {
  CHECK_EQ(vars.size(), coeffs.size());
  DoubleLinearExpr result;
  for (int i = 0; i < vars.size(); ++i) {
    result.AddTerm(vars[i], coeffs[i]);
  }
  return result;
}

DoubleLinearExpr DoubleLinearExpr::ScalProd(absl::Span<const BoolVar> vars,
                                            absl::Span<const double> coeffs) {
  CHECK_EQ(vars.size(), coeffs.size());
  DoubleLinearExpr result;
  for (int i = 0; i < vars.size(); ++i) {
    result.AddTerm(vars[i], coeffs[i]);
  }
  return result;
}

DoubleLinearExpr DoubleLinearExpr::ScalProd(std::initializer_list<IntVar> vars,
                                            absl::Span<const double> coeffs) {
  CHECK_EQ(vars.size(), coeffs.size());
  DoubleLinearExpr result;
  int count = 0;
  for (const IntVar& var : vars) {
    result.AddTerm(var, coeffs[count++]);
  }
  return result;
}

DoubleLinearExpr& DoubleLinearExpr::operator+=(double value) {
  constant_ += value;
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::AddConstant(double constant) {
  constant_ += constant;
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::operator+=(IntVar var) {
  AddTerm(var, 1);
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::operator+=(BoolVar var) {
  AddTerm(var, 1);
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::operator+=(const DoubleLinearExpr& expr) {
  constant_ += expr.constant_;
  variables_.insert(variables_.end(), expr.variables_.begin(),
                    expr.variables_.end());
  coefficients_.insert(coefficients_.end(), expr.coefficients_.begin(),
                       expr.coefficients_.end());
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::AddTerm(IntVar var, double coeff) {
  variables_.push_back(var.index_);
  coefficients_.push_back(coeff);
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::AddTerm(BoolVar var, double coeff) {
  const int index = var.index_;
  if (RefIsPositive(index)) {
    variables_.push_back(index);
    coefficients_.push_back(coeff);
  } else {
    variables_.push_back(PositiveRef(index));
    coefficients_.push_back(-coeff);
    constant_ += coeff;
  }
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::operator-=(double value) {
  constant_ -= value;
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::operator-=(IntVar var) {
  AddTerm(var, -1.0);
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::operator-=(const DoubleLinearExpr& expr) {
  constant_ -= expr.constant_;
  variables_.insert(variables_.end(), expr.variables_.begin(),
                    expr.variables_.end());
  for (const double coeff : expr.coefficients()) {
    coefficients_.push_back(-coeff);
  }
  return *this;
}

DoubleLinearExpr& DoubleLinearExpr::operator*=(double coeff) {
  constant_ *= coeff;
  for (double& c : coefficients_) {
    c *= coeff;
  }
  return *this;
}

std::string DoubleLinearExpr::DebugString(const CpModelProto* proto) const {
  std::string result;
  for (int i = 0; i < variables_.size(); ++i) {
    const double coeff = coefficients_[i];
    const std::string var_string = proto == nullptr
                                       ? absl::StrCat("V", variables_[i])
                                       : VarDebugString(*proto, variables_[i]);
    if (i == 0) {
      if (coeff == 1.0) {
        absl::StrAppend(&result, var_string);
      } else if (coeff == -1.0) {
        absl::StrAppend(&result, "-", var_string);
      } else if (coeff != 0.0) {
        absl::StrAppend(&result, coeff, " * ", var_string);
      }
    } else if (coeff == 1.0) {
      absl::StrAppend(&result, " + ", var_string);
    } else if (coeff > 0.0) {
      absl::StrAppend(&result, " + ", coeff, " * ", var_string);
    } else if (coeff == -1.0) {
      absl::StrAppend(&result, " - ", var_string);
    } else if (coeff < 0.0) {
      absl::StrAppend(&result, " - ", -coeff, " * ", var_string);
    }
  }

  if (constant_ != 0.0) {
    if (variables_.empty()) {
      return absl::StrCat(constant_);
    } else if (constant_ > 0.0) {
      absl::StrAppend(&result, " + ", constant_);
    } else {
      absl::StrAppend(&result, " - ", -constant_);
    }
  }
  return result;
}

std::ostream& operator<<(std::ostream& os, const DoubleLinearExpr& e) {
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

void ReservoirConstraint::AddEvent(LinearExpr time, int64_t level_change) {
  *proto_->mutable_reservoir()->add_time_exprs() =
      builder_->LinearExprToProto(time);
  proto_->mutable_reservoir()->add_level_changes(level_change);
  proto_->mutable_reservoir()->add_active_literals(
      builder_->IndexFromConstant(1));
}

void ReservoirConstraint::AddOptionalEvent(LinearExpr time,
                                           int64_t level_change,
                                           BoolVar is_active) {
  *proto_->mutable_reservoir()->add_time_exprs() =
      builder_->LinearExprToProto(time);
  proto_->mutable_reservoir()->add_level_changes(level_change);
  proto_->mutable_reservoir()->add_active_literals(is_active.index_);
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

void CumulativeConstraint::AddDemand(IntervalVar interval, LinearExpr demand) {
  proto_->mutable_cumulative()->add_intervals(interval.index_);
  *proto_->mutable_cumulative()->add_demands() =
      builder_->LinearExprToProto(demand);
}

IntervalVar::IntervalVar() : builder_(nullptr), index_() {}

IntervalVar::IntervalVar(int index, CpModelBuilder* builder)
    : builder_(builder), index_(index) {}

IntervalVar IntervalVar::WithName(const std::string& name) {
  DCHECK(builder_ != nullptr);
  if (builder_ == nullptr) return *this;
  builder_->MutableProto()->mutable_constraints(index_)->set_name(name);
  return *this;
}

LinearExpr IntervalVar::StartExpr() const {
  DCHECK(builder_ != nullptr);
  if (builder_ == nullptr) return LinearExpr();
  return LinearExpr::FromProto(
      builder_->Proto().constraints(index_).interval().start());
}

LinearExpr IntervalVar::SizeExpr() const {
  DCHECK(builder_ != nullptr);
  if (builder_ == nullptr) return LinearExpr();
  return LinearExpr::FromProto(
      builder_->Proto().constraints(index_).interval().size());
}

LinearExpr IntervalVar::EndExpr() const {
  DCHECK(builder_ != nullptr);
  if (builder_ == nullptr) return LinearExpr();
  return LinearExpr::FromProto(
      builder_->Proto().constraints(index_).interval().end());
}

BoolVar IntervalVar::PresenceBoolVar() const {
  DCHECK(builder_ != nullptr);
  if (builder_ == nullptr) return BoolVar();
  return BoolVar(builder_->Proto().constraints(index_).enforcement_literal(0),
                 builder_);
}

std::string IntervalVar::Name() const {
  if (builder_ == nullptr) return "null";
  return builder_->Proto().constraints(index_).name();
}

std::string IntervalVar::DebugString() const {
  if (builder_ == nullptr) return "null";

  CHECK_GE(index_, 0);
  const CpModelProto& proto = builder_->Proto();
  const ConstraintProto& ct_proto = proto.constraints(index_);
  std::string output;
  if (ct_proto.name().empty()) {
    absl::StrAppend(&output, "IntervalVar", index_, "(");
  } else {
    absl::StrAppend(&output, ct_proto.name(), "(");
  }
  absl::StrAppend(&output, StartExpr().DebugString(&proto), ", ",
                  SizeExpr().DebugString(&proto), ", ",
                  EndExpr().DebugString(&proto), ", ",
                  PresenceBoolVar().DebugString(), ")");
  return output;
}

std::ostream& operator<<(std::ostream& os, const IntervalVar& var) {
  os << var.DebugString();
  return os;
}

void CpModelBuilder::SetName(const std::string& name) {
  cp_model_.set_name(name);
}

int CpModelBuilder::IndexFromConstant(int64_t value) {
  if (!constant_to_index_map_.contains(value)) {
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
  if (!bool_to_integer_index_map_.contains(index)) {
    const int var = PositiveRef(index);
    const IntegerVariableProto& old_var = cp_model_.variables(var);
    const int new_index = cp_model_.variables_size();
    IntegerVariableProto* const new_var = cp_model_.add_variables();
    new_var->add_domain(0);
    new_var->add_domain(1);
    if (!old_var.name().empty()) {
      new_var->set_name(absl::StrCat("Not(", old_var.name(), ")"));
    }
    AddEquality(IntVar(new_index, this), BoolVar(index, this));
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
  return IntVar(index, this);
}

BoolVar CpModelBuilder::NewBoolVar() {
  const int index = cp_model_.variables_size();
  IntegerVariableProto* const var_proto = cp_model_.add_variables();
  var_proto->add_domain(0);
  var_proto->add_domain(1);
  return BoolVar(index, this);
}

IntVar CpModelBuilder::NewConstant(int64_t value) {
  return IntVar(IndexFromConstant(value), this);
}

BoolVar CpModelBuilder::TrueVar() {
  return BoolVar(IndexFromConstant(1), this);
}

BoolVar CpModelBuilder::FalseVar() {
  return BoolVar(IndexFromConstant(0), this);
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
  *interval->mutable_start() = LinearExprToProto(start);
  *interval->mutable_size() = LinearExprToProto(size);
  *interval->mutable_end() = LinearExprToProto(end);
  return IntervalVar(index, this);
}

IntervalVar CpModelBuilder::NewOptionalFixedSizeIntervalVar(
    const LinearExpr& start, int64_t size, BoolVar presence) {
  const int index = cp_model_.constraints_size();
  ConstraintProto* const ct = cp_model_.add_constraints();
  ct->add_enforcement_literal(presence.index_);
  IntervalConstraintProto* const interval = ct->mutable_interval();
  *interval->mutable_start() = LinearExprToProto(start);
  interval->mutable_size()->set_offset(size);
  *interval->mutable_end() = LinearExprToProto(start);
  interval->mutable_end()->set_offset(interval->end().offset() + size);
  return IntervalVar(index, this);
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
  for (const int x : left.variables()) {
    proto->add_vars(x);
  }
  for (const int64_t coeff : left.coefficients()) {
    proto->add_coeffs(coeff);
  }
  for (const int x : right.variables()) {
    proto->add_vars(x);
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
  for (const int x : expr.variables()) {
    proto->mutable_linear()->add_vars(x);
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
    auto* expr = proto->mutable_all_diff()->add_exprs();
    expr->add_vars(var.index_);
    expr->add_coeffs(1);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddAllDifferent(absl::Span<const LinearExpr> exprs) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const LinearExpr& expr : exprs) {
    *proto->mutable_all_diff()->add_exprs() = LinearExprToProto(expr);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddAllDifferent(
    std::initializer_list<LinearExpr> exprs) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const LinearExpr& expr : exprs) {
    *proto->mutable_all_diff()->add_exprs() = LinearExprToProto(expr);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddVariableElement(
    IntVar index, absl::Span<const IntVar> variables, IntVar target) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_element()->set_index(index.index_);
  proto->mutable_element()->set_target(target.index_);
  for (const IntVar& var : variables) {
    proto->mutable_element()->add_vars(var.index_);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddElement(IntVar index,
                                      absl::Span<const int64_t> values,
                                      IntVar target) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  proto->mutable_element()->set_index(index.index_);
  proto->mutable_element()->set_target(target.index_);
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
    proto->mutable_table()->add_vars(var.index_);
  }
  return TableConstraint(proto);
}

TableConstraint CpModelBuilder::AddForbiddenAssignments(
    absl::Span<const IntVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntVar& var : vars) {
    proto->mutable_table()->add_vars(var.index_);
  }
  proto->mutable_table()->set_negated(true);
  return TableConstraint(proto);
}

Constraint CpModelBuilder::AddInverseConstraint(
    absl::Span<const IntVar> variables,
    absl::Span<const IntVar> inverse_variables) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntVar& var : variables) {
    proto->mutable_inverse()->add_f_direct(var.index_);
  }
  for (const IntVar& var : inverse_variables) {
    proto->mutable_inverse()->add_f_inverse(var.index_);
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
    proto->mutable_automaton()->add_vars(var.index_);
  }
  proto->mutable_automaton()->set_starting_state(starting_state);
  for (const int final_state : final_states) {
    proto->mutable_automaton()->add_final_states(final_state);
  }
  return AutomatonConstraint(proto);
}

LinearExpressionProto CpModelBuilder::LinearExprToProto(const LinearExpr& expr,
                                                        bool negate) {
  LinearExpressionProto expr_proto;
  for (const int var : expr.variables()) {
    expr_proto.add_vars(var);
  }
  const int64_t mult = negate ? -1 : 1;
  for (const int64_t coeff : expr.coefficients()) {
    expr_proto.add_coeffs(coeff * mult);
  }
  expr_proto.set_offset(expr.constant() * mult);
  return expr_proto;
}

Constraint CpModelBuilder::AddMinEquality(const LinearExpr& target,
                                          absl::Span<const IntVar> vars) {
  ConstraintProto* ct = cp_model_.add_constraints();
  *ct->mutable_lin_max()->mutable_target() =
      LinearExprToProto(target, /*negate=*/true);
  for (const IntVar& var : vars) {
    *ct->mutable_lin_max()->add_exprs() =
        LinearExprToProto(var, /*negate=*/true);
  }
  return Constraint(ct);
}

Constraint CpModelBuilder::AddMinEquality(const LinearExpr& target,
                                          absl::Span<const LinearExpr> exprs) {
  ConstraintProto* ct = cp_model_.add_constraints();
  *ct->mutable_lin_max()->mutable_target() =
      LinearExprToProto(target, /*negate=*/true);
  for (const LinearExpr& expr : exprs) {
    *ct->mutable_lin_max()->add_exprs() =
        LinearExprToProto(expr, /*negate=*/true);
  }
  return Constraint(ct);
}

Constraint CpModelBuilder::AddMinEquality(
    const LinearExpr& target, std::initializer_list<LinearExpr> exprs) {
  ConstraintProto* ct = cp_model_.add_constraints();
  *ct->mutable_lin_max()->mutable_target() =
      LinearExprToProto(target, /*negate=*/true);
  for (const LinearExpr& expr : exprs) {
    *ct->mutable_lin_max()->add_exprs() =
        LinearExprToProto(expr, /*negate=*/true);
  }
  return Constraint(ct);
}

Constraint CpModelBuilder::AddMaxEquality(const LinearExpr& target,
                                          absl::Span<const IntVar> vars) {
  ConstraintProto* ct = cp_model_.add_constraints();
  *ct->mutable_lin_max()->mutable_target() = LinearExprToProto(target);
  for (const IntVar& var : vars) {
    *ct->mutable_lin_max()->add_exprs() = LinearExprToProto(var);
  }
  return Constraint(ct);
}

Constraint CpModelBuilder::AddMaxEquality(const LinearExpr& target,
                                          absl::Span<const LinearExpr> exprs) {
  ConstraintProto* ct = cp_model_.add_constraints();
  *ct->mutable_lin_max()->mutable_target() = LinearExprToProto(target);
  for (const LinearExpr& expr : exprs) {
    *ct->mutable_lin_max()->add_exprs() = LinearExprToProto(expr);
  }
  return Constraint(ct);
}

Constraint CpModelBuilder::AddMaxEquality(
    const LinearExpr& target, std::initializer_list<LinearExpr> exprs) {
  ConstraintProto* ct = cp_model_.add_constraints();
  *ct->mutable_lin_max()->mutable_target() = LinearExprToProto(target);
  for (const LinearExpr& expr : exprs) {
    *ct->mutable_lin_max()->add_exprs() = LinearExprToProto(expr);
  }
  return Constraint(ct);
}

Constraint CpModelBuilder::AddDivisionEquality(const LinearExpr& target,
                                               const LinearExpr& numerator,
                                               const LinearExpr& denominator) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  *proto->mutable_int_div()->mutable_target() = LinearExprToProto(target);
  *proto->mutable_int_div()->add_exprs() = LinearExprToProto(numerator);
  *proto->mutable_int_div()->add_exprs() = LinearExprToProto(denominator);
  return Constraint(proto);
}

Constraint CpModelBuilder::AddAbsEquality(const LinearExpr& target,
                                          const LinearExpr& expr) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  *proto->mutable_lin_max()->mutable_target() = LinearExprToProto(target);
  *proto->mutable_lin_max()->add_exprs() = LinearExprToProto(expr);
  *proto->mutable_lin_max()->add_exprs() =
      LinearExprToProto(expr, /*negate=*/true);
  return Constraint(proto);
}

Constraint CpModelBuilder::AddModuloEquality(const LinearExpr& target,
                                             const LinearExpr& var,
                                             const LinearExpr& mod) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  *proto->mutable_int_mod()->mutable_target() = LinearExprToProto(target);
  *proto->mutable_int_mod()->add_exprs() = LinearExprToProto(var);
  *proto->mutable_int_mod()->add_exprs() = LinearExprToProto(mod);
  return Constraint(proto);
}

Constraint CpModelBuilder::AddMultiplicationEquality(
    const LinearExpr& target, absl::Span<const IntVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  *proto->mutable_int_prod()->mutable_target() = LinearExprToProto(target);
  for (const IntVar& var : vars) {
    *proto->mutable_int_prod()->add_exprs() = LinearExprToProto(var);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddMultiplicationEquality(
    const LinearExpr& target, absl::Span<const LinearExpr> exprs) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  *proto->mutable_int_prod()->mutable_target() = LinearExprToProto(target);
  for (const LinearExpr& expr : exprs) {
    *proto->mutable_int_prod()->add_exprs() = LinearExprToProto(expr);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddMultiplicationEquality(
    const LinearExpr& target, std::initializer_list<LinearExpr> exprs) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  *proto->mutable_int_prod()->mutable_target() = LinearExprToProto(target);
  for (const LinearExpr& expr : exprs) {
    *proto->mutable_int_prod()->add_exprs() = LinearExprToProto(expr);
  }
  return Constraint(proto);
}

Constraint CpModelBuilder::AddNoOverlap(absl::Span<const IntervalVar> vars) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  for (const IntervalVar& var : vars) {
    proto->mutable_no_overlap()->add_intervals(var.index_);
  }
  return Constraint(proto);
}

NoOverlap2DConstraint CpModelBuilder::AddNoOverlap2D() {
  return NoOverlap2DConstraint(cp_model_.add_constraints());
}

CumulativeConstraint CpModelBuilder::AddCumulative(LinearExpr capacity) {
  ConstraintProto* const proto = cp_model_.add_constraints();
  *proto->mutable_cumulative()->mutable_capacity() =
      LinearExprToProto(capacity);
  return CumulativeConstraint(proto, this);
}

void CpModelBuilder::Minimize(const LinearExpr& expr) {
  cp_model_.clear_objective();
  cp_model_.clear_floating_point_objective();
  for (const int x : expr.variables()) {
    cp_model_.mutable_objective()->add_vars(x);
  }
  for (const int64_t coeff : expr.coefficients()) {
    cp_model_.mutable_objective()->add_coeffs(coeff);
  }
  cp_model_.mutable_objective()->set_offset(expr.constant());
}

void CpModelBuilder::Maximize(const LinearExpr& expr) {
  cp_model_.clear_objective();
  cp_model_.clear_floating_point_objective();
  for (const int x : expr.variables()) {
    cp_model_.mutable_objective()->add_vars(x);
  }
  for (const int64_t coeff : expr.coefficients()) {
    cp_model_.mutable_objective()->add_coeffs(-coeff);
  }
  cp_model_.mutable_objective()->set_offset(-expr.constant());
  cp_model_.mutable_objective()->set_scaling_factor(-1.0);
}

void CpModelBuilder::Minimize(const DoubleLinearExpr& expr) {
  cp_model_.clear_objective();
  cp_model_.clear_floating_point_objective();
  for (int i = 0; i < expr.variables().size(); ++i) {
    cp_model_.mutable_floating_point_objective()->add_vars(expr.variables()[i]);
    cp_model_.mutable_floating_point_objective()->add_coeffs(
        expr.coefficients()[i]);
  }
  cp_model_.mutable_floating_point_objective()->set_offset(expr.constant());
  cp_model_.mutable_floating_point_objective()->set_maximize(false);
}

void CpModelBuilder::Maximize(const DoubleLinearExpr& expr) {
  cp_model_.clear_objective();
  cp_model_.clear_floating_point_objective();
  for (int i = 0; i < expr.variables().size(); ++i) {
    cp_model_.mutable_floating_point_objective()->add_vars(expr.variables()[i]);
    cp_model_.mutable_floating_point_objective()->add_coeffs(
        expr.coefficients()[i]);
  }
  cp_model_.mutable_floating_point_objective()->set_offset(expr.constant());
  cp_model_.mutable_floating_point_objective()->set_maximize(true);
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
  cp_model_.mutable_solution_hint()->add_vars(var.index_);
  cp_model_.mutable_solution_hint()->add_values(value);
}

void CpModelBuilder::AddHint(BoolVar var, bool value) {
  if (var.index_ >= 0) {
    cp_model_.mutable_solution_hint()->add_vars(var.index_);
    cp_model_.mutable_solution_hint()->add_values(value);
  } else {
    cp_model_.mutable_solution_hint()->add_vars(PositiveRef(var.index_));
    cp_model_.mutable_solution_hint()->add_values(!value);
  }
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
  return BoolVar(index, this);
}

IntVar CpModelBuilder::GetIntVarFromProtoIndex(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, cp_model_.variables_size());
  return IntVar(index, this);
}

IntervalVar CpModelBuilder::GetIntervalVarFromProtoIndex(int index) {
  CHECK_GE(index, 0);
  CHECK_LT(index, cp_model_.constraints_size());
  const ConstraintProto& ct = cp_model_.constraints(index);
  CHECK_EQ(ct.constraint_case(), ConstraintProto::kInterval)
      << "CpModelBuilder::GetIntervalVarFromProtoIndex: the referenced "
         "object is not an interval variable";
  return IntervalVar(index, this);
}

int64_t SolutionIntegerValue(const CpSolverResponse& r,
                             const LinearExpr& expr) {
  int64_t result = expr.constant();
  const std::vector<int>& variables = expr.variables();
  const std::vector<int64_t>& coefficients = expr.coefficients();
  for (int i = 0; i < variables.size(); ++i) {
    result += r.solution(variables[i]) * coefficients[i];
  }
  return result;
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
