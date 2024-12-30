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

#ifndef OR_TOOLS_SAT_PYTHON_LINEAR_EXPR_H_
#define OR_TOOLS_SAT_PYTHON_LINEAR_EXPR_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/container/fixed_array.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research {
namespace sat {
namespace python {

class BoundedLinearExpression;
class CanonicalFloatExpression;
class FloatExprVisitor;
class LinearExpr;
class IntExprVisitor;
class LinearExpr;
class BaseIntVar;
class NotBooleanVariable;

// A class to hold an linear expression or a constant.
struct ExprOrValue {
  explicit ExprOrValue(LinearExpr* e) : expr(e) {}
  explicit ExprOrValue(double v) : double_value(v) {}
  explicit ExprOrValue(int64_t v) : int_value(v) {}

  LinearExpr* expr = nullptr;
  double double_value = 0.0;
  int64_t int_value = 0;
};

// A linear expression that can be either integer or floating point.
class LinearExpr {
 public:
  virtual ~LinearExpr() = default;
  virtual void VisitAsFloat(FloatExprVisitor* /*lin*/, double /*c*/) = 0;
  virtual bool VisitAsInt(IntExprVisitor* /*lin*/, int64_t /*c*/) = 0;
  bool IsInteger();
  virtual std::string ToString() const { return "LinearExpr"; }
  virtual std::string DebugString() const { return "LinearExpr()"; }

  static LinearExpr* Sum(const std::vector<LinearExpr*>& exprs);
  static LinearExpr* MixedSum(const std::vector<ExprOrValue>& exprs);
  static LinearExpr* WeightedSumInt(const std::vector<LinearExpr*>& exprs,
                                    const std::vector<int64_t>& coeffs);
  static LinearExpr* WeightedSumDouble(const std::vector<LinearExpr*>& exprs,
                                       const std::vector<double>& coeffs);
  static LinearExpr* MixedWeightedSumInt(const std::vector<ExprOrValue>& exprs,
                                         const std::vector<int64_t>& coeffs);
  static LinearExpr* MixedWeightedSumDouble(
      const std::vector<ExprOrValue>& exprs, const std::vector<double>& coeffs);
  static LinearExpr* TermInt(LinearExpr* expr, int64_t coeff);
  static LinearExpr* TermDouble(LinearExpr* expr, double coeff);
  static LinearExpr* AffineInt(LinearExpr* expr, int64_t coeff, int64_t offset);
  static LinearExpr* AffineDouble(LinearExpr* expr, double coeff,
                                  double offset);
  static LinearExpr* ConstantInt(int64_t value);
  static LinearExpr* ConstantDouble(double value);

  LinearExpr* Add(LinearExpr* expr);
  LinearExpr* AddInt(int64_t cst);
  LinearExpr* AddDouble(double cst);
  LinearExpr* Sub(LinearExpr* expr);
  LinearExpr* SubInt(int64_t cst);
  LinearExpr* SubDouble(double cst);
  LinearExpr* RSubInt(int64_t cst);
  LinearExpr* RSubDouble(double cst);
  LinearExpr* MulInt(int64_t cst);
  LinearExpr* MulDouble(double cst);
  LinearExpr* Neg();

  BoundedLinearExpression* Eq(LinearExpr* rhs);
  BoundedLinearExpression* EqCst(int64_t rhs);
  BoundedLinearExpression* Ne(LinearExpr* rhs);
  BoundedLinearExpression* NeCst(int64_t rhs);
  BoundedLinearExpression* Ge(LinearExpr* rhs);
  BoundedLinearExpression* GeCst(int64_t rhs);
  BoundedLinearExpression* Le(LinearExpr* rhs);
  BoundedLinearExpression* LeCst(int64_t rhs);
  BoundedLinearExpression* Lt(LinearExpr* rhs);
  BoundedLinearExpression* LtCst(int64_t rhs);
  BoundedLinearExpression* Gt(LinearExpr* rhs);
  BoundedLinearExpression* GtCst(int64_t rhs);
};

// Compare the indices of variables.
struct BaseIntVarComparator {
  bool operator()(const BaseIntVar* lhs, const BaseIntVar* rhs) const;
};

// A visitor class to process a floating point linear expression.
class FloatExprVisitor {
 public:
  void AddToProcess(LinearExpr* expr, double coeff);
  void AddConstant(double constant);
  void AddVarCoeff(BaseIntVar* var, double coeff);
  double Process(LinearExpr* expr, std::vector<BaseIntVar*>* vars,
                 std::vector<double>* coeffs);

