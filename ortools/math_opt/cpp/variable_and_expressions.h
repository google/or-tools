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

// An object oriented wrapper for variables in IndexedModel with support for
// arithmetic operations to build linear expressions and express linear
// constraints.
//
// Types are:
//   - Variable: a reference to a variable of an IndexedModel.
//
//   - LinearExpression: a weighted sum of variables with an optional offset;
//     something like `3*x + 2*y + 5`.
//
//   - LinearTerm: a term of a linear expression, something like `2*x`. It is
//     used as an intermediate in the arithmetic operations that builds linear
//     expressions.
//
//   - (Lower|Upper)BoundedLinearExpression: two classes representing the result
//     of the comparison of a LinearExpression with a constant. For example `3*x
//     + 2*y + 5 >= 3`.
//
//   - BoundedLinearExpression: the result of the comparison of a linear
//     expression with two bounds, an upper bound and a lower bound. For example
//     `2 <= 3*x + 2*y + 5 <= 3`; or `4 >= 3*x + 2*y + 5 >= 1`.
//
//   - VariablesEquality: the result of comparing two Variable instances with
//     the == operator. For example `a == b`. This intermediate class support
//     implicit conversion to both bool and BoundedLinearExpression types. This
//     enables using variables as key of maps (using the conversion to bool)
//     without preventing adding constraints of variable equality.
//
// The basic arithmetic operators are overloaded for those types so that we can
// write math expressions with variables to build linear expressions. The >=, <=
// and == comparison operators are overloaded to produce BoundedLinearExpression
// that can be used to build constraints.
//
// For example we can have:
//   const Variable x = ...;
//   const Variable y = ...;
//   const LinearExpression expr = 2 * x + 3 * y - 2;
//   const BoundedLinearExpression bounded_expr = 1 <= 2 * x + 3 * y - 2 <= 10;
//
// To making working with containers of doubles/Variables/LinearExpressions
// easier, the template methods Sum() and InnerProduct() are provided, e.g.
//   const std::vector<int> ints = ...;
//   const std::vector<double> doubles = ...;
//   const std::vector<Variable> vars = ...;
//   const std::vector<LinearTerm> terms = ...;
//   const std::vector<LinearExpression> exprs = ...;
//   const LinearExpression s1 = Sum(ints);
//   const LinearExpression s2 = Sum(doubles);
//   const LinearExpression s3 = Sum(vars);
//   const LinearExpression s4 = Sum(terms);
//   const LinearExpression s5 = Sum(exprs);
//   const LinearExpression p1 = InnerProduct(ints, vars);
//   const LinearExpression p2 = InnerProduct(terms, doubles);
//   const LinearExpression p3 = InnerProduct(doubles, exprs);
// These methods work on any iterable type (defining begin() and end()). For
// InnerProduct, the inputs must be of equal size, and a compile time error will
// be generated unless at least one input is a container of a type implicitly
// convertible to double.
//
// Pre C++20, avoid the use of std::accumulate and std::inner_product with
// LinearExpression, they cause a quadratic blowup in running time.
//
// While there is some complexity in the source, users typically should not need
// to look at types other than Variable and LinearExpression too closely. Their
// code usually will only refer to those types.
#ifndef OR_TOOLS_MATH_OPT_CPP_VARIABLE_AND_EXPRESSIONS_H_
#define OR_TOOLS_MATH_OPT_CPP_VARIABLE_AND_EXPRESSIONS_H_

#include <stdint.h>

#include <initializer_list>
#include <iterator>
#include <limits>
#include <string>
#include <utility>

#include "ortools/base/logging.h"
#include "absl/container/flat_hash_map.h"
#include "ortools/base/int_type.h"
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/id_map.h"  // IWYU pragma: export

namespace operations_research {
namespace math_opt {

// A value type that references a variable from IndexedModel. Usually this type
// is passed by copy.
class Variable {
 public:
  // The typed integer used for ids.
  using IdType = VariableId;

  // Usually users will obtain variables using MathOpt::AddVariable(). There
  // should be little for users to build this object from an IndexedModel.
  inline Variable(IndexedModel* model, VariableId id);

  // Each call to AddVariable will produce Variables id() increasing by one,
  // starting at zero. Deleted ids are NOT reused. Thus, if no variables are
  // deleted, the ids in the model will be consecutive.
  inline int64_t id() const;

  inline VariableId typed_id() const;
  inline IndexedModel* model() const;

  inline double lower_bound() const;
  inline double upper_bound() const;
  inline bool is_integer() const;
  inline const std::string& name() const;

  inline void set_lower_bound(double lower_bound) const;
  inline void set_upper_bound(double upper_bound) const;
  inline void set_is_integer(bool is_integer) const;
  inline void set_integer() const;
  inline void set_continuous() const;

  template <typename H>
  friend H AbslHashValue(H h, const Variable& variable);
  friend std::ostream& operator<<(std::ostream& ostr, const Variable& variable);

