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
#ifndef ORTOOLS_MATH_OPT_CPP_VARIABLE_AND_EXPRESSIONS_H_
#define ORTOOLS_MATH_OPT_CPP_VARIABLE_AND_EXPRESSIONS_H_

#include <stdint.h>

#include <initializer_list>
#include <iterator>
#include <ostream>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/string_view.h"
#include "ortools/math_opt/cpp/key_types.h"  // IWYU pragma: export
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_item.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research {
namespace math_opt {

// Forward declaration needed by Variable.
class LinearExpression;

// A value type that references a variable from ModelStorage. Usually this type
// is passed by copy.
class Variable final : public ModelStorageElement<
                           ElementType::kVariable, Variable,
                           // This type has a special equality operator
                           // (see `VariablesEquality` below).
                           ModelStorageElementEquality::kWithoutEquality> {
 public:
  using ModelStorageElement::ModelStorageElement;

  double lower_bound() const;
  double upper_bound() const;
  bool is_integer() const;
  absl::string_view name() const;

  LinearExpression operator-() const;
};

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
  VariablesEquality(Variable lhs, Variable rhs);
  operator bool() const;  // NOLINT
  Variable lhs;
  Variable rhs;
};

}  // namespace internal

internal::VariablesEquality operator==(const Variable& lhs,
                                       const Variable& rhs);
bool operator!=(const Variable& lhs, const Variable& rhs);

template <typename V>
using VariableMap = absl::flat_hash_map<Variable, V>;

// A term in an sum of variables multiplied by coefficients.
struct LinearTerm {
  // Usually this constructor is never called explicitly by users. Instead it
  // will be implicitly used when writing linear expression. For example `x +
  // 2*y` will automatically use this constructor to build a LinearTerm from `x`
  // and the overload of the operator* will also automatically create the one
  // from `2*y`.
  LinearTerm(Variable variable, double coefficient);
  LinearTerm operator-() const;
  LinearTerm& operator*=(double d);
  LinearTerm& operator/=(double d);
  Variable variable;
  double coefficient;
};

LinearTerm operator*(double coefficient, LinearTerm term);
LinearTerm operator*(LinearTerm term, double coefficient);
LinearTerm operator*(double coefficient, Variable variable);
LinearTerm operator*(Variable variable, double coefficient);
LinearTerm operator/(LinearTerm term, double coefficient);
LinearTerm operator/(Variable variable, double coefficient);

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
class LinearExpression final : public ModelStorageItemContainer {
 public:
  // For unit testing purpose, we define optional counters. We have to
  // explicitly define the default constructor, copy constructor and assignment
  // operators in that case. Else we use the defaults.
#ifndef MATH_OPT_USE_EXPRESSION_COUNTERS
  LinearExpression() = default;
  LinearExpression(const LinearExpression& other) = default;
#else   // MATH_OPT_USE_EXPRESSION_COUNTERS
  LinearExpression();
  LinearExpression(const LinearExpression& other);
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
        // Usually users should use the overloads of operators to build linear
  // expressions. For example, assuming `x` and `y` are Variable, then `x + 2*y
  // + 5` will build a LinearExpression automatically.
  LinearExpression(std::initializer_list<LinearTerm> terms, double offset);
  LinearExpression(double offset);           // NOLINT
  LinearExpression(Variable variable);       // NOLINT
  LinearExpression(const LinearTerm& term);  // NOLINT
  LinearExpression& operator=(const LinearExpression& other) = default;
  // A moved-from `LinearExpression` is the zero expression: it's not associated
  // to a storage, has no terms and its offset is zero.
  LinearExpression(LinearExpression&& other) noexcept;
  LinearExpression& operator=(LinearExpression&& other) noexcept;

  LinearExpression& operator+=(const LinearExpression& other);
  LinearExpression& operator+=(const LinearTerm& term);
  LinearExpression& operator+=(Variable variable);
  LinearExpression& operator+=(double value);
  LinearExpression& operator-=(const LinearExpression& other);
  LinearExpression& operator-=(const LinearTerm& term);
  LinearExpression& operator-=(Variable variable);
  LinearExpression& operator-=(double value);
  LinearExpression& operator*=(double value);
  LinearExpression& operator/=(double value);

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
  void AddSum(const Iterable& items);

  // Creates a new LinearExpression object equal to the sum. The implementation
  // is equivalent to:
  //   LinearExpression expr;
  //   expr.AddSum(items);
  template <typename Iterable>
  static LinearExpression Sum(const Iterable& items);

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
  void AddInnerProduct(const LeftIterable& left, const RightIterable& right);

