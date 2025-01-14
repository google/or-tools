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

#include "ortools/math_opt/constraints/quadratic/validator.h"

#include <cstdint>

#include "absl/status/status.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/ids_validator.h"
#include "ortools/math_opt/validators/scalar_validator.h"
#include "ortools/math_opt/validators/sparse_matrix_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"

namespace operations_research::math_opt {

absl::Status ValidateConstraint(const QuadraticConstraintProto& constraint,
                                const IdNameBiMap& variable_universe) {
  // Step 1: Validate linear terms.
  RETURN_IF_ERROR(CheckIdsAndValues(
      MakeView(constraint.linear_terms()),
      {.allow_positive_infinity = false, .allow_negative_infinity = false}))
      << "bad linear term in quadratic constraint";
  RETURN_IF_ERROR(
      CheckIdsSubset(constraint.linear_terms().ids(), variable_universe))
      << "bad linear term ID in quadratic constraint";

  // Step 2: Validate quadratic terms.
  RETURN_IF_ERROR(SparseMatrixValid(constraint.quadratic_terms(),
                                    /*enforce_upper_triangular=*/true))
      << "bad quadratic term in quadratic constraint";
  RETURN_IF_ERROR(SparseMatrixIdsAreKnown(constraint.quadratic_terms(),
                                          variable_universe, variable_universe))
      << "bad quadratic term ID in quadratic constraint";

  // Step 3: Validate bounds.
  {
    const double lb = constraint.lower_bound();
    const double ub = constraint.upper_bound();
    RETURN_IF_ERROR(CheckScalar(lb, {.allow_positive_infinity = false}))
        << "bad quadratic constraint lower bound";
    RETURN_IF_ERROR(CheckScalar(ub, {.allow_negative_infinity = false}))
        << "bad quadratic constraint upper bound";
    if (lb > ub) {
      return util::InvalidArgumentErrorBuilder()
             << "Quadratic constraint bounds are inverted, rendering model "
                "trivially infeasible: lb = "
             << lb << " > " << ub << " = ub";
    }
  }

  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