 private:
  IndexedModel* model_;
  VariableId id_;
};

// Implements the API of std::unordered_map<Variable, V>, but forbids Variables
// from different models in the same map.
template <typename V>
using VariableMap = IdMap<Variable, V>;

inline std::ostream& operator<<(std::ostream& ostr, const Variable& variable);

// A term in an sum of variables multiplied by coefficients.
struct LinearTerm {
  // Usually this constructor is never called explicitly by users. Instead it
  // will be implicitly used when writing linear expression. For example `x +
  // 2*y` will automatically use this constructor to build a LinearTerm from `x`
  // and the overload of the operator* will also automatically create the one
  // from `2*y`.
  inline LinearTerm(Variable variable, double coefficient);
  inline LinearTerm operator-() const;
  inline LinearTerm& operator*=(double d);
  inline LinearTerm& operator/=(double d);
  Variable variable;
  double coefficient;
};

inline LinearTerm operator*(double coefficient, LinearTerm term);
inline LinearTerm operator*(LinearTerm term, double coefficient);
inline LinearTerm operator*(double coefficient, Variable variable);
inline LinearTerm operator*(Variable variable, double coefficient);
inline LinearTerm operator/(LinearTerm term, double coefficient);
inline LinearTerm operator/(Variable variable, double coefficient);

// This class represents a sum of variables multiplied by coefficient and an
// optional offset constant. For example: "3*x + 2*y + 5".
//
// All operations, including constructor, will raise an assertion if the
// operands involve variables from different MathOpt objects.
//
// Contrary to Variable type, expressions owns the linear expression their
// represent. Hence they are usually passed by reference to prevent unnecessary
// copies.
//
// TODO(b/169415098): add a function to remove zero terms.
// TODO(b/169415834): study if exact zeros should be automatically removed.
// TODO(b/169415103): add tests that some expressions don't compile.
class LinearExpression {
 public:
  // For unit testing purpose, we define optional counters. We have to
  // explicitly define default constructors in that case.
#ifndef USE_LINEAR_EXPRESSION_COUNTERS
  LinearExpression() = default;
#else   // USE_LINEAR_EXPRESSION_COUNTERS
  LinearExpression();
  LinearExpression(const LinearExpression& other);
  LinearExpression(LinearExpression&& other);
  LinearExpression& operator=(const LinearExpression& other);
#endif  // USE_LINEAR_EXPRESSION_COUNTERS
  // Usually users should use the overloads of operators to build linear
  // expressions. For example, assuming `x` and `y` are Variable, then `x + 2*y
  // + 5` will build a LinearExpression automatically.
  inline LinearExpression(std::initializer_list<LinearTerm> terms,
                          double offset);
  inline LinearExpression(double offset);           // NOLINT
  inline LinearExpression(Variable variable);       // NOLINT
  inline LinearExpression(const LinearTerm& term);  // NOLINT

  inline LinearExpression& operator+=(const LinearExpression& other);
  inline LinearExpression& operator+=(const LinearTerm& term);
  inline LinearExpression& operator+=(Variable variable);
  inline LinearExpression& operator+=(double value);
  inline LinearExpression& operator-=(const LinearExpression& other);
  inline LinearExpression& operator-=(const LinearTerm& term);
  inline LinearExpression& operator-=(Variable variable);
  inline LinearExpression& operator-=(double value);
  inline LinearExpression& operator*=(double value);
  inline LinearExpression& operator/=(double value);

  // Adds each element of items to this.
  //
  // Specifically, letting
  //   (i_1, i_2, ..., i_n) = items
  // adds
  //   i_1 + i_2 + ... + i_n
  // to this.
  //
  // Example:
  //   Variable a = ...;
  //   Variable b = ...;
  //   const std::vector<Variable> vars = {a, b};
  //   LinearExpression expr(8.0);
  //   expr.AddSum(vars);
  // Results in expr having the value a + b + 8.0.
  //
  // Compile time requirements:
  //  * Iterable is a sequence (an array or object with begin() and end()).
  //  * The type of an element of items is one of double, Variable, LinearTerm
  //    or LinearExpression (or is implicitly convertible to one of these types,
  //    e.g. int).
  //
  // Note: The implementation is equivalent to:
  //   for(const auto item : items) {
  //     *this += item;
  //   }
  template <typename Iterable>
  inline void AddSum(const Iterable& items);

  // Adds the inner product of left and right to this.
  //
  // Specifically, letting
  //   (l_1, l_2 ..., l_n) = left,
  //   (r_1, r_2, ..., r_n) = right,
  // adds
  //   l_1 * r_1 + l_2 * r_2 + ... + l_n * r_n
  // to this.
  //
  // Example:
  //   Variable a = ...;
  //   Variable b = ...;
  //   const std::vector<Variable> left = {a, b};
  //   const std::vector<double> right = {10.0, 2.0};
  //   LinearExpression expr(3.0);
  //   expr.AddInnerProduct(left, right)
  // Results in expr having the value 10.0 * a + 2.0 * b + 3.0.
  //
  // Compile time requirements:
  //  * LeftIterable and RightIterable are both sequences (arrays or objects
  //    with begin() and end())
  //  * For both left and right, their elements a type of either double,
  //    Variable, LinearTerm or LinearExpression (or type implicitly convertible
  //    to one of these types, e.g. int).
  //  * At least one of left or right has elements with type double (or a type
  //    implicitly convertible, e.g. int).
  // Runtime requirements (or CHECK fails):
  //  * left and right have an equal number of elements.
  //
  // Note: The implementation is equivalent to:
  //   for(const auto& [l, r] : zip(left, right)) {
  //     *this += l * r;
  //   }
  // In particular, the multiplication will be performed on the types of the
  // elements in left and right (take care with low precision types), but the
  // addition will always use double precision.
  template <typename LeftIterable, typename RightIterable>
  inline void AddInnerProduct(const LeftIterable& left,
                              const RightIterable& right);

  // Returns the terms in this expression.
  inline const VariableMap<double>& terms() const;
  inline double offset() const;

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values.
  //
  // Will CHECK fail the underlying model is different or if a variable in
  // terms() is missing from variables_values.
  double Evaluate(const VariableMap<double>& variable_values) const;

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values, or zero if missing from the map.
  //
  // Will CHECK fail the underlying model is different.
  double EvaluateWithDefaultZero(
      const VariableMap<double>& variable_values) const;