 private:
  std::vector<std::pair<LinearExpr*, double>> to_process_;
  absl::btree_map<BaseIntVar*, double, BaseIntVarComparator> canonical_terms_;
  double offset_ = 0;
};

// A class to build a canonical floating point linear expression.
class CanonicalFloatExpression {
 public:
  explicit CanonicalFloatExpression(LinearExpr* expr);
  const std::vector<BaseIntVar*>& vars() const { return vars_; }
  const std::vector<double>& coeffs() const { return coeffs_; }
  double offset() const { return offset_; }

 private:
  std::vector<BaseIntVar*> vars_;
  std::vector<double> coeffs_;
  double offset_;
};

// A visitor class to process an integer linear expression.
class IntExprVisitor {
 public:
  void AddToProcess(LinearExpr* expr, int64_t coeff);
  void AddConstant(int64_t constant);
  void AddVarCoeff(BaseIntVar* var, int64_t coeff);
  bool ProcessAll();
  bool Process(std::vector<BaseIntVar*>* vars, std::vector<int64_t>* coeffs,
               int64_t* offset);
  bool Evaluate(LinearExpr* expr, const CpSolverResponse& solution,
                int64_t* value);

 private:
  std::vector<std::pair<LinearExpr*, int64_t>> to_process_;
  absl::btree_map<BaseIntVar*, int64_t, BaseIntVarComparator> canonical_terms_;
  int64_t offset_ = 0;
};

// A class to build a canonical integer linear expression.
class CanonicalIntExpression {
 public:
  explicit CanonicalIntExpression(LinearExpr* expr);
  const std::vector<BaseIntVar*>& vars() const { return vars_; }
  const std::vector<int64_t>& coeffs() const { return coeffs_; }
  int64_t offset() const { return offset_; }
  bool ok() const { return ok_; }

 private:
  std::vector<BaseIntVar*> vars_;
  std::vector<int64_t> coeffs_;
  int64_t offset_;
  bool ok_;
};

// A class to hold a sum of linear expressions, and optional integer and
// double offsets.
class SumArray : public LinearExpr {
 public:
  explicit SumArray(const std::vector<LinearExpr*>& exprs,
                    int64_t int_offset = 0, double double_offset = 0.0)
      : exprs_(exprs.begin(), exprs.end()),
        int_offset_(int_offset),
        double_offset_(double_offset) {}
  ~SumArray() override = default;

  bool VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    if (double_offset_ != 0.0) return false;
    for (int i = 0; i < exprs_.size(); ++i) {
      lin->AddToProcess(exprs_[i], c);
    }
    lin->AddConstant(int_offset_ * c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin->AddToProcess(exprs_[i], c);
    }
    if (int_offset_ != 0) {
      lin->AddConstant(int_offset_ * c);
    } else if (double_offset_ != 0.0) {
      lin->AddConstant(double_offset_ * c);
    }
  }