  // Creates a new LinearExpression object equal to the inner product. The
  // implementation is equivalent to:
  //   LinearExpression expr;
  //   expr.AddInnerProduct(left, right);
  template <typename LeftIterable, typename RightIterable>
  static LinearExpression InnerProduct(const LeftIterable& left,
                                       const RightIterable& right);

  // Returns the terms in this expression.
  const VariableMap<double>& terms() const;
  double offset() const;

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values.
  //
  // Will CHECK fail if a variable in terms() is missing from variables_values.
  double Evaluate(const VariableMap<double>& variable_values) const;

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values, or zero if missing from the map.
  //
  // This function won't check that the variables in the input map are indeed in
  // the same model as the ones of the expression.
  double EvaluateWithDefaultZero(
      const VariableMap<double>& variable_values) const;

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

  // Invariants:
  // * storage() == v.storage()for each v in terms_;
  // * storage() == nullptr, if terms_ is empty.
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
LinearExpression Sum(const Iterable& items);

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
LinearExpression InnerProduct(const LeftIterable& left,
                              const RightIterable& right);

std::ostream& operator<<(std::ostream& ostr,
                         const LinearExpression& expression);

// We intentionally pass one of the LinearExpression argument by value so
// that we don't make unnecessary copies of temporary objects by using the move
// constructor and the returned values optimization (RVO).
LinearExpression operator-(LinearExpression expr);
LinearExpression operator+(Variable lhs, double rhs);
LinearExpression operator+(double lhs, Variable rhs);
LinearExpression operator+(Variable lhs, Variable rhs);
LinearExpression operator+(const LinearTerm& lhs, double rhs);
LinearExpression operator+(double lhs, const LinearTerm& rhs);
LinearExpression operator+(const LinearTerm& lhs, Variable rhs);
LinearExpression operator+(Variable lhs, const LinearTerm& rhs);
LinearExpression operator+(const LinearTerm& lhs, const LinearTerm& rhs);
LinearExpression operator+(LinearExpression lhs, double rhs);
LinearExpression operator+(double lhs, LinearExpression rhs);
LinearExpression operator+(LinearExpression lhs, Variable rhs);
LinearExpression operator+(Variable lhs, LinearExpression rhs);
LinearExpression operator+(LinearExpression lhs, const LinearTerm& rhs);
LinearExpression operator+(LinearTerm lhs, LinearExpression rhs);
LinearExpression operator+(LinearExpression lhs, const LinearExpression& rhs);
LinearExpression operator-(Variable lhs, double rhs);
LinearExpression operator-(double lhs, Variable rhs);
LinearExpression operator-(Variable lhs, Variable rhs);
LinearExpression operator-(const LinearTerm& lhs, double rhs);
LinearExpression operator-(double lhs, const LinearTerm& rhs);
LinearExpression operator-(const LinearTerm& lhs, Variable rhs);
LinearExpression operator-(Variable lhs, const LinearTerm& rhs);
LinearExpression operator-(const LinearTerm& lhs, const LinearTerm& rhs);
LinearExpression operator-(LinearExpression lhs, double rhs);
LinearExpression operator-(double lhs, LinearExpression rhs);
LinearExpression operator-(LinearExpression lhs, Variable rhs);
LinearExpression operator-(Variable lhs, LinearExpression rhs);
LinearExpression operator-(LinearExpression lhs, const LinearTerm& rhs);
LinearExpression operator-(LinearTerm lhs, LinearExpression rhs);
LinearExpression operator-(LinearExpression lhs, const LinearExpression& rhs);
LinearExpression operator*(LinearExpression lhs, double rhs);
LinearExpression operator*(double lhs, LinearExpression rhs);
LinearExpression operator/(LinearExpression lhs, double rhs);

// A LinearExpression with a lower bound.
struct LowerBoundedLinearExpression {
  // Users are not expected to use this constructor. Instead, they should build
  // this object using overloads of the >= and <= operators. For example, `x + y
  // >= 3`.
  LowerBoundedLinearExpression(LinearExpression expression, double lower_bound);
  LinearExpression expression;
  double lower_bound;
};

// A LinearExpression with an upper bound.
struct UpperBoundedLinearExpression {
  // Users are not expected to use this constructor. Instead they should build
  // this object using overloads of the >= and <= operators. For example, `x + y
  // <= 3`.
  UpperBoundedLinearExpression(LinearExpression expression, double upper_bound);
  LinearExpression expression;
  double upper_bound;
};

// A LinearExpression with upper and lower bounds.
struct BoundedLinearExpression {
  // Users are not expected to use this constructor. Instead they should build
  // this object using overloads of the >=, <=, and == operators. For example,
  // `3 <= x + y <= 3`.
  BoundedLinearExpression(LinearExpression expression, double lower_bound,
                          double upper_bound);
  // Users are not expected to use this constructor. This implicit conversion
  // will be used where a BoundedLinearExpression is expected and the user uses
  // == comparison of two variables. For example `AddLinearConstraint(x == y);`.
  BoundedLinearExpression(  // NOLINT
      const internal::VariablesEquality& eq);
  BoundedLinearExpression(  // NOLINT
      LowerBoundedLinearExpression lb_expression);
  BoundedLinearExpression(  // NOLINT
      UpperBoundedLinearExpression ub_expression);