  inline IndexedModel* model() const;
  inline const absl::flat_hash_map<VariableId, double>& raw_terms() const;

#ifdef USE_LINEAR_EXPRESSION_COUNTERS
  static thread_local int num_calls_default_constructor_;
  static thread_local int num_calls_copy_constructor_;
  static thread_local int num_calls_move_constructor_;
  static thread_local int num_calls_initializer_list_constructor_;
  // Reset all counters in the current thread to 0.
  static void ResetCounters();
#endif  // USE_LINEAR_EXPRESSION_COUNTERS

 private:
  friend LinearExpression operator-(LinearExpression expr);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const LinearExpression& expression);

  VariableMap<double> terms_;
  double offset_ = 0.0;
};

// Returns the sum of the elements of items.
//
// Specifically, letting
//   (i_1, i_2, ..., i_n) = items
// returns
//   i_1 + i_2 + ... + i_n.
//
// Example:
//   Variable a = ...;
//   Variable b = ...;
//   const std::vector<Variable> vars = {a, b, a};
//   Sum(vars)
//     => 2.0 * a + b
// Note, instead of:
//   LinearExpression expr(3.0);
//   expr += Sum(items);
// Prefer:
//   expr.AddSum(items);
//
// See LinearExpression::AddSum() for a precise contract on the type Iterable.
template <typename Iterable>
inline LinearExpression Sum(const Iterable& items);

// Returns the inner product of left and right.
//
// Specifically, letting
//   (l_1, l_2 ..., l_n) = left,
//   (r_1, r_2, ..., r_n) = right,
// returns
//   l_1 * r_1 + l_2 * r_2 + ... + l_n * r_n.
//
// Example:
//   Variable a = ...;
//   Variable b = ...;
//   const std::vector<Variable> left = {a, b};
//   const std::vector<double> right = {10.0, 2.0};
//   InnerProduct(left, right);
//     -=> 10.0 * a + 2.0 * b
// Note, instead of:
//   LinearExpression expr(3.0);
//   expr += InnerProduct(left, right);
// Prefer:
//   expr.AddInnerProduct(left, right);
//
// Requires that left and right have equal size, see
// LinearExpression::AddInnerProduct for a precise contract on template types.
template <typename LeftIterable, typename RightIterable>
inline LinearExpression InnerProduct(const LeftIterable& left,
                                     const RightIterable& right);

std::ostream& operator<<(std::ostream& ostr,
                         const LinearExpression& expression);

// We intentionally pass one of the LinearExpression argument by value so
// that we don't make unnecessary copies of temporary objects by using the move
// constructor and the returned values optimization (RVO).
inline LinearExpression operator-(LinearExpression expr);
inline LinearExpression operator+(Variable lhs, double rhs);
inline LinearExpression operator+(double lhs, Variable rhs);
inline LinearExpression operator+(Variable lhs, Variable rhs);
inline LinearExpression operator+(const LinearTerm& lhs, double rhs);
inline LinearExpression operator+(double lhs, const LinearTerm& rhs);
inline LinearExpression operator+(const LinearTerm& lhs, Variable rhs);
inline LinearExpression operator+(Variable lhs, const LinearTerm& rhs);
inline LinearExpression operator+(const LinearTerm& lhs, const LinearTerm& rhs);
inline LinearExpression operator+(LinearExpression lhs, double rhs);
inline LinearExpression operator+(double lhs, LinearExpression rhs);
inline LinearExpression operator+(LinearExpression lhs, Variable rhs);
inline LinearExpression operator+(Variable lhs, LinearExpression rhs);
inline LinearExpression operator+(LinearExpression lhs, const LinearTerm& rhs);
inline LinearExpression operator+(LinearTerm lhs, LinearExpression rhs);
inline LinearExpression operator+(LinearExpression lhs,
                                  const LinearExpression& rhs);
inline LinearExpression operator-(Variable lhs, double rhs);
inline LinearExpression operator-(double lhs, Variable rhs);
inline LinearExpression operator-(Variable lhs, Variable rhs);
inline LinearExpression operator-(const LinearTerm& lhs, double rhs);
inline LinearExpression operator-(double lhs, const LinearTerm& rhs);
inline LinearExpression operator-(const LinearTerm& lhs, Variable rhs);
inline LinearExpression operator-(Variable lhs, const LinearTerm& rhs);
inline LinearExpression operator-(const LinearTerm& lhs, const LinearTerm& rhs);
inline LinearExpression operator-(LinearExpression lhs, double rhs);
inline LinearExpression operator-(double lhs, LinearExpression rhs);
inline LinearExpression operator-(LinearExpression lhs, Variable rhs);
inline LinearExpression operator-(Variable lhs, LinearExpression rhs);
inline LinearExpression operator-(LinearExpression lhs, const LinearTerm& rhs);
inline LinearExpression operator-(LinearTerm lhs, LinearExpression rhs);
inline LinearExpression operator-(LinearExpression lhs,
                                  const LinearExpression& rhs);
inline LinearExpression operator*(LinearExpression lhs, double rhs);
inline LinearExpression operator*(double lhs, LinearExpression rhs);
inline LinearExpression operator/(LinearExpression lhs, double rhs);

namespace internal {

// The result of the equality comparison between two Variable.
//
// We use an object here to delay the evaluation of equality so that we can use
// the operator== in two use-cases:
//
// 1. when the user want to test that two Variable values references the same
//    variable. This is supported by having this object support implicit
//    conversion to bool.
//
// 2. when the user want to use the equality to create a constraint of equality
//    between two variables.
struct VariablesEquality {
  // Users are not expected to call this constructor. Instead they should only
  // use the overload of `operator==` that returns this when comparing two
  // Variable. For example `x == y`.
  inline VariablesEquality(Variable lhs, Variable rhs);
  inline operator bool() const;  // NOLINT
  Variable lhs;
  Variable rhs;
};

}  // namespace internal

inline internal::VariablesEquality operator==(const Variable& lhs,
                                              const Variable& rhs);
inline bool operator!=(const Variable& lhs, const Variable& rhs);

// A LinearExpression with a lower bound.
struct LowerBoundedLinearExpression {
  // Users are not expected to use this constructor. Instead they should build
  // this object using the overloads of >= and <= operators. For example `x + y
  // >= 3`.
  inline LowerBoundedLinearExpression(LinearExpression expression,
                                      double lower_bound);
  LinearExpression expression;
  double lower_bound;
};

// A LinearExpression with an upper bound.
struct UpperBoundedLinearExpression {
  // Users are not expected to use this constructor. Instead they should build
  // this object using the overloads of >= and <= operators. For example `x + y
  // <= 3`.
  inline UpperBoundedLinearExpression(LinearExpression expression,
                                      double upper_bound);
  LinearExpression expression;
  double upper_bound;
};

// A LinearExpression with upper and lower bounds.
struct BoundedLinearExpression {
  // Users are not expected to use this constructor. Instead they should build
  // this object using the overloads of >= and <= operators. For example `3 <= x
  // + y <= 3`.
  inline BoundedLinearExpression(LinearExpression expression,
                                 double lower_bound, double upper_bound);
  // Users are not expected to use this constructor. This implicit conversion
  // will be used where a BoundedLinearExpression is expected and the user uses
  // == comparison of two variables. For example `AddLinearConstraint(x == y);`.
  inline BoundedLinearExpression(  // NOLINT
      const internal::VariablesEquality& eq);
  inline BoundedLinearExpression(  // NOLINT
      LowerBoundedLinearExpression lb_expression);
  inline BoundedLinearExpression(  // NOLINT
      UpperBoundedLinearExpression ub_expression);

