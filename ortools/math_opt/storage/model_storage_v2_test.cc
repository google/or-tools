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

#include "ortools/math_opt/storage/model_storage_v2.h"

#include <limits>
#include <memory>
#include <optional>
#include <tuple>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/math_opt/constraints/indicator/storage.h"
#include "ortools/math_opt/constraints/indicator/storage_test_helper.h"  // IWYU pragma: keep
#include "ortools/math_opt/constraints/quadratic/storage.h"
#include "ortools/math_opt/constraints/quadratic/storage_test_helper.h"  // IWYU pragma: keep
#include "ortools/math_opt/constraints/second_order_cone/storage_test_helper.h"  // IWYU pragma: keep
#include "ortools/math_opt/constraints/sos/storage_test_helper.h"  // IWYU pragma: keep
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/atomic_constraint_testing.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::EqualsProto;
using ::testing::FieldsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(ModelStorageV2Test, ModelName) {
  ModelStorageV2 storage("simple_model_test");
  EXPECT_EQ(storage.name(), "simple_model_test");
}

TEST(ModelStorageV2Test, IndividualVariables) {
  ModelStorageV2 storage;
  const VariableId x = storage.AddVariable("x");
  const VariableId y = storage.AddVariable(0.0, 1.0, true, "y");
  EXPECT_EQ(storage.variable_lower_bound(x), -kInf);
  EXPECT_EQ(storage.variable_upper_bound(x), kInf);
  EXPECT_EQ(storage.variable_lower_bound(y), 0.0);
  EXPECT_EQ(storage.variable_upper_bound(y), 1.0);
  EXPECT_FALSE(storage.is_variable_integer(x));
  EXPECT_TRUE(storage.is_variable_integer(y));
  EXPECT_EQ(storage.variable_name(x), "x");
  EXPECT_EQ(storage.variable_name(y), "y");
}

TEST(ModelStorageV2Test, AllVariables) {
  ModelStorageV2 storage;

  EXPECT_EQ(storage.num_variables(), 0);
  EXPECT_EQ(storage.next_variable_id(), VariableId{0});
  EXPECT_FALSE(storage.has_variable(VariableId{0}));
  EXPECT_THAT(storage.Variables(), IsEmpty());
  EXPECT_THAT(storage.SortedVariables(), IsEmpty());

  const VariableId x = storage.AddVariable("x");
  const VariableId y = storage.AddVariable(0.0, 1.0, true, "y");

  EXPECT_EQ(storage.num_variables(), 2);
  EXPECT_EQ(storage.next_variable_id(), VariableId(2));
  EXPECT_TRUE(storage.has_variable(x));
  EXPECT_TRUE(storage.has_variable(y));
  EXPECT_FALSE(storage.has_variable(VariableId(2)));
  EXPECT_THAT(storage.Variables(), UnorderedElementsAre(x, y));
  EXPECT_THAT(storage.SortedVariables(), ElementsAre(x, y));
}

TEST(ModelStorageV2Test, DeleteVariables) {
  ModelStorageV2 storage;
  const VariableId w = storage.AddVariable("w");
  const VariableId x = storage.AddVariable("x");
  const VariableId y = storage.AddVariable("y");

  storage.DeleteVariable(w);
  storage.DeleteVariable(y);

  EXPECT_EQ(storage.num_variables(), 1);
  EXPECT_EQ(storage.next_variable_id(), VariableId(3));
  EXPECT_FALSE(storage.has_variable(w));
  EXPECT_TRUE(storage.has_variable(x));
  EXPECT_FALSE(storage.has_variable(y));
  EXPECT_THAT(storage.Variables(), UnorderedElementsAre(x));
  EXPECT_THAT(storage.SortedVariables(), ElementsAre(x));

  // Add one more variable, make sure the id is correct.
  const VariableId z = storage.AddVariable("z");

  EXPECT_EQ(z, VariableId(3));
  EXPECT_EQ(storage.num_variables(), 2);
  EXPECT_EQ(storage.next_variable_id(), VariableId(4));
  EXPECT_THAT(storage.SortedVariables(), ElementsAre(x, z));
}

TEST(ModelStorageV2Test, EnsureNextVariableIdAtLeast) {
  ModelStorageV2 storage;

  storage.ensure_next_variable_id_at_least(VariableId{3});

  EXPECT_EQ(storage.next_variable_id(), VariableId{3});
  EXPECT_THAT(storage.Variables(), IsEmpty());

  EXPECT_EQ(storage.AddVariable("x"), VariableId(3));
  EXPECT_EQ(storage.next_variable_id(), VariableId{4});
  EXPECT_THAT(storage.Variables(), UnorderedElementsAre(VariableId(3)));
}

