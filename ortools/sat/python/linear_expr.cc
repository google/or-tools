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

#include "ortools/sat/python/linear_expr.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace python {

FloatLinearExpr* FloatLinearExpr::Sum(
    const std::vector<FloatExprOrValue>& exprs) {
  return Sum(exprs, 0.0);
}

FloatLinearExpr* FloatLinearExpr::Sum(
    const std::vector<FloatExprOrValue>& exprs, double cst) {
  std::vector<FloatLinearExpr*> lin_exprs;
  for (const FloatExprOrValue& choice : exprs) {
    if (choice.expr != nullptr) {
      lin_exprs.push_back(choice.expr);
    } else {
      cst += choice.value;
    }
  }
  if (lin_exprs.empty()) return new FloatConstant(cst);
  if (lin_exprs.size() == 1) return Affine(lin_exprs[0], 1.0, cst);
  return new FloatWeightedSum(lin_exprs, cst);
}

FloatLinearExpr* FloatLinearExpr::WeightedSum(
    const std::vector<FloatExprOrValue>& exprs,
    const std::vector<double>& coeffs) {
  return WeightedSum(exprs, coeffs, 0.0);
}

FloatLinearExpr* FloatLinearExpr::WeightedSum(
    const std::vector<FloatExprOrValue>& exprs,
    const std::vector<double>& coeffs, double cst) {
  std::vector<FloatLinearExpr*> lin_exprs;
  std::vector<double> lin_coeffs;
  for (int i = 0; i < exprs.size(); ++i) {
    if (exprs[i].expr != nullptr) {
      lin_exprs.push_back(exprs[i].expr);
      lin_coeffs.push_back(coeffs[i]);
    } else {
      cst += exprs[i].value * coeffs[i];
    }
  }

  if (lin_exprs.empty()) return new FloatConstant(cst);
  if (lin_exprs.size() == 1) {
    return Affine(lin_exprs[0], lin_coeffs[0], cst);
  }
  return new FloatWeightedSum(lin_exprs, lin_coeffs, cst);
}

FloatLinearExpr* FloatLinearExpr::Term(FloatLinearExpr* expr, double coeff) {
  return new FloatAffine(expr, coeff, 0.0);
}

FloatLinearExpr* FloatLinearExpr::Affine(FloatLinearExpr* expr, double coeff,
                                         double offset) {
  return new FloatAffine(expr, coeff, offset);
}

FloatLinearExpr* FloatLinearExpr::Constant(double value) {
  return new FloatConstant(value);
}

FloatLinearExpr* FloatLinearExpr::FloatAddCst(double cst) {
  if (cst == 0.0) return this;
  return new FloatAffine(this, 1.0, cst);
}

FloatLinearExpr* FloatLinearExpr::FloatAdd(FloatLinearExpr* other) {
  std::vector<FloatLinearExpr*> exprs;
  exprs.push_back(this);
  exprs.push_back(other);
  return new FloatWeightedSum(exprs, 0);
}

FloatLinearExpr* FloatLinearExpr::FloatSubCst(double cst) {
  if (cst == 0.0) return this;
  return new FloatAffine(this, 1.0, -cst);
}

FloatLinearExpr* FloatLinearExpr::FloatSub(FloatLinearExpr* other) {
  std::vector<FloatLinearExpr*> exprs;
  exprs.push_back(this);
  exprs.push_back(other);
  return new FloatWeightedSum(exprs, {1, -1}, 0);
}

FloatLinearExpr* FloatLinearExpr::FloatRSub(FloatLinearExpr* other) {
  std::vector<FloatLinearExpr*> exprs;
  exprs.push_back(this);
  exprs.push_back(other);
  return new FloatWeightedSum(exprs, {-1, 1}, 0);
}

FloatLinearExpr* FloatLinearExpr::FloatRSubCst(double cst) {
  return new FloatAffine(this, -1.0, cst);
}

FloatLinearExpr* FloatLinearExpr::FloatMulCst(double cst) {
  if (cst == 0.0) return Sum({});
  if (cst == 1.0) return this;
  return new FloatAffine(this, cst, 0.0);
}

FloatLinearExpr* FloatLinearExpr::FloatNeg() {
  return new FloatAffine(this, -1.0, 0.0);
}

