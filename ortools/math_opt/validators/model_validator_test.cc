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

#include <cmath>
#include <limits>
#include <optional>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/types.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
namespace operations_research {
namespace math_opt {
namespace {

using ::absl::StatusCode;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(ModelValidatorTest, EmptyModelValid) {
  // TODO(b/232619901): centralize tracking of all everything that needs to be
  // updated when adding a new constraint type.
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary, ValidateModel(ModelProto()));
  EXPECT_TRUE(summary.variables.Empty());
  EXPECT_TRUE(summary.linear_constraints.Empty());
}

ModelProto ParseModel(absl::string_view model) {
  ModelProto result;
  CHECK(google::protobuf::TextFormat::ParseFromString(model, &result));
  return result;
}

// Maximize 5.0*x + 4.0
//          2.0 <= x <= 3.0
//          -inf <= 6.0*x <= 15.0
//
// with variable/constraint ids and names.
ModelProto SimpleModel() {
  return ParseModel(R"pb(
    name: "simple_model"
    variables: {
      ids: [ 3 ],
      lower_bounds: [ 2.0 ]
      upper_bounds: [ 3.0 ]
      integers: [ true ]
      names: [ "x3" ]
    }
    objective: {
      maximize: true
      offset: 4.0
      linear_coefficients {
        ids: [ 3 ]
        values: [ 5.0 ]
      }
    }
    linear_constraints: {
      ids: [ 4 ]
      lower_bounds: [ -infinity ]
      upper_bounds: [ 15.0 ]
      names: [ "c4" ]
    }
    linear_constraint_matrix {
      column_ids: [ 3 ]
      row_ids: [ 4 ]
      coefficients: [ 6.0 ]
    }
  )pb");
}

TEST(ModelValidatorTest, SimpleModelValid) {
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary,
                       ValidateModel(SimpleModel()));
  EXPECT_THAT(summary.variables.id_to_name(),
              UnorderedElementsAre(Pair(3, "x3")));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(4, "c4")));
}

// Maximize 4.0*x3^2 + 3.0*x3*x5 + 2.0*x5*x8 + 5.0*x3 + 6.0*x4 + 7.0*x5 + 4.0
//          2.0 <= x3 <= 7.0
//          3.0 <= x4 <= 6.0
//          4.0 <= x5 <= 5.0
//          -inf <= 1.0*x3 + 1.0*x5 + 1.0*x8 <= 10.0
//
// with variable/constraint ids and names.
ModelProto SimpleQuadraticObjectiveModel() {
  return ParseModel(R"pb(
    name: "simple_three_variable_model"
    variables: {
      ids: [ 3, 5, 8 ],
      lower_bounds: [ 2.0, 3.0, 4.0 ]
      upper_bounds: [ 7.0, 6.0, 5.0 ]
      integers: [ true, false, false ]
      names: [ "x3", "x5", "x8" ]
    }
    objective: {
      maximize: true
      offset: 4.0
      linear_coefficients {
        ids: [ 3, 5, 8 ]
        values: [ 5.0, 6.0, 7.0 ]
      }
      quadratic_coefficients {
        row_ids: [ 3, 3, 5 ]
        column_ids: [ 3, 5, 8 ]
        coefficients: [ 4.0, 3.0, 2.0 ]
      }
    }
    linear_constraints: {
      ids: [ 4 ]
      lower_bounds: [ -infinity ]
      upper_bounds: [ 10.0 ]
      names: [ "c4" ]
    }
    linear_constraint_matrix {
      column_ids: [ 3, 5, 8 ]
      row_ids: [ 4, 4, 4 ]
      coefficients: [ 1.0, 1.0, 1.0 ]
    }
  )pb");
}

TEST(ModelValidatorTest, SimpleQuadraticObjectiveModelValid) {
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary,
                       ValidateModel(SimpleQuadraticObjectiveModel()));
  EXPECT_THAT(
      summary.variables.id_to_name(),
      UnorderedElementsAre(Pair(3, "x3"), Pair(5, "x5"), Pair(8, "x8")));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(4, "c4")));
}

// /////////////////////////////////////////////////////////////////////////////
// Variables submessage
// /////////////////////////////////////////////////////////////////////////////