TEST(ModelStorageV2DeathTest, DeleteVariableTwice) {
  ModelStorageV2 storage;
  const VariableId x = storage.AddVariable("x");
  storage.DeleteVariable(x);
  EXPECT_DEATH_IF_SUPPORTED(storage.DeleteVariable(x),
                            HasSubstr("cannot delete variable"));
}

TEST(ModelStorageV2Test, IndividualLinearConstraints) {
  ModelStorageV2 storage;
  const LinearConstraintId c = storage.AddLinearConstraint("c");
  const LinearConstraintId d = storage.AddLinearConstraint(0.0, 1.0, "d");

  EXPECT_EQ(storage.linear_constraint_lower_bound(c), -kInf);
  EXPECT_EQ(storage.linear_constraint_upper_bound(c), kInf);
  EXPECT_EQ(storage.linear_constraint_lower_bound(d), 0.0);
  EXPECT_EQ(storage.linear_constraint_upper_bound(d), 1.0);
  EXPECT_EQ(storage.linear_constraint_name(c), "c");
  EXPECT_EQ(storage.linear_constraint_name(d), "d");
}

TEST(ModelStorageV2Test, AllLinearConstraints) {
  ModelStorageV2 storage;

  EXPECT_EQ(storage.num_linear_constraints(), 0);
  EXPECT_EQ(storage.next_linear_constraint_id(), LinearConstraintId(0));
  EXPECT_FALSE(storage.has_linear_constraint(LinearConstraintId(0)));
  EXPECT_THAT(storage.LinearConstraints(), IsEmpty());
  EXPECT_THAT(storage.SortedLinearConstraints(), IsEmpty());

  const LinearConstraintId c = storage.AddLinearConstraint("c");
  const LinearConstraintId d = storage.AddLinearConstraint(0.0, 1.0, "d");

  EXPECT_EQ(storage.num_linear_constraints(), 2);
  EXPECT_EQ(storage.next_linear_constraint_id(), LinearConstraintId(2));
  EXPECT_TRUE(storage.has_linear_constraint(c));
  EXPECT_TRUE(storage.has_linear_constraint(d));
  EXPECT_FALSE(storage.has_linear_constraint(LinearConstraintId(2)));
  EXPECT_THAT(storage.LinearConstraints(), UnorderedElementsAre(c, d));
  EXPECT_THAT(storage.SortedLinearConstraints(), ElementsAre(c, d));
}

TEST(ModelStorageV2Test, DeleteLinearConstraints) {
  ModelStorageV2 storage;
  const LinearConstraintId c = storage.AddLinearConstraint("c");
  const LinearConstraintId d = storage.AddLinearConstraint("d");
  const LinearConstraintId e = storage.AddLinearConstraint("e");

  storage.DeleteLinearConstraint(c);
  storage.DeleteLinearConstraint(e);

  EXPECT_EQ(storage.num_linear_constraints(), 1);
  EXPECT_EQ(storage.next_linear_constraint_id(), LinearConstraintId(3));
  EXPECT_FALSE(storage.has_linear_constraint(c));
  EXPECT_TRUE(storage.has_linear_constraint(d));
  EXPECT_FALSE(storage.has_linear_constraint(e));
  EXPECT_THAT(storage.LinearConstraints(), UnorderedElementsAre(d));
  EXPECT_THAT(storage.SortedLinearConstraints(), ElementsAre(d));

  // Add one more linear constraint, make sure the id is correct.
  const LinearConstraintId f = storage.AddLinearConstraint("f");

  EXPECT_EQ(f, LinearConstraintId(3));
  EXPECT_EQ(storage.num_linear_constraints(), 2);
  EXPECT_EQ(storage.next_linear_constraint_id(), LinearConstraintId(4));
  EXPECT_THAT(storage.SortedLinearConstraints(), ElementsAre(d, f));
}

TEST(ModelStorageV2Test, EnsureNextLinearConstraintIdAtLeast) {
  ModelStorageV2 storage;

  storage.ensure_next_linear_constraint_id_at_least(LinearConstraintId{3});

  EXPECT_EQ(storage.next_linear_constraint_id(), LinearConstraintId{3});
  EXPECT_THAT(storage.LinearConstraints(), IsEmpty());

  EXPECT_EQ(storage.AddLinearConstraint("c"), LinearConstraintId(3));
  EXPECT_EQ(storage.next_linear_constraint_id(), LinearConstraintId{4});
  EXPECT_THAT(storage.LinearConstraints(),
              UnorderedElementsAre(LinearConstraintId(3)));
}

