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

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/base/types.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/model_validator.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::absl::StatusCode;
using ::google::protobuf::contrib::parse_proto::ParseTextProto;
using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

ModelSummary SimpleModelSummary(std::vector<int64_t> variable_ids,
                                std::vector<int64_t> constraint_ids) {
  ModelSummary summary;
  absl::c_sort(variable_ids);
  for (const int64_t id : variable_ids) {
    CHECK_OK(summary.variables.Insert(id, ""));
  }
  absl::c_sort(constraint_ids);
  for (const int64_t id : constraint_ids) {
    CHECK_OK(summary.linear_constraints.Insert(id, ""));
  }
  return summary;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Model Update
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, EmptyUpdateValid) {
  ModelSummary summary;
  const ModelUpdateProto update;
  EXPECT_OK(ValidateModelUpdate(update, summary));
  EXPECT_TRUE(summary.variables.Empty());
  EXPECT_TRUE(summary.linear_constraints.Empty());
}

absl::StatusOr<ModelUpdateProto> ParseModelUpdateProto(
    absl::string_view model) {
  return ParseTextProto<ModelUpdateProto>(model);
}

////////////////////////////////////////////////////////////////////////////////
// Variable deletion
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, DeleteOneVar) {
  ModelSummary summary = SimpleModelSummary({3}, {});
  ModelUpdateProto update;
  update.add_deleted_variable_ids(3);
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, DeleteSeveralVars) {
  ModelSummary summary = SimpleModelSummary({3, 4, 7}, {});
  ModelUpdateProto update;
  for (const int id : {3, 4, 7}) {
    update.add_deleted_variable_ids(id);
  }
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, DeleteVarFailNegativeId) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.add_deleted_variable_ids(-2);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("nonnegative"),
                             HasSubstr("invalid variables"))));
}

TEST(ModelUpdateProtoValidatorTest, DeleteVarFailMaxInt64) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.add_deleted_variable_ids(kint64max);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("max(int64_t)"),
                             HasSubstr("invalid variables"))));
}

TEST(ModelUpdateProtoValidatorTest, DeleteVarFailNotIncreasing) {
  ModelSummary summary = SimpleModelSummary({2, 3, 7}, {});
  ModelUpdateProto update;
  for (const int id : {3, 2, 7}) {
    update.add_deleted_variable_ids(id);
  }
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("increasing"), HasSubstr("deleted"),
                             HasSubstr("variable"))));
}

////////////////////////////////////////////////////////////////////////////////
// Linear constraint deletion
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, DeleteOneLinearConstraint) {
  ModelSummary summary = SimpleModelSummary({}, {3});
  ModelUpdateProto update;
  update.add_deleted_linear_constraint_ids(3);
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_TRUE(summary.linear_constraints.Empty());
}

TEST(ModelUpdateProtoValidatorTest, DeleteSeveralLinearConstraints) {
  ModelSummary summary = SimpleModelSummary({}, {3, 4, 7});
  ModelUpdateProto update;
  for (const int id : {3, 4, 7}) {
    update.add_deleted_linear_constraint_ids(id);
  }
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_TRUE(summary.linear_constraints.Empty());
}

TEST(ModelUpdateProtoValidatorTest, DeleteLinearConstraintFailNegativeId) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.add_deleted_linear_constraint_ids(-2);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("nonnegative"),
                             HasSubstr("invalid linear constraints"))));
}

TEST(ModelUpdateProtoValidatorTest, DeleteLinearConstraintFailMaxInt64) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.add_deleted_linear_constraint_ids(kint64max);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("max(int64_t)"),
                             HasSubstr("invalid linear constraints"))));
}

TEST(ModelUpdateProtoValidatorTest, DeleteLinearConstraintFailNotIncreasing) {
  ModelSummary summary = SimpleModelSummary({}, {2, 3, 7});
  ModelUpdateProto update;
  for (const int id : {3, 2, 7}) {
    update.add_deleted_linear_constraint_ids(id);
  }
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("increasing"), HasSubstr("deleted"),
                             HasSubstr("invalid linear constraints"))));
}

////////////////////////////////////////////////////////////////////////////////
// VariableUpdates
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, UpdateOneVariableLb) {
  ModelSummary summary = SimpleModelSummary({3}, {});
  ModelUpdateProto update;
  auto& lbs = *update.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(3);
  lbs.add_values(5.0);
  EXPECT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(3, "")));
}

TEST(ModelUpdateProtoValidatorTest, UpdateOneVariableUb) {
  ModelSummary summary = SimpleModelSummary({3}, {});
  ModelUpdateProto update;
  auto& ubs = *update.mutable_variable_updates()->mutable_upper_bounds();
  ubs.add_ids(3);
  ubs.add_values(5.0);
  EXPECT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(3, "")));
}

TEST(ModelUpdateProtoValidatorTest, UpdateOneVariableInteger) {
  ModelSummary summary = SimpleModelSummary({3}, {});
  ModelUpdateProto update;
  auto& ubs = *update.mutable_variable_updates()->mutable_integers();
  ubs.add_ids(3);
  ubs.add_values(true);
  EXPECT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(3, "")));
}

