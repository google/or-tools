// Copyright 2010-2018 Google LLC
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

#include <vector>

#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"

namespace operations_research {
namespace sat {

// One linear constraint on a set of Integer variables.
// Important: there should be no duplicate variables.
//
// We also assume that we never have integer overflow when evaluating such
// constraint. This should be enforced by the checker for user given
// constraints, and we must enforce it ourselves for the newly created
// constraint. We requires:
//  -  sum_i max(0, max(c_i * lb_i, c_i * ub_i)) < kMaxIntegerValue.
//  -  sum_i min(0, min(c_i * lb_i, c_i * ub_i)) > kMinIntegerValue
// so that in whichever order we compute the sum, we have no overflow. Note
// that this condition invoves the bounds of the variables.
//
// TODO(user): Add DCHECKs for the no-overflow property? but we need access
// to the variable bounds.
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
      const IntegerValue coeff =
          VariableIsPositive(vars[i]) ? coeffs[i] : -coeffs[i];
      absl::StrAppend(&result, i > 0 ? " " : "", coeff.value(), "*X",
                      vars[i].value() / 2);
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

// Allow to build a LinearConstraint while making sure there is no duplicate
// variables. Note that we do not simplify literal/variable that are currently
// fixed here.
class LinearConstraintBuilder {
 public:
  // We support "sticky" kMinIntegerValue for lb and kMaxIntegerValue for ub
  // for one-sided constraints.
  //
  // Assumes that the 'model' has IntegerEncoder.
  LinearConstraintBuilder(const Model* model, IntegerValue lb, IntegerValue ub)
      : encoder_(*model->Get<IntegerEncoder>()), lb_(lb), ub_(ub) {}

  // Adds var * coeff to the constraint.
  void AddTerm(IntegerVariable var, IntegerValue coeff);
  void AddTerm(AffineExpression expr, IntegerValue coeff);

  // Add value as a constant term to the linear equation.
  void AddConstant(IntegerValue value);

  // Add literal * coeff to the constaint. Returns false and do nothing if the
  // given literal didn't have an integer view.
  ABSL_MUST_USE_RESULT bool AddLiteralTerm(Literal lit, IntegerValue coeff);

  // Builds and return the corresponding constraint in a canonical form.
  // All the IntegerVariable will be positive and appear in increasing index
  // order.
  //
  // TODO(user): this doesn't invalidate the builder object, but if one wants
  // to do a lot of dynamic editing to the constraint, then then underlying
  // algorithm needs to be optimized of that.
  LinearConstraint Build();

 private:
  const IntegerEncoder& encoder_;
  IntegerValue lb_;
  IntegerValue ub_;

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

// Sorts and merges duplicate IntegerVariable in the given "terms".
// Fills the given LinearConstraint with the result.
void CleanTermsAndFillConstraint(
    std::vector<std::pair<IntegerVariable, IntegerValue>>* terms,
    LinearConstraint* constraint);

// Sorts the terms and makes all IntegerVariable positive. This assumes that a
// variable or its negation only appear once.
//
// Note that currently this allocates some temporary memory.
void CanonicalizeConstraint(LinearConstraint* ct);

// Returns false if duplicate variables are found in ct.
bool NoDuplicateVariable(const LinearConstraint& ct);

// Helper struct to model linear expression for lin_min/lin_max constraints. The
// canonical expression should only contain positive coefficients.
struct LinearExpression {
  std::vector<IntegerVariable> vars;
  std::vector<IntegerValue> coeffs;
  IntegerValue offset = IntegerValue(0);
};

// Returns the same expression in the canonical form (all positive
// coefficients).
LinearExpression CanonicalizeExpr(const LinearExpression& expr);

// Returns lower bound of linear expression using variable bounds of the
// variables in expression. Assumes Canonical expression (all positive
// coefficients).
IntegerValue LinExprLowerBound(const LinearExpression& expr,
                               const IntegerTrail& integer_trail);

// Returns upper bound of linear expression using variable bounds of the
// variables in expression. Assumes Canonical expression (all positive
// coefficients).
IntegerValue LinExprUpperBound(const LinearExpression& expr,
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

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_LINEAR_CONSTRAINT_H_
