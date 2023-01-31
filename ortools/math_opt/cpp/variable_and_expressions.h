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

// IWYU pragma: private, include "ortools/math_opt/cpp/math_opt.h"
// IWYU pragma: friend "ortools/math_opt/cpp/.*"

// An object oriented wrapper for variables in ModelStorage (used internally by
// Model) with support for arithmetic operations to build linear expressions and
// express linear constraints.
//
// Types are:
//   - Variable: a reference to a variable of an ModelStorage.
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
//   - QuadraticTermKey: a key used internally to represent a pair of Variables.
//
//   - QuadraticTerm: a term representing the product of a scalar coefficient
//     and two Variables (possibly the same); something like `2*x*y` or `3*x*x`.
//     It is used as an intermediate in the arithmetic operations that build
//     quadratic expressions.
//
//   - QuadraticExpression: a sum of a quadratic terms, linear terms, and a
//     scalar offset; something like `3*x*y + 2*x*x + 4x + 5`.
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
#include <ostream>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/log/check.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/cpp/id_map.h"     // IWYU pragma: export
#include "ortools/math_opt/cpp/key_types.h"  // IWYU pragma: export
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research {
namespace math_opt {

// Forward declaration needed by Variable.
class LinearExpression;

// A value type that references a variable from ModelStorage. Usually this type
// is passed by copy.
class Variable {
 public:
  // The typed integer used for ids.
  using IdType = VariableId;

  // Usually users will obtain variables using Model::AddVariable(). There
  // should be little for users to build this object from an ModelStorage.
  inline Variable(const ModelStorage* storage, VariableId id);

  // Each call to AddVariable will produce Variables id() increasing by one,
  // starting at zero. Deleted ids are NOT reused. Thus, if no variables are
  // deleted, the ids in the model will be consecutive.
  inline int64_t id() const;

  inline VariableId typed_id() const;
  inline const ModelStorage* storage() const;

  inline double lower_bound() const;
  inline double upper_bound() const;
  inline bool is_integer() const;
  inline absl::string_view name() const;

  template <typename H>
  friend H AbslHashValue(H h, const Variable& variable);
  friend std::ostream& operator<<(std::ostream& ostr, const Variable& variable);

  inline LinearExpression operator-() const;

 private:
  const ModelStorage* storage_;
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

// Forward declaration so that we may add it as a friend to LinearExpression
class QuadraticExpression;

// This class represents a sum of variables multiplied by coefficient and an
// optional offset constant. For example: "3*x + 2*y + 5".
//
// All operations, including constructor, will raise an assertion if the
// operands involve variables from different Model objects.
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
#ifndef MATH_OPT_USE_EXPRESSION_COUNTERS
  LinearExpression() = default;
#else   // MATH_OPT_USE_EXPRESSION_COUNTERS
  LinearExpression();
  LinearExpression(const LinearExpression& other);
  LinearExpression(LinearExpression&& other);
  LinearExpression& operator=(const LinearExpression& other);
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
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
  //   const Variable a = ...;
  //   const Variable b = ...;
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

  // Creates a new LinearExpression object equal to the sum. The implementation
  // is equivalent to:
  //   LinearExpression expr;
  //   expr.AddSum(items);
  template <typename Iterable>
  static inline LinearExpression Sum(const Iterable& items);

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
  //   const Variable a = ...;
  //   const Variable b = ...;
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
  // Note: The implementation is equivalent to the following pseudocode:
  //   for(const auto& [l, r] : zip(left, right)) {
  //     *this += l * r;
  //   }
  // In particular, the multiplication will be performed on the types of the
  // elements in left and right (take care with low precision types), but the
  // addition will always use double precision.
  template <typename LeftIterable, typename RightIterable>
  inline void AddInnerProduct(const LeftIterable& left,
                              const RightIterable& right);

  // Creates a new LinearExpression object equal to the inner product. The
  // implementation is equivalent to:
  //   LinearExpression expr;
  //   expr.AddInnerProduct(left, right);
  template <typename LeftIterable, typename RightIterable>
  static inline LinearExpression InnerProduct(const LeftIterable& left,
                                              const RightIterable& right);

  // Returns the terms in this expression.
  inline const VariableMap<double>& terms() const;
  inline double offset() const;

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values.
  //
  // Will CHECK fail the underlying model storage is different or if a variable
  // in terms() is missing from variables_values.
  double Evaluate(const VariableMap<double>& variable_values) const;

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values, or zero if missing from the map.
  //
  // Will CHECK fail the underlying model storage is different.
  double EvaluateWithDefaultZero(
      const VariableMap<double>& variable_values) const;

  inline const ModelStorage* storage() const;
  inline const absl::flat_hash_map<VariableId, double>& raw_terms() const;

#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  static thread_local int num_calls_default_constructor_;
  static thread_local int num_calls_copy_constructor_;
  static thread_local int num_calls_move_constructor_;
  static thread_local int num_calls_initializer_list_constructor_;
  // Reset all counters in the current thread to 0.
  static void ResetCounters();
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS

 private:
  friend LinearExpression operator-(LinearExpression expr);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const LinearExpression& expression);
  friend QuadraticExpression;

  VariableMap<double> terms_;
  double offset_ = 0.0;
};

// Returns the sum of the elements of items as a LinearExpression.
//
// Specifically, letting
//   (i_1, i_2, ..., i_n) = items
// returns
//   i_1 + i_2 + ... + i_n.
//
// Example:
//   const Variable a = ...;
//   const Variable b = ...;
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
//
// If the inner product cannot be represented as a LinearExpression, consider
// instead QuadraticExpression::Sum().
template <typename Iterable>
inline LinearExpression Sum(const Iterable& items);

// Returns the inner product of left and right as a LinearExpression.
//
// Specifically, letting
//   (l_1, l_2 ..., l_n) = left,
//   (r_1, r_2, ..., r_n) = right,
// returns
//   l_1 * r_1 + l_2 * r_2 + ... + l_n * r_n.
//
// Example:
//   const Variable a = ...;
//   const Variable b = ...;
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
//
// If the inner product cannot be represented as a LinearExpression, consider
// instead QuadraticExpression::InnerProduct().
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
  // Users are not expected to use this constructor. Instead, they should build
  // this object using overloads of the >= and <= operators. For example, `x + y
  // >= 3`.
  inline LowerBoundedLinearExpression(LinearExpression expression,
                                      double lower_bound);
  LinearExpression expression;
  double lower_bound;
};

// A LinearExpression with an upper bound.
struct UpperBoundedLinearExpression {
  // Users are not expected to use this constructor. Instead they should build
  // this object using overloads of the >= and <= operators. For example, `x + y
  // <= 3`.
  inline UpperBoundedLinearExpression(LinearExpression expression,
                                      double upper_bound);
  LinearExpression expression;
  double upper_bound;
};

// A LinearExpression with upper and lower bounds.
struct BoundedLinearExpression {
  // Users are not expected to use this constructor. Instead they should build
  // this object using overloads of the >=, <=, and == operators. For example,
  // `3 <= x + y <= 3`.
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

// Id type used for quadratic terms, i.e. products of two variables.
using QuadraticProductId = std::pair<VariableId, VariableId>;

// Couples a QuadraticProductId with a ModelStorage, for use with IdMaps.
// Namely, this key type satisfies the requirements stated in key_types.h.
// Invariant:
//   * variable_ids_.first <= variable_ids_.second. The constructor will
//     silently correct this if not satisfied by the inputs.
class QuadraticTermKey {
 public:
  // NOTE: this definition is for use by IdMap; clients should not rely upon it.
  using IdType = QuadraticProductId;

  // NOTE: This constructor will silently re-order the passed id so that, upon
  // exiting the constructor, variable_ids_.first <= variable_ids_.second.
  inline QuadraticTermKey(const ModelStorage* storage, QuadraticProductId id);
  // NOTE: This constructor will CHECK fail if the variable models do not agree,
  // i.e. first_variable.storage() != second_variable.storage(). It will also
  // silently re-order the passed id so that, upon exiting the constructor,
  // variable_ids_.first <= variable_ids_.second.
  inline QuadraticTermKey(Variable first_variable, Variable second_variable);