TEST(ModelUpdateProtoValidatorTest, UpdateSeveralVariables) {
  ModelSummary summary = SimpleModelSummary({0, 1, 2, 4}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         variable_updates {
                           lower_bounds {
                             ids: [ 0, 2 ]
                             values: [ -inf, 5.0 ]
                           }
                           upper_bounds {
                             ids: [ 1, 2 ]
                             values: [ 2.0, inf ]
                           }
                           integers {
                             ids: [ 1, 4 ]
                             values: [ true, false ]
                           }
                         }
                       )pb"));
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

auto BadVariableLowerBounds(absl::string_view error) {
  return StatusIs(StatusCode::kInvalidArgument,
                  AllOf(HasSubstr("ModelUpdateProto.variable_updates invalid"),
                        HasSubstr("Bad lower bounds"), HasSubstr(error)));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateLbInf) {
  ModelSummary summary = SimpleModelSummary({3}, {});
  ModelUpdateProto update;
  auto& lbs = *update.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(3);
  lbs.add_values(kInf);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadVariableLowerBounds("inf"));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateLbNan) {
  ModelSummary summary = SimpleModelSummary({3}, {});
  ModelUpdateProto update;
  auto& lbs = *update.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(3);
  lbs.add_values(kNaN);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadVariableLowerBounds("NaN"));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateLbNotMonotone) {
  ModelSummary summary = SimpleModelSummary({2, 3}, {});
  ModelUpdateProto update;
  auto& lbs = *update.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(3);
  lbs.add_ids(2);
  lbs.add_values(1.0);
  lbs.add_values(1.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadVariableLowerBounds("increasing"));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateLbWrongNumberValues) {
  ModelSummary summary = SimpleModelSummary({3, 5}, {});
  ModelUpdateProto update;
  auto& lbs = *update.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(3);
  lbs.add_ids(5);
  lbs.add_values(1.0);
  lbs.add_values(1.0);
  lbs.add_values(8.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadVariableLowerBounds("Ids size"));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateNegativeId) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& lbs = *update.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(-3);
  lbs.add_values(0.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadVariableLowerBounds("nonnegative"));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateMaxInt64Id) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& lbs = *update.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(kint64max);
  lbs.add_values(0.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadVariableLowerBounds("max(int64_t)"));
}

auto BadUpperBounds(absl::string_view error) {
  return StatusIs(StatusCode::kInvalidArgument,
                  AllOf(HasSubstr("ModelUpdateProto.variable_updates invalid"),
                        HasSubstr("Bad upper bounds"), HasSubstr(error)));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateUbIdsIncreasing) {
  ModelSummary summary = SimpleModelSummary({1, 3}, {});
  ModelUpdateProto update;
  auto& ubs = *update.mutable_variable_updates()->mutable_upper_bounds();
  ubs.add_ids(3);
  ubs.add_ids(1);
  ubs.add_values(2.0);
  ubs.add_values(2.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadUpperBounds("increasing"));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateUbMinusInf) {
  ModelSummary summary = SimpleModelSummary({3}, {});
  ModelUpdateProto update;
  auto& ubs = *update.mutable_variable_updates()->mutable_upper_bounds();
  ubs.add_ids(3);
  ubs.add_values(-kInf);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadUpperBounds("negative infinite"));
}

TEST(ModelUpdateProtoValidatorTest, BadVariableUpdateIntegersIdsIncreasing) {
  ModelSummary summary = SimpleModelSummary({1, 3}, {});
  ModelUpdateProto update;
  auto& integers = *update.mutable_variable_updates()->mutable_integers();
  integers.add_ids(3);
  integers.add_ids(1);
  integers.add_values(true);
  integers.add_values(true);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.variable_updates invalid"),
                     HasSubstr("Bad integers"), HasSubstr("increasing"))));
}

////////////////////////////////////////////////////////////////////////////////
// LinearConstraintUpdates
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, UpdateOneLinearConstraintLb) {
  ModelSummary summary = SimpleModelSummary({}, {3});
  ModelUpdateProto update;
  auto& lbs =
      *update.mutable_linear_constraint_updates()->mutable_lower_bounds();
  lbs.add_ids(3);
  lbs.add_values(5.0);
  EXPECT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(3, "")));
}

TEST(ModelUpdateProtoValidatorTest, UpdateOneLinearConstraintUb) {
  ModelSummary summary = SimpleModelSummary({}, {3});
  ModelUpdateProto update;
  auto& ubs =
      *update.mutable_linear_constraint_updates()->mutable_upper_bounds();
  ubs.add_ids(3);
  ubs.add_values(5.0);
  EXPECT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(3, "")));
}

TEST(ModelUpdateProtoValidatorTest, UpdateSeveralLinearConstraint) {
  ModelSummary summary = SimpleModelSummary({}, {0, 1, 2, 4, 6});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         linear_constraint_updates {
                           lower_bounds {
                             ids: [ 0, 2, 4 ]
                             values: [ -inf, 7.0, 5.5 ]
                           }
                           upper_bounds {
                             ids: [ 1, 2, 6 ]
                             values: [ 2.0, inf, inf ]
                           }
                         }
                       )pb"));
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

auto BadLinearConstraintLowerBounds(absl::string_view error) {
  return StatusIs(
      StatusCode::kInvalidArgument,
      AllOf(HasSubstr("ModelUpdateProto.linear_constraint_updates invalid"),
            HasSubstr("Bad lower bounds"), HasSubstr(error)));
}

TEST(ModelUpdateProtoValidatorTest, BadLinearConstraintLowerUpdateNegativeId) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& lbs =
      *update.mutable_linear_constraint_updates()->mutable_lower_bounds();
  lbs.add_ids(-3);
  lbs.add_values(0.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadLinearConstraintLowerBounds("nonnegative"));
}

TEST(ModelUpdateProtoValidatorTest, BadLinearConstraintUpdateMaxInt64Id) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& lbs =
      *update.mutable_linear_constraint_updates()->mutable_lower_bounds();
  lbs.add_ids(kint64max);
  lbs.add_values(0.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadLinearConstraintLowerBounds("max(int64_t)"));
}

TEST(ModelUpdateProtoValidatorTest, BadLinearConstraintUpdatesLbIdsIncreasing) {
  ModelSummary summary = SimpleModelSummary({}, {1, 3});
  ModelUpdateProto update;
  auto& lbs =
      *update.mutable_linear_constraint_updates()->mutable_lower_bounds();
  lbs.add_ids(3);
  lbs.add_ids(1);
  lbs.add_values(0.0);
  lbs.add_values(0.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadLinearConstraintLowerBounds("increasing"));
}

TEST(ModelUpdateProtoValidatorTest, BadLinearConstraintUpdateLbInf) {
  ModelSummary summary = SimpleModelSummary({}, {3});
  ModelUpdateProto update;
  auto& lbs =
      *update.mutable_linear_constraint_updates()->mutable_lower_bounds();
  lbs.add_ids(3);
  lbs.add_values(kInf);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadLinearConstraintLowerBounds("inf"));
}

auto BadLinearConstraintUpperBounds(absl::string_view error) {
  return StatusIs(
      StatusCode::kInvalidArgument,
      AllOf(HasSubstr("ModelUpdateProto.linear_constraint_updates invalid"),
            HasSubstr("Bad upper bounds"), HasSubstr(error)));
}

