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

#include "ortools/math_opt/validators/model_parameters_validator.h"

#include "absl/status/status.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::absl::StatusCode;
using ::testing::HasSubstr;
using ::testing::status::StatusIs;

TEST(ValidateModelSolveParametersTest, EmptyParameters) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{3, "c"}};

  const ModelSolveParametersProto empty;
  EXPECT_OK(ValidateModelSolveParameters(empty, summary));
}

TEST(ValidateModelSolveParametersTest, ValidIds) {
  ModelSummary summary;
  summary.variables = {{2, "x"}, {3, "y"}};
  summary.linear_constraints = {{0, "c"}, {1, "d"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_variable_values_filter()->set_filter_by_ids(true);
  parameters.mutable_variable_values_filter()->add_filtered_ids(3);
  parameters.mutable_reduced_costs_filter()->set_filter_by_ids(true);
  parameters.mutable_reduced_costs_filter()->add_filtered_ids(2);
  parameters.mutable_dual_values_filter()->set_filter_by_ids(true);
  parameters.mutable_dual_values_filter()->add_filtered_ids(1);

  EXPECT_OK(ValidateModelSolveParameters(parameters, summary));
}

TEST(ValidateModelSolveParametersTest, InvalidIdInPrimalVariablesFilter) {
  ModelSummary summary;
  summary.variables = {{2, "x"}, {3, "y"}};
  summary.linear_constraints = {{0, "c"}, {1, "d"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_variable_values_filter()->add_filtered_ids(1);

  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("variable_values_filter")));
}

TEST(ValidateModelSolveParametersTest, InvalidIdInDualVariablesFilter) {
  ModelSummary summary;
  summary.variables = {{2, "x"}, {3, "y"}};
  summary.linear_constraints = {{0, "c"}, {1, "d"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_reduced_costs_filter()->add_filtered_ids(4);

  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("reduced_costs_filter")));
}

TEST(ValidateModelSolveParametersTest, InvalidIdInDualLinearConstraintsFilter) {
  ModelSummary summary;
  summary.variables = {{2, "x"}, {3, "y"}};
  summary.linear_constraints = {{0, "c"}, {1, "d"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_dual_values_filter()->add_filtered_ids(2);

  EXPECT_THAT(
      ValidateModelSolveParameters(parameters, summary),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("dual_values_filter")));
}

TEST(ValidateModelSolveParametersTest, BasisOk) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_initial_basis()->mutable_variable_status()->add_ids(2);
  parameters.mutable_initial_basis()->mutable_variable_status()->add_values(
      BASIS_STATUS_AT_LOWER_BOUND);
  parameters.mutable_initial_basis()->mutable_constraint_status()->add_ids(0);
  parameters.mutable_initial_basis()->mutable_constraint_status()->add_values(
      BASIS_STATUS_BASIC);

  EXPECT_OK(ValidateModelSolveParameters(parameters, summary));
}

TEST(ValidateModelSolveParametersTest, InvalidBasisStatus) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_initial_basis()->mutable_variable_status()->add_ids(2);
  parameters.mutable_initial_basis()->mutable_variable_status()->add_values(
      BASIS_STATUS_UNSPECIFIED);
  parameters.mutable_initial_basis()->mutable_constraint_status()->add_ids(0);
  parameters.mutable_initial_basis()->mutable_constraint_status()->add_values(
      BASIS_STATUS_BASIC);

  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("found BASIS_STATUS_UNSPECIFIED"),
                             HasSubstr("BasisProto.variable_status invalid"))));
}

TEST(ValidateModelSolveParametersTest, SolutionHintsOk) {
  ModelSummary summary;
  summary.variables = {{2, "x1"}, {3, "x2"}};
  summary.linear_constraints = {{0, "c"}, {5, "d"}};

  ModelSolveParametersProto parameters;
  {
    SolutionHintProto& partial_hint = *parameters.add_solution_hints();
    partial_hint.mutable_variable_values()->add_ids(2);
    partial_hint.mutable_variable_values()->add_values(1.0);
    partial_hint.mutable_dual_values()->add_ids(5);
    partial_hint.mutable_dual_values()->add_values(3.0);
  }
  {
    SolutionHintProto& full_hint = *parameters.add_solution_hints();
    full_hint.mutable_variable_values()->add_ids(2);
    full_hint.mutable_variable_values()->add_values(1.0);
    full_hint.mutable_variable_values()->add_ids(3);
    full_hint.mutable_variable_values()->add_values(2.0);
    full_hint.mutable_dual_values()->add_ids(0);
    full_hint.mutable_dual_values()->add_values(4.0);
    full_hint.mutable_dual_values()->add_ids(5);
    full_hint.mutable_dual_values()->add_values(3.0);
  }
  EXPECT_OK(ValidateModelSolveParameters(parameters, summary));
}

