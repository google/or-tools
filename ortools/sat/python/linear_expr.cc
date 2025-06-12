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

#include "ortools/sat/python/linear_expr.h"

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace python {

bool LinearExpr::IsInteger() {
  IntExprVisitor lin;
  lin.AddToProcess(shared_from_this(), 1);
  return lin.ProcessAll();
}

std::shared_ptr<LinearExpr> LinearExpr::TermInt(
    std::shared_ptr<LinearExpr> expr, int64_t coeff) {
  return std::make_shared<IntAffine>(expr, coeff, 0);
}

std::shared_ptr<LinearExpr> LinearExpr::TermFloat(
    std::shared_ptr<LinearExpr> expr, double coeff) {
  return std::make_shared<FloatAffine>(expr, coeff, 0.0);
}

std::shared_ptr<LinearExpr> LinearExpr::AffineInt(
    std::shared_ptr<LinearExpr> expr, int64_t coeff, int64_t offset) {
  if (coeff == 1 && offset == 0) return expr;
  return std::make_shared<IntAffine>(expr, coeff, offset);
}

std::shared_ptr<LinearExpr> LinearExpr::AffineFloat(
    std::shared_ptr<LinearExpr> expr, double coeff, double offset) {
  if (coeff == 1.0 && offset == 0.0) return expr;
  return std::make_shared<FloatAffine>(expr, coeff, offset);
}

std::shared_ptr<LinearExpr> LinearExpr::ConstantInt(int64_t value) {
  return std::make_shared<IntConstant>(value);
}

std::shared_ptr<LinearExpr> LinearExpr::ConstantFloat(double value) {
  return std::make_shared<FloatConstant>(value);
}

std::shared_ptr<LinearExpr> LinearExpr::Add(std::shared_ptr<LinearExpr> other) {
  std::vector<std::shared_ptr<LinearExpr>> exprs;
  exprs.push_back(shared_from_this());
  exprs.push_back(other);
  return std::make_shared<SumArray>(exprs);
}

std::shared_ptr<LinearExpr> LinearExpr::AddInt(int64_t cst) {
  if (cst == 0) return shared_from_this();
  return std::make_shared<IntAffine>(shared_from_this(), 1, cst);
}

std::shared_ptr<LinearExpr> LinearExpr::AddFloat(double cst) {
  if (cst == 0.0) return shared_from_this();
  return std::make_shared<FloatAffine>(shared_from_this(), 1.0, cst);
}

std::shared_ptr<LinearExpr> LinearExpr::Sub(std::shared_ptr<LinearExpr> other) {
  std::vector<std::shared_ptr<LinearExpr>> exprs;
  exprs.push_back(shared_from_this());
  exprs.push_back(other->MulInt(-1));
  return std::make_shared<SumArray>(exprs);
}

std::shared_ptr<LinearExpr> LinearExpr::SubInt(int64_t cst) {
  if (cst == 0) return shared_from_this();
  return std::make_shared<IntAffine>(shared_from_this(), 1, -cst);
}

std::shared_ptr<LinearExpr> LinearExpr::SubFloat(double cst) {
  if (cst == 0.0) return shared_from_this();
  return std::make_shared<FloatAffine>(shared_from_this(), 1.0, -cst);
}

std::shared_ptr<LinearExpr> LinearExpr::RSub(
    std::shared_ptr<LinearExpr> other) {
  std::vector<std::shared_ptr<LinearExpr>> exprs;
  exprs.push_back(shared_from_this());
  exprs.push_back(other);
  const std::vector<int64_t> coeffs = {-1, 1};
  return std::make_shared<IntWeightedSum>(exprs, coeffs, 0);
}

std::shared_ptr<LinearExpr> LinearExpr::RSubInt(int64_t cst) {
  return std::make_shared<IntAffine>(shared_from_this(), -1, cst);
}

std::shared_ptr<LinearExpr> LinearExpr::RSubFloat(double cst) {
  return std::make_shared<FloatAffine>(shared_from_this(), -1.0, cst);
}

std::shared_ptr<LinearExpr> LinearExpr::MulInt(int64_t cst) {
  if (cst == 0) return std::make_shared<IntConstant>(0);
  if (cst == 1) return shared_from_this();
  return std::make_shared<IntAffine>(shared_from_this(), cst, 0);
}

std::shared_ptr<LinearExpr> LinearExpr::MulFloat(double cst) {
  if (cst == 0.0) return std::make_shared<IntConstant>(0);
  if (cst == 1.0) return shared_from_this();
  return std::make_shared<FloatAffine>(shared_from_this(), cst, 0.0);
}

std::shared_ptr<LinearExpr> LinearExpr::Neg() {
  return std::make_shared<IntAffine>(shared_from_this(), -1, 0);
}

void FloatExprVisitor::AddToProcess(std::shared_ptr<LinearExpr> expr,
                                    double coeff) {
  to_process_.push_back(std::make_pair(expr, coeff));
}

void FloatExprVisitor::AddConstant(double constant) { offset_ += constant; }

void FloatExprVisitor::AddVarCoeff(std::shared_ptr<BaseIntVar> var,
                                   double coeff) {
  canonical_terms_[var] += coeff;
}

void FloatExprVisitor::ProcessAll() {
  while (!to_process_.empty()) {
    const auto [expr, coeff] = to_process_.back();
    to_process_.pop_back();
    expr->VisitAsFloat(*this, coeff);
  }
}

double FloatExprVisitor::Process(std::vector<std::shared_ptr<BaseIntVar>>* vars,
                                 std::vector<double>* coeffs) {
  ProcessAll();

  vars->clear();
  coeffs->clear();
  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0.0) continue;
    vars->push_back(var);
    coeffs->push_back(coeff);
  }

  return offset_;
}