  // Returns the actual lower_bound after taking into account the linear
  // expression offset.
  inline double lower_bound_minus_offset() const;
  // Returns the actual upper_bound after taking into account the linear
  // expression offset.
  inline double upper_bound_minus_offset() const;

  LinearExpression expression;
  double lower_bound;
  double upper_bound;
};

std::ostream& operator<<(std::ostream& ostr,
                         const BoundedLinearExpression& bounded_expression);

// We intentionally pass the LinearExpression argument by value so that we don't
// make unnecessary copies of temporary objects by using the move constructor
// and the returned values optimization (RVO).
inline LowerBoundedLinearExpression operator>=(LinearExpression expression,
                                               double constant);
inline LowerBoundedLinearExpression operator<=(double constant,
                                               LinearExpression expression);
inline LowerBoundedLinearExpression operator>=(const LinearTerm& term,
                                               double constant);
inline LowerBoundedLinearExpression operator<=(double constant,
                                               const LinearTerm& term);
inline LowerBoundedLinearExpression operator>=(Variable variable,
                                               double constant);
inline LowerBoundedLinearExpression operator<=(double constant,
                                               Variable variable);
inline UpperBoundedLinearExpression operator<=(LinearExpression expression,
                                               double constant);
inline UpperBoundedLinearExpression operator>=(double constant,
                                               LinearExpression expression);
inline UpperBoundedLinearExpression operator<=(const LinearTerm& term,
                                               double constant);
inline UpperBoundedLinearExpression operator>=(double constant,
                                               const LinearTerm& term);
inline UpperBoundedLinearExpression operator<=(Variable variable,
                                               double constant);
inline UpperBoundedLinearExpression operator>=(double constant,
                                               Variable variable);

// We intentionally pass the UpperBoundedLinearExpression and
// LowerBoundedLinearExpression arguments by value so that we don't
// make unnecessary copies of temporary objects by using the move constructor
// and the returned values optimization (RVO).
inline BoundedLinearExpression operator<=(LowerBoundedLinearExpression lhs,
                                          double rhs);
inline BoundedLinearExpression operator>=(double lhs,
                                          LowerBoundedLinearExpression rhs);
inline BoundedLinearExpression operator>=(UpperBoundedLinearExpression lhs,
                                          double rhs);
inline BoundedLinearExpression operator<=(double lhs,
                                          UpperBoundedLinearExpression rhs);
// We intentionally pass one LinearExpression argument by value so that we don't
// make unnecessary copies of temporary objects by using the move constructor
// and the returned values optimization (RVO).
inline BoundedLinearExpression operator<=(LinearExpression lhs,
                                          const LinearExpression& rhs);
inline BoundedLinearExpression operator>=(LinearExpression lhs,
                                          const LinearExpression& rhs);
inline BoundedLinearExpression operator<=(LinearExpression lhs,
                                          const LinearTerm& rhs);
inline BoundedLinearExpression operator>=(LinearExpression lhs,
                                          const LinearTerm& rhs);
inline BoundedLinearExpression operator<=(const LinearTerm& lhs,
                                          LinearExpression rhs);
inline BoundedLinearExpression operator>=(const LinearTerm& lhs,
                                          LinearExpression rhs);
inline BoundedLinearExpression operator<=(LinearExpression lhs, Variable rhs);
inline BoundedLinearExpression operator>=(LinearExpression lhs, Variable rhs);
inline BoundedLinearExpression operator<=(Variable lhs, LinearExpression rhs);
inline BoundedLinearExpression operator>=(Variable lhs, LinearExpression rhs);
inline BoundedLinearExpression operator<=(const LinearTerm& lhs,
                                          const LinearTerm& rhs);
inline BoundedLinearExpression operator>=(const LinearTerm& lhs,
                                          const LinearTerm& rhs);
inline BoundedLinearExpression operator<=(const LinearTerm& lhs, Variable rhs);
inline BoundedLinearExpression operator>=(const LinearTerm& lhs, Variable rhs);
inline BoundedLinearExpression operator<=(Variable lhs, const LinearTerm& rhs);
inline BoundedLinearExpression operator>=(Variable lhs, const LinearTerm& rhs);
inline BoundedLinearExpression operator<=(Variable lhs, Variable rhs);
inline BoundedLinearExpression operator>=(Variable lhs, Variable rhs);
inline BoundedLinearExpression operator==(LinearExpression lhs,
                                          const LinearExpression& rhs);
inline BoundedLinearExpression operator==(LinearExpression lhs,
                                          const LinearTerm& rhs);
inline BoundedLinearExpression operator==(const LinearTerm& lhs,
                                          LinearExpression rhs);
inline BoundedLinearExpression operator==(LinearExpression lhs, Variable rhs);
inline BoundedLinearExpression operator==(Variable lhs, LinearExpression rhs);
inline BoundedLinearExpression operator==(LinearExpression lhs, double rhs);
inline BoundedLinearExpression operator==(double lhs, LinearExpression rhs);
inline BoundedLinearExpression operator==(const LinearTerm& lhs,
                                          const LinearTerm& rhs);
inline BoundedLinearExpression operator==(const LinearTerm& lhs, Variable rhs);
inline BoundedLinearExpression operator==(Variable lhs, const LinearTerm& rhs);
inline BoundedLinearExpression operator==(const LinearTerm& lhs, double rhs);
inline BoundedLinearExpression operator==(double lhs, const LinearTerm& rhs);
inline BoundedLinearExpression operator==(Variable lhs, double rhs);
inline BoundedLinearExpression operator==(double lhs, Variable rhs);

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Variable
////////////////////////////////////////////////////////////////////////////////

Variable::Variable(IndexedModel* const model, const VariableId id)
    : model_(model), id_(id) {
  DCHECK(model != nullptr);
}

int64_t Variable::id() const { return id_.value(); }

VariableId Variable::typed_id() const { return id_; }

IndexedModel* Variable::model() const { return model_; }

double Variable::lower_bound() const {
  return model_->variable_lower_bound(id_);
}
double Variable::upper_bound() const {
  return model_->variable_upper_bound(id_);
}
bool Variable::is_integer() const { return model_->is_variable_integer(id_); }
const std::string& Variable::name() const { return model_->variable_name(id_); }

void Variable::set_lower_bound(const double lower_bound) const {
  model_->set_variable_lower_bound(id_, lower_bound);
}
void Variable::set_upper_bound(const double upper_bound) const {
  model_->set_variable_upper_bound(id_, upper_bound);
}
void Variable::set_is_integer(const bool is_integer) const {
  model_->set_variable_is_integer(id_, is_integer);
}
void Variable::set_integer() const { set_is_integer(true); }
void Variable::set_continuous() const { set_is_integer(false); }

template <typename H>
H AbslHashValue(H h, const Variable& variable) {
  return H::combine(std::move(h), variable.id_.value(), variable.model_);
}

std::ostream& operator<<(std::ostream& ostr, const Variable& variable) {
  // TODO(b/170992529): handle the case of empty variable name and quoting when
  // the variable name contains invalid characters.
  ostr << variable.name();
  return ostr;
}

////////////////////////////////////////////////////////////////////////////////
// LinearTerm
////////////////////////////////////////////////////////////////////////////////

LinearTerm::LinearTerm(Variable variable, const double coefficient)
    : variable(std::move(variable)), coefficient(coefficient) {}

LinearTerm LinearTerm::operator-() const {
  return LinearTerm(variable, -coefficient);
}

LinearTerm& LinearTerm::operator*=(const double d) {
  coefficient *= d;
  return *this;
}

LinearTerm& LinearTerm::operator/=(const double d) {
  coefficient /= d;
  return *this;
}

LinearTerm operator*(const double coefficient, LinearTerm term) {
  term *= coefficient;
  return term;
}

LinearTerm operator*(LinearTerm term, const double coefficient) {
  term *= coefficient;
  return term;
}

LinearTerm operator*(const double coefficient, Variable variable) {
  return LinearTerm(std::move(variable), coefficient);
}

LinearTerm operator*(Variable variable, const double coefficient) {
  return LinearTerm(std::move(variable), coefficient);
}

LinearTerm operator/(LinearTerm term, const double coefficient) {
  term /= coefficient;
  return term;
}

LinearTerm operator/(Variable variable, const double coefficient) {
  return LinearTerm(std::move(variable), 1 / coefficient);
}

////////////////////////////////////////////////////////////////////////////////
// LinearExpression
////////////////////////////////////////////////////////////////////////////////

LinearExpression::LinearExpression(std::initializer_list<LinearTerm> terms,
                                   const double offset)
    : offset_(offset) {
#ifdef USE_LINEAR_EXPRESSION_COUNTERS
  ++num_calls_initializer_list_constructor_;
#endif  // USE_LINEAR_EXPRESSION_COUNTERS
  for (const auto& term : terms) {
    // The same variable may appear multiple times in the input list; we must
    // accumulate the coefficients.
    terms_[term.variable] += term.coefficient;
  }
}

LinearExpression::LinearExpression(double offset)
    : LinearExpression({}, offset) {}

LinearExpression::LinearExpression(Variable variable)
    : LinearExpression({LinearTerm(variable, 1.0)}, 0.0) {}

LinearExpression::LinearExpression(const LinearTerm& term)
    : LinearExpression({term}, 0.0) {}

LinearExpression operator-(LinearExpression expr) {
  expr.offset_ = -expr.offset_;
  for (auto term : expr.terms_) {
    term.second = -term.second;
  }
  return expr;
}

LinearExpression operator+(const Variable lhs, const double rhs) {
  return LinearTerm(lhs, 1.0) + rhs;
}

LinearExpression operator+(const double lhs, const Variable rhs) {
  return lhs + LinearTerm(rhs, 1.0);
}

LinearExpression operator+(const Variable lhs, const Variable rhs) {
  return LinearTerm(lhs, 1.0) + LinearTerm(rhs, 1.0);
}

LinearExpression operator+(const LinearTerm& lhs, const double rhs) {
  return LinearExpression({lhs}, rhs);
}

LinearExpression operator+(const double lhs, const LinearTerm& rhs) {
  return LinearExpression({rhs}, lhs);
}

LinearExpression operator+(const LinearTerm& lhs, const Variable rhs) {
  return lhs + LinearTerm(rhs, 1.0);
}

LinearExpression operator+(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) + rhs;
}

