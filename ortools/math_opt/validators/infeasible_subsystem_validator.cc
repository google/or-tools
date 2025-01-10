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

#include "ortools/math_opt/validators/infeasible_subsystem_validator.h"

#include <cstdint>

#include "absl/status/status.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/validators/bounds_and_status_validator.h"
#include "ortools/math_opt/validators/ids_validator.h"

namespace operations_research::math_opt {

namespace {

absl::Status CheckMapKeys(
    const google::protobuf::Map<int64_t, ModelSubsetProto::Bounds>& bounds_map,
    const IdNameBiMap& universe) {
  for (const auto& [id, unused] : bounds_map) {
    if (!universe.HasId(id)) {
      return util::InvalidArgumentErrorBuilder() << "unrecognized id: " << id;
    }
  }
  return absl::OkStatus();
}

absl::Status CheckRepeatedIds(
    const google::protobuf::RepeatedField<int64_t>& ids,
    const IdNameBiMap& universe) {
  RETURN_IF_ERROR(CheckIdsRangeAndStrictlyIncreasing(ids));
  RETURN_IF_ERROR(CheckIdsSubset(ids, universe));
  return absl::OkStatus();
}

}  // namespace

absl::Status ValidateModelSubset(const ModelSubsetProto& model_subset,
                                 const ModelSummary& model_summary) {
  RETURN_IF_ERROR(
      CheckMapKeys(model_subset.variable_bounds(), model_summary.variables))
      << "bad ModelSubsetProto.variable_bounds";
  RETURN_IF_ERROR(CheckRepeatedIds(model_subset.variable_integrality(),
                                   model_summary.variables))
      << "bad ModelSubsetProto.variable_integrality";
  RETURN_IF_ERROR(CheckMapKeys(model_subset.linear_constraints(),
                               model_summary.linear_constraints))
      << "bad ModelSubsetProto.linear_constraints";
  RETURN_IF_ERROR(CheckMapKeys(model_subset.quadratic_constraints(),
                               model_summary.quadratic_constraints))
      << "bad ModelSubsetProto.quadratic_constraints";
  RETURN_IF_ERROR(CheckRepeatedIds(model_subset.second_order_cone_constraints(),
                                   model_summary.second_order_cone_constraints))
      << "bad ModelSubsetProto.second_order_cone_constraints";
  RETURN_IF_ERROR(CheckRepeatedIds(model_subset.sos1_constraints(),
                                   model_summary.sos1_constraints))
      << "bad ModelSubsetProto.sos1_constraints";
  RETURN_IF_ERROR(CheckRepeatedIds(model_subset.sos2_constraints(),
                                   model_summary.sos2_constraints))
      << "bad ModelSubsetProto.sos2_constraints";
  RETURN_IF_ERROR(CheckRepeatedIds(model_subset.indicator_constraints(),
                                   model_summary.indicator_constraints))
      << "bad ModelSubsetProto.indicator_constraints";
  return absl::OkStatus();
}

absl::Status ValidateComputeInfeasibleSubsystemResult(
    const ComputeInfeasibleSubsystemResultProto& result,
    const ModelSummary& model_summary) {
  RETURN_IF_ERROR(ValidateComputeInfeasibleSubsystemResultNoModel(result));
  if (result.feasibility() == FEASIBILITY_STATUS_INFEASIBLE) {
    RETURN_IF_ERROR(
        ValidateModelSubset(result.infeasible_subsystem(), model_summary));
  }
  return absl::OkStatus();
}

absl::Status ValidateComputeInfeasibleSubsystemResultNoModel(
    const ComputeInfeasibleSubsystemResultProto& result) {
  RETURN_IF_ERROR(ValidateFeasibilityStatus(result.feasibility()))
      << "bad ComputeInfeasibleSubsystemResultProto.feasibility";
  if (result.feasibility() != FEASIBILITY_STATUS_INFEASIBLE) {
    // Check that the `infeasible_subsystem` is empty by validating against an
    // empty ModelSummary.
    if (!ValidateModelSubset(result.infeasible_subsystem(), ModelSummary())
             .ok()) {
      return util::InvalidArgumentErrorBuilder()
             << "nonempty infeasible_subsystem with feasibility status: "
             << FeasibilityStatusProto_Name(result.feasibility());
    }
    if (result.is_minimal()) {
      return util::InvalidArgumentErrorBuilder()
             << "is_minimal is true with feasibility status: "
             << FeasibilityStatusProto_Name(result.feasibility());
    }
  }
  return absl::OkStatus();
}

}  // namespace operations_research::math_opt