TEST(ModelUpdateProtoValidatorTest, BadLinearConstraintUpdatesUbIdsIncreasing) {
  ModelSummary summary = SimpleModelSummary({}, {1, 3});
  ModelUpdateProto update;
  auto& ubs =
      *update.mutable_linear_constraint_updates()->mutable_upper_bounds();
  ubs.add_ids(3);
  ubs.add_ids(1);
  ubs.add_values(2.0);
  ubs.add_values(2.0);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadLinearConstraintUpperBounds("increasing"));
}

TEST(ModelUpdateProtoValidatorTest, BadLinearConstraintUpdateUbMinusInf) {
  ModelSummary summary = SimpleModelSummary({}, {3});
  ModelUpdateProto update;
  auto& ubs =
      *update.mutable_linear_constraint_updates()->mutable_upper_bounds();
  ubs.add_ids(3);
  ubs.add_values(-kInf);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              BadLinearConstraintUpperBounds("negative infinite"));
}

////////////////////////////////////////////////////////////////////////////////
// New variables
//
// Note that the failure modes of the Variables validation are tested
// exhaustively in the tests for ValidateModel();
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, UpdateOneNewVariable) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& vars = *update.mutable_new_variables();
  vars.add_ids(7);
  vars.add_lower_bounds(3.0);
  vars.add_upper_bounds(5.0);
  vars.add_integers(true);
  vars.add_names("x7");
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(7, "x7")));
}

TEST(ModelUpdateProtoValidatorTest, UpdateSeveralNewVariables) {
  ModelSummary summary;
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         new_variables {
                           ids: [ 7, 9, 10 ]
                           lower_bounds: [ -inf, 4.5, 2.0 ]
                           upper_bounds: [ 5.0, 5.0, inf ]
                           integers: [ true, false, true ]
                           names: [ "", "x2", "" ]
                         }
                       )pb"));
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(7, ""), Pair(9, "x2"), Pair(10, "")));
}

TEST(ModelUpdateProtoValidatorTest, BadNewVariablesInternal) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& vars = *update.mutable_new_variables();
  vars.add_ids(7);
  vars.add_lower_bounds(3.0);
  vars.add_upper_bounds(5.0);
  vars.add_upper_bounds(8.0);
  vars.add_integers(true);
  vars.add_names("x7");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("ModelUpdateProto.new_variables invalid")));
}

TEST(ModelUpdateProtoValidatorTest, BadNewVariablesIdsMustIncrease) {
  ModelSummary summary = SimpleModelSummary({9}, {});
  ModelUpdateProto update;
  auto& vars = *update.mutable_new_variables();
  vars.add_ids(7);
  vars.add_lower_bounds(3.0);
  vars.add_upper_bounds(5.0);
  vars.add_integers(true);
  vars.add_names("x7");
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("increasing"), HasSubstr("invalid variables"))));
}

TEST(ModelUpdateProtoValidatorTest,
     BadNewVariablesIdsMustIncreaseSetNextFreeId) {
  ModelSummary summary = SimpleModelSummary({2}, {});
  ASSERT_OK(summary.variables.SetNextFreeId(10));
  ModelUpdateProto update;
  auto& vars = *update.mutable_new_variables();
  vars.add_ids(7);
  vars.add_lower_bounds(3.0);
  vars.add_upper_bounds(5.0);
  vars.add_integers(true);
  vars.add_names("x7");
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("increasing"), HasSubstr("invalid variables"))));
}

TEST(ModelUpdateProtoValidatorTest, NewVariablesDistinctNamesOk) {
  ModelSummary summary;
  ASSERT_OK(summary.variables.Insert(3, "x3"));
  ModelUpdateProto update;
  auto& vars = *update.mutable_new_variables();
  vars.add_ids(7);
  vars.add_lower_bounds(3.0);
  vars.add_upper_bounds(5.0);
  vars.add_integers(true);
  vars.add_names("x7");
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(3, "x3"), Pair(7, "x7")));
}

TEST(ModelUpdateProtoValidatorTest, NewVariablesDupExistingName) {
  ModelSummary summary;
  ASSERT_OK(summary.variables.Insert(3, "x3"));
  ModelUpdateProto update;
  auto& vars = *update.mutable_new_variables();
  vars.add_ids(7);
  vars.add_lower_bounds(3.0);
  vars.add_upper_bounds(5.0);
  vars.add_integers(true);
  vars.add_names("x3");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid variables"),
                             HasSubstr("duplicate name"))));
}

TEST(ModelUpdateProtoValidatorTest, NewVariablesDupNewName) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& vars = *update.mutable_new_variables();
  vars.add_ids(7);
  vars.add_ids(8);
  vars.add_lower_bounds(3.0);
  vars.add_lower_bounds(3.0);
  vars.add_upper_bounds(5.0);
  vars.add_upper_bounds(5.0);
  vars.add_integers(true);
  vars.add_integers(true);
  vars.add_names("x7");
  vars.add_names("x7");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid variables"),
                             HasSubstr("duplicate name"))));
}

////////////////////////////////////////////////////////////////////////////////
// New linear constraints
//
// Note that the failure modes of the Variables validation are tested
// exhaustively in the tests for ValidateModel();
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, UpdateOneNewLinearConstraint) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& lin_cons = *update.mutable_new_linear_constraints();
  lin_cons.add_ids(7);
  lin_cons.add_lower_bounds(3.0);
  lin_cons.add_upper_bounds(5.0);
  lin_cons.add_names("c7");
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(7, "c7")));
}

TEST(ModelUpdateProtoValidatorTest, UpdateSeveralNewLinearConstraints) {
  ModelSummary summary;
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         new_linear_constraints {
                           ids: [ 7, 9, 10 ]
                           lower_bounds: [ -inf, 4.5, 2.0 ]
                           upper_bounds: [ 5.0, 5.0, inf ]
                           names: [ "", "c2", "" ]
                         }
                       )pb"));
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(7, ""), Pair(9, "c2"), Pair(10, "")));
}