  // Returns the actual lower_bound after taking into account the linear
  // expression offset.
  double lower_bound_minus_offset() const;
  // Returns the actual upper_bound after taking into account the linear
  // expression offset.
  double upper_bound_minus_offset() const;

  LinearExpression expression;
  double lower_bound;
  double upper_bound;
};

std::ostream& operator<<(std::ostream& ostr,
                         const BoundedLinearExpression& bounded_expression);

// We intentionally pass the LinearExpression argument by value so that we don't
// make unnecessary copies of temporary objects by using the move constructor
// and the returned values optimization (RVO).
LowerBoundedLinearExpression operator>=(LinearExpression expression,
                                        double constant);
LowerBoundedLinearExpression operator<=(double constant,
                                        LinearExpression expression);
LowerBoundedLinearExpression operator>=(const LinearTerm& term,
                                        double constant);
LowerBoundedLinearExpression operator<=(double constant,
                                        const LinearTerm& term);
LowerBoundedLinearExpression operator>=(Variable variable, double constant);
LowerBoundedLinearExpression operator<=(double constant, Variable variable);
UpperBoundedLinearExpression operator<=(LinearExpression expression,
                                        double constant);
UpperBoundedLinearExpression operator>=(double constant,
                                        LinearExpression expression);
UpperBoundedLinearExpression operator<=(const LinearTerm& term,
                                        double constant);
UpperBoundedLinearExpression operator>=(double constant,
                                        const LinearTerm& term);
UpperBoundedLinearExpression operator<=(Variable variable, double constant);
UpperBoundedLinearExpression operator>=(double constant, Variable variable);

// We intentionally pass the UpperBoundedLinearExpression and
// LowerBoundedLinearExpression arguments by value so that we don't
// make unnecessary copies of temporary objects by using the move constructor
// and the returned values optimization (RVO).
BoundedLinearExpression operator<=(LowerBoundedLinearExpression lhs,
                                   double rhs);
BoundedLinearExpression operator>=(double lhs,
                                   LowerBoundedLinearExpression rhs);
BoundedLinearExpression operator>=(UpperBoundedLinearExpression lhs,
                                   double rhs);
BoundedLinearExpression operator<=(double lhs,
                                   UpperBoundedLinearExpression rhs);
// We intentionally pass one LinearExpression argument by value so that we don't
// make unnecessary copies of temporary objects by using the move constructor
// and the returned values optimization (RVO).
BoundedLinearExpression operator<=(LinearExpression lhs,
                                   const LinearExpression& rhs);
BoundedLinearExpression operator>=(LinearExpression lhs,
                                   const LinearExpression& rhs);
BoundedLinearExpression operator<=(LinearExpression lhs, const LinearTerm& rhs);
BoundedLinearExpression operator>=(LinearExpression lhs, const LinearTerm& rhs);
BoundedLinearExpression operator<=(const LinearTerm& lhs, LinearExpression rhs);
BoundedLinearExpression operator>=(const LinearTerm& lhs, LinearExpression rhs);
BoundedLinearExpression operator<=(LinearExpression lhs, Variable rhs);
BoundedLinearExpression operator>=(LinearExpression lhs, Variable rhs);
BoundedLinearExpression operator<=(Variable lhs, LinearExpression rhs);
BoundedLinearExpression operator>=(Variable lhs, LinearExpression rhs);
BoundedLinearExpression operator<=(const LinearTerm& lhs,
                                   const LinearTerm& rhs);
BoundedLinearExpression operator>=(const LinearTerm& lhs,
                                   const LinearTerm& rhs);
BoundedLinearExpression operator<=(const LinearTerm& lhs, Variable rhs);
BoundedLinearExpression operator>=(const LinearTerm& lhs, Variable rhs);
BoundedLinearExpression operator<=(Variable lhs, const LinearTerm& rhs);
BoundedLinearExpression operator>=(Variable lhs, const LinearTerm& rhs);
BoundedLinearExpression operator<=(Variable lhs, Variable rhs);
BoundedLinearExpression operator>=(Variable lhs, Variable rhs);
BoundedLinearExpression operator==(LinearExpression lhs,
                                   const LinearExpression& rhs);
BoundedLinearExpression operator==(LinearExpression lhs, const LinearTerm& rhs);
BoundedLinearExpression operator==(const LinearTerm& lhs, LinearExpression rhs);
BoundedLinearExpression operator==(LinearExpression lhs, Variable rhs);
BoundedLinearExpression operator==(Variable lhs, LinearExpression rhs);
BoundedLinearExpression operator==(LinearExpression lhs, double rhs);
BoundedLinearExpression operator==(double lhs, LinearExpression rhs);
BoundedLinearExpression operator==(const LinearTerm& lhs,
                                   const LinearTerm& rhs);
BoundedLinearExpression operator==(const LinearTerm& lhs, Variable rhs);
BoundedLinearExpression operator==(Variable lhs, const LinearTerm& rhs);
BoundedLinearExpression operator==(const LinearTerm& lhs, double rhs);
BoundedLinearExpression operator==(double lhs, const LinearTerm& rhs);
BoundedLinearExpression operator==(Variable lhs, double rhs);
BoundedLinearExpression operator==(double lhs, Variable rhs);

// Id type used for quadratic terms, i.e. products of two variables.
using QuadraticProductId = std::pair<VariableId, VariableId>;

// Couples a QuadraticProductId with a ModelStorage, for use with IdMaps.
// Namely, this key type satisfies the requirements stated in key_types.h.
// Invariant:
//   * variable_ids_.first <= variable_ids_.second. The constructor will
//     silently correct this if not satisfied by the inputs.
//
// This type can be used as a key in ABSL hash containers.
class QuadraticTermKey final : public ModelStorageItem {
 public:
  // NOTE: this definition is for use by IdMap; clients should not rely upon it.
  using IdType = QuadraticProductId;