  inline QuadraticProductId typed_id() const;
  inline const ModelStorage* storage() const;

  // Returns the Variable with the smallest id.
  Variable first() const { return Variable(storage_, variable_ids_.first); }

  // Returns the Variable the largest id.
  Variable second() const { return Variable(storage_, variable_ids_.second); }

  template <typename H>
  friend H AbslHashValue(H h, const QuadraticTermKey& key);

 private:
  const ModelStorage* storage_;
  QuadraticProductId variable_ids_;
};

inline std::ostream& operator<<(std::ostream& ostr,
                                const QuadraticTermKey& key);

inline bool operator==(const QuadraticTermKey lhs, const QuadraticTermKey rhs);
inline bool operator!=(const QuadraticTermKey lhs, const QuadraticTermKey rhs);

// Represents a quadratic term in a sum: coefficient * variable_1 * variable_2.
// Invariant:
//   * first_variable.storage() == second_variable.storage(). The constructor
//     will CHECK fail if not satisfied.
class QuadraticTerm {
 public:
  QuadraticTerm() = delete;
  // NOTE: This will CHECK fail if
  // first_variable.storage() != second_variable.storage().
  inline QuadraticTerm(Variable first_variable, Variable second_variable,
                       double coefficient);

  inline double coefficient() const;
  inline Variable first_variable() const;
  inline Variable second_variable() const;

  // This is useful for working with IdMaps
  inline QuadraticTermKey GetKey() const;

  inline QuadraticTerm& operator*=(double value);
  inline QuadraticTerm& operator/=(double value);

 private:
  friend QuadraticTerm operator-(QuadraticTerm term);
  friend QuadraticTerm operator*(double lhs, QuadraticTerm rhs);
  friend QuadraticTerm operator*(QuadraticTerm lhs, double rhs);
  friend QuadraticTerm operator/(QuadraticTerm lhs, double rhs);

  Variable first_variable_;
  Variable second_variable_;
  double coefficient_;
};
// We declare those operator overloads that result in a QuadraticTerm, stated in
// lexicographic ordering based on lhs type, rhs type):
inline QuadraticTerm operator-(QuadraticTerm term);
inline QuadraticTerm operator*(double lhs, QuadraticTerm rhs);
inline QuadraticTerm operator*(Variable lhs, Variable rhs);
inline QuadraticTerm operator*(Variable lhs, LinearTerm rhs);
inline QuadraticTerm operator*(LinearTerm lhs, Variable rhs);
inline QuadraticTerm operator*(LinearTerm lhs, LinearTerm rhs);
inline QuadraticTerm operator*(QuadraticTerm lhs, double rhs);
inline QuadraticTerm operator/(QuadraticTerm lhs, double rhs);

// Implements the API of std::unordered_map<QuadraticTermKey, V>, but forbids
// QuadraticTermKeys from different models in the same map.
template <typename V>
using QuadraticTermMap = IdMap<QuadraticTermKey, V>;

// This class represents a sum of quadratic terms, linear terms, and constant
// offset. For example: "3*x*y + 2*x + 1".
//
// Mixing terms involving variables from different ModelStorage objects will
// lead to CHECK fails, including from the constructors.
//
// The type owns the associated data representing the terms, and so should
// usually be passed by (const) reference to avoid unnecessary copies.
//
// Note for implementers: Care must be taken to ensure that
// linear_terms_.storage() and quadratic_terms_.storage() do not disagree. That
// is, it is forbidden that both are non-null and not equal. Use
// CheckModelsAgree() and the initializer_list constructor to enforce this
// invariant in any class or friend method.
class QuadraticExpression {
 public:
#ifndef MATH_OPT_USE_EXPRESSION_COUNTERS
  QuadraticExpression() = default;
#else   // MATH_OPT_USE_EXPRESSION_COUNTERS
  QuadraticExpression();
  QuadraticExpression(const QuadraticExpression& other);
  QuadraticExpression(QuadraticExpression&& other);
  QuadraticExpression& operator=(const QuadraticExpression& other);
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
  // Users should prefer the default constructor and operator overloads to build
  // expressions.
  inline QuadraticExpression(
      std::initializer_list<QuadraticTerm> quadratic_terms,
      std::initializer_list<LinearTerm> linear_terms, double offset);
  inline QuadraticExpression(double offset);              // NOLINT
  inline QuadraticExpression(Variable variable);          // NOLINT
  inline QuadraticExpression(const LinearTerm& term);     // NOLINT
  inline QuadraticExpression(LinearExpression expr);      // NOLINT
  inline QuadraticExpression(const QuadraticTerm& term);  // NOLINT

  inline double offset() const;
  inline const VariableMap<double>& linear_terms() const;
  inline const QuadraticTermMap<double>& quadratic_terms() const;

  inline const absl::flat_hash_map<VariableId, double>& raw_linear_terms()
      const;
  inline const absl::flat_hash_map<QuadraticProductId, double>&
  raw_quadratic_terms() const;

  inline QuadraticExpression& operator+=(double value);
  inline QuadraticExpression& operator+=(Variable variable);
  inline QuadraticExpression& operator+=(const LinearTerm& term);
  inline QuadraticExpression& operator+=(const LinearExpression& expr);
  inline QuadraticExpression& operator+=(const QuadraticTerm& term);
  inline QuadraticExpression& operator+=(const QuadraticExpression& expr);
  inline QuadraticExpression& operator-=(double value);
  inline QuadraticExpression& operator-=(Variable variable);
  inline QuadraticExpression& operator-=(const LinearTerm& term);
  inline QuadraticExpression& operator-=(const LinearExpression& expr);
  inline QuadraticExpression& operator-=(const QuadraticTerm& term);
  inline QuadraticExpression& operator-=(const QuadraticExpression& expr);
  inline QuadraticExpression& operator*=(double value);
  inline QuadraticExpression& operator/=(double value);

  // Adds each element of items to this.
  //
  // Specifically, letting
  //   (i_1, i_2, ..., i_n) = items
  // adds
  //   i_1 + i_2 + ... + i_n
  // to this.
  //
  // Example:
  //   const Variable a = ...;
  //   const Variable b = ...;
  //   const std::vector<Variable> vars = {a, b};
  //   const std::vector<QuadraticTerm> terms = {2 * a * b};
  //   QuadraticExpression expr = 8;
  //   expr.AddSum(vars);
  //   expr.AddSum(terms);
  // Results in expr having the value 2 * a * b + a + b + 8.0.
  //
  // Compile time requirements:
  //  * Iterable is a sequence (an array or object with begin() and end()).
  //  * The type of an element of items is one of double, Variable, LinearTerm,
  //    LinearExpression, QuadraticTerm, or QuadraticExpression (or is
  //    implicitly convertible to one of these types, e.g. int).
  //
  // Note: The implementation is equivalent to:
  //   for(const auto item : items) {
  //     *this += item;
  //   }
  template <typename Iterable>
  inline void AddSum(const Iterable& items);

  // Returns the sum of the elements of items.
  //
  // Specifically, letting
  //   (i_1, i_2, ..., i_n) = items
  // returns
  //   i_1 + i_2 + ... + i_n.
  //
  // Example:
  //   const Variable a = ...;
  //   const Variable b = ...;
  //   const std::vector<QuadraticTerm> terms = {a * a, 2 * a * b, 3 * b * a};
  //   QuadraticExpression::Sum(vars)
  //     => a^2 + 5 a * b
  // Note, instead of:
  //   QuadraticExpression expr(3.0);
  //   expr += QuadraticExpression::Sum(items);
  // Prefer:
  //   expr.AddSum(items);
  //
  // See QuadraticExpression::AddSum() for a precise contract on the type
  // Iterable.
  template <typename Iterable>
  static inline QuadraticExpression Sum(const Iterable& items);

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
  //   const Variable a = ...;
  //   const Variable b = ...;
  //   const std::vector<Variable> vars = {a, b};
  //   const std::vector<double> coeffs = {10.0, 2.0};
  //   QuadraticExpression expr = 3.0;
  //   expr.AddInnerProduct(coeffs, vars);
  //   expr.AddInnerProduct(vars, vars);
  // Results in expr having the value a^2 + b^2 + 10.0 * a + 2.0 * b + 3.0.
  //
  // Compile time requirements:
  //  * LeftIterable and RightIterable are both sequences (arrays or objects
  //    with begin() and end())
  //  * For both left and right, their elements are of type double, Variable,
  //    LinearTerm, LinearExpression, QuadraticTerm, or QuadraticExpression (or
  //    is implicitly convertible to one of these types, e.g. int).
  // Runtime requirements (or CHECK fails):
  //  * The inner product value, and its constitutive intermediate terms, can be
  //    represented as a QuadraticExpression (potentially through an implicit
  //    conversion).
  //  * left and right have an equal number of elements.
  //
  // Note: The implementation is equivalent to the following pseudocode:
  //   for(const auto& [l, r] : zip(left, right)) {
  //     *this += l * r;
  //   }
  // In particular, the multiplication will be performed on the types of the
  // elements in left and right (take care with low precision types), but the
  // addition will always use double precision.
  template <typename LeftIterable, typename RightIterable>
  inline void AddInnerProduct(const LeftIterable& left,
                              const RightIterable& right);