TEST(ModelUpdateProtoValidatorTest, BadNewLinearConstraints) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& lin_cons = *update.mutable_new_linear_constraints();
  lin_cons.add_ids(7);
  lin_cons.add_lower_bounds(3.0);
  lin_cons.add_upper_bounds(5.0);
  lin_cons.add_upper_bounds(8.0);
  lin_cons.add_names("c7");
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               HasSubstr("ModelUpdateProto.new_linear_constraints invalid")));
}

TEST(ModelUpdateProtoValidatorTest, BadNewLinearConstraintsIdsMustIncrease) {
  ModelSummary summary = SimpleModelSummary({}, {9});
  ModelUpdateProto update;
  auto& lin_cons = *update.mutable_new_linear_constraints();
  lin_cons.add_ids(7);
  lin_cons.add_lower_bounds(3.0);
  lin_cons.add_upper_bounds(5.0);
  lin_cons.add_names("c7");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("increasing"),
                             HasSubstr("invalid linear constraints"))));
}

TEST(ModelUpdateProtoValidatorTest,
     BadNewLinearConstraintsIdsMustIncreaseSetNextFreeId) {
  ModelSummary summary = SimpleModelSummary({}, {2});
  ASSERT_OK(summary.linear_constraints.SetNextFreeId(10));
  ModelUpdateProto update;
  auto& lin_cons = *update.mutable_new_linear_constraints();
  lin_cons.add_ids(7);
  lin_cons.add_lower_bounds(3.0);
  lin_cons.add_upper_bounds(5.0);
  lin_cons.add_names("c7");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("increasing"),
                             HasSubstr("invalid linear constraints"))));
}

TEST(ModelUpdateProtoValidatorTest, NewLinearConstraintsDistinctNamesOk) {
  ModelSummary summary;
  ASSERT_OK(summary.linear_constraints.Insert(3, "c3"));
  ModelUpdateProto update;
  auto& lin_cons = *update.mutable_new_linear_constraints();
  lin_cons.add_ids(7);
  lin_cons.add_lower_bounds(3.0);
  lin_cons.add_upper_bounds(5.0);
  lin_cons.add_names("c7");
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(3, "c3"), Pair(7, "c7")));
}

TEST(ModelUpdateProtoValidatorTest, NewLinearConstraintsDupExistingName) {
  ModelSummary summary;
  ASSERT_OK(summary.linear_constraints.Insert(3, "c3"));
  ModelUpdateProto update;
  auto& lin_cons = *update.mutable_new_linear_constraints();
  lin_cons.add_ids(7);
  lin_cons.add_lower_bounds(3.0);
  lin_cons.add_upper_bounds(5.0);
  lin_cons.add_names("c3");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid linear constraints"),
                             HasSubstr("duplicate name"))));
}

TEST(ModelUpdateProtoValidatorTest, NewLinearConstraintsDupNewName) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& lin_cons = *update.mutable_new_linear_constraints();
  lin_cons.add_ids(7);
  lin_cons.add_ids(8);
  lin_cons.add_lower_bounds(3.0);
  lin_cons.add_lower_bounds(3.0);
  lin_cons.add_upper_bounds(5.0);
  lin_cons.add_upper_bounds(5.0);
  lin_cons.add_names("c7");
  lin_cons.add_names("c7");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid linear constraints"),
                             HasSubstr("duplicate name"))));
}

////////////////////////////////////////////////////////////////////////////////
// Objective update
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, ObjectiveUpdateOneVar) {
  ModelSummary summary = SimpleModelSummary({7}, {});
  ModelUpdateProto update;
  auto& lin_obj =
      *update.mutable_objective_updates()->mutable_linear_coefficients();
  lin_obj.add_ids(7);
  lin_obj.add_values(3.0);
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(7, "")));
}

TEST(ModelUpdateProtoValidatorTest, JustQuadraticObjectiveUpdate) {
  ModelSummary summary = SimpleModelSummary({2, 3, 7}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         objective_updates {
                           quadratic_coefficients {
                             row_ids: [ 2, 2, 3 ]
                             column_ids: [ 2, 3, 7 ]
                             coefficients: [ 1.2, -1.3, 1.4 ]
                           }
                         }
                       )pb"));
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, ComplexObjectiveUpdate) {
  ModelSummary summary = SimpleModelSummary({2, 3, 7}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         objective_updates {
                           quadratic_coefficients {
                             row_ids: [ 2, 2, 3 ]
                             column_ids: [ 2, 3, 7 ]
                             coefficients: [ 1.2, -1.3, 1.4 ]
                           }
                           linear_coefficients {
                             ids: [ 2, 3, 7 ]
                             values: [ 8.0, -1.5, 3.0 ]
                           }
                           direction_update: true
                           offset_update: 3.5
                         }
                       )pb"));
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, ObjectiveUpdateBadOffset) {
  ModelSummary summary;
  ModelUpdateProto update;
  update.mutable_objective_updates()->set_offset_update(kInf);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.objective_update invalid"),
                     HasSubstr("Offset update invalid"), HasSubstr("inf"))));
}

TEST(ModelUpdateProtoValidatorTest, ObjectiveUpdateInvalidLinearCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         objective_updates {
                           linear_coefficients {
                             ids: [ 2, 10, 7 ]
                             values: [ 8.0, -1.5, 3.0 ]
                           }
                         }
                       )pb"));
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.objective_update invalid"),
                     HasSubstr("Linear objective coefficients invalid"),
                     HasSubstr("strictly increasing"))));
}

TEST(ModelUpdateProtoValidatorTest,
     ObjectiveUpdatePreviouslyDeletedLinearCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ModelUpdateProto update;
  auto& lin_obj =
      *update.mutable_objective_updates()->mutable_linear_coefficients();
  lin_obj.add_ids(4);
  lin_obj.add_values(1.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.objective_update invalid"),
                     HasSubstr("ids not found"))));
}

TEST(ModelUpdateProtoValidatorTest,
     ObjectiveUpdateCurrentlyDeletedLinearCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ModelUpdateProto update;
  update.add_deleted_variable_ids(7);
  auto& lin_obj =
      *update.mutable_objective_updates()->mutable_linear_coefficients();
  lin_obj.add_ids(7);
  lin_obj.add_values(1.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.objective_update invalid"),
                     HasSubstr("ids not found"))));
}