TEST(ModelStorageV2DeathTest, DeleteLinearConstraintTwice) {
  ModelStorageV2 storage;
  const LinearConstraintId c = storage.AddLinearConstraint("c");
  storage.DeleteLinearConstraint(c);
  EXPECT_DEATH_IF_SUPPORTED(storage.DeleteLinearConstraint(c),
                            HasSubstr("cannot delete linear constraint"));
}

TEST(ModelStorageV2Test, LinearConstraintMatrix) {
  ModelStorageV2 storage;
  const LinearConstraintId c = storage.AddLinearConstraint("c");
  const LinearConstraintId d = storage.AddLinearConstraint("d");
  const VariableId x = storage.AddVariable("x");
  const VariableId y = storage.AddVariable("y");

  // Check the matrix while it is empty.
  EXPECT_FALSE(storage.is_linear_constraint_coefficient_nonzero(c, x));
  EXPECT_EQ(storage.linear_constraint_coefficient(c, x), 0.0);
  EXPECT_THAT(storage.linear_constraints_with_variable(x), IsEmpty());
  EXPECT_THAT(storage.variables_in_linear_constraint(c), IsEmpty());
  EXPECT_THAT(storage.linear_constraint_matrix(), IsEmpty());

  // Set the matrix.
  storage.set_linear_constraint_coefficient(c, x, 2.0);
  storage.set_linear_constraint_coefficient(c, y, 3.0);
  storage.set_linear_constraint_coefficient(d, y, 4.0);

  // Check the constraint matrix.
  EXPECT_TRUE(storage.is_linear_constraint_coefficient_nonzero(c, x));
  EXPECT_TRUE(storage.is_linear_constraint_coefficient_nonzero(c, y));
  EXPECT_FALSE(storage.is_linear_constraint_coefficient_nonzero(d, x));
  EXPECT_TRUE(storage.is_linear_constraint_coefficient_nonzero(d, y));

  EXPECT_EQ(storage.linear_constraint_coefficient(c, x), 2.0);
  EXPECT_EQ(storage.linear_constraint_coefficient(c, y), 3.0);
  EXPECT_EQ(storage.linear_constraint_coefficient(d, x), 0.0);
  EXPECT_EQ(storage.linear_constraint_coefficient(d, y), 4.0);

  EXPECT_THAT(storage.linear_constraint_matrix(),
              UnorderedElementsAre(
                  std::make_tuple(LinearConstraintId(0), VariableId(0), 2.0),
                  std::make_tuple(LinearConstraintId(0), VariableId(1), 3.0),
                  std::make_tuple(LinearConstraintId(1), VariableId(1), 4.0)));
  EXPECT_THAT(storage.variables_in_linear_constraint(c),
              UnorderedElementsAre(x, y));
  EXPECT_THAT(storage.variables_in_linear_constraint(d),
              UnorderedElementsAre(y));

  EXPECT_THAT(storage.linear_constraints_with_variable(x),
              UnorderedElementsAre(c));
  EXPECT_THAT(storage.linear_constraints_with_variable(y),
              UnorderedElementsAre(c, d));
}

TEST(ModelStorageV2Test, LinearConstraintMatrixExcludesDeletedVariable) {
  ModelStorageV2 storage;
  const VariableId x = storage.AddVariable("x");
  const VariableId y = storage.AddVariable("y");
  const LinearConstraintId c = storage.AddLinearConstraint("c");
  storage.set_linear_constraint_coefficient(c, x, 2.0);
  storage.set_linear_constraint_coefficient(c, y, 3.0);

  storage.DeleteVariable(x);

  EXPECT_TRUE(storage.is_linear_constraint_coefficient_nonzero(c, y));
  EXPECT_EQ(storage.linear_constraint_coefficient(c, y), 3.0);
  EXPECT_THAT(storage.linear_constraint_matrix(),
              UnorderedElementsAre(std::make_tuple(c, y, 3.0)));
  EXPECT_THAT(storage.variables_in_linear_constraint(c),
              UnorderedElementsAre(y));
}