  // Returns the inner product of left and right.
  //
  // Specifically, letting
  //   (l_1, l_2 ..., l_n) = left,
  //   (r_1, r_2, ..., r_n) = right,
  // returns
  //   l_1 * r_1 + l_2 * r_2 + ... + l_n * r_n.
  //
  // Example:
  //   const Variable a = ...;
  //   const Variable b = ...;
  //   const std::vector<Variable> left = {a, a};
  //   const std::vector<Variable> left = {a, b};
  //   QuadraticExpression::InnerProduct(left, right);
  //     -=> a^2 + a * b
  // Note, instead of:
  //   QuadraticExpression expr(3.0);
  //   expr += QuadraticExpression::InnerProduct(left, right);
  // Prefer:
  //   expr.AddInnerProduct(left, right);
  //
  // Requires that left and right have equal size, see
  // QuadraticExpression::AddInnerProduct() for a precise contract on template
  // types.
  template <typename LeftIterable, typename RightIterable>
  static inline QuadraticExpression InnerProduct(const LeftIterable& left,
                                                 const RightIterable& right);

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values.
  //
  // Will CHECK fail if the underlying model storage is different, or if a
  // variable in linear_terms() or quadratic_terms() is missing from
  // variables_values.
  double Evaluate(const VariableMap<double>& variable_values) const;

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values, or zero if missing from the map.
  //
  // Will CHECK fail the underlying model storage is different.
  double EvaluateWithDefaultZero(
      const VariableMap<double>& variable_values) const;

  inline const ModelStorage* storage() const;

#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  static thread_local int num_calls_default_constructor_;
  static thread_local int num_calls_copy_constructor_;
  static thread_local int num_calls_move_constructor_;
  static thread_local int num_calls_initializer_list_constructor_;
  static thread_local int num_calls_linear_expression_constructor_;
  // Reset all counters in the current thread to 0.
  static void ResetCounters();
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS

 private:
  friend QuadraticExpression operator-(QuadraticExpression expr);
  friend std::ostream& operator<<(std::ostream& ostr,
                                  const QuadraticExpression& expr);
  inline void CheckModelsAgree();

  QuadraticTermMap<double> quadratic_terms_;
  VariableMap<double> linear_terms_;
  double offset_ = 0.0;
};

// We have 6 types that we must consider arithmetic among:
//   1. double (scalar value)
//   2. Variable (affine value)
//   3. LinearTerm (affine value)
//   4. LinearExpression (affine value)
//   5. QuadraticTerm (quadratic value)
//   6. QuadraticExpression (quadratic value)
// We care only about those methods that result in a QuadraticExpression. For
// example, multiplying a linear value with a linear value, or adding a scalar
// to a quadratic value. The single unary method is:
inline QuadraticExpression operator-(QuadraticExpression expr);

// The binary methods, listed in lexicographic order based on
// (operator, lhs type #, rhs type #), with the type #s are listed above, are:
inline QuadraticExpression operator+(double lhs, const QuadraticTerm& rhs);
inline QuadraticExpression operator+(double lhs, QuadraticExpression rhs);
inline QuadraticExpression operator+(Variable lhs, const QuadraticTerm& rhs);
inline QuadraticExpression operator+(Variable lhs, QuadraticExpression rhs);
inline QuadraticExpression operator+(const LinearTerm& lhs,
                                     const QuadraticTerm& rhs);
inline QuadraticExpression operator+(const LinearTerm& lhs,
                                     QuadraticExpression rhs);
inline QuadraticExpression operator+(LinearExpression lhs,
                                     const QuadraticTerm& rhs);
inline QuadraticExpression operator+(const LinearExpression& lhs,
                                     QuadraticExpression rhs);
inline QuadraticExpression operator+(const QuadraticTerm& lhs, double rhs);
inline QuadraticExpression operator+(const QuadraticTerm& lhs, Variable rhs);
inline QuadraticExpression operator+(const QuadraticTerm& lhs,
                                     const LinearTerm& rhs);
inline QuadraticExpression operator+(const QuadraticTerm& lhs,
                                     LinearExpression rhs);
inline QuadraticExpression operator+(const QuadraticTerm& lhs,
                                     const QuadraticTerm& rhs);
inline QuadraticExpression operator+(const QuadraticTerm& lhs,
                                     QuadraticExpression rhs);
inline QuadraticExpression operator+(QuadraticExpression lhs, double rhs);
inline QuadraticExpression operator+(QuadraticExpression lhs, Variable rhs);
inline QuadraticExpression operator+(QuadraticExpression lhs,
                                     const LinearTerm& rhs);
inline QuadraticExpression operator+(QuadraticExpression lhs,
                                     const LinearExpression& rhs);
inline QuadraticExpression operator+(QuadraticExpression lhs,
                                     const QuadraticTerm& rhs);
inline QuadraticExpression operator+(QuadraticExpression lhs,
                                     const QuadraticExpression& rhs);

inline QuadraticExpression operator-(double lhs, const QuadraticTerm& rhs);
inline QuadraticExpression operator-(double lhs, QuadraticExpression rhs);
inline QuadraticExpression operator-(Variable lhs, const QuadraticTerm& rhs);
inline QuadraticExpression operator-(Variable lhs, QuadraticExpression rhs);
inline QuadraticExpression operator-(const LinearTerm& lhs,
                                     const QuadraticTerm& rhs);
inline QuadraticExpression operator-(const LinearTerm& lhs,
                                     QuadraticExpression rhs);
inline QuadraticExpression operator-(LinearExpression lhs,
                                     const QuadraticTerm& rhs);
inline QuadraticExpression operator-(const LinearExpression& lhs,
                                     QuadraticExpression rhs);
inline QuadraticExpression operator-(const QuadraticTerm& lhs, double rhs);
inline QuadraticExpression operator-(const QuadraticTerm& lhs, Variable rhs);
inline QuadraticExpression operator-(const QuadraticTerm& lhs,
                                     const LinearTerm& rhs);
inline QuadraticExpression operator-(const QuadraticTerm& lhs,
                                     LinearExpression rhs);
inline QuadraticExpression operator-(const QuadraticTerm& lhs,
                                     const QuadraticTerm& rhs);
inline QuadraticExpression operator-(const QuadraticTerm& lhs,
                                     QuadraticExpression rhs);
inline QuadraticExpression operator-(QuadraticExpression lhs, double rhs);
inline QuadraticExpression operator-(QuadraticExpression lhs, Variable rhs);
inline QuadraticExpression operator-(QuadraticExpression lhs,
                                     const LinearTerm& rhs);
inline QuadraticExpression operator-(QuadraticExpression lhs,
                                     const LinearExpression& rhs);
inline QuadraticExpression operator-(QuadraticExpression lhs,
                                     const QuadraticTerm& rhs);
inline QuadraticExpression operator-(QuadraticExpression lhs,
                                     const QuadraticExpression& rhs);

inline QuadraticExpression operator*(double lhs, QuadraticExpression rhs);
inline QuadraticExpression operator*(Variable lhs, const LinearExpression& rhs);
inline QuadraticExpression operator*(LinearTerm lhs,
                                     const LinearExpression& rhs);
inline QuadraticExpression operator*(const LinearExpression& lhs, Variable rhs);
inline QuadraticExpression operator*(const LinearExpression& lhs,
                                     LinearTerm rhs);
inline QuadraticExpression operator*(const LinearExpression& lhs,
                                     const LinearExpression& rhs);
inline QuadraticExpression operator*(QuadraticExpression lhs, double rhs);

inline QuadraticExpression operator/(QuadraticExpression lhs, double rhs);

// A QuadraticExpression with a lower bound.
struct LowerBoundedQuadraticExpression {
  // Users are not expected to use this constructor. Instead, they should build
  // this object using overloads of the >= and <= operators. For example, `x * y
  // >= 3`.
  inline LowerBoundedQuadraticExpression(QuadraticExpression expression,
                                         double lower_bound);
  // Users are not expected to explicitly use the following constructor.
  inline LowerBoundedQuadraticExpression(  // NOLINT
      LowerBoundedLinearExpression lb_expression);

