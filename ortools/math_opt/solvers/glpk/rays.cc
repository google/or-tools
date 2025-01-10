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

#include "ortools/math_opt/solvers/glpk/rays.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/glpk/glpk_computational_form.h"
#include "ortools/glpk/glpk_formatters.h"

namespace operations_research::math_opt {
namespace {

absl::StatusOr<GlpkRay> ComputePrimalRay(glp_prob* const problem,
                                         const int non_basic_variable) {
  const int num_cstrs = glp_get_num_rows(problem);

  // Check that the non_basic_variable is indeed non basic.
  const int non_basic_variable_status =
      ComputeFormVarStatus(problem,
                           /*num_cstrs=*/num_cstrs,
                           /*k=*/non_basic_variable);
  CHECK_NE(non_basic_variable_status, GLP_BS);

  // When we perform the (primal) simplex algorithm, we detect the primal
  // unboundness when we have a non-basic variable (here variable can be a
  // structural or an auxiliary variable) which contributes to increase (for
  // maximization, decrease for minimization) the objective but none of the
  // basic variables bounds are limiting its growth. GLPK returns the index of
  // this non-basic tableau variable.
  //
  // To be more precise, here we will use the conventions used in
  // glpk-5.0/doc/glpk.pdf available from glpk-5.0.tar.gz.
  //
  // From (glpk eq. 3.13) we know that the values of the basic variables are
  // dependent on the values of the non-basic ones:
  //
  //   x_B = 𝚵 x_N
  //
  // where 𝚵 is the tableau defined by (glpk eq. 3.12):
  //
  //   𝚵 = -B^-1 N
  //
  // Thus if the c-th non basic variable is changed:
  //
  //   x'_N = x_N + t e_c  , e_c ∈ R^n is the c-th standard unit vector
  //                         t   ∈ R   is the change
  //
  // Then to keep the primal feasible we must have:
  //
  //   x'_B = 𝚵 x'_N
  //        = 𝚵 x_N + t 𝚵 e_c
  //        = 𝚵 x_N + t 𝚵 e_c
  //        = x_B   + t 𝚵 e_c
  //
  // We thus have the primal ray:
  //
  //   x'_N - x_N = t e_c
  //   x'_B - x_B = t 𝚵 e_c
  //
  // From (glpk eq. 3.34) we know that the primal objective is:
  //
  //   z = d^T x_N + c_0
  //
  // I.e. reduced cost d_j shows how the non-basic variable x_j influences the
  // objective.
  //
  // Thus if the problem is a minimization we know that:
  //
  //   t > 0  , if d_c < 0
  //   t < 0  , if d_c > 0
  //
  // Since if it was not the case, the primal simplex algorithm would not have
  // picked this variable.
  //
  // The signs for a maximization are reversed:
  //
  //   t < 0  , if d_c < 0
  //   t > 0  , if d_c > 0
  const double reduced_cost = ComputeFormVarReducedCost(
      problem, /*num_cstrs=*/num_cstrs, /*k=*/non_basic_variable);
  const double t = (glp_get_obj_dir(problem) == GLP_MAX ? 1.0 : -1.0) *
                   (reduced_cost >= 0 ? 1.0 : -1.0);

  // In case of bounded variables, we can check that the result agrees with the
  // current active bound. We can't do so for free variables though.
  switch (non_basic_variable_status) {
    case GLP_NL:  // At lower-bound.
      if (t == -1.0) {
        return absl::InternalError(
            "a non-basic variable at its lower-bound is reported as cause of "
            "unboundness but the reduced cost's sign indicates that the solver "
            "considered making it smaller");
      }
      break;
    case GLP_NU:  // At upper-bound.
      if (t == 1.0) {
        return absl::InternalError(
            "a non-basic variable at its upper-bound is reported as cause of "
            "unboundness but the reduced cost's sign indicates that the solver "
            "considered making it bigger");
      }
      break;
    case GLP_NF:  // Free (unbounded).
      break;
    default:  // GLP_BS (basic), GLP_NS (fixed) or invalid value
      return absl::InternalError(absl::StrCat(
          "unexpected ", BasisStatusString(non_basic_variable_status),
          " reported as cause of unboundness"));
  }

  GlpkRay::SparseVector ray_non_zeros;

  // As seen in the maths above:
  //
  //   x'_N - x_N = t e_c
  //
  ray_non_zeros.emplace_back(non_basic_variable, t);

  // As seen in the maths above:
  //
  //   x'_B - x_B = t 𝚵 e_c
  //
  // Here 𝚵 e_c is the c-th column of the tableau. We thus use the GLPK function
  // that returns this column.
  std::vector<int> inds(num_cstrs + 1);
  std::vector<double> vals(num_cstrs + 1);
  const int non_zeros =
      glp_eval_tab_col(problem, non_basic_variable, inds.data(), vals.data());
  for (int i = 1; i <= non_zeros; ++i) {
    ray_non_zeros.emplace_back(inds[i], t * vals[i]);
  }

  return GlpkRay(GlpkRayType::kPrimal, std::move(ray_non_zeros));
}

absl::StatusOr<GlpkRay> ComputeDualRay(glp_prob* const problem,
                                       const int basic_variable) {
  const int num_cstrs = glp_get_num_rows(problem);

  // Check that the basic_variable is indeed basic.
  {
    const int status = ComputeFormVarStatus(problem,
                                            /*num_cstrs=*/num_cstrs,
                                            /*k=*/basic_variable);
    CHECK_EQ(status, GLP_BS) << BasisStatusString(status);
  }

  // The dual simplex proceeds by repeatedly finding basic variables (here
  // variable includes structural and auxiliary variables) that are primal
  // infeasible and replacing them in the basis with a non-basic variable whose
  // growth is limited by their reduced cost.
  //
  // This algorithm detects dual unboundness when we have a basic variable is
  // primal infeasible (out of its bounds) but there are no non-basic variable
  // that would limit the growth of its reduced cost, and thus the growth of the
  // dual objective.
  //
  // To be more precise, here we will use the conventions used in
  // glpk-5.0/doc/glpk.pdf available from glpk-5.0.tar.gz. The dual simplex
  // algorithm is defined by (https://d-nb.info/978580478/34): Koberstein,
  // Achim. "The dual simplex method, techniques for a fast and stable
  // implementation." Unpublished doctoral thesis, Universität Paderborn,
  // Paderborn, Germany (2005).
  //
  // In the following reasoning, we will considering the dual after the
  // permutation of the basis (glpk eq. 3.27):
  //
  //   B^T π + λ_B = c_B
  //   N^T π + λ_N = c_N
  //
  // We will now see what happens when we relax a basic variable that would
  // leave the base. See (Koberstein §3.1.2) for details.
  //
  // Let's assume we have (π, λ_B, λ_N) that is a basic dual feasible
  // solution. By definition:
  //
  //   λ_B = 0
  //
  // If we relax the equality constraint of the basic variable r that is primal
  // infeasible, that is if we relax λ_B_r and get another solution (π', λ'_B,
  // λ'_N). By definition, all other basic variables stays at equality and thus:
  //
  //  λ'_B = t e_r  , e_r ∈ R^m is the standard unit vector
  //                  t   ∈ R   is the relaxation
  //
  // From (glpk eq. 3.30) we have:
  //
  //  λ'_N = N^T B^-T λ'_B + (c_N - N^T B^-T c_B)
  //  λ'_N = t (B^-1 N)^T e_r + λ_N
  //
  // Using the (glpk eq. 3.12) definition of the tableau:
  //
  //  𝚵 = -B^-1 N
  //
  // We have:
  //
  //  λ'_N = -t 𝚵^T e_r + λ_N
  //
  // That is that the change of the reduced cost of the basic variable r has to
  // be compensated by the change of the reduced costs the non-basic variables.
  //
  // We can write the new dual objective:
  //
  //  Z' = l^T λ'_l + u^T λ'_u
  //
  // If the problem is a minimization we have:
  //
  //  Z' = sum_{j:λ'_N_j >= 0} l_N_j λ'_N_j +
  //       sum_{j:λ'_N_j <= 0} u_N_j λ'_N_j +
  //       {l_B_r, if t >= 0, u_B_r, else} t
  //
  // Here we assume the signs of λ'_N are identical to the ones of λ_N (this is
  // not an issue with dual simplex since we want to make one non-basic tight to
  // use it in the basis) we can replace λ'_N with the value computed above and
  // considering the initial solution was basic which implied that non-basic
  // where at their bound we can rewrite the objective as:
  //
  //  Z' = Z - t e_r^T 𝚵 x_N + {l_B_r, if t >= 0, u_B_r, else} t
  //
  // We have, using (glpk eq. 3.13):
  //
  //  e_r^T 𝚵 x_N = e_r^T x_B = x_B_r
  //
  // And thus, for a minimization we have:
  //
  //  Z' - Z = t * {l_B_r - x_B_r, if t >= 0,
  //                u_B_r - x_B_r, if t <= 0}
  //
  // Depending on the type of constraint, i.e. depending on whether l_B_r and/or
  // u_B_r are finite), we have constraints on the sign of `t`. But we can see
  // that since we pick the basic variable r because it was primal infeasible,
  // then it should break one of its finite bounds.
  //
  //   either x_B_r < l_B_r
  //   or     u_B_r < x_B_r
  //
  // If l_B_r is finite and x_B_r < l_B_r, then choosing:
  //
  //  t >= 0
  //
  // leads to:
  //
  //  Z' - Z >= 0
  //
  // and we see from (glpk eq. 3.17) and the "rule of signs" table (glpk page
  // 101) that we keep the solution dual feasible by doing so.
  //
  // The same logic applies if x_B_r > u_B_r:
  //
  //   t <= 0
  //
  // leads to:
  //
  //   Z' - Z >= 0
  //
  // The dual objective increase in both cases; which is what we want for a
  // minimization problem since the dual is a maximization.
  //
  // For a maximization problem the results are similar but the sign of t
  // changes (which is expected since the dual is a minimization):
  //
  //  Z' - Z = t * {l_B_r - x_B_r, if t <= 0,
  //                u_B_r - x_B_r, if t >= 0}
  //
  // If a problem is dual unbounded, this means that it is possible to grow t
  // without limit. I.e. is possible to choose any value for t without making
  // any λ'_N change sign.
  //
  // We can then express the changes of λ' from t:
  //
  //  λ'_B = t e_r
  //  λ'_N = -t 𝚵^T e_r + λ_N
  //
  // Since λ_B = 0, we can rewrite those as:
  //
  //  λ'_B - λ_B =  t e_r
  //  λ'_N - λ_N = -t 𝚵^T e_r
  //
  // That is the dual ray.
  const double primal_value = ComputeFormVarPrimalValue(
      problem, /*num_cstrs=*/num_cstrs, /*k=*/basic_variable);

  const double upper_bound = ComputeFormVarUpperBound(
      problem, /*num_cstrs=*/num_cstrs, /*k=*/basic_variable);
  const double lower_bound = ComputeFormVarLowerBound(
      problem, /*num_cstrs=*/num_cstrs, /*k=*/basic_variable);
  if (!(primal_value > upper_bound || primal_value < lower_bound)) {
    return absl::InternalError(
        "dual ray computation failed: GLPK identified a basic variable as the "
        "source of unboundness but its primal value is within its bounds");
  }

  // As we have seen in the maths above, depending on which primal bound is
  // violated and the optimization direction, we choose the sign of t.
  //
  // Here the problem is unbounded so we can pick any value for t we want.
  const double t = (glp_get_obj_dir(problem) == GLP_MAX ? 1.0 : -1.0) *
                   (primal_value > upper_bound ? 1.0 : -1.0);

  GlpkRay::SparseVector ray_non_zeros;

  // As seen in the math above:
  //
  //  λ'_B - λ_B = t e_r
  //
  ray_non_zeros.emplace_back(basic_variable, t);

  // As we have seen above, to keep the dual feasible, we must update the
  // reduced costs of the non-basic variables by the formula:
  //
  //  λ'_N - λ_N = -t 𝚵^T e_r
  //
  // Here 𝚵^T e_r is the r-th row of the tableau. We thus use the GLPK function
  // that returns this row.
  const int num_structural_vars = glp_get_num_cols(problem);
  std::vector<int> inds(num_structural_vars + 1);
  std::vector<double> vals(num_structural_vars + 1);
  const int non_zeros =
      glp_eval_tab_row(problem, basic_variable, inds.data(), vals.data());
  for (int i = 1; i <= non_zeros; ++i) {
    ray_non_zeros.emplace_back(inds[i], -t * vals[i]);
  }

  return GlpkRay(GlpkRayType::kDual, std::move(ray_non_zeros));
}

}  // namespace

GlpkRay::GlpkRay(const GlpkRayType type, SparseVector non_zero_components)
    : type(type), non_zero_components(std::move(non_zero_components)) {}

absl::StatusOr<std::optional<GlpkRay>> GlpkComputeUnboundRay(
    glp_prob* const problem) {
  const int unbound_ray = glp_get_unbnd_ray(problem);
  if (unbound_ray <= 0) {
    // No ray, do nothing.
    DCHECK_EQ(unbound_ray, 0);
    return std::nullopt;
  }

  // The factorization may not exists when GLPK's trivial_lp() is used to solve
  // a trivial LP. Here we force the computation of the factorization if
  // necessary.
  if (!glp_bf_exists(problem)) {
    const int factorization_rc = glp_factorize(problem);
    if (factorization_rc != 0) {
      return util::InternalErrorBuilder() << "glp_factorize() failed: "
                                          << ReturnCodeString(factorization_rc);
    }
  }

  // The function glp_get_unbnd_ray() returns either:
  //   - a non-basic tableau variable if we have primal unboundness.
  //   - a basic tableau variable if we have dual unboundness.
  const bool is_dual_ray =
      ComputeFormVarStatus(problem,
                           /*num_cstrs=*/glp_get_num_rows(problem),
                           /*k=*/unbound_ray) == GLP_BS;
  ASSIGN_OR_RETURN(const GlpkRay ray,
                   (is_dual_ray ? ComputeDualRay(problem, unbound_ray)
                                : ComputePrimalRay(problem, unbound_ray)));
  return ray;
}

}  // namespace operations_research::math_opt