  std::string ToString() const override {
    if (exprs_.empty()) {
      if (double_offset_ != 0.0) {
        return absl::StrCat(double_offset_);
      } else {
        return absl::StrCat(int_offset_);
      }
    }
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

  std::string DebugString() const override {
    std::string s = absl::StrCat(
        "SumArray(",
        absl::StrJoin(exprs_, ", ", [](std::string* out, LinearExpr* expr) {
          absl::StrAppend(out, expr->DebugString());
        }));
    if (int_offset_ != 0) {
      absl::StrAppend(&s, ", int_offset=", int_offset_);
    }
    if (double_offset_ != 0.0) {
      absl::StrAppend(&s, ", double_offset=", double_offset_);
    }
    absl::StrAppend(&s, ")");
    return s;
  }

 private:
  const absl::FixedArray<LinearExpr*, 2> exprs_;
  const int64_t int_offset_;
  const double double_offset_;
};

// A class to hold a weighted sum of floating point linear expressions.
class FloatWeightedSum : public LinearExpr {
 public:
  FloatWeightedSum(const std::vector<LinearExpr*>& exprs, double offset);
  FloatWeightedSum(const std::vector<LinearExpr*>& exprs,
                   const std::vector<double>& coeffs, double offset);
  ~FloatWeightedSum() override = default;

  void VisitAsFloat(FloatExprVisitor* lin, double c) override;
  std::string ToString() const override;
  std::string DebugString() const override;

  bool VisitAsInt(IntExprVisitor* /*lin*/, int64_t /*c*/) override {
    return false;
  }

 private:
  const absl::FixedArray<LinearExpr*, 2> exprs_;
  const absl::FixedArray<double, 2> coeffs_;
  double offset_;
};

// A class to hold a weighted sum of integer linear expressions.
class IntWeightedSum : public LinearExpr {
 public:
  IntWeightedSum(const std::vector<LinearExpr*>& exprs,
                 const std::vector<int64_t>& coeffs, int64_t offset)
      : exprs_(exprs.begin(), exprs.end()),
        coeffs_(coeffs.begin(), coeffs.end()),
        offset_(offset) {}
  ~IntWeightedSum() override = default;

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin->AddToProcess(exprs_[i], coeffs_[i] * c);
    }
    lin->AddConstant(offset_ * c);
  }

  bool VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin->AddToProcess(exprs_[i], coeffs_[i] * c);
    }
    lin->AddConstant(offset_ * c);
    return true;
  }

  std::string ToString() const override;
  std::string DebugString() const override;

 private:
  const absl::FixedArray<LinearExpr*, 2> exprs_;
  const absl::FixedArray<int64_t, 2> coeffs_;
  int64_t offset_;
};

// A class to hold float_exr * a = b.
class FloatAffine : public LinearExpr {
 public:
  FloatAffine(LinearExpr* expr, double coeff, double offset);
  ~FloatAffine() override = default;

  void VisitAsFloat(FloatExprVisitor* lin, double c) override;
  bool VisitAsInt(IntExprVisitor* /*lin*/, int64_t /*c*/) override {
    return false;
  }
  std::string ToString() const override;
  std::string DebugString() const override;

  LinearExpr* expression() const { return expr_; }
  double coefficient() const { return coeff_; }
  double offset() const { return offset_; }

 private:
  LinearExpr* expr_;
  double coeff_;
  double offset_;
};

// A class to hold int_exr * a = b.
class IntAffine : public LinearExpr {
 public:
  IntAffine(LinearExpr* expr, int64_t coeff, int64_t offset)
      : expr_(expr), coeff_(coeff), offset_(offset) {}
  ~IntAffine() override = default;

  bool VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    lin->AddToProcess(expr_, c * coeff_);
    lin->AddConstant(offset_ * c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    lin->AddToProcess(expr_, c * coeff_);
    lin->AddConstant(offset_ * c);
  }

  std::string ToString() const override {
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

  std::string DebugString() const override {
    return absl::StrCat("IntAffine(expr=", expr_->DebugString(),
                        ", coeff=", coeff_, ", offset=", offset_, ")");
  }

  LinearExpr* expression() const { return expr_; }
  int64_t coefficient() const { return coeff_; }
  int64_t offset() const { return offset_; }

 private:
  LinearExpr* expr_;
  int64_t coeff_;
  int64_t offset_;
};

// A class to hold a constant.
class FloatConstant : public LinearExpr {
 public:
  explicit FloatConstant(double value) : value_(value) {}
  ~FloatConstant() override = default;