  QuadraticExpression expression;
  double lower_bound;
};

// A QuadraticExpression with an upper bound.
struct UpperBoundedQuadraticExpression {
  // Users are not expected to use this constructor. Instead, they should build
  // this object using overloads of the >= and <= operators. For example, `x * y
  // <= 3`.
  inline UpperBoundedQuadraticExpression(QuadraticExpression expression,
                                         double upper_bound);
  // Users are not expected to explicitly use the following constructor.
  inline UpperBoundedQuadraticExpression(  // NOLINT
      UpperBoundedLinearExpression ub_expression);

  QuadraticExpression expression;
  double upper_bound;
};

// A QuadraticExpression with upper and lower bounds.
struct BoundedQuadraticExpression {
  // Users are not expected to use this constructor. Instead, they should build
  // this object using overloads of the >=, <=, and == operators. For example,
  // `3 <= x * y <= 3`.
  inline BoundedQuadraticExpression(QuadraticExpression expression,
                                    double lower_bound, double upper_bound);

  // Users are not expected to explicitly use the following constructors.
  inline BoundedQuadraticExpression(  // NOLINT
      internal::VariablesEquality var_equality);
  inline BoundedQuadraticExpression(  // NOLINT
      LowerBoundedLinearExpression lb_expression);
  inline BoundedQuadraticExpression(  // NOLINT
      UpperBoundedLinearExpression ub_expression);
  inline BoundedQuadraticExpression(  // NOLINT
      BoundedLinearExpression bounded_expression);
  inline BoundedQuadraticExpression(  // NOLINT
      LowerBoundedQuadraticExpression lb_expression);
  inline BoundedQuadraticExpression(  // NOLINT
      UpperBoundedQuadraticExpression ub_expression);

  // Returns the actual lower_bound after taking into account the quadratic
  // expression offset.
  inline double lower_bound_minus_offset() const;
  // Returns the actual upper_bound after taking into account the quadratic
  // expression offset.
  inline double upper_bound_minus_offset() const;

  QuadraticExpression expression;
  double lower_bound;
  double upper_bound;
};

std::ostream& operator<<(std::ostream& ostr,
                         const BoundedQuadraticExpression& bounded_expression);

// We intentionally pass the QuadraticExpression argument by value so that we
// don't make unnecessary copies of temporary objects by using the move
// constructor and the returned values optimization (RVO).
inline LowerBoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                                  double rhs);
inline LowerBoundedQuadraticExpression operator>=(QuadraticTerm lhs,
                                                  double rhs);
inline LowerBoundedQuadraticExpression operator<=(double lhs,
                                                  QuadraticExpression rhs);
inline LowerBoundedQuadraticExpression operator<=(double lhs,
                                                  QuadraticTerm rhs);

inline UpperBoundedQuadraticExpression operator>=(double lhs,
                                                  QuadraticExpression rhs);
inline UpperBoundedQuadraticExpression operator>=(double lhs,
                                                  QuadraticTerm rhs);
inline UpperBoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                                  double rhs);
inline UpperBoundedQuadraticExpression operator<=(QuadraticTerm lhs,
                                                  double rhs);

// We intentionally pass the UpperBoundedQuadraticExpression and
// LowerBoundedQuadraticExpression arguments by value so that we don't
// make unnecessary copies of temporary objects by using the move constructor
// and the returned values optimization (RVO).
inline BoundedQuadraticExpression operator>=(
    UpperBoundedQuadraticExpression lhs, double rhs);
inline BoundedQuadraticExpression operator>=(
    double lhs, LowerBoundedQuadraticExpression rhs);
inline BoundedQuadraticExpression operator<=(
    LowerBoundedQuadraticExpression lhs, double rhs);
inline BoundedQuadraticExpression operator<=(
    double lhs, UpperBoundedQuadraticExpression rhs);
// We intentionally pass one QuadraticExpression argument by value so that we
// don't make unnecessary copies of temporary objects by using the move
// constructor and the returned values optimization (RVO).