  // NOTE: This constructor will silently re-order the passed id so that, upon
  // exiting the constructor, variable_ids_.first <= variable_ids_.second.
  QuadraticTermKey(ModelStorageCPtr storage, QuadraticProductId id);
  // NOTE: This constructor will CHECK fail if the variable models do not agree,
  // i.e. first_variable.storage() != second_variable.storage(). It will also
  // silently re-order the passed id so that, upon exiting the constructor,
  // variable_ids_.first <= variable_ids_.second.
  QuadraticTermKey(Variable first_variable, Variable second_variable);

  QuadraticProductId typed_id() const;

  // Returns the Variable with the smallest id.
  Variable first() const { return Variable(storage(), variable_ids_.first); }

  // Returns the Variable the largest id.
  Variable second() const { return Variable(storage(), variable_ids_.second); }

  template <typename H>
  friend H AbslHashValue(H h, const QuadraticTermKey& key);

 private:
  QuadraticProductId variable_ids_;
};

std::ostream& operator<<(std::ostream& ostr, const QuadraticTermKey& key);

bool operator==(QuadraticTermKey lhs, QuadraticTermKey rhs);
bool operator!=(QuadraticTermKey lhs, QuadraticTermKey rhs);

// Represents a quadratic term in a sum: coefficient * variable_1 * variable_2.
// Invariant:
//   * first_variable.storage() == second_variable.storage(). The constructor
//     will CHECK fail if not satisfied.
class QuadraticTerm {
 public:
  QuadraticTerm() = delete;
  // NOTE: This will CHECK fail if
  // first_variable.storage() != second_variable.storage().
  QuadraticTerm(Variable first_variable, Variable second_variable,
                double coefficient);

  double coefficient() const;
  Variable first_variable() const;
  Variable second_variable() const;

  // This is useful for working with IdMaps
  QuadraticTermKey GetKey() const;

