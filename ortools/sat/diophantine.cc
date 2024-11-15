// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/diophantine.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <vector>

#include "absl/log/check.h"
#include "absl/numeric/int128.h"
#include "absl/types/span.h"
#include "ortools/base/mathutil.h"
#include "ortools/sat/util.h"

namespace operations_research::sat {

namespace {

int64_t Gcd(const absl::Span<const int64_t> coeffs) {
  DCHECK(coeffs[0] != std::numeric_limits<int64_t>::min());
  int64_t gcd = std::abs(coeffs[0]);
  for (int i = 1; i < coeffs.size(); ++i) {
    DCHECK(coeffs[i] != std::numeric_limits<int64_t>::min());
    const int64_t abs_coeff = std::abs(coeffs[i]);
    gcd = std::gcd(gcd, abs_coeff);
  }
  return gcd;
}

}  // namespace

void ReduceModuloBasis(absl::Span<const std::vector<absl::int128>> basis,
                       const int elements_to_consider,
                       std::vector<absl::int128>& v) {
  DCHECK(!basis.empty());
  for (int i = elements_to_consider - 1; i >= 0; --i) {
    const int n = static_cast<int>(basis[i].size()) - 1;
    const absl::int128 leading_coeff = basis[i][n];
    if (leading_coeff == 0) continue;

    const absl::int128 q =
        leading_coeff > 0
            ? MathUtil::FloorOfRatio(
                  v[n] + MathUtil::FloorOfRatio(leading_coeff, absl::int128(2)),
                  leading_coeff)
            : -MathUtil::FloorOfRatio(
                  v[n] +
                      MathUtil::FloorOfRatio(-leading_coeff, absl::int128(2)),
                  -leading_coeff);
    if (q == 0) continue;
    for (int j = 0; j <= n; ++j) v[j] -= q * basis[i][j];
  }
}

std::vector<int> GreedyFastDecreasingGcd(
    const absl::Span<const int64_t> coeffs) {
  std::vector<int> result;
  DCHECK(coeffs[0] != std::numeric_limits<int64_t>::min());
  int64_t min_abs_coeff = std::abs(coeffs[0]);
  int min_term = 0;
  int64_t global_gcd = min_abs_coeff;
  for (int i = 1; i < coeffs.size(); ++i) {
    DCHECK(coeffs[i] != std::numeric_limits<int64_t>::min());
    const int64_t abs_coeff = std::abs(coeffs[i]);
    global_gcd = std::gcd(global_gcd, abs_coeff);
    if (abs_coeff < min_abs_coeff) {
      min_abs_coeff = abs_coeff;
      min_term = i;
    }
  }
  if (min_abs_coeff == global_gcd) return result;
  int64_t current_gcd = min_abs_coeff;
  result.reserve(coeffs.size());
  result.push_back(min_term);
  while (current_gcd > global_gcd) {
    // TODO(user): The following is a heuristic to make drop the GCD as fast
    // as possible. It might be suboptimal in general (as we could miss two
    // coprime coefficients for instance).
    int64_t new_gcd = std::gcd(current_gcd, std::abs(coeffs[0]));
    int term = 0;
    for (int i = 1; i < coeffs.size(); ++i) {
      const int64_t gcd = std::gcd(current_gcd, std::abs(coeffs[i]));
      if (gcd < new_gcd) {
        term = i;
        new_gcd = gcd;
      }
    }
    result.push_back(term);
    current_gcd = new_gcd;
  }
  const int initial_count = static_cast<int>(result.size());
  for (int i = 0; i < coeffs.size(); ++i) {
    bool seen = false;
    // initial_count is very small (proven <= 15, usually much smaller).
    for (int j = 0; j < initial_count; ++j) {
      if (result[j] == i) {
        seen = true;
        break;
      }
    }
    if (seen) continue;

    result.push_back(i);
  }
  return result;
}

DiophantineSolution SolveDiophantine(absl::Span<const int64_t> coeffs,
                                     int64_t rhs,
                                     absl::Span<const int64_t> var_lbs,
                                     absl::Span<const int64_t> var_ubs) {
  const int64_t global_gcd = Gcd(coeffs);

  if (rhs % global_gcd != 0) return {.has_solutions = false};

  const std::vector<int> pivots = GreedyFastDecreasingGcd(coeffs);
  if (pivots.empty()) {
    return {.no_reformulation_needed = true, .has_solutions = true};
  }
  int64_t current_gcd = std::abs(coeffs[pivots[0]]);

  // x_i's Satisfying sum(x_i * coeffs[pivots[i]]) = current_gcd.
  std::vector<absl::int128> special_solution = {current_gcd /
                                                coeffs[pivots[0]]};
  // Z-basis of sum(x_i * arg.coeffs(pivots[i])) = 0.
  std::vector<std::vector<absl::int128>> kernel_basis;
  kernel_basis.reserve(coeffs.size() - 1);
  int i = 1;
  for (; i < pivots.size() && current_gcd > global_gcd; ++i) {
    const int64_t coeff = coeffs[pivots[i]];
    const int64_t new_gcd = std::gcd(current_gcd, std::abs(coeff));
    kernel_basis.emplace_back(i + 1);
    kernel_basis.back().back() = current_gcd / new_gcd;
    for (int i = 0; i < special_solution.size(); ++i) {
      kernel_basis.back()[i] = -special_solution[i] * coeff / new_gcd;
    }
    ReduceModuloBasis(kernel_basis, static_cast<int>(kernel_basis.size()) - 1,
                      kernel_basis.back());
    // Solves current_gcd * u + coeff * v = new_gcd. Copy the coefficients as
    // the function below modifies them.
    int64_t a = current_gcd;
    int64_t b = coeff;
    int64_t c = new_gcd;
    int64_t u, v;
    SolveDiophantineEquationOfSizeTwo(a, b, c, u, v);
    for (int i = 0; i < special_solution.size(); ++i) {
      special_solution[i] *= u;
    }
    special_solution.push_back(v);
    ReduceModuloBasis(kernel_basis, static_cast<int>(kernel_basis.size()),
                      special_solution);
    current_gcd = new_gcd;
  }
  const int replaced_variables_count = i;
  for (; i < pivots.size(); ++i) {
    const int64_t coeff = coeffs[pivots[i]];
    kernel_basis.emplace_back(replaced_variables_count);
    for (int i = 0; i < special_solution.size(); ++i) {
      kernel_basis.back()[i] = -special_solution[i] * coeff / global_gcd;
    }
    ReduceModuloBasis(kernel_basis, replaced_variables_count - 1,
                      kernel_basis.back());
  }

  for (int i = 0; i < special_solution.size(); ++i) {
    special_solution[i] *= rhs / global_gcd;
  }
  ReduceModuloBasis(kernel_basis, replaced_variables_count - 1,
                    special_solution);

  // To compute the domains, we use the triangular shape of the basis. The first
  // one is special as it is controlled by two columns of the basis. Note that
  // we don't try to compute exact domains as we would need to multiply then
  // making the number of interval explode.
  // For i = 0, ..., replaced_variable_count - 1, uses identities
  //  x[i] = special_solution[i]
  //        + sum(linear_basis[k][i]*y[k], max(1, i) <= k < vars.size)
  // where:
  //  y[k] is a newly created variable if 1 <= k < replaced_variable_count
  //  y[k] = x[pivots[k]] else.
  // TODO(user): look if there is a natural improvement.
  std::vector<absl::int128> kernel_vars_lbs(replaced_variables_count - 1);
  std::vector<absl::int128> kernel_vars_ubs(replaced_variables_count - 1);
  for (int i = replaced_variables_count - 1; i >= 0; --i) {
    absl::int128 lb = var_lbs[pivots[i]] - special_solution[i];
    absl::int128 ub = var_ubs[pivots[i]] - special_solution[i];
    // Identities 0 and 1 both bound the first element of the basis.
    const int bounds_to_update = i > 0 ? i - 1 : 0;
    for (int j = bounds_to_update + 1; j < replaced_variables_count - 1; ++j) {
      const absl::int128 coeff = kernel_basis[j][i];
      lb -= coeff * (coeff < 0 ? kernel_vars_lbs[j] : kernel_vars_ubs[j]);
      ub -= coeff * (coeff < 0 ? kernel_vars_ubs[j] : kernel_vars_lbs[j]);
    }
    for (int j = replaced_variables_count - 1; j < pivots.size() - 1; ++j) {
      const absl::int128 coeff = kernel_basis[j][i];
      const int64_t lb_var = var_lbs[pivots[j + 1]];
      const int64_t ub_var = var_ubs[pivots[j + 1]];
      lb -= coeff * (coeff < 0 ? lb_var : ub_var);
      ub -= coeff * (coeff < 0 ? ub_var : lb_var);
    }
    const absl::int128 coeff = kernel_basis[bounds_to_update][i];
    const absl::int128 deduced_lb =
        MathUtil::CeilOfRatio(coeff > 0 ? lb : ub, coeff);
    const absl::int128 deduced_ub =
        MathUtil::FloorOfRatio(coeff > 0 ? ub : lb, coeff);
    if (i > 0) {
      kernel_vars_lbs[i - 1] = deduced_lb;
      kernel_vars_ubs[i - 1] = deduced_ub;
    } else {
      kernel_vars_lbs[0] = std::max(kernel_vars_lbs[0], deduced_lb);
      kernel_vars_ubs[0] = std::min(kernel_vars_ubs[0], deduced_ub);
    }
  }
  for (int i = 0; i < replaced_variables_count - 1; ++i) {
    if (kernel_vars_lbs[i] > kernel_vars_ubs[i])
      return {.has_solutions = false};
  }
  return {.no_reformulation_needed = false,
          .has_solutions = true,
          .index_permutation = pivots,
          .special_solution = special_solution,
          .kernel_basis = kernel_basis,
          .kernel_vars_lbs = kernel_vars_lbs,
          .kernel_vars_ubs = kernel_vars_ubs};
}

}  // namespace operations_research::sat
