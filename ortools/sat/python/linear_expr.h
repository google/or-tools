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
class FloatLinearExpr;
class IntExprVisitor;
class IntLinExpr;
class BaseIntVar;
class NotBooleanVariable;

// A class to hold an floating point linear expression or a double constant.
struct FloatExprOrValue {
  explicit FloatExprOrValue(FloatLinearExpr* e) : expr(e) {}
  explicit FloatExprOrValue(double v) : value(v) {}

  FloatLinearExpr* expr = nullptr;
  double value = 0;
};

// A linear expression that can be either integer or floating point.
class FloatLinearExpr {
 public:
  virtual ~FloatLinearExpr() = default;
  virtual void VisitAsFloat(FloatExprVisitor* /*lin*/, double /*c*/) {}
  virtual bool is_integer() const { return false; }
  virtual std::string ToString() const { return "FloatLinearExpr"; }
  virtual std::string DebugString() const { return ToString(); }

  static FloatLinearExpr* Sum(const std::vector<FloatExprOrValue>& exprs);
  static FloatLinearExpr* Sum(const std::vector<FloatExprOrValue>& exprs,
                              double cst);
  static FloatLinearExpr* WeightedSum(
      const std::vector<FloatExprOrValue>& exprs,
      const std::vector<double>& coeffs);
  static FloatLinearExpr* WeightedSum(
      const std::vector<FloatExprOrValue>& exprs,
      const std::vector<double>& coeffs, double cst);
  static FloatLinearExpr* Term(FloatLinearExpr* expr, double coeff);
  static FloatLinearExpr* Affine(FloatLinearExpr* expr, double coeff,
                                 double offset);
  static FloatLinearExpr* Constant(double value);

  FloatLinearExpr* FloatAddCst(double cst);
  FloatLinearExpr* FloatAdd(FloatLinearExpr* other);
  FloatLinearExpr* FloatSubCst(double cst);
  FloatLinearExpr* FloatSub(FloatLinearExpr* other);
  FloatLinearExpr* FloatRSub(FloatLinearExpr* other);
  FloatLinearExpr* FloatRSubCst(double cst);
  FloatLinearExpr* FloatMulCst(double cst);
  FloatLinearExpr* FloatNeg();
};

// Compare the indices of variables.
struct BaseIntVarComparator {
  bool operator()(const BaseIntVar* lhs, const BaseIntVar* rhs) const;
};

// A visitor class to process a floating point linear expression.
class FloatExprVisitor {
 public:
  void AddToProcess(FloatLinearExpr* expr, double coeff);
  void AddConstant(double constant);
  void AddVarCoeff(BaseIntVar* var, double coeff);
  double Process(FloatLinearExpr* expr, std::vector<BaseIntVar*>* vars,
                 std::vector<double>* coeffs);

 private:
  std::vector<std::pair<FloatLinearExpr*, double>> to_process_;
  absl::btree_map<BaseIntVar*, double, BaseIntVarComparator> canonical_terms_;
  double offset_ = 0;
};

// A class to build a canonical floating point linear expression.
class CanonicalFloatExpression {
 public:
  explicit CanonicalFloatExpression(FloatLinearExpr* expr);
  const std::vector<BaseIntVar*>& vars() const { return vars_; }
  const std::vector<double>& coeffs() const { return coeffs_; }
  double offset() const { return offset_; }

 private:
  double offset_;
  std::vector<BaseIntVar*> vars_;
  std::vector<double> coeffs_;
};

// A class to hold a constant.
class FloatConstant : public FloatLinearExpr {
 public:
  explicit FloatConstant(double value) : value_(value) {}
  ~FloatConstant() override = default;

  void VisitAsFloat(FloatExprVisitor* lin, double c) override;
  std::string ToString() const override;
  std::string DebugString() const override;

 private:
  double value_;
};