  QuadraticTerm& operator*=(double value);
  QuadraticTerm& operator/=(double value);

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
QuadraticTerm operator-(QuadraticTerm term);
QuadraticTerm operator*(double lhs, QuadraticTerm rhs);
QuadraticTerm operator*(Variable lhs, Variable rhs);
QuadraticTerm operator*(Variable lhs, LinearTerm rhs);
QuadraticTerm operator*(LinearTerm lhs, Variable rhs);
QuadraticTerm operator*(LinearTerm lhs, LinearTerm rhs);
QuadraticTerm operator*(QuadraticTerm lhs, double rhs);
QuadraticTerm operator/(QuadraticTerm lhs, double rhs);

template <typename V>
using QuadraticTermMap = absl::flat_hash_map<QuadraticTermKey, V>;

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
class QuadraticExpression final : public ModelStorageItemContainer {
 public:
  // For unit testing purpose, we define optional counters. We have to
  // explicitly define the default constructor, copy constructor and assignment
  // operators in that case. Else we use the defaults.
#ifndef MATH_OPT_USE_EXPRESSION_COUNTERS
  QuadraticExpression() = default;
  QuadraticExpression(const QuadraticExpression& other) = default;
#else   // MATH_OPT_USE_EXPRESSION_COUNTERS
  QuadraticExpression();
  QuadraticExpression(const QuadraticExpression& other);
#endif  // MATH_OPT_USE_EXPRESSION_COUNTERS
  // Users should prefer the default constructor and operator overloads to build
  // expressions.
  QuadraticExpression(std::initializer_list<QuadraticTerm> quadratic_terms,
                      std::initializer_list<LinearTerm> linear_terms,
                      double offset);
  QuadraticExpression(double offset);              // NOLINT
  QuadraticExpression(Variable variable);          // NOLINT
  QuadraticExpression(const LinearTerm& term);     // NOLINT
  QuadraticExpression(LinearExpression expr);      // NOLINT
  QuadraticExpression(const QuadraticTerm& term);  // NOLINT
  QuadraticExpression& operator=(const QuadraticExpression& other) = default;
  // A moved-from `LinearExpression` is the zero expression: it's not associated
  // to a storage, has no terms and its offset is zero.
  QuadraticExpression(QuadraticExpression&& other) noexcept;
  QuadraticExpression& operator=(QuadraticExpression&& other) noexcept;

  double offset() const;
  const VariableMap<double>& linear_terms() const;
  const QuadraticTermMap<double>& quadratic_terms() const;

  QuadraticExpression& operator+=(double value);
  QuadraticExpression& operator+=(Variable variable);
  QuadraticExpression& operator+=(const LinearTerm& term);
  QuadraticExpression& operator+=(const LinearExpression& expr);
  QuadraticExpression& operator+=(const QuadraticTerm& term);
  QuadraticExpression& operator+=(const QuadraticExpression& expr);
  QuadraticExpression& operator-=(double value);
  QuadraticExpression& operator-=(Variable variable);
  QuadraticExpression& operator-=(const LinearTerm& term);
  QuadraticExpression& operator-=(const LinearExpression& expr);
  QuadraticExpression& operator-=(const QuadraticTerm& term);
  QuadraticExpression& operator-=(const QuadraticExpression& expr);
  QuadraticExpression& operator*=(double value);
  QuadraticExpression& operator/=(double value);

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
  void AddSum(const Iterable& items);

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
  static QuadraticExpression Sum(const Iterable& items);

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
  void AddInnerProduct(const LeftIterable& left, const RightIterable& right);

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
  static QuadraticExpression InnerProduct(const LeftIterable& left,
                                          const RightIterable& right);

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values.
  //
  // Will CHECK fail if a variable in linear_terms() or quadratic_terms() is
  // missing from variables_values.
  double Evaluate(const VariableMap<double>& variable_values) const;

  // Compute the numeric value of this expression when variables are substituted
  // by their values in variable_values, or zero if missing from the map.
  //
  // This function won't check that the variables in the input map are indeed in
  // the same model as the ones of the expression.
  double EvaluateWithDefaultZero(
      const VariableMap<double>& variable_values) const;

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