class ModelStorageLinearObjectiveTest : public testing::Test {
 protected:
  void TestCommon(ObjectiveId objective_id) {
    EXPECT_EQ(storage_.num_linear_objective_terms(objective_id), 0);
    EXPECT_FALSE(storage_.is_maximize(objective_id));
    EXPECT_EQ(storage_.objective_offset(objective_id), 0.0);
    EXPECT_EQ(storage_.linear_objective_coefficient(objective_id, x_), 0.0);
    EXPECT_FALSE(
        storage_.is_linear_objective_coefficient_nonzero(objective_id, x_));
    EXPECT_THAT(storage_.LinearObjectiveNonzeros(objective_id), IsEmpty());

    storage_.set_maximize(objective_id);
    storage_.set_linear_objective_coefficient(objective_id, x_, 2.0);
    storage_.set_linear_objective_coefficient(objective_id, z_, 3.0);
    storage_.set_objective_offset(objective_id, 4.0);

    EXPECT_EQ(storage_.num_linear_objective_terms(objective_id), 2);
    EXPECT_TRUE(storage_.is_maximize(objective_id));
    EXPECT_EQ(storage_.objective_offset(objective_id), 4.0);
    EXPECT_EQ(storage_.linear_objective_coefficient(objective_id, x_), 2.0);
    EXPECT_EQ(storage_.linear_objective_coefficient(objective_id, y_), 0.0);
    EXPECT_EQ(storage_.linear_objective_coefficient(objective_id, z_), 3.0);

    EXPECT_TRUE(
        storage_.is_linear_objective_coefficient_nonzero(objective_id, x_));
    EXPECT_FALSE(
        storage_.is_linear_objective_coefficient_nonzero(objective_id, y_));
    EXPECT_TRUE(
        storage_.is_linear_objective_coefficient_nonzero(objective_id, z_));

    EXPECT_THAT(storage_.LinearObjectiveNonzeros(objective_id),
                UnorderedElementsAre(x_, z_));
  }

  ModelStorageV2 storage_{"model", "primary"};
  const VariableId x_ = storage_.AddVariable("x");
  const VariableId y_ = storage_.AddVariable("y");
  const VariableId z_ = storage_.AddVariable("z");
};

TEST_F(ModelStorageLinearObjectiveTest, Primary) {
  EXPECT_EQ(storage_.objective_name(kPrimaryObjectiveId), "primary");
  TestCommon(kPrimaryObjectiveId);
}

TEST_F(ModelStorageLinearObjectiveTest, Auxiliary) {
  AuxiliaryObjectiveId auxiliary = storage_.AddAuxiliaryObjective(3, "aux");
  EXPECT_EQ(storage_.objective_name(auxiliary), "aux");
  TestCommon(auxiliary);
}

TEST(ModelStorageV2Test, ObjectiveExcludesDeletedVariable) {
  ModelStorageV2 storage;
  const VariableId x = storage.AddVariable("x");
  const VariableId y = storage.AddVariable("y");
  storage.set_linear_objective_coefficient(kPrimaryObjectiveId, x, 2.0);
  storage.set_linear_objective_coefficient(kPrimaryObjectiveId, y, 3.0);

  storage.DeleteVariable(x);

  EXPECT_EQ(storage.num_linear_objective_terms(kPrimaryObjectiveId), 1);
  EXPECT_EQ(storage.linear_objective_coefficient(kPrimaryObjectiveId, y), 3.0);
  EXPECT_TRUE(
      storage.is_linear_objective_coefficient_nonzero(kPrimaryObjectiveId, y));
  EXPECT_THAT(storage.LinearObjectiveNonzeros(kPrimaryObjectiveId),
              UnorderedElementsAre(y));
}

// TODO(b/367370928): Test quadratic objectives

TEST(ModelStorageV2Test, ExportModelTest) {
  ModelStorageV2 storage;
  storage.AddVariable(2.0, 3.0, true, "x");

  for (bool remove_names : {false, true}) {
    SCOPED_TRACE(absl::StrCat("remove_names=", remove_names));
    ModelProto expected;
    auto& vars = *expected.mutable_variables();
    vars.add_ids(0);
    vars.add_lower_bounds(2.0);
    vars.add_upper_bounds(3.0);
    vars.add_integers(true);
    if (!remove_names) {
      vars.add_names("x");
    }
    EXPECT_THAT(storage.ExportModelV2(remove_names),
                IsOkAndHolds(EqualsProto(expected)));
  }
}

// TODO(b/367370928): Test FromModelProto on good input.

TEST(ModelStorageV2Test, DISABLED_FromInvalidModelProto) {
  // Here we only test one case of invalidity. We internally use
  // ValidateModel() and thus trust it to have checked all other cases.
  ModelProto bad_model_proto;
  bad_model_proto.mutable_variables()->add_ids(1);
  bad_model_proto.mutable_variables()->add_lower_bounds(0.0);
  bad_model_proto.mutable_variables()->add_upper_bounds(1.0);
  // Here we intentionally don't call add_integers().

  EXPECT_THAT(
      ModelStorageV2::FromModelProto(bad_model_proto),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("integers")));
}