TEST(ModelUpdateProtoValidatorTest, ObjectiveUpdateInvalidLinearCoefsInf) {
  ModelSummary summary = SimpleModelSummary({2}, {});
  ModelUpdateProto update;
  auto& lin_obj =
      *update.mutable_objective_updates()->mutable_linear_coefficients();
  lin_obj.add_ids(2);
  lin_obj.add_values(kInf);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.objective_update invalid"),
                     HasSubstr("Linear objective coefficients invalid"),
                     HasSubstr("inf"))));
}

TEST(ModelUpdateProtoValidatorTest, ObjectiveUpdateInvalidQuadraticCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 3}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         objective_updates {
                           quadratic_coefficients {
                             row_ids: [ 2, 3 ]
                             column_ids: [ 2, 2 ]
                             coefficients: [ 8.0, -1.5 ]
                           }
                         }
                       )pb"));
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.objective_update invalid"),
                     HasSubstr("Objective.quadratic_coefficients"),
                     HasSubstr("lower triangular"))));
}

TEST(ModelUpdateProtoValidatorTest, ObjectiveUpdatePreviouslyDeletedQuadCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ModelUpdateProto update;
  auto& quad_obj =
      *update.mutable_objective_updates()->mutable_quadratic_coefficients();
  quad_obj.add_row_ids(4);
  quad_obj.add_column_ids(4);
  quad_obj.add_coefficients(1.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.objective_update invalid"),
                     HasSubstr("quadratic_coefficients invalid"))));
}

TEST(ModelUpdateProtoValidatorTest, ObjectiveUpdateCurrentlyDeletedQuadCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ModelUpdateProto update;
  update.add_deleted_variable_ids(7);
  auto& quad_obj =
      *update.mutable_objective_updates()->mutable_quadratic_coefficients();
  quad_obj.add_row_ids(7);
  quad_obj.add_column_ids(7);
  quad_obj.add_coefficients(1.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.objective_update invalid"),
                     HasSubstr("quadratic_coefficients invalid"))));
}

TEST(ModelUpdateProtoValidatorTest, BadObjectivePriorityUpdate) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ModelUpdateProto update;
  update.mutable_objective_updates()->set_priority_update(-1);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("nonnegative"), HasSubstr("priority: -1"))));
}

////////////////////////////////////////////////////////////////////////////////
// Auxiliary objectives update
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, AuxiliaryObjectiveUpdateOneVar) {
  ModelSummary summary = SimpleModelSummary({7}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ModelUpdateProto update;
  auto& lin_obj = (*update.mutable_auxiliary_objectives_updates()
                        ->mutable_objective_updates())[0];
  lin_obj.mutable_linear_coefficients()->add_ids(7);
  lin_obj.mutable_linear_coefficients()->add_values(3.0);
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(7, "")));
}

TEST(ModelUpdateProtoValidatorTest, JustAuxiliaryQuadraticObjectiveUpdate) {
  ModelSummary summary = SimpleModelSummary({2, 3, 7}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         auxiliary_objectives_updates {
                           objective_updates {
                             key: 0
                             value: {
                               quadratic_coefficients {
                                 row_ids: [ 2, 2, 3 ]
                                 column_ids: [ 2, 3, 7 ]
                                 coefficients: [ 1.2, -1.3, 1.4 ]
                               }
                             }
                           }
                         }
                       )pb"));
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, ComplexAuxiliaryObjectiveUpdate) {
  ModelSummary summary = SimpleModelSummary({2, 3, 7}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         auxiliary_objectives_updates {
                           objective_updates {
                             key: 0
                             value: {
                               quadratic_coefficients {
                                 row_ids: [ 2, 2, 3 ]
                                 column_ids: [ 2, 3, 7 ]
                                 coefficients: [ 1.2, -1.3, 1.4 ]
                               }
                               linear_coefficients {
                                 ids: [ 2, 3, 7 ]
                                 values: [ 8.0, -1.5, 3.0 ]
                               }
                               direction_update: true
                               offset_update: 3.5
                             }
                           }
                         }
                       )pb"));
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, AuxiliaryObjectiveUpdateBadOffset) {
  ModelSummary summary;
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[0]
      .set_offset_update(kInf);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr(
                    "ModelUpdateProto.auxiliary_objectives_updates invalid"),
                HasSubstr("Offset update invalid"), HasSubstr("inf"))));
}

TEST(ModelUpdateProtoValidatorTest,
     AuxiliaryObjectiveUpdateInvalidLinearCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         auxiliary_objectives_updates {
                           objective_updates {
                             key: 0
                             value: {
                               linear_coefficients {
                                 ids: [ 2, 10, 7 ]
                                 values: [ 8.0, -1.5, 3.0 ]
                               }
                             }
                           }
                         }
                       )pb"));
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr(
                    "ModelUpdateProto.auxiliary_objectives_updates invalid"),
                HasSubstr("Linear objective coefficients invalid"),
                HasSubstr("strictly increasing"))));
}

TEST(ModelUpdateProtoValidatorTest,
     AuxiliaryObjectiveUpdatePreviouslyDeletedLinearCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ModelUpdateProto update;
  auto& lin_obj = *(*update.mutable_auxiliary_objectives_updates()
                         ->mutable_objective_updates())[0]
                       .mutable_linear_coefficients();
  lin_obj.add_ids(4);
  lin_obj.add_values(1.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr(
                    "ModelUpdateProto.auxiliary_objectives_updates invalid"),
                HasSubstr("ids not found"))));
}

TEST(ModelUpdateProtoValidatorTest,
     AuxiliaryObjectiveUpdateCurrentlyDeletedLinearCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ModelUpdateProto update;
  update.add_deleted_variable_ids(7);
  auto& lin_obj = *(*update.mutable_auxiliary_objectives_updates()
                         ->mutable_objective_updates())[0]
                       .mutable_linear_coefficients();
  lin_obj.add_ids(7);
  lin_obj.add_values(1.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr(
                    "ModelUpdateProto.auxiliary_objectives_updates invalid"),
                HasSubstr("ids not found"))));
}