  // Invariants:
  // * storage() == v.storage() for each v in linear_terms_;
  // * storage() == v.storage() for each v in quadratic_terms_;
  // * storage() == nullptr, if both terms_ and quadratic_terms_ are empty.
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
QuadraticExpression operator-(QuadraticExpression expr);

// The binary methods, listed in lexicographic order based on
// (operator, lhs type #, rhs type #), with the type #s are listed above, are:
QuadraticExpression operator+(double lhs, const QuadraticTerm& rhs);
QuadraticExpression operator+(double lhs, QuadraticExpression rhs);
QuadraticExpression operator+(Variable lhs, const QuadraticTerm& rhs);
QuadraticExpression operator+(Variable lhs, QuadraticExpression rhs);
QuadraticExpression operator+(const LinearTerm& lhs, const QuadraticTerm& rhs);
QuadraticExpression operator+(const LinearTerm& lhs, QuadraticExpression rhs);
QuadraticExpression operator+(LinearExpression lhs, const QuadraticTerm& rhs);
QuadraticExpression operator+(const LinearExpression& lhs,
                              QuadraticExpression rhs);
QuadraticExpression operator+(const QuadraticTerm& lhs, double rhs);
QuadraticExpression operator+(const QuadraticTerm& lhs, Variable rhs);
QuadraticExpression operator+(const QuadraticTerm& lhs, const LinearTerm& rhs);
QuadraticExpression operator+(const QuadraticTerm& lhs, LinearExpression rhs);
QuadraticExpression operator+(const QuadraticTerm& lhs,
                              const QuadraticTerm& rhs);
QuadraticExpression operator+(const QuadraticTerm& lhs,
                              QuadraticExpression rhs);
QuadraticExpression operator+(QuadraticExpression lhs, double rhs);
QuadraticExpression operator+(QuadraticExpression lhs, Variable rhs);
QuadraticExpression operator+(QuadraticExpression lhs, const LinearTerm& rhs);
QuadraticExpression operator+(QuadraticExpression lhs,
                              const LinearExpression& rhs);
QuadraticExpression operator+(QuadraticExpression lhs,
                              const QuadraticTerm& rhs);
QuadraticExpression operator+(QuadraticExpression lhs,
                              const QuadraticExpression& rhs);

QuadraticExpression operator-(double lhs, const QuadraticTerm& rhs);
QuadraticExpression operator-(double lhs, QuadraticExpression rhs);
QuadraticExpression operator-(Variable lhs, const QuadraticTerm& rhs);
QuadraticExpression operator-(Variable lhs, QuadraticExpression rhs);
QuadraticExpression operator-(const LinearTerm& lhs, const QuadraticTerm& rhs);
QuadraticExpression operator-(const LinearTerm& lhs, QuadraticExpression rhs);
QuadraticExpression operator-(LinearExpression lhs, const QuadraticTerm& rhs);
QuadraticExpression operator-(const LinearExpression& lhs,
                              QuadraticExpression rhs);
QuadraticExpression operator-(const QuadraticTerm& lhs, double rhs);
QuadraticExpression operator-(const QuadraticTerm& lhs, Variable rhs);
QuadraticExpression operator-(const QuadraticTerm& lhs, const LinearTerm& rhs);
QuadraticExpression operator-(const QuadraticTerm& lhs, LinearExpression rhs);
QuadraticExpression operator-(const QuadraticTerm& lhs,
                              const QuadraticTerm& rhs);
QuadraticExpression operator-(const QuadraticTerm& lhs,
                              QuadraticExpression rhs);
QuadraticExpression operator-(QuadraticExpression lhs, double rhs);
QuadraticExpression operator-(QuadraticExpression lhs, Variable rhs);
QuadraticExpression operator-(QuadraticExpression lhs, const LinearTerm& rhs);
QuadraticExpression operator-(QuadraticExpression lhs,
                              const LinearExpression& rhs);
QuadraticExpression operator-(QuadraticExpression lhs,
                              const QuadraticTerm& rhs);
QuadraticExpression operator-(QuadraticExpression lhs,
                              const QuadraticExpression& rhs);

QuadraticExpression operator*(double lhs, QuadraticExpression rhs);
QuadraticExpression operator*(Variable lhs, const LinearExpression& rhs);
QuadraticExpression operator*(LinearTerm lhs, const LinearExpression& rhs);
QuadraticExpression operator*(const LinearExpression& lhs, Variable rhs);
QuadraticExpression operator*(const LinearExpression& lhs, LinearTerm rhs);
QuadraticExpression operator*(const LinearExpression& lhs,
                              const LinearExpression& rhs);
QuadraticExpression operator*(QuadraticExpression lhs, double rhs);

QuadraticExpression operator/(QuadraticExpression lhs, double rhs);

// A QuadraticExpression with a lower bound.
struct LowerBoundedQuadraticExpression {
  // Users are not expected to use this constructor. Instead, they should build
  // this object using overloads of the >= and <= operators. For example, `x * y
  // >= 3`.
  LowerBoundedQuadraticExpression(QuadraticExpression expression,
                                  double lower_bound);
  // Users are not expected to explicitly use the following constructor.
  LowerBoundedQuadraticExpression(  // NOLINT
      LowerBoundedLinearExpression lb_expression);

