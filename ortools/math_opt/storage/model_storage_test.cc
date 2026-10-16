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

#include "ortools/math_opt/storage/model_storage.h"

#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/indicator/storage_test_helper.h"  // IWYU pragma: keep
#include "ortools/math_opt/constraints/quadratic/storage_test_helper.h"  // IWYU pragma: keep
#include "ortools/math_opt/constraints/second_order_cone/storage_test_helper.h"  // IWYU pragma: keep
#include "ortools/math_opt/constraints/sos/storage_test_helper.h"  // IWYU pragma: keep
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_testing.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/validators/model_validator.h"
namespace operations_research {
namespace math_opt {
namespace {

using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ::testing::ElementsAre;
using ::testing::EqualsProto;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

//   max primary:   1.5 * x * y + 3.5 * y^2 + 2.0 * y + 3.5
//   min secondary: 2.0 * x + 3.0
//   s.t. x + y <= 1.5 (c)
//      2.0 * y >= 0.5 (d)
//            x unbounded
//            y in {0, 1}
class SimpleModelTest : public testing::Test {
 protected:
  SimpleModelTest()
      : storage_("simple_model_test", "primary"),
        update_tracker_(storage_.NewUpdateTracker()) {
    x_ = storage_.AddVariable("x");
    y_ = storage_.AddVariable(0.0, 1.0, true, "y");

    c_ = storage_.AddLinearConstraint(-kInf, 1.5, "c");
    d_ = storage_.AddLinearConstraint("d");
    storage_.set_linear_constraint_lower_bound(d_, 0.5);

    storage_.set_linear_constraint_coefficient(c_, x_, 1.0);
    storage_.set_linear_constraint_coefficient(c_, y_, 1.0);
    storage_.set_linear_constraint_coefficient(d_, y_, 2.0);

    storage_.set_maximize(kPrimaryObjectiveId);
    storage_.set_objective_offset(kPrimaryObjectiveId, 3.5);
    storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, y_, 2.0);
    storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_,
                                                 1.5);
    storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, y_, y_,
                                                 3.5);

    o_ = storage_.AddAuxiliaryObjective(/*priority=*/1, "secondary");
    storage_.set_linear_objective_coefficient(o_, x_, 2.0);
    storage_.set_objective_offset(o_, 3.0);
  }

  ModelProto proto_form() {
    return ParseTextProtoOrDie(R"pb(
      name: "simple_model_test"
      variables: {
        ids: [ 0, 1 ]
        lower_bounds: [ -infinity, 0.0 ]
        upper_bounds: [ infinity, 1.0 ]
        integers: [ false, true ]
        names: [ "x", "y" ]
      }
      objective: {
        maximize: true
        offset: 3.5
        linear_coefficients {
          ids: [ 1 ]
          values: [ 2.0 ]
        }
        quadratic_coefficients {
          row_ids: [ 0, 1 ]
          column_ids: [ 1, 1 ]
          coefficients: [ 1.5, 3.5 ]
        }
        name: "primary"
      }
      auxiliary_objectives {
        key: 0
        value: {
          maximize: false
          priority: 1
          offset: 3.0
          linear_coefficients {
            ids: [ 0 ]
            values: [ 2.0 ]
          }
          name: "secondary"
        }
      }
      linear_constraints: {
        ids: [ 0, 1 ]
        lower_bounds: [ -infinity, 0.5 ],
        upper_bounds: [ 1.5, infinity ]
        names: [ "c", "d" ]
      }
      linear_constraint_matrix {
        row_ids: [ 0, 0, 1 ]
        column_ids: [ 0, 1, 1 ],
        coefficients: [ 1.0, 1.0, 2.0 ]
      }
    )pb");
  }

  ModelStorage storage_;
  const UpdateTrackerId update_tracker_;
  VariableId x_;
  VariableId y_;
  AuxiliaryObjectiveId o_;
  LinearConstraintId c_;
  LinearConstraintId d_;
};

TEST_F(SimpleModelTest, ModelName) {
  EXPECT_EQ(storage_.name(), "simple_model_test");
}

TEST_F(SimpleModelTest, CreateOutput) {
  EXPECT_EQ(x_, VariableId(0));
  EXPECT_EQ(y_, VariableId(1));
  EXPECT_EQ(c_, LinearConstraintId(0));
  EXPECT_EQ(d_, LinearConstraintId(1));
}

TEST_F(SimpleModelTest, ReadVariableAttributes) {
  EXPECT_DOUBLE_EQ(storage_.variable_lower_bound(x_), -kInf);
  EXPECT_DOUBLE_EQ(storage_.variable_upper_bound(x_), kInf);
  EXPECT_DOUBLE_EQ(storage_.variable_lower_bound(y_), 0.0);
  EXPECT_DOUBLE_EQ(storage_.variable_upper_bound(y_), 1.0);
  EXPECT_FALSE(storage_.is_variable_integer(x_));
  EXPECT_TRUE(storage_.is_variable_integer(y_));
  EXPECT_EQ(storage_.variable_name(x_), "x");
  EXPECT_EQ(storage_.variable_name(y_), "y");
}

TEST_F(SimpleModelTest, ReadVariables) {
  EXPECT_EQ(storage_.num_variables(), 2);
  EXPECT_EQ(storage_.next_variable_id(), VariableId(2));
  EXPECT_TRUE(storage_.has_variable(VariableId(0)));
  EXPECT_TRUE(storage_.has_variable(VariableId(1)));
  EXPECT_FALSE(storage_.has_variable(VariableId(2)));
  EXPECT_THAT(storage_.variables(),
              UnorderedElementsAre(VariableId(0), VariableId(1)));
  EXPECT_THAT(storage_.SortedVariables(),
              ElementsAre(VariableId(0), VariableId(1)));
}

TEST_F(SimpleModelTest, ReadLinearConstraintAttributes) {
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_lower_bound(c_), -kInf);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_upper_bound(c_), 1.5);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_lower_bound(d_), 0.5);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_upper_bound(d_), kInf);
  EXPECT_EQ(storage_.linear_constraint_name(c_), "c");
  EXPECT_EQ(storage_.linear_constraint_name(d_), "d");
}

TEST_F(SimpleModelTest, ReadLinearConstraints) {
  EXPECT_EQ(storage_.num_linear_constraints(), 2);
  EXPECT_EQ(storage_.next_linear_constraint_id(), LinearConstraintId(2));
  EXPECT_TRUE(storage_.has_linear_constraint(LinearConstraintId(0)));
  EXPECT_TRUE(storage_.has_linear_constraint(LinearConstraintId(1)));
  EXPECT_FALSE(storage_.has_linear_constraint(LinearConstraintId(2)));

  EXPECT_THAT(
      storage_.LinearConstraints(),
      UnorderedElementsAre(LinearConstraintId(0), LinearConstraintId(1)));
  EXPECT_THAT(storage_.SortedLinearConstraints(),
              ElementsAre(LinearConstraintId(0), LinearConstraintId(1)));
}

TEST_F(SimpleModelTest, ReadLinearConstraintMatrix) {
  // Check the constraint matrix
  EXPECT_TRUE(storage_.is_linear_constraint_coefficient_nonzero(c_, x_));
  EXPECT_TRUE(storage_.is_linear_constraint_coefficient_nonzero(c_, y_));
  EXPECT_FALSE(storage_.is_linear_constraint_coefficient_nonzero(d_, x_));
  EXPECT_TRUE(storage_.is_linear_constraint_coefficient_nonzero(d_, y_));

  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(c_, x_), 1.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(c_, y_), 1.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(d_, x_), 0.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(d_, y_), 2.0);

  EXPECT_THAT(storage_.linear_constraint_matrix(),
              UnorderedElementsAre(
                  std::make_tuple(LinearConstraintId(0), VariableId(0), 1.0),
                  std::make_tuple(LinearConstraintId(0), VariableId(1), 1.0),
                  std::make_tuple(LinearConstraintId(1), VariableId(1), 2.0)));
}

TEST_F(SimpleModelTest, ReadRows) {
  EXPECT_THAT(storage_.variables_in_linear_constraint(c_),
              UnorderedElementsAre(x_, y_));
  EXPECT_THAT(storage_.variables_in_linear_constraint(d_),
              UnorderedElementsAre(y_));
}
TEST_F(SimpleModelTest, ReadColumns) {
  EXPECT_THAT(storage_.linear_constraints_with_variable(x_),
              UnorderedElementsAre(c_));
  EXPECT_THAT(storage_.linear_constraints_with_variable(y_),
              UnorderedElementsAre(c_, d_));
}

TEST_F(SimpleModelTest, ReadPrimaryObjective) {
  EXPECT_EQ(storage_.num_linear_objective_terms(kPrimaryObjectiveId), 1);
  EXPECT_EQ(storage_.num_quadratic_objective_terms(kPrimaryObjectiveId), 2);
  EXPECT_TRUE(storage_.is_maximize(kPrimaryObjectiveId));
  EXPECT_EQ(storage_.objective_name(kPrimaryObjectiveId), "primary");
  EXPECT_DOUBLE_EQ(storage_.objective_offset(kPrimaryObjectiveId), 3.5);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, x_), 0.0);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, y_), 2.0);
  EXPECT_FALSE(storage_.is_linear_objective_coefficient_nonzero(
      kPrimaryObjectiveId, x_));
  EXPECT_TRUE(storage_.is_linear_objective_coefficient_nonzero(
      kPrimaryObjectiveId, y_));
  EXPECT_THAT(storage_.linear_objective(kPrimaryObjectiveId),
              UnorderedElementsAre(Pair(VariableId(1), 2.0)));
  EXPECT_THAT(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, x_),
      0.0);
  EXPECT_THAT(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_),
      1.5);
  EXPECT_THAT(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, y_, x_),
      1.5);
  EXPECT_THAT(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, y_, y_),
      3.5);
  EXPECT_FALSE(storage_.is_quadratic_objective_coefficient_nonzero(
      kPrimaryObjectiveId, x_, x_));
  EXPECT_TRUE(storage_.is_quadratic_objective_coefficient_nonzero(
      kPrimaryObjectiveId, x_, y_));
  EXPECT_TRUE(storage_.is_quadratic_objective_coefficient_nonzero(
      kPrimaryObjectiveId, y_, x_));
  EXPECT_TRUE(storage_.is_quadratic_objective_coefficient_nonzero(
      kPrimaryObjectiveId, y_, y_));
  // NOTE: util/tuple/matchers.h is not in open source.
  std::vector<std::tuple<VariableId, VariableId, double>> terms =
      storage_.quadratic_objective_terms(kPrimaryObjectiveId);
  ASSERT_EQ(2, terms.size());
  absl::c_sort(terms);
  EXPECT_EQ(std::make_tuple(VariableId(0), VariableId(1), 1.5), terms[0]);
  EXPECT_EQ(std::make_tuple(VariableId(1), VariableId(1), 3.5), terms[1]);
}