void FloatExprVisitor::AddToProcess(FloatLinearExpr* expr, double coeff) {
  to_process_.push_back(std::make_pair(expr, coeff));
}
void FloatExprVisitor::AddConstant(double constant) { offset_ += constant; }
void FloatExprVisitor::AddVarCoeff(BaseIntVar* var, double coeff) {
  canonical_terms_[var] += coeff;
}
double FloatExprVisitor::Process(FloatLinearExpr* expr,
                                 std::vector<BaseIntVar*>* vars,
                                 std::vector<double>* coeffs) {
  AddToProcess(expr, 1.0);
  while (!to_process_.empty()) {
    const auto [expr, coeff] = to_process_.back();
    to_process_.pop_back();
    expr->VisitAsFloat(this, coeff);
  }

  vars->clear();
  coeffs->clear();
  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0.0) continue;
    vars->push_back(var);
    coeffs->push_back(coeff);
  }

  return offset_;
}

CanonicalFloatExpression::CanonicalFloatExpression(FloatLinearExpr* expr) {
  FloatExprVisitor lin;
  offset_ = lin.Process(expr, &vars_, &coeffs_);
}

void FloatConstant::VisitAsFloat(FloatExprVisitor* lin, double c) {
  lin->AddConstant(value_ * c);
}

std::string FloatConstant::ToString() const { return absl::StrCat(value_); }

std::string FloatConstant::DebugString() const {
  return absl::StrCat("FloatConstant(", value_, ")");
}

FloatWeightedSum::FloatWeightedSum(const std::vector<FloatLinearExpr*>& exprs,
                                   double offset)
    : exprs_(exprs.begin(), exprs.end()),
      coeffs_(exprs.size(), 1),
      offset_(offset) {}

FloatWeightedSum::FloatWeightedSum(const std::vector<FloatLinearExpr*>& exprs,
                                   const std::vector<double>& coeffs,
                                   double offset)
    : exprs_(exprs.begin(), exprs.end()),
      coeffs_(coeffs.begin(), coeffs.end()),
      offset_(offset) {}

void FloatWeightedSum::VisitAsFloat(FloatExprVisitor* lin, double c) {
  for (int i = 0; i < exprs_.size(); ++i) {
    lin->AddToProcess(exprs_[i], coeffs_[i] * c);
  }
  lin->AddConstant(offset_ * c);
}

std::string FloatWeightedSum::ToString() const {
  if (exprs_.empty()) {
    return absl::StrCat(offset_);
  }
  std::string s = "(";
  bool first_printed = true;
  for (int i = 0; i < exprs_.size(); ++i) {
    if (coeffs_[i] == 0.0) continue;
    if (first_printed) {
      first_printed = false;
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, exprs_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, "-", exprs_[i]->ToString());
      } else {
        absl::StrAppend(&s, coeffs_[i], " * ", exprs_[i]->ToString());
      }
    } else {
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, " + ", exprs_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, " - ", exprs_[i]->ToString());
      } else if (coeffs_[i] > 0.0) {
        absl::StrAppend(&s, " + ", coeffs_[i], " * ", exprs_[i]->ToString());
      } else {
        absl::StrAppend(&s, " - ", -coeffs_[i], " * ", exprs_[i]->ToString());
      }
    }
  }
  // If there are no terms, just print the offset.
  if (first_printed) {
    return absl::StrCat(offset_);
  }

  // If there is an offset, print it.
  if (offset_ != 0.0) {
    if (offset_ > 0.0) {
      absl::StrAppend(&s, " + ", offset_);
    } else {
      absl::StrAppend(&s, " - ", -offset_);
    }
  }
  absl::StrAppend(&s, ")");
  return s;
}

FloatAffine::FloatAffine(FloatLinearExpr* expr, double coeff, double offset)
    : expr_(expr), coeff_(coeff), offset_(offset) {}

void FloatAffine::VisitAsFloat(FloatExprVisitor* lin, double c) {
  lin->AddToProcess(expr_, c * coeff_);
  lin->AddConstant(offset_ * c);
}

std::string FloatAffine::ToString() const {
  std::string s = "(";
  if (coeff_ == 1.0) {
    absl::StrAppend(&s, expr_->ToString());
  } else if (coeff_ == -1.0) {
    absl::StrAppend(&s, "-", expr_->ToString());
  } else {
    absl::StrAppend(&s, coeff_, " * ", expr_->ToString());
  }
  if (offset_ > 0.0) {
    absl::StrAppend(&s, " + ", offset_);
  } else if (offset_ < 0.0) {
    absl::StrAppend(&s, " - ", -offset_);
  }
  absl::StrAppend(&s, ")");
  return s;
}

