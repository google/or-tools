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

// This header defines primal/dual unboundness ray computation functions for
// GLPK solver. They use the index space of the computation form of the model as
// defined in operations_research/glpk/glpk_computational_form.h.
#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GLPK_RAYS_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GLPK_RAYS_H_

#include <optional>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"

extern "C" {
#include <glpk.h>
}

namespace operations_research::math_opt {

// The type of the GlpkRay.
enum GlpkRayType {
  // A primal ray.
  //
  // If x (vector of variables) is a primal feasible solution to a primal
  // unbounded problem and r is the ray, then x' = x + t r is also a primal
  // feasible solution for all t >= 0.
  kPrimal,

  // A dual ray.
  //
  // If λ (vector of reduced costs) is a dual feasible solution to a dual
  // unbounded problem and r is the ray, then λ' = λ + t r is also a dual
  // feasible solution for all t >= 0.
  kDual,
};

// A primal or dual unbound ray for the model in computational form.
//
// See the top comment of operations_research/glpk/glpk_computational_form.h to
// understand that the computational form is. This structure uses the word
// "variable" to mean a variable in the joint set of structural and auxiliary
// variables.
struct GlpkRay {
  using SparseVector = std::vector<std::pair<int, double>>;

  GlpkRay(GlpkRayType type, SparseVector non_zero_components);

  // The type of ray, primal or dual.
  GlpkRayType type;

  // The non zero components of the vector, in no particular order.
  //
  // The first member of the pair is the index of the variable (or of its
  // corresponding reduced cost) and the second is the component's value.
  //
  // A given index can only appear once.
  //
  // The indices in GLPK are one-based. Here the indices are defined by:
  // - if 1 <= k <= m: k is the index of the k-th auxiliary variable
  //   (a.k.a. row, a.k.a. constraint)
  // - if m + 1 <= k <= m + n: k is the index of the (k-m)-th structural
  //   variable (a.k.a. column)
  // Note that the value k = 0 is not used.
  SparseVector non_zero_components;
};

// Returns the primal or dual ray if one is identified by
// glp_get_unbnd_ray(). Returns an error status if an internal error occurs.
absl::StatusOr<std::optional<GlpkRay>> GlpkComputeUnboundRay(glp_prob* problem);

}  // namespace operations_research::math_opt

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GLPK_RAYS_H_