TEST_F(SimpleModelTest, ReadAuxiliaryObjective) {
  EXPECT_EQ(storage_.num_linear_objective_terms(o_), 1);
  EXPECT_EQ(storage_.num_quadratic_objective_terms(o_), 0);
  EXPECT_FALSE(storage_.is_maximize(o_));
  EXPECT_EQ(storage_.objective_priority(o_), 1);
  EXPECT_DOUBLE_EQ(storage_.objective_offset(o_), 3.0);
  EXPECT_DOUBLE_EQ(storage_.linear_objective_coefficient(o_, x_), 2.0);
  EXPECT_DOUBLE_EQ(storage_.linear_objective_coefficient(o_, y_), 0.0);
  EXPECT_TRUE(storage_.is_linear_objective_coefficient_nonzero(o_, x_));
  EXPECT_FALSE(storage_.is_linear_objective_coefficient_nonzero(o_, y_));
  EXPECT_THAT(storage_.linear_objective(o_),
              UnorderedElementsAre(Pair(VariableId(0), 2.0)));
  EXPECT_EQ(storage_.objective_name(o_), "secondary");
  EXPECT_EQ(storage_.num_auxiliary_objectives(), 1);
  EXPECT_EQ(storage_.next_auxiliary_objective_id(), AuxiliaryObjectiveId(1));
  EXPECT_TRUE(storage_.has_auxiliary_objective(AuxiliaryObjectiveId(0)));
  EXPECT_FALSE(storage_.has_auxiliary_objective(AuxiliaryObjectiveId(1)));
  EXPECT_THAT(storage_.AuxiliaryObjectives(),
              UnorderedElementsAre(AuxiliaryObjectiveId(0)));
  EXPECT_THAT(storage_.SortedAuxiliaryObjectives(),
              ElementsAre(AuxiliaryObjectiveId(0)));
}

// `set_objective_offset` and `set_linear_objective_coefficient` are already
// tested via the fixture constructor.
TEST_F(SimpleModelTest, SetAuxiliaryObjective) {
  EXPECT_FALSE(storage_.is_maximize(o_));
  storage_.set_minimize(o_);
  EXPECT_FALSE(storage_.is_maximize(o_));
  storage_.set_maximize(o_);
  EXPECT_TRUE(storage_.is_maximize(o_));
  storage_.set_is_maximize(o_, false);

  EXPECT_EQ(storage_.objective_priority(o_), 1);
  storage_.set_objective_priority(o_, 3);
  EXPECT_EQ(storage_.objective_priority(o_), 3);
}

TEST_F(SimpleModelTest, ModelSizeDebugString) {
  EXPECT_EQ(storage_.ModelSizeDebugString(),
            "2 vars, 2 lin. constraints with 3 non-zeros");
}

TEST_F(SimpleModelTest, ExportModel) {
  const ModelProto actual = storage_.ExportModel();
  ASSERT_OK(ValidateModel(actual).status());
  const ModelProto expected = proto_form();
  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST_F(SimpleModelTest, ExportModelRemoveNames) {
  const ModelProto actual = storage_.ExportModel(/*remove_names=*/true);
  ASSERT_OK(ValidateModel(actual).status());
  ModelProto expected = proto_form();
  // This model has only variables, linear constraints, and auxiliary
  // objectives, clear all their names.
  expected.clear_name();
  expected.mutable_variables()->clear_names();
  expected.mutable_linear_constraints()->clear_names();
  for (auto& [id, objective] : *expected.mutable_auxiliary_objectives()) {
    objective.clear_name();
  }
  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST_F(SimpleModelTest, FromModelProto) {
  // Here we simply check that we end up with the same proto if we build a
  // storage from a proto and export a new proto.
  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<ModelStorage> parsed_storage,
                       ModelStorage::FromModelProto(proto_form()));
  EXPECT_THAT(parsed_storage->ExportModel(), EquivToProto(proto_form()));
}

TEST(ModelStorageTest, FromModelProtoWithHoles) {
  // Test building a storage from an input model with missing variables and
  // constraints ids.
  ModelProto model_proto;

  VariablesProto& variables = *model_proto.mutable_variables();
  variables.add_ids(1);
  variables.add_lower_bounds(0.0);
  variables.add_upper_bounds(1.0);
  variables.add_integers(false);
  variables.add_names("x");

  variables.add_ids(3);
  variables.add_lower_bounds(-2.0);
  variables.add_upper_bounds(4.0);
  variables.add_integers(true);
  variables.add_names("y");

  LinearConstraintsProto& linear_constraints =
      *model_proto.mutable_linear_constraints();
  linear_constraints.add_ids(5);
  linear_constraints.add_lower_bounds(0.0);
  linear_constraints.add_upper_bounds(kInf);
  linear_constraints.add_names("u");

  linear_constraints.add_ids(7);
  linear_constraints.add_lower_bounds(-kInf);
  linear_constraints.add_upper_bounds(0.0);
  linear_constraints.add_names("v");

  (*model_proto.mutable_auxiliary_objectives())[3] = {};

  model_proto.mutable_linear_constraint_matrix()->add_row_ids(7);
  model_proto.mutable_linear_constraint_matrix()->add_column_ids(3);
  model_proto.mutable_linear_constraint_matrix()->add_coefficients(-1.0);

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<ModelStorage> parsed_storage,
                       ModelStorage::FromModelProto(model_proto));

  EXPECT_THAT(parsed_storage->ExportModel(), EquivToProto(model_proto));

  // Test that adding a new variable/constraint returns the expected new index
  // (i.e. that next_variable_id_/next_linear_constraint_id_ has been correctly
  // updated).
  EXPECT_EQ(parsed_storage->AddVariable(), VariableId(4));
  EXPECT_EQ(parsed_storage->AddAuxiliaryObjective(1, ""),
            AuxiliaryObjectiveId(4));
  EXPECT_EQ(parsed_storage->AddLinearConstraint(), LinearConstraintId(8));
}

TEST(ModelStorageTest, FromInvalidModelProto) {
  // Here we only test one case of invalidity. We internally use
  // ValidateModel() and thus trust it to have checked all other cases.
  ModelProto bad_model_proto;
  bad_model_proto.mutable_variables()->add_ids(1);
  bad_model_proto.mutable_variables()->add_lower_bounds(0.0);
  bad_model_proto.mutable_variables()->add_upper_bounds(1.0);
  // Here we intentionally don't call add_integers().

  EXPECT_THAT(
      ModelStorage::FromModelProto(bad_model_proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("integers")));
}

TEST(ModelStorageTest, FromModelProtoWithNoVariableNames) {
  // Keep names() empty for variables.
  ModelProto model_proto;
  model_proto.mutable_variables()->add_ids(1);
  model_proto.mutable_variables()->add_lower_bounds(0.0);
  model_proto.mutable_variables()->add_upper_bounds(1.0);
  model_proto.mutable_variables()->add_integers(false);

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<ModelStorage> parsed_storage,
                       ModelStorage::FromModelProto(model_proto));

  EXPECT_EQ(parsed_storage->variable_lower_bound(VariableId(1)), 0.0);
  EXPECT_EQ(parsed_storage->variable_upper_bound(VariableId(1)), 1.0);
  EXPECT_FALSE(parsed_storage->is_variable_integer(VariableId(1)));
  EXPECT_THAT(parsed_storage->variable_name(VariableId(1)), IsEmpty());
}

TEST(ModelStorageTest, FromModelProtoWithNoLinearConstraintNames) {
  // Keep names() empty for linear constraints.
  ModelProto model_proto;
  model_proto.mutable_linear_constraints()->add_ids(1);
  model_proto.mutable_linear_constraints()->add_lower_bounds(0.0);
  model_proto.mutable_linear_constraints()->add_upper_bounds(1.0);

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<ModelStorage> parsed_storage,
                       ModelStorage::FromModelProto(model_proto));

  EXPECT_EQ(
      parsed_storage->linear_constraint_lower_bound(LinearConstraintId(1)),
      0.0);
  EXPECT_EQ(
      parsed_storage->linear_constraint_upper_bound(LinearConstraintId(1)),
      1.0);
  EXPECT_THAT(parsed_storage->linear_constraint_name(LinearConstraintId(1)),
              IsEmpty());
}

TEST_F(SimpleModelTest, Clone) {
  // Here we simply check that we end up with the same proto if we clone the
  // storage.
  const ModelStorage& const_ref = storage_;
  const std::unique_ptr<ModelStorage> clone = const_ref.Clone();
  EXPECT_THAT(clone->ExportModel(), EquivToProto(proto_form()));
}

TEST_F(SimpleModelTest, CloneEmptyNewName) {
  // Here we simply check that we end up with the same proto if we clone the
  // storage.
  const ModelStorage& const_ref = storage_;
  const std::unique_ptr<ModelStorage> clone = const_ref.Clone("");
  ModelProto expected_proto = proto_form();
  expected_proto.set_name("");
  EXPECT_THAT(clone->ExportModel(), EquivToProto(expected_proto));
}