std::string FloatAffine::DebugString() const {
  return absl::StrCat("FloatAffine(expr=", expr_->DebugString(),
                      ", coeff=", coeff_, ", offset=", offset_, ")");
}

IntLinExpr* IntLinExpr::Sum(const std::vector<IntExprOrValue>& exprs) {
  return Sum(exprs, 0);
}

IntLinExpr* IntLinExpr::Sum(const std::vector<IntExprOrValue>& exprs,
                            int64_t cst) {
  std::vector<IntLinExpr*> lin_exprs;
  for (const IntExprOrValue& choice : exprs) {
    if (choice.expr != nullptr) {
      lin_exprs.push_back(choice.expr);
    } else {
      cst += choice.value;
    }
  }
  if (lin_exprs.empty()) return new IntConstant(cst);
  if (lin_exprs.size() == 1) return Affine(lin_exprs[0], 1, cst);
  return new IntSum(lin_exprs, cst);
}

IntLinExpr* IntLinExpr::WeightedSum(const std::vector<IntExprOrValue>& exprs,
                                    const std::vector<int64_t>& coeffs) {
  return WeightedSum(exprs, coeffs, 0);
}

IntLinExpr* IntLinExpr::WeightedSum(const std::vector<IntExprOrValue>& exprs,
                                    const std::vector<int64_t>& coeffs,
                                    int64_t cst) {
  std::vector<IntLinExpr*> lin_exprs;
  std::vector<int64_t> lin_coeffs;
  for (int i = 0; i < exprs.size(); ++i) {
    if (exprs[i].expr != nullptr) {
      lin_exprs.push_back(exprs[i].expr);
      lin_coeffs.push_back(coeffs[i]);
    } else {
      cst += exprs[i].value * coeffs[i];
    }
  }
  if (lin_exprs.empty()) return new IntConstant(cst);
  if (lin_exprs.size() == 1) {
    return IntLinExpr::Affine(lin_exprs[0], lin_coeffs[0], cst);
  }
  return new IntWeightedSum(lin_exprs, lin_coeffs, cst);
}

IntLinExpr* IntLinExpr::Term(IntLinExpr* expr, int64_t coeff) {
  return Affine(expr, coeff, 0);
}

IntLinExpr* IntLinExpr::Affine(IntLinExpr* expr, int64_t coeff,
                               int64_t offset) {
  if (coeff == 1 && offset == 0) return expr;
  if (coeff == 0) return new IntConstant(offset);
  return new IntAffine(expr, coeff, offset);
}

IntLinExpr* IntLinExpr::Constant(int64_t value) {
  return new IntConstant(value);
}

IntLinExpr* IntLinExpr::IntAddCst(int64_t cst) {
  if (cst == 0) return this;
  return new IntAffine(this, 1, cst);
}

IntLinExpr* IntLinExpr::IntAdd(IntLinExpr* other) {
  std::vector<IntLinExpr*> exprs;
  exprs.push_back(this);
  exprs.push_back(other);
  return new IntSum(exprs, 0);
}

IntLinExpr* IntLinExpr::IntSubCst(int64_t cst) {
  if (cst == 0) return this;
  return new IntAffine(this, 1, -cst);
}

IntLinExpr* IntLinExpr::IntSub(IntLinExpr* other) {
  std::vector<IntLinExpr*> exprs;
  exprs.push_back(this);
  exprs.push_back(other);
  return new IntWeightedSum(exprs, {1, -1}, 0);
}

IntLinExpr* IntLinExpr::IntRSubCst(int64_t cst) {
  return new IntAffine(this, -1, cst);
}

IntLinExpr* IntLinExpr::IntMulCst(int64_t cst) {
  if (cst == 0) return new IntConstant(0);
  if (cst == 1) return this;
  return new IntAffine(this, cst, 0);
}

IntLinExpr* IntLinExpr::IntNeg() { return new IntAffine(this, -1, 0); }

BoundedLinearExpression* IntLinExpr::Eq(IntLinExpr* other) {
  return new BoundedLinearExpression(this, other, Domain(0));
}

BoundedLinearExpression* IntLinExpr::EqCst(int64_t cst) {
  return new BoundedLinearExpression(this, Domain(cst));
}