LinearExpression operator+(const LinearTerm& lhs, const LinearTerm& rhs) {
  return LinearExpression({lhs, rhs}, 0);
}

LinearExpression operator+(LinearExpression lhs, const double rhs) {
  lhs += rhs;
  return lhs;
}

LinearExpression operator+(const double lhs, LinearExpression rhs) {
  rhs += lhs;
  return rhs;
}

LinearExpression operator+(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) + LinearTerm(rhs, 1.0);
}

LinearExpression operator+(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) + std::move(rhs);
}

LinearExpression operator+(LinearExpression lhs, const LinearTerm& rhs) {
  lhs += rhs;
  return lhs;
}

LinearExpression operator+(LinearTerm lhs, LinearExpression rhs) {
  rhs += lhs;
  return rhs;
}

LinearExpression operator+(LinearExpression lhs, const LinearExpression& rhs) {
  lhs += rhs;
  return lhs;
}

LinearExpression operator-(const Variable lhs, const double rhs) {
  return LinearTerm(lhs, 1.0) - rhs;
}

LinearExpression operator-(const double lhs, const Variable rhs) {
  return lhs - LinearTerm(rhs, 1.0);
}

LinearExpression operator-(const Variable lhs, const Variable rhs) {
  return LinearTerm(lhs, 1.0) - LinearTerm(rhs, 1.0);
}