TEST(ModelValidatorTest, ExtraVariableUpperBound) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->add_upper_bounds(1.0);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("upper_bounds size"),
                             HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelValidatorTest, MissingVariableInteger) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->clear_integers();
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("integers size"),
                             HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelValidatorTest, ExtraVariableId) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->add_ids(12);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       (HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelValidatorTest, ExtraVariableName) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->add_names("z");
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("size"),
                             HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelValidatorTest, NegativeVarId) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->set_ids(0, -3);
  // the variable id in the constraint would be bad.
  model.mutable_linear_constraints()->Clear();
  model.mutable_linear_constraint_matrix()->Clear();
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("nonnegative")));
}

TEST(ModelValidatorTest, MaxInt64VarId) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->set_ids(0, kint64max);
  // the variable id in the constraint would be bad.
  model.mutable_linear_constraints()->Clear();
  model.mutable_linear_constraint_matrix()->Clear();
  EXPECT_THAT(ValidateModel(model), StatusIs(StatusCode::kInvalidArgument,
                                             HasSubstr("max(int64_t)")));
}

TEST(ModelValidatorTest, BadVariableLbInf) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->set_lower_bounds(0, kInf);
  model.mutable_variables()->set_upper_bounds(0, kInf);
  model.mutable_linear_constraints()->Clear();
  model.mutable_linear_constraint_matrix()->Clear();
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("positive infinite"), HasSubstr("lower_bounds"),
                     HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelValidatorTest, BadVariableUbMinusInf) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->set_lower_bounds(0, -kInf);
  model.mutable_variables()->set_upper_bounds(0, -kInf);
  model.mutable_linear_constraints()->Clear();
  model.mutable_linear_constraint_matrix()->Clear();
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("negative infinite"), HasSubstr("upper_bounds"),
                     HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelValidatorTest, BadVariableLbNan) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->set_lower_bounds(0, std::nan(""));
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"), HasSubstr("lower_bounds"),
                             HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelValidatorTest, BadVariableUbNan) {
  ModelProto model = SimpleModel();
  model.mutable_variables()->set_upper_bounds(0, std::nan(""));
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"), HasSubstr("upper_bounds"),
                             HasSubstr("ModelProto.variables are invalid"))));
}

TEST(ModelValidatorTest, RepeatVariableName) {
  const ModelProto model = ParseModel(R"pb(
    name: "simple_model"
    variables: {
      ids: [ 0, 1, 2 ]
      lower_bounds: [ 2.0, 3.0, 4.0 ]
      upper_bounds: [ 3.0, 4.0, 5.0 ]
      integers: [ false, false, false ]
      names: [ "x1", "x2", "x1" ]
    }
    objective: {
      linear_coefficients {
        ids: [ 0, 1, 2 ]
        values: [ 1.0, 1.0, 1.0 ]
      }
    }
  )pb");
  EXPECT_THAT(ValidateModel(model), StatusIs(StatusCode::kInvalidArgument,
                                             HasSubstr("duplicate name")));
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary,
                       ValidateModel(model, /*check_names=*/false));
  EXPECT_THAT(
      summary.variables.id_to_name(),
      UnorderedElementsAre(Pair(0, "x1"), Pair(1, "x2"), Pair(2, "x1")));
  EXPECT_THAT(summary.variables.nonempty_name_to_id(), Eq(std::nullopt));
}

TEST(ModelValidatorTest, VariableIdsDontIncrease) {
  const ModelProto model = ParseModel(R"pb(
    name: "simple_model"
    variables: {
      ids: [ 5, 8, 6 ]
      lower_bounds: [ 2.0, 3.0, 4.0 ]
      upper_bounds: [ 3.0, 4.0, 5.0 ]
      integers: [ false, false, false ]
    }
    objective: {
      linear_coefficients {
        ids: [ 5, 8, 6 ]
        values: [ 1.0, 1.0, 1.0 ]
      }
    }
  )pb");
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("increasing")));
}

// /////////////////////////////////////////////////////////////////////////////
// Objective submessage
// /////////////////////////////////////////////////////////////////////////////

TEST(ModelValidatorTest, JustOffsetValid) {
  ModelProto model;
  model.mutable_objective()->set_offset(12.0);
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary, ValidateModel(model));
  EXPECT_TRUE(summary.variables.Empty());
  EXPECT_TRUE(summary.linear_constraints.Empty());
}

TEST(ModelValidatorTest, NanOffsetInvalid) {
  ModelProto model;
  model.mutable_objective()->set_offset(std::nan(""));
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Expected no NaN"),
                             HasSubstr("Objective offset"))));
}