TEST(ModelStorageV2Test, TestClone) {
  ModelStorageV2 storage("mod");
  const VariableId x = storage.AddVariable("x");
  const std::unique_ptr<ModelStorageV2> clone = storage.Clone();
  EXPECT_EQ(clone->name(), "mod");
  EXPECT_THAT(clone->Variables(), UnorderedElementsAre(x));
}

TEST(ModelStorageV2Test, TestCloneRename) {
  ModelStorageV2 storage("mod");
  const VariableId x = storage.AddVariable("x");
  const std::unique_ptr<ModelStorageV2> clone = storage.Clone("mod2");
  EXPECT_EQ(clone->name(), "mod2");
  EXPECT_THAT(clone->Variables(), UnorderedElementsAre(x));
}

TEST(ModelStorageV2Test, ExportModelUpdate) {
  ModelStorageV2 storage;

  const UpdateTrackerId update_tracker = storage.NewUpdateTracker();
  storage.AddVariable(2.0, 3.0, true, "x");

  for (bool remove_names : {false, true}) {
    SCOPED_TRACE(absl::StrCat("remove_names=", remove_names));
    ModelUpdateProto expected;
    auto& vars = *expected.mutable_new_variables();
    vars.add_ids(0);
    vars.add_lower_bounds(2.0);
    vars.add_upper_bounds(3.0);
    vars.add_integers(true);
    if (!remove_names) {
      vars.add_names("x");
    }
    EXPECT_THAT(storage.ExportModelUpdateV2(update_tracker, remove_names),
                IsOkAndHolds(Optional(EqualsProto(expected))));
  }
}

// TODO(b/367370928): Test ApplyUpdateProto on empty model.
// TODO(b/367370928): Test ApplyUpdateProto on non-empty model.

TEST(ModelStorageV2UpdateTrackerTest, UpdateEmptyAfterCheckpoint) {
  ModelStorageV2 storage;
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.AddVariable("x");
  storage.AdvanceCheckpoint(tracker);
  EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
              IsOkAndHolds(Eq(std::nullopt)));
}

TEST(ModelStorageV2UpdateTrackerTest, ModifyThenDeleteVariable) {
  ModelStorageV2 storage;
  const VariableId x = storage.AddVariable("x");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.set_variable_lower_bound(x, 2.0);
  storage.DeleteVariable(x);

  // The change to the variable lower bound is excluded, since x was deleted.
  ModelUpdateProto expected;
  expected.add_deleted_variable_ids(x.value());

  EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ModelStorageV2UpdateTrackerTest, TrackerOnEmptyModel) {
  ModelStorageV2 storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
              IsOkAndHolds(Eq(std::nullopt)));
}

TEST(ModelStorageV2UpdateTrackerTest, TrackerAddedBeforeAnyChange) {
  ModelStorageV2 storage("model");

  const UpdateTrackerId tracker = storage.NewUpdateTracker();

  const VariableId x = storage.AddVariable("x");

  {
    ModelUpdateProto expected;
    expected.mutable_new_variables()->add_ids(x.value());
    expected.mutable_new_variables()->add_lower_bounds(-kInf);
    expected.mutable_new_variables()->add_upper_bounds(kInf);
    expected.mutable_new_variables()->add_integers(false);
    expected.mutable_new_variables()->add_names("x");
    EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
                IsOkAndHolds(Optional(EqualsProto(expected))));
  }

  // Updating the checkpoint should reset the returned update.
  storage.AdvanceCheckpoint(tracker);
  EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
              IsOkAndHolds(Eq(std::nullopt)));

  // Do a new change to validate that the checkpoint is used.
  storage.set_variable_as_integer(x);

  {
    ModelUpdateProto expected;
    expected.mutable_variable_updates()->mutable_integers()->add_ids(x.value());
    expected.mutable_variable_updates()->mutable_integers()->add_values(true);
    EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
                IsOkAndHolds(Optional(EqualsProto(expected))));
  }
}

