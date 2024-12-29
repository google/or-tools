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

bool LinearExpr::IsInteger() {
  IntExprVisitor lin;
  lin.AddToProcess(this, 1);
  return lin.ProcessAll();
}

LinearExpr* LinearExpr::Sum(const std::vector<LinearExpr*>& exprs) {
  if (exprs.empty()) {
    return new IntConstant(0);
  } else if (exprs.size() == 1) {
    return exprs[0];
  } else {
    return new IntSum(exprs, 0);
  }
}

LinearExpr* LinearExpr::Sum(const std::vector<ExprOrValue>& exprs) {
  std::vector<LinearExpr*> lin_exprs;
  int64_t int_offset = 0;
  double double_offset = 0.0;

  for (const ExprOrValue& choice : exprs) {
    if (choice.expr != nullptr) {
      lin_exprs.push_back(choice.expr);
    } else {
      int_offset += choice.int_value;
      double_offset += choice.double_value;
    }
  }

  // Special case: if there is only one term, return it.
  if (int_offset == 0 && double_offset == 0.0 && lin_exprs.size() == 1) {
    return lin_exprs[0];
  }

  // Special case: if there is no double offset, return an integer expression.
  if (double_offset == 0.0) {
    if (lin_exprs.empty()) {
      return new IntConstant(int_offset);
    } else if (lin_exprs.size() == 1) {
      return new IntAffine(lin_exprs[0], 1, int_offset);
    } else {
      return new IntSum(lin_exprs, int_offset);
    }
  } else {  // General floating point case.
    double_offset += static_cast<double>(int_offset);
    if (lin_exprs.empty()) {
      return new FloatConstant(double_offset);
    } else if (lin_exprs.size() == 1) {
      return new FloatAffine(lin_exprs[0], 1.0, double_offset);
    } else {
      return new FloatSum(lin_exprs, double_offset);
    }
  }
}

LinearExpr* LinearExpr::WeightedSum(const std::vector<LinearExpr*>& exprs,
                                    const std::vector<double>& coeffs) {
  if (exprs.empty()) return new FloatConstant(0.0);
  if (exprs.size() == 1) {
    return new FloatAffine(exprs[0], coeffs[0], 0.0);
  }
  return new FloatWeightedSum(exprs, coeffs, 0.0);
}

LinearExpr* LinearExpr::WeightedSum(const std::vector<LinearExpr*>& exprs,
                                    const std::vector<int64_t>& coeffs) {
  if (exprs.empty()) return new IntConstant(0);
  if (exprs.size() == 1) {
    return new IntAffine(exprs[0], coeffs[0], 0);
  }
  return new IntWeightedSum(exprs, coeffs, 0);
}

LinearExpr* LinearExpr::WeightedSum(const std::vector<ExprOrValue>& exprs,
                                    const std::vector<double>& coeffs) {
  std::vector<LinearExpr*> lin_exprs;
  std::vector<double> lin_coeffs;
  double cst = 0.0;
  for (int i = 0; i < exprs.size(); ++i) {
    if (exprs[i].expr != nullptr) {
      lin_exprs.push_back(exprs[i].expr);
      lin_coeffs.push_back(coeffs[i]);
    } else {
      cst += coeffs[i] *
             (exprs[i].double_value + static_cast<double>(exprs[i].int_value));
    }
  }

  if (lin_exprs.empty()) return new FloatConstant(cst);
  if (lin_exprs.size() == 1) {
    return new FloatAffine(lin_exprs[0], lin_coeffs[0], cst);
  }
  return new FloatWeightedSum(lin_exprs, lin_coeffs, cst);
}

