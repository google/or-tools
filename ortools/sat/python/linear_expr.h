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
#include "ortools/util/fp_roundtrip_conv.h"
#include "ortools/util/sorted_interval_list.h"

namespace operations_research::sat::python {

class BoundedLinearExpression;
class CanonicalFloatExpression;
class FloatExprVisitor;
class LinearExpr;
class IntExprVisitor;
class LinearExpr;
class BaseIntVar;
class NotBooleanVariable;

/// A class to hold a pointer to a linear expression or a constant.
struct ExprOrValue {
  explicit ExprOrValue(LinearExpr* e) : expr(e) {}
  explicit ExprOrValue(double v) : double_value(v) {}
  explicit ExprOrValue(int64_t v) : int_value(v) {}

  LinearExpr* expr = nullptr;
  double double_value = 0.0;
  int64_t int_value = 0;
};

/**
Holds an integer or floating point linear expression.

A linear expression is built from (integer or floating point) constants and
variables. For example, `x + 2 * (y - z + 1)`.

Linear expressions are used in CP-SAT models in constraints and in the
objective.

Note that constraints only accept linear expressions with integral coefficients
and constants. On the other hand, The objective can be a linear expression with
floating point coefficients and constants.

You can define linear constraints as in:

```
  model.add(x + 2 * y <= 5)
  model.add(sum(array_of_vars) == 5)
```

* In CP-SAT, the objective is a linear expression:

```
  model.minimize(x + 2 * y + z)
```

* For large arrays, using the LinearExpr class is faster that using the python
  `sum()` function. You can create constraints and the objective from lists of
  linear expressions or coefficients as follows:

```
  model.minimize(cp_model.LinearExpr.sum(expressions))
  model.add(cp_model.LinearExpr.weighted_sum(expressions, coefficients) >= 0)
```
*/
class LinearExpr {
 public:
  virtual ~LinearExpr() = default;
  virtual void VisitAsFloat(FloatExprVisitor& /*lin*/, double /*c*/) const = 0;
  virtual bool VisitAsInt(IntExprVisitor& /*lin*/, int64_t /*c*/) const = 0;
  bool IsInteger() const;
  virtual std::string ToString() const = 0;
  virtual std::string DebugString() const = 0;

  /**
   * Returns a new LinearExpr that is the sum of the given expressions.
   */
  static LinearExpr* Sum(const std::vector<LinearExpr*>& exprs);
  /**
   * Returns a new LinearExpr that is the sum of the given expressions or
   * constants.
   */
  static LinearExpr* MixedSum(const std::vector<ExprOrValue>& exprs);
  /// Returns the sum(exprs[i] * coeffs[i]).
  static LinearExpr* WeightedSumInt(const std::vector<LinearExpr*>& exprs,
                                    const std::vector<int64_t>& coeffs);
  /// Returns the sum(exprs[i] * coeffs[i]).
  static LinearExpr* WeightedSumFloat(const std::vector<LinearExpr*>& exprs,
                                      const std::vector<double>& coeffs);
  /// Returns the sum(exprs[i] * coeffs[i]).
  static LinearExpr* MixedWeightedSumInt(const std::vector<ExprOrValue>& exprs,
                                         const std::vector<int64_t>& coeffs);
  /// Returns the sum(exprs[i] * coeffs[i]).
  static LinearExpr* MixedWeightedSumFloat(
      const std::vector<ExprOrValue>& exprs, const std::vector<double>& coeffs);
  /// Returns expr * coeff.
  static LinearExpr* TermInt(LinearExpr* expr, int64_t coeff);
  /// Returns expr * coeff.
  static LinearExpr* TermFloat(LinearExpr* expr, double coeff);
  /// Returns expr * coeff + offset.
  static LinearExpr* AffineInt(LinearExpr* expr, int64_t coeff, int64_t offset);
  /// Returns expr * coeff + offset.
  static LinearExpr* AffineFloat(LinearExpr* expr, double coeff, double offset);
  /// Returns a new LinearExpr that is the given constant.
  static LinearExpr* ConstantInt(int64_t value);
  /// Returns a new LinearExpr that is the given constant.
  static LinearExpr* ConstantFloat(double value);