double FloatExprVisitor::Evaluate(const CpSolverResponse& solution) {
  ProcessAll();

  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0) continue;
    offset_ += coeff * solution.solution(var->index());
  }
  return offset_;
}

FlatFloatExpr::FlatFloatExpr(std::shared_ptr<LinearExpr> expr) {
  FloatExprVisitor lin;
  lin.AddToProcess(expr, 1.0);
  offset_ = lin.Process(&vars_, &coeffs_);
}

void FlatFloatExpr::VisitAsFloat(FloatExprVisitor& lin, double c) {
  for (int i = 0; i < vars_.size(); ++i) {
    lin.AddVarCoeff(vars_[i], coeffs_[i] * c);
  }
  lin.AddConstant(offset_ * c);
}

std::string FlatFloatExpr::ToString() const {
  if (vars_.empty()) {
    return absl::StrCat(RoundTripDoubleFormat(offset_));
  }

  std::string s = "(";
  bool first_printed = true;
  for (int i = 0; i < vars_.size(); ++i) {
    if (coeffs_[i] == 0.0) continue;
    if (first_printed) {
      first_printed = false;
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, vars_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, "-", vars_[i]->ToString());
      } else {
        absl::StrAppend(&s, RoundTripDoubleFormat(coeffs_[i]), " * ",
                        vars_[i]->ToString());
      }
    } else {
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, " + ", vars_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, " - ", vars_[i]->ToString());
      } else if (coeffs_[i] > 0.0) {
        absl::StrAppend(&s, " + ", RoundTripDoubleFormat(coeffs_[i]), " * ",
                        vars_[i]->ToString());
      } else {
        absl::StrAppend(&s, " - ", RoundTripDoubleFormat(-coeffs_[i]), " * ",
                        vars_[i]->ToString());
      }
    }
  }
  // If there are no terms, just print the offset.
  if (first_printed) {
    return absl::StrCat(RoundTripDoubleFormat(offset_));
  }

  // If there is an offset, print it.
  if (offset_ != 0.0) {
    if (offset_ > 0.0) {
      absl::StrAppend(&s, " + ", RoundTripDoubleFormat(offset_));
    } else {
      absl::StrAppend(&s, " - ", RoundTripDoubleFormat(-offset_));
    }
  }
  absl::StrAppend(&s, ")");
  return s;
}

