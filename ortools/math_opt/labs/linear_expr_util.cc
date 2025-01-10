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

#include "ortools/math_opt/labs/linear_expr_util.h"

#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {
namespace {

double ComputeBound(const LinearExpression& linear_expression,
                    bool is_upper_bound) {
  // The algorithm used is as follows:
  //  (1) Make a list of the terms to add up, e.g.
  //        [offset, x1.lb()*c1, x3.ub()*c3]
  //  (2) Sort the list by {abs(x), x} lexicographically
  //  (3) Sum up the values from the smallest absolute value to largest.
  // The result will give deterministic output with reasonable precision.
  std::vector<double> terms_to_add;
  terms_to_add.reserve(linear_expression.terms().size() + 1);
  terms_to_add.push_back(linear_expression.offset());
  for (const auto [var, coef] : linear_expression.terms()) {
    const bool use_ub =
        (is_upper_bound && coef > 0) || (!is_upper_bound && coef < 0);
    const double val =
        use_ub ? var.upper_bound() * coef : var.lower_bound() * coef;
    terms_to_add.push_back(val);
  }
  // all values in terms_to_add are finite.
  absl::c_sort(terms_to_add, [](const double left, const double right) {
    return std::pair(std::abs(left), left) < std::pair(std::abs(right), right);
  });
  return absl::c_accumulate(terms_to_add, 0.0);
}

}  // namespace

double LowerBound(const LinearExpression& linear_expression) {
  return ComputeBound(linear_expression, /*is_upper_bound=*/false);
}

double UpperBound(const LinearExpression& linear_expression) {
  return ComputeBound(linear_expression, /*is_upper_bound=*/true);
}

}  // namespace operations_research::math_opt