  QuadraticExpression expression;
  double lower_bound;
};

// A QuadraticExpression with an upper bound.
struct UpperBoundedQuadraticExpression {
  // Users are not expected to use this constructor. Instead, they should build
  // this object using overloads of the >= and <= operators. For example, `x * y
  // <= 3`.
  UpperBoundedQuadraticExpression(QuadraticExpression expression,
                                  double upper_bound);
  // Users are not expected to explicitly use the following constructor.
  UpperBoundedQuadraticExpression(  // NOLINT
      UpperBoundedLinearExpression ub_expression);

  QuadraticExpression expression;
  double upper_bound;
};

// A QuadraticExpression with upper and lower bounds.
struct BoundedQuadraticExpression {
  // Users are not expected to use this constructor. Instead, they should build
  // this object using overloads of the >=, <=, and == operators. For example,
  // `3 <= x * y <= 3`.
  BoundedQuadraticExpression(QuadraticExpression expression, double lower_bound,
                             double upper_bound);

  // Users are not expected to explicitly use the following constructors.
  BoundedQuadraticExpression(  // NOLINT
      internal::VariablesEquality var_equality);
  BoundedQuadraticExpression(  // NOLINT
      LowerBoundedLinearExpression lb_expression);
  BoundedQuadraticExpression(  // NOLINT
      UpperBoundedLinearExpression ub_expression);
  BoundedQuadraticExpression(  // NOLINT
      BoundedLinearExpression bounded_expression);
  BoundedQuadraticExpression(  // NOLINT
      LowerBoundedQuadraticExpression lb_expression);
  BoundedQuadraticExpression(  // NOLINT
      UpperBoundedQuadraticExpression ub_expression);

  // Returns the actual lower_bound after taking into account the quadratic
  // expression offset.
  double lower_bound_minus_offset() const;
  // Returns the actual upper_bound after taking into account the quadratic
  // expression offset.
  double upper_bound_minus_offset() const;

  QuadraticExpression expression;
  double lower_bound;
  double upper_bound;
};

std::ostream& operator<<(std::ostream& ostr,
                         const BoundedQuadraticExpression& bounded_expression);

// We intentionally pass the QuadraticExpression argument by value so that we
// don't make unnecessary copies of temporary objects by using the move
// constructor and the returned values optimization (RVO).
LowerBoundedQuadraticExpression operator>=(QuadraticExpression lhs, double rhs);
LowerBoundedQuadraticExpression operator>=(QuadraticTerm lhs, double rhs);
LowerBoundedQuadraticExpression operator<=(double lhs, QuadraticExpression rhs);
LowerBoundedQuadraticExpression operator<=(double lhs, QuadraticTerm rhs);

UpperBoundedQuadraticExpression operator>=(double lhs, QuadraticExpression rhs);
UpperBoundedQuadraticExpression operator>=(double lhs, QuadraticTerm rhs);
UpperBoundedQuadraticExpression operator<=(QuadraticExpression lhs, double rhs);
UpperBoundedQuadraticExpression operator<=(QuadraticTerm lhs, double rhs);

// We intentionally pass the UpperBoundedQuadraticExpression and
// LowerBoundedQuadraticExpression arguments by value so that we don't
// make unnecessary copies of temporary objects by using the move constructor
// and the returned values optimization (RVO).
BoundedQuadraticExpression operator>=(UpperBoundedQuadraticExpression lhs,
                                      double rhs);
BoundedQuadraticExpression operator>=(double lhs,
                                      LowerBoundedQuadraticExpression rhs);
BoundedQuadraticExpression operator<=(LowerBoundedQuadraticExpression lhs,
                                      double rhs);
BoundedQuadraticExpression operator<=(double lhs,
                                      UpperBoundedQuadraticExpression rhs);
// We intentionally pass one QuadraticExpression argument by value so that we
// don't make unnecessary copies of temporary objects by using the move
// constructor and the returned values optimization (RVO).

// Comparisons with lhs = QuadraticExpression
BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs);
BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      QuadraticTerm rhs);
BoundedQuadraticExpression operator>=(QuadraticExpression lhs,
                                      const LinearExpression& rhs);
BoundedQuadraticExpression operator>=(QuadraticExpression lhs, LinearTerm rhs);
BoundedQuadraticExpression operator>=(QuadraticExpression lhs, Variable rhs);
BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs);
BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      QuadraticTerm rhs);
BoundedQuadraticExpression operator<=(QuadraticExpression lhs,
                                      const LinearExpression& rhs);