// A class to hold a weighted sum of floating point linear expressions.
class FloatWeightedSum : public FloatLinearExpr {
 public:
  FloatWeightedSum(const std::vector<FloatLinearExpr*>& exprs, double offset);
  FloatWeightedSum(const std::vector<FloatLinearExpr*>& exprs,
                   const std::vector<double>& coeffs, double offset);
  ~FloatWeightedSum() override = default;

  void VisitAsFloat(FloatExprVisitor* lin, double c) override;
  std::string ToString() const override;

 private:
  const absl::FixedArray<FloatLinearExpr*, 2> exprs_;
  const absl::FixedArray<double, 2> coeffs_;
  double offset_;
};

// A class to hold float_exr * a = b.
class FloatAffine : public FloatLinearExpr {
 public:
  FloatAffine(FloatLinearExpr* expr, double coeff, double offset);
  ~FloatAffine() override = default;

  void VisitAsFloat(FloatExprVisitor* lin, double c) override;
  std::string ToString() const override;
  std::string DebugString() const override;

  FloatLinearExpr* expression() const { return expr_; }
  double coefficient() const { return coeff_; }
  double offset() const { return offset_; }

 private:
  FloatLinearExpr* expr_;
  double coeff_;
  double offset_;
};

// A struct to hold an integer linear expression or an integer constant.
struct IntExprOrValue {
  explicit IntExprOrValue(IntLinExpr* e) : expr(e) {}
  explicit IntExprOrValue(int64_t v) : value(v) {}

  IntLinExpr* expr = nullptr;
  int64_t value = 0;
};

class IntLinExpr : public FloatLinearExpr {
 public:
  ~IntLinExpr() override = default;
  virtual void VisitAsInt(IntExprVisitor* /*lin*/, int64_t /*c*/) {}
  bool is_integer() const override { return true; }
  std::string ToString() const override { return "IntLinExpr"; }

  static IntLinExpr* Sum(const std::vector<IntLinExpr*>& exprs);
  static IntLinExpr* Sum(const std::vector<IntLinExpr*>& exprs, int64_t cst);
  static IntLinExpr* Sum(const std::vector<IntExprOrValue>& exprs, int64_t cst);
  static IntLinExpr* Sum(const std::vector<IntExprOrValue>& exprs);
  static IntLinExpr* WeightedSum(const std::vector<IntExprOrValue>& exprs,
                                 const std::vector<int64_t>& coeffs);
  static IntLinExpr* WeightedSum(const std::vector<IntExprOrValue>& exprs,
                                 const std::vector<int64_t>& coeffs,
                                 int64_t cst);
  static IntLinExpr* Term(IntLinExpr* expr, int64_t coeff);
  static IntLinExpr* Affine(IntLinExpr* expr, int64_t coeff, int64_t offset);
  static IntLinExpr* Constant(int64_t value);

  IntLinExpr* IntAddCst(int64_t cst);
  IntLinExpr* IntAdd(IntLinExpr* other);
  IntLinExpr* IntSubCst(int64_t cst);
  IntLinExpr* IntSub(IntLinExpr* other);
  IntLinExpr* IntRSubCst(int64_t cst);
  IntLinExpr* IntMulCst(int64_t cst);
  IntLinExpr* IntNeg();

  BoundedLinearExpression* EqCst(int64_t cst);
  BoundedLinearExpression* NeCst(int64_t cst);
  BoundedLinearExpression* GeCst(int64_t cst);
  BoundedLinearExpression* LeCst(int64_t cst);
  BoundedLinearExpression* LtCst(int64_t cst);
  BoundedLinearExpression* GtCst(int64_t cst);
  BoundedLinearExpression* Eq(IntLinExpr* other);
  BoundedLinearExpression* Ne(IntLinExpr* other);
  BoundedLinearExpression* Ge(IntLinExpr* other);
  BoundedLinearExpression* Le(IntLinExpr* other);
  BoundedLinearExpression* Lt(IntLinExpr* other);
  BoundedLinearExpression* Gt(IntLinExpr* other);
};

