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

#ifndef OR_TOOLS_LINEAR_SOLVER_LINEAR_EXPR_H_
#define OR_TOOLS_LINEAR_SOLVER_LINEAR_EXPR_H_

/**
 * \file
 * This file allows you to write natural code (like a mathematical equation) to
 * model optimization problems with MPSolver. It is syntatic sugar on top of
 * the MPSolver API, it provides no additional functionality. Use of these APIs
 * makes it much easier to write code that is both simple and big-O optimal for
 * creating your model, at the cost of some additional constant factor
 * overhead. If model creation is a bottleneck in your problem, consider using
 * the MPSolver API directly instead.
 *
 * This file contains two classes:
 *   1. LinearExpr: models offset + sum_{i in S} a_i*x_i for decision var x_i,
 *   2. LinearRange: models lb <= sum_{i in S} a_i*x_i <= ub,
 * and it provides various operator overloads to build up "LinearExpr"s and
 * then convert them to "LinearRange"s.
 *
 * Recommended use (avoids dangerous code):
 *
 * \code
   MPSolver solver = ...;
   const LinearExpr x = solver.MakeVar(...);   * Note: implicit conversion
   const LinearExpr y = solver.MakeVar(...);
   const LinearExpr z = solver.MakeVar(...);
   const LinearExpr e1 = x + y;
   const LinearExpr e2 = (e1 + 7.0 + z)/3.0;
   const LinearRange r = e1 <= e2;
   solver.MakeRowConstraint(r);
   \endcode
 *
 * \b WARNING, AVOID THIS TRAP:
 *
 * \code
   MPSolver solver = ...;
   MPVariable* x = solver.MakeVar(...);
   LinearExpr y = x + 5;
   \endcode
 *
 * In evaluating "x+5" above, x is NOT converted to a LinearExpr before the
 * addition, but rather is treated as a pointer, so x+5 gives a new pointer to
 * garbage memory.
 *
 * For this reason, when using LinearExpr, it is best practice to:
 *   1. use double literals instead of ints (e.g. "x + 5.0", not "x + 5"),
 *   2. Immediately convert all MPVariable* to LinearExpr on creation, and only
 *      hold references to the "LinearExpr"s.
 *
 * Likewise, the following code is NOT recommended:
 * \code
   MPSolver solver = ...;
   MPVariable* x = solver.MakeVar(...);
   MPVariable* y = solver.MakeVar(...);
   LinearExpr e1 = LinearExpr(x) + y + 5;
   \endcode
 *
 * While it is correct, it violates the natural assumption that the + operator
 * is associative.  Thus you are setting a trap for future modifications of the
 * code, as any of the following changes would lead to the above failure mode:
 *
 *    * \code LinearExpr e1 = LinearExpr(x) + (y + 5); \endcode
 *    * \code LinearExpr e1 = y + 5 + LinearExpr(x); \endcode
 */

#include <ostream>
#include <string>

#include "absl/container/flat_hash_map.h"

namespace operations_research {

// NOTE(user): forward declaration is necessary due to cyclic dependencies,
// MPVariable is defined in linear_solver.h, which depends on LinearExpr.
class MPVariable;

/**
 * LinearExpr models a quantity that is linear in the decision variables
 * (MPVariable) of an optimization problem, i.e.
 *
 * offset + sum_{i in S} a_i*x_i,
 *
 * where the a_i and offset are constants and the x_i are MPVariables. You can
 * use a LinearExpr "linear_expr" with an MPSolver "solver" to:
 *   * Set as the objective of your optimization problem, e.g.
 *
 *     solver.MutableObjective()->MaximizeLinearExpr(linear_expr);
 *
 *   * Create a constraint in your optimization, e.g.
 *
 *     solver.MakeRowConstraint(linear_expr1 <= linear_expr2);
 *
 *   * Get the value of the quantity after solving, e.g.
 *
 *     solver.Solve();
 *     linear_expr.SolutionValue();
 *
 * LinearExpr is allowed to delete variables with coefficient zero from the map,
 * but is not obligated to do so.
 */
class LinearExpr {
 public:
  LinearExpr();
  /// Possible implicit conversions are intentional.
  LinearExpr(double constant);  // NOLINT

