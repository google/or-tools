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

#include "ortools/math_opt/constraints/sos/validator.h"

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/linear_expression_validator.h"
#include "ortools/math_opt/validators/scalar_validator.h"

namespace operations_research::math_opt {

absl::Status ValidateConstraint(const SosConstraintProto& constraint,
                                const IdNameBiMap& variable_universe) {
  if (!constraint.weights().empty() &&
      constraint.weights_size() != constraint.expressions_size()) {
    return util::InvalidArgumentErrorBuilder()
           << "Length mismatch between weights and expressions: "
           << constraint.weights_size() << " vs. "
           << constraint.expressions_size();
  }
  for (const LinearExpressionProto& expression : constraint.expressions()) {
    RETURN_IF_ERROR(ValidateLinearExpression(expression, variable_universe))
        << "Invalid SOS expression";
  }
  // Check weights for uniqueness.
  absl::flat_hash_set<double> weights;
  for (const double weight : constraint.weights()) {
    RETURN_IF_ERROR(CheckScalarNoNanNoInf(weight)) << "Invalid SOS weight";
    if (!weights.insert(weight).second) {
      return util::InvalidArgumentErrorBuilder()
             << "SOS weights must be unique, but encountered duplicate weight: "
             << weight;
    }
  }
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