TEST(ModelValidatorTest, InfOffsetInvalid) {
  ModelProto model;
  model.mutable_objective()->set_offset(kInf);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("inf"), HasSubstr("Objective offset"))));
}

TEST(ModelValidatorTest, LinearObjectiveRepeatTerm) {
  ModelProto model = SimpleModel();
  model.mutable_objective()->mutable_linear_coefficients()->add_ids(0);
  model.mutable_objective()->mutable_linear_coefficients()->add_values(1.0);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("strictly increasing"),
                             HasSubstr("Linear objective coefficients bad"))));
}

TEST(ModelValidatorTest, LinearObjectiveMismatched) {
  ModelProto model = SimpleModel();
  model.mutable_objective()->mutable_linear_coefficients()->add_values(1.0);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Linear objective coefficients bad")));
}

TEST(ModelValidatorTest, LinearObjectiveBadId) {
  ModelProto model = SimpleModel();
  model.mutable_objective()->mutable_linear_coefficients()->set_ids(0, 0);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Objective.linear_coefficients.ids "
                                 "not found in Variables.ids")));
}

TEST(ModelValidatorTest, BadLinearObjectiveCoefficientNan) {
  ModelProto model = SimpleModel();
  model.mutable_objective()->mutable_linear_coefficients()->set_values(
      0, std::nan(""));
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"),
                             HasSubstr("Linear objective coefficients bad"))));
}

TEST(ModelValidatorTest, BadLinearObjectiveCoefficientInf) {
  ModelProto model = SimpleModel();
  model.mutable_objective()->mutable_linear_coefficients()->set_values(0,
                                                                       -kInf);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("negative infinite"),
                             HasSubstr("Linear objective coefficients bad"))));
}

TEST(ModelValidatorTest, QuadraticObjectiveIdsNotKnown) {
  ModelProto model = SimpleQuadraticObjectiveModel();
  model.mutable_objective()->mutable_quadratic_coefficients()->set_row_ids(1,
                                                                           4);
  EXPECT_THAT(ValidateModel(model), StatusIs(StatusCode::kInvalidArgument,
                                             HasSubstr("Unknown row_id")));
}

TEST(ModelValidatorTest, QuadraticObjectiveNotValid) {
  ModelProto model = SimpleQuadraticObjectiveModel();
  model.mutable_objective()->mutable_quadratic_coefficients()->set_row_ids(1,
                                                                           5);
  model.mutable_objective()->mutable_quadratic_coefficients()->set_column_ids(
      1, 3);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("lower triangular entry"))));
}

TEST(ModelValidatorTest, BadObjectivePriority) {
  ModelProto model = SimpleModel();
  model.mutable_objective()->set_priority(-1);
  EXPECT_THAT(ValidateModel(model), StatusIs(StatusCode::kInvalidArgument,
                                             AllOf(HasSubstr("nonnegative"),
                                                   HasSubstr("priority: -1"))));
}

// /////////////////////////////////////////////////////////////////////////////
// Auxiliary objectives submessage
// /////////////////////////////////////////////////////////////////////////////

TEST(ModelValidatorTest, AuxiliaryObjectiveJustOffsetValid) {
  ModelProto model;
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective.set_offset(12.0);
  }
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary, ValidateModel(model));
  EXPECT_TRUE(summary.variables.Empty());
  EXPECT_TRUE(summary.linear_constraints.Empty());
}

TEST(ModelValidatorTest, AuxiliaryObjectiveNanOffsetInvalid) {
  ModelProto model;
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective.set_offset(std::nan(""));
  }
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("Expected no NaN"),
                             HasSubstr("Objective offset"))));
}

TEST(ModelValidatorTest, AuxiliaryObjectiveInfOffsetInvalid) {
  ModelProto model;
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective.set_offset(kInf);
  }
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("inf"), HasSubstr("Objective offset"))));
}

TEST(ModelValidatorTest, AuxiliaryObjectiveLinearRepeatTerm) {
  ModelProto model = SimpleModel();
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective.mutable_linear_coefficients()->add_ids(0);
    objective.mutable_linear_coefficients()->add_ids(0);
    objective.mutable_linear_coefficients()->add_values(1.0);
    objective.mutable_linear_coefficients()->add_values(2.0);
  }
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("strictly increasing"),
                             HasSubstr("Linear objective coefficients bad"))));
}