TEST(ModelStorageV2UpdateTrackerTest, TrackerAddedAfterSomeChanges) {
  ModelStorageV2 storage("model");
  const VariableId x = storage.AddVariable("x");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();

  EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
              IsOkAndHolds(Eq(std::nullopt)));

  storage.set_variable_as_integer(x);

  ModelUpdateProto expected;
  auto& integers = *expected.mutable_variable_updates()->mutable_integers();
  integers.add_ids(0);
  integers.add_values(true);
  EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ModelStorageV2UpdateTrackerTest, UpdatesOnSingleTrackerWithoutCheckpoint) {
  ModelStorageV2 storage("model");
  const VariableId x = storage.AddVariable("x");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();

  storage.set_variable_as_integer(x);
  {
    ModelUpdateProto expected;
    auto& integers = *expected.mutable_variable_updates()->mutable_integers();
    integers.add_ids(0);
    integers.add_values(true);
    EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
                IsOkAndHolds(Optional(EqualsProto(expected))));
  }

  storage.set_variable_lower_bound(x, 3.0);
  storage.set_variable_upper_bound(x, 5.0);

  ModelUpdateProto expected;
  auto& integers = *expected.mutable_variable_updates()->mutable_integers();
  integers.add_ids(0);
  integers.add_values(true);
  auto& lbs = *expected.mutable_variable_updates()->mutable_lower_bounds();
  lbs.add_ids(0);
  lbs.add_values(3.0);
  auto& ubs = *expected.mutable_variable_updates()->mutable_upper_bounds();
  ubs.add_ids(0);
  ubs.add_values(5.0);
  EXPECT_THAT(storage.ExportModelUpdateV2(tracker),
              IsOkAndHolds(Optional(EqualsProto(expected))));
}

TEST(ModelStorageV2UpdateTrackerTest, TrackerDestruction) {
  ModelStorageV2 storage("model");

  const VariableId x = storage.AddVariable("x");

  // Create two trackers sharing the same checkpoint.
  const UpdateTrackerId tracker1 = storage.NewUpdateTracker();
  const UpdateTrackerId tracker2 = storage.NewUpdateTracker();

  storage.set_variable_as_integer(x);

  ModelUpdateProto expected;
  auto& integers = *expected.mutable_variable_updates()->mutable_integers();
  integers.add_ids(0);
  integers.add_values(true);
  EXPECT_THAT(storage.ExportModelUpdateV2(tracker1),
              IsOkAndHolds(Optional(EqualsProto(expected))));
  EXPECT_THAT(storage.ExportModelUpdateV2(tracker2),
              IsOkAndHolds(Optional(EqualsProto(expected))));

  // Destroy one tracker.
  storage.DeleteUpdateTracker(tracker1);

  // Checkpointing should not crash. It would if the destroyed tracker has not
  // been unregistered by its destructor.
  storage.AdvanceCheckpoint(tracker2);
}

TEST(ModelStorageV2UpdateTrackerDeathTest, DeleteTwice) {
  ModelStorageV2 storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.DeleteUpdateTracker(tracker);
  EXPECT_DEATH_IF_SUPPORTED(
      storage.DeleteUpdateTracker(tracker),
      AllOf(HasSubstr("UpdateTrackerId"), HasSubstr("not found")));
}

TEST(ModelStorageV2UpdateTrackerDeathTest, ExportModelUpdateAfterDestruction) {
  ModelStorageV2 storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.DeleteUpdateTracker(tracker);
  EXPECT_DEATH_IF_SUPPORTED(
      storage.ExportModelUpdateV2(tracker).IgnoreError(),
      AllOf(HasSubstr("UpdateTrackerId"), HasSubstr("not found")));
}

TEST(ModelStorageV2UpdateTrackerDeathTest, CheckpointAfterDestruction) {
  ModelStorageV2 storage("model");
  const UpdateTrackerId tracker = storage.NewUpdateTracker();
  storage.DeleteUpdateTracker(tracker);
  EXPECT_DEATH_IF_SUPPORTED(
      storage.AdvanceCheckpoint(tracker),
      AllOf(HasSubstr("UpdateTrackerId"), HasSubstr("not found")));
}

////////////////////////////////////////////////////////////////////////////////
// Atomic constraints
////////////////////////////////////////////////////////////////////////////////

template <typename ConstraintData>
class AtomicConstraintV2Test : public testing::Test {
 public:
  void DoTest();
};

TYPED_TEST_SUITE_P(AtomicConstraintV2Test);

TYPED_TEST_P(AtomicConstraintV2Test, Empty) {
  using ConData = TypeParam;
  using IdType = ConData::IdType;
  ModelStorageV2 storage;

  EXPECT_FALSE(storage.has_constraint(IdType{0}));
  EXPECT_FALSE(storage.has_constraint(IdType{2}));
  EXPECT_THAT(storage.Constraints<IdType>(), IsEmpty());
  EXPECT_THAT(storage.SortedConstraints<IdType>(), IsEmpty());
  EXPECT_EQ(storage.num_constraints<IdType>(), 0);
  EXPECT_EQ(storage.next_constraint_id<IdType>(), IdType{0});
}

