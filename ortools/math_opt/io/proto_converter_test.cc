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

#include "ortools/math_opt/io/proto_converter.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_test_proto.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::absl::StatusCode;
using ::google::protobuf::contrib::parse_proto::ParseTestProto;
using ::testing::Eq;
using ::testing::EqualsProto;
using ::testing::HasSubstr;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

// Returns the result of calling MPModelProtoToMathOptModel() with an empty
// input model. Used as a starting point for expected values.
ModelProto EmptyModelProto() {
  ModelProto empty;
  empty.mutable_variables();
  empty.mutable_objective();
  empty.mutable_linear_constraints();
  empty.mutable_linear_constraint_matrix();
  return empty;
}

// Returns the result of calling MathOptModelToMPModelProto() with an empty
// input model. Used as a starting point for expected values.
MPModelProto EmptyMPModelProto() {
  MPModelProto empty;
  empty.set_maximize(false);
  empty.set_objective_offset(0.0);
  empty.set_name("");
  return empty;
}

TEST(MinimalTest, EmptyMPModel) {
  ASSERT_OK_AND_ASSIGN(auto model, MPModelProtoToMathOptModel(MPModelProto()));
  EXPECT_THAT(model, EqualsProto(EmptyModelProto()));
}

TEST(MinimalTest, EmptyMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model, MathOptModelToMPModelProto(ModelProto()));
  EXPECT_THAT(model, EqualsProto(EmptyMPModelProto()));
}

