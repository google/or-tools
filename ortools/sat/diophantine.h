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

#ifndef OR_TOOLS_SAT_DIOPHANTINE_H_
#define OR_TOOLS_SAT_DIOPHANTINE_H_

#include <cstdint>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/types/span.h"

namespace operations_research::sat {

// Reduces v modulo the elements_to_consider first elements of the (normal
// form) basis. The leading coeff of a basis element is the last one. In other
// terms, basis has the form:
//  * A 0 0 0 0 0
//  * * B 0 0 0 0
//  * * * C 0 0 0
//  .............
// with non-zero pivots elements A, B, C, ... and the reduction is performed in
// such a way that for a pivot P of the basis and the correspond entry x of v at
// the end of the reduction, we have
//     -floor(|P|/2) <= v < ceil(|P|/2).
void ReduceModuloBasis(const std::vector<std::vector<absl::int128>>& basis,
                       const int elements_to_consider,
                       std::vector<absl::int128>& v);

// Returns an ordering of the indices of coefficients such that the GCD of its
// initial segments decreases fast. As the product of the 15 smallest prime
// numbers is the biggest fitting in an int64_t, it is guaranteed that the GCD
// becomes stationary after at most 15 steps. Returns an empty vector if the GCD
// is equal to the absolute value of one of the coefficients.
std::vector<int> GreedyFastDecreasingGcd(absl::Span<const int64_t> coeffs);

// The comments here describe basic feature of the fields. See more details in
// the description of the function below SolveDiophantine.
struct DiophantineSolution {
  // One of the coefficients is equal to the GCD of all coefficients.
  bool no_reformulation_needed;

  // false if the equation is proven infeasible.
  bool has_solutions;

  // Order of indices of the next fields.
  // This is a permutation of [0, num_vars_of_initial_equation). It starts by
  // the chosen pivots.
  std::vector<int> index_permutation;

  // Special (reduced) solution of the constraint. Only coefficients of pivots
  // are specified. Further coefficients are 0.
  // All coefficients except the first one are guaranteed to be int64_t (see
  // ReductionModuloBasis).
  std::vector<absl::int128> special_solution;

  // Reduced basis of the kernel.
  // All coefficients except the first one are guaranteed to be int64_t (see
  // ReductionModuloBasis).
  // Size is index_order.size() - 1.
  std::vector<std::vector<absl::int128>> kernel_basis;

  // Bounds of kernel multiples.
  // Same size as kernel_basis.
  std::vector<absl::int128> kernel_vars_lbs;
  std::vector<absl::int128> kernel_vars_ubs;
};

// Gives a parametric description of the solutions of the Diophantine equation
// with n variables:
//  sum(coeffs[i] * x[i]) = rhs.
// var_lbs and var_ubs are bounds on desired values for variables x_i's.
//
// It is known that, ignoring variable bounds, the set of solutions of such an
// equation is
//  1. either empty if the gcd(coeffs[i]) does not divide rhs;
//  2. or the sum of a special solution and an element of the kernel of the
//     equation.
// In case 1, the function return .has_solution = false;
// In case 2, if one coefficient is equal to the GCD of all (in absolute value),
// returns .no_reformulation_needed = true. Otherwise, it behaves as follows:
//
// The kernel of the equation as dimension n-1.
//
// We assume we permute the variable by index_permutation, such that the first k
// k terms have a gcd equal to the gcd of all coefficient (it is possible to do
// this with k <= 15).
// Under this assumption, we can find:
//  * a special solution that is entirely supported by the k first variables;
//  * a basis {b[0], b[1], ..., b[n-2]} of the kernel such that:
//    - for i  = 0 ... k-2, b[i][j] = 0 if j > i+1;
//    - for i >= k-1, b[i][j] = 0 if j >= k except b[i][i+1] = 1.
// The function returns the k first coefficients of the special solution and the
// at most k first non-zero coefficients of each elements of the basis.
//
// In other terms, solutions have the form, for i in [0, k):
//  x[i] = special_solution[i] + sum(sum linear_basis[j][i] * y[j])
// where:
//  * y[j] is a newly created variable for 0 <= j < k - 1;
//  * y[j] = x[index_permutation[j + 1]] otherwise.
//
// The function reduces the basis and the special solution in such a way that
// the only coefficients that could get outside the range of input coefficients
// are the first coefficient of the special solution and the first coefficient
// of each element of the basis (see ReduceModuloBasis for more specific
// conditions).
//
// Moreover, the function compute bounds for the newly created variables using
// bounds of the variables passed as input. Note that:
//  * It can happen that a computed upper bound is lower than the corresponding
//    lower bound. It happens when a newly created variable can be bounded on an
//    interval containing no integer. In such a case, the function returns
//    .has_solution = false.
//  * The returned bounds describe a necessary condition for
//      x[i] in [var_lbs[i], var_ubs[i]]
//    but not a sufficient one.
DiophantineSolution SolveDiophantine(absl::Span<const int64_t> coeffs,
                                     int64_t rhs,
                                     absl::Span<const int64_t> var_lbs,
                                     absl::Span<const int64_t> var_ubs);

}  // namespace operations_research::sat

#endif  // OR_TOOLS_SAT_DIOPHANTINE_H_