LinearExpr* LinearExpr::WeightedSum(const std::vector<ExprOrValue>& exprs,
                                    const std::vector<int64_t>& coeffs) {
  std::vector<LinearExpr*> lin_exprs;
  std::vector<int64_t> lin_coeffs;
  int64_t int_cst = 0;
  double double_cst = 0.0;
  for (int i = 0; i < exprs.size(); ++i) {
    if (exprs[i].expr != nullptr) {
      lin_exprs.push_back(exprs[i].expr);
      lin_coeffs.push_back(coeffs[i]);
    } else {
      int_cst += coeffs[i] * exprs[i].int_value;
      double_cst += coeffs[i] * exprs[i].double_value;
    }
  }

  if (double_cst != 0.0) {
    double_cst += static_cast<double>(int_cst);
    if (lin_exprs.empty()) return new FloatConstant(double_cst);
    if (lin_exprs.size() == 1) {
      return new FloatAffine(lin_exprs[0], static_cast<double>(lin_coeffs[0]),
                             double_cst);
    }
    std::vector<double> lin_coeffs_double;
    lin_coeffs_double.reserve(lin_coeffs.size());
    for (int64_t coeff : lin_coeffs) {
      lin_coeffs_double.push_back(static_cast<double>(coeff));
    }
    return new FloatWeightedSum(lin_exprs, lin_coeffs_double, double_cst);
  }

  if (lin_exprs.empty()) return new IntConstant(int_cst);
  if (lin_exprs.size() == 1) {
    return new IntAffine(lin_exprs[0], lin_coeffs[0], int_cst);
  }
  return new IntWeightedSum(lin_exprs, lin_coeffs, int_cst);
}

LinearExpr* LinearExpr::Term(LinearExpr* expr, double coeff) {
  return new FloatAffine(expr, coeff, 0.0);
}

LinearExpr* LinearExpr::Term(LinearExpr* expr, int64_t coeff) {
  return new IntAffine(expr, coeff, 0);
}

LinearExpr* LinearExpr::Affine(LinearExpr* expr, double coeff, double offset) {
  return new FloatAffine(expr, coeff, offset);
}

LinearExpr* LinearExpr::Affine(LinearExpr* expr, int64_t coeff,
                               int64_t offset) {
  return new IntAffine(expr, coeff, offset);
}

LinearExpr* LinearExpr::Constant(double value) {
  return new FloatConstant(value);
}

LinearExpr* LinearExpr::Constant(int64_t value) {
  return new IntConstant(value);
}

LinearExpr* LinearExpr::Add(LinearExpr* other) {
  return new BinaryAdd(this, other);
}

LinearExpr* LinearExpr::AddInt(int64_t cst) {
  return new IntAffine(this, 1, cst);
}

LinearExpr* LinearExpr::AddDouble(double cst) {
  return new FloatAffine(this, 1.0, cst);
}

LinearExpr* LinearExpr::Sub(ExprOrValue other) {
  if (other.expr != nullptr) {
    return new IntWeightedSum({this, other.expr}, {1, -1}, 0);
  } else if (other.double_value != 0.0) {
    return new FloatAffine(this, 1.0, -other.double_value);
  } else if (other.int_value != 0) {
    return new IntAffine(this, 1, -other.int_value);
  } else {
    return this;
  }
}

LinearExpr* LinearExpr::RSub(ExprOrValue other) {
  if (other.expr != nullptr) {
    return new IntWeightedSum({this, other.expr}, {-1, 1}, 0);
  } else if (other.double_value != 0.0) {
    return new FloatAffine(this, -1.0, other.double_value);
  } else {
    return new IntAffine(this, -1, other.int_value);
  }
}

LinearExpr* LinearExpr::Mul(double cst) {
  if (cst == 0.0) return new IntConstant(0);
  if (cst == 1.0) return this;
  return new FloatAffine(this, cst, 0.0);
}

LinearExpr* LinearExpr::Mul(int64_t cst) {
  if (cst == 0) return new IntConstant(0);
  if (cst == 1) return this;
  return new IntAffine(this, cst, 0);
}

LinearExpr* LinearExpr::Neg() { return new IntAffine(this, -1, 0); }

