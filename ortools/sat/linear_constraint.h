// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_SAT_LINEAR_CONSTRAINT_H_
#define OR_TOOLS_SAT_LINEAR_CONSTRAINT_H_

#include <algorithm>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {

// One linear constraint on a set of Integer variables.
// Important: there should be no duplicate variables.
//
// We also assume that we never have integer overflow when evaluating such
// constraint at the ROOT node. This should be enforced by the checker for user
// given constraints, and we must enforce it ourselves for the newly created
// constraint. See ValidateLinearConstraintForOverflow().
struct LinearConstraint {
  IntegerValue lb;
  IntegerValue ub;
  std::vector<IntegerVariable> vars;
  std::vector<IntegerValue> coeffs;

  LinearConstraint() {}
  LinearConstraint(IntegerValue _lb, IntegerValue _ub) : lb(_lb), ub(_ub) {}

  void AddTerm(IntegerVariable var, IntegerValue coeff) {
    vars.push_back(var);
    coeffs.push_back(coeff);
  }

  void Clear() {
    lb = ub = IntegerValue(0);
    ClearTerms();
  }

  void ClearTerms() {
    vars.clear();
    coeffs.clear();
  }

  std::string DebugString() const {
    std::string result;
    if (lb.value() > kMinIntegerValue) {
      absl::StrAppend(&result, lb.value(), " <= ");
    }
    for (int i = 0; i < vars.size(); ++i) {
      absl::StrAppend(&result, i > 0 ? " " : "",
                      IntegerTermDebugString(vars[i], coeffs[i]));
    }
    if (ub.value() < kMaxIntegerValue) {
      absl::StrAppend(&result, " <= ", ub.value());
    }
    return result;
  }

  bool operator==(const LinearConstraint other) const {
    if (this->lb != other.lb) return false;
    if (this->ub != other.ub) return false;
    if (this->vars != other.vars) return false;
    if (this->coeffs != other.coeffs) return false;
    return true;
  }
};

inline std::ostream& operator<<(std::ostream& os, const LinearConstraint& ct) {
  os << ct.DebugString();
  return os;
}

// Helper struct to model linear expression for lin_min/lin_max constraints. The
// canonical expression should only contain positive coefficients.
struct LinearExpression {
  std::vector<IntegerVariable> vars;
  std::vector<IntegerValue> coeffs;
  IntegerValue offset = IntegerValue(0);

  // Return[s] the evaluation of the linear expression.
  double LpValue(
      const absl::StrongVector<IntegerVariable, double>& lp_values) const;

  IntegerValue LevelZeroMin(IntegerTrail* integer_trail) const;

  // Returns lower bound of linear expression using variable bounds of the
  // variables in expression.
  IntegerValue Min(const IntegerTrail& integer_trail) const;

  // Returns upper bound of linear expression using variable bounds of the
  // variables in expression.
  IntegerValue Max(const IntegerTrail& integer_trail) const;

  std::string DebugString() const;
};

// Returns the same expression in the canonical form (all positive
// coefficients).
LinearExpression CanonicalizeExpr(const LinearExpression& expr);

// Makes sure that any of our future computation on this constraint will not
// cause overflow. We use the level zero bounds and use the same definition as
// in PossibleIntegerOverflow() in the cp_model.proto checker.
//
// Namely, the sum of positive terms, the sum of negative terms and their
// difference shouldn't overflow. Note that we don't validate the rhs, but if
// the bounds are properly relaxed, then this shouldn't cause any issues.
//
// Note(user): We should avoid doing this test too often as it can be slow. At
// least do not do it more than once on each constraint.
bool ValidateLinearConstraintForOverflow(const LinearConstraint& constraint,
                                         const IntegerTrail& integer_trail);

// Preserves canonicality.
LinearExpression NegationOf(const LinearExpression& expr);

// Returns the same expression with positive variables.
LinearExpression PositiveVarExpr(const LinearExpression& expr);

// Returns the coefficient of the variable in the expression. Works in linear
// time.
// Note: GetCoefficient(NegationOf(var, expr)) == -GetCoefficient(var, expr).
IntegerValue GetCoefficient(const IntegerVariable var,
                            const LinearExpression& expr);
IntegerValue GetCoefficientOfPositiveVar(const IntegerVariable var,
                                         const LinearExpression& expr);

// Allow to build a LinearConstraint while making sure there is no duplicate
// variables. Note that we do not simplify literal/variable that are currently
// fixed here.
//
// All the functions manipulate a linear expression with an offset. The final
// constraint bounds will include this offset.
//
// TODO(user): Rename to LinearExpressionBuilder?
class LinearConstraintBuilder {
 public:
  // We support "sticky" kMinIntegerValue for lb and kMaxIntegerValue for ub
  // for one-sided constraints.
  //
  // Assumes that the 'model' has IntegerEncoder. The bounds can either be
  // specified at construction or during the Build() call.
  explicit LinearConstraintBuilder(const Model* model)
      : encoder_(model->Get<IntegerEncoder>()), lb_(0), ub_(0) {}
  LinearConstraintBuilder(const Model* model, IntegerValue lb, IntegerValue ub)
      : encoder_(model->Get<IntegerEncoder>()), lb_(lb), ub_(ub) {}

  // Warning: this version without encoder cannot be used to add literals, so
  // one shouldn't call AddLiteralTerm() on it. All other functions works.
  //
  // TODO(user): Have a subclass so we can enforce than caller using
  // AddLiteralTerm() must construct the Builder with an encoder.
  LinearConstraintBuilder() : encoder_(nullptr), lb_(0), ub_(0) {}