LinearExpression operator-(const LinearTerm& lhs, const double rhs) {
  return LinearExpression({lhs}, -rhs);
}

LinearExpression operator-(const double lhs, const LinearTerm& rhs) {
  return LinearExpression({-rhs}, lhs);
}

LinearExpression operator-(const LinearTerm& lhs, const Variable rhs) {
  return lhs - LinearTerm(rhs, 1.0);
}

LinearExpression operator-(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) - rhs;
}

LinearExpression operator-(const LinearTerm& lhs, const LinearTerm& rhs) {
  return LinearExpression({lhs, -rhs}, 0);
}

LinearExpression operator-(LinearExpression lhs, const double rhs) {
  lhs -= rhs;
  return lhs;
}

LinearExpression operator-(const double lhs, LinearExpression rhs) {
  auto ret = -std::move(rhs);
  ret += lhs;
  return ret;
}

LinearExpression operator-(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) - LinearTerm(rhs, 1.0);
}

LinearExpression operator-(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) - std::move(rhs);
}

LinearExpression operator-(LinearExpression lhs, const LinearTerm& rhs) {
  lhs -= rhs;
  return lhs;
}

LinearExpression operator-(LinearTerm lhs, LinearExpression rhs) {
  auto ret = -std::move(rhs);
  ret += lhs;
  return ret;
}

LinearExpression operator-(LinearExpression lhs, const LinearExpression& rhs) {
  lhs -= rhs;
  return lhs;
}

LinearExpression operator*(LinearExpression lhs, const double rhs) {
  lhs *= rhs;
  return lhs;
}

LinearExpression operator*(const double lhs, LinearExpression rhs) {
  rhs *= lhs;
  return rhs;
}

LinearExpression operator/(LinearExpression lhs, const double rhs) {
  lhs /= rhs;
  return lhs;
}

LinearExpression& LinearExpression::operator+=(const LinearExpression& other) {
  terms_.Add(other.terms_);
  offset_ += other.offset_;
  return *this;
}

LinearExpression& LinearExpression::operator+=(const LinearTerm& term) {
  terms_[term.variable] += term.coefficient;
  return *this;
}

LinearExpression& LinearExpression::operator+=(const Variable variable) {
  return *this += LinearTerm(variable, 1.0);
}

LinearExpression& LinearExpression::operator+=(const double value) {
  offset_ += value;
  return *this;
}

LinearExpression& LinearExpression::operator-=(const LinearExpression& other) {
  terms_.Subtract(other.terms_);
  offset_ -= other.offset_;
  return *this;
}

LinearExpression& LinearExpression::operator-=(const LinearTerm& term) {
  terms_[term.variable] -= term.coefficient;
  return *this;
}

LinearExpression& LinearExpression::operator-=(const Variable variable) {
  return *this -= LinearTerm(variable, 1.0);
}

LinearExpression& LinearExpression::operator-=(const double value) {
  offset_ -= value;
  return *this;
}

LinearExpression& LinearExpression::operator*=(const double value) {
  offset_ *= value;
  for (auto term : terms_) {
    term.second *= value;
  }
  return *this;
}

LinearExpression& LinearExpression::operator/=(const double value) {
  offset_ /= value;
  for (auto term : terms_) {
    term.second /= value;
  }
  return *this;
}

template <typename Iterable>
void LinearExpression::AddSum(const Iterable& items) {
  for (const auto& item : items) {
    *this += item;
  }
}

template <typename Iterable>
LinearExpression Sum(const Iterable& items) {
  LinearExpression result;
  result.AddSum(items);
  return result;
}

template <typename LeftIterable, typename RightIterable>
void LinearExpression::AddInnerProduct(const LeftIterable& left,
                                       const RightIterable& right) {
  using std::begin;
  using std::end;
  auto l = begin(left);
  auto r = begin(right);
  auto l_end = end(left);
  auto r_end = end(right);
  for (; l != l_end && r != r_end; ++l, ++r) {
    *this += (*l) * (*r);
  }
  CHECK(l == l_end)
      << "left had more elements than right, sizes should be equal";
  CHECK(r == r_end)
      << "right had more elements than left, sizes should be equal";
}

