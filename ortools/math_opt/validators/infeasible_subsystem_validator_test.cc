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

#include <string>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/result.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

TEST(ValidateModelSubsetTest, EmptyModelSubsetOk) {
  EXPECT_OK(ValidateModelSubset(ModelSubsetProto(), ModelSummary()));
}

TEST(ValidateModelSubsetTest, SimpleModelSubsetOk) {
  ModelSummary summary;
  ASSERT_OK(summary.variables.Insert(1, ""));
  ASSERT_OK(summary.linear_constraints.Insert(2, ""));
  ASSERT_OK(summary.quadratic_constraints.Insert(3, ""));
  ASSERT_OK(summary.second_order_cone_constraints.Insert(4, ""));
  ASSERT_OK(summary.sos1_constraints.Insert(5, ""));
  ASSERT_OK(summary.sos2_constraints.Insert(6, ""));
  ASSERT_OK(summary.indicator_constraints.Insert(7, ""));

  ModelSubsetProto subset;
  (*subset.mutable_variable_bounds())[1];
  subset.add_variable_integrality(1);
  (*subset.mutable_linear_constraints())[2];
  (*subset.mutable_quadratic_constraints())[3];
  subset.add_second_order_cone_constraints(4);
  subset.add_sos1_constraints(5);
  subset.add_sos2_constraints(6);
  subset.add_indicator_constraints(7);

  EXPECT_OK(ValidateModelSubset(subset, summary));
}

TEST(ValidateModelSubsetTest, UnknownVariableBounds) {
  ModelSubsetProto subset;
  (*subset.mutable_variable_bounds())[1];
  EXPECT_THAT(ValidateModelSubset(subset, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("variable_bounds")));
}

TEST(ValidateModelSubsetTest, UnknownVariableIntegrity) {
  ModelSubsetProto subset;
  subset.add_variable_integrality(1);
  EXPECT_THAT(
      ValidateModelSubset(subset, ModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("variable")));
}

TEST(ValidateModelSubsetTest, UnknownLinearConstraint) {
  ModelSubsetProto subset;
  (*subset.mutable_linear_constraints())[2];
  EXPECT_THAT(ValidateModelSubset(subset, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("linear_constraints")));
}

TEST(ValidateModelSubsetTest, UnknownQuadraticConstraint) {
  ModelSubsetProto subset;
  (*subset.mutable_quadratic_constraints())[3];
  EXPECT_THAT(ValidateModelSubset(subset, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("quadratic_constraints")));
}

TEST(ValidateModelSubsetTest, UnknownSecondOrderConeConstraint) {
  ModelSubsetProto subset;
  subset.add_second_order_cone_constraints(4);
  EXPECT_THAT(ValidateModelSubset(subset, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("second_order_cone_constraints")));
}

TEST(ValidateModelSubsetTest, UnsortedSecondOrderConeConstraints) {
  ModelSubsetProto subset;
  subset.add_second_order_cone_constraints(4);
  subset.add_second_order_cone_constraints(2);
  ModelSummary summary;
  ASSERT_OK(summary.second_order_cone_constraints.Insert(2, ""));
  ASSERT_OK(summary.second_order_cone_constraints.Insert(4, ""));
  EXPECT_THAT(ValidateModelSubset(subset, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("second_order_cone_constraints")));
}

TEST(ValidateModelSubsetTest, UnknownSos1Constraint) {
  ModelSubsetProto subset;
  subset.add_sos1_constraints(5);
  EXPECT_THAT(ValidateModelSubset(subset, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("sos1_constraint")));
}

TEST(ValidateModelSubsetTest, UnsortedSos1Constraints) {
  ModelSubsetProto subset;
  subset.add_sos1_constraints(4);
  subset.add_sos1_constraints(2);
  ModelSummary summary;
  ASSERT_OK(summary.sos1_constraints.Insert(2, ""));
  ASSERT_OK(summary.sos1_constraints.Insert(4, ""));
  EXPECT_THAT(ValidateModelSubset(subset, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("sos1_constraint")));
}

TEST(ValidateModelSubsetTest, UnknownSos2Constraint) {
  ModelSubsetProto subset;
  subset.add_sos2_constraints(6);
  EXPECT_THAT(ValidateModelSubset(subset, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("sos2_constraint")));
}