  /***
   * Possible implicit conversions are intentional.
   *
   * Warning: var is not owned.
   */
  LinearExpr(const MPVariable* var);  // NOLINT

  /**
   * Returns 1-var.
   *
   * NOTE(user): if var is binary variable, this corresponds to the logical
   * negation of var.
   * Passing by value is intentional, see the discussion on binary ops.
   */
  static LinearExpr NotVar(LinearExpr var);

  LinearExpr& operator+=(const LinearExpr& rhs);
  LinearExpr& operator-=(const LinearExpr& rhs);
  LinearExpr& operator*=(double rhs);
  LinearExpr& operator/=(double rhs);
  LinearExpr operator-() const;

  double offset() const { return offset_; }
  const absl::flat_hash_map<const MPVariable*, double>& terms() const {
    return terms_;
  }

  /**
   * Evaluates the value of this expression at the solution found.
   *
   * It must be called only after calling MPSolver::Solve.
   */
  double SolutionValue() const;

  /**
   * A human readable representation of this. Variables will be printed in order
   * of lowest index first.
   */
  std::string ToString() const;

 private:
  double offset_;
  absl::flat_hash_map<const MPVariable*, double> terms_;
};

std::ostream& operator<<(std::ostream& stream, const LinearExpr& linear_expr);

// NOTE(user): in the ops below, the non-"const LinearExpr&" are intentional.
// We need to create a new LinearExpr for the result, so we lose nothing by
// passing one argument by value, mutating it, and then returning it. In
// particular, this allows (with move semantics and RVO) an optimized
// evaluation of expressions such as
// a + b + c + d
// (see http://en.cppreference.com/w/cpp/language/operators).
LinearExpr operator+(LinearExpr lhs, const LinearExpr& rhs);
LinearExpr operator-(LinearExpr lhs, const LinearExpr& rhs);
LinearExpr operator*(LinearExpr lhs, double rhs);
LinearExpr operator/(LinearExpr lhs, double rhs);
LinearExpr operator*(double lhs, LinearExpr rhs);

/**
 * An expression of the form:
 *
 * \code lower_bound <= sum_{i in S} a_i*x_i <= upper_bound. \endcode
 * The sum is represented as a LinearExpr with offset 0.
 *
 * Must be added to model with
 * \code
   MPSolver::AddRowConstraint(const LinearRange& range,
                              const std::string& name);
   \endcode
 */
class LinearRange {
 public:
  LinearRange() : lower_bound_(0), upper_bound_(0) {}
  /**
   * The bounds of the linear range are updated so that they include the offset
   * from "linear_expr", i.e., we form the range:
   * \code
     lower_bound - offset <= linear_expr - offset <= upper_bound - offset.
     \endcode
   */
  LinearRange(double lower_bound, const LinearExpr& linear_expr,
              double upper_bound);

  double lower_bound() const { return lower_bound_; }
  const LinearExpr& linear_expr() const { return linear_expr_; }
  double upper_bound() const { return upper_bound_; }

 private:
  double lower_bound_;
  // invariant: linear_expr_.offset() == 0.
  LinearExpr linear_expr_;
  double upper_bound_;
};

LinearRange operator<=(const LinearExpr& lhs, const LinearExpr& rhs);
LinearRange operator==(const LinearExpr& lhs, const LinearExpr& rhs);
LinearRange operator>=(const LinearExpr& lhs, const LinearExpr& rhs);

// TODO(user): explore defining more overloads to support:
// solver.AddRowConstraint(0.0 <= x + y + z <= 1.0);

}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_LINEAR_EXPR_H_