TYPED_TEST_P(AtomicConstraintV2Test, Create) {
  using ConData = TypeParam;
  using IdType = ConData::IdType;
  ModelStorageV2 storage;

  ConData con1;
  EXPECT_EQ(storage.AddAtomicConstraint(con1), IdType{0});
  ConData con2;
  EXPECT_EQ(storage.AddAtomicConstraint(con2), IdType{1});

  EXPECT_TRUE(storage.has_constraint(IdType{0}));
  EXPECT_TRUE(storage.has_constraint(IdType{1}));
  EXPECT_FALSE(storage.has_constraint(IdType{2}));
  EXPECT_THAT(storage.Constraints<IdType>(),
              UnorderedElementsAre(IdType{0}, IdType{1}));
  EXPECT_THAT(storage.SortedConstraints<IdType>(),
              ElementsAre(IdType{0}, IdType{1}));
  EXPECT_EQ(storage.num_constraints<IdType>(), 2);
  EXPECT_EQ(storage.next_constraint_id<IdType>(), IdType{2});
}

TYPED_TEST_P(AtomicConstraintV2Test, EnsureNext) {
  using ConData = TypeParam;
  using IdType = ConData::IdType;
  ModelStorageV2 storage;

  storage.ensure_next_constraint_id_at_least(IdType{3});
  ConData con1;
  EXPECT_EQ(storage.AddAtomicConstraint(con1), IdType{3});
  ConData con2;
  EXPECT_EQ(storage.AddAtomicConstraint(con2), IdType{4});

  EXPECT_FALSE(storage.has_constraint(IdType{0}));
  EXPECT_TRUE(storage.has_constraint(IdType{3}));
  EXPECT_TRUE(storage.has_constraint(IdType{4}));
  EXPECT_FALSE(storage.has_constraint(IdType{5}));
  EXPECT_THAT(storage.SortedConstraints<IdType>(),
              ElementsAre(IdType{3}, IdType{4}));
  EXPECT_EQ(storage.num_constraints<IdType>(), 2);
  EXPECT_EQ(storage.next_constraint_id<IdType>(), IdType{5});
}

TYPED_TEST_P(AtomicConstraintV2Test, Delete) {
  using ConData = TypeParam;
  using IdType = ConData::IdType;
  ModelStorageV2 storage;
  storage.AddAtomicConstraint(ConData{});
  storage.AddAtomicConstraint(ConData{});
  storage.AddAtomicConstraint(ConData{});

  storage.DeleteAtomicConstraint(IdType{1});

  EXPECT_TRUE(storage.has_constraint(IdType{0}));
  EXPECT_FALSE(storage.has_constraint(IdType{1}));
  EXPECT_TRUE(storage.has_constraint(IdType{2}));
  EXPECT_THAT(storage.SortedConstraints<IdType>(),
              ElementsAre(IdType{0}, IdType{2}));
  EXPECT_EQ(storage.num_constraints<IdType>(), 2);
  EXPECT_EQ(storage.next_constraint_id<IdType>(), IdType{3});
}

REGISTER_TYPED_TEST_SUITE_P(AtomicConstraintV2Test, Empty, Create, EnsureNext,
                            Delete);

using ConstraintDataTyes =
    ::testing::Types<QuadraticConstraintData, IndicatorConstraintData>;
INSTANTIATE_TYPED_TEST_SUITE_P(My, AtomicConstraintV2Test, ConstraintDataTyes);