TEST(ModelValidatorTest, AuxiliaryObjectiveLinearMismatch) {
  ModelProto model = SimpleModel();
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective.mutable_linear_coefficients()->add_values(1.0);
  }
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Linear objective coefficients bad")));
}

TEST(ModelValidatorTest, AuxiliaryObjectiveBadLinearId) {
  ModelProto model = SimpleModel();
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective = model.objective();
    objective.mutable_linear_coefficients()->set_ids(0, 0);
  }
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("Objective.linear_coefficients.ids "
                                 "not found in Variables.ids")));
}

TEST(ModelValidatorTest, AuxiliaryObjectiveBadLinearCoefficientNan) {
  ModelProto model = SimpleModel();
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective = model.objective();
    objective.mutable_linear_coefficients()->set_values(0, std::nan(""));
  }
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("NaN"),
                             HasSubstr("Linear objective coefficients bad"))));
}

TEST(ModelValidatorTest, AuxiliaryObjectiveBadLinearCoefficientInf) {
  ModelProto model = SimpleModel();
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective = model.objective();
    objective.mutable_linear_coefficients()->set_values(0, -kInf);
  }
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("negative infinite"),
                             HasSubstr("Linear objective coefficients bad"))));
}

TEST(ModelValidatorTest, AuxiliaryObjectiveQuadraticIdsNotKnown) {
  ModelProto model = SimpleQuadraticObjectiveModel();
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective = model.objective();
    objective.mutable_quadratic_coefficients()->set_row_ids(1, 4);
  }
  EXPECT_THAT(ValidateModel(model), StatusIs(StatusCode::kInvalidArgument,
                                             HasSubstr("Unknown row_id")));
}

TEST(ModelValidatorTest, AuxiliaryObjectiveQuadraticObjectiveNotValid) {
  ModelProto model = SimpleQuadraticObjectiveModel();
  {
    ObjectiveProto& objective = (*model.mutable_auxiliary_objectives())[0];
    objective = model.objective();
    objective.mutable_quadratic_coefficients()->set_row_ids(1, 5);
    objective.mutable_quadratic_coefficients()->set_column_ids(1, 3);
  }
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("lower triangular entry"))));
}

TEST(ModelValidatorTest, NegativeAuxiliaryObjectiveId) {
  ModelProto model = SimpleModel();
  model.mutable_auxiliary_objectives()->insert({-1, ObjectiveProto()});
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument, HasSubstr("-1")));
}

TEST(ModelValidatorTest, MaxInt64AuxiliaryObjectiveId) {
  ModelProto model = SimpleModel();
  model.mutable_auxiliary_objectives()->insert({kint64max, ObjectiveProto()});
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelProto.auxiliary_objectives are invalid"),
                     HasSubstr("max"))));
}

TEST(ModelValidatorTest, RepeatedAuxiliaryObjectiveName) {
  ModelProto model;
  model.mutable_auxiliary_objectives()->insert({3, ObjectiveProto()});
  model.mutable_auxiliary_objectives()->insert({5, ObjectiveProto()});
  model.mutable_auxiliary_objectives()->at(3).set_name("obj");
  model.mutable_auxiliary_objectives()->at(5).set_name("obj");
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("objectives"), HasSubstr("duplicate"))));
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary,
                       ValidateModel(model, /*check_names=*/false));
  EXPECT_THAT(summary.auxiliary_objectives.id_to_name(),
              UnorderedElementsAre(Pair(3, "obj"), Pair(5, "obj")));
  EXPECT_THAT(summary.auxiliary_objectives.nonempty_name_to_id(),
              Eq(std::nullopt));
}

TEST(ModelValidatorTest, RepeatedNameBetweenPrimaryAndAuxiliaryObjective) {
  ModelProto model;
  model.mutable_auxiliary_objectives()->insert({3, ObjectiveProto()});
  model.mutable_auxiliary_objectives()->at(3).set_name("obj");
  model.mutable_objective()->set_name("obj");
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("duplicate objective name")));
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary,
                       ValidateModel(model, /*check_names=*/false));
  EXPECT_EQ(summary.primary_objective_name, "obj");
  EXPECT_THAT(summary.auxiliary_objectives.id_to_name(),
              UnorderedElementsAre(Pair(3, "obj")));
  EXPECT_THAT(summary.auxiliary_objectives.nonempty_name_to_id(),
              Eq(std::nullopt));
}