  // Adds the corresponding term to the current linear expression.
  void AddConstant(IntegerValue value);
  void AddTerm(IntegerVariable var, IntegerValue coeff);
  void AddTerm(AffineExpression expr, IntegerValue coeff);
  void AddLinearExpression(const LinearExpression& expr);
  void AddLinearExpression(const LinearExpression& expr, IntegerValue coeff);

  // Add the corresponding decomposed products (obtained from
  // TryToDecomposeProduct). The code assumes all literals to be in an
  // exactly_one relation.
  // It returns false if one literal does not have an integer view, as it
  // actually calls AddLiteralTerm().
  ABSL_MUST_USE_RESULT bool AddDecomposedProduct(
      const std::vector<LiteralValueValue>& product);

  // Add literal * coeff to the constaint. Returns false and do nothing if the
  // given literal didn't have an integer view.
  ABSL_MUST_USE_RESULT bool AddLiteralTerm(
      Literal lit, IntegerValue coeff = IntegerValue(1));

  // Add an under linearization of the product of two affine expressions.
  // If at least one of them is fixed, then we add the exact product (which is
  // linear). Otherwise, we use McCormick relaxation:
  //     left * right = (left_min + delta_left) * (right_min + delta_right) =
  //         left_min * right_min + delta_left * right_min +
  //          delta_right * left_min + delta_left * delta_right
  //     which is >= (by ignoring the quatratic term)
  //         right_min * left + left_min * right - right_min * left_min
  //
  // TODO(user): We could use (max - delta) instead of (min + delta) for each
  // expression instead. This would depend on the LP value of the left and
  // right.
  void AddQuadraticLowerBound(AffineExpression left, AffineExpression right,
                              IntegerTrail* integer_trail,
                              bool* is_quadratic = nullptr);

  // Clears all added terms and constants. Keeps the original bounds.
  void Clear() {
    offset_ = IntegerValue(0);
    terms_.clear();
  }

  // Reset the bounds passed at construction time.
  void ResetBounds(IntegerValue lb, IntegerValue ub) {
    lb_ = lb;
    ub_ = ub;
  }

  // Builds and returns the corresponding constraint in a canonical form.
  // All the IntegerVariable will be positive and appear in increasing index
  // order.
  //
  // The bounds can be changed here or taken at construction.
  //
  // TODO(user): this doesn't invalidate the builder object, but if one wants
  // to do a lot of dynamic editing to the constraint, then then underlying
  // algorithm needs to be optimized for that.
  LinearConstraint Build();
  LinearConstraint BuildConstraint(IntegerValue lb, IntegerValue ub);

  // Returns the linear expression part of the constraint only, without the
  // bounds.
  LinearExpression BuildExpression();

 private:
  const IntegerEncoder* encoder_;
  IntegerValue lb_;
  IntegerValue ub_;

  IntegerValue offset_ = IntegerValue(0);

  // Initially we push all AddTerm() here, and during Build() we merge terms
  // on the same variable.
  std::vector<std::pair<IntegerVariable, IntegerValue>> terms_;
};

// Returns the activity of the given constraint. That is the current value of
// the linear terms.
double ComputeActivity(
    const LinearConstraint& constraint,
    const absl::StrongVector<IntegerVariable, double>& values);

// Returns sqrt(sum square(coeff)).
double ComputeL2Norm(const LinearConstraint& constraint);

// Returns the maximum absolute value of the coefficients.
IntegerValue ComputeInfinityNorm(const LinearConstraint& constraint);

// Returns the scalar product of given constraint coefficients. This method
// assumes that the constraint variables are in sorted order.
double ScalarProduct(const LinearConstraint& constraint1,
                     const LinearConstraint& constraint2);

// Computes the GCD of the constraint coefficient, and divide them by it. This
// also tighten the constraint bounds assumming all the variables are integer.
void DivideByGCD(LinearConstraint* constraint);

// Removes the entries with a coefficient of zero.
void RemoveZeroTerms(LinearConstraint* constraint);

// Makes all coefficients positive by transforming a variable to its negation.
void MakeAllCoefficientsPositive(LinearConstraint* constraint);

// Makes all variables "positive" by transforming a variable to its negation.
void MakeAllVariablesPositive(LinearConstraint* constraint);

// Sorts the terms and makes all IntegerVariable positive. This assumes that a
// variable or its negation only appear once.
//
// Note that currently this allocates some temporary memory.
void CanonicalizeConstraint(LinearConstraint* ct);

// Returns false if duplicate variables are found in ct.
bool NoDuplicateVariable(const LinearConstraint& ct);

// Sorts and merges duplicate IntegerVariable in the given "terms".
// Fills the given LinearConstraint or LinearExpression with the result.
//
// TODO(user): This actually only sort the terms, we don't clean them.
template <class ClassWithVarsAndCoeffs>
void CleanTermsAndFillConstraint(
    std::vector<std::pair<IntegerVariable, IntegerValue>>* terms,
    ClassWithVarsAndCoeffs* output) {
  output->vars.clear();
  output->coeffs.clear();

  // Sort and add coeff of duplicate variables. Note that a variable and
  // its negation will appear one after another in the natural order.
  std::sort(terms->begin(), terms->end());
  IntegerVariable previous_var = kNoIntegerVariable;
  IntegerValue current_coeff(0);
  for (const std::pair<IntegerVariable, IntegerValue>& entry : *terms) {
    if (previous_var == entry.first) {
      current_coeff += entry.second;
    } else if (previous_var == NegationOf(entry.first)) {
      current_coeff -= entry.second;
    } else {
      if (current_coeff != 0) {
        output->vars.push_back(previous_var);
        output->coeffs.push_back(current_coeff);
      }
      previous_var = entry.first;
      current_coeff = entry.second;
    }
  }
  if (current_coeff != 0) {
    output->vars.push_back(previous_var);
    output->coeffs.push_back(current_coeff);
  }
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_CONSTRAINT_H_