TEST(ValidateModelSolveParametersTest, SolutionHintInvalidVariableValueId) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  SolutionHintProto& hint = *parameters.add_solution_hints();

  hint.mutable_variable_values()->add_ids(-1);
  hint.mutable_variable_values()->add_values(1.0);

  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Expected ids to be nonnegative"),
                             HasSubstr("Invalid solution_hint"),
                             HasSubstr("variable_values"))));
}

TEST(ValidateModelSolveParametersTest, SolutionHintVariableValueIdNotInModel) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  SolutionHintProto& hint = *parameters.add_solution_hints();
  hint.mutable_variable_values()->add_ids(3);
  hint.mutable_variable_values()->add_values(1.0);
  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("solution_hint.variable_values ids"),
                             HasSubstr("model variable ids"))));
}

TEST(ValidateModelSolveParametersTest, SolutionHintInvalidDualValueId) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  SolutionHintProto& hint = *parameters.add_solution_hints();

  hint.mutable_dual_values()->add_ids(-1);
  hint.mutable_dual_values()->add_values(1.0);

  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Expected ids to be nonnegative"),
                             HasSubstr("Invalid solution_hint"),
                             HasSubstr("dual_values"))));
}

TEST(ValidateModelSolveParametersTest, SolutionHintDualValueIdNotInModel) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  SolutionHintProto& hint = *parameters.add_solution_hints();
  hint.mutable_dual_values()->add_ids(3);
  hint.mutable_dual_values()->add_values(1.0);
  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("solution_hint.dual_values ids"),
                             HasSubstr("model linear constraint ids"))));
}

TEST(ValidateModelSolveParametersTest, BranchingPrioritiesOk) {
  ModelSummary summary;
  summary.variables = {{2, "x1"}, {3, "x2"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_branching_priorities()->add_ids(2);
  parameters.mutable_branching_priorities()->add_values(3);
  parameters.mutable_branching_priorities()->add_ids(3);
  parameters.mutable_branching_priorities()->add_values(2);
  EXPECT_OK(ValidateModelSolveParameters(parameters, summary));
}

TEST(ValidateModelSolveParametersTest, BranchingPrioritiesInvalidId) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_branching_priorities()->add_ids(-1);
  parameters.mutable_branching_priorities()->add_values(1);

  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Expected ids to be nonnegative"),
                             HasSubstr("Invalid branching_priorities"))));
}

TEST(ValidateModelSolveParametersTest, BranchingPrioritiesIdNotInModel) {
  ModelSummary summary;
  summary.variables = {{2, "x"}};
  summary.linear_constraints = {{0, "c"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_branching_priorities()->add_ids(3);
  parameters.mutable_branching_priorities()->add_values(1);
  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("branching_priorities ids"),
                             HasSubstr("missing from model IDs"))));
}

TEST(ValidateModelSolveParametersTest, ObjectiveParametersOk) {
  ModelSummary summary;
  summary.auxiliary_objectives = {{2, "o2"}, {3, "o3"}};

  ModelSolveParametersProto parameters;
  parameters.mutable_primary_objective_parameters()
      ->set_objective_degradation_absolute_tolerance(1.0);
  parameters.mutable_primary_objective_parameters()
      ->set_objective_degradation_relative_tolerance(2.0);
  (*parameters.mutable_auxiliary_objective_parameters())[2]
      .set_objective_degradation_absolute_tolerance(3.0);
  (*parameters.mutable_auxiliary_objective_parameters())[3]
      .set_objective_degradation_relative_tolerance(4.0);
  EXPECT_OK(ValidateModelSolveParameters(parameters, summary));
}

TEST(ValidateModelSolveParametersTest,
     NegativePrimaryObjectiveDegradationAbsoluteTolerance) {
  ModelSummary summary;
  ModelSolveParametersProto parameters;
  parameters.mutable_primary_objective_parameters()
      ->set_objective_degradation_absolute_tolerance(-1.0);
  EXPECT_THAT(
      ValidateModelSolveParameters(parameters, summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("primary"), HasSubstr("< 0"),
                     HasSubstr("objective_degradation_absolute_tolerance"))));
}

