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

#ifndef ORTOOLS_SAT_LINEAR_CONSTRAINT_H_
#define ORTOOLS_SAT_LINEAR_CONSTRAINT_H_

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/saturated_arithmetic.h"
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

  // Rather than using two std::vector<> this class is optimized for memory
  // consumption, given that most of our LinearConstraint are constructed once
  // and for all.
  //
  // It is however up to clients to maintain the invariants that both vars
  // and coeffs are properly allocated and of size num_terms.
  //
  // Also note that we did not add a copy constructor, to make sure that this is
  // moved as often as possible. This allowed to optimize a few call site and so
  // far we never copy this.
  int num_terms = 0;
  std::unique_ptr<IntegerVariable[]> vars;
  std::unique_ptr<IntegerValue[]> coeffs;

  LinearConstraint() = default;
  LinearConstraint(IntegerValue _lb, IntegerValue _ub) : lb(_lb), ub(_ub) {}

  // Compute the normalized violation of the constraint.
  // For a cut, this is the usual definition of its efficacy.
  double NormalizedViolation(
      const util_intops::StrongVector<IntegerVariable, double>& lp_values)
      const;

  // Resize the LinearConstraint to have space for num_terms. We always
  // re-allocate if the size is different to always be tight in memory.
  void resize(int size) {
    if (size == num_terms) return;
    IntegerVariable* tmp_vars = new IntegerVariable[size];
    IntegerValue* tmp_coeffs = new IntegerValue[size];
    const int to_copy = std::min(size, num_terms);
    if (to_copy > 0) {
      memcpy(tmp_vars, vars.get(), sizeof(IntegerVariable) * to_copy);
      memcpy(tmp_coeffs, coeffs.get(), sizeof(IntegerValue) * to_copy);
    }
    num_terms = size;
    vars.reset(tmp_vars);
    coeffs.reset(tmp_coeffs);
  }

  std::string DebugString() const {
    std::string result;
    if (lb.value() > kMinIntegerValue) {
      absl::StrAppend(&result, lb.value(), " <= ");
    }
    for (int i = 0; i < num_terms; ++i) {
      absl::StrAppend(&result, i > 0 ? " " : "",
                      IntegerTermDebugString(vars[i], coeffs[i]));
    }
    if (ub.value() < kMaxIntegerValue) {
      absl::StrAppend(&result, " <= ", ub.value());
    }
    return result;
  }

  bool IsEqualIgnoringBounds(const LinearConstraint& other) const {
    if (this->num_terms != other.num_terms) return false;
    if (this->num_terms == 0) return true;
    if (memcmp(this->vars.get(), other.vars.get(),
               sizeof(IntegerVariable) * this->num_terms)) {
      return false;
    }
    if (memcmp(this->coeffs.get(), other.coeffs.get(),
               sizeof(IntegerValue) * this->num_terms)) {
      return false;
    }
    return true;
  }

  // We rarelly need to copy a LinearConstraint and it should almost always
  // be moved instead, so we don't want a copy constructor. This can be used
  // if one really need to copy it.
  void CopyFrom(const LinearConstraint& other) {
    const int n = other.num_terms;
    resize(n);
    lb = other.lb;
    ub = other.ub;
    std::memcpy(vars.get(), other.vars.get(), n * sizeof(IntegerVariable));
    std::memcpy(coeffs.get(), other.coeffs.get(), n * sizeof(IntegerValue));
  }

  bool operator==(const LinearConstraint& other) const {
    if (this->lb != other.lb) return false;
    if (this->ub != other.ub) return false;
    return IsEqualIgnoringBounds(other);
  }

  absl::Span<const IntegerVariable> VarsAsSpan() const {
    return absl::MakeSpan(vars.get(), num_terms);
  }

  absl::Span<const IntegerValue> CoeffsAsSpan() const {
    return absl::MakeSpan(coeffs.get(), num_terms);
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
  double LpValue(const util_intops::StrongVector<IntegerVariable, double>&
                     lp_values) const;

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
IntegerValue GetCoefficient(IntegerVariable var, const LinearExpression& expr);
IntegerValue GetCoefficientOfPositiveVar(IntegerVariable var,
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
  explicit LinearConstraintBuilder(IntegerEncoder* encoder)
      : encoder_(encoder), lb_(0), ub_(0) {}
  LinearConstraintBuilder(const Model* model, IntegerValue lb, IntegerValue ub)
      : encoder_(model->Get<IntegerEncoder>()), lb_(lb), ub_(ub) {}
  LinearConstraintBuilder(IntegerEncoder* encoder, IntegerValue lb,
                          IntegerValue ub)
      : encoder_(encoder), lb_(lb), ub_(ub) {}

  // Warning: this version without encoder cannot be used to add literals, so
  // one shouldn't call AddLiteralTerm() on it. All other functions works.
  //
  // TODO(user): Have a subclass so we can enforce that a caller using
  // AddLiteralTerm() must construct the Builder with an encoder.
  LinearConstraintBuilder() : encoder_(nullptr), lb_(0), ub_(0) {}
  LinearConstraintBuilder(IntegerValue lb, IntegerValue ub)
      : encoder_(nullptr), lb_(lb), ub_(ub) {}

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
      absl::Span<const LiteralValueValue> product);

  // Add literal * coeff to the constraint. Returns false and do nothing if the
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

  // Similar to BuildConstraint() but make sure we don't overflow while we merge
  // terms referring to the same variables.
  bool BuildIntoConstraintAndCheckOverflow(IntegerValue lb, IntegerValue ub,
                                           LinearConstraint* ct);

  // Returns the linear expression part of the constraint only, without the
  // bounds.
  LinearExpression BuildExpression();

  int NumTerms() const { return terms_.size(); }

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
    const util_intops::StrongVector<IntegerVariable, double>& values);