  /// Returns (this) + (expr).
  LinearExpr* Add(LinearExpr* expr);
  /// Returns (this) + (cst).
  LinearExpr* AddInt(int64_t cst);
  /// Returns (this) + (cst).
  LinearExpr* AddFloat(double cst);
  /// Returns (this) - (expr).
  LinearExpr* Sub(LinearExpr* expr);
  /// Returns (this) - (cst).
  LinearExpr* SubInt(int64_t cst);
  /// Returns (this) - (cst).
  LinearExpr* SubFloat(double cst);
  /// Returns (cst) - (this).
  LinearExpr* RSubInt(int64_t cst);
  /// Returns (cst) - (this).
  LinearExpr* RSubFloat(double cst);
  /// Returns (this) * (cst).
  LinearExpr* MulInt(int64_t cst);
  /// Returns (this) * (cst).
  LinearExpr* MulFloat(double cst);
  /// Returns -(this).
  LinearExpr* Neg();

  /// Returns (this) == (rhs).
  BoundedLinearExpression* Eq(LinearExpr* rhs);
  /// Returns (this) == (rhs).
  BoundedLinearExpression* EqCst(int64_t rhs);
  /// Returns (this) != (rhs).
  BoundedLinearExpression* Ne(LinearExpr* rhs);
  /// Returns (this) != (rhs).
  BoundedLinearExpression* NeCst(int64_t rhs);
  /// Returns (this) >= (rhs).
  BoundedLinearExpression* Ge(LinearExpr* rhs);
  /// Returns (this) >= (rhs).
  BoundedLinearExpression* GeCst(int64_t rhs);
  /// Returns (this) <= (rhs).
  BoundedLinearExpression* Le(LinearExpr* rhs);
  /// Returns (this) <= (rhs).
  BoundedLinearExpression* LeCst(int64_t rhs);
  /// Returns (this) < (rhs).
  BoundedLinearExpression* Lt(LinearExpr* rhs);
  /// Returns (this) < (rhs).
  BoundedLinearExpression* LtCst(int64_t rhs);
  /// Returns (this) > (rhs).
  BoundedLinearExpression* Gt(LinearExpr* rhs);
  /// Returns (this) > (rhs).
  BoundedLinearExpression* GtCst(int64_t rhs);
};

/// Compare the indices of variables.
struct BaseIntVarComparator {
  bool operator()(const BaseIntVar* lhs, const BaseIntVar* rhs) const;
};

/// A visitor class to process a floating point linear expression.
class FloatExprVisitor {
 public:
  void AddToProcess(const LinearExpr* expr, double coeff);
  void AddConstant(double constant);
  void AddVarCoeff(const BaseIntVar* var, double coeff);
  double Process(const LinearExpr* expr, std::vector<const BaseIntVar*>* vars,
                 std::vector<double>* coeffs);

 private:
  std::vector<std::pair<const LinearExpr*, double>> to_process_;
  absl::btree_map<const BaseIntVar*, double, BaseIntVarComparator>
      canonical_terms_;
  double offset_ = 0;
};

/// A class to build a canonical floating point linear expression.
class CanonicalFloatExpression {
 public:
  explicit CanonicalFloatExpression(LinearExpr* expr);
  const std::vector<const BaseIntVar*>& vars() const { return vars_; }
  const std::vector<double>& coeffs() const { return coeffs_; }
  double offset() const { return offset_; }

 private:
  std::vector<const BaseIntVar*> vars_;
  std::vector<double> coeffs_;
  double offset_ = 0;
};

/// A visitor class to process an integer linear expression.
class IntExprVisitor {
 public:
  void AddToProcess(const LinearExpr* expr, int64_t coeff);
  void AddConstant(int64_t constant);
  void AddVarCoeff(const BaseIntVar* var, int64_t coeff);
  bool ProcessAll();
  bool Process(std::vector<const BaseIntVar*>* vars,
               std::vector<int64_t>* coeffs, int64_t* offset);
  bool Evaluate(const LinearExpr* expr, const CpSolverResponse& solution,
                int64_t* value);

 private:
  std::vector<std::pair<const LinearExpr*, int64_t>> to_process_;
  absl::btree_map<const BaseIntVar*, int64_t, BaseIntVarComparator>
      canonical_terms_;
  int64_t offset_ = 0;
};

/// A class to build a canonical integer linear expression.
class CanonicalIntExpression {
 public:
  explicit CanonicalIntExpression(LinearExpr* expr);
  const std::vector<const BaseIntVar*>& vars() const { return vars_; }
  const std::vector<int64_t>& coeffs() const { return coeffs_; }
  int64_t offset() const { return offset_; }
  bool ok() const { return ok_; }