// Comparisons with lhs = QuadraticExpression
inline BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                             const QuadraticExpression& rhs);
inline BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                             QuadraticTerm rhs);
inline BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                             const LinearExpression& rhs);
inline BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                             LinearTerm rhs);
inline BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                             Variable rhs);
inline BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                             const QuadraticExpression& rhs);
inline BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                             QuadraticTerm rhs);
inline BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                             const LinearExpression& rhs);
inline BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                             LinearTerm rhs);
inline BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                             Variable rhs);
inline BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                             const QuadraticExpression& rhs);
inline BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                             QuadraticTerm rhs);
inline BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                             const LinearExpression& rhs);
inline BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                             LinearTerm rhs);
inline BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                             Variable rhs);
inline BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                             double rhs);
// Comparisons with lhs = QuadraticTerm
inline BoundedQuadraticExpression operator>=(QuadraticTerm lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator>=(QuadraticTerm lhs,
                                             QuadraticTerm rhs);
inline BoundedQuadraticExpression operator>=(QuadraticTerm lhs,
                                             LinearExpression rhs);
inline BoundedQuadraticExpression operator>=(QuadraticTerm lhs, LinearTerm rhs);
inline BoundedQuadraticExpression operator>=(QuadraticTerm lhs, Variable rhs);
inline BoundedQuadraticExpression operator<=(QuadraticTerm lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator<=(QuadraticTerm lhs,
                                             QuadraticTerm rhs);
inline BoundedQuadraticExpression operator<=(QuadraticTerm lhs,
                                             LinearExpression rhs);
inline BoundedQuadraticExpression operator<=(QuadraticTerm lhs, LinearTerm rhs);
inline BoundedQuadraticExpression operator<=(QuadraticTerm lhs, Variable rhs);
inline BoundedQuadraticExpression operator==(QuadraticTerm lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator==(QuadraticTerm lhs,
                                             QuadraticTerm rhs);
inline BoundedQuadraticExpression operator==(QuadraticTerm lhs,
                                             LinearExpression rhs);
inline BoundedQuadraticExpression operator==(QuadraticTerm lhs, LinearTerm rhs);
inline BoundedQuadraticExpression operator==(QuadraticTerm lhs, Variable rhs);
inline BoundedQuadraticExpression operator==(QuadraticTerm lhs, double rhs);
// Comparisons with lhs = LinearExpression
inline BoundedQuadraticExpression operator>=(const LinearExpression& lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator>=(LinearExpression lhs,
                                             QuadraticTerm rhs);
inline BoundedQuadraticExpression operator<=(const LinearExpression& lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator<=(LinearExpression lhs,
                                             QuadraticTerm rhs);
inline BoundedQuadraticExpression operator==(const LinearExpression& lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator==(LinearExpression lhs,
                                             QuadraticTerm rhs);
// Comparisons with lhs = LinearTerm
inline BoundedQuadraticExpression operator>=(LinearTerm lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator>=(LinearTerm lhs, QuadraticTerm rhs);
inline BoundedQuadraticExpression operator<=(LinearTerm lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator<=(LinearTerm lhs, QuadraticTerm rhs);
inline BoundedQuadraticExpression operator==(LinearTerm lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator==(LinearTerm lhs, QuadraticTerm rhs);
// Comparisons with lhs = Variable
inline BoundedQuadraticExpression operator>=(Variable lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator>=(Variable lhs, QuadraticTerm rhs);
inline BoundedQuadraticExpression operator<=(Variable lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator<=(Variable lhs, QuadraticTerm rhs);
inline BoundedQuadraticExpression operator==(Variable lhs,
                                             QuadraticExpression rhs);
inline BoundedQuadraticExpression operator==(Variable lhs, QuadraticTerm rhs);
// Comparisons with lhs = Double
inline BoundedQuadraticExpression operator==(double lhs, QuadraticTerm rhs);
inline BoundedQuadraticExpression operator==(double lhs,
                                             QuadraticExpression rhs);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Inline function implementations /////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Variable
////////////////////////////////////////////////////////////////////////////////

Variable::Variable(const ModelStorage* const storage, const VariableId id)
    : storage_(storage), id_(id) {
  DCHECK(storage != nullptr);
}

int64_t Variable::id() const { return id_.value(); }

VariableId Variable::typed_id() const { return id_; }

const ModelStorage* Variable::storage() const { return storage_; }

double Variable::lower_bound() const {
  return storage_->variable_lower_bound(id_);
}

double Variable::upper_bound() const {
  return storage_->variable_upper_bound(id_);
}

bool Variable::is_integer() const { return storage_->is_variable_integer(id_); }

absl::string_view Variable::name() const {
  if (storage()->has_variable(id_)) {
    return storage_->variable_name(id_);
  }
  return "[variable deleted from model]";
}

template <typename H>
H AbslHashValue(H h, const Variable& variable) {
  return H::combine(std::move(h), variable.id_.value(), variable.storage_);
}

std::ostream& operator<<(std::ostream& ostr, const Variable& variable) {
  // TODO(b/170992529): handle quoting of invalid characters in the name.
  const absl::string_view name = variable.name();
  if (name.empty()) {
    ostr << "__var#" << variable.id() << "__";
  } else {
    ostr << name;
  }
  return ostr;
}

LinearExpression Variable::operator-() const {
  return LinearExpression({LinearTerm(*this, -1.0)}, 0.0);
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
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  ++num_calls_initializer_list_constructor_;
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
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
LinearExpression LinearExpression::Sum(const Iterable& items) {
  LinearExpression result;
  result.AddSum(items);
  return result;
}

template <typename Iterable>
LinearExpression Sum(const Iterable& items) {
  return LinearExpression::Sum(items);
}

namespace internal {

template <typename LeftIterable, typename RightIterable, typename Expression>
void AddInnerProduct(const LeftIterable& left, const RightIterable& right,
                     Expression& expr) {
  using std::begin;
  using std::end;
  auto l = begin(left);
  auto r = begin(right);
  const auto l_end = end(left);
  const auto r_end = end(right);
  for (; l != l_end && r != r_end; ++l, ++r) {
    expr += (*l) * (*r);
  }
  CHECK(l == l_end)
      << "left had more elements than right, sizes should be equal";
  CHECK(r == r_end)
      << "right had more elements than left, sizes should be equal";
}

}  // namespace internal

template <typename LeftIterable, typename RightIterable>
void LinearExpression::AddInnerProduct(const LeftIterable& left,
                                       const RightIterable& right) {
  internal::AddInnerProduct(left, right, *this);
}

template <typename LeftIterable, typename RightIterable>
LinearExpression LinearExpression::InnerProduct(const LeftIterable& left,
                                                const RightIterable& right) {
  LinearExpression result;
  result.AddInnerProduct(left, right);
  return result;
}

template <typename LeftIterable, typename RightIterable>
LinearExpression InnerProduct(const LeftIterable& left,
                              const RightIterable& right) {
  return LinearExpression::InnerProduct(left, right);
}

const VariableMap<double>& LinearExpression::terms() const { return terms_; }

double LinearExpression::offset() const { return offset_; }

const ModelStorage* LinearExpression::storage() const {
  return terms_.storage();
}

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
  return lhs.typed_id() == rhs.typed_id() && lhs.storage() == rhs.storage();
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

////////////////////////////////////////////////////////////////////////////////
// QuadraticTermKey
////////////////////////////////////////////////////////////////////////////////

QuadraticTermKey::QuadraticTermKey(const ModelStorage* storage,
                                   const QuadraticProductId id)
    : storage_(storage), variable_ids_(id) {
  if (variable_ids_.first > variable_ids_.second) {
    using std::swap;  // go/using-std-swap
    swap(variable_ids_.first, variable_ids_.second);
  }
}

QuadraticTermKey::QuadraticTermKey(const Variable first_variable,
                                   const Variable second_variable)
    : QuadraticTermKey(first_variable.storage(), {first_variable.typed_id(),
                                                  second_variable.typed_id()}) {
  CHECK_EQ(first_variable.storage(), second_variable.storage())
      << internal::kObjectsFromOtherModelStorage;
}

QuadraticProductId QuadraticTermKey::typed_id() const { return variable_ids_; }

const ModelStorage* QuadraticTermKey::storage() const { return storage_; }

template <typename H>
H AbslHashValue(H h, const QuadraticTermKey& key) {
  return H::combine(std::move(h), key.typed_id().first.value(),
                    key.typed_id().second.value(), key.storage());
}

std::ostream& operator<<(std::ostream& ostr, const QuadraticTermKey& key) {
  ostr << "(" << Variable(key.storage(), key.typed_id().first) << ", "
       << Variable(key.storage(), key.typed_id().second) << ")";
  return ostr;
}

bool operator==(const QuadraticTermKey lhs, const QuadraticTermKey rhs) {
  return lhs.storage() == rhs.storage() && lhs.typed_id() == rhs.typed_id();
}

bool operator!=(const QuadraticTermKey lhs, const QuadraticTermKey rhs) {
  return !(lhs == rhs);
}

////////////////////////////////////////////////////////////////////////////////
// QuadraticTerm (no arithmetic)
////////////////////////////////////////////////////////////////////////////////

QuadraticTerm::QuadraticTerm(Variable first_variable, Variable second_variable,
                             const double coefficient)
    : first_variable_(std::move(first_variable)),
      second_variable_(std::move(second_variable)),
      coefficient_(coefficient) {
  CHECK_EQ(first_variable_.storage(), second_variable_.storage())
      << internal::kObjectsFromOtherModelStorage;
}

double QuadraticTerm::coefficient() const { return coefficient_; }
Variable QuadraticTerm::first_variable() const { return first_variable_; }
Variable QuadraticTerm::second_variable() const { return second_variable_; }

QuadraticTermKey QuadraticTerm::GetKey() const {
  return QuadraticTermKey(
      first_variable_.storage(),
      std::make_pair(first_variable_.typed_id(), second_variable_.typed_id()));
}

////////////////////////////////////////////////////////////////////////////////
// QuadraticExpression (no arithmetic)
////////////////////////////////////////////////////////////////////////////////

QuadraticExpression::QuadraticExpression(
    const std::initializer_list<QuadraticTerm> quadratic_terms,
    const std::initializer_list<LinearTerm> linear_terms, const double offset)
    : offset_(offset) {
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  ++num_calls_initializer_list_constructor_;
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
  for (const LinearTerm& term : linear_terms) {
    linear_terms_[term.variable] += term.coefficient;
  }
  for (const QuadraticTerm& term : quadratic_terms) {
    quadratic_terms_[term.GetKey()] += term.coefficient();
  }
  CheckModelsAgree();
}

QuadraticExpression::QuadraticExpression(const double offset)
    : QuadraticExpression({}, {}, offset) {}

QuadraticExpression::QuadraticExpression(const Variable variable)
    : QuadraticExpression({}, {LinearTerm(variable, 1.0)}, 0.0) {}

QuadraticExpression::QuadraticExpression(const LinearTerm& term)
    : QuadraticExpression({}, {term}, 0.0) {}

QuadraticExpression::QuadraticExpression(LinearExpression expr)
    : linear_terms_(std::move(expr.terms_)),
      offset_(std::exchange(expr.offset_, 0.0)) {
#ifdef MATH_OPT_USE_EXPRESSION_COUNTERS
  ++num_calls_linear_expression_constructor_;
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
}

QuadraticExpression::QuadraticExpression(const QuadraticTerm& term)
    : QuadraticExpression({term}, {}, 0.0) {}

void QuadraticExpression::CheckModelsAgree() {
  const ModelStorage* const quadratic_model = quadratic_terms_.storage();
  const ModelStorage* const linear_model = linear_terms_.storage();
  if ((linear_model != nullptr) && (quadratic_model != nullptr) &&
      (quadratic_model != linear_model)) {
    LOG(FATAL) << internal::kObjectsFromOtherModelStorage;
  }
}

const ModelStorage* QuadraticExpression::storage() const {
  if (quadratic_terms().storage()) {
    return quadratic_terms().storage();
  } else {
    return linear_terms().storage();
  }
}

double QuadraticExpression::offset() const { return offset_; }

const VariableMap<double>& QuadraticExpression::linear_terms() const {
  return linear_terms_;
}

const QuadraticTermMap<double>& QuadraticExpression::quadratic_terms() const {
  return quadratic_terms_;
}

const absl::flat_hash_map<VariableId, double>&
QuadraticExpression::raw_linear_terms() const {
  return linear_terms_.raw_map();
}

const absl::flat_hash_map<QuadraticProductId, double>&
QuadraticExpression::raw_quadratic_terms() const {
  return quadratic_terms_.raw_map();
}

////////////////////////////////////////////////////////////////////////////////
// Arithmetic operators (non-member).
//
// These are NOT required to explicitly CHECK that the underlying model storages
// agree between linear_terms_ and quadratic_terms_ unless they are a friend of
// QuadraticExpression. As much as possible, defer to the assignment operators
// and the initializer list constructor for QuadraticExpression.
////////////////////////////////////////////////////////////////////////////////

// ----------------------------- Addition (+) ----------------------------------

QuadraticExpression operator+(const double lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({rhs}, {}, lhs);
}

QuadraticExpression operator+(const double lhs, QuadraticExpression rhs) {
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator+(const Variable lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({rhs}, {LinearTerm(lhs, 1.0)}, 0.0);
}

QuadraticExpression operator+(const Variable lhs, QuadraticExpression rhs) {
  rhs += LinearTerm(lhs, 1.0);
  return rhs;
}

QuadraticExpression operator+(const LinearTerm& lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({rhs}, {lhs}, 0.0);
}

QuadraticExpression operator+(const LinearTerm& lhs, QuadraticExpression rhs) {
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator+(LinearExpression lhs, const QuadraticTerm& rhs) {
  QuadraticExpression expr(std::move(lhs));
  expr += rhs;
  return expr;
}

QuadraticExpression operator+(const LinearExpression& lhs,
                              QuadraticExpression rhs) {
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator+(const QuadraticTerm& lhs, const double rhs) {
  return QuadraticExpression({lhs}, {}, rhs);
}

QuadraticExpression operator+(const QuadraticTerm& lhs, const Variable rhs) {
  return QuadraticExpression({lhs}, {LinearTerm(rhs, 1.0)}, 0.0);
}

QuadraticExpression operator+(const QuadraticTerm& lhs, const LinearTerm& rhs) {
  return QuadraticExpression({lhs}, {rhs}, 0.0);
}

QuadraticExpression operator+(const QuadraticTerm& lhs, LinearExpression rhs) {
  QuadraticExpression expr(std::move(rhs));
  expr += lhs;
  return expr;
}

QuadraticExpression operator+(const QuadraticTerm& lhs,
                              const QuadraticTerm& rhs) {
  return QuadraticExpression({lhs, rhs}, {}, 0.0);
}

QuadraticExpression operator+(const QuadraticTerm& lhs,
                              QuadraticExpression rhs) {
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator+(QuadraticExpression lhs, const double rhs) {
  lhs += rhs;
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs, const Variable rhs) {
  lhs += LinearTerm(rhs, 1.0);
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs, const LinearTerm& rhs) {
  lhs += rhs;
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs,
                              const LinearExpression& rhs) {
  lhs += rhs;
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs,
                              const QuadraticTerm& rhs) {
  lhs += rhs;
  return lhs;
}

QuadraticExpression operator+(QuadraticExpression lhs,
                              const QuadraticExpression& rhs) {
  lhs += rhs;
  return lhs;
}

// --------------------------- Subtraction (-) ---------------------------------

// NOTE: A friend of QuadraticTerm, but does not touch variables
QuadraticTerm operator-(QuadraticTerm term) {
  term.coefficient_ *= -1.0;
  return term;
}

// NOTE: A friend of QuadraticExpression, but does not touch variables
QuadraticExpression operator-(QuadraticExpression expr) {
  expr.offset_ = -expr.offset_;
  for (auto term : expr.linear_terms_) {
    term.second = -term.second;
  }
  for (auto term : expr.quadratic_terms_) {
    term.second = -term.second;
  }
  return expr;
}

QuadraticExpression operator-(const double lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({-rhs}, {}, lhs);
}

QuadraticExpression operator-(const double lhs, QuadraticExpression rhs) {
  auto expr = -std::move(rhs);
  expr += lhs;
  return expr;
}

QuadraticExpression operator-(const Variable lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({-rhs}, {LinearTerm(lhs, 1.0)}, 0.0);
}

QuadraticExpression operator-(const Variable lhs, QuadraticExpression rhs) {
  return LinearTerm(lhs, 1.0) - std::move(rhs);
}

QuadraticExpression operator-(const LinearTerm& lhs, const QuadraticTerm& rhs) {
  return QuadraticExpression({-rhs}, {lhs}, 0.0);
}

QuadraticExpression operator-(const LinearTerm& lhs, QuadraticExpression rhs) {
  auto expr = -std::move(rhs);
  expr += lhs;
  return expr;
}

QuadraticExpression operator-(LinearExpression lhs, const QuadraticTerm& rhs) {
  QuadraticExpression expr(std::move(lhs));
  expr -= rhs;
  return expr;
}

QuadraticExpression operator-(const LinearExpression& lhs,
                              QuadraticExpression rhs) {
  auto expr = -std::move(rhs);
  expr += lhs;
  return expr;
}

QuadraticExpression operator-(const QuadraticTerm& lhs, const double rhs) {
  return QuadraticExpression({lhs}, {}, -rhs);
}

QuadraticExpression operator-(const QuadraticTerm& lhs, const Variable rhs) {
  return QuadraticExpression({lhs}, {LinearTerm(rhs, -1.0)}, 0.0);
}

QuadraticExpression operator-(const QuadraticTerm& lhs, const LinearTerm& rhs) {
  return QuadraticExpression({lhs}, {-rhs}, 0.0);
}

QuadraticExpression operator-(const QuadraticTerm& lhs, LinearExpression rhs) {
  QuadraticExpression expr(-std::move(rhs));
  expr += lhs;
  return expr;
}

QuadraticExpression operator-(const QuadraticTerm& lhs,
                              const QuadraticTerm& rhs) {
  return QuadraticExpression({lhs, -rhs}, {}, 0.0);
}

QuadraticExpression operator-(const QuadraticTerm& lhs,
                              QuadraticExpression rhs) {
  rhs *= -1.0;
  rhs += lhs;
  return rhs;
}

QuadraticExpression operator-(QuadraticExpression lhs, const double rhs) {
  lhs -= rhs;
  return lhs;
}

// NOTE: Out-of-order for compilation purposes
QuadraticExpression operator-(QuadraticExpression lhs, const LinearTerm& rhs) {
  lhs -= rhs;
  return lhs;
}

QuadraticExpression operator-(QuadraticExpression lhs, const Variable rhs) {
  lhs -= LinearTerm(rhs, 1.0);
  return lhs;
}

// NOTE: operator-(QuadraticExpression, const LinearTerm) appears above

QuadraticExpression operator-(QuadraticExpression lhs,
                              const LinearExpression& rhs) {
  lhs -= rhs;
  return lhs;
}

QuadraticExpression operator-(QuadraticExpression lhs,
                              const QuadraticTerm& rhs) {
  lhs -= rhs;
  return lhs;
}

QuadraticExpression operator-(QuadraticExpression lhs,
                              const QuadraticExpression& rhs) {
  lhs -= rhs;
  return lhs;
}

// ---------------------------- Multiplication (*) -----------------------------

// NOTE: A friend of QuadraticTerm, but does not touch variables
QuadraticTerm operator*(const double lhs, QuadraticTerm rhs) {
  rhs.coefficient_ *= lhs;
  return rhs;
}

QuadraticExpression operator*(const double lhs, QuadraticExpression rhs) {
  rhs *= lhs;
  return rhs;
}

QuadraticTerm operator*(Variable lhs, Variable rhs) {
  return QuadraticTerm(std::move(lhs), std::move(rhs), 1.0);
}

QuadraticTerm operator*(Variable lhs, LinearTerm rhs) {
  return QuadraticTerm(std::move(lhs), std::move(rhs.variable),
                       rhs.coefficient);
}

QuadraticExpression operator*(Variable lhs, const LinearExpression& rhs) {
  QuadraticExpression expr;
  for (const auto& [var, coeff] : rhs.terms()) {
    expr += QuadraticTerm(lhs, var, coeff);
  }
  if (rhs.offset() != 0) {
    expr += LinearTerm(std::move(lhs), rhs.offset());
  }
  return expr;
}

QuadraticTerm operator*(LinearTerm lhs, Variable rhs) {
  return QuadraticTerm(std::move(lhs.variable), std::move(rhs),
                       lhs.coefficient);
}

QuadraticTerm operator*(LinearTerm lhs, LinearTerm rhs) {
  return QuadraticTerm(std::move(lhs.variable), std::move(rhs.variable),
                       lhs.coefficient * rhs.coefficient);
}

QuadraticExpression operator*(LinearTerm lhs, const LinearExpression& rhs) {
  QuadraticExpression expr;
  for (const auto& [var, coeff] : rhs.terms()) {
    expr += QuadraticTerm(lhs.variable, var, lhs.coefficient * coeff);
  }
  if (rhs.offset() != 0) {
    expr += LinearTerm(std::move(lhs.variable), lhs.coefficient * rhs.offset());
  }
  return expr;
}

QuadraticExpression operator*(const LinearExpression& lhs, Variable rhs) {
  QuadraticExpression expr;
  for (const auto& [var, coeff] : lhs.terms()) {
    expr += QuadraticTerm(var, rhs, coeff);
  }
  if (lhs.offset() != 0) {
    expr += LinearTerm(std::move(rhs), lhs.offset());
  }
  return expr;
}

QuadraticExpression operator*(const LinearExpression& lhs, LinearTerm rhs) {
  QuadraticExpression expr;
  for (const auto& [var, coeff] : lhs.terms()) {
    expr += QuadraticTerm(var, rhs.variable, coeff * rhs.coefficient);
  }
  if (lhs.offset() != 0) {
    expr += LinearTerm(std::move(rhs.variable), lhs.offset() * rhs.coefficient);
  }
  return expr;
}

QuadraticExpression operator*(const LinearExpression& lhs,
                              const LinearExpression& rhs) {
  QuadraticExpression expr = lhs.offset() * rhs.offset();
  if (rhs.offset() != 0) {
    for (const auto& [var, coeff] : lhs.terms()) {
      expr += LinearTerm(var, coeff * rhs.offset());
    }
  }
  if (lhs.offset() != 0) {
    for (const auto& [var, coeff] : rhs.terms()) {
      expr += LinearTerm(var, lhs.offset() * coeff);
    }
  }
  for (const auto& [lhs_var, lhs_coeff] : lhs.terms()) {
    for (const auto& [rhs_var, rhs_coeff] : rhs.terms()) {
      expr += QuadraticTerm(lhs_var, rhs_var, lhs_coeff * rhs_coeff);
    }
  }
  return expr;
}

// NOTE: A friend of QuadraticTerm, but does not touch variables
QuadraticTerm operator*(QuadraticTerm lhs, const double rhs) {
  lhs.coefficient_ *= rhs;
  return lhs;
}

QuadraticExpression operator*(QuadraticExpression lhs, const double rhs) {
  lhs *= rhs;
  return lhs;
}

// ------------------------------- Division (/) --------------------------------

// NOTE: A friend of QuadraticTerm, but does not touch variables
QuadraticTerm operator/(QuadraticTerm lhs, const double rhs) {
  lhs.coefficient_ /= rhs;
  return lhs;
}

QuadraticExpression operator/(QuadraticExpression lhs, const double rhs) {
  lhs /= rhs;
  return lhs;
}

////////////////////////////////////////////////////////////////////////////////
// In-place arithmetic operators.
//
// These must guarantee that the underlying model storages for linear_terms_ and
// quadratic_terms_ agree upon exit of the function, using CheckModelsAgree(),
// the list initializer constructor for QuadraticExpression, or similar logic.
////////////////////////////////////////////////////////////////////////////////

QuadraticExpression& QuadraticExpression::operator+=(const double value) {
  offset_ += value;
  // NOTE: Not touching terms, no need to check models
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(const Variable variable) {
  linear_terms_[variable] += 1;
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(const LinearTerm& term) {
  linear_terms_[term.variable] += term.coefficient;
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(
    const LinearExpression& expr) {
  offset_ += expr.offset();
  linear_terms_.Add(expr.terms());
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(
    const QuadraticTerm& term) {
  quadratic_terms_[term.GetKey()] += term.coefficient();
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator+=(
    const QuadraticExpression& expr) {
  offset_ += expr.offset();
  linear_terms_.Add(expr.linear_terms());
  quadratic_terms_.Add(expr.quadratic_terms());
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(const double value) {
  offset_ -= value;
  // NOTE: Not touching terms, no need to check models
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(const Variable variable) {
  linear_terms_[variable] -= 1;
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(const LinearTerm& term) {
  linear_terms_[term.variable] -= term.coefficient;
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(
    const LinearExpression& expr) {
  offset_ -= expr.offset();
  linear_terms_.Subtract(expr.terms());
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(
    const QuadraticTerm& term) {
  quadratic_terms_[term.GetKey()] -= term.coefficient();
  CheckModelsAgree();
  return *this;
}

QuadraticExpression& QuadraticExpression::operator-=(
    const QuadraticExpression& expr) {
  offset_ -= expr.offset();
  linear_terms_.Subtract(expr.linear_terms());
  quadratic_terms_.Subtract(expr.quadratic_terms());
  CheckModelsAgree();
  return *this;
}

QuadraticTerm& QuadraticTerm::operator*=(const double value) {
  coefficient_ *= value;
  // NOTE: Not touching variables in term, just modifying coefficient, so no
  // need to check that models agree.
  return *this;
}

QuadraticExpression& QuadraticExpression::operator*=(const double value) {
  offset_ *= value;
  for (auto term : linear_terms_) {
    term.second *= value;
  }
  for (auto term : quadratic_terms_) {
    term.second *= value;
  }
  // NOTE: Not adding/removing/altering variables in expression, just modifying
  // coefficients, so no need to check that models agree.
  return *this;
}

QuadraticTerm& QuadraticTerm::operator/=(const double value) {
  coefficient_ /= value;
  // NOTE: Not touching variables in term, just modifying coefficient, so no
  // need to check that models agree.
  return *this;
}

QuadraticExpression& QuadraticExpression::operator/=(const double value) {
  offset_ /= value;
  for (auto term : linear_terms_) {
    term.second /= value;
  }
  for (auto term : quadratic_terms_) {
    term.second /= value;
  }
  // NOTE: Not adding/removing/altering variables in expression, just modifying
  // coefficients, so no need to check that models agree.
  return *this;
}

template <typename Iterable>
void QuadraticExpression::AddSum(const Iterable& items) {
  for (const auto& item : items) {
    *this += item;
  }
}

template <typename Iterable>
QuadraticExpression QuadraticExpression::Sum(const Iterable& items) {
  QuadraticExpression result;
  result.AddSum(items);
  return result;
}

template <typename LeftIterable, typename RightIterable>
void QuadraticExpression::AddInnerProduct(const LeftIterable& left,
                                          const RightIterable& right) {
  internal::AddInnerProduct(left, right, *this);
}

template <typename LeftIterable, typename RightIterable>
QuadraticExpression QuadraticExpression::InnerProduct(
    const LeftIterable& left, const RightIterable& right) {
  QuadraticExpression result;
  result.AddInnerProduct(left, right);
  return result;
}

/////////////////////////////////////////////////////////////////////////////////
// LowerBoundedQuadraticExpression
// UpperBoundedQuadraticExpression
// BoundedQuadraticExpression
////////////////////////////////////////////////////////////////////////////////

LowerBoundedQuadraticExpression::LowerBoundedQuadraticExpression(
    QuadraticExpression expression, const double lower_bound)
    : expression(std::move(expression)), lower_bound(lower_bound) {}
LowerBoundedQuadraticExpression::LowerBoundedQuadraticExpression(
    LowerBoundedLinearExpression lb_expression)
    : expression(std::move(lb_expression.expression)),
      lower_bound(lb_expression.lower_bound) {}

UpperBoundedQuadraticExpression::UpperBoundedQuadraticExpression(
    QuadraticExpression expression, const double upper_bound)
    : expression(std::move(expression)), upper_bound(upper_bound) {}
UpperBoundedQuadraticExpression::UpperBoundedQuadraticExpression(
    UpperBoundedLinearExpression ub_expression)
    : expression(std::move(ub_expression.expression)),
      upper_bound(ub_expression.upper_bound) {}

BoundedQuadraticExpression::BoundedQuadraticExpression(
    QuadraticExpression expression, const double lower_bound,
    const double upper_bound)
    : expression(std::move(expression)),
      lower_bound(lower_bound),
      upper_bound(upper_bound) {}
BoundedQuadraticExpression::BoundedQuadraticExpression(
    internal::VariablesEquality var_equality)
    : lower_bound(0), upper_bound(0) {
  expression += var_equality.lhs;
  expression -= var_equality.rhs;
}
BoundedQuadraticExpression::BoundedQuadraticExpression(
    LowerBoundedLinearExpression lb_expression)
    : expression(std::move(lb_expression.expression)),
      lower_bound(lb_expression.lower_bound),
      upper_bound(std::numeric_limits<double>::infinity()) {}
BoundedQuadraticExpression::BoundedQuadraticExpression(
    UpperBoundedLinearExpression ub_expression)
    : expression(std::move(ub_expression.expression)),
      lower_bound(-std::numeric_limits<double>::infinity()),
      upper_bound(ub_expression.upper_bound) {}
BoundedQuadraticExpression::BoundedQuadraticExpression(
    BoundedLinearExpression bounded_expression)
    : expression(std::move(bounded_expression.expression)),
      lower_bound(bounded_expression.lower_bound),
      upper_bound(bounded_expression.upper_bound) {}
BoundedQuadraticExpression::BoundedQuadraticExpression(
    LowerBoundedQuadraticExpression lb_expression)
    : expression(std::move(lb_expression.expression)),
      lower_bound(lb_expression.lower_bound),
      upper_bound(std::numeric_limits<double>::infinity()) {}
BoundedQuadraticExpression::BoundedQuadraticExpression(
    UpperBoundedQuadraticExpression ub_expression)
    : expression(std::move(ub_expression.expression)),
      lower_bound(-std::numeric_limits<double>::infinity()),
      upper_bound(ub_expression.upper_bound) {}

double BoundedQuadraticExpression::lower_bound_minus_offset() const {
  return lower_bound - expression.offset();
}

double BoundedQuadraticExpression::upper_bound_minus_offset() const {
  return upper_bound - expression.offset();
}

LowerBoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                           const double rhs) {
  return LowerBoundedQuadraticExpression(std::move(lhs), rhs);
}
LowerBoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                           const double rhs) {
  return LowerBoundedQuadraticExpression(lhs, rhs);
}
LowerBoundedQuadraticExpression operator<=(const double lhs,
                                           QuadraticExpression rhs) {
  return LowerBoundedQuadraticExpression(std::move(rhs), lhs);
}
LowerBoundedQuadraticExpression operator<=(const double lhs,
                                           const QuadraticTerm rhs) {
  return LowerBoundedQuadraticExpression(rhs, lhs);
}

UpperBoundedQuadraticExpression operator>=(const double lhs,
                                           QuadraticExpression rhs) {
  return UpperBoundedQuadraticExpression(std::move(rhs), lhs);
}
UpperBoundedQuadraticExpression operator>=(const double lhs,
                                           const QuadraticTerm rhs) {
  return UpperBoundedQuadraticExpression(rhs, lhs);
}
UpperBoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                           const double rhs) {
  return UpperBoundedQuadraticExpression(std::move(lhs), rhs);
}
UpperBoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                           const double rhs) {
  return UpperBoundedQuadraticExpression(lhs, rhs);
}

BoundedQuadraticExpression operator>=(UpperBoundedQuadraticExpression lhs,
                                      const double rhs) {
  return BoundedQuadraticExpression(std::move(lhs.expression), rhs,
                                    lhs.upper_bound);
}
BoundedQuadraticExpression operator>=(const double lhs,
                                      LowerBoundedQuadraticExpression rhs) {
  return BoundedQuadraticExpression(std::move(rhs.expression), rhs.lower_bound,
                                    lhs);
}
BoundedQuadraticExpression operator<=(LowerBoundedQuadraticExpression lhs,
                                      const double rhs) {
  return BoundedQuadraticExpression(std::move(lhs.expression), lhs.lower_bound,
                                    rhs);
}
BoundedQuadraticExpression operator<=(const double lhs,
                                      UpperBoundedQuadraticExpression rhs) {
  return BoundedQuadraticExpression(std::move(rhs.expression), lhs,
                                    rhs.upper_bound);
}

BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const QuadraticTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const LinearTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const Variable rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const QuadraticTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const LinearTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const Variable rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(
      std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const QuadraticTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const LinearExpression& rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const LinearTerm rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const Variable rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const double rhs) {
  lhs -= rhs;
  return BoundedQuadraticExpression(std::move(lhs), 0, 0);
}

BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(
      std::move(rhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      LinearExpression rhs) {
  return BoundedQuadraticExpression(
      std::move(rhs) - lhs, -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      const LinearTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator>=(const QuadraticTerm lhs,
                                      const Variable rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      LinearExpression rhs) {
  return BoundedQuadraticExpression(std::move(rhs) - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      const LinearTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator<=(const QuadraticTerm lhs,
                                      const Variable rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}
BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}
BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      LinearExpression rhs) {
  return BoundedQuadraticExpression(std::move(rhs) - lhs, 0, 0);
}
BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      const LinearTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}
BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      const Variable rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}
BoundedQuadraticExpression operator==(const QuadraticTerm lhs,
                                      const double rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

BoundedQuadraticExpression operator>=(const LinearExpression& lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(
      std::move(rhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator>=(LinearExpression lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - std::move(lhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator<=(const LinearExpression& lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator<=(LinearExpression lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - std::move(lhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator==(const LinearExpression& lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}
BoundedQuadraticExpression operator==(LinearExpression lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - std::move(lhs), 0, 0);
}
// LinearTerm --
BoundedQuadraticExpression operator>=(const LinearTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(
      std::move(rhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator>=(const LinearTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator<=(const LinearTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator<=(const LinearTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator==(const LinearTerm lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}
BoundedQuadraticExpression operator==(const LinearTerm lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}
// Variable --
BoundedQuadraticExpression operator>=(const Variable lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(
      std::move(rhs), -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator>=(const Variable lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(
      rhs - lhs, -std::numeric_limits<double>::infinity(), 0);
}
BoundedQuadraticExpression operator<=(const Variable lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator<=(const Variable lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0,
                                    std::numeric_limits<double>::infinity());
}
BoundedQuadraticExpression operator==(const Variable lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}
BoundedQuadraticExpression operator==(const Variable lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

// Double --
BoundedQuadraticExpression operator==(const double lhs,
                                      QuadraticExpression rhs) {
  rhs -= lhs;
  return BoundedQuadraticExpression(std::move(rhs), 0, 0);
}
BoundedQuadraticExpression operator==(const double lhs,
                                      const QuadraticTerm rhs) {
  return BoundedQuadraticExpression(rhs - lhs, 0, 0);
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_VARIABLE_AND_EXPRESSIONS_H_