BoundedQuadraticExpression operator<=(QuadraticExpression lhs, LinearTerm rhs);
BoundedQuadraticExpression operator<=(QuadraticExpression lhs, Variable rhs);
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const QuadraticExpression& rhs);
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      QuadraticTerm rhs);
BoundedQuadraticExpression operator==(QuadraticExpression lhs,
                                      const LinearExpression& rhs);
BoundedQuadraticExpression operator==(QuadraticExpression lhs, LinearTerm rhs);
BoundedQuadraticExpression operator==(QuadraticExpression lhs, Variable rhs);
BoundedQuadraticExpression operator==(QuadraticExpression lhs, double rhs);
// Comparisons with lhs = QuadraticTerm
BoundedQuadraticExpression operator>=(QuadraticTerm lhs,
                                      QuadraticExpression rhs);
BoundedQuadraticExpression operator>=(QuadraticTerm lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator>=(QuadraticTerm lhs, LinearExpression rhs);
BoundedQuadraticExpression operator>=(QuadraticTerm lhs, LinearTerm rhs);
BoundedQuadraticExpression operator>=(QuadraticTerm lhs, Variable rhs);
BoundedQuadraticExpression operator<=(QuadraticTerm lhs,
                                      QuadraticExpression rhs);
BoundedQuadraticExpression operator<=(QuadraticTerm lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator<=(QuadraticTerm lhs, LinearExpression rhs);
BoundedQuadraticExpression operator<=(QuadraticTerm lhs, LinearTerm rhs);
BoundedQuadraticExpression operator<=(QuadraticTerm lhs, Variable rhs);
BoundedQuadraticExpression operator==(QuadraticTerm lhs,
                                      QuadraticExpression rhs);
BoundedQuadraticExpression operator==(QuadraticTerm lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator==(QuadraticTerm lhs, LinearExpression rhs);
BoundedQuadraticExpression operator==(QuadraticTerm lhs, LinearTerm rhs);
BoundedQuadraticExpression operator==(QuadraticTerm lhs, Variable rhs);
BoundedQuadraticExpression operator==(QuadraticTerm lhs, double rhs);
// Comparisons with lhs = LinearExpression
BoundedQuadraticExpression operator>=(const LinearExpression& lhs,
                                      QuadraticExpression rhs);
BoundedQuadraticExpression operator>=(LinearExpression lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator<=(const LinearExpression& lhs,
                                      QuadraticExpression rhs);
BoundedQuadraticExpression operator<=(LinearExpression lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator==(const LinearExpression& lhs,
                                      QuadraticExpression rhs);
BoundedQuadraticExpression operator==(LinearExpression lhs, QuadraticTerm rhs);
// Comparisons with lhs = LinearTerm
BoundedQuadraticExpression operator>=(LinearTerm lhs, QuadraticExpression rhs);
BoundedQuadraticExpression operator>=(LinearTerm lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator<=(LinearTerm lhs, QuadraticExpression rhs);
BoundedQuadraticExpression operator<=(LinearTerm lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator==(LinearTerm lhs, QuadraticExpression rhs);
BoundedQuadraticExpression operator==(LinearTerm lhs, QuadraticTerm rhs);
// Comparisons with lhs = Variable
BoundedQuadraticExpression operator>=(Variable lhs, QuadraticExpression rhs);
BoundedQuadraticExpression operator>=(Variable lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator<=(Variable lhs, QuadraticExpression rhs);
BoundedQuadraticExpression operator<=(Variable lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator==(Variable lhs, QuadraticExpression rhs);
BoundedQuadraticExpression operator==(Variable lhs, QuadraticTerm rhs);
// Comparisons with lhs = Double
BoundedQuadraticExpression operator==(double lhs, QuadraticTerm rhs);
BoundedQuadraticExpression operator==(double lhs, QuadraticExpression rhs);

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Template implementations ////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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

template <typename H>
H AbslHashValue(H h, const QuadraticTermKey& key) {
  return H::combine(std::move(h), key.typed_id().first.value(),
                    key.typed_id().second.value(), key.storage());
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

}  // namespace math_opt
}  // namespace operations_research

#endif  // ORTOOLS_MATH_OPT_CPP_VARIABLE_AND_EXPRESSIONS_H_