  void VisitAsFloat(FloatExprVisitor* lin, double c) override;
  bool VisitAsInt(IntExprVisitor* /*lin*/, int64_t /*c*/) override {
    return false;
  }
  std::string ToString() const override;
  std::string DebugString() const override;

 private:
  double value_;
};

// A class to hold a constant.
class IntConstant : public LinearExpr {
 public:
  explicit IntConstant(int64_t value) : value_(value) {}
  ~IntConstant() override = default;
  bool VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    lin->AddConstant(value_ * c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    lin->AddConstant(value_ * c);
  }

  std::string ToString() const override { return absl::StrCat(value_); }

  std::string DebugString() const override {
    return absl::StrCat("IntConstant(", value_, ")");
  }

 private:
  int64_t value_;
};

// A Boolean literal (a Boolean variable or its negation).
class Literal : public LinearExpr {
 public:
  ~Literal() override = default;
  virtual int index() const = 0;
  virtual Literal* negated() const = 0;
};

// A class to hold a variable index.
class BaseIntVar : public Literal {
 public:
  explicit BaseIntVar(int index) : index_(index), negated_(nullptr) {
    DCHECK_GE(index, 0);
  }
  BaseIntVar(int index, bool is_boolean);

  ~BaseIntVar() override {
    if (negated_ != nullptr) delete negated_;
  }

  int index() const override { return index_; }

  bool VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    lin->AddVarCoeff(this, c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    lin->AddVarCoeff(this, c);
  }

  std::string ToString() const override {
    if (negated_ != nullptr) {
      return absl::StrCat("BooleanBaseIntVar(", index_, ")");
    } else {
      return absl::StrCat("BaseIntVar(", index_, ")");
    }
  }

  std::string DebugString() const override {
    return absl::StrCat("BaseIntVar(index=", index_,
                        ", is_boolean=", negated_ != nullptr, ")");
  }

  Literal* negated() const override { return negated_; }

  bool is_boolean() const { return negated_ != nullptr; }

  bool operator<(const BaseIntVar& other) const {
    return index_ < other.index_;
  }

 protected:
  const int index_;
  Literal* const negated_;
};

template <typename H>
H AbslHashValue(H h, const BaseIntVar* i) {
  return H::combine(std::move(h), i->index());
}

// A class to hold a negated variable index.
class NotBooleanVariable : public Literal {
 public:
  explicit NotBooleanVariable(BaseIntVar* var) : var_(var) {}
  ~NotBooleanVariable() override = default;

  int index() const override { return -var_->index() - 1; }

  bool VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    lin->AddVarCoeff(var_, -c);
    lin->AddConstant(c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    lin->AddVarCoeff(var_, -c);
    lin->AddConstant(c);
  }

  std::string ToString() const override {
    return absl::StrCat("not(", var_->ToString(), ")");
  }

  Literal* negated() const override { return var_; }

  std::string DebugString() const override {
    return absl::StrCat("NotBooleanVariable(index=", var_->index(), ")");
  }

 private:
  BaseIntVar* var_;
};

// A class to hold a linear expression with bounds.
class BoundedLinearExpression {
 public:
  BoundedLinearExpression(std::vector<BaseIntVar*> vars,
                          std::vector<int64_t> coeffs, int64_t offset,
                          const Domain& bounds);

  ~BoundedLinearExpression() = default;

  const Domain& bounds() const;
  const std::vector<BaseIntVar*>& vars() const;
  const std::vector<int64_t>& coeffs() const;
  int64_t offset() const;
  std::string ToString() const;
  std::string DebugString() const;
  bool CastToBool(bool* result) const;

 private:
  std::vector<BaseIntVar*> vars_;
  std::vector<int64_t> coeffs_;
  int64_t offset_;
  const Domain bounds_;
};

}  // namespace python
}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PYTHON_LINEAR_EXPR_H_