void FloatExprVisitor::AddToProcess(LinearExpr* expr, double coeff) {
  to_process_.push_back(std::make_pair(expr, coeff));
}
void FloatExprVisitor::AddConstant(double constant) { offset_ += constant; }
void FloatExprVisitor::AddVarCoeff(BaseIntVar* var, double coeff) {
  canonical_terms_[var] += coeff;
}
double FloatExprVisitor::Process(LinearExpr* expr,
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

CanonicalFloatExpression::CanonicalFloatExpression(LinearExpr* expr) {
  FloatExprVisitor lin;
  offset_ = lin.Process(expr, &vars_, &coeffs_);
}

CanonicalIntExpression::CanonicalIntExpression(LinearExpr* expr) : ok_(true) {
  IntExprVisitor lin;
  lin.AddToProcess(expr, 1);
  ok_ = lin.Process(&vars_, &coeffs_, &offset_);
}

void FloatConstant::VisitAsFloat(FloatExprVisitor* lin, double c) {
  lin->AddConstant(value_ * c);
}

std::string FloatConstant::ToString() const { return absl::StrCat(value_); }

std::string FloatConstant::DebugString() const {
  return absl::StrCat("FloatConstant(", value_, ")");
}

FloatWeightedSum::FloatWeightedSum(const std::vector<LinearExpr*>& exprs,
                                   double offset)
    : exprs_(exprs.begin(), exprs.end()),
      coeffs_(exprs.size(), 1),
      offset_(offset) {}

FloatWeightedSum::FloatWeightedSum(const std::vector<LinearExpr*>& exprs,
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

std::string FloatWeightedSum::DebugString() const {
  return absl::StrCat("FloatWeightedSum([",
                      absl::StrJoin(exprs_, ", ",
                                    [](std::string* out, const LinearExpr* e) {
                                      absl::StrAppend(out, e->DebugString());
                                    }),
                      "], [", absl::StrJoin(coeffs_, "], "), offset_, ")");
}

std::string IntWeightedSum::ToString() const {
  if (exprs_.empty()) {
    return absl::StrCat(offset_);
  }
  std::string s = "(";
  bool first_printed = true;
  for (int i = 0; i < exprs_.size(); ++i) {
    if (coeffs_[i] == 0) continue;
    if (first_printed) {
      first_printed = false;
      if (coeffs_[i] == 1) {
        absl::StrAppend(&s, exprs_[i]->ToString());
      } else if (coeffs_[i] == -1) {
        absl::StrAppend(&s, "-", exprs_[i]->ToString());
      } else {
        absl::StrAppend(&s, coeffs_[i], " * ", exprs_[i]->ToString());
      }
    } else {
      if (coeffs_[i] == 1) {
        absl::StrAppend(&s, " + ", exprs_[i]->ToString());
      } else if (coeffs_[i] == -1) {
        absl::StrAppend(&s, " - ", exprs_[i]->ToString());
      } else if (coeffs_[i] > 1) {
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
  if (offset_ != 0) {
    if (offset_ > 0) {
      absl::StrAppend(&s, " + ", offset_);
    } else {
      absl::StrAppend(&s, " - ", -offset_);
    }
  }
  absl::StrAppend(&s, ")");
  return s;
}

std::string IntWeightedSum::DebugString() const {
  return absl::StrCat("IntWeightedSum([",
                      absl::StrJoin(exprs_, ", ",
                                    [](std::string* out, LinearExpr* expr) {
                                      absl::StrAppend(out, expr->DebugString());
                                    }),
                      "], [", absl::StrJoin(coeffs_, ", "), "], ", offset_,
                      ")");
}

FloatAffine::FloatAffine(LinearExpr* expr, double coeff, double offset)
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

BoundedLinearExpression* LinearExpr::Eq(ExprOrValue other) {
  if (other.double_value != 0.0) return nullptr;
  if (other.expr != nullptr) {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    lin.AddToProcess(other.expr, -1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(vars, coeffs, offset, Domain(0));
  } else {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(vars, coeffs, offset,
                                       Domain(other.int_value));
  }
}

BoundedLinearExpression* LinearExpr::Ne(ExprOrValue other) {
  if (other.double_value != 0.0) return nullptr;
  if (other.expr != nullptr) {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    lin.AddToProcess(other.expr, -1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(vars, coeffs, offset,
                                       Domain(0).Complement());
  } else {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(vars, coeffs, offset,
                                       Domain(other.int_value).Complement());
  }
}

BoundedLinearExpression* LinearExpr::Le(ExprOrValue other) {
  if (other.double_value != 0.0) return nullptr;
  if (other.expr != nullptr) {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    lin.AddToProcess(other.expr, -1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(
        vars, coeffs, offset, Domain(std::numeric_limits<int64_t>::min(), 0));
  } else {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(
        vars, coeffs, offset,
        Domain(std::numeric_limits<int64_t>::min(), other.int_value));
  }
}

BoundedLinearExpression* LinearExpr::Lt(ExprOrValue other) {
  if (other.double_value != 0.0) return nullptr;
  if (other.expr != nullptr) {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    lin.AddToProcess(other.expr, -1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(
        vars, coeffs, offset, Domain(std::numeric_limits<int64_t>::min(), -1));
  } else {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(
        vars, coeffs, offset,
        Domain(std::numeric_limits<int64_t>::min(), other.int_value - 1));
  }
}

BoundedLinearExpression* LinearExpr::Ge(ExprOrValue other) {
  if (other.double_value != 0.0) return nullptr;
  if (other.expr != nullptr) {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    lin.AddToProcess(other.expr, -1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(
        vars, coeffs, offset, Domain(0, std::numeric_limits<int64_t>::max()));
  } else {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(
        vars, coeffs, offset,
        Domain(other.int_value, std::numeric_limits<int64_t>::max()));
  }
}

BoundedLinearExpression* LinearExpr::Gt(ExprOrValue other) {
  if (other.double_value != 0.0) return nullptr;
  if (other.expr != nullptr) {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    lin.AddToProcess(other.expr, -1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(
        vars, coeffs, offset, Domain(1, std::numeric_limits<int64_t>::max()));
  } else {
    IntExprVisitor lin;
    lin.AddToProcess(this, 1);
    std::vector<BaseIntVar*> vars;
    std::vector<int64_t> coeffs;
    int64_t offset;
    if (!lin.Process(&vars, &coeffs, &offset)) return nullptr;
    return new BoundedLinearExpression(
        vars, coeffs, offset,
        Domain(other.int_value + 1, std::numeric_limits<int64_t>::max()));
  }
}

void IntExprVisitor::AddToProcess(LinearExpr* expr, int64_t coeff) {
  to_process_.push_back(std::make_pair(expr, coeff));
}

void IntExprVisitor::AddConstant(int64_t constant) { offset_ += constant; }

void IntExprVisitor::AddVarCoeff(BaseIntVar* var, int64_t coeff) {
  canonical_terms_[var] += coeff;
}

bool IntExprVisitor::ProcessAll() {
  while (!to_process_.empty()) {
    const auto [expr, coeff] = to_process_.back();
    to_process_.pop_back();
    if (!expr->VisitAsInt(this, coeff)) return false;
  }
  return true;
}

bool IntExprVisitor::Process(std::vector<BaseIntVar*>* vars,
                             std::vector<int64_t>* coeffs, int64_t* offset) {
  if (!ProcessAll()) return false;
  vars->clear();
  coeffs->clear();
  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0) continue;
    vars->push_back(var);
    coeffs->push_back(coeff);
  }
  *offset = offset_;
  return true;
}

bool IntExprVisitor::Evaluate(LinearExpr* expr,
                              const CpSolverResponse& solution,
                              int64_t* value) {
  AddToProcess(expr, 1);
  if (!ProcessAll()) return false;

  *value = offset_;
  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0) continue;
    *value += coeff * solution.solution(var->index());
  }
  return true;
}

bool BaseIntVarComparator::operator()(const BaseIntVar* lhs,
                                      const BaseIntVar* rhs) const {
  return lhs->index() < rhs->index();
}

BaseIntVar::BaseIntVar(int index, bool is_boolean)
    : index_(index),
      negated_(is_boolean ? new NotBooleanVariable(this) : nullptr) {}

BoundedLinearExpression::BoundedLinearExpression(std::vector<BaseIntVar*> vars,
                                                 std::vector<int64_t> coeffs,
                                                 int64_t offset,
                                                 const Domain& bounds)
    : vars_(vars), coeffs_(coeffs), offset_(offset), bounds_(bounds) {}

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