TEST(ModelUpdateProtoValidatorTest,
     AuxiliaryObjectiveUpdateInvalidLinearCoefsInf) {
  ModelSummary summary = SimpleModelSummary({2}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ModelUpdateProto update;
  auto& lin_obj = *(*update.mutable_auxiliary_objectives_updates()
                         ->mutable_objective_updates())[0]
                       .mutable_linear_coefficients();
  lin_obj.add_ids(2);
  lin_obj.add_values(kInf);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr(
                    "ModelUpdateProto.auxiliary_objectives_updates invalid"),
                HasSubstr("Linear objective coefficients invalid"),
                HasSubstr("inf"))));
}

TEST(ModelUpdateProtoValidatorTest,
     AuxiliaryObjectiveUpdateInvalidQuadraticCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 3}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(4, ""));
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         auxiliary_objectives_updates {
                           objective_updates {
                             key: 4
                             value: {
                               quadratic_coefficients {
                                 row_ids: [ 2, 3 ]
                                 column_ids: [ 2, 2 ]
                                 coefficients: [ 8.0, -1.5 ]
                               }
                             }
                           }
                         }
                       )pb"));
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr(
                    "ModelUpdateProto.auxiliary_objectives_updates invalid"),
                HasSubstr("Objective.quadratic_coefficients"),
                HasSubstr("lower triangular"))));
}

TEST(ModelUpdateProtoValidatorTest,
     AuxiliaryObjectiveUpdatePreviouslyDeletedQuadCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ModelUpdateProto update;
  auto& quad_obj = *(*update.mutable_auxiliary_objectives_updates()
                          ->mutable_objective_updates())[0]
                        .mutable_quadratic_coefficients();
  quad_obj.add_row_ids(4);
  quad_obj.add_column_ids(4);
  quad_obj.add_coefficients(1.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr(
                    "ModelUpdateProto.auxiliary_objectives_updates invalid"),
                HasSubstr("quadratic_coefficients invalid"))));
}

TEST(ModelUpdateProtoValidatorTest,
     AuxiliaryObjectiveUpdateCurrentlyDeletedQuadCoefs) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, ""));
  ModelUpdateProto update;
  update.add_deleted_variable_ids(7);
  auto& quad_obj = *(*update.mutable_auxiliary_objectives_updates()
                          ->mutable_objective_updates())[0]
                        .mutable_quadratic_coefficients();
  quad_obj.add_row_ids(7);
  quad_obj.add_column_ids(7);
  quad_obj.add_coefficients(1.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr(
                    "ModelUpdateProto.auxiliary_objectives_updates invalid"),
                HasSubstr("quadratic_coefficients invalid"))));
}

TEST(ModelUpdateProtoValidatorTest, BadNewAuxiliaryObjectivePriority) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[2]
      .set_priority(-1);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("nonnegative"), HasSubstr("priority: -1"))));
}

TEST(ModelUpdateProtoValidatorTest, BadAuxiliaryObjectivePriorityUpdate) {
  ModelSummary summary = SimpleModelSummary({2, 7, 10}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(2, ""));
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[2]
      .set_priority_update(-1);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("nonnegative"), HasSubstr("priority: -1"))));
}

TEST(ModelUpdateProtoValidatorTest, UpdateAddsAuxiliaryObjective) {
  ModelSummary summary = SimpleModelSummary({7}, {});
  ASSERT_OK(summary.auxiliary_objectives.Insert(0, "obj0"));
  ASSERT_OK(summary.auxiliary_objectives.Insert(2, "obj2"));
  ModelUpdateProto update;
  auto& new_obj = (*update.mutable_auxiliary_objectives_updates()
                        ->mutable_new_objectives())[5];
  new_obj.mutable_linear_coefficients()->add_ids(7);
  new_obj.mutable_linear_coefficients()->add_values(1.0);
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(
      summary.auxiliary_objectives.id_to_name(),
      UnorderedElementsAre(Pair(0, "obj0"), Pair(2, "obj2"), Pair(5, "")));
}

TEST(ModelUpdateProtoValidatorTest, UpdateAddsSeveralAuxiliaryObjectives) {
  ModelSummary summary;
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         auxiliary_objectives_updates {
                           new_objectives {
                             key: 3
                             value: { name: "obj3" }
                           }
                           new_objectives {
                             key: 7
                             value: {}
                           }
                         }
                       )pb"));
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.auxiliary_objectives.id_to_name(),
              UnorderedElementsAre(Pair(3, "obj3"), Pair(7, "")));
}

TEST(ModelUpdateProtoValidatorTest, BadNewAuxiliaryObjective) {
  ModelSummary summary;
  ModelUpdateProto update;
  auto& new_obj = (*update.mutable_auxiliary_objectives_updates()
                        ->mutable_new_objectives())[3];
  new_obj.mutable_linear_coefficients()->add_values(2.0);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr("ModelUpdateProto.auxiliary_objectives_updates invalid")));
}

TEST(ModelUpdateProtoValidatorTest, BadNewAuxiliaryObjectivesIdsMustIncrease) {
  ModelSummary summary;
  ASSERT_OK(summary.auxiliary_objectives.Insert(3, "obj3"));
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[2];
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("increasing"),
                             HasSubstr("invalid auxiliary objectives"))));
}

TEST(ModelUpdateProtoValidatorTest,
     BadNewAuxiliaryObjectivesIdsMustIncreaseSetNextFreeId) {
  ModelSummary summary;
  ASSERT_OK(summary.auxiliary_objectives.Insert(3, "obj3"));
  ASSERT_OK(summary.auxiliary_objectives.SetNextFreeId(10));
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[7];
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("increasing"),
                             HasSubstr("invalid auxiliary objectives"))));
}

TEST(ModelUpdateProtoValidatorTest, NewAuxiliaryObjectivesDistinctNamesOk) {
  ModelSummary summary;
  ASSERT_OK(summary.auxiliary_objectives.Insert(3, "obj3"));
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[7]
      .set_name("obj7");
  ASSERT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.auxiliary_objectives.id_to_name(),
              UnorderedElementsAre(Pair(3, "obj3"), Pair(7, "obj7")));
}

TEST(ModelUpdateProtoValidatorTest,
     NewAuxiliaryObjectivesDupExistingAuxiliaryName) {
  ModelSummary summary;
  ASSERT_OK(summary.auxiliary_objectives.Insert(3, "obj"));
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[5]
      .set_name("obj");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid auxiliary objectives"),
                             HasSubstr("duplicate name"))));
}