// Tests for possible overflow in the given linear constraint used for the
// linear relaxation. This is a bit relaxed compared to what we require for
// generic linear constraint that are used in our CP propagators.
//
// If this check pass, our constraint should be safe to use in our
// simplification code, our cut computation, etc...
bool PossibleOverflow(const IntegerTrail& integer_trail,
                      const LinearConstraint& constraint);

// Returns sqrt(sum square(coeff)).
double ComputeL2Norm(const LinearConstraint& constraint);

// Returns the maximum absolute value of the coefficients.
IntegerValue ComputeInfinityNorm(const LinearConstraint& constraint);

// Returns the scalar product of given constraint coefficients. This method
// assumes that the constraint variables are in sorted order.
double ScalarProduct(const LinearConstraint& constraint1,
                     const LinearConstraint& constraint2);

// Computes the GCD of the constraint coefficient, and divide them by it. This
// also tighten the constraint bounds assuming all the variables are integer.
void DivideByGCD(LinearConstraint* constraint);

// Makes all coefficients positive by transforming a variable to its negation.
void MakeAllCoefficientsPositive(LinearConstraint* constraint);

// Makes all variables "positive" by transforming a variable to its negation.
void MakeAllVariablesPositive(LinearConstraint* constraint);

// Returns false if duplicate variables are found in ct.
bool NoDuplicateVariable(const LinearConstraint& ct);

// Sorts and merges duplicate IntegerVariable in the given "terms".
// Fills the given LinearConstraint or LinearExpression with the result.
inline void CleanTermsAndFillConstraint(
    std::vector<std::pair<IntegerVariable, IntegerValue>>* terms,
    LinearExpression* output) {
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

inline void CleanTermsAndFillConstraint(
    std::vector<std::pair<IntegerVariable, IntegerValue>>* terms,
    LinearConstraint* output) {
  // Sort and add coeff of duplicate variables. Note that a variable and
  // its negation will appear one after another in the natural order.
  int new_size = 0;
  output->resize(terms->size());
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
        output->vars[new_size] = previous_var;
        output->coeffs[new_size] = current_coeff;
        ++new_size;
      }
      previous_var = entry.first;
      current_coeff = entry.second;
    }
  }
  if (current_coeff != 0) {
    output->vars[new_size] = previous_var;
    output->coeffs[new_size] = current_coeff;
    ++new_size;
  }
  output->resize(new_size);
}

inline bool MergePositiveVariableTermsAndCheckForOverflow(
    std::vector<std::pair<IntegerVariable, IntegerValue>>* terms,
    LinearConstraint* output) {
  // Sort and add coeff of duplicate variables. Note that a variable and
  // its negation will appear one after another in the natural order.
  int new_size = 0;
  output->resize(terms->size());
  std::sort(terms->begin(), terms->end());
  IntegerVariable previous_var = kNoIntegerVariable;
  int64_t current_coeff = 0;
  for (const std::pair<IntegerVariable, IntegerValue>& entry : *terms) {
    DCHECK(VariableIsPositive(entry.first));
    if (previous_var == entry.first) {
      if (AddIntoOverflow(entry.second.value(), &current_coeff)) {
        return false;
      }
    } else {
      if (current_coeff != 0) {
        output->vars[new_size] = previous_var;
        output->coeffs[new_size] = current_coeff;
        ++new_size;
      }
      previous_var = entry.first;
      current_coeff = entry.second.value();
    }
  }
  if (current_coeff != 0) {
    output->vars[new_size] = previous_var;
    output->coeffs[new_size] = current_coeff;
    ++new_size;
  }
  output->resize(new_size);
  return true;
}

}  // namespace sat
}  // namespace operations_research

#endif  // ORTOOLS_SAT_LINEAR_CONSTRAINT_H_