TEST(ModelValidatorTest, BadAuxiliaryObjectivePriority) {
  ModelProto model = SimpleModel();
  (*model.mutable_auxiliary_objectives())[2].set_priority(-1);
  EXPECT_THAT(ValidateModel(model), StatusIs(StatusCode::kInvalidArgument,
                                             AllOf(HasSubstr("nonnegative"),
                                                   HasSubstr("priority: -1"))));
}

// /////////////////////////////////////////////////////////////////////////////
// Linear constraints submessage (except matrix of coefficients)
// /////////////////////////////////////////////////////////////////////////////

TEST(ModelValidatorTest, ExtraLinearConstraintUpperBound) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->add_upper_bounds(1.0);
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("upper_bounds size"),
                     HasSubstr("ModelProto.linear_constraints are invalid"))));
}

TEST(ModelValidatorTest, ExtraLinearConstraintId) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->add_ids(12);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       HasSubstr("ModelProto.linear_constraints are invalid")));
}

TEST(ModelValidatorTest, ExtraLinearConstraintName) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->add_names("z");
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("names"),
                     HasSubstr("ModelProto.linear_constraints are invalid"))));
}

TEST(ModelValidatorTest, NegativeConstraintId) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->set_ids(0, -3);
  model.mutable_linear_constraint_matrix()->set_row_ids(0, -3);
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelProto.linear_constraints are invalid"),
                     HasSubstr("nonnegative"))));
}

TEST(ModelValidatorTest, MaxInt64ConstraintId) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->set_ids(0, kint64max);
  model.mutable_linear_constraint_matrix()->set_row_ids(0, kint64max);
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelProto.linear_constraints are invalid"),
                     HasSubstr("max"))));
}

ModelProto NoVarsMultipleConstraints() {
  return ParseModel(R"pb(
    name: "no_vars_many_constraints"
    linear_constraints: {
      ids: [ 3, 5, 6 ]
      lower_bounds: [ -2.0, 1.0, 0.0 ]
      upper_bounds: [ 1.0, 2.0, 3.0 ]
      names: [ "c", "d", "e" ]
    }
  )pb");
}

TEST(ModelValidatorTest, NoVarsOkay) {
  ModelProto model = NoVarsMultipleConstraints();
  EXPECT_OK(ValidateModel(model));
}

TEST(ModelValidatorTest, RepeatConstraintName) {
  ModelProto model = NoVarsMultipleConstraints();
  model.mutable_linear_constraints()->set_names(2, "c");
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("linear_constraints"), HasSubstr("duplicate"))));
  ASSERT_OK_AND_ASSIGN(const ModelSummary summary,
                       ValidateModel(model, /*check_names=*/false));
  EXPECT_THAT(summary.linear_constraints.id_to_name(),
              UnorderedElementsAre(Pair(3, "c"), Pair(5, "d"), Pair(6, "c")));
  EXPECT_THAT(summary.linear_constraints.nonempty_name_to_id(),
              Eq(std::nullopt));
}

TEST(ModelValidatorTest, LinearConstraintIdsDontIncrease) {
  ModelProto model = NoVarsMultipleConstraints();
  model.mutable_linear_constraints()->set_ids(1, 1);
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelProto.linear_constraints are invalid"),
                     HasSubstr("strictly increasing"))));
}

TEST(ModelValidatorTest, BadConstraintLbInf) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->set_lower_bounds(0, kInf);
  model.mutable_linear_constraints()->set_upper_bounds(0, kInf);
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelProto.linear_constraints are invalid"),
                     HasSubstr("positive infinite"))));
}

TEST(ModelValidatorTest, BadConstraintUbMinusInf) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->set_lower_bounds(0, -kInf);
  model.mutable_linear_constraints()->set_upper_bounds(0, -kInf);
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelProto.linear_constraints are invalid"),
                     HasSubstr("negative infinite"))));
}

TEST(ModelValidatorTest, BadConstraintLbNan) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->set_lower_bounds(0, std::nan(""));
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelProto.linear_constraints are invalid"),
                     HasSubstr("NaN"))));
}

TEST(ModelValidatorTest, BadConstraintUbNan) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraints()->set_upper_bounds(0, std::nan(""));
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ModelProto.linear_constraints are invalid"),
                     HasSubstr("NaN"))));
}