template <typename LeftIterable, typename RightIterable>
LinearExpression InnerProduct(const LeftIterable& left,
                              const RightIterable& right) {
  LinearExpression result;
  result.AddInnerProduct(left, right);
  return result;
}

const VariableMap<double>& LinearExpression::terms() const { return terms_; }

double LinearExpression::offset() const { return offset_; }

IndexedModel* LinearExpression::model() const { return terms_.model(); }

const absl::flat_hash_map<VariableId, double>& LinearExpression::raw_terms()
    const {
  return terms_.raw_map();
}

////////////////////////////////////////////////////////////////////////////////
// VariablesEquality
////////////////////////////////////////////////////////////////////////////////

namespace internal {

VariablesEquality::VariablesEquality(Variable lhs, Variable rhs)
    : lhs(std::move(lhs)), rhs(std::move(rhs)) {}

inline VariablesEquality::operator bool() const {
  return lhs.typed_id() == rhs.typed_id() && lhs.model() == rhs.model();
}

}  // namespace internal

internal::VariablesEquality operator==(const Variable& lhs,
                                       const Variable& rhs) {
  return internal::VariablesEquality(lhs, rhs);
}

bool operator!=(const Variable& lhs, const Variable& rhs) {
  return !(lhs == rhs);
}

/////////////////////////////////////////////////////////////////////////////////
// LowerBoundedLinearExpression
// UpperBoundedLinearExpression
// BoundedLinearExpression
////////////////////////////////////////////////////////////////////////////////

LowerBoundedLinearExpression::LowerBoundedLinearExpression(
    LinearExpression expression, const double lower_bound)
    : expression(std::move(expression)), lower_bound(lower_bound) {}

UpperBoundedLinearExpression::UpperBoundedLinearExpression(
    LinearExpression expression, const double upper_bound)
    : expression(std::move(expression)), upper_bound(upper_bound) {}

BoundedLinearExpression::BoundedLinearExpression(LinearExpression expression,
                                                 const double lower_bound,
                                                 const double upper_bound)
    : expression(std::move(expression)),
      lower_bound(lower_bound),
      upper_bound(upper_bound) {}

BoundedLinearExpression::BoundedLinearExpression(
    const internal::VariablesEquality& eq)
    : expression({{eq.lhs, 1.0}, {eq.rhs, -1.0}}, 0.0),
      lower_bound(0.0),
      upper_bound(0.0) {}

BoundedLinearExpression::BoundedLinearExpression(
    LowerBoundedLinearExpression lb_expression)
    : expression(std::move(lb_expression.expression)),
      lower_bound(lb_expression.lower_bound),
      upper_bound(std::numeric_limits<double>::infinity()) {}

BoundedLinearExpression::BoundedLinearExpression(
    UpperBoundedLinearExpression ub_expression)
    : expression(std::move(ub_expression.expression)),
      lower_bound(-std::numeric_limits<double>::infinity()),
      upper_bound(ub_expression.upper_bound) {}

double BoundedLinearExpression::lower_bound_minus_offset() const {
  return lower_bound - expression.offset();
}

double BoundedLinearExpression::upper_bound_minus_offset() const {
  return upper_bound - expression.offset();
}

LowerBoundedLinearExpression operator>=(LinearExpression expression,
                                        const double constant) {
  return LowerBoundedLinearExpression(std::move(expression), constant);
}

LowerBoundedLinearExpression operator<=(const double constant,
                                        LinearExpression expression) {
  return LowerBoundedLinearExpression(std::move(expression), constant);
}

LowerBoundedLinearExpression operator>=(const LinearTerm& term,
                                        const double constant) {
  return LowerBoundedLinearExpression(LinearExpression({term}, 0.0), constant);
}

LowerBoundedLinearExpression operator<=(const double constant,
                                        const LinearTerm& term) {
  return LowerBoundedLinearExpression(LinearExpression({term}, 0.0), constant);
}

LowerBoundedLinearExpression operator>=(const Variable variable,
                                        const double constant) {
  return LinearTerm(variable, 1.0) >= constant;
}

LowerBoundedLinearExpression operator<=(const double constant,
                                        const Variable variable) {
  return constant <= LinearTerm(variable, 1.0);
}

UpperBoundedLinearExpression operator<=(LinearExpression expression,
                                        const double constant) {
  return UpperBoundedLinearExpression(std::move(expression), constant);
}

UpperBoundedLinearExpression operator>=(const double constant,
                                        LinearExpression expression) {
  return UpperBoundedLinearExpression(std::move(expression), constant);
}

UpperBoundedLinearExpression operator<=(const LinearTerm& term,
                                        const double constant) {
  return UpperBoundedLinearExpression(LinearExpression({term}, 0.0), constant);
}

UpperBoundedLinearExpression operator>=(const double constant,
                                        const LinearTerm& term) {
  return UpperBoundedLinearExpression(LinearExpression({term}, 0.0), constant);
}

UpperBoundedLinearExpression operator<=(const Variable variable,
                                        const double constant) {
  return LinearTerm(variable, 1.0) <= constant;
}

UpperBoundedLinearExpression operator>=(const double constant,
                                        const Variable variable) {
  return constant >= LinearTerm(variable, 1.0);
}

BoundedLinearExpression operator<=(LowerBoundedLinearExpression lhs,
                                   const double rhs) {
  return BoundedLinearExpression(std::move(lhs.expression),
                                 /*lower_bound=*/lhs.lower_bound,
                                 /*upper_bound=*/rhs);
}

