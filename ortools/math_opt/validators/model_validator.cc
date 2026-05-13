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

#include "ortools/math_opt/validators/model_validator.h"

#include <cstdint>
#include <limits>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/constraints/indicator/validator.h"
#include "ortools/math_opt/constraints/quadratic/validator.h"
#include "ortools/math_opt/constraints/second_order_cone/validator.h"
#include "ortools/math_opt/constraints/sos/validator.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/ids_validator.h"
#include "ortools/math_opt/validators/scalar_validator.h"
#include "ortools/math_opt/validators/sparse_matrix_validator.h"
#include "ortools/math_opt/validators/sparse_vector_validator.h"

namespace operations_research {
namespace math_opt {
namespace {

////////////////////////////////////////////////////////////////////////////////
// Submessages
////////////////////////////////////////////////////////////////////////////////

absl::Status VariablesValid(const VariablesProto& variables) {
  RETURN_IF_ERROR(CheckIdsRangeAndStrictlyIncreasing(variables.ids()))
      << "Bad variable ids";
  RETURN_IF_ERROR(
      CheckValues(MakeView(variables.ids(), variables.lower_bounds()),
                  {.allow_positive_infinity = false}, "lower_bounds"));
  RETURN_IF_ERROR(
      CheckValues(MakeView(variables.ids(), variables.upper_bounds()),
                  {.allow_negative_infinity = false}, "upper_bounds"));
  RETURN_IF_ERROR(
      CheckValues(MakeView(variables.ids(), variables.integers()), "integers"));
  return absl::OkStatus();
}

absl::Status VariableUpdatesValid(const VariableUpdatesProto& variable_updates,
                                  const IdNameBiMap& variable_ids,
                                  const int64_t old_var_id_ub) {
  RETURN_IF_ERROR(CheckIdsAndValues(MakeView(variable_updates.lower_bounds()),
                                    {.allow_positive_infinity = false}))
      << "Bad lower bounds";
  RETURN_IF_ERROR(CheckIdsAndValues(MakeView(variable_updates.upper_bounds()),
                                    {.allow_negative_infinity = false}))
      << "Bad upper bounds";
  RETURN_IF_ERROR(CheckIdsAndValues(MakeView(variable_updates.integers())))
      << "Bad integers";
  RETURN_IF_ERROR(CheckIdsSubset(variable_updates.lower_bounds().ids(),
                                 variable_ids, old_var_id_ub))
      << "lower bound update on invalid variable id";
  RETURN_IF_ERROR(CheckIdsSubset(variable_updates.upper_bounds().ids(),
                                 variable_ids, old_var_id_ub))
      << "upper bound update on invalid variable id";
  RETURN_IF_ERROR(CheckIdsSubset(variable_updates.integers().ids(),
                                 variable_ids, old_var_id_ub))
      << "integer update on invalid variable id";
  return absl::OkStatus();
}

absl::Status ObjectiveValid(const ObjectiveProto& objective,
                            const IdNameBiMap& variable_ids) {
  // 1. Validate offset
  RETURN_IF_ERROR(CheckScalarNoNanNoInf(objective.offset()))
      << "Objective offset invalid";
  // 2. Validate linear terms
  const auto linear_coefficients = MakeView(objective.linear_coefficients());
  RETURN_IF_ERROR(CheckIdsAndValues(
      linear_coefficients,
      {.allow_positive_infinity = false, .allow_negative_infinity = false}))
      << "Linear objective coefficients bad";
  RETURN_IF_ERROR(CheckIdsSubset(linear_coefficients.ids(), variable_ids))
      << "Objective.linear_coefficients.ids not found in Variables.ids";
  // 3. Validate quadratic terms
  RETURN_IF_ERROR(SparseMatrixValid(objective.quadratic_coefficients(),
                                    /*enforce_upper_triangular=*/true))
      << "Objective.quadratic_coefficients invalid";
  RETURN_IF_ERROR(SparseMatrixIdsAreKnown(objective.quadratic_coefficients(),
                                          variable_ids, variable_ids))
      << "Objective.quadratic_coefficients invalid";
  if (const int64_t priority = objective.priority(); priority < 0) {
    return util::InvalidArgumentErrorBuilder()
           << "expected Objective.priority to be nonnegative but found "
              "priority: "
           << priority;
  }
  return absl::OkStatus();
}

// NOTE: This method does not check requirements on the IDs
absl::Status ObjectiveUpdatesValid(
    const ObjectiveUpdatesProto& objective_updates,
    const IdNameBiMap& variable_ids) {
  // 1. Validate offset
  RETURN_IF_ERROR(CheckScalarNoNanNoInf(objective_updates.offset_update()))
      << "Offset update invalid";
  // 2. Validate linear terms
  RETURN_IF_ERROR(CheckIdsAndValues(
      MakeView(objective_updates.linear_coefficients()),
      {.allow_positive_infinity = false, .allow_negative_infinity = false}))
      << "Linear objective coefficients invalid";
  // 3. Validate quadratic terms
  RETURN_IF_ERROR(SparseMatrixValid(objective_updates.quadratic_coefficients(),
                                    /*enforce_upper_triangular=*/true))
      << "Objective.quadratic_coefficients invalid";
  RETURN_IF_ERROR(CheckIdsSubset(objective_updates.linear_coefficients().ids(),
                                 variable_ids))
      << "Linear coefficients ids not found in variable ids";
  RETURN_IF_ERROR(SparseMatrixIdsAreKnown(
      objective_updates.quadratic_coefficients(), variable_ids, variable_ids))
      << "quadratic_coefficients invalid";
  if (objective_updates.has_priority_update()) {
    const int64_t priority = objective_updates.priority_update();
    if (priority < 0) {
      return util::InvalidArgumentErrorBuilder()
             << "expected Objective.priority to be nonnegative but found "
                "priority: "
             << priority;
    }
  }
  return absl::OkStatus();
}

absl::Status AuxiliaryObjectivesUpdatesValid(
    const AuxiliaryObjectivesUpdatesProto& objectives,
    const IdNameBiMap& variable_ids, const IdNameBiMap& objective_ids) {
  for (const auto& [id, new_objective] : objectives.new_objectives()) {
    RETURN_IF_ERROR(ObjectiveValid(new_objective, variable_ids))
        << "bad new auxiliary objective with id: " << id;
  }
  for (const auto& [id, objective_update] : objectives.objective_updates()) {
    if (!objective_ids.HasId(id)) {
      return util::InvalidArgumentErrorBuilder()
             << "objective update on auxiliary objective not present in model "
                "with id: "
             << id;
    }
    RETURN_IF_ERROR(ObjectiveUpdatesValid(objective_update, variable_ids));
  }
  return absl::OkStatus();
}

absl::Status LinearConstraintsValid(
    const LinearConstraintsProto& linear_constraints) {
  RETURN_IF_ERROR(CheckIdsRangeAndStrictlyIncreasing(linear_constraints.ids()))
      << "Bad linear constraint ids";
  RETURN_IF_ERROR(CheckValues(
      MakeView(linear_constraints.ids(), linear_constraints.lower_bounds()),
      {.allow_positive_infinity = false}, "lower_bounds"));
  RETURN_IF_ERROR(CheckValues(
      MakeView(linear_constraints.ids(), linear_constraints.upper_bounds()),
      {.allow_negative_infinity = false}, "upper_bounds"));
  return absl::OkStatus();
}

absl::Status LinearConstraintUpdatesValid(
    const LinearConstraintUpdatesProto& linear_constraint_updates,
    const IdNameBiMap& linear_constraint_ids, const int64_t old_lin_con_id_ub) {
  RETURN_IF_ERROR(
      CheckIdsAndValues(MakeView(linear_constraint_updates.lower_bounds()),
                        {.allow_positive_infinity = false}))
      << "Bad lower bounds";
  RETURN_IF_ERROR(
      CheckIdsAndValues(MakeView(linear_constraint_updates.upper_bounds()),
                        {.allow_negative_infinity = false}))
      << "Bad upper bounds";
  RETURN_IF_ERROR(CheckIdsSubset(linear_constraint_updates.lower_bounds().ids(),
                                 linear_constraint_ids, old_lin_con_id_ub))
      << "lower bound update on invalid linear constraint id";
  RETURN_IF_ERROR(CheckIdsSubset(linear_constraint_updates.upper_bounds().ids(),
                                 linear_constraint_ids, old_lin_con_id_ub))
      << "upper bound update on invalid linear constraint id";
  return absl::OkStatus();
}

absl::Status LinearConstraintMatrixIdsValidForUpdate(
    const SparseDoubleMatrixProto& matrix,
    const IdNameBiMap& linear_constraint_ids, const IdNameBiMap& variable_ids) {
  RETURN_IF_ERROR(CheckIdsSubset(matrix.row_ids(), linear_constraint_ids))
      << "Unknown linear_constraint_id";
  RETURN_IF_ERROR(CheckIdsSubset(matrix.column_ids(), variable_ids))
      << "Unknown variable_id";
  return absl::OkStatus();
}

// To use this helper, you must implement an overload for:
//   ValidateConstraint(const MyConstraintProto& constraint,
//                      const IdNameBiMap& variable_universe);
template <typename ConstraintType>
absl::Status ValidateConstraintMap(
    const google::protobuf::Map<int64_t, ConstraintType>& constraints,
    const IdNameBiMap& variable_universe) {
  for (const auto& [id, constraint] : constraints) {
    RETURN_IF_ERROR(ValidateConstraint(constraint, variable_universe))
        << "invalid constraint with id: " << id;
  }
  return absl::OkStatus();
}

}  // namespace

// /////////////////////////////////////////////////////////////////////////////
// Model
// /////////////////////////////////////////////////////////////////////////////

absl::StatusOr<ModelSummary> ValidateModel(const ModelProto& model,
                                           const bool check_names) {
  ASSIGN_OR_RETURN(const auto model_summary,
                   ModelSummary::Create(model, check_names));
  RETURN_IF_ERROR(VariablesValid(model.variables()))
      << "ModelProto.variables are invalid.";
  RETURN_IF_ERROR(ObjectiveValid(model.objective(), model_summary.variables))
      << "ModelProto.objective is invalid";
  for (const auto& [id, objective] : model.auxiliary_objectives()) {
    RETURN_IF_ERROR(ObjectiveValid(objective, model_summary.variables))
        << "ModelProto.auxiliary_objectives is invalid with objective id: "
        << id;
  }
  RETURN_IF_ERROR(LinearConstraintsValid(model.linear_constraints()))
      << "ModelProto.linear_constraints are invalid";
  RETURN_IF_ERROR(SparseMatrixValid(model.linear_constraint_matrix()))
      << "ModelProto.linear_constraint_matrix invalid";
  RETURN_IF_ERROR(SparseMatrixIdsAreKnown(model.linear_constraint_matrix(),
                                          model_summary.linear_constraints,
                                          model_summary.variables))
      << "ModelProto.linear_constraint_matrix ids are inconsistent";

  RETURN_IF_ERROR(ValidateConstraintMap(model.quadratic_constraints(),
                                        model_summary.variables))
      << "ModelProto.quadratic_constraints invalid";
  RETURN_IF_ERROR(ValidateConstraintMap(model.second_order_cone_constraints(),
                                        model_summary.variables))
      << "ModelProto.second_order_cone_constraints invalid";
  RETURN_IF_ERROR(
      ValidateConstraintMap(model.sos1_constraints(), model_summary.variables))
      << "ModelProto.sos1_constraints invalid";
  RETURN_IF_ERROR(
      ValidateConstraintMap(model.sos2_constraints(), model_summary.variables))
      << "ModelProto.sos2_constraints invalid";
  RETURN_IF_ERROR(ValidateConstraintMap(model.indicator_constraints(),
                                        model_summary.variables))
      << "ModelProto.indicator_constraints invalid";

  return model_summary;
}

////////////////////////////////////////////////////////////////////////////////
// Model Update
////////////////////////////////////////////////////////////////////////////////

absl::Status ValidateModelUpdate(const ModelUpdateProto& model_update,
                                 ModelSummary& model_summary) {
  RETURN_IF_ERROR(model_summary.Update(model_update));
  const int64_t old_var_id_ub = model_update.new_variables().ids_size() > 0
                                    ? model_update.new_variables().ids(0)
                                    : model_summary.variables.next_free_id();
  const int64_t old_lin_con_id_ub =
      model_update.new_linear_constraints().ids_size() > 0
          ? model_update.new_linear_constraints().ids(0)
          : model_summary.linear_constraints.next_free_id();
  RETURN_IF_ERROR(VariableUpdatesValid(model_update.variable_updates(),
                                       model_summary.variables, old_var_id_ub))
      << "ModelUpdateProto.variable_updates invalid";
  RETURN_IF_ERROR(ObjectiveUpdatesValid(model_update.objective_updates(),
                                        model_summary.variables))
      << "ModelUpdateProto.objective_update invalid";
  RETURN_IF_ERROR(AuxiliaryObjectivesUpdatesValid(
      model_update.auxiliary_objectives_updates(), model_summary.variables,
      model_summary.auxiliary_objectives))
      << "ModelUpdateProto.auxiliary_objectives_updates invalid";
  RETURN_IF_ERROR(LinearConstraintUpdatesValid(
      model_update.linear_constraint_updates(),
      model_summary.linear_constraints, old_lin_con_id_ub))
      << "ModelUpdateProto.linear_constraint_updates invalid";
  RETURN_IF_ERROR(VariablesValid(model_update.new_variables()))
      << "ModelUpdateProto.new_variables invalid";
  RETURN_IF_ERROR(LinearConstraintsValid(model_update.new_linear_constraints()))
      << "ModelUpdateProto.new_linear_constraints invalid";
  RETURN_IF_ERROR(
      SparseMatrixValid(model_update.linear_constraint_matrix_updates()))
      << "ModelUpdateProto.linear_constraint_matrix_updates invalid";

  RETURN_IF_ERROR(LinearConstraintMatrixIdsValidForUpdate(
      model_update.linear_constraint_matrix_updates(),
      model_summary.linear_constraints, model_summary.variables))
      << "invalid linear constraint matrix update";

  RETURN_IF_ERROR(ValidateConstraintMap(
      model_update.quadratic_constraint_updates().new_constraints(),
      model_summary.variables))
      << "ModelUpdateProto.quadratic_constraint_updates.new_constraints "
         "invalid";
  RETURN_IF_ERROR(ValidateConstraintMap(
      model_update.second_order_cone_constraint_updates().new_constraints(),
      model_summary.variables))
      << "ModelUpdateProto.second_order_cone_constraint_updates.new_"
         "constraints invalid";
  RETURN_IF_ERROR(ValidateConstraintMap(
      model_update.sos1_constraint_updates().new_constraints(),
      model_summary.variables))
      << "ModelUpdateProto.sos1_constraint_updates.new_constraints invalid";
  RETURN_IF_ERROR(ValidateConstraintMap(
      model_update.sos2_constraint_updates().new_constraints(),
      model_summary.variables))
      << "ModelUpdateProto.sos2_constraint_updates.new_constraints invalid";
  RETURN_IF_ERROR(ValidateConstraintMap(
      model_update.indicator_constraint_updates().new_constraints(),
      model_summary.variables))
      << "ModelUpdateProto.indicator_constraint_updates.new_constraints "
         "invalid";

  return absl::OkStatus();
}

}  // namespace math_opt
}  // namespace operations_research