 private:
  std::vector<const BaseIntVar*> vars_;
  std::vector<int64_t> coeffs_;
  int64_t offset_ = 0;
  bool ok_ = true;
};

/**
 * A class to hold a sum of linear expressions, and optional integer and
 * double offsets (at most one of them can be non-zero, this is DCHECKed).
 */
class SumArray : public LinearExpr {
 public:
  explicit SumArray(const std::vector<LinearExpr*>& exprs,
                    int64_t int_offset = 0, double double_offset = 0.0)
      : exprs_(exprs.begin(), exprs.end()),
        int_offset_(int_offset),
        double_offset_(double_offset) {
    DCHECK(int_offset_ == 0 || double_offset_ == 0.0);
  }
  ~SumArray() override = default;

  bool VisitAsInt(IntExprVisitor& lin, int64_t c) const override {
    if (double_offset_ != 0.0) return false;
    for (int i = 0; i < exprs_.size(); ++i) {
      lin.AddToProcess(exprs_[i], c);
    }
    lin.AddConstant(int_offset_ * c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin.AddToProcess(exprs_[i], c);
    }
    if (int_offset_ != 0) {
      lin.AddConstant(int_offset_ * c);
    } else if (double_offset_ != 0.0) {
      lin.AddConstant(double_offset_ * c);
    }
  }

  std::string ToString() const override {
    if (exprs_.empty()) {
      if (double_offset_ != 0.0) {
        return absl::StrCat(RoundTripDoubleFormat(double_offset_));
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
      absl::StrAppend(
          &s, ", double_offset=", RoundTripDoubleFormat(double_offset_));
    }
    absl::StrAppend(&s, ")");
    return s;
  }

 private:
  const absl::FixedArray<LinearExpr*, 2> exprs_;
  const int64_t int_offset_;
  const double double_offset_;
};

/** A class to hold a weighted sum of floating point linear expressions. */
class FloatWeightedSum : public LinearExpr {
 public:
  FloatWeightedSum(const std::vector<LinearExpr*>& exprs, double offset);
  FloatWeightedSum(const std::vector<LinearExpr*>& exprs,
                   const std::vector<double>& coeffs, double offset);
  ~FloatWeightedSum() override = default;

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override;
  std::string ToString() const override;
  std::string DebugString() const override;

  bool VisitAsInt(IntExprVisitor& /*lin*/, int64_t /*c*/) const override {
    return false;
  }

 private:
  const absl::FixedArray<LinearExpr*, 2> exprs_;
  const absl::FixedArray<double, 2> coeffs_;
  double offset_;
};

/// A class to hold a weighted sum of integer linear expressions.
class IntWeightedSum : public LinearExpr {
 public:
  IntWeightedSum(const std::vector<LinearExpr*>& exprs,
                 const std::vector<int64_t>& coeffs, int64_t offset)
      : exprs_(exprs.begin(), exprs.end()),
        coeffs_(coeffs.begin(), coeffs.end()),
        offset_(offset) {}
  ~IntWeightedSum() override = default;

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin.AddToProcess(exprs_[i], coeffs_[i] * c);
    }
    lin.AddConstant(offset_ * c);
  }

  bool VisitAsInt(IntExprVisitor& lin, int64_t c) const override {
    for (int i = 0; i < exprs_.size(); ++i) {
      lin.AddToProcess(exprs_[i], coeffs_[i] * c);
    }
    lin.AddConstant(offset_ * c);
    return true;
  }

  std::string ToString() const override;
  std::string DebugString() const override;

 private:
  const absl::FixedArray<LinearExpr*, 2> exprs_;
  const absl::FixedArray<int64_t, 2> coeffs_;
  int64_t offset_;
};