BoundedLinearExpression* IntLinExpr::Ne(IntLinExpr* other) {
  return new BoundedLinearExpression(this, other, Domain(0).Complement());
}

BoundedLinearExpression* IntLinExpr::NeCst(int64_t cst) {
  return new BoundedLinearExpression(this, Domain(cst).Complement());
}

BoundedLinearExpression* IntLinExpr::Le(IntLinExpr* other) {
  return new BoundedLinearExpression(
      this, other, Domain(std::numeric_limits<int64_t>::min(), 0));
}

BoundedLinearExpression* IntLinExpr::LeCst(int64_t cst) {
  return new BoundedLinearExpression(
      this, Domain(std::numeric_limits<int64_t>::min(), cst));
}

BoundedLinearExpression* IntLinExpr::Lt(IntLinExpr* other) {
  return new BoundedLinearExpression(
      this, other, Domain(std::numeric_limits<int64_t>::min(), -1));
}

BoundedLinearExpression* IntLinExpr::LtCst(int64_t cst) {
  return new BoundedLinearExpression(
      this, Domain(std::numeric_limits<int64_t>::min(), cst - 1));
}

BoundedLinearExpression* IntLinExpr::Ge(IntLinExpr* other) {
  return new BoundedLinearExpression(
      this, other, Domain(0, std::numeric_limits<int64_t>::max()));
}

BoundedLinearExpression* IntLinExpr::GeCst(int64_t cst) {
  return new BoundedLinearExpression(
      this, Domain(cst, std::numeric_limits<int64_t>::max()));
}

BoundedLinearExpression* IntLinExpr::Gt(IntLinExpr* other) {
  return new BoundedLinearExpression(
      this, other, Domain(1, std::numeric_limits<int64_t>::max()));
}

BoundedLinearExpression* IntLinExpr::GtCst(int64_t cst) {
  return new BoundedLinearExpression(
      this, Domain(cst + 1, std::numeric_limits<int64_t>::max()));
}

void IntExprVisitor::AddToProcess(IntLinExpr* expr, int64_t coeff) {
  to_process_.push_back(std::make_pair(expr, coeff));
}

void IntExprVisitor::AddConstant(int64_t constant) { offset_ += constant; }

void IntExprVisitor::AddVarCoeff(BaseIntVar* var, int64_t coeff) {
  canonical_terms_[var] += coeff;
}

void IntExprVisitor::ProcessAll() {
  while (!to_process_.empty()) {
    const auto [expr, coeff] = to_process_.back();
    to_process_.pop_back();
    expr->VisitAsInt(this, coeff);
  }
}

int64_t IntExprVisitor::Process(std::vector<BaseIntVar*>* vars,
                                std::vector<int64_t>* coeffs) {
  ProcessAll();
  vars->clear();
  coeffs->clear();
  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0) continue;
    vars->push_back(var);
    coeffs->push_back(coeff);
  }

  return offset_;
}

int64_t IntExprVisitor::Evaluate(IntLinExpr* expr,
                                 const CpSolverResponse& solution) {
  AddToProcess(expr, 1);
  ProcessAll();
  int64_t value = offset_;
  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0) continue;
    value += coeff * solution.solution(var->index());
  }
  return value;
}

bool BaseIntVarComparator::operator()(const BaseIntVar* lhs,
                                      const BaseIntVar* rhs) const {
  return lhs->index() < rhs->index();
}

BaseIntVar::BaseIntVar(int index, bool is_boolean)
    : index_(index),
      negated_(is_boolean ? new NotBooleanVariable(this) : nullptr) {}

BoundedLinearExpression::BoundedLinearExpression(IntLinExpr* expr,
                                                 const Domain& bounds)
    : bounds_(bounds) {
  IntExprVisitor lin;
  lin.AddToProcess(expr, 1);
  offset_ = lin.Process(&vars_, &coeffs_);
}

BoundedLinearExpression::BoundedLinearExpression(IntLinExpr* pos,
                                                 IntLinExpr* neg,
                                                 const Domain& bounds)
    : bounds_(bounds) {
  IntExprVisitor lin;
  lin.AddToProcess(pos, 1);
  lin.AddToProcess(neg, -1);
  offset_ = lin.Process(&vars_, &coeffs_);
}

BoundedLinearExpression::BoundedLinearExpression(int64_t offset,
                                                 const Domain& bounds)
    : bounds_(bounds), offset_(offset) {}