TEST_F(SimpleModelTest, CloneNewName) {
  // Here we simply check that we end up with the same proto if we clone the
  // storage.
  const ModelStorage& const_ref = storage_;
  const std::unique_ptr<ModelStorage> clone =
      const_ref.Clone("not_simple_anymore");
  ModelProto expected_proto = proto_form();
  expected_proto.set_name("not_simple_anymore");
  EXPECT_THAT(clone->ExportModel(), EquivToProto(expected_proto));
}

TEST(ModelStorageTest, CloneWithHoles) {
  // Test cloning a storage with missing variables and constraints ids before
  // the last items.
  ModelStorage initial_storage("model");

  // Here we directly delete variables since the ModelStorage won't reuse an id
  // already returned. We don't bother giving names or bounds to these
  // variables.
  initial_storage.DeleteVariable(initial_storage.AddVariable());
  const VariableId x =
      initial_storage.AddVariable(/*lower_bound=*/0.0, /*upper_bound=*/1.0,
                                  /*is_integer=*/false, "x");
  initial_storage.DeleteVariable(initial_storage.AddVariable());
  const VariableId y =
      initial_storage.AddVariable(/*lower_bound=*/-2.0, /*upper_bound=*/4.0,
                                  /*is_integer=*/true, "y");

  initial_storage.DeleteLinearConstraint(initial_storage.AddLinearConstraint());
  initial_storage.DeleteLinearConstraint(initial_storage.AddLinearConstraint());
  initial_storage.DeleteLinearConstraint(initial_storage.AddLinearConstraint());
  initial_storage.DeleteLinearConstraint(initial_storage.AddLinearConstraint());
  initial_storage.DeleteLinearConstraint(initial_storage.AddLinearConstraint());
  initial_storage.AddLinearConstraint(/*lower_bound=*/0.0, /*upper_bound=*/kInf,
                                      "u");
  initial_storage.DeleteLinearConstraint(initial_storage.AddLinearConstraint());
  const LinearConstraintId v = initial_storage.AddLinearConstraint(
      /*lower_bound=*/-kInf, /*upper_bound=*/0.0, "v");

  initial_storage.set_linear_objective_coefficient(kPrimaryObjectiveId, x, 2.0);
  initial_storage.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x, y,
                                                      5.0);

  initial_storage.set_linear_constraint_coefficient(v, x, -1.0);

  {
    const std::unique_ptr<ModelStorage> clone = initial_storage.Clone();

    EXPECT_THAT(clone->ExportModel(),
                EquivToProto(initial_storage.ExportModel()));

    // Test that adding a new variable/constraint returns the expected new index
    // (i.e. that next_variable_id_/next_linear_constraint_id_ has been
    // correctly updated to take into account removed ids from the initial
    // model).
    EXPECT_EQ(clone->AddVariable(), y.Next());
    EXPECT_EQ(clone->AddLinearConstraint(), v.Next());
  }

  // Add removed variables/constraints at the end.
  initial_storage.DeleteVariable(initial_storage.AddVariable());
  const VariableId last_added_variable = initial_storage.AddVariable();
  initial_storage.DeleteVariable(last_added_variable);

  initial_storage.DeleteLinearConstraint(initial_storage.AddLinearConstraint());
  initial_storage.DeleteLinearConstraint(initial_storage.AddLinearConstraint());
  const LinearConstraintId last_added_linear_constraint =
      initial_storage.AddLinearConstraint();
  initial_storage.DeleteLinearConstraint(last_added_linear_constraint);

  const std::unique_ptr<ModelStorage> clone = initial_storage.Clone();

  EXPECT_THAT(clone->ExportModel(),
              EquivToProto(initial_storage.ExportModel()));

  // We expect to take into account the removed variables/constraints.
  EXPECT_EQ(clone->AddVariable(), last_added_variable.Next());
  EXPECT_EQ(clone->AddLinearConstraint(), last_added_linear_constraint.Next());
}

TEST(ModelStorageTest, CloneWithDuplicatedNames) {
  // The ModelStorage class does not check that names are not duplicated. We
  // don't want the Clone() to fail (and crash) if this happens.
  ModelStorage initial_storage("model");

  initial_storage.AddVariable(/*lower_bound=*/0.0, /*upper_bound=*/1.0,
                              /*is_integer=*/false, "x");
  initial_storage.AddVariable(/*lower_bound=*/-2.0, /*upper_bound=*/4.0,
                              /*is_integer=*/true, "x");

  initial_storage.AddLinearConstraint(/*lower_bound=*/0.0, /*upper_bound=*/kInf,
                                      "u");
  initial_storage.AddLinearConstraint(
      /*lower_bound=*/-kInf, /*upper_bound=*/0.0, "u");

  const std::unique_ptr<ModelStorage> clone = initial_storage.Clone();

  EXPECT_THAT(clone->ExportModel(),
              EquivToProto(initial_storage.ExportModel()));
}

TEST_F(SimpleModelTest, ExportModelUpdate) {
  const std::optional<ModelUpdateProto> actual =
      storage_.ExportModelUpdate(update_tracker_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(
      R"pb(
        new_variables: {
          ids: [ 0, 1 ]
          lower_bounds: [ -infinity, 0.0 ]
          upper_bounds: [ infinity, 1.0 ]
          integers: [ false, true ]
          names: [ "x", "y" ]
        }
        objective_updates: {
          direction_update: true
          offset_update: 3.5
          linear_coefficients {
            ids: [ 1 ]
            values: [ 2.0 ]
          }
          quadratic_coefficients {
            row_ids: [ 0, 1 ]
            column_ids: [ 1, 1 ]
            coefficients: [ 1.5, 3.5 ]
          }
        }
        auxiliary_objectives_updates {
          new_objectives {
            key: 0
            value {
              offset: 3
              linear_coefficients { ids: 0 values: 2 }
              name: "secondary"
              priority: 1
            }
          }
        }
        new_linear_constraints: {
          ids: [ 0, 1 ]
          lower_bounds: [ -infinity, 0.5 ],
          upper_bounds: [ 1.5, infinity ]
          names: [ "c", "d" ]
        }
        linear_constraint_matrix_updates {
          row_ids: [ 0, 0, 1 ]
          column_ids: [ 0, 1, 1 ],
          coefficients: [ 1.0, 1.0, 2.0 ]
        }
      )pb");
  EXPECT_THAT(actual, Optional(EquivToProto(expected)));
}

TEST_F(SimpleModelTest, ApplyModelUpdateOnEmptyStorage) {
  // TODO(b/213868088): it should not be necessary to copy the name after we
  // support renaming the model (here and below).
  ModelStorage updated_storage(storage_.name(),
                               storage_.objective_name(kPrimaryObjectiveId));
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_NE(update, std::nullopt);
  ASSERT_OK(updated_storage.ApplyUpdateProto(update.value()));
  EXPECT_THAT(updated_storage.ExportModel(),
              EquivToProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, ApplyModelUpdateWithReusedVariableId) {
  storage_.DeleteVariable(y_);

  ModelUpdateProto update;
  update.mutable_new_variables()->add_ids(y_.value());
  update.mutable_new_variables()->add_lower_bounds(-2.0);
  update.mutable_new_variables()->add_upper_bounds(5.0);
  update.mutable_new_variables()->add_integers(false);

  EXPECT_THAT(
      storage_.ApplyUpdateProto(update),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("increasing")));
}

TEST_F(SimpleModelTest, ApplyModelUpdateWithReusedLinearConstraintId) {
  storage_.DeleteLinearConstraint(d_);

  ModelUpdateProto update;
  update.mutable_new_linear_constraints()->add_ids(d_.value());
  update.mutable_new_linear_constraints()->add_lower_bounds(-2.0);
  update.mutable_new_linear_constraints()->add_upper_bounds(5.0);

  EXPECT_THAT(
      storage_.ApplyUpdateProto(update),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("increasing")));
}

TEST_F(SimpleModelTest, ApplyModelUpdateWithReusedAuxiliaryObjectiveId) {
  storage_.DeleteAuxiliaryObjective(o_);

  ModelUpdateProto update;
  (*update.mutable_auxiliary_objectives_updates()
        ->mutable_new_objectives())[o_.value()] = {};

  EXPECT_THAT(
      storage_.ApplyUpdateProto(update),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("increasing")));
}

TEST_F(SimpleModelTest, ApplyModelUpdateDeleteNonExistingVariableId) {
  storage_.DeleteVariable(y_);

  ModelUpdateProto update;
  update.add_deleted_variable_ids(y_.value());

  EXPECT_THAT(
      storage_.ApplyUpdateProto(update),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("delete")));
}

TEST_F(SimpleModelTest, ApplyInvalidModelUpdate) {
  ModelUpdateProto update;
  update.mutable_new_variables()->add_ids(y_.value() + 1);
  // Missing lower_bound.
  update.mutable_new_variables()->add_upper_bounds(5.0);
  update.mutable_new_variables()->add_integers(false);

  EXPECT_THAT(
      storage_.ApplyUpdateProto(update),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("lower_bound")));
}

TEST_F(SimpleModelTest, UpdateEmptyAfterCheckpoint) {
  storage_.AdvanceCheckpoint(update_tracker_);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, ExportModelUpdateDiscardName) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.AddVariable(0.0, 1.0, false, "z");
  ModelUpdateProto expected;
  expected.mutable_new_variables()->add_ids(2);
  expected.mutable_new_variables()->add_upper_bounds(1.0);
  expected.mutable_new_variables()->add_lower_bounds(0.0);
  expected.mutable_new_variables()->add_integers(false);
  EXPECT_THAT(
      storage_.ExportModelUpdate(update_tracker_, /*remove_names=*/true),
      Optional(EquivToProto(expected)));
  expected.mutable_new_variables()->add_names("z");
  EXPECT_THAT(
      storage_.ExportModelUpdate(update_tracker_, /*remove_names=*/false),
      Optional(EquivToProto(expected)));
}