BoundedLinearExpression operator>=(const double lhs,
                                   LowerBoundedLinearExpression rhs) {
  return BoundedLinearExpression(std::move(rhs.expression),
                                 /*lower_bound=*/rhs.lower_bound,
                                 /*upper_bound=*/lhs);
}

BoundedLinearExpression operator>=(UpperBoundedLinearExpression lhs,
                                   const double rhs) {
  return BoundedLinearExpression(std::move(lhs.expression),
                                 /*lower_bound=*/rhs,
                                 /*upper_bound=*/lhs.upper_bound);
}

BoundedLinearExpression operator<=(const double lhs,
                                   UpperBoundedLinearExpression rhs) {
  return BoundedLinearExpression(std::move(rhs.expression),
                                 /*lower_bound=*/lhs,
                                 /*upper_bound=*/rhs.upper_bound);
}

BoundedLinearExpression operator<=(LinearExpression lhs,
                                   const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(
      std::move(lhs), /*lower_bound=*/-std::numeric_limits<double>::infinity(),
      /*upper_bound=*/0.0);
}

BoundedLinearExpression operator>=(LinearExpression lhs,
                                   const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(
      std::move(lhs), /*lower_bound=*/0.0,
      /*upper_bound=*/std::numeric_limits<double>::infinity());
}

BoundedLinearExpression operator<=(LinearExpression lhs,
                                   const LinearTerm& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(
      std::move(lhs), /*lower_bound=*/-std::numeric_limits<double>::infinity(),
      /*upper_bound=*/0.0);
}

BoundedLinearExpression operator>=(LinearExpression lhs,
                                   const LinearTerm& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(
      std::move(lhs), /*lower_bound=*/0.0,
      /*upper_bound=*/std::numeric_limits<double>::infinity());
}

BoundedLinearExpression operator<=(const LinearTerm& lhs,
                                   LinearExpression rhs) {
  rhs -= lhs;
  return BoundedLinearExpression(
      std::move(rhs), /*lower_bound=*/0.0,
      /*upper_bound=*/std::numeric_limits<double>::infinity());
}

BoundedLinearExpression operator>=(const LinearTerm& lhs,
                                   LinearExpression rhs) {
  rhs -= lhs;
  return BoundedLinearExpression(
      std::move(rhs), /*lower_bound=*/-std::numeric_limits<double>::infinity(),
      /*upper_bound=*/0.0);
}

BoundedLinearExpression operator<=(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) <= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator>=(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) >= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator<=(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) <= std::move(rhs);
}

BoundedLinearExpression operator>=(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) >= std::move(rhs);
}

BoundedLinearExpression operator<=(const LinearTerm& lhs,
                                   const LinearTerm& rhs) {
  return BoundedLinearExpression(
      LinearExpression({lhs, -rhs}, 0.0),
      /*lower_bound=*/-std::numeric_limits<double>::infinity(),
      /*upper_bound=*/0.0);
}

BoundedLinearExpression operator>=(const LinearTerm& lhs,
                                   const LinearTerm& rhs) {
  return BoundedLinearExpression(
      LinearExpression({lhs, -rhs}, 0.0), /*lower_bound=*/0.0,
      /*upper_bound=*/std::numeric_limits<double>::infinity());
}

BoundedLinearExpression operator<=(const LinearTerm& lhs, const Variable rhs) {
  return lhs <= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator>=(const LinearTerm& lhs, const Variable rhs) {
  return lhs >= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator<=(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) <= rhs;
}

BoundedLinearExpression operator>=(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) >= rhs;
}

BoundedLinearExpression operator<=(const Variable lhs, const Variable rhs) {
  return LinearTerm(lhs, 1.0) <= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator>=(const Variable lhs, const Variable rhs) {
  return LinearTerm(lhs, 1.0) >= LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator==(LinearExpression lhs,
                                   const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(std::move(lhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(LinearExpression lhs,
                                   const LinearTerm& rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(std::move(lhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const LinearTerm& lhs,
                                   LinearExpression rhs) {
  rhs -= lhs;
  return BoundedLinearExpression(std::move(rhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(LinearExpression lhs, const Variable rhs) {
  return std::move(lhs) == LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator==(const Variable lhs, LinearExpression rhs) {
  return LinearTerm(lhs, 1.0) == std::move(rhs);
}

BoundedLinearExpression operator==(LinearExpression lhs, const double rhs) {
  lhs -= rhs;
  return BoundedLinearExpression(std::move(lhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const double lhs, LinearExpression rhs) {
  rhs -= lhs;
  return BoundedLinearExpression(std::move(rhs), /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const LinearTerm& lhs,
                                   const LinearTerm& rhs) {
  return BoundedLinearExpression(LinearExpression({lhs, -rhs}, 0.0),
                                 /*lower_bound=*/0.0,
                                 /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const LinearTerm& lhs, const Variable rhs) {
  return lhs == LinearTerm(rhs, 1.0);
}

BoundedLinearExpression operator==(const Variable lhs, const LinearTerm& rhs) {
  return LinearTerm(lhs, 1.0) == rhs;
}

BoundedLinearExpression operator==(const LinearTerm& lhs, const double rhs) {
  return BoundedLinearExpression(LinearExpression({lhs}, -rhs),
                                 /*lower_bound=*/0.0, /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const double lhs, const LinearTerm& rhs) {
  return BoundedLinearExpression(LinearExpression({rhs}, -lhs),
                                 /*lower_bound=*/0.0, /*upper_bound=*/0.0);
}

BoundedLinearExpression operator==(const Variable lhs, const double rhs) {
  return LinearTerm(lhs, 1.0) == rhs;
}

BoundedLinearExpression operator==(const double lhs, const Variable rhs) {
  return lhs == LinearTerm(rhs, 1.0);
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_VARIABLE_AND_EXPRESSIONS_H_