TEST(ModelUpdateProtoValidatorTest,
     NewAuxiliaryObjectivesDupExistingPrimaryName) {
  ModelSummary summary;
  summary.primary_objective_name = "obj";
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[5]
      .set_name("obj");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("duplicate objective name")));
}

TEST(ModelUpdateProtoValidatorTest, NewAuxiliaryObjectivesDupNewName) {
  ModelSummary summary;
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[3]
      .set_name("obj");
  (*update.mutable_auxiliary_objectives_updates()->mutable_new_objectives())[5]
      .set_name("obj");
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid auxiliary objectives"),
                             HasSubstr("duplicate name"))));
}

TEST(ModelUpdateProtoValidatorTest, BadAuxiliaryObjectiveUpdateId) {
  ModelSummary summary;
  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[2];
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.auxiliary_objectives_updates"),
                     HasSubstr("objective update"), HasSubstr("id: 2"))));
}

TEST(ModelUpdateProtoValidatorTest, AddAndUpdateSameAuxiliaryObjective) {
  ModelSummary summary;
  EXPECT_THAT(
      ValidateModelUpdate(ParseTextProtoOrDie(
                              R"pb(auxiliary_objectives_updates {
                                     new_objectives {
                                       key: 1
                                       value {}
                                     }
                                     objective_updates {
                                       key: 1
                                       value { offset_update: 0.5 }
                                     }
                                   }
                              )pb"),
                          summary),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          AllOf(
              HasSubstr("ModelUpdateProto.auxiliary_objectives_updates"),
              HasSubstr(
                  "objective update on new auxiliary objective with id: 1"))));
}

TEST(ElementalFromProtoTest, UpdateAndDeleteSameAuxiliaryObjective) {
  ModelSummary summary;
  ASSERT_OK(summary.Update(ParseTextProtoOrDie(
      R"pb(auxiliary_objectives_updates {
             new_objectives {
               key: 1
               value {}
             }
           }
      )pb")));
  EXPECT_THAT(
      ValidateModelUpdate(ParseTextProtoOrDie(
                              R"pb(auxiliary_objectives_updates {
                                     deleted_objective_ids: 1
                                     objective_updates {
                                       key: 1
                                       value { offset_update: 0.5 }
                                     }
                                   }
                              )pb"),
                          summary),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelUpdateProto.auxiliary_objectives_updates"),
                     HasSubstr("objective update on deleted auxiliary "
                               "objective with id: 1"))));
}

////////////////////////////////////////////////////////////////////////////////
// Linear constraint matrix update
//
// NOTE: the validation of this submessage is already tested through Model.
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, LinearConstraintMatrixSmallUpdate) {
  ModelSummary summary = SimpleModelSummary({12}, {7});
  ModelUpdateProto update;
  auto& lin_con_mat = *update.mutable_linear_constraint_matrix_updates();
  lin_con_mat.add_row_ids(7);
  lin_con_mat.add_column_ids(12);
  lin_con_mat.add_coefficients(3.0);
  EXPECT_OK(ValidateModelUpdate(update, summary));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(12, "")));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(7, "")));
}

TEST(ModelUpdateProtoValidatorTest, InvalidLinearConstraintMatrixUpdate) {
  ModelSummary summary = SimpleModelSummary({12}, {7, 19});
  ModelUpdateProto update;
  auto& lin_con_mat = *update.mutable_linear_constraint_matrix_updates();
  lin_con_mat.add_row_ids(7);
  lin_con_mat.add_row_ids(19);
  lin_con_mat.add_column_ids(12);
  lin_con_mat.add_coefficients(3.0);

  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(
          StatusCode::kInvalidArgument,
          HasSubstr(
              "ModelUpdateProto.linear_constraint_matrix_updates invalid")));
}

TEST(ModelUpdateProtoValidatorTest, InvalidLinearConstraintMatrixBadRow) {
  ModelSummary summary = SimpleModelSummary({12}, {19});
  ModelUpdateProto update;
  auto& lin_con_mat = *update.mutable_linear_constraint_matrix_updates();
  lin_con_mat.add_row_ids(7);
  lin_con_mat.add_column_ids(12);
  lin_con_mat.add_coefficients(3.0);

  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid linear constraint matrix update")));
}

TEST(ModelUpdateProtoValidatorTest, InvalidLinearConstraintMatrixBadColumn) {
  ModelSummary summary = SimpleModelSummary({12}, {19});
  ModelUpdateProto update;
  auto& lin_con_mat = *update.mutable_linear_constraint_matrix_updates();
  lin_con_mat.add_row_ids(19);
  lin_con_mat.add_column_ids(99);
  lin_con_mat.add_coefficients(3.0);

  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("invalid linear constraint matrix update")));
}

TEST(ModelUpdateProtoValidatorTest, ComplexLinearConstraintMatrixUpdate) {
  ModelSummary summary = SimpleModelSummary({2, 3, 5, 6, 7}, {100, 101, 104});
  ASSERT_OK_AND_ASSIGN(const auto good_update, ParseModelUpdateProto(R"pb(
                         deleted_variable_ids: [ 5, 6 ]
                         deleted_linear_constraint_ids: [ 100 ]
                         new_variables: {
                           ids: [ 10, 12 ]
                           lower_bounds: [ 0.0, 0.0 ]
                           upper_bounds: [ 1.0, 1.0 ]
                           integers: [ true, true ]
                         }
                         new_linear_constraints: {
                           ids: [ 110, 112 ]
                           lower_bounds: [ 0.0, 0.0 ]
                           upper_bounds: [ 1.0, 1.0 ]
                         }
                         linear_constraint_matrix_updates {
                           row_ids: [ 101, 101, 110, 112 ]
                           column_ids: [ 3, 10, 2, 12 ]
                           coefficients: [ 3.0, 4.0, 5.0, 6.0 ]
                         }
                       )pb"));
  EXPECT_OK(ValidateModelUpdate(good_update, summary));
}

////////////////////////////////////////////////////////////////////////////////
// Second-order cone constraints
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, AddTrivialSecondOrderConeConstraint) {
  ModelSummary summary = SimpleModelSummary({12}, {});
  ModelUpdateProto update;
  (*update.mutable_second_order_cone_constraint_updates()
        ->mutable_new_constraints())[1] = {};
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, AddInvalidSecondOrderConeConstraint) {
  ModelSummary summary = SimpleModelSummary({12}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         second_order_cone_constraint_updates {
                           new_constraints {
                             key: 1
                             value {
                               upper_bound {
                                 ids: [ 13 ]
                                 coefficients: [ 1.0 ]
                               }
                             }
                           }
                         }
                       )pb"));
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("invalid variable id: 13"),
                     HasSubstr("ModelUpdateProto.second_order_cone_constraint_"
                               "updates.new_constraints invalid"))));
}
TEST(ModelUpdateProtoValidatorTest, DeleteSecondOrderConeConstraint) {
  ModelSummary summary;
  ASSERT_OK(summary.second_order_cone_constraints.Insert(3, ""));
  ModelUpdateProto update;
  update.mutable_second_order_cone_constraint_updates()
      ->add_deleted_constraint_ids(3);
  EXPECT_OK(ValidateModelUpdate(update, summary));

  update.mutable_second_order_cone_constraint_updates()
      ->add_deleted_constraint_ids(4);
  EXPECT_THAT(
      ValidateModelUpdate(update, summary),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("cannot delete missing id 3"),
                     HasSubstr("invalid second-order cone constraints"))));
}