TEST_F(SimpleModelTest, UpdateVariableLowerBound) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_variable_lower_bound(y_, 1.0);
  EXPECT_DOUBLE_EQ(storage_.variable_lower_bound(y_), 1.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    variable_updates {
      lower_bounds {
        ids: [ 1 ]
        values: [ 1.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateEmptyOnIdenticalVariableLowerBound) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_variable_lower_bound(y_, 0.0);
  EXPECT_DOUBLE_EQ(storage_.variable_lower_bound(y_), 0.0);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateVariableUpperBound) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_variable_upper_bound(x_, 2.0);
  EXPECT_DOUBLE_EQ(storage_.variable_upper_bound(x_), 2.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    variable_updates {
      upper_bounds {
        ids: [ 0 ]
        values: [ 2.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateVariableIsInteger) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_variable_as_integer(x_);
  storage_.set_variable_as_continuous(y_);
  EXPECT_TRUE(storage_.is_variable_integer(x_));
  EXPECT_FALSE(storage_.is_variable_integer(y_));
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    variable_updates {
      integers {
        ids: [ 0, 1 ]
        values: [ true, false ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateAddVariable) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  const VariableId z = storage_.AddVariable(0.0, 1.0, true, "z");
  storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, z, 10.0);
  storage_.set_linear_constraint_coefficient(d_, z, -1.0);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, z, x_, 5.0);
  EXPECT_EQ(storage_.num_variables(), 3);
  EXPECT_EQ(storage_.num_linear_constraints(), 2);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    new_variables {
      ids: [ 2 ]
      lower_bounds: [ 0.0 ]
      upper_bounds: [ 1.0 ]
      integers: [ true ]
      names: [ "z" ]
    }
    objective_updates {
      linear_coefficients {
        ids: [ 2 ]
        values: [ 10.0 ]
      }
      quadratic_coefficients {
        row_ids: [ 0 ]
        column_ids: [ 2 ]
        coefficients: [ 5.0 ]
      }
    }
    linear_constraint_matrix_updates {
      row_ids: [ 1 ]
      column_ids: [ 2 ]
      coefficients: [ -1.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateLinearConstraintLowerBound) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_lower_bound(c_, -1.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_lower_bound(c_), -1.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    linear_constraint_updates {
      lower_bounds {
        ids: [ 0 ]
        values: [ -1.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateEmptyOnIdenticalLinearConstraintLowerBound) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_lower_bound(c_, -kInf);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_lower_bound(c_), -kInf);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateLinearConstraintUpperBound) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_upper_bound(d_, 2.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_upper_bound(d_), 2.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    linear_constraint_updates {
      upper_bounds {
        ids: [ 1 ]
        values: [ 2.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateAddLinearConstraint) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  const LinearConstraintId e = storage_.AddLinearConstraint(0.0, 1.0, "e");
  storage_.set_linear_constraint_coefficient(e, y_, 3.0);
  EXPECT_EQ(storage_.num_variables(), 2);
  EXPECT_EQ(storage_.num_linear_constraints(), 3);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    new_linear_constraints {
      ids: [ 2 ]
      lower_bounds: [ 0.0 ]
      upper_bounds: [ 1.0 ]
      names: [ "e" ]
    }
    linear_constraint_matrix_updates {
      row_ids: [ 2 ]
      column_ids: [ 1 ]
      coefficients: [ 3.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateLinearConstraintMatrixChangeNonzero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_coefficient(d_, y_, 4.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(d_, y_), 4.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    linear_constraint_matrix_updates {
      row_ids: [ 1 ]
      column_ids: [ 1 ]
      coefficients: [ 4.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateLinearConstraintMatrixAddNonzero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_coefficient(d_, x_, 4.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(d_, x_), 4.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    linear_constraint_matrix_updates {
      row_ids: [ 1 ]
      column_ids: [ 0 ]
      coefficients: [ 4.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateLinearConstraintMatrixDeleteNonzero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_coefficient(c_, y_, 0.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(c_, y_), 0.0);
  EXPECT_FALSE(storage_.is_linear_constraint_coefficient_nonzero(c_, y_));
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    linear_constraint_matrix_updates {
      row_ids: [ 0 ]
      column_ids: [ 1 ]
      coefficients: [ 0.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateEmptyOnIdenticalLinearConstraintMatrixNonzero) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_coefficient(c_, y_, 1.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(c_, y_), 1.0);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateEmptyOnIdenticalLinearConstraintMatrixZero) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_coefficient(d_, x_, 0.0);
  EXPECT_DOUBLE_EQ(storage_.linear_constraint_coefficient(d_, x_), 0.0);
  EXPECT_FALSE(storage_.is_linear_constraint_coefficient_nonzero(d_, x_));
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateLinearObjectiveCoefficientWasZero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, x_, 3.0);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, x_), 3.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates {
      linear_coefficients {
        ids: [ 0 ]
        values: [ 3.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateQuadraticObjectiveCoefficientWasZero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, x_,
                                               3.0);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, x_),
      3.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates {
      quadratic_coefficients {
        row_ids: [ 0 ]
        column_ids: [ 0 ]
        coefficients: [ 3.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateLinearObjectiveCoefficientWasNonzero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, y_, 3.0);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, y_), 3.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates {
      linear_coefficients {
        ids: [ 1 ]
        values: [ 3.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateQuadraticObjectiveCoefficientWasNonzero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, y_, x_,
                                               3.0);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_),
      3.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates {
      quadratic_coefficients {
        row_ids: [ 0 ]
        column_ids: [ 1 ]
        coefficients: [ 3.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateLinearObjectiveCoefficientSetToZero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, y_, 0.0);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, y_), 0.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates {
      linear_coefficients {
        ids: [ 1 ]
        values: [ 0.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateQuadraticObjectiveCoefficientSetToZero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_,
                                               0.0);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_),
      0.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates {
      quadratic_coefficients {
        row_ids: [ 0 ]
        column_ids: [ 1 ]
        coefficients: [ 0.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, ClearQuadraticObjectiveThenDeleteInvolvedVariable) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);

  // Set all quadratic objective terms to zero
  // ...then delete a variable previously involved
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_,
                                               0.0);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, y_, y_,
                                               0.0);
  storage_.DeleteVariable(x_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_variable_ids: [ 0 ]
    objective_updates {
      quadratic_coefficients {
        row_ids: [ 1 ]
        column_ids: [ 1 ]
        coefficients: [ 0.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateClearObjective) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.clear_objective(kPrimaryObjectiveId);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, x_), 0.0);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, y_), 0.0);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, x_),
      0.0);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_),
      0.0);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, y_, x_),
      0.0);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, y_, y_),
      0.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates {
      linear_coefficients {
        ids: [ 1 ]
        values: [ 0.0 ]
      }
      quadratic_coefficients {
        row_ids: [ 0, 1 ]
        column_ids: [ 1, 1 ]
        coefficients: [ 0.0, 0.0 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest,
       UpdateEmptyOnIdenticalNonzeroLinearObjectiveCoefficient) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, y_, 2.0);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, y_), 2.0);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest,
       UpdateEmptyOnIdenticalNonzeroQuadraticObjectiveCoefficient) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, y_, x_,
                                               1.5);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_),
      1.5);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateEmptyOnIdenticalZeroLinearObjectiveCoefficient) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, x_, 0.0);
  EXPECT_DOUBLE_EQ(
      storage_.linear_objective_coefficient(kPrimaryObjectiveId, x_), 0.0);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest,
       UpdateEmptyOnIdenticalZeroQuadraticObjectiveCoefficient) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, x_,
                                               0.0);
  EXPECT_DOUBLE_EQ(
      storage_.quadratic_objective_coefficient(kPrimaryObjectiveId, x_, x_),
      0.0);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, MultipleUpdatesWithDifferentQuadraticObjectiveTerms) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, x_,
                                               1.2);
  {
    const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
      objective_updates {
        quadratic_coefficients {
          row_ids: [ 0 ]
          column_ids: [ 0 ]
          coefficients: [ 1.2 ]
        }
      }
    )pb");
    const std::optional<ModelUpdateProto> update =
        storage_.ExportModelUpdate(update_tracker_);
    ASSERT_THAT(update, Optional(EquivToProto(expected)));

    ASSERT_OK(clone->ApplyUpdateProto(update.value()));
    EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
  }

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_,
                                               3.4);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates {
      quadratic_coefficients {
        row_ids: [ 0 ]
        column_ids: [ 1 ]
        coefficients: [ 3.4 ]
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateChangeObjectiveDirection) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_minimize(kPrimaryObjectiveId);
  EXPECT_FALSE(storage_.is_maximize(kPrimaryObjectiveId));
  // TODO(b/185892947): EquivToProto() does not differentiate unset from set to
  // false. We should stop using it here and elsewhere.
  {
    const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
      objective_updates { direction_update: false }
    )pb");
    const std::optional<ModelUpdateProto> update =
        storage_.ExportModelUpdate(update_tracker_);
    ASSERT_THAT(update, Optional(EquivToProto(expected)));

    ASSERT_OK(clone->ApplyUpdateProto(update.value()));
    EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
  }

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_maximize(kPrimaryObjectiveId);
  EXPECT_TRUE(storage_.is_maximize(kPrimaryObjectiveId));
  {
    const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
      objective_updates { direction_update: true }
    )pb");
    const std::optional<ModelUpdateProto> update =
        storage_.ExportModelUpdate(update_tracker_);
    ASSERT_THAT(update, Optional(EquivToProto(expected)));

    ASSERT_OK(clone->ApplyUpdateProto(update.value()));
    EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
  }
}