const Domain& BoundedLinearExpression::bounds() const { return bounds_; }
const std::vector<BaseIntVar*>& BoundedLinearExpression::vars() const {
  return vars_;
}
const std::vector<int64_t>& BoundedLinearExpression::coeffs() const {
  return coeffs_;
}
int64_t BoundedLinearExpression::offset() const { return offset_; }

std::string BoundedLinearExpression::ToString() const {
  std::string s;
  if (vars_.empty()) {
    s = absl::StrCat(offset_);
  } else if (vars_.size() == 1) {
    const std::string var_name = vars_[0]->ToString();
    if (coeffs_[0] == 1) {
      s = var_name;
    } else if (coeffs_[0] == -1) {
      s = absl::StrCat("-", var_name);
    } else {
      s = absl::StrCat(coeffs_[0], " * ", var_name);
    }
    if (offset_ > 0) {
      absl::StrAppend(&s, " + ", offset_);
    } else if (offset_ < 0) {
      absl::StrAppend(&s, " - ", -offset_);
    }
  } else {
    s = "(";
    for (int i = 0; i < vars_.size(); ++i) {
      const std::string var_name = vars_[i]->ToString();
      if (i == 0) {
        if (coeffs_[i] == 1) {
          absl::StrAppend(&s, var_name);
        } else if (coeffs_[i] == -1) {
          absl::StrAppend(&s, "-", var_name);
        } else {
          absl::StrAppend(&s, coeffs_[i], " * ", var_name);
        }
      } else {
        if (coeffs_[i] == 1) {
          absl::StrAppend(&s, " + ", var_name);
        } else if (coeffs_[i] == -1) {
          absl::StrAppend(&s, " - ", var_name);
        } else if (coeffs_[i] > 1) {
          absl::StrAppend(&s, " + ", coeffs_[i], " * ", var_name);
        } else {
          absl::StrAppend(&s, " - ", -coeffs_[i], " * ", var_name);
        }
      }
    }
    if (offset_ > 0) {
      absl::StrAppend(&s, " + ", offset_);
    } else if (offset_ < 0) {
      absl::StrAppend(&s, " - ", -offset_);
    }
    absl::StrAppend(&s, ")");
  }
  if (bounds_.IsFixed()) {
    absl::StrAppend(&s, " == ", bounds_.Min());
  } else if (bounds_.NumIntervals() == 1) {
    if (bounds_.Min() == std::numeric_limits<int64_t>::min()) {
      if (bounds_.Max() == std::numeric_limits<int64_t>::max()) {
        return absl::StrCat("True (unbounded expr ", s, ")");
      } else {
        absl::StrAppend(&s, " <= ", bounds_.Max());
      }
    } else if (bounds_.Max() == std::numeric_limits<int64_t>::max()) {
      absl::StrAppend(&s, " >= ", bounds_.Min());
    } else {
      return absl::StrCat(bounds_.Min(), " <= ", s, " <= ", bounds_.Max());
    }
  } else if (bounds_.Complement().IsFixed()) {
    absl::StrAppend(&s, " != ", bounds_.Complement().Min());
  } else {
    absl::StrAppend(&s, " in ", bounds_.ToString());
  }
  return s;
}

std::string BoundedLinearExpression::DebugString() const {
  return absl::StrCat("BoundedLinearExpression(vars=[",
                      absl::StrJoin(vars_, ", ",
                                    [](std::string* out, BaseIntVar* var) {
                                      absl::StrAppend(out, var->DebugString());
                                    }),
                      "], coeffs=[", absl::StrJoin(coeffs_, ", "),
                      "], offset=", offset_, ", bounds=", bounds_.ToString(),
                      ")");
}

bool BoundedLinearExpression::CastToBool(bool* result) const {
  const bool is_zero = bounds_.IsFixed() && bounds_.FixedValue() == 0;
  const Domain complement = bounds_.Complement();
  const bool is_all_but_zero =
      complement.IsFixed() && complement.FixedValue() == 0;
  if (is_zero || is_all_but_zero) {
    if (vars_.empty()) {
      *result = is_zero;
      return true;
    } else if (vars_.size() == 2 && coeffs_[0] + coeffs_[1] == 0 &&
               std::abs(coeffs_[0]) == 1) {
      *result = is_all_but_zero;
      return true;
    }
  }
  return false;
}

}  // namespace python
}  // namespace sat
}  // namespace operations_research