// A visitor class to process an integer linear expression.
class IntExprVisitor {
 public:
  void AddToProcess(IntLinExpr* expr, int64_t coeff);
  void AddConstant(int64_t constant);
  void AddVarCoeff(BaseIntVar* var, int64_t coeff);
  void ProcessAll();
  int64_t Process(std::vector<BaseIntVar*>* vars, std::vector<int64_t>* coeffs);
  int64_t Evaluate(IntLinExpr* expr, const CpSolverResponse& solution);

 private:
  std::vector<std::pair<IntLinExpr*, int64_t>> to_process_;
  absl::btree_map<BaseIntVar*, int64_t, BaseIntVarComparator> canonical_terms_;
  int64_t offset_ = 0;
};

// A class to hold a linear expression with bounds.
class BoundedLinearExpression {
 public:
  BoundedLinearExpression(IntLinExpr* expr, const Domain& bounds);

  BoundedLinearExpression(IntLinExpr* pos, IntLinExpr* neg,
                          const Domain& bounds);
  BoundedLinearExpression(int64_t offset, const Domain& bounds);
  ~BoundedLinearExpression() = default;

  const Domain& bounds() const;
  const std::vector<BaseIntVar*>& vars() const;
  const std::vector<int64_t>& coeffs() const;
  int64_t offset() const;
  std::string ToString() const;
  std::string DebugString() const;
  bool CastToBool(bool* result) const;

 private:
  Domain bounds_;
  int64_t offset_;
  std::vector<BaseIntVar*> vars_;
  std::vector<int64_t> coeffs_;
};

// A class to hold a constant.
class IntConstant : public IntLinExpr {
 public:
  explicit IntConstant(int64_t value) : value_(value) {}
  ~IntConstant() override = default;
  void VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    lin->AddConstant(value_ * c);
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

// A class to hold a sum of integer linear expressions.
class IntSum : public IntLinExpr {
 public:
  IntSum(const std::vector<IntLinExpr*>& exprs, int64_t offset)
      : exprs_(exprs.begin(), exprs.end()), offset_(offset) {}
  ~IntSum() override = default;

  void VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin->AddToProcess(exprs_[i], c);
    }
    lin->AddConstant(offset_ * c);
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin->AddToProcess(exprs_[i], c);
    }
    lin->AddConstant(offset_ * c);
  }

  std::string ToString() const override {
    if (exprs_.empty()) {
      return absl::StrCat(offset_);
    }
    std::string s = "(";
    for (int i = 0; i < exprs_.size(); ++i) {
      if (i > 0) {
        absl::StrAppend(&s, " + ");
      }
      absl::StrAppend(&s, exprs_[i]->ToString());
    }
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

  std::string DebugString() const override {
    return absl::StrCat("IntSum(",
                        absl::StrJoin(exprs_, ", ",
                                      [](std::string* out, IntLinExpr* expr) {
                                        absl::StrAppend(out,
                                                        expr->DebugString());
                                      }),
                        ", ", offset_, ")");
  }

 private:
  const absl::FixedArray<IntLinExpr*, 2> exprs_;
  int64_t offset_;
};

// A class to hold a weighted sum of integer linear expressions.
class IntWeightedSum : public IntLinExpr {
 public:
  IntWeightedSum(const std::vector<IntLinExpr*>& exprs,
                 const std::vector<int64_t>& coeffs, int64_t offset)
      : exprs_(exprs.begin(), exprs.end()),
        coeffs_(coeffs.begin(), coeffs.end()),
        offset_(offset) {}
  ~IntWeightedSum() override = default;

