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

// This headers defines APIs that wrap GLPK APIs for indices of variables from
// the computational form.
//
// In GLPK (for details see glpk-5.0/doc/glpk.pdf available from
// glpk-5.0.tar.gz) the general form of the problem is:
//
//   min (or max) z = c^T x_S + c_0
//           s.t. x_R = A x_S
//                l_R <= x_R <= u_R
//                l_S <= x_S <= u_S
//
// where:
//   x_S are the structural variables
//   x_R are the auxiliary variables used to define constraints
//
// This is this form that is used by most GLPK's APIs.
//
// But to implement the simplex algorithms, GLPK uses the following
// computational form:
//
//   min (or max) z = (0 | c)^T x + c_0
//           s.t. (I | -A) x = 0
//                l <= x <= u
//
// where:
//   x = (x_R | x_S)
//
// That is it merges the auxiliary and structural variables in a single set of
// variables.
//
// The dual of this problem is, when the primal is a minimization:
//
//   max  Z = l^T λ_l + u^T λ_u
//   s.t. (I | -A)^T π + λ_l + λ_u = (O | c)^T
//        λ_l >= 0, λ_u <= 0
//
// and if the primal is a maximization:
//
//   min  Z = l^T λ_l + u^T λ_u
//   s.t. (I | -A)^T π + λ_l + λ_u = (O | c)^T
//        λ_l <= 0, λ_u >= 0
//
// There is a reduced cost λ_k for each variable x_k:
//
//   λ = λ_l + λ_u
//
// This header contains basic adapter functions that takes the index of a
// variable x in the computational form and use the corresponding API for either
// x_R or x_S (a.k.a. primal values) and for the corresponding reduced costs λ
// (a.k.a. dual values).
//
// This logic is usually necessary when using advanced APIs that deal with
// indices in the computational form.
#ifndef OR_TOOLS_GLPK_GLPK_COMPUTATIONAL_FORM_H_
#define OR_TOOLS_GLPK_GLPK_COMPUTATIONAL_FORM_H_

extern "C" {
#include <glpk.h>
}

namespace operations_research {

// Returns the status of the variable k of the computational form by calling
// either glp_get_row_stat() or glp_get_col_stat().
//
// Here k is an index in the joint set of indices of variables and constraints
// in the computational form (see the comment at the top of this header for
// details):
//
//   - 1 <= k <= num_cstrs: index of the k-th auxiliary variable in the general
//     form (the variable associate with the k-th constraint).
//
//   - num_cstrs + 1 <= k <= num_cstrs + num_vars: index of the (k-num_cstrs)-th
//     structural variable in the general form.
inline int ComputeFormVarStatus(glp_prob* const problem, const int num_cstrs,
                                const int k) {
  return k <= num_cstrs ? glp_get_row_stat(problem, k)
                        : glp_get_col_stat(problem, k - num_cstrs);
}

// Returns the reduced cost of the variable k of the computational form by
// calling either glp_get_row_dual() or glp_get_col_dual().
//
// See ComputeFormVarStatus() for details about k.
inline double ComputeFormVarReducedCost(glp_prob* const problem,
                                        const int num_cstrs, const int k) {
  return k <= num_cstrs ? glp_get_row_dual(problem, k)
                        : glp_get_col_dual(problem, k - num_cstrs);
}

// Returns the primal value of the variable k of the computational form by
// calling either glp_get_row_prim() or glp_get_col_prim().
//
// See ComputeFormVarStatus() for details about k.
inline double ComputeFormVarPrimalValue(glp_prob* const problem,
                                        const int num_cstrs, const int k) {
  return k <= num_cstrs ? glp_get_row_prim(problem, k)
                        : glp_get_col_prim(problem, k - num_cstrs);
}

// Returns the lower bound of the variable k of the computational form by
// calling either glp_get_row_lb() or glp_get_col_lb().
//
// See ComputeFormVarStatus() for details about k.
inline double ComputeFormVarLowerBound(glp_prob* const problem,
                                       const int num_cstrs, const int k) {
  return k <= num_cstrs ? glp_get_row_lb(problem, k)
                        : glp_get_col_lb(problem, k - num_cstrs);
}

// Returns the upper bound of the variable k of the computational form by
// calling either glp_get_row_ub() or glp_get_col_ub().
//
// See ComputeFormVarStatus() for details about k.
inline double ComputeFormVarUpperBound(glp_prob* const problem,
                                       const int num_cstrs, const int k) {
  return k <= num_cstrs ? glp_get_row_ub(problem, k)
                        : glp_get_col_ub(problem, k - num_cstrs);
}

}  // namespace operations_research

#endif  // OR_TOOLS_GLPK_GLPK_COMPUTATIONAL_FORM_H_