TEST_F(SimpleModelTest, UpdateObjectiveDirectionUnchanged) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_maximize(
      kPrimaryObjectiveId);  // Was already maximize, should be a no-op.
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateObjectiveOffset) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_objective_offset(kPrimaryObjectiveId, 12.0);
  EXPECT_DOUBLE_EQ(storage_.objective_offset(kPrimaryObjectiveId), 12.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates { offset_update: 12.0 }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateEmptyOnIdenticalObjectiveOffset) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_objective_offset(kPrimaryObjectiveId, 3.5);
  EXPECT_DOUBLE_EQ(storage_.objective_offset(kPrimaryObjectiveId), 3.5);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateAddAuxiliaryObjective) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  const AuxiliaryObjectiveId e = storage_.AddAuxiliaryObjective(3, "e");
  storage_.set_objective_offset(e, 16.0);
  storage_.set_linear_objective_coefficient(e, y_, 5.0);
  EXPECT_EQ(storage_.num_auxiliary_objectives(), 2);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    auxiliary_objectives_updates {
      new_objectives {
        key: 1
        value: {
          maximize: false
          priority: 3
          offset: 16.0
          linear_coefficients {
            ids: [ 1 ]
            values: [ 5.0 ]
          }
          name: "e"
        }
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateChangeAuxiliaryObjectiveDirection) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_maximize(o_);
  EXPECT_TRUE(storage_.is_maximize(o_));
  // TODO(b/185892947): EquivToProto() does not differentiate unset from set to
  // false. We should stop using it here and elsewhere.
  {
    ModelUpdateProto expected;
    (*expected.mutable_auxiliary_objectives_updates()
          ->mutable_objective_updates())[0]
        .set_direction_update(true);
    const std::optional<ModelUpdateProto> update =
        storage_.ExportModelUpdate(update_tracker_);
    ASSERT_THAT(update, Optional(EquivToProto(expected)));

    ASSERT_OK(clone->ApplyUpdateProto(update.value()));
    EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
  }

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_minimize(o_);
  EXPECT_FALSE(storage_.is_maximize(o_));
  {
    ModelUpdateProto expected;
    (*expected.mutable_auxiliary_objectives_updates()
          ->mutable_objective_updates())[0]
        .set_direction_update(false);
    const std::optional<ModelUpdateProto> update =
        storage_.ExportModelUpdate(update_tracker_);
    ASSERT_THAT(update, Optional(EquivToProto(expected)));

    ASSERT_OK(clone->ApplyUpdateProto(update.value()));
    EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
  }
}