  void VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin->AddToProcess(exprs_[i], coeffs_[i] * c);
    }
    lin->AddConstant(offset_ * c);
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin->AddToProcess(exprs_[i], coeffs_[i] * c);
    }
    lin->AddConstant(offset_ * c);
  }

  std::string ToString() const override {
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

  std::string DebugString() const override {
    return absl::StrCat(
        "IntWeightedSum([",
        absl::StrJoin(exprs_, ", ",
                      [](std::string* out, IntLinExpr* expr) {
                        absl::StrAppend(out, expr->DebugString());
                      }),
        "], [", absl::StrJoin(coeffs_, ", "), "], ", offset_, ")");
  }

 private:
  const absl::FixedArray<IntLinExpr*, 2> exprs_;
  const absl::FixedArray<int64_t, 2> coeffs_;
  int64_t offset_;
};

// A class to hold int_exr * a = b.
class IntAffine : public IntLinExpr {
 public:
  IntAffine(IntLinExpr* expr, int64_t coeff, int64_t offset)
      : expr_(expr), coeff_(coeff), offset_(offset) {}
  ~IntAffine() override = default;

  void VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    lin->AddToProcess(expr_, c * coeff_);
    lin->AddConstant(offset_ * c);
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

  IntLinExpr* expression() const { return expr_; }
  int64_t coefficient() const { return coeff_; }
  int64_t offset() const { return offset_; }

 private:
  IntLinExpr* expr_;
  int64_t coeff_;
  int64_t offset_;
};

// A Boolean literal (a Boolean variable or its negation).
class Literal {
 public:
  virtual ~Literal() = default;
  virtual int index() const = 0;
  virtual Literal* negated() = 0;
};

// A class to hold a variable index.
class BaseIntVar : public IntLinExpr, public Literal {
 public:
  explicit BaseIntVar(int index)
      : index_(index), is_boolean_(false), negated_(nullptr) {
    DCHECK_GE(index, 0);
  }
  BaseIntVar(int index, bool is_boolean)
      : index_(index), is_boolean_(is_boolean), negated_(nullptr) {
    DCHECK_GE(index, 0);
  }

  ~BaseIntVar() override = default;

  int index() const override { return index_; }

  void VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    lin->AddVarCoeff(this, c);
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    lin->AddVarCoeff(this, c);
  }

  std::string ToString() const override {
    if (is_boolean_) {
      return absl::StrCat("BooleanBaseIntVar(", index_, ")");
    } else {
      return absl::StrCat("BaseIntVar(", index_, ")");
    }
  }

  std::string DebugString() const override {
    return absl::StrCat("BaseIntVar(index=", index_,
                        ", is_boolean=", is_boolean_, ")");
  }

  Literal* negated() override;

  bool is_boolean() const { return is_boolean_; }

  bool operator<(const BaseIntVar& other) const {
    return index_ < other.index_;
  }

 protected:
  const int index_;
  bool is_boolean_;
  Literal* negated_;
};

// A class to hold a negated variable index.
class NotBooleanVariable : public IntLinExpr, public Literal {
 public:
  explicit NotBooleanVariable(BaseIntVar* var) : var_(var) {}
  ~NotBooleanVariable() override = default;

  int index() const override { return -var_->index() - 1; }

  void VisitAsInt(IntExprVisitor* lin, int64_t c) override {
    lin->AddVarCoeff(var_, -c);
    lin->AddConstant(c);
  }

  void VisitAsFloat(FloatExprVisitor* lin, double c) override {
    lin->AddVarCoeff(var_, -c);
    lin->AddConstant(c);
  }

  std::string ToString() const override {
    return absl::StrCat("not(", var_->ToString(), ")");
  }

  Literal* negated() override { return var_; }

  std::string DebugString() const override {
    return absl::StrCat("NotBooleanVariable(index=", var_->index(), ")");
  }

 private:
  BaseIntVar* var_;
};

inline Literal* BaseIntVar::negated() {
  if (negated_ == nullptr) {
    negated_ = new NotBooleanVariable(this);
  }
  return negated_;
}

}  // namespace python
}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_PYTHON_LINEAR_EXPR_H_
