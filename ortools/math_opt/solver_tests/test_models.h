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

// Holds MathOpt models that are shared across tests from this directory.
#ifndef ORTOOLS_MATH_OPT_SOLVER_TESTS_TEST_MODELS_H_
#define ORTOOLS_MATH_OPT_SOLVER_TESTS_TEST_MODELS_H_

#include <memory>

#include "ortools/math_opt/cpp/math_opt.h"

namespace operations_research::math_opt {

// Decision variables:
//   * x[i], i=1..3
//   * y[i], i=1..3
//
// Problem statement:
//   max  sum_i 3 * x[i] + 2 * y[i]
//   s.t. x[i] + y[i] <= 1.5                   for i=1..3
//        0 <= x[i], y[i] <= 1                 for i=1..3
//        Optionally, x[i], y[i] integer,      for i=1..3
//
// Analysis:
//   * For IP, x[i] = 1, y[i] = 0 for all i is optimal, objective 9.
//   * For LP, x[i] = 1, y[i] = 0.5 for all i is optimal, objective is 12.
std::unique_ptr<Model> SmallModel(bool integer);

// Problem data: m = 3, n > 0, c = [5, 4, 3]
//
// Decision variables: x[i][j], i = 1..m, j = 1..n
//
// Problem statement:
//   max   sum_i sum_j c[i] * x[i,j]
//   s.t.  x[i, j] + x[i, k] <= 1         for i = 1..m, j = 1..n, k = j+1..n (A)
//         x[0, j] + x[i, k] <= 1         for i = 2..m, j = 1..n, k = 1..n   (B)
//         0 <= x[i, j] <= 1
//         Optionally, x[i, j] integer.
//
// Analysis:
//   * Constraint (A) says that for each row i, pick at most one j to be on.
//   * Constraint (B) says that if you pick any from row i = 0, you cannot use
//     rows i = 1, 2.
//   * Optimal objective is 7, e.g. x[1][0] = x[2][0] = 1, all other x zero.
//   * Heuristics are likely to pick elements with x[0][j] = 1 to get the larger
//     objective coefficient, global reasoning (beyond one linear constraint) is
//     needed to see this doesn't work well.
//   * LP optimal objective is 10 * (5 + 4 + 3) / 2, taking all x[i, j] = 1/2,
//     so the problem has a large initial gap.
//   * For LP, variable is at a bound, so likely some pivots will be required.
//   * The MIP has many symmetric solutions.
std::unique_ptr<Model> DenseIndependentSet(bool integer, int n = 10);

// A hint with objective value of 5 for the model returned by
// DenseIndependentSet().
ModelSolveParameters::SolutionHint DenseIndependentSetHint5(const Model& model);

// Problem data: n > 0
//
// Decision variables: x[i], i = 0..n-1
//
// Problem statement:
//   max  sum_i x[i]
//   s.t. x[i] + x[j] <= 1                   for i = 0..n-1, j = i+1..n-1
//          0 <= x[i] <= 1                   for i = 0..n-1
//
// Analysis:
//   * The unique optimal solution to this problem is x[i] = 1/2 for all i, with
//     an objective value of n/2.
//   * Setting an iteration of limit significantly smaller than n should prevent
//     an LP solver from finding an optimal solution. Specific state at such
//     termination is solver-dependent.
std::unique_ptr<Model> IndependentSetCompleteGraph(bool integer, int n = 10);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_SOLVER_TESTS_TEST_MODELS_H_