TEST(ModelStorageV2Test, QuadraticConstraintGeneratesProto) {
  ModelStorageV2 storage;
  const VariableId x = storage.AddVariable("x");
  const VariableId y = storage.AddVariable("y");
  const VariableId z = storage.AddVariable("z");

  QuadraticConstraintData con = {
      .lower_bound = 3.0, .upper_bound = 4.0, .name = "c"};
  con.linear_terms.set(x, 5.0);
  con.linear_terms.set(z, 6.0);
  con.quadratic_terms.set(x, y, 7.0);
  con.quadratic_terms.set(z, z, 8.0);

  EXPECT_EQ(storage.AddAtomicConstraint(con), QuadraticConstraintId{0});

  {
    const QuadraticConstraintData round_trip =
        storage.GetConstraintData(QuadraticConstraintId{0});
    EXPECT_EQ(round_trip.name, "c");
    EXPECT_EQ(round_trip.lower_bound, 3.0);
    EXPECT_EQ(round_trip.upper_bound, 4.0);
    EXPECT_THAT(round_trip.linear_terms.terms(),
                UnorderedElementsAre(Pair(x, 5.0), Pair(z, 6.0)));
    EXPECT_THAT(
        round_trip.quadratic_terms.Terms(),
        UnorderedElementsAre(FieldsAre(x, y, 7.0), FieldsAre(z, z, 8.0)));
  }

  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  for (int i = 0; i < 3; ++i) {
    vars.add_ids(i);
    vars.add_lower_bounds(-kInf);
    vars.add_upper_bounds(kInf);
    vars.add_integers(false);
  }
  vars.add_names("x");
  vars.add_names("y");
  vars.add_names("z");
  QuadraticConstraintProto& con_proto =
      (*proto.mutable_quadratic_constraints())[0];
  con_proto.set_name("c");
  con_proto.set_lower_bound(3.0);
  con_proto.set_upper_bound(4.0);
  SparseDoubleVectorProto& lin_terms = *con_proto.mutable_linear_terms();
  lin_terms.add_ids(0);
  lin_terms.add_values(5.0);
  lin_terms.add_ids(2);
  lin_terms.add_values(6.0);
  SparseDoubleMatrixProto& quad_terms = *con_proto.mutable_quadratic_terms();
  quad_terms.add_row_ids(0);
  quad_terms.add_column_ids(1);
  quad_terms.add_coefficients(7.0);
  quad_terms.add_row_ids(2);
  quad_terms.add_column_ids(2);
  quad_terms.add_coefficients(8.0);

  EXPECT_THAT(storage.ExportModelV2(), IsOkAndHolds(EqualsProto(proto)));
}

TEST(ModelStorageV2Test, IndicatorConstraintGeneratesProto) {
  ModelStorageV2 storage;
  const VariableId x = storage.AddVariable("x");
  const VariableId y = storage.AddVariable("y");
  const VariableId z = storage.AddVariable("z");

  IndicatorConstraintData con = {
      .lower_bound = 3.0, .upper_bound = 4.0, .name = "c"};
  con.linear_terms.set(x, 5.0);
  con.linear_terms.set(z, 6.0);
  con.activate_on_zero = true;
  con.indicator = y;
  storage.AddAtomicConstraint(con);

  {
    const IndicatorConstraintData round_trip =
        storage.GetConstraintData(IndicatorConstraintId{0});
    EXPECT_EQ(round_trip.name, "c");
    EXPECT_EQ(round_trip.lower_bound, 3.0);
    EXPECT_EQ(round_trip.upper_bound, 4.0);
    EXPECT_THAT(round_trip.linear_terms.terms(),
                UnorderedElementsAre(Pair(x, 5.0), Pair(z, 6.0)));
    EXPECT_THAT(round_trip.indicator, Optional(y));
    EXPECT_TRUE(round_trip.activate_on_zero);
  }

  ModelProto proto;
  VariablesProto& vars = *proto.mutable_variables();
  for (int i = 0; i < 3; ++i) {
    vars.add_ids(i);
    vars.add_lower_bounds(-kInf);
    vars.add_upper_bounds(kInf);
    vars.add_integers(false);
  }
  vars.add_names("x");
  vars.add_names("y");
  vars.add_names("z");
  IndicatorConstraintProto& con_proto =
      (*proto.mutable_indicator_constraints())[0];
  con_proto.set_name("c");
  con_proto.set_lower_bound(3.0);
  con_proto.set_upper_bound(4.0);
  SparseDoubleVectorProto& lin_terms = *con_proto.mutable_expression();
  lin_terms.add_ids(0);
  lin_terms.add_values(5.0);
  lin_terms.add_ids(2);
  lin_terms.add_values(6.0);
  con_proto.set_activate_on_zero(true);
  con_proto.set_indicator_id(1);

  EXPECT_THAT(storage.ExportModelV2(), IsOkAndHolds(EqualsProto(proto)));
}

INSTANTIATE_TYPED_TEST_SUITE_P(DISABLED_SecondOrderConeConstraintTestSuite,
                               AtomicConstraintTest,
                               SecondOrderConeConstraintId);

INSTANTIATE_TYPED_TEST_SUITE_P(DISABLED_Sos1ConstraintTestSuite,
                               AtomicConstraintTest, Sos1ConstraintId);
INSTANTIATE_TYPED_TEST_SUITE_P(DISABLED_Sos2ConstraintTestSuite,
                               AtomicConstraintTest, Sos2ConstraintId);

}  // namespace
}  // namespace operations_research::math_opt