// /////////////////////////////////////////////////////////////////////////////
// Linear constraint matrix of coefficients
// /////////////////////////////////////////////////////////////////////////////

TEST(ModelValidatorTest, SparseMatrixNotValid) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraint_matrix()->add_column_ids(0);
  EXPECT_THAT(ValidateModel(model), StatusIs(StatusCode::kInvalidArgument,
                                             HasSubstr("column_ids.size")));
}

TEST(ModelValidatorTest, SparseMatrixIdsNotKnown) {
  ModelProto model = SimpleModel();
  model.mutable_linear_constraint_matrix()->set_column_ids(0, 40);
  EXPECT_THAT(ValidateModel(model),
              StatusIs(StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("40"),
                             HasSubstr("ModelProto.linear_constraint_matrix "
                                       "ids are inconsistent"))));
}

// /////////////////////////////////////////////////////////////////////////////
// Mapped constraints (e.g., logical constraints).
//
// The helper validator code is tested elsewhere, so here we just test its
// integration into the model validator.
// /////////////////////////////////////////////////////////////////////////////

TEST(ModelValidatorTest, TrivialQuadraticConstraintOk) {
  ModelProto model;
  model.mutable_quadratic_constraints()->insert(
      {1, QuadraticConstraintProto()});
  EXPECT_OK(ValidateModel(model));
}

TEST(ModelValidatorTest, BadQuadraticConstraint) {
  ModelProto model;
  model.mutable_quadratic_constraints()->insert(
      {-1, QuadraticConstraintProto()});
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr("ids should be nonnegative"),
                HasSubstr("ModelProto.quadratic_constraints are invalid"))));
}

TEST(ModelValidatorTest, TrivialSecondOrderConeConstraintOk) {
  ModelProto model;
  (*model.mutable_second_order_cone_constraints())[1] = {};
  EXPECT_OK(ValidateModel(model));
}

TEST(ModelValidatorTest, BadSecondOrderConeConstraint) {
  ModelProto model;
  (*model.mutable_second_order_cone_constraints())[-1] = {};
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr("ids should be nonnegative"),
                HasSubstr(
                    "ModelProto.second_order_cone_constraints are invalid"))));
}

TEST(ModelValidatorTest, TrivialSos1ConstraintOk) {
  ModelProto model;
  model.mutable_sos1_constraints()->insert({1, SosConstraintProto()});
  EXPECT_OK(ValidateModel(model));
}

TEST(ModelValidatorTest, BadSos1Constraint) {
  ModelProto model;
  model.mutable_sos1_constraints()->insert({-1, SosConstraintProto()});
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ids should be nonnegative"),
                     HasSubstr("ModelProto.sos1_constraints are invalid"))));
}

TEST(ModelValidatorTest, TrivialSos2ConstraintOk) {
  ModelProto model;
  model.mutable_sos2_constraints()->insert({1, SosConstraintProto()});
  EXPECT_OK(ValidateModel(model));
}

TEST(ModelValidatorTest, BadSos2Constraint) {
  ModelProto model;
  model.mutable_sos2_constraints()->insert({-1, SosConstraintProto()});
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(StatusCode::kInvalidArgument,
               AllOf(HasSubstr("ids should be nonnegative"),
                     HasSubstr("ModelProto.sos2_constraints are invalid"))));
}

TEST(ModelValidatorTest, TrivialIndicatorConstraintOk) {
  ModelProto model = SimpleModel();
  IndicatorConstraintProto& constraint =
      (*model.mutable_indicator_constraints())[1];
  constraint.set_indicator_id(3);
  constraint.set_lower_bound(-1.0);
  constraint.set_upper_bound(1.0);
  EXPECT_OK(ValidateModel(model));
}

TEST(ModelValidatorTest, BadIndicatorConstraint) {
  ModelProto model = SimpleModel();
  IndicatorConstraintProto& constraint =
      (*model.mutable_indicator_constraints())[-1];
  constraint.set_indicator_id(3);
  constraint.set_lower_bound(-1.0);
  constraint.set_upper_bound(1.0);
  EXPECT_THAT(
      ValidateModel(model),
      StatusIs(
          StatusCode::kInvalidArgument,
          AllOf(HasSubstr("ids should be nonnegative"),
                HasSubstr("ModelProto.indicator_constraints are invalid"))));
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