////////////////////////////////////////////////////////////////////////////////
// SOS1 constraints
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, AddTrivialSos1Constraint) {
  ModelSummary summary = SimpleModelSummary({12}, {});
  ModelUpdateProto update;
  update.mutable_sos1_constraint_updates()->mutable_new_constraints()->insert(
      {1, SosConstraintProto()});
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, AddInvalidSos1Constraint) {
  ModelSummary summary = SimpleModelSummary({12}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         sos1_constraint_updates {
                           new_constraints {
                             key: 1
                             value {
                               expressions {
                                 ids: [ 13 ]
                                 coefficients: [ 1.0 ]
                               }
                               weights: [ 1.0 ]
                             }
                           }
                         }
                       )pb"));
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid variable id: 13"),
                             HasSubstr("ModelUpdateProto.sos1_constraint_"
                                       "updates.new_constraints invalid"))));
}
TEST(ModelUpdateProtoValidatorTest, DeleteSos1Constraint) {
  ModelSummary summary;
  ASSERT_OK(summary.sos1_constraints.Insert(3, ""));
  ModelUpdateProto update;
  update.mutable_sos1_constraint_updates()->add_deleted_constraint_ids(3);
  EXPECT_OK(ValidateModelUpdate(update, summary));

  update.mutable_sos1_constraint_updates()->add_deleted_constraint_ids(4);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id 3"),
                             HasSubstr("invalid sos1 constraints"))));
}

////////////////////////////////////////////////////////////////////////////////
// SOS2 constraints
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, AddTrivialSos2Constraint) {
  ModelSummary summary = SimpleModelSummary({12}, {});
  ModelUpdateProto update;
  update.mutable_sos2_constraint_updates()->mutable_new_constraints()->insert(
      {1, SosConstraintProto()});
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, AddInvalidSos2Constraint) {
  ModelSummary summary = SimpleModelSummary({12}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         sos2_constraint_updates {
                           new_constraints {
                             key: 1
                             value {
                               expressions {
                                 ids: [ 13 ]
                                 coefficients: [ 1.0 ]
                               }
                               weights: [ 1.0 ]
                             }
                           }
                         }
                       )pb"));
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("invalid variable id: 13"),
                             HasSubstr("ModelUpdateProto.sos2_constraint_"
                                       "updates.new_constraints invalid"))));
}
TEST(ModelUpdateProtoValidatorTest, DeleteSos2Constraint) {
  ModelSummary summary;
  ASSERT_OK(summary.sos2_constraints.Insert(3, ""));
  ModelUpdateProto update;
  update.mutable_sos2_constraint_updates()->add_deleted_constraint_ids(3);
  EXPECT_OK(ValidateModelUpdate(update, summary));

  update.mutable_sos2_constraint_updates()->add_deleted_constraint_ids(4);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id 3"),
                             HasSubstr("invalid sos2 constraints"))));
}

////////////////////////////////////////////////////////////////////////////////
// Indicator constraints
////////////////////////////////////////////////////////////////////////////////

TEST(ModelUpdateProtoValidatorTest, AddTrivialIndicatorConstraint) {
  ModelSummary summary = SimpleModelSummary({12}, {});
  ModelUpdateProto update;
  IndicatorConstraintProto& constraint =
      (*update.mutable_indicator_constraint_updates()
            ->mutable_new_constraints())[1];
  constraint.set_indicator_id(12);
  constraint.set_lower_bound(-1.0);
  constraint.set_upper_bound(1.0);
  EXPECT_OK(ValidateModelUpdate(update, summary));
}

TEST(ModelUpdateProtoValidatorTest, AddInvalidIndicatorConstraint) {
  ModelSummary summary = SimpleModelSummary({12}, {});
  ASSERT_OK_AND_ASSIGN(const auto update, ParseModelUpdateProto(R"pb(
                         indicator_constraint_updates {
                           new_constraints {
                             key: 1
                             value {
                               indicator_id: 12
                               expression {
                                 ids: [ 13 ]
                                 values: [ 1.0 ]
                               }
                               lower_bound: -1.0
                               upper_bound: 1.0
                             }
                           }
                         }
                       )pb"));
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Invalid variable id"),
                             HasSubstr("ModelUpdateProto.indicator_constraint_"
                                       "updates.new_constraints invalid"))));
}
TEST(ModelUpdateProtoValidatorTest, DeleteIndicatorConstraint) {
  ModelSummary summary;
  ASSERT_OK(summary.indicator_constraints.Insert(3, ""));
  ModelUpdateProto update;
  update.mutable_indicator_constraint_updates()->add_deleted_constraint_ids(3);
  EXPECT_OK(ValidateModelUpdate(update, summary));

  update.mutable_indicator_constraint_updates()->add_deleted_constraint_ids(4);
  EXPECT_THAT(ValidateModelUpdate(update, summary),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("cannot delete missing id 3"),
                             HasSubstr("invalid indicator constraints"))));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