TEST(MinimalTest, NamedMPModel) {
  ASSERT_OK_AND_ASSIGN(auto model, MPModelProtoToMathOptModel(MPModelProto(
                                       ParseTestProto(R"pb(name: 'name')pb"))));
  ModelProto expected = EmptyModelProto();
  expected.set_name("name");
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, NamedMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model, MathOptModelToMPModelProto(ModelProto(
                                       ParseTestProto(R"pb(name: 'name')pb"))));
  MPModelProto expected = EmptyMPModelProto();
  expected.set_name("name");
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, BoundedVarMPModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model,
      MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
          R"pb(variable { lower_bound: 0.0 upper_bound: 1.0 })pb"))));
  ModelProto expected = EmptyModelProto();
  expected.mutable_variables()->add_ids(0);
  expected.mutable_variables()->add_lower_bounds(0);
  expected.mutable_variables()->add_upper_bounds(1);
  expected.mutable_variables()->add_integers(false);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, BoundedVarMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model, MathOptModelToMPModelProto(ModelProto(
                                       ParseTestProto(R"pb(variables {
                                                             ids: 0
                                                             lower_bounds: 0.0
                                                             upper_bounds: 1.0
                                                             integers: false
                                                           })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  MPVariableProto& var = *expected.add_variable();
  var.set_lower_bound(0.0);
  var.set_upper_bound(1.0);
  var.set_is_integer(false);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, OffsetMPModel) {
  ASSERT_OK_AND_ASSIGN(auto model,
                       MPModelProtoToMathOptModel(MPModelProto(
                           ParseTestProto(R"pb(objective_offset: 1)pb"))));
  ModelProto expected = EmptyModelProto();
  expected.mutable_objective()->set_offset(1.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, OffsetMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model,
                       MathOptModelToMPModelProto(ModelProto(
                           ParseTestProto(R"pb(objective { offset: 1 })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  expected.set_objective_offset(1.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, IntegerVariableMPModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model, MPModelProtoToMathOptModel(MPModelProto(
                      ParseTestProto(R"pb(variable { is_integer: true })pb"))));
  ModelProto expected = EmptyModelProto();
  expected.mutable_variables()->add_ids(0);
  expected.mutable_variables()->add_lower_bounds(-kInf);
  expected.mutable_variables()->add_upper_bounds(kInf);
  expected.mutable_variables()->add_integers(true);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, IntegerVariableMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model, MathOptModelToMPModelProto(ModelProto(
                                       ParseTestProto(R"pb(variables {
                                                             ids: 0
                                                             lower_bounds: -inf
                                                             upper_bounds: inf
                                                             integers: true
                                                           })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  MPVariableProto& var = *expected.add_variable();
  var.set_lower_bound(-kInf);
  var.set_upper_bound(kInf);
  var.set_is_integer(true);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, MaximizeMPModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model, MPModelProtoToMathOptModel(
                      MPModelProto(ParseTestProto(R"pb(maximize: true)pb"))));
  ModelProto expected = EmptyModelProto();
  expected.mutable_objective()->set_maximize(true);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, MaximizeMathOptModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model, MathOptModelToMPModelProto(ModelProto(
                      ParseTestProto(R"pb(objective { maximize: true })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  expected.set_maximize(true);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, ZeroCoefficientInConstraint) {
  ASSERT_OK_AND_ASSIGN(
      auto model,
      MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
          R"pb(variable {}
               constraint { var_index: 0 coefficient: 0.0 })pb"))));
  ModelProto expected = EmptyModelProto();
  expected.mutable_variables()->add_ids(0);
  expected.mutable_variables()->add_lower_bounds(-kInf);
  expected.mutable_variables()->add_upper_bounds(kInf);
  expected.mutable_variables()->add_integers(false);
  expected.mutable_linear_constraints()->add_ids(0);
  expected.mutable_linear_constraints()->add_lower_bounds(-kInf);
  expected.mutable_linear_constraints()->add_upper_bounds(kInf);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, ObjectiveMPModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model, MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
                      R"pb(variable {}
                           variable { objective_coefficient: 1.0 })pb"))));
  ModelProto expected = EmptyModelProto();
  for (int64_t v = 0; v < 2; ++v) {
    expected.mutable_variables()->add_ids(v);
    expected.mutable_variables()->add_lower_bounds(-kInf);
    expected.mutable_variables()->add_upper_bounds(kInf);
    expected.mutable_variables()->add_integers(false);
  }
  SparseDoubleVectorProto& obj_coeffs =
      *expected.mutable_objective()->mutable_linear_coefficients();
  obj_coeffs.add_ids(1);
  obj_coeffs.add_values(1.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, ObjectiveMathOptModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model,
      MathOptModelToMPModelProto(ModelProto(ParseTestProto(
          R"pb(objective { linear_coefficients { ids: 1 values: 1.0 } }
               variables {
                 ids: [ 0, 1 ]
                 lower_bounds: [ -inf, -inf ]
                 upper_bounds: [ inf, inf ]
                 integers: [ false, false ]
               })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  for (int i = 0; i < 2; ++i) {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(-kInf);
    var.set_upper_bound(kInf);
    var.set_is_integer(false);
  }
  expected.mutable_variable(1)->set_objective_coefficient(1.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, QuadraticObjectiveMPModel) {
  ASSERT_OK_AND_ASSIGN(auto model,
                       MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
                           R"pb(variable {}
                                variable {}
                                quadratic_objective {
                                  qvar1_index: [ 0, 1, 0, 0 ]
                                  qvar2_index: [ 0, 0, 1, 0 ]
                                  coefficient: [ 1.0, 2.0, 4.0, 8.0 ]
                                }
                           )pb"))));
  ModelProto expected = EmptyModelProto();
  for (int64_t v = 0; v < 2; ++v) {
    expected.mutable_variables()->add_ids(v);
    expected.mutable_variables()->add_lower_bounds(-kInf);
    expected.mutable_variables()->add_upper_bounds(kInf);
    expected.mutable_variables()->add_integers(false);
  }
  SparseDoubleMatrixProto& obj_quad_coeffs =
      *expected.mutable_objective()->mutable_quadratic_coefficients();
  obj_quad_coeffs.add_row_ids(0);
  obj_quad_coeffs.add_column_ids(0);
  obj_quad_coeffs.add_coefficients(9.0);
  obj_quad_coeffs.add_row_ids(0);
  obj_quad_coeffs.add_column_ids(1);
  obj_quad_coeffs.add_coefficients(6.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, QuadraticObjectiveMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model,
                       MathOptModelToMPModelProto(ModelProto(ParseTestProto(
                           R"pb(objective {
                                  quadratic_coefficients {
                                    row_ids: [ 0, 0 ]
                                    column_ids: [ 0, 1 ]
                                    coefficients: [ 9.0, 6.0 ]
                                  }
                                }
                                variables {
                                  ids: [ 0, 1 ]
                                  lower_bounds: [ -inf, -inf ]
                                  upper_bounds: [ inf, inf ]
                                  integers: [ false, false ]
                                })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  for (int i = 0; i < 2; ++i) {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(-kInf);
    var.set_upper_bound(kInf);
    var.set_is_integer(false);
  }
  MPQuadraticObjective& quad_obj = *expected.mutable_quadratic_objective();
  quad_obj.add_qvar1_index(0);
  quad_obj.add_qvar2_index(0);
  quad_obj.add_coefficient(9.0);
  quad_obj.add_qvar1_index(0);
  quad_obj.add_qvar2_index(1);
  quad_obj.add_coefficient(6.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, OneConstraintMPModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model, MPModelProtoToMathOptModel(
                      MPModelProto(ParseTestProto(R"pb(variable { name: 'c1' }
                                                       constraint {
                                                         lower_bound: 0.0
                                                         upper_bound: 1.0
                                                         var_index: 0
                                                         coefficient: 1.0
                                                         name: 'r1'
                                                       })pb"))));
  ModelProto expected = EmptyModelProto();
  expected.mutable_variables()->add_ids(0);
  expected.mutable_variables()->add_lower_bounds(-kInf);
  expected.mutable_variables()->add_upper_bounds(kInf);
  expected.mutable_variables()->add_integers(false);
  expected.mutable_variables()->add_names("c1");
  expected.mutable_linear_constraints()->add_ids(0);
  expected.mutable_linear_constraints()->add_lower_bounds(0.0);
  expected.mutable_linear_constraints()->add_upper_bounds(1.0);
  expected.mutable_linear_constraints()->add_names("r1");
  expected.mutable_linear_constraint_matrix()->add_row_ids(0);
  expected.mutable_linear_constraint_matrix()->add_column_ids(0);
  expected.mutable_linear_constraint_matrix()->add_coefficients(1.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, OneConstraintMathOptModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model, MathOptModelToMPModelProto(
                      ModelProto(ParseTestProto(R"pb(linear_constraints {
                                                       ids: 0
                                                       lower_bounds: 0.0
                                                       upper_bounds: 1.0
                                                       names: 'r1'
                                                     }
                                                     linear_constraint_matrix {
                                                       column_ids: 0
                                                       row_ids: 0
                                                       coefficients: 1.0
                                                     }
                                                     variables {
                                                       ids: 0
                                                       names: 'c1'
                                                       lower_bounds: -inf
                                                       upper_bounds: inf
                                                       integers: false
                                                     })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  MPVariableProto& var = *expected.add_variable();
  var.set_lower_bound(-kInf);
  var.set_upper_bound(kInf);
  var.set_is_integer(false);
  var.set_name("c1");
  MPConstraintProto& cstr = *expected.add_constraint();
  cstr.set_lower_bound(0.0);
  cstr.set_upper_bound(1.0);
  cstr.set_name("r1");
  cstr.add_var_index(0);
  cstr.add_coefficient(1.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, AsymmetricConstraintMatrixMPModel) {
  ASSERT_OK_AND_ASSIGN(
      auto model, MPModelProtoToMathOptModel(
                      MPModelProto(ParseTestProto(R"pb(variable { name: 'c1' }
                                                       variable { name: 'c2' }
                                                       constraint {
                                                         lower_bound: 0.0
                                                         upper_bound: 1.0
                                                         var_index: 0
                                                         coefficient: 3.0
                                                         var_index: 1
                                                         coefficient: 4.0
                                                         name: 'r1'
                                                       })pb"))));
  ModelProto expected = EmptyModelProto();
  for (int64_t v = 0; v < 2; ++v) {
    expected.mutable_variables()->add_ids(v);
    expected.mutable_variables()->add_lower_bounds(-kInf);
    expected.mutable_variables()->add_upper_bounds(kInf);
    expected.mutable_variables()->add_integers(false);
    expected.mutable_variables()->add_names(absl::StrCat("c", v + 1));
  }
  expected.mutable_linear_constraints()->add_ids(0);
  expected.mutable_linear_constraints()->add_lower_bounds(0.0);
  expected.mutable_linear_constraints()->add_upper_bounds(1.0);
  expected.mutable_linear_constraints()->add_names("r1");
  expected.mutable_linear_constraint_matrix()->add_row_ids(0);
  expected.mutable_linear_constraint_matrix()->add_column_ids(0);
  expected.mutable_linear_constraint_matrix()->add_coefficients(3.0);
  expected.mutable_linear_constraint_matrix()->add_row_ids(0);
  expected.mutable_linear_constraint_matrix()->add_column_ids(1);
  expected.mutable_linear_constraint_matrix()->add_coefficients(4.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, AsymmetricConstraintMatrixMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model,
                       MathOptModelToMPModelProto(ModelProto(
                           ParseTestProto(R"pb(linear_constraints {
                                                 ids: 0
                                                 lower_bounds: 0.0
                                                 upper_bounds: 1.0
                                                 names: 'r1'
                                               }
                                               linear_constraint_matrix {
                                                 column_ids: [ 0, 1 ]
                                                 row_ids: [ 0, 0 ]
                                                 coefficients: [ 3.0, 4.0 ]
                                               }
                                               variables {
                                                 ids: [ 0, 1 ]
                                                 names: [ 'c1', 'c2' ]
                                                 lower_bounds: [ -inf, -inf ]
                                                 upper_bounds: [ inf, inf ]
                                                 integers: [ false, false ]
                                               })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  for (int i = 1; i < 3; ++i) {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(-kInf);
    var.set_upper_bound(kInf);
    var.set_is_integer(false);
    var.set_name(absl::StrCat("c", i));
  }
  MPConstraintProto& cstr = *expected.add_constraint();
  cstr.set_lower_bound(0.0);
  cstr.set_upper_bound(1.0);
  cstr.set_name("r1");
  cstr.add_var_index(0);
  cstr.add_coefficient(3.0);
  cstr.add_var_index(1);
  cstr.add_coefficient(4.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, VarIdMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model, MathOptModelToMPModelProto(ModelProto(
                                       ParseTestProto(R"pb(variables {
                                                             ids: 1024
                                                             lower_bounds: 0.0
                                                             upper_bounds: inf
                                                             integers: false
                                                           })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  MPVariableProto& var = *expected.add_variable();
  var.set_lower_bound(0.0);
  var.set_upper_bound(kInf);
  var.set_is_integer(false);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, ConstraintIdMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model,
                       MathOptModelToMPModelProto(ModelProto(ParseTestProto(
                           R"pb(variables {
                                  ids: 512
                                  ids: 1024
                                  lower_bounds: [ -inf, -inf ]
                                  upper_bounds: [ inf, inf ]
                                  integers: [ false, true ]
                                }
                                linear_constraints {
                                  ids: 128
                                  ids: 256
                                  lower_bounds: [ 0.0, 1.0 ]
                                  upper_bounds: [ 1.0, 2.0 ]
                                }
                                linear_constraint_matrix {
                                  column_ids: [ 512, 1024 ]
                                  row_ids: [ 128, 256 ]
                                  coefficients: [ 1.0, 2.0 ]
                                })pb"))));
  MPModelProto expected = EmptyMPModelProto();
  {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(-kInf);
    var.set_upper_bound(kInf);
    var.set_is_integer(false);
  }
  {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(-kInf);
    var.set_upper_bound(kInf);
    var.set_is_integer(true);
  }
  {
    MPConstraintProto& cstr = *expected.add_constraint();
    cstr.set_lower_bound(0.0);
    cstr.set_upper_bound(1.0);
    cstr.add_var_index(0);
    cstr.add_coefficient(1.0);
  }
  {
    MPConstraintProto& cstr = *expected.add_constraint();
    cstr.set_lower_bound(1.0);
    cstr.set_upper_bound(2.0);
    cstr.add_var_index(1);
    cstr.add_coefficient(2.0);
  }
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, HintMPModel) {
  const MPModelProto mp_model = ParseTestProto(
      R"pb(variable {}
           variable {}
           solution_hint { var_index: 1 var_value: 3.5 })pb");

  {
    ModelProto expected_model = EmptyModelProto();
    for (int64_t v = 0; v < 2; ++v) {
      expected_model.mutable_variables()->add_ids(v);
      expected_model.mutable_variables()->add_lower_bounds(-kInf);
      expected_model.mutable_variables()->add_upper_bounds(kInf);
      expected_model.mutable_variables()->add_integers(false);
    }
    EXPECT_THAT(MPModelProtoToMathOptModel(mp_model),
                IsOkAndHolds(EqualsProto(expected_model)));
  }
  {
    SolutionHintProto expected_hint;
    expected_hint.mutable_variable_values()->add_ids(1);
    expected_hint.mutable_variable_values()->add_values(3.5);
    EXPECT_THAT(MPModelProtoSolutionHintToMathOptHint(mp_model),
                IsOkAndHolds(Optional(EqualsProto(expected_hint))));
  }
}

TEST(MinimalTest, EmptyHintMPModel) {
  const MPModelProto mp_model = ParseTestProto(
      R"pb(variable {}
           variable {}
           solution_hint {})pb");

  {
    ModelProto expected_model = EmptyModelProto();
    for (int64_t v = 0; v < 2; ++v) {
      expected_model.mutable_variables()->add_ids(v);
      expected_model.mutable_variables()->add_lower_bounds(-kInf);
      expected_model.mutable_variables()->add_upper_bounds(kInf);
      expected_model.mutable_variables()->add_integers(false);
    }
    EXPECT_THAT(MPModelProtoToMathOptModel(mp_model),
                IsOkAndHolds(EqualsProto(expected_model)));
  }
  EXPECT_THAT(MPModelProtoSolutionHintToMathOptHint(mp_model),
              IsOkAndHolds(Eq(std::nullopt)));
}

TEST(MinimalTest, NoHintMPModel) {
  const MPModelProto mp_model = ParseTestProto(R"pb(variable {}
                                                    variable {})pb");

  EXPECT_THAT(MPModelProtoSolutionHintToMathOptHint(mp_model),
              IsOkAndHolds(Eq(std::nullopt)));
}

TEST(MinimalTest, FailWithAbsConstraint) {
  absl::StatusOr<ModelProto> status_or =
      MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
          R"pb(variable {}
               general_constraint {
                 abs_constraint { var_index: 0 resultant_var_index: 0 }
               })pb")));
  EXPECT_THAT(status_or, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Unsupported general constraint")));
}

TEST(MinimalTest, FailWithAndConstraint) {
  absl::StatusOr<ModelProto> status_or =
      MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
          R"pb(variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               general_constraint {
                 and_constraint {
                   var_index: 0
                   var_index: 1
                   resultant_var_index: 2
                 }
               })pb")));
  EXPECT_THAT(status_or, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Unsupported general constraint")));
}

TEST(MinimalTest, FailWithMaxConstraint) {
  absl::StatusOr<ModelProto> status_or =
      MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
          R"pb(variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               general_constraint {
                 max_constraint {
                   var_index: 0
                   var_index: 1
                   resultant_var_index: 2
                 }
               })pb")));
  EXPECT_THAT(status_or, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Unsupported general constraint")));
}

TEST(MinimalTest, FailWithMinConstraint) {
  absl::StatusOr<ModelProto> status_or =
      MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
          R"pb(variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               general_constraint {
                 min_constraint {
                   var_index: 0
                   var_index: 1
                   resultant_var_index: 2
                 }
               })pb")));
  EXPECT_THAT(status_or, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Unsupported general constraint")));
}

TEST(MinimalTest, FailWithOrConstraint) {
  absl::StatusOr<ModelProto> status_or =
      MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
          R"pb(variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               variable { is_integer: true lower_bound: 0.0 upper_bound: 1.0 }
               general_constraint {
                 or_constraint {
                   var_index: 0
                   var_index: 1
                   resultant_var_index: 2
                 }
               })pb")));
  EXPECT_THAT(status_or, StatusIs(StatusCode::kInvalidArgument,
                                  HasSubstr("Unsupported general constraint")));
}

TEST(MinimalTest, InvalidMPModelProto) {
  const MPModelProto model =
      ParseTestProto(R"pb(constraint { var_index: 0 coefficient: 1.0 })pb");
  EXPECT_THAT(MPModelProtoToMathOptModel(model),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(MPModelProtoSolutionHintToMathOptHint(model),
              StatusIs(StatusCode::kInvalidArgument));
}

TEST(MinimalTest, InvalidMPModelProtoHint) {
  const MPModelProto model = ParseTestProto(
      R"pb(variable {}
           variable {}
           solution_hint {
             var_index: [ 1, 0 ]
             var_value: 3.5
           })pb");
  EXPECT_THAT(MPModelProtoToMathOptModel(model),
              StatusIs(StatusCode::kInvalidArgument));
  EXPECT_THAT(MPModelProtoSolutionHintToMathOptHint(model),
              StatusIs(StatusCode::kInvalidArgument));
}

TEST(MinimalTest, InvalidMathOptModel) {
  absl::StatusOr<MPModelProto> status_or = MathOptModelToMPModelProto(
      ModelProto(ParseTestProto(R"pb(variables { ids: -1 })pb")));
  EXPECT_THAT(status_or, StatusIs(StatusCode::kInvalidArgument));
}

TEST(MinimalTest, QuadraticConstraintMPModel) {
  ASSERT_OK_AND_ASSIGN(const auto model,
                       MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
                           R"pb(variable {}
                                variable {}
                                general_constraint {
                                  name: "qc"
                                  quadratic_constraint {
                                    var_index: [ 1, 0 ]
                                    coefficient: [ 1.0, 2.0 ]
                                    qvar1_index: [ 0, 0, 1, 1 ]
                                    qvar2_index: [ 0, 1, 1, 0 ]
                                    qcoefficient: [ 3.0, 4.0, 0.0, 6.0 ]
                                    lower_bound: 7.0
                                    upper_bound: 8.0
                                  }
                                }
                           )pb"))));
  ModelProto expected = EmptyModelProto();
  for (int64_t v = 0; v < 2; ++v) {
    expected.mutable_variables()->add_ids(v);
    expected.mutable_variables()->add_lower_bounds(-kInf);
    expected.mutable_variables()->add_upper_bounds(kInf);
    expected.mutable_variables()->add_integers(false);
  }
  QuadraticConstraintProto& constraint =
      (*expected.mutable_quadratic_constraints())[0];
  constraint.set_name("qc");
  constraint.set_lower_bound(7.0);
  constraint.set_upper_bound(8.0);
  constraint.mutable_linear_terms()->add_ids(0);
  constraint.mutable_linear_terms()->add_values(2.0);
  constraint.mutable_linear_terms()->add_ids(1);
  constraint.mutable_linear_terms()->add_values(1.0);
  constraint.mutable_quadratic_terms()->add_row_ids(0);
  constraint.mutable_quadratic_terms()->add_column_ids(0);
  constraint.mutable_quadratic_terms()->add_coefficients(3.0);
  constraint.mutable_quadratic_terms()->add_row_ids(0);
  constraint.mutable_quadratic_terms()->add_column_ids(1);
  constraint.mutable_quadratic_terms()->add_coefficients(10.0);
  constraint.mutable_quadratic_terms()->add_row_ids(1);
  constraint.mutable_quadratic_terms()->add_column_ids(1);
  constraint.mutable_quadratic_terms()->add_coefficients(0.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, SecondOrderConeMathOptModel) {
  ModelProto model;
  (*model.mutable_second_order_cone_constraints())[0] = {};
  EXPECT_THAT(
      MathOptModelToMPModelProto(model),
      StatusIs(StatusCode::kInvalidArgument, HasSubstr("second-order cone")));
}

TEST(MinimalTest, QuadraticConstraintMathOptModel) {
  ASSERT_OK_AND_ASSIGN(auto model,
                       MathOptModelToMPModelProto(ModelProto(ParseTestProto(
                           R"pb(variables {
                                  ids: [ 0, 1 ]
                                  lower_bounds: [ -inf, -inf ]
                                  upper_bounds: [ inf, inf ]
                                  integers: [ false, false ]
                                }
                                quadratic_constraints {
                                  key: 0
                                  value {
                                    lower_bound: 7.0
                                    upper_bound: 8.0
                                    linear_terms {
                                      ids: [ 0, 1 ]
                                      values: [ 2.0, 1.0 ]
                                    }
                                    quadratic_terms {
                                      row_ids: [ 0, 0, 1 ]
                                      column_ids: [ 0, 1, 1 ]
                                      coefficients: [ 3.0, 10.0, 0.0 ]
                                    }
                                    name: "qc"
                                  }
                                }
                           )pb"))));
  MPModelProto expected = EmptyMPModelProto();
  for (int i = 0; i < 2; ++i) {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(-kInf);
    var.set_upper_bound(kInf);
    var.set_is_integer(false);
  }
  MPGeneralConstraintProto& general_constraint =
      *expected.add_general_constraint();
  general_constraint.set_name("qc");
  MPQuadraticConstraint& constraint =
      *general_constraint.mutable_quadratic_constraint();
  constraint.set_lower_bound(7.0);
  constraint.set_upper_bound(8.0);
  constraint.add_var_index(0);
  constraint.add_coefficient(2.0);
  constraint.add_var_index(1);
  constraint.add_coefficient(1.0);
  constraint.add_qvar1_index(0);
  constraint.add_qvar2_index(0);
  constraint.add_qcoefficient(3.0);
  constraint.add_qvar1_index(0);
  constraint.add_qvar2_index(1);
  constraint.add_qcoefficient(10.0);
  constraint.add_qvar1_index(1);
  constraint.add_qvar2_index(1);
  constraint.add_qcoefficient(0.0);
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, SosConstraintMPModel) {
  ASSERT_OK_AND_ASSIGN(const auto model,
                       MPModelProtoToMathOptModel(MPModelProto(ParseTestProto(
                           R"pb(variable {}
                                variable {}
                                general_constraint {
                                  name: "sos1"
                                  sos_constraint { var_index: [ 0, 1 ] }
                                }
                                general_constraint {
                                  name: "sos2"
                                  sos_constraint {
                                    type: SOS2
                                    var_index: [ 1, 0 ]
                                    weight: [ 2.0, 3.0 ]
                                  }
                                }
                           )pb"))));
  ModelProto expected = EmptyModelProto();
  for (int64_t v = 0; v < 2; ++v) {
    expected.mutable_variables()->add_ids(v);
    expected.mutable_variables()->add_lower_bounds(-kInf);
    expected.mutable_variables()->add_upper_bounds(kInf);
    expected.mutable_variables()->add_integers(false);
  }
  {
    SosConstraintProto& sos1_constraint =
        (*expected.mutable_sos1_constraints())[0];
    sos1_constraint.set_name("sos1");
    {
      LinearExpressionProto& expr = *sos1_constraint.add_expressions();
      expr.add_ids(0);
      expr.add_coefficients(1.0);
    }
    {
      LinearExpressionProto& expr = *sos1_constraint.add_expressions();
      expr.add_ids(1);
      expr.add_coefficients(1.0);
    }
  }
  {
    SosConstraintProto& sos2_constraint =
        (*expected.mutable_sos2_constraints())[0];
    sos2_constraint.set_name("sos2");
    {
      LinearExpressionProto& expr = *sos2_constraint.add_expressions();
      expr.add_ids(1);
      expr.add_coefficients(1.0);
    }
    {
      LinearExpressionProto& expr = *sos2_constraint.add_expressions();
      expr.add_ids(0);
      expr.add_coefficients(1.0);
    }
    sos2_constraint.add_weights(2.0);
    sos2_constraint.add_weights(3.0);
  }
  EXPECT_THAT(model, EqualsProto(expected));
}

TEST(MinimalTest, SosConstraintMathOptModel) {
  const ModelProto starting_model = ParseTestProto(
      R"pb(variables {
             ids: [ 0, 1 ]
             lower_bounds: [ -inf, -inf ]
             upper_bounds: [ inf, inf ]
             integers: [ false, false ]
           }
           sos1_constraints {
             key: 0
             value {
               expressions {
                 ids: [ 0 ]
                 coefficients: [ 1.0 ]
               }
               expressions {
                 ids: [ 1 ]
                 coefficients: [ 1.0 ]
               }
               name: "sos1"
             }
           }
           sos2_constraints {
             key: 0
             value {
               expressions {
                 ids: [ 0 ]
                 coefficients: [ 1.0 ]
               }
               expressions {
                 ids: [ 1 ]
                 coefficients: [ 1.0 ]
               }
               weights: [ 2.0, 3.0 ]
               name: "sos2"
             }
           }
      )pb");
  ASSERT_OK_AND_ASSIGN(auto model, MathOptModelToMPModelProto(starting_model));
  MPModelProto expected = EmptyMPModelProto();
  for (int i = 0; i < 2; ++i) {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(-kInf);
    var.set_upper_bound(kInf);
    var.set_is_integer(false);
  }
  {
    MPGeneralConstraintProto& general_constraint =
        *expected.add_general_constraint();
    general_constraint.set_name("sos1");
    MPSosConstraint& constraint = *general_constraint.mutable_sos_constraint();
    constraint.set_type(MPSosConstraint::SOS1_DEFAULT);
    constraint.add_var_index(0);
    constraint.add_var_index(1);
  }
  {
    MPGeneralConstraintProto& general_constraint =
        *expected.add_general_constraint();
    general_constraint.set_name("sos2");
    MPSosConstraint& constraint = *general_constraint.mutable_sos_constraint();
    constraint.set_type(MPSosConstraint::SOS2);
    constraint.add_var_index(0);
    constraint.add_weight(2.0);
    constraint.add_var_index(1);
    constraint.add_weight(3.0);
  }
  EXPECT_THAT(model, EqualsProto(expected));

  // We reject MathOpt models with SOS expressions with more than one variable.
  {
    ModelProto bad_model = starting_model;
    (*bad_model.mutable_sos1_constraints())[0].mutable_expressions(0)->add_ids(
        1);
    (*bad_model.mutable_sos1_constraints())[0]
        .mutable_expressions(0)
        ->add_coefficients(1.0);
    EXPECT_THAT(MathOptModelToMPModelProto(bad_model),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr("SOS constraints")));
  }
  // We reject MathOpt models with SOS expression coefficients not equal to one.
  {
    ModelProto bad_model = starting_model;
    (*bad_model.mutable_sos1_constraints())[0]
        .mutable_expressions(0)
        ->set_coefficients(0, 2.0);
    EXPECT_THAT(MathOptModelToMPModelProto(bad_model),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr("SOS constraints")));
  }
  // We reject MathOpt models with nonzero SOS expression offsets.
  {
    ModelProto bad_model = starting_model;
    (*bad_model.mutable_sos1_constraints())[0]
        .mutable_expressions(0)
        ->set_offset(1.0);
    EXPECT_THAT(MathOptModelToMPModelProto(bad_model),
                StatusIs(absl::StatusCode::kInvalidArgument,
                         HasSubstr("SOS constraints")));
  }
}

TEST(MinimalTest, IndicatorConstraintMPModel) {
  const MPModelProto starting_model = ParseTestProto(
      R"pb(variable {}
           variable { lower_bound: 0.0 upper_bound: 1.0 is_integer: true }
           general_constraint {
             name: "indicator"
             indicator_constraint {
               var_index: 1
               var_value: 1
               constraint {
                 var_index: [ 1, 0 ]
                 coefficient: [ 1.0, 2.0 ]
                 lower_bound: -inf
                 upper_bound: 3.0
               }
             }
           }
      )pb");
  ASSERT_OK_AND_ASSIGN(const auto model,
                       MPModelProtoToMathOptModel(starting_model));

  ModelProto expected = EmptyModelProto();
  expected.mutable_variables()->add_ids(0);
  expected.mutable_variables()->add_lower_bounds(-kInf);
  expected.mutable_variables()->add_upper_bounds(kInf);
  expected.mutable_variables()->add_integers(false);
  expected.mutable_variables()->add_ids(1);
  expected.mutable_variables()->add_lower_bounds(0.0);
  expected.mutable_variables()->add_upper_bounds(1.0);
  expected.mutable_variables()->add_integers(true);
  IndicatorConstraintProto& constraint =
      (*expected.mutable_indicator_constraints())[0];
  constraint.set_name("indicator");
  constraint.set_indicator_id(1);
  constraint.set_activate_on_zero(false);
  constraint.set_lower_bound(-kInf);
  constraint.set_upper_bound(3.0);
  constraint.mutable_expression()->add_ids(0);
  constraint.mutable_expression()->add_values(2.0);
  constraint.mutable_expression()->add_ids(1);
  constraint.mutable_expression()->add_values(1.0);
  EXPECT_THAT(model, EqualsProto(expected));

  MPModelProto mp_activate_on_zero_model = starting_model;
  mp_activate_on_zero_model.mutable_general_constraint(0)
      ->mutable_indicator_constraint()
      ->set_var_value(0);
  ASSERT_OK_AND_ASSIGN(const auto mo_activate_on_zero_model,
                       MPModelProtoToMathOptModel(mp_activate_on_zero_model));
  ModelProto expected_activate_on_zero_model = expected;
  (*expected_activate_on_zero_model.mutable_indicator_constraints())[0]
      .set_activate_on_zero(true);
  EXPECT_THAT(mo_activate_on_zero_model,
              EqualsProto(expected_activate_on_zero_model));
}

TEST(MinimalTest, IndicatorConstraintMathOptModel) {
  const ModelProto starting_model = ParseTestProto(
      R"pb(variables {
             ids: [ 0, 1 ]
             lower_bounds: [ -inf, 0.0 ]
             upper_bounds: [ inf, 1.0 ]
             integers: [ false, true ]
           }
           indicator_constraints {
             key: 0
             value {
               indicator_id: 1
               activate_on_zero: false
               lower_bound: -inf
               upper_bound: 3.0
               expression {
                 ids: [ 0, 1 ]
                 values: [ 2.0, 1.0 ]
               }
               name: "indicator"
             }
           }
      )pb");
  ASSERT_OK_AND_ASSIGN(auto model, MathOptModelToMPModelProto(starting_model));
  MPModelProto expected = EmptyMPModelProto();
  {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(-kInf);
    var.set_upper_bound(kInf);
    var.set_is_integer(false);
  }
  {
    MPVariableProto& var = *expected.add_variable();
    var.set_lower_bound(0.0);
    var.set_upper_bound(1.0);
    var.set_is_integer(true);
  }
  MPGeneralConstraintProto& general_constraint =
      *expected.add_general_constraint();
  general_constraint.set_name("indicator");
  MPIndicatorConstraint& constraint =
      *general_constraint.mutable_indicator_constraint();
  constraint.set_var_index(1);
  constraint.set_var_value(1);
  constraint.mutable_constraint()->set_lower_bound(-kInf);
  constraint.mutable_constraint()->set_upper_bound(3.0);
  constraint.mutable_constraint()->add_var_index(0);
  constraint.mutable_constraint()->add_coefficient(2.0);
  constraint.mutable_constraint()->add_var_index(1);
  constraint.mutable_constraint()->add_coefficient(1.0);
  EXPECT_THAT(model, EqualsProto(expected));

  {
    ModelProto mo_activate_on_zero_model = starting_model;
    (*mo_activate_on_zero_model.mutable_indicator_constraints())[0]
        .set_activate_on_zero(true);
    ASSERT_OK_AND_ASSIGN(auto mp_activate_on_zero_model,
                         MathOptModelToMPModelProto(mo_activate_on_zero_model));
    MPModelProto expected_activate_on_zero_model = expected;
    expected_activate_on_zero_model.mutable_general_constraint(0)
        ->mutable_indicator_constraint()
        ->set_var_value(0);
    EXPECT_THAT(mp_activate_on_zero_model,
                EqualsProto(expected_activate_on_zero_model));
  }

  // Unset indicator variables in a MathOpt model are not copied to MPModels.
  {
    ModelProto unset_model = starting_model;
    (*unset_model.mutable_indicator_constraints())[0].clear_indicator_id();
    MPModelProto unset_expected = expected;
    unset_expected.clear_general_constraint();
    EXPECT_THAT(MathOptModelToMPModelProto(unset_model),
                IsOkAndHolds(EqualsProto(unset_expected)));
  }
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
