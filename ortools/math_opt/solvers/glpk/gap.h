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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GLPK_GAP_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GLPK_GAP_H_

namespace operations_research::math_opt {

// Returns the worst dual bound corresponding to the given objective value and
// relative gap limit. This should be used when glp_intopt() returns GLP_EMIPGAP
// (i.e. stopped because of the gap limit) but the best_dual_bound is not
// available.
//
// GLPK define the relative gap as:
//
//          |best_objective_value − best_dual_bound|
//   gap := ----------------------------------------
//            |best_objective_value| + DBL_EPSILON
//
// This function thus returns the value of best_dual_bound that makes gap match
// the relative_gap_limit.
//
// Negative or NaN relative_gap_limit is considered 0. If the relative_gap_limit
// is +inf, returns the infinite dual bound corresponding to the is_maximize.
//
// If the objective_value is infinite or NaN, returns the same value as the
// worst dual bound (in practice the objective_value should be finite).
double WorstGLPKDualBound(bool is_maximize, double objective_value,
                          double relative_gap_limit);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GLPK_GAP_H_
