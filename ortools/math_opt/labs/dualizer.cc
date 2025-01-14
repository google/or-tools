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

// Let L be a matrix and b a vector so that a(w) = L * w + b. Then
//
// max_w{ a(w) * x : w in W} = max_w{ w' * L' * x : w in W} + b * x
//
// where ' is the transpose operation. Because of this we can focus on
// max_w{ l(w) * x : w in W}.
//
// We need the dual to be an LP even when uncertainty_model contains ranged
// constraints, so we use the LP reformulation of go/mathopt-dual from
// go/mathopt-traditional-dual#lp-reformulation-split. Using
// that reformulation, for any fixed x the dual of max_w{ w' * L' * x : w in W}
// is
//
// min_{y, yp, yn, r, rp, rn}    obj(yp, yn, rp, rn)
//
//                       A' y + r == L' * x
//                       sign constraints on y and r
//                        yp + yn == y
//                        rp + rn == r
//                         yp, rp >= 0
//                         yn, rn <= 0
//
// where
//
//   obj(yp, yn, rp, rn) = uc * yp + lc * yn + uv * rp + lv * rn
//
// with the convention 0 * infinity = 0 * -infinity = 0.
//
// In this dual form x is not multiplied with w so we can consider x a variable
// instead of a fixed value.
//
// Then max_w{ a(w) * x : w in W} <= rhs is equivalent to
//
//          obj(yp, yn, rp, rn) + b * x <= rhs
//                             A' y + r == L' * x
//                             sign constraints on y and r
//                              yp + yn == y
//                              rp + rn == r
//                               yp, rp >= 0
//                               yn, rn <= 0
//
// Note that we can use the equalities yp + yn == y and rp + rn == r to
// eliminate variables y and r to reduce the number of constraints and variables
// in the reformulation.

#include "ortools/math_opt/labs/dualizer.h"

#include <limits>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "ortools/base/map_util.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research {
namespace math_opt {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

class RobustConstraintDualizer {
 public:
  RobustConstraintDualizer(
      const Model& uncertainty_model, Variable rhs,
      absl::Span<const std::pair<LinearExpression, Variable>>
          uncertain_coefficients,
      Model& main_model);

 private:
  LinearExpression AddDualizedVariable(double lower_bound, double upper_bound);
  Model& main_model_;

  // Over the variables of main_model
  LinearExpression objective_expression_;
  // The keys are from the uncertain model, the values are from main_model
  absl::flat_hash_map<LinearConstraint, LinearExpression> y_;
  // The keys are from the uncertain model, the values are from main_model
  absl::flat_hash_map<Variable, LinearExpression> r_;
};

// Let
// (var, varp, varn, lower_bound, upper_bound) = (y_i, yp_i, yn_i, lc_i, uc_i)
// or
// (var, varp, varn, lower_bound, upper_bound) = (r_j, rp_j, rn_j, lv_j, uv_j).
//
// The constraints from go/mathopt-traditional-dual#lp-reformulation-split that
// only involve var, varp and varn are (note that our dual has a max objective):
//
//           var >= 0    if    lower_bound = -infinity
//           var <= 0    if    upper_bound = +infinity
//   varp + varn == var
//          varp >= 0
//          varn <= 0
//
// and the corresponding term in obj(yp, yn, rp, rn) is
//
//   upper_bound * varp + lower_bound * varn
//
// The following function adds varp and varn, updates the expression for
// obj(yp, yn, rp, rn) with the associated term and returns the expression for
// var. The function uses the sign constraints on var, varp and varn and the
// values of lower_bound and upper_bound to minimize the number of created
// variables.
LinearExpression RobustConstraintDualizer::AddDualizedVariable(
    const double lower_bound, const double upper_bound) {
  if ((lower_bound <= -kInf) && (upper_bound >= kInf)) {
    return 0.0;
  } else if (lower_bound <= -kInf) {
    const Variable varp = main_model_.AddContinuousVariable(0.0, kInf);
    objective_expression_ += upper_bound * varp;
    return varp;
  } else if (upper_bound >= kInf) {
    const Variable varn = main_model_.AddContinuousVariable(-kInf, 0.0);
    objective_expression_ += lower_bound * varn;
    return varn;
  } else if (lower_bound == upper_bound) {
    const Variable var = main_model_.AddContinuousVariable(-kInf, kInf);
    objective_expression_ += lower_bound * var;
    return var;
  } else {
    const Variable varp = main_model_.AddContinuousVariable(0.0, kInf);
    const Variable varn = main_model_.AddContinuousVariable(-kInf, 0.0);
    objective_expression_ += upper_bound * varp + lower_bound * varn;
    return varp + varn;
  }
}

// L' * x
absl::flat_hash_map<Variable, LinearExpression> TransposeUncertainCoefficients(
    absl::Span<const std::pair<LinearExpression, Variable>>
        uncertain_coefficients) {
  absl::flat_hash_map<Variable, LinearExpression> result;
  for (const auto& [expression, main_model_variable] : uncertain_coefficients) {
    for (const auto [v, coefficient] : expression.terms()) {
      result[v] += coefficient * main_model_variable;
    }
  }
  return result;
}

RobustConstraintDualizer::RobustConstraintDualizer(
    const Model& uncertainty_model, const Variable rhs,
    absl::Span<const std::pair<LinearExpression, Variable>>
        uncertain_coefficients,
    Model& main_model)
    : main_model_(main_model) {
  const std::vector<Variable> uncertainty_variables =
      uncertainty_model.SortedVariables();
  const std::vector<LinearConstraint> uncertainty_constraints =
      uncertainty_model.SortedLinearConstraints();
  for (const LinearConstraint c : uncertainty_constraints) {
    y_.insert({c, AddDualizedVariable(c.lower_bound(), c.upper_bound())});
  }
  for (const Variable v : uncertainty_variables) {
    r_.insert({v, AddDualizedVariable(v.lower_bound(), v.upper_bound())});
  }

  // Add obj(yp, yn, rp, rn) + b * x <= rhs
  {
    LinearExpression offset_expression;
    for (const auto& [expression, variable] : uncertain_coefficients) {
      offset_expression += expression.offset() * variable;
    }
    main_model_.AddLinearConstraint(objective_expression_ + offset_expression <=
                                    rhs);
  }
  {  // Add A' y + r = L' * x
    const absl::flat_hash_map<Variable, LinearExpression>
        equality_rhs_expressions =
            TransposeUncertainCoefficients(uncertain_coefficients);
    for (const Variable v : uncertainty_variables) {
      LinearExpression equality_lhs_expression = r_.at(v);
      for (const LinearConstraint c : uncertainty_model.ColumnNonzeros(v)) {
        equality_lhs_expression += c.coefficient(v) * y_.at(c);
      }
      main_model_.AddLinearConstraint(
          equality_lhs_expression ==
          gtl::FindWithDefault(equality_rhs_expressions, v));
    }
  }
}

}  // namespace

void AddRobustConstraint(const Model& uncertainty_model, const Variable rhs,
                         absl::Span<const std::pair<LinearExpression, Variable>>
                             uncertain_coefficients,
                         Model& main_model) {
  RobustConstraintDualizer dualizer(uncertainty_model, rhs,
                                    uncertain_coefficients, main_model);
}

}  // namespace math_opt
}  // namespace operations_research