TEST(ValidateModelSubsetTest, UnsortedSos2Constraints) {
  ModelSubsetProto subset;
  subset.add_sos2_constraints(4);
  subset.add_sos2_constraints(2);
  ModelSummary summary;
  ASSERT_OK(summary.sos2_constraints.Insert(2, ""));
  ASSERT_OK(summary.sos2_constraints.Insert(4, ""));
  EXPECT_THAT(ValidateModelSubset(subset, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("sos2_constraint")));
}

TEST(ValidateModelSubsetTest, UnknownIndicatorConstraint) {
  ModelSubsetProto subset;
  subset.add_indicator_constraints(7);
  EXPECT_THAT(ValidateModelSubset(subset, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator_constraint")));
}

TEST(ValidateModelSubsetTest, UnsortedIndicatorConstraints) {
  ModelSubsetProto subset;
  subset.add_indicator_constraints(4);
  subset.add_indicator_constraints(2);
  ModelSummary summary;
  ASSERT_OK(summary.indicator_constraints.Insert(2, ""));
  ASSERT_OK(summary.indicator_constraints.Insert(4, ""));
  EXPECT_THAT(ValidateModelSubset(subset, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("indicator_constraint")));
}

ComputeInfeasibleSubsystemResultProto SimpleResultProto() {
  ComputeInfeasibleSubsystemResultProto result;
  result.set_feasibility(FEASIBILITY_STATUS_INFEASIBLE);
  result.set_is_minimal(true);
  return result;
}

TEST(ValidateComputeInfeasibleSubsystemResultTest, TrivialResultOk) {
  EXPECT_OK(ValidateComputeInfeasibleSubsystemResult(SimpleResultProto(),
                                                     ModelSummary()));
}

TEST(ValidateComputeInfeasibleSubsystemResultNoModelTest, InvalidFeasibility) {
  ComputeInfeasibleSubsystemResultProto result = SimpleResultProto();
  result.set_feasibility(FEASIBILITY_STATUS_UNSPECIFIED);
  EXPECT_THAT(ValidateComputeInfeasibleSubsystemResult(result, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("FEASIBILITY_STATUS_UNSPECIFIED")));
  EXPECT_THAT(ValidateComputeInfeasibleSubsystemResultNoModel(result),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("FEASIBILITY_STATUS_UNSPECIFIED")));
}

TEST(ValidateComputeInfeasibleSubsystemResultTest,
     InvalidComputeInfeasibleSubsystem) {
  ComputeInfeasibleSubsystemResultProto result = SimpleResultProto();
  result.mutable_infeasible_subsystem()->add_second_order_cone_constraints(1);
  EXPECT_THAT(ValidateComputeInfeasibleSubsystemResult(result, ModelSummary()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("second_order_cone_constraints")));
}

TEST(ValidateComputeInfeasibleSubsystemResultTest,
     FeasibleWithNonemptyComputeInfeasibleSubsystem) {
  ComputeInfeasibleSubsystemResultProto result;
  result.set_feasibility(FEASIBILITY_STATUS_FEASIBLE);
  result.mutable_infeasible_subsystem()->add_second_order_cone_constraints(1);
  ModelSummary summary;
  ASSERT_OK(summary.second_order_cone_constraints.Insert(1, ""));
  EXPECT_THAT(ValidateComputeInfeasibleSubsystemResult(result, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("infeasible_subsystem")));
  EXPECT_THAT(ValidateComputeInfeasibleSubsystemResultNoModel(result),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("infeasible_subsystem")));
}

TEST(ValidateComputeInfeasibleSubsystemResultTest, FeasibleWithIsMinimalTrue) {
  ComputeInfeasibleSubsystemResultProto result = SimpleResultProto();
  result.set_feasibility(FEASIBILITY_STATUS_FEASIBLE);
  result.clear_infeasible_subsystem();
  EXPECT_THAT(
      ValidateComputeInfeasibleSubsystemResult(result, ModelSummary()),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("is_minimal")));
  EXPECT_THAT(
      ValidateComputeInfeasibleSubsystemResultNoModel(result),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("is_minimal")));
}

}  // namespace
}  // namespace operations_research::math_opt