std::string FlatFloatExpr::DebugString() const {
  return absl::StrCat(
      "FlatFloatExpr([",
      absl::StrJoin(vars_, ", ",
                    [](std::string* out, std::shared_ptr<LinearExpr> e) {
                      absl::StrAppend(out, e->DebugString());
                    }),
      "], [",
      absl::StrJoin(coeffs_, ", ",
                    [](std::string* out, double coeff) {
                      absl::StrAppend(out, RoundTripDoubleFormat(coeff));
                    }),
      "], ", RoundTripDoubleFormat(offset_), ")");
}

FlatIntExpr::FlatIntExpr(std::shared_ptr<LinearExpr> expr) {
  IntExprVisitor lin;
  lin.AddToProcess(expr, 1);
  ok_ = lin.Process(&vars_, &coeffs_, &offset_);
}

std::string FlatIntExpr::ToString() const {
  if (vars_.empty()) {
    return absl::StrCat(offset_);
  }

  std::string s = "(";
  bool first_printed = true;
  for (int i = 0; i < vars_.size(); ++i) {
    if (coeffs_[i] == 0) continue;
    if (first_printed) {
      first_printed = false;
      if (coeffs_[i] == 1) {
        absl::StrAppend(&s, vars_[i]->ToString());
      } else if (coeffs_[i] == -1) {
        absl::StrAppend(&s, "-", vars_[i]->ToString());
      } else {
        absl::StrAppend(&s, coeffs_[i], " * ", vars_[i]->ToString());
      }
    } else {
      if (coeffs_[i] == 1) {
        absl::StrAppend(&s, " + ", vars_[i]->ToString());
      } else if (coeffs_[i] == -1) {
        absl::StrAppend(&s, " - ", vars_[i]->ToString());
      } else if (coeffs_[i] > 1) {
        absl::StrAppend(&s, " + ", coeffs_[i], " * ", vars_[i]->ToString());
      } else {
        absl::StrAppend(&s, " - ", -coeffs_[i], " * ", vars_[i]->ToString());
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

std::string FlatIntExpr::DebugString() const {
  return absl::StrCat(
      "FlatIntExpr([",
      absl::StrJoin(vars_, ", ",
                    [](std::string* out, std::shared_ptr<BaseIntVar> var) {
                      absl::StrAppend(out, var->DebugString());
                    }),
      "], [", absl::StrJoin(coeffs_, ", "), "], ", offset_, ")");
}

void FloatConstant::VisitAsFloat(FloatExprVisitor& lin, double c) {
  lin.AddConstant(value_ * c);
}

std::string FloatConstant::ToString() const { return absl::StrCat(value_); }

std::string FloatConstant::DebugString() const {
  return absl::StrCat("FloatConstant(", RoundTripDoubleFormat(value_), ")");
}

SumArray::SumArray(std::vector<std::shared_ptr<LinearExpr>> exprs,
                   int64_t int_offset, double double_offset)
    : exprs_(std::move(exprs)),
      int_offset_(int_offset),
      double_offset_(double_offset) {
  DCHECK_GE(exprs_.size(), 2);
}

void SumArray::AddInPlace(std::shared_ptr<LinearExpr> expr) {
  exprs_.push_back(std::move(expr));
}

bool SumArray::VisitAsInt(IntExprVisitor& lin, int64_t c) {
  if (double_offset_ != 0.0) return false;
  for (int i = 0; i < exprs_.size(); ++i) {
    lin.AddToProcess(exprs_[i], c);
  }
  lin.AddConstant(int_offset_ * c);
  return true;
}

void SumArray::VisitAsFloat(FloatExprVisitor& lin, double c) {
  for (int i = 0; i < exprs_.size(); ++i) {
    lin.AddToProcess(exprs_[i], c);
  }
  if (int_offset_ != 0) {
    lin.AddConstant(int_offset_ * c);
  }
  if (double_offset_ != 0.0) {
    lin.AddConstant(double_offset_ * c);
  }
}

std::string SumArray::ToString() const {
  DCHECK(!exprs_.empty());

  std::string s = "(";
  for (int i = 0; i < exprs_.size(); ++i) {
    if (i > 0) {
      absl::StrAppend(&s, " + ");
    }
    absl::StrAppend(&s, exprs_[i]->ToString());
  }
  if (double_offset_ != 0.0) {
    if (double_offset_ > 0.0) {
      absl::StrAppend(&s, " + ", double_offset_);
    } else {
      absl::StrAppend(&s, " - ", -double_offset_);
    }
  }
  if (int_offset_ != 0) {
    if (int_offset_ > 0) {
      absl::StrAppend(&s, " + ", int_offset_);
    } else {
      absl::StrAppend(&s, " - ", -int_offset_);
    }
  }
  absl::StrAppend(&s, ")");
  return s;
}

std::string SumArray::DebugString() const {
  std::string s = absl::StrCat(
      "SumArray(",
      absl::StrJoin(exprs_, ", ",
                    [](std::string* out, std::shared_ptr<LinearExpr> expr) {
                      absl::StrAppend(out, expr->DebugString());
                    }));
  if (int_offset_ != 0) {
    absl::StrAppend(&s, ", int_offset=", int_offset_);
  }
  if (double_offset_ != 0.0) {
    absl::StrAppend(&s,
                    ", double_offset=", RoundTripDoubleFormat(double_offset_));
  }
  absl::StrAppend(&s, ")");
  return s;
}

FloatWeightedSum::FloatWeightedSum(
    absl::Span<const std::shared_ptr<LinearExpr>> exprs,
    absl::Span<const double> coeffs, double offset)
    : exprs_(exprs.begin(), exprs.end()),
      coeffs_(coeffs.begin(), coeffs.end()),
      offset_(offset) {
  DCHECK_GE(exprs_.size(), 2);
}

void FloatWeightedSum::VisitAsFloat(FloatExprVisitor& lin, double c) {
  for (int i = 0; i < exprs_.size(); ++i) {
    lin.AddToProcess(exprs_[i], coeffs_[i] * c);
  }
  lin.AddConstant(offset_ * c);
}

std::string FloatWeightedSum::ToString() const {
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
        absl::StrAppend(&s, RoundTripDoubleFormat(coeffs_[i]), " * ",
                        exprs_[i]->ToString());
      }
    } else {
      if (coeffs_[i] == 1.0) {
        absl::StrAppend(&s, " + ", exprs_[i]->ToString());
      } else if (coeffs_[i] == -1.0) {
        absl::StrAppend(&s, " - ", exprs_[i]->ToString());
      } else if (coeffs_[i] > 0.0) {
        absl::StrAppend(&s, " + ", RoundTripDoubleFormat(coeffs_[i]), " * ",
                        exprs_[i]->ToString());
      } else {
        absl::StrAppend(&s, " - ", RoundTripDoubleFormat(-coeffs_[i]), " * ",
                        exprs_[i]->ToString());
      }
    }
  }
  // If there are no terms, just print the offset.
  if (first_printed) {
    return absl::StrCat(RoundTripDoubleFormat(offset_));
  }

  // If there is an offset, print it.
  if (offset_ != 0.0) {
    if (offset_ > 0.0) {
      absl::StrAppend(&s, " + ", RoundTripDoubleFormat(offset_));
    } else {
      absl::StrAppend(&s, " - ", RoundTripDoubleFormat(-offset_));
    }
  }
  absl::StrAppend(&s, ")");
  return s;
}

std::string FloatWeightedSum::DebugString() const {
  return absl::StrCat(
      "FloatWeightedSum([",
      absl::StrJoin(exprs_, ", ",
                    [](std::string* out, std::shared_ptr<LinearExpr> e) {
                      absl::StrAppend(out, e->DebugString());
                    }),
      "], [",
      absl::StrJoin(coeffs_, ", ",
                    [](std::string* out, double coeff) {
                      absl::StrAppend(out, RoundTripDoubleFormat(coeff));
                    }),
      RoundTripDoubleFormat(offset_), ")");
}

IntWeightedSum::IntWeightedSum(
    absl::Span<const std::shared_ptr<LinearExpr>> exprs,
    absl::Span<const int64_t> coeffs, int64_t offset)
    : exprs_(exprs.begin(), exprs.end()),
      coeffs_(coeffs.begin(), coeffs.end()),
      offset_(offset) {}

void IntWeightedSum::VisitAsFloat(FloatExprVisitor& lin, double c) {
  for (int i = 0; i < exprs_.size(); ++i) {
    lin.AddToProcess(exprs_[i], coeffs_[i] * c);
  }
  lin.AddConstant(offset_ * c);
}

bool IntWeightedSum::VisitAsInt(IntExprVisitor& lin, int64_t c) {
  for (int i = 0; i < exprs_.size(); ++i) {
    lin.AddToProcess(exprs_[i], coeffs_[i] * c);
  }
  lin.AddConstant(offset_ * c);
  return true;
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
  return absl::StrCat(
      "IntWeightedSum([",
      absl::StrJoin(exprs_, ", ",
                    [](std::string* out, std::shared_ptr<LinearExpr> expr) {
                      absl::StrAppend(out, expr->DebugString());
                    }),
      "], [", absl::StrJoin(coeffs_, ", "), "], ", offset_, ")");
}

FloatAffine::FloatAffine(std::shared_ptr<LinearExpr> expr, double coeff,
                         double offset)
    : expr_(expr), coeff_(coeff), offset_(offset) {}

void FloatAffine::VisitAsFloat(FloatExprVisitor& lin, double c) {
  lin.AddToProcess(expr_, c * coeff_);
  lin.AddConstant(offset_ * c);
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

IntAffine::IntAffine(std::shared_ptr<LinearExpr> expr, int64_t coeff,
                     int64_t offset)
    : expr_(expr), coeff_(coeff), offset_(offset) {}

bool IntAffine::VisitAsInt(IntExprVisitor& lin, int64_t c) {
  lin.AddToProcess(expr_, c * coeff_);
  lin.AddConstant(offset_ * c);
  return true;
}

void IntAffine::VisitAsFloat(FloatExprVisitor& lin, double c) {
  lin.AddToProcess(expr_, c * coeff_);
  lin.AddConstant(offset_ * c);
}

std::string IntAffine::ToString() const {
  std::string s = "(";
  if (coeff_ == 1) {
    absl::StrAppend(&s, expr_->ToString());
  } else if (coeff_ == -1) {
    absl::StrAppend(&s, "-", expr_->ToString());
  } else {
    absl::StrAppend(&s, coeff_, " * ", expr_->ToString());
  }
  if (offset_ > 0) {
    absl::StrAppend(&s, " + ", offset_);
  } else if (offset_ < 0) {
    absl::StrAppend(&s, " - ", -offset_);
  }
  absl::StrAppend(&s, ")");
  return s;
}

std::string IntAffine::DebugString() const {
  return absl::StrCat("IntAffine(expr=", expr_->DebugString(),
                      ", coeff=", coeff_, ", offset=", offset_, ")");
}

std::shared_ptr<LinearExpr> IntAffine::AddInt(int64_t cst) {
  if (cst == 0) return shared_from_this();
  return LinearExpr::AffineInt(expr_, coeff_, offset_ + cst);
}

std::shared_ptr<LinearExpr> IntAffine::SubInt(int64_t cst) {
  if (cst == 0) return shared_from_this();
  return LinearExpr::AffineInt(expr_, coeff_, offset_ - cst);
}

std::shared_ptr<LinearExpr> IntAffine::RSubInt(int64_t cst) {
  return LinearExpr::AffineInt(expr_, -coeff_, cst - offset_);
}

std::shared_ptr<LinearExpr> IntAffine::MulInt(int64_t cst) {
  if (cst == 0) return std::make_shared<IntConstant>(0);
  if (cst == 1) return shared_from_this();
  return LinearExpr::AffineInt(expr_, coeff_ * cst, offset_ * cst);
}

std::shared_ptr<LinearExpr> IntAffine::Neg() {
  return LinearExpr::AffineInt(expr_, -coeff_, -offset_);
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::Eq(
    std::shared_ptr<LinearExpr> rhs) {
  return std::make_shared<BoundedLinearExpression>(shared_from_this(), rhs,
                                                   Domain(0));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::EqCst(int64_t rhs) {
  return std::make_shared<BoundedLinearExpression>(shared_from_this(),
                                                   Domain(rhs));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::Ne(
    std::shared_ptr<LinearExpr> rhs) {
  return std::make_shared<BoundedLinearExpression>(shared_from_this(), rhs,
                                                   Domain(0).Complement());
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::NeCst(int64_t rhs) {
  return std::make_shared<BoundedLinearExpression>(shared_from_this(),
                                                   Domain(rhs).Complement());
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::Le(
    std::shared_ptr<LinearExpr> rhs) {
  return std::make_shared<BoundedLinearExpression>(
      shared_from_this(), rhs, Domain(std::numeric_limits<int64_t>::min(), 0));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::LeCst(int64_t rhs) {
  return std::make_shared<BoundedLinearExpression>(
      shared_from_this(), Domain(std::numeric_limits<int64_t>::min(), rhs));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::Lt(
    std::shared_ptr<LinearExpr> rhs) {
  return std::make_shared<BoundedLinearExpression>(
      shared_from_this(), rhs, Domain(std::numeric_limits<int64_t>::min(), -1));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::LtCst(int64_t rhs) {
  return std::make_shared<BoundedLinearExpression>(
      shared_from_this(), Domain(std::numeric_limits<int64_t>::min(), rhs - 1));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::Ge(
    std::shared_ptr<LinearExpr> rhs) {
  return std::make_shared<BoundedLinearExpression>(
      shared_from_this(), rhs, Domain(0, std::numeric_limits<int64_t>::max()));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::GeCst(int64_t rhs) {
  return std::make_shared<BoundedLinearExpression>(
      shared_from_this(), Domain(rhs, std::numeric_limits<int64_t>::max()));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::Gt(
    std::shared_ptr<LinearExpr> rhs) {
  return std::make_shared<BoundedLinearExpression>(
      shared_from_this(), rhs, Domain(1, std::numeric_limits<int64_t>::max()));
}

std::shared_ptr<BoundedLinearExpression> LinearExpr::GtCst(int64_t rhs) {
  return std::make_shared<BoundedLinearExpression>(
      shared_from_this(), Domain(rhs + 1, std::numeric_limits<int64_t>::max()));
}

void IntExprVisitor::AddToProcess(std::shared_ptr<LinearExpr> expr,
                                  int64_t coeff) {
  to_process_.push_back(std::make_pair(expr, coeff));
}

void IntExprVisitor::AddConstant(int64_t constant) { offset_ += constant; }

void IntExprVisitor::AddVarCoeff(std::shared_ptr<BaseIntVar> var,
                                 int64_t coeff) {
  canonical_terms_[var] += coeff;
}

bool IntExprVisitor::ProcessAll() {
  while (!to_process_.empty()) {
    const auto [expr, coeff] = to_process_.back();
    to_process_.pop_back();
    if (!expr->VisitAsInt(*this, coeff)) return false;
  }
  return true;
}

bool IntExprVisitor::Process(std::vector<std::shared_ptr<BaseIntVar>>* vars,
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

bool IntExprVisitor::Evaluate(const CpSolverResponse& solution,
                              int64_t* value) {
  if (!ProcessAll()) return false;

  *value = offset_;
  for (const auto& [var, coeff] : canonical_terms_) {
    if (coeff == 0) continue;
    *value += coeff * solution.solution(var->index());
  }
  return true;
}

// TODO(user): This hash method does not distinguish between variables with
// the same index and different models.
int64_t Literal::Hash() const { return absl::HashOf(index()); }

bool BaseIntVarComparator::operator()(std::shared_ptr<BaseIntVar> lhs,
                                      std::shared_ptr<BaseIntVar> rhs) const {
  return lhs->index() < rhs->index();
}

BaseIntVar::BaseIntVar(int index, bool is_boolean)
    : index_(index), is_boolean_(is_boolean) {}

std::shared_ptr<Literal> BaseIntVar::negated() {
  if (negated_ == nullptr) {
    std::shared_ptr<BaseIntVar> self =
        std::static_pointer_cast<BaseIntVar>(shared_from_this());
    negated_ = std::make_shared<NotBooleanVariable>(self);
  }
  return negated_;
}

int NotBooleanVariable::index() const {
  std::shared_ptr<BaseIntVar> var = var_.lock();
  CHECK(var != nullptr);  // Cannot happen as checked in the pybind11 code.
  return -var->index() - 1;
}

/**
 * Returns the negation of the current literal, that is the original Boolean
 * variable.
 */
std::shared_ptr<Literal> NotBooleanVariable::negated() {
  std::shared_ptr<BaseIntVar> var = var_.lock();
  CHECK(var != nullptr);  // Cannot happen as checked in the pybind11 code.
  return var;
}

bool NotBooleanVariable::VisitAsInt(IntExprVisitor& lin, int64_t c) {
  std::shared_ptr<BaseIntVar> var = var_.lock();
  CHECK(var != nullptr);  // Cannot happen as checked in the pybind11 code.
  lin.AddVarCoeff(var, -c);
  lin.AddConstant(c);
  return true;
}

void NotBooleanVariable::VisitAsFloat(FloatExprVisitor& lin, double c) {
  std::shared_ptr<BaseIntVar> var = var_.lock();
  CHECK(var != nullptr);  // Cannot happen as checked in the pybind11 code.
  lin.AddVarCoeff(var, -c);
  lin.AddConstant(c);
}

std::string NotBooleanVariable::ToString() const {
  std::shared_ptr<BaseIntVar> var = var_.lock();
  CHECK(var != nullptr);  // Cannot happen as checked in the pybind11 code.
  return absl::StrCat("not(", var->ToString(), ")");
}

std::string NotBooleanVariable::DebugString() const {
  std::shared_ptr<BaseIntVar> var = var_.lock();
  CHECK(var != nullptr);  // Cannot happen as checked in the pybind11 code.
  return absl::StrCat("NotBooleanVariable(index=", var->index(), ")");
}

BoundedLinearExpression::BoundedLinearExpression(
    std::shared_ptr<LinearExpr> expr, const Domain& bounds)
    : bounds_(bounds) {
  IntExprVisitor lin;
  lin.AddToProcess(expr, 1);
  ok_ = lin.Process(&vars_, &coeffs_, &offset_);
}

BoundedLinearExpression::BoundedLinearExpression(
    std::shared_ptr<LinearExpr> pos, std::shared_ptr<LinearExpr> neg,
    const Domain& bounds)
    : bounds_(bounds) {
  IntExprVisitor lin;
  lin.AddToProcess(pos, 1);
  lin.AddToProcess(neg, -1);
  ok_ = lin.Process(&vars_, &coeffs_, &offset_);
}

const Domain& BoundedLinearExpression::bounds() const { return bounds_; }
const std::vector<std::shared_ptr<BaseIntVar>>& BoundedLinearExpression::vars()
    const {
  return vars_;
}
const std::vector<int64_t>& BoundedLinearExpression::coeffs() const {
  return coeffs_;
}
int64_t BoundedLinearExpression::offset() const { return offset_; }

bool BoundedLinearExpression::ok() const { return ok_; }

std::string BoundedLinearExpression::ToString() const {
  if (!ok_) return "Invalid BoundedLinearExpression";
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
  if (!ok_) return "Invalid BoundedLinearExpression";
  return absl::StrCat(
      "BoundedLinearExpression(vars=[",
      absl::StrJoin(vars_, ", ",
                    [](std::string* out, std::shared_ptr<BaseIntVar> var) {
                      absl::StrAppend(out, var->DebugString());
                    }),
      "], coeffs=[", absl::StrJoin(coeffs_, ", "), "], offset=", offset_,
      ", bounds=", bounds_.ToString(), ")");
}

bool BoundedLinearExpression::CastToBool(bool* result) const {
  if (!ok_) return false;
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