TEST_F(SimpleModelTest, UpdateAuxiliaryObjectivePriority) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_objective_priority(o_, 42);

  ModelUpdateProto expected;
  (*expected.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[o_.value()]
      .set_priority_update(42);
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateEmptyOnIdenticalAuxiliaryObjectivePriority) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_objective_priority(o_, 1);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateAuxiliaryObjectiveOffset) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_objective_offset(o_, 16.0);

  ModelUpdateProto expected;
  (*expected.mutable_auxiliary_objectives_updates()
        ->mutable_objective_updates())[o_.value()]
      .set_offset_update(16.0);
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateEmptyOnIdenticalAuxiliaryObjectiveOffset) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_objective_offset(o_, 3.0);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, UpdateAuxiliaryObjectiveCoefficientWasZero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(o_, y_, 7.0);
  EXPECT_DOUBLE_EQ(storage_.linear_objective_coefficient(o_, y_), 7.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    auxiliary_objectives_updates {
      objective_updates {
        key: 0
        value {
          linear_coefficients {
            ids: [ 1 ]
            values: [ 7.0 ]
          }
        }
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateAuxiliaryObjectiveCoefficientWasNonzero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(o_, x_, 7.0);
  EXPECT_DOUBLE_EQ(storage_.linear_objective_coefficient(o_, x_), 7.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    auxiliary_objectives_updates {
      objective_updates {
        key: 0
        value {
          linear_coefficients {
            ids: [ 0 ]
            values: [ 7.0 ]
          }
        }
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateAuxiliaryObjectiveCoefficientSetToZero) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(o_, x_, 0.0);
  EXPECT_DOUBLE_EQ(storage_.linear_objective_coefficient(o_, x_), 0.0);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    auxiliary_objectives_updates {
      objective_updates {
        key: 0
        value {
          linear_coefficients {
            ids: [ 0 ]
            values: [ 0.0 ]
          }
        }
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest,
       UpdateEmptyOnIdenticalNonzeroAuxiliaryObjectiveCoefficient) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(o_, x_, 2.0);
  EXPECT_DOUBLE_EQ(storage_.linear_objective_coefficient(o_, x_), 2.0);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest,
       UpdateEmptyOnIdenticalZeroAuxiliaryObjectiveCoefficient) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_objective_coefficient(o_, y_, 0.0);
  EXPECT_DOUBLE_EQ(storage_.linear_objective_coefficient(o_, y_), 0.0);
  EXPECT_EQ(storage_.ExportModelUpdate(update_tracker_), std::nullopt);
}

TEST_F(SimpleModelTest, ClonePreservesNextAuxiliaryObjectiveId) {
  storage_.ensure_next_auxiliary_objective_id_at_least(AuxiliaryObjectiveId(9));
  EXPECT_EQ(storage_.Clone()->next_auxiliary_objective_id(),
            AuxiliaryObjectiveId(9));
}

void CheckInMemoryModelAfterDeletingX(const ModelStorage& storage) {
  const VariableId y(1);
  EXPECT_DOUBLE_EQ(storage.variable_lower_bound(y), 0.0);
  EXPECT_DOUBLE_EQ(storage.variable_upper_bound(y), 1.0);
  EXPECT_TRUE(storage.is_variable_integer(y));
  EXPECT_EQ(storage.variable_name(y), "y");
  EXPECT_EQ(storage.num_variables(), 1);
  EXPECT_EQ(storage.next_variable_id(), VariableId(2));
  EXPECT_FALSE(storage.has_variable(VariableId(0)));
  EXPECT_TRUE(storage.has_variable(VariableId(1)));
  EXPECT_FALSE(storage.has_variable(VariableId(2)));
  EXPECT_THAT(storage.variables(), UnorderedElementsAre(VariableId(1)));

  EXPECT_THAT(storage.linear_constraint_matrix(),
              UnorderedElementsAre(
                  std::make_tuple(LinearConstraintId(0), VariableId(1), 1.0),
                  std::make_tuple(LinearConstraintId(1), VariableId(1), 2.0)));

  EXPECT_THAT(storage.linear_objective(AuxiliaryObjectiveId(0)), IsEmpty());
}

ModelProto ModelProtoAfterDeletingX() {
  return ParseTextProtoOrDie(R"pb(
    name: "simple_model_test"
    variables: {
      ids: [ 1 ]
      lower_bounds: [ 0.0 ]
      upper_bounds: [ 1.0 ]
      integers: [ true ]
      names: [ "y" ]
    }
    objective: {
      maximize: true
      offset: 3.5
      linear_coefficients {
        ids: [ 1 ]
        values: [ 2.0 ]
      }
      quadratic_coefficients {
        row_ids: [ 1 ]
        column_ids: [ 1 ]
        coefficients: [ 3.5 ]
      }
      name: "primary"
    }
    auxiliary_objectives {
      key: 0
      value: { maximize: false priority: 1 offset: 3.0 name: "secondary" }
    }
    linear_constraints: {
      ids: [ 0, 1 ]
      lower_bounds: [ -infinity, 0.5 ],
      upper_bounds: [ 1.5, infinity ]
      names: [ "c", "d" ]
    }
    linear_constraint_matrix {
      row_ids: [ 0, 1 ]
      column_ids: [ 1, 1 ],
      coefficients: [ 1.0, 2.0 ]
    }
  )pb");
}

TEST_F(SimpleModelTest, DeleteVariableNotYetExtracted) {
  storage_.DeleteVariable(x_);
  CheckInMemoryModelAfterDeletingX(storage_);
  const ModelProto model_proto = storage_.ExportModel();
  ASSERT_OK(ValidateModel(model_proto));
  const ModelProto expected = ModelProtoAfterDeletingX();
  EXPECT_THAT(model_proto, EquivToProto(expected));
}

TEST_F(SimpleModelTest, DeleteVariableAlreadyExtracted) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.DeleteVariable(x_);
  CheckInMemoryModelAfterDeletingX(storage_);
  const ModelProto model_proto = storage_.ExportModel();
  ASSERT_OK(ValidateModel(model_proto));
  const ModelProto expected = ModelProtoAfterDeletingX();
  EXPECT_THAT(model_proto, EquivToProto(expected));
}

TEST_F(SimpleModelTest, UpdateDeleteVariableNotYetExtracted) {
  // For this test, storage_.Checkpoint() is not called; so the clone must start
  // empty for the update to make sense.
  ModelStorage clone(storage_.name(),
                     storage_.objective_name(kPrimaryObjectiveId));

  storage_.DeleteVariable(x_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    new_variables: {
      ids: [ 1 ]
      lower_bounds: [ 0.0 ]
      upper_bounds: [ 1.0 ]
      integers: [ true ]
      names: [ "y" ]
    }
    objective_updates: {
      direction_update: true
      offset_update: 3.5
      linear_coefficients {
        ids: [ 1 ]
        values: [ 2.0 ]
      }
      quadratic_coefficients {
        row_ids: [ 1 ]
        column_ids: [ 1 ]
        coefficients: [ 3.5 ]
      }
    }
    auxiliary_objectives_updates {
      new_objectives {
        key: 0
        value { maximize: false priority: 1 offset: 3.0 name: "secondary" }
      }
    }
    new_linear_constraints: {
      ids: [ 0, 1 ]
      lower_bounds: [ -infinity, 0.5 ],
      upper_bounds: [ 1.5, infinity ]
      names: [ "c", "d" ]
    }
    linear_constraint_matrix_updates {
      row_ids: [ 0, 1 ]
      column_ids: [ 1, 1 ],
      coefficients: [ 1.0, 2.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone.ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone.ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateDeleteVariableAlreadyExtracted) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.DeleteVariable(x_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_variable_ids: [ 0 ]
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

void CheckInMemoryModelAfterDeletingC(const ModelStorage& storage) {
  const LinearConstraintId d(1);
  EXPECT_DOUBLE_EQ(storage.linear_constraint_lower_bound(d), 0.5);
  EXPECT_DOUBLE_EQ(storage.linear_constraint_upper_bound(d), kInf);
  EXPECT_EQ(storage.linear_constraint_name(d), "d");

  EXPECT_EQ(storage.num_linear_constraints(), 1);
  EXPECT_EQ(storage.next_linear_constraint_id(), LinearConstraintId(2));
  EXPECT_FALSE(storage.has_linear_constraint(LinearConstraintId(0)));
  EXPECT_TRUE(storage.has_linear_constraint(LinearConstraintId(1)));
  EXPECT_FALSE(storage.has_linear_constraint(LinearConstraintId(2)));

  EXPECT_THAT(storage.LinearConstraints(),
              UnorderedElementsAre(LinearConstraintId(1)));

  EXPECT_THAT(storage.linear_constraint_matrix(),
              UnorderedElementsAre(
                  std::make_tuple(LinearConstraintId(1), VariableId(1), 2.0)));
}

ModelProto ModelProtoAfterDeletingC() {
  return ParseTextProtoOrDie(R"pb(
    name: "simple_model_test"
    variables: {
      ids: [ 0, 1 ]
      lower_bounds: [ -infinity, 0.0 ]
      upper_bounds: [ infinity, 1.0 ]
      integers: [ false, true ]
      names: [ "x", "y" ]
    }
    objective: {
      maximize: true
      offset: 3.5
      linear_coefficients: {
        ids: [ 1 ]
        values: [ 2.0 ]
      }
      quadratic_coefficients: {
        row_ids: [ 0, 1 ]
        column_ids: [ 1, 1 ]
        coefficients: [ 1.5, 3.5 ]
      }
      name: "primary"
    }
    auxiliary_objectives {
      key: 0
      value: {
        maximize: false
        priority: 1
        offset: 3.0
        linear_coefficients {
          ids: [ 0 ]
          values: [ 2.0 ]
        }
        name: "secondary"
      }
    }
    linear_constraints: {
      ids: [ 1 ]
      lower_bounds: [ 0.5 ],
      upper_bounds: [ infinity ]
      names: [ "d" ]
    }
    linear_constraint_matrix {
      row_ids: [ 1 ]
      column_ids: [ 1 ],
      coefficients: [ 2.0 ]
    }
  )pb");
}

TEST_F(SimpleModelTest, DeleteLinearConstraintNotYetExtracted) {
  storage_.DeleteLinearConstraint(c_);
  CheckInMemoryModelAfterDeletingC(storage_);
  const ModelProto model_proto = storage_.ExportModel();
  ASSERT_OK(ValidateModel(model_proto));
  const ModelProto expected = ModelProtoAfterDeletingC();
  EXPECT_THAT(model_proto, EquivToProto(expected));
}

TEST_F(SimpleModelTest, DeleteLinearConstraintAlreadyExtracted) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.DeleteLinearConstraint(c_);
  CheckInMemoryModelAfterDeletingC(storage_);
  const ModelProto model_proto = storage_.ExportModel();
  ASSERT_OK(ValidateModel(model_proto));
  const ModelProto expected = ModelProtoAfterDeletingC();
  EXPECT_THAT(model_proto, EquivToProto(expected));
}

TEST_F(SimpleModelTest, UpdateDeleteLinearConstraintNotYetExtracted) {
  // For this test, storage_.Checkpoint() is not called; so the clone must start
  // empty for the update to make sense.
  ModelStorage clone(storage_.name(),
                     storage_.objective_name(kPrimaryObjectiveId));

  storage_.DeleteLinearConstraint(c_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    new_variables: {
      ids: [ 0, 1 ]
      lower_bounds: [ -inf, 0.0 ]
      upper_bounds: [ inf, 1.0 ]
      integers: [ false, true ]
      names: [ "x", "y" ]
    }
    objective_updates: {
      direction_update: true
      offset_update: 3.5
      linear_coefficients {
        ids: [ 1 ]
        values: [ 2.0 ]
      }
      quadratic_coefficients {
        row_ids: [ 0, 1 ]
        column_ids: [ 1, 1 ]
        coefficients: [ 1.5, 3.5 ]
      }
    }
    auxiliary_objectives_updates {
      new_objectives {
        key: 0
        value: {
          maximize: false
          priority: 1
          offset: 3.0
          linear_coefficients {
            ids: [ 0 ]
            values: [ 2.0 ]
          }
          name: "secondary"
        }
      }
    }
    new_linear_constraints: {
      ids: [ 1 ]
      lower_bounds: [ 0.5 ],
      upper_bounds: [ infinity ]
      names: [ "d" ]
    }
    linear_constraint_matrix_updates {
      row_ids: [ 1 ]
      column_ids: [ 1 ]
      coefficients: [ 2.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone.ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone.ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateDeleteLinearConstraintAlreadyExtracted) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.DeleteLinearConstraint(c_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_linear_constraint_ids: [ 0 ]
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

ModelProto ModelProtoAfterDeletingO() {
  return ParseTextProtoOrDie(R"pb(
    name: "simple_model_test"
    variables: {
      ids: [ 0, 1 ]
      lower_bounds: [ -infinity, 0.0 ]
      upper_bounds: [ infinity, 1.0 ]
      integers: [ false, true ]
      names: [ "x", "y" ]
    }
    objective: {
      maximize: true
      offset: 3.5
      linear_coefficients {
        ids: [ 1 ]
        values: [ 2.0 ]
      }
      quadratic_coefficients {
        row_ids: [ 0, 1 ]
        column_ids: [ 1, 1 ]
        coefficients: [ 1.5, 3.5 ]
      }
      name: "primary"
    }
    linear_constraints: {
      ids: [ 0, 1 ]
      lower_bounds: [ -infinity, 0.5 ],
      upper_bounds: [ 1.5, infinity ]
      names: [ "c", "d" ]
    }
    linear_constraint_matrix {
      row_ids: [ 0, 0, 1 ]
      column_ids: [ 0, 1, 1 ],
      coefficients: [ 1.0, 1.0, 2.0 ]
    }
  )pb");
}

TEST_F(SimpleModelTest, DeleteAuxiliaryObjectiveNotYetExtracted) {
  storage_.DeleteAuxiliaryObjective(o_);
  EXPECT_THAT(storage_.AuxiliaryObjectives(), IsEmpty());
  const ModelProto model_proto = storage_.ExportModel();
  ASSERT_OK(ValidateModel(model_proto));
  EXPECT_THAT(model_proto, EquivToProto(ModelProtoAfterDeletingO()));
}

TEST_F(SimpleModelTest, DeleteAuxiliaryObjectiveAlreadyExtracted) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.DeleteAuxiliaryObjective(o_);
  EXPECT_THAT(storage_.AuxiliaryObjectives(), IsEmpty());
  const ModelProto model_proto = storage_.ExportModel();
  ASSERT_OK(ValidateModel(model_proto));
  EXPECT_THAT(model_proto, EquivToProto(ModelProtoAfterDeletingO()));
}

TEST_F(SimpleModelTest, UpdateDeleteAuxiliaryObjectiveNotYetExtracted) {
  // For this test, storage_.Checkpoint() is not called; so the clone must start
  // empty for the update to make sense.
  ModelStorage clone(storage_.name(),
                     storage_.objective_name(kPrimaryObjectiveId));

  storage_.DeleteAuxiliaryObjective(o_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    new_variables: {
      ids: [ 0, 1 ]
      lower_bounds: [ -inf, 0.0 ]
      upper_bounds: [ inf, 1.0 ]
      integers: [ false, true ]
      names: [ "x", "y" ]
    }
    objective_updates: {
      direction_update: true
      offset_update: 3.5
      linear_coefficients {
        ids: [ 1 ]
        values: [ 2.0 ]
      }
      quadratic_coefficients {
        row_ids: [ 0, 1 ]
        column_ids: [ 1, 1 ]
        coefficients: [ 1.5, 3.5 ]
      }
    }
    new_linear_constraints: {
      ids: [ 0, 1 ]
      lower_bounds: [ -infinity, 0.5 ],
      upper_bounds: [ 1.5, infinity ]
      names: [ "c", "d" ]
    }
    linear_constraint_matrix_updates {
      row_ids: [ 0, 0, 1 ]
      column_ids: [ 0, 1, 1 ],
      coefficients: [ 1.0, 1.0, 2.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone.ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone.ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, UpdateDeleteAuxiliaryObjectiveAlreadyExtracted) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.DeleteAuxiliaryObjective(o_);
  ModelUpdateProto expected;
  expected.mutable_auxiliary_objectives_updates()->add_deleted_objective_ids(
      o_.value());
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, ModifyThenDeleteVariable) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_variable_lower_bound(y_, -3.0);
  storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, y_, 4.0);
  storage_.set_linear_constraint_coefficient(d_, y_, 7.0);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, x_,
                                               3.4);
  storage_.set_quadratic_objective_coefficient(kPrimaryObjectiveId, x_, y_,
                                               3.2);

  storage_.set_variable_lower_bound(x_, -4.0);
  storage_.set_variable_upper_bound(x_, 10.0);
  storage_.set_variable_as_integer(x_);
  storage_.set_linear_objective_coefficient(kPrimaryObjectiveId, x_, 5.0);
  storage_.set_linear_constraint_coefficient(d_, x_, 12.0);

  storage_.DeleteVariable(x_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_variable_ids: [ 0 ]
    variable_updates {
      lower_bounds {
        ids: [ 1 ]
        values: [ -3.0 ]
      }
    }
    objective_updates {
      linear_coefficients {
        ids: [ 1 ]
        values: [ 4.0 ]
      }
    }
    linear_constraint_matrix_updates {
      row_ids: [ 1 ]
      column_ids: [ 1 ],
      coefficients: [ 7.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, ModifyThenDeleteLinearConstraint) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_linear_constraint_lower_bound(d_, -3.0);
  storage_.set_linear_constraint_coefficient(d_, y_, 7.0);

  storage_.set_linear_constraint_lower_bound(c_, -4.0);
  storage_.set_linear_constraint_upper_bound(c_, 10.0);
  storage_.set_linear_constraint_coefficient(c_, x_, 12.0);

  storage_.DeleteLinearConstraint(c_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_linear_constraint_ids: [ 0 ]
    linear_constraint_updates {
      lower_bounds {
        ids: [ 1 ]
        values: [ -3.0 ]
      }
    }
    linear_constraint_matrix_updates {
      row_ids: [ 1 ]
      column_ids: [ 1 ],
      coefficients: [ 7.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, ModifyThenDeleteAuxiliaryObjective) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.set_maximize(o_);
  storage_.set_objective_priority(o_, 12);
  storage_.set_objective_offset(o_, 5.0);
  storage_.set_linear_objective_coefficient(o_, y_, 4.0);

  storage_.DeleteAuxiliaryObjective(o_);

  ModelUpdateProto expected;
  expected.mutable_auxiliary_objectives_updates()->add_deleted_objective_ids(
      o_.value());
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, DeleteVariableWithNewConstraint) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  const LinearConstraintId e = storage_.AddLinearConstraint(0.0, 1.0, "e");
  storage_.set_linear_constraint_coefficient(e, x_, 1.0);
  storage_.set_linear_constraint_coefficient(e, y_, 1.0);

  storage_.DeleteVariable(x_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_variable_ids: [ 0 ]
    new_linear_constraints {
      ids: [ 2 ]
      lower_bounds: [ 0.0 ]
      upper_bounds: [ 1.0 ]
      names: [ "e" ]
    }
    linear_constraint_matrix_updates {
      row_ids: [ 2 ]
      column_ids: [ 1 ],
      coefficients: [ 1.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, DeleteVariableWithNewAuxiliaryObjective) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  const AuxiliaryObjectiveId e = storage_.AddAuxiliaryObjective(2, "e");
  storage_.set_linear_objective_coefficient(e, x_, 1.0);
  storage_.set_linear_objective_coefficient(e, y_, 1.0);

  storage_.DeleteVariable(y_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_variable_ids: [ 1 ]
    auxiliary_objectives_updates {
      new_objectives {
        key: 1
        value {
          maximize: false
          priority: 2
          linear_coefficients {
            ids: [ 0 ]
            values: [ 1.0 ]
          }
          name: "e"
        }
      }
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, DeleteLinearConstraintWithNewVariable) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  const VariableId z = storage_.AddVariable(0.0, 1.0, false, "z");
  storage_.set_linear_constraint_coefficient(c_, z, 1.0);
  storage_.set_linear_constraint_coefficient(d_, z, 1.0);

  storage_.DeleteLinearConstraint(c_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_linear_constraint_ids: [ 0 ]
    new_variables {
      ids: [ 2 ]
      lower_bounds: [ 0.0 ]
      upper_bounds: [ 1.0 ]
      integers: [ false ]
      names: [ "z" ]
    }
    linear_constraint_matrix_updates {
      row_ids: [ 1 ]
      column_ids: [ 2 ],
      coefficients: [ 1.0 ]
    }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, DeleteAuxiliaryObjectiveWithNewVariable) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  const VariableId z = storage_.AddVariable(0.0, 1.0, false, "z");
  storage_.set_linear_objective_coefficient(o_, z, 1.0);

  storage_.DeleteAuxiliaryObjective(o_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    new_variables {
      ids: [ 2 ]
      lower_bounds: [ 0.0 ]
      upper_bounds: [ 1.0 ]
      integers: [ false ]
      names: [ "z" ]
    }
    auxiliary_objectives_updates { deleted_objective_ids: [ 0 ] }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, DeleteAllVariablesAndConstraintsUpdateFromCheckpoint) {
  std::unique_ptr<ModelStorage> clone = storage_.Clone();

  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.DeleteLinearConstraint(c_);
  storage_.DeleteLinearConstraint(d_);
  storage_.DeleteVariable(x_);
  storage_.DeleteVariable(y_);
  storage_.DeleteAuxiliaryObjective(o_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    deleted_variable_ids: [ 0, 1 ]
    deleted_linear_constraint_ids: [ 0, 1 ]
    auxiliary_objectives_updates { deleted_objective_ids: [ 0 ] }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, DeleteAllVariablesAndConstraintsUpdateNoCheckpoint) {
  // For this test, storage_.Checkpoint() is not called; so the clone must start
  // empty for the update to make sense.
  ModelStorage clone(storage_.name(),
                     storage_.objective_name(kPrimaryObjectiveId));

  storage_.DeleteLinearConstraint(c_);
  storage_.DeleteLinearConstraint(d_);
  storage_.DeleteVariable(x_);
  storage_.DeleteVariable(y_);
  storage_.DeleteAuxiliaryObjective(o_);
  const ModelUpdateProto expected = ParseTextProtoOrDie(R"pb(
    objective_updates: { direction_update: true offset_update: 3.5 }
  )pb");
  const std::optional<ModelUpdateProto> update =
      storage_.ExportModelUpdate(update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone.ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone.ExportModel(), EqualsProto(storage_.ExportModel()));
}

TEST_F(SimpleModelTest, DeleteAllVariablesAndConstraintsFromCheckpointModel) {
  storage_.AdvanceCheckpoint(update_tracker_);
  storage_.DeleteLinearConstraint(c_);
  storage_.DeleteLinearConstraint(d_);
  storage_.DeleteVariable(x_);
  storage_.DeleteVariable(y_);
  storage_.DeleteAuxiliaryObjective(o_);
  const ModelProto expected = ParseTextProtoOrDie(R"pb(
    name: "simple_model_test"
    objective: { maximize: true offset: 3.5 name: "primary" }
  )pb");
  EXPECT_THAT(storage_.ExportModel(), EquivToProto(expected));
}

TEST_F(SimpleModelTest, DeleteAllVariablesAndConstraintsNoCheckpointModel) {
  storage_.DeleteLinearConstraint(c_);
  storage_.DeleteLinearConstraint(d_);
  storage_.DeleteVariable(x_);
  storage_.DeleteVariable(y_);
  storage_.DeleteAuxiliaryObjective(o_);
  const ModelProto expected = ParseTextProtoOrDie(R"pb(
    name: "simple_model_test"
    objective: { maximize: true offset: 3.5 name: "primary" }
  )pb");
  EXPECT_THAT(storage_.ExportModel(), EquivToProto(expected));
}

TEST(ModelStorageTest, ReadEmptyColumn) {
  ModelStorage storage("model");
  const VariableId x = storage.AddVariable("x");
  EXPECT_THAT(storage.linear_constraints_with_variable(x), IsEmpty());
}

TEST(ModelStorageTest, ReadEmptyRow) {
  ModelStorage storage("model");
  const LinearConstraintId c = storage.AddLinearConstraint("c");
  EXPECT_THAT(storage.variables_in_linear_constraint(c), IsEmpty());
}

TEST(ModelStorageTest, TestExportModelUpdateEmptyColumn) {
  ModelStorage storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.AddVariable("x");
  ModelUpdateProto expected;
  auto new_vars = expected.mutable_new_variables();
  new_vars->add_ids(0);
  new_vars->add_lower_bounds(-kInf);
  new_vars->add_upper_bounds(kInf);
  new_vars->add_integers(false);
  new_vars->add_names("x");
  const std::optional<ModelUpdateProto> update =
      storage.ExportModelUpdate(tracker);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ModelStorage clone(storage.name());
  ASSERT_OK(clone.ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone.ExportModel(), EqualsProto(storage.ExportModel()));
}

TEST(ModelStorageTest, TestExportModelUpdateEmptyRow) {
  ModelStorage storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.AddLinearConstraint("c");
  ModelUpdateProto expected;
  auto new_cons = expected.mutable_new_linear_constraints();
  new_cons->add_ids(0);
  new_cons->add_lower_bounds(-kInf);
  new_cons->add_upper_bounds(kInf);
  new_cons->add_names("c");
  const std::optional<ModelUpdateProto> update =
      storage.ExportModelUpdate(tracker);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ModelStorage clone(storage.name());
  ASSERT_OK(clone.ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone.ExportModel(), EqualsProto(storage.ExportModel()));
}

TEST(ModelStorageUpdateTrackerTest, TrackerOnEmptyModel) {
  ModelStorage storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  EXPECT_EQ(storage.ExportModelUpdate(tracker), std::nullopt);
}

TEST(ModelStorageUpdateTrackerTest, TrackerAddedBeforeAnyChange) {
  ModelStorage storage("model");

  const UpdateTrackerId tracker = storage.NewUpdateTracker();

  const VariableId x = storage.AddVariable("x");

  {
    const std::optional<ModelUpdateProto> update =
        storage.ExportModelUpdate(tracker);

    ModelUpdateProto expected;
    expected.mutable_new_variables()->add_ids(x.value());
    expected.mutable_new_variables()->add_lower_bounds(-kInf);
    expected.mutable_new_variables()->add_upper_bounds(kInf);
    expected.mutable_new_variables()->add_integers(false);
    expected.mutable_new_variables()->add_names("x");
    EXPECT_THAT(update, Optional(EquivToProto(expected)));
  }

  // Updating the checkpoint should reset the returned update.
  storage.AdvanceCheckpoint(tracker);
  EXPECT_EQ(storage.ExportModelUpdate(tracker), std::nullopt);

  // Do a new change to validate that the checkpoint is used.
  storage.set_variable_as_integer(x);

  {
    const std::optional<ModelUpdateProto> update =
        storage.ExportModelUpdate(tracker);

    ModelUpdateProto expected;
    expected.mutable_variable_updates()->mutable_integers()->add_ids(x.value());
    expected.mutable_variable_updates()->mutable_integers()->add_values(true);
    EXPECT_THAT(update, Optional(EquivToProto(expected)));
  }
}

TEST(ModelStorageUpdateTrackerTest, TrackerAddedAfterSomeChanges) {
  ModelStorage storage("model");

  const VariableId x = storage.AddVariable("x");

  const UpdateTrackerId tracker = storage.NewUpdateTracker();

  EXPECT_EQ(storage.ExportModelUpdate(tracker), std::nullopt);

  storage.set_variable_as_integer(x);

  const std::optional<ModelUpdateProto> update =
      storage.ExportModelUpdate(tracker);

  EXPECT_THAT(update, Optional(EquivToProto(R"pb(variable_updates {
                                                   integers {
                                                     ids: [ 0 ]
                                                     values: [ true ]
                                                   }
                                                 })pb")));
}

TEST(ModelStorageUpdateTrackerTest, UpdatesOnSingleTrackerWithoutCheckpoint) {
  ModelStorage storage("model");

  const VariableId x = storage.AddVariable("x");

  const UpdateTrackerId tracker = storage.NewUpdateTracker();

  storage.set_variable_as_integer(x);

  EXPECT_THAT(storage.ExportModelUpdate(tracker),
              Optional(EquivToProto(R"pb(variable_updates {
                                           integers {
                                             ids: [ 0 ]
                                             values: [ true ]
                                           }
                                         })pb")));

  storage.set_variable_lower_bound(x, 3.0);
  storage.set_variable_upper_bound(x, 5.0);

  EXPECT_THAT(storage.ExportModelUpdate(tracker),
              Optional(EquivToProto(R"pb(variable_updates {
                                           integers {
                                             ids: [ 0 ]
                                             values: [ true ]
                                           }
                                           lower_bounds {
                                             ids: [ 0 ]
                                             values: [ 3 ]
                                           }
                                           upper_bounds {
                                             ids: [ 0 ]
                                             values: [ 5 ]
                                           }
                                         })pb")));
}

TEST(ModelStorageUpdateTrackerTest, MergeOfUpdatesWithMultipleTrackers) {
  // Note that this test dates back from the time where the update trackers used
  // a shared data structure for tracking model changes and thus needed to merge
  // multiple ModelUpdateProto. It is kept as-is in case we return to this
  // strategy. It also tests some advance trackers usage anyway.
  ModelStorage storage("model");

  const VariableId x = storage.AddVariable("x");

  // Create two trackers sharing the same checkpoint.
  const UpdateTrackerId tracker1 = storage.NewUpdateTracker();
  const UpdateTrackerId tracker2 = storage.NewUpdateTracker();

  storage.set_variable_as_integer(x);

  // This third tracker will have a more advanced checkpoint.
  const UpdateTrackerId tracker3 = storage.NewUpdateTracker();

  storage.set_variable_lower_bound(x, 3.0);

  // This fourth tracker will have a even more advanced checkpoint.
  const UpdateTrackerId tracker4 = storage.NewUpdateTracker();

  storage.set_variable_upper_bound(x, 5.0);

  // This fifth tracker will have an empty updates_.
  const UpdateTrackerId tracker5 = storage.NewUpdateTracker();

  // At this point we expect the following values of UpdateTracker::updates_:
  //   * tracker1 and tracker2:
  //     - x -> integer
  //     - x.lower_bound = 3
  //     - x.upper_bound = 5
  //   * tracker3:
  //     - x.lower_bound = 3
  //     - x.upper_bound = 5
  //   * tracker4:
  //     - x.upper_bound = 5
  //   * tracker5 is empty.

  // Exporting tracker2 should merge all three updates and replace the set of
  // updates_ of both tracker1 and tracker2 with the result.
  {
    const std::string expected = R"pb(variable_updates {
                                        integers {
                                          ids: [ 0 ]
                                          values: [ true ]
                                        }
                                        lower_bounds {
                                          ids: [ 0 ]
                                          values: [ 3 ]
                                        }
                                        upper_bounds {
                                          ids: [ 0 ]
                                          values: [ 5 ]
                                        }
                                      })pb";
    EXPECT_THAT(storage.ExportModelUpdate(tracker1),
                Optional(EquivToProto(expected)));
    EXPECT_THAT(storage.ExportModelUpdate(tracker2),
                Optional(EquivToProto(expected)));
  }

  EXPECT_EQ(storage.ExportModelUpdate(tracker5), std::nullopt);

  EXPECT_THAT(storage.ExportModelUpdate(tracker4),
              Optional(EquivToProto(R"pb(variable_updates {
                                           upper_bounds {
                                             ids: [ 0 ]
                                             values: [ 5 ]
                                           }
                                         })pb")));

  EXPECT_THAT(storage.ExportModelUpdate(tracker3),
              Optional(EquivToProto(R"pb(variable_updates {
                                           lower_bounds {
                                             ids: [ 0 ]
                                             values: [ 3 ]
                                           }
                                           upper_bounds {
                                             ids: [ 0 ]
                                             values: [ 5 ]
                                           }
                                         })pb")));

  storage.AdvanceCheckpoint(tracker3);

  storage.set_variable_upper_bound(x, 10.0);

  // Now trackers 1 and 2 have not updated their checkpoint but the new update
  // should be merged into the existing merge and shared between them.
  {
    const std::string expected = R"pb(variable_updates {
                                        integers {
                                          ids: [ 0 ]
                                          values: [ true ]
                                        }
                                        lower_bounds {
                                          ids: [ 0 ]
                                          values: [ 3 ]
                                        }
                                        upper_bounds {
                                          ids: [ 0 ]
                                          values: [ 10 ]
                                        }
                                      })pb";
    EXPECT_THAT(storage.ExportModelUpdate(tracker1),
                Optional(EquivToProto(expected)));
    EXPECT_THAT(storage.ExportModelUpdate(tracker2),
                Optional(EquivToProto(expected)));
  }

  // Trackers 3 and 5 should share the new update.
  {
    const std::string expected = R"pb(variable_updates {
                                        upper_bounds {
                                          ids: [ 0 ]
                                          values: [ 10 ]
                                        }
                                      })pb";
    EXPECT_THAT(storage.ExportModelUpdate(tracker3),
                Optional(EquivToProto(expected)));
    EXPECT_THAT(storage.ExportModelUpdate(tracker5),
                Optional(EquivToProto(expected)));
  }

  EXPECT_THAT(storage.ExportModelUpdate(tracker4),
              Optional(EquivToProto(R"pb(variable_updates {
                                           upper_bounds {
                                             ids: [ 0 ]
                                             values: [ 10 ]
                                           }
                                         })pb")));
}

TEST(ModelStorageUpdateTrackerTest, TrackerDestruction) {
  ModelStorage storage("model");

  const VariableId x = storage.AddVariable("x");

  // Create two trackers sharing the same checkpoint.
  const UpdateTrackerId tracker1 = storage.NewUpdateTracker();
  const UpdateTrackerId tracker2 = storage.NewUpdateTracker();

  storage.set_variable_as_integer(x);

  const std::string expected = R"pb(variable_updates {
                                      integers {
                                        ids: [ 0 ]
                                        values: [ true ]
                                      }
                                    })pb";
  EXPECT_THAT(storage.ExportModelUpdate(tracker1),
              Optional(EquivToProto(expected)));
  EXPECT_THAT(storage.ExportModelUpdate(tracker2),
              Optional(EquivToProto(expected)));

  // Destroy one tracker.
  storage.DeleteUpdateTracker(tracker1);

  // Checkpointing should not crash. It would if the destroyed tracker has not
  // been unregistered by its destructor.
  storage.AdvanceCheckpoint(tracker2);
}

TEST(ModelStorageUpdateTrackerDeathTest, DeleteTwice) {
  ModelStorage storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.DeleteUpdateTracker(tracker);
  EXPECT_DEATH_IF_SUPPORTED(storage.DeleteUpdateTracker(tracker),
                            "does not exist");
}

TEST(ModelStorageUpdateTrackerDeathTest, ExportModelUpdateAfterDestruction) {
  ModelStorage storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.DeleteUpdateTracker(tracker);
  EXPECT_DEATH_IF_SUPPORTED(storage.ExportModelUpdate(tracker),
                            "does not exist");
}

TEST(ModelStorageUpdateTrackerDeathTest, CheckpointAfterDestruction) {
  ModelStorage storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.DeleteUpdateTracker(tracker);
  EXPECT_DEATH_IF_SUPPORTED(storage.AdvanceCheckpoint(tracker),
                            "does not exist");
}

////////////////////////////////////////////////////////////////////////////////
// Atomic constraints
////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TYPED_TEST_SUITE_P(QuadraticConstraintTestSuite,
                               AtomicConstraintTest, QuadraticConstraintId);

INSTANTIATE_TYPED_TEST_SUITE_P(SecondOrderConeConstraintTestSuite,
                               AtomicConstraintTest,
                               SecondOrderConeConstraintId);

INSTANTIATE_TYPED_TEST_SUITE_P(Sos1ConstraintTestSuite, AtomicConstraintTest,
                               Sos1ConstraintId);
INSTANTIATE_TYPED_TEST_SUITE_P(Sos2ConstraintTestSuite, AtomicConstraintTest,
                               Sos2ConstraintId);

INSTANTIATE_TYPED_TEST_SUITE_P(IndicatorConstraintTestSuite,
                               AtomicConstraintTest, IndicatorConstraintId);

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