TEST(ValidateModelSolveParametersTest,
     NegativePrimaryObjectiveDegradationRelativeTolerance) {
  ModelSummary summary;
  ModelSolveParametersProto parameters;
  parameters.mutable_primary_objective_parameters()
      ->set_objective_degradation_relative_tolerance(-1.0);
  EXPECT_THAT(
      ValidateModelSolveParameters(parameters, summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("primary"), HasSubstr("< 0"),
                     HasSubstr("objective_degradation_relative_tolerance"))));
}

TEST(ValidateModelSolveParametersTest,
     NegativeAuxiliaryObjectiveDegradationAbsoluteTolerance) {
  ModelSummary summary;
  summary.auxiliary_objectives = {{2, "o2"}};
  ModelSolveParametersProto parameters;
  (*parameters.mutable_auxiliary_objective_parameters())[2]
      .set_objective_degradation_absolute_tolerance(-1.0);
  EXPECT_THAT(
      ValidateModelSolveParameters(parameters, summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("auxiliary"), HasSubstr("< 0"),
                     HasSubstr("objective_degradation_absolute_tolerance"))));
}

TEST(ValidateModelSolveParametersTest,
     NegativeAuxiliaryObjectiveDegradationRelativeTolerance) {
  ModelSummary summary;
  summary.auxiliary_objectives = {{2, "o2"}};
  ModelSolveParametersProto parameters;
  (*parameters.mutable_auxiliary_objective_parameters())[2]
      .set_objective_degradation_relative_tolerance(-1.0);
  EXPECT_THAT(
      ValidateModelSolveParameters(parameters, summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("auxiliary"), HasSubstr("< 0"),
                     HasSubstr("objective_degradation_relative_tolerance"))));
}

TEST(ValidateModelSolveParametersTest, NegativePrimaryObjectiveTimeLimit) {
  ModelSummary summary;
  ModelSolveParametersProto parameters;
  parameters.mutable_primary_objective_parameters()
      ->mutable_time_limit()
      ->set_seconds(-1);
  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("primary"), HasSubstr("< 0"),
                             HasSubstr("time_limit"))));
}

TEST(ValidateModelSolveParametersTest, NegativeAuxiliaryObjectiveTimeLimit) {
  ModelSummary summary;
  summary.auxiliary_objectives = {{2, "o2"}};
  ModelSolveParametersProto parameters;
  (*parameters.mutable_auxiliary_objective_parameters())[2]
      .mutable_time_limit()
      ->set_seconds(-1.0);
  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("auxiliary"), HasSubstr("< 0"),
                             HasSubstr("time_limit"))));
}

TEST(ValidateModelSolveParametersTest, ObjectiveParametersIdNotInModel) {
  ModelSummary summary;
  summary.auxiliary_objectives = {{2, "o2"}};
  ModelSolveParametersProto parameters;
  (*parameters.mutable_auxiliary_objective_parameters())[3]
      .set_objective_degradation_relative_tolerance(-1.0);
  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("auxiliary"), HasSubstr("3"))));
}

TEST(ValidateModelSolveParametersTest, LazyLinearConstraintIdsOk) {
  ModelSummary summary;
  summary.linear_constraints = {{0, "c"}, {3, "d"}};

  ModelSolveParametersProto parameters;
  parameters.add_lazy_linear_constraint_ids(0);
  parameters.add_lazy_linear_constraint_ids(3);
  EXPECT_OK(ValidateModelSolveParameters(parameters, summary));
}

TEST(ValidateModelSolveParametersTest, LazyLinearConstraintIdInvalid) {
  ModelSummary summary;
  summary.linear_constraints = {{0, "c"}, {3, "d"}};

  ModelSolveParametersProto parameters;
  parameters.add_lazy_linear_constraint_ids(2);
  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Id: 2"),
                             HasSubstr("lazy_linear_constraint_ids"))));
}

TEST(ValidateModelSolveParametersTest, LazyLinearConstraintIdsOutOfOrder) {
  ModelSummary summary;
  summary.linear_constraints = {{0, "c"}, {3, "d"}};

  ModelSolveParametersProto parameters;
  parameters.add_lazy_linear_constraint_ids(3);
  parameters.add_lazy_linear_constraint_ids(0);
  EXPECT_THAT(ValidateModelSolveParameters(parameters, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("strictly increasing"),
                             HasSubstr("lazy_linear_constraint_ids"))));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