/// A class to hold linear_expr * a = b (a and b are floating point numbers).
class FloatAffine : public LinearExpr {
 public:
  FloatAffine(LinearExpr* expr, double coeff, double offset);
  ~FloatAffine() override = default;

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override;
  bool VisitAsInt(IntExprVisitor& /*lin*/, int64_t /*c*/) const override {
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

/// A class to hold linear_expr * a = b (a and b are integers).
class IntAffine : public LinearExpr {
 public:
  IntAffine(LinearExpr* expr, int64_t coeff, int64_t offset)
      : expr_(expr), coeff_(coeff), offset_(offset) {}
  ~IntAffine() override = default;

  bool VisitAsInt(IntExprVisitor& lin, int64_t c) const override {
    lin.AddToProcess(expr_, c * coeff_);
    lin.AddConstant(offset_ * c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override {
    lin.AddToProcess(expr_, c * coeff_);
    lin.AddConstant(offset_ * c);
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

/// A class to hold a floating point constant as a linear expression.
class FloatConstant : public LinearExpr {
 public:
  explicit FloatConstant(double value) : value_(value) {}
  ~FloatConstant() override = default;

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override;
  bool VisitAsInt(IntExprVisitor& /*lin*/, int64_t /*c*/) const override {
    return false;
  }
  std::string ToString() const override;
  std::string DebugString() const override;

 private:
  double value_;
};

/// A class to hold an integer constant as a linear expression.
class IntConstant : public LinearExpr {
 public:
  explicit IntConstant(int64_t value) : value_(value) {}
  ~IntConstant() override = default;
  bool VisitAsInt(IntExprVisitor& lin, int64_t c) const override {
    lin.AddConstant(value_ * c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override {
    lin.AddConstant(value_ * c);
  }

  std::string ToString() const override { return absl::StrCat(value_); }

  std::string DebugString() const override {
    return absl::StrCat("IntConstant(", value_, ")");
  }

 private:
  int64_t value_;
};

/**
 * A class to hold a Boolean literal.
 *
 * A literal is a Boolean variable or its negation.
 *
 * Literals are used in CP-SAT models in constraints and in the objective.
 *
 * - You can define literal as in:
 *
 * ```
 * b1 = model.new_bool_var()
 * b2 = model.new_bool_var()
 * # Simple Boolean constraint.
 * model.add_bool_or(b1, b2.negated())
 * # We can use the ~ operator to negate a literal.
 * model.add_bool_or(b1, ~b2)
 * # Enforcement literals must be literals.
 * x = model.new_int_var(0, 10, 'x')
 * model.add(x == 5).only_enforced_if(~b1)
 * ```
 *
 * - Literals can be used directly in linear constraints or in the objective:
 *
 * ```
 *     model.minimize(b1  + 2 * ~b2)
 * ```
 */
class Literal : public LinearExpr {
 public:
  ~Literal() override = default;
  /// Returns the index of the current literal.
  virtual int index() const = 0;

  /**
   * Returns the negation of a literal (a Boolean variable or its negation).
   *
   * This method implements the logical negation of a Boolean variable.
   * It is only valid if the variable has a Boolean domain (0 or 1).
   *
   * Note that this method is nilpotent: `x.negated().negated() == x`.
   *
   * Returns:
   *   The negation of the current literal.
   */
  virtual Literal* negated() const = 0;
};

/**
 * A class to hold a variable index. It is the base class for Integer
 * variables.
 */
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

  bool VisitAsInt(IntExprVisitor& lin, int64_t c) const override {
    lin.AddVarCoeff(this, c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override {
    lin.AddVarCoeff(this, c);
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

  /// Returns the negation of the current variable.
  Literal* negated() const override { return negated_; }

  /// Returns true if the variable has a Boolean domain (0 or 1).
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

/// A class to hold a negated variable index.
class NotBooleanVariable : public Literal {
 public:
  explicit NotBooleanVariable(BaseIntVar* var) : var_(var) {}
  ~NotBooleanVariable() override = default;

  int index() const override { return -var_->index() - 1; }

  bool VisitAsInt(IntExprVisitor& lin, int64_t c) const override {
    lin.AddVarCoeff(var_, -c);
    lin.AddConstant(c);
    return true;
  }

  void VisitAsFloat(FloatExprVisitor& lin, double c) const override {
    lin.AddVarCoeff(var_, -c);
    lin.AddConstant(c);
  }

  std::string ToString() const override {
    return absl::StrCat("not(", var_->ToString(), ")");
  }

  /**
   * Returns the negation of the current literal, that is the original Boolean
   * variable.
   */
  Literal* negated() const override { return var_; }

  std::string DebugString() const override {
    return absl::StrCat("NotBooleanVariable(index=", var_->index(), ")");
  }

 private:
  BaseIntVar* const var_;
};

//// A class to hold a linear expression with bounds.
class BoundedLinearExpression {
 public:
  BoundedLinearExpression(std::vector<const BaseIntVar*> vars,
                          std::vector<int64_t> coeffs, int64_t offset,
                          const Domain& bounds);

  ~BoundedLinearExpression() = default;

  const Domain& bounds() const;
  const std::vector<const BaseIntVar*>& vars() const;
  const std::vector<int64_t>& coeffs() const;
  int64_t offset() const;
  std::string ToString() const;
  std::string DebugString() const;
  bool CastToBool(bool* result) const;

 private:
  const std::vector<const BaseIntVar*> vars_;
  const std::vector<int64_t> coeffs_;
  int64_t offset_;
  const Domain bounds_;
};

}  // namespace operations_research::sat::python

#endif  // OR_TOOLS_SAT_PYTHON_LINEAR_EXPR_H_
