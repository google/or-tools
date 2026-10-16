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

// To test a new atomic constraint family at the `ModelStorage` layer:
//
//   1. Add a template specialization for `ModelStorageTestHelper<IdType>`,
//      where `IdType` is the strong-int ID type for the constraint family. See
//      struct-level comments for detailed requirements.
//   2. In `model_storage_test.cc`, #include your helper specialization and
//      instantiate the new test suite:
//        INSTANTIATE_TYPED_TEST_SUITE_P(MyNewConstraintTestSuite,
//                             AtomicConstraintTest, MyNewConstraintId);
#ifndef ORTOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINT_TESTING_H_
#define ORTOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINT_TESTING_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/map.h"
#include "google/protobuf/repeated_field.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/validators/model_validator.h"

namespace operations_research::math_opt {

using ::testing::ElementsAre;
using ::testing::EquivToProto;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::status::StatusIs;

// TODO(b/381013955): Remove when all constraints use ElementId.
template <ElementType element_type>
auto NextElementId(ElementId<element_type> id) {
  return id.Next();
}
template <typename Tag, typename Int, typename Validator>
auto NextElementId(::util_intops::StrongInt<Tag, Int, Validator> id) {
  return id + ::util_intops::StrongInt<Tag, Int, Validator>(1);
}

// A helper type for testing a particular constraint family at the
// `ModelStorage` layer. Each specialization must satisfy the duck-typed
// interface described below.
template <typename IdType>
struct ModelStorageTestHelper {
  // The proto type to represent a single constraint in the family.
  using ProtoType = void;

  // This is a convenience for the interface code below; specializations are not
  // strictly required to provide such an alias.
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;

  // Produces the data for a valid, empty constraint with the provided name.
  static ConstraintData EmptyConstraint(absl::string_view name);

  // Produces the data for a valid constraint involving the variables in `vars`,
  // as reflected by `ConstraintData.RelatedVariables()`.
  static ConstraintData ConstraintOverVars(const std::vector<VariableId>& vars,
                                           absl::string_view name);

  // Returns the mutable map containing all constraints of the family in a
  // given `ModelProto`.
  static google::protobuf::Map<int64_t, ProtoType>& ModelProtoConstraints(
      ModelProto& model_proto);

  // Returns the mutable list of all deleted constraints of the family in a
  // given `ModelUpdateProto`.
  static google::protobuf::RepeatedField<int64_t>& UpdateProtoDeletes(
      ModelUpdateProto& model_update_proto);

  // Returns the mutable map containing all new constraints of the family in a
  // given `ModelUpdateProto`.
  static google::protobuf::Map<int64_t, ProtoType>& UpdateProtoCreates(
      ModelUpdateProto& model_update_proto);
};

template <typename IdType>
class AtomicConstraintTest : public testing::Test {
 public:
  using ConstraintData =
      typename AtomicConstraintTraits<IdType>::ConstraintData;
  using Helper = ModelStorageTestHelper<IdType>;

  AtomicConstraintTest()
      : storage_("atomic_constraint_test"),
        update_tracker_(storage_.NewUpdateTracker()),
        x_(storage_.AddVariable("x")),
        y_(storage_.AddVariable(0.0, 1.0, true, "y")),
        c_data_(Helper::ConstraintOverVars({x_}, "c")),
        d_data_(Helper::ConstraintOverVars({x_, y_}, "d")),
        c_(storage_.AddAtomicConstraint(c_data_)),
        d_(storage_.AddAtomicConstraint(d_data_)) {}

 protected:
  // Proto representation of the base model: just the variables, no constraints.
  ModelProto base_proto_model() const {
    return google::protobuf::contrib::parse_proto::ParseTextProtoOrDie(R"pb(
      name: "atomic_constraint_test"
      variables: {
        ids: [ 0, 1 ]
        lower_bounds: [ -infinity, 0.0 ]
        upper_bounds: [ infinity, 1.0 ]
        integers: [ false, true ]
        names: [ "x", "y" ]
      }
    )pb");
  }

  // Proto representation to construct the base model via an update: just the
  // variables, no constraints.
  ModelUpdateProto base_proto_update() const {
    return google::protobuf::contrib::parse_proto::ParseTextProtoOrDie(R"pb(
      new_variables: {
        ids: [ 0, 1 ]
        lower_bounds: [ -infinity, 0.0 ]
        upper_bounds: [ infinity, 1.0 ]
        integers: [ false, true ]
        names: [ "x", "y" ]
      }
    )pb");
  }

  ModelStorage storage_;
  UpdateTrackerId update_tracker_;
  VariableId x_;
  VariableId y_;
  ConstraintData c_data_;
  ConstraintData d_data_;
  IdType c_;
  IdType d_;
};

TYPED_TEST_SUITE_P(AtomicConstraintTest);

TYPED_TEST_P(AtomicConstraintTest, CreateOutput) {
  using IdType = TypeParam;

  EXPECT_EQ(this->x_, VariableId(0));
  EXPECT_EQ(this->y_, VariableId(1));
  EXPECT_EQ(this->c_, IdType(0));
  EXPECT_EQ(this->d_, IdType(1));
}

TYPED_TEST_P(AtomicConstraintTest, ReadConstraints) {
  using IdType = TypeParam;
  const ModelStorage& storage = this->storage_;

  EXPECT_EQ(storage.num_constraints<IdType>(), 2);
  EXPECT_EQ(storage.next_constraint_id<IdType>(), IdType(2));
  EXPECT_TRUE(storage.has_constraint(IdType(0)));
  EXPECT_TRUE(storage.has_constraint(IdType(1)));
  EXPECT_FALSE(storage.has_constraint(IdType(2)));
  EXPECT_THAT(storage.Constraints<IdType>(),
              UnorderedElementsAre(IdType(0), IdType(1)));
  EXPECT_THAT(storage.SortedConstraints<IdType>(),
              ElementsAre(IdType(0), IdType(1)));
}

TYPED_TEST_P(AtomicConstraintTest, ExportModel) {
  using Helper = typename TestFixture::Helper;
  const ModelStorage& storage = this->storage_;

  const ModelProto actual = storage.ExportModel();
  ASSERT_OK(ValidateModel(actual).status());

  ModelProto expected = this->base_proto_model();
  auto& proto_constraints = Helper::ModelProtoConstraints(expected);
  proto_constraints[0] = this->c_data_.Proto();
  proto_constraints[1] = this->d_data_.Proto();
  EXPECT_THAT(actual, testing::EquivToProto(expected));
}

TYPED_TEST_P(AtomicConstraintTest, FromModelProto) {
  // Here we simply check that we end up with the same proto if we build a
  // storage from a proto and export a new proto.
  using Helper = typename TestFixture::Helper;

  ModelProto model = this->base_proto_model();
  auto& proto_constraints = Helper::ModelProtoConstraints(model);
  proto_constraints[0] = this->c_data_.Proto();
  proto_constraints[1] = this->d_data_.Proto();

  ASSERT_OK_AND_ASSIGN(const std::unique_ptr<ModelStorage> parsed_storage,
                       ModelStorage::FromModelProto(model));
  EXPECT_THAT(parsed_storage->ExportModel(), EquivToProto(model));
}

TYPED_TEST_P(AtomicConstraintTest, FromInvalidModelProto) {
  // Here we only test one case of invalidity (bad variable ID) to ensure that
  // the model validator catches it.
  using Helper = typename TestFixture::Helper;

  ModelProto bad_model_proto;
  auto& proto_constraints = Helper::ModelProtoConstraints(bad_model_proto);
  proto_constraints[0] =
      Helper::ConstraintOverVars({VariableId(12)}, "c").Proto();

  EXPECT_THAT(ModelStorage::FromModelProto(bad_model_proto),
              StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("12")));
}

TYPED_TEST_P(AtomicConstraintTest, ClonePreservesNextId) {
  using IdType = TypeParam;
  ModelStorage& storage = this->storage_;

  storage.ensure_next_constraint_id_at_least(IdType(4));
  const std::unique_ptr<ModelStorage> clone = storage.Clone();
  EXPECT_EQ(clone->next_constraint_id<IdType>(), IdType(4));
}

TYPED_TEST_P(AtomicConstraintTest, CloneWithHoles) {
  // Test cloning a storage with missing constraints ids before the last items.
  using IdType = TypeParam;
  using Helper = typename TestFixture::Helper;
  ModelStorage& storage = this->storage_;

  storage.ensure_next_constraint_id_at_least(IdType(4));
  storage.AddAtomicConstraint(Helper::EmptyConstraint(""));
  storage.ensure_next_constraint_id_at_least(IdType(6));
  const IdType e = storage.AddAtomicConstraint(Helper::EmptyConstraint("e"));

  {
    const std::unique_ptr<ModelStorage> clone = storage.Clone();

    EXPECT_THAT(clone->ExportModel(), EquivToProto(storage.ExportModel()));

    // Test that adding a new constraint returns the expected new index.
    EXPECT_EQ(storage.AddAtomicConstraint(Helper::EmptyConstraint("")),
              NextElementId(e));
  }

  storage.ensure_next_constraint_id_at_least(NextElementId(NextElementId(e)));
  const IdType last_added_constraint =
      storage.AddAtomicConstraint(Helper::EmptyConstraint(""));
  storage.ensure_next_constraint_id_at_least(
      NextElementId(last_added_constraint));

  {
    const std::unique_ptr<ModelStorage> clone = storage.Clone();

    EXPECT_THAT(clone->ExportModel(), EquivToProto(storage.ExportModel()));

    // We expect to take into account the removed variables/constraints.
    EXPECT_EQ(storage.AddAtomicConstraint(Helper::EmptyConstraint("")),
              NextElementId(last_added_constraint));
  }
}

TYPED_TEST_P(AtomicConstraintTest, CloneWithDuplicatedNames) {
  // The ModelStorage class does not check that names are not duplicated. We
  // don't want the Clone() to fail (and crash) if this happens.
  using Helper = typename TestFixture::Helper;
  ModelStorage& storage = this->storage_;

  storage.AddAtomicConstraint(Helper::EmptyConstraint("q"));
  storage.AddAtomicConstraint(Helper::EmptyConstraint("q"));

  EXPECT_THAT(storage.Clone()->ExportModel(),
              EquivToProto(storage.ExportModel()));
}

TYPED_TEST_P(AtomicConstraintTest, ExportModelUpdate) {
  using Helper = typename TestFixture::Helper;
  const ModelStorage& storage = this->storage_;

  const std::optional<ModelUpdateProto> actual =
      storage.ExportModelUpdate(this->update_tracker_);
  ModelUpdateProto proto_update = this->base_proto_update();
  auto& new_constraints = Helper::UpdateProtoCreates(proto_update);
  new_constraints[0] = this->c_data_.Proto();
  new_constraints[1] = this->d_data_.Proto();

  EXPECT_THAT(actual, Optional(EquivToProto(proto_update)));
}

TYPED_TEST_P(AtomicConstraintTest, DeletedVariableIsDeletedFromStorage) {
  using IdType = TypeParam;
  ModelStorage& storage = this->storage_;

  const IdType c = this->c_;
  const IdType d = this->d_;
  const VariableId x = this->x_;
  const VariableId y = this->y_;

  ASSERT_THAT(storage.VariablesInConstraint(c), UnorderedElementsAre(x));
  ASSERT_THAT(storage.VariablesInConstraint(d), UnorderedElementsAre(x, y));
  ASSERT_THAT(storage.ConstraintsWithVariable<IdType>(x),
              UnorderedElementsAre(c, d));
  ASSERT_THAT(storage.ConstraintsWithVariable<IdType>(y),
              UnorderedElementsAre(d));

  storage.DeleteVariable(x);
  EXPECT_THAT(storage.VariablesInConstraint(c), IsEmpty());
  EXPECT_THAT(storage.VariablesInConstraint(d), UnorderedElementsAre(y));
}

TYPED_TEST_P(AtomicConstraintTest, ApplyModelUpdateWithReusedConstraintId) {
  using Helper = typename TestFixture::Helper;
  ModelStorage& storage = this->storage_;

  storage.DeleteAtomicConstraint(this->c_);

  ModelUpdateProto update;
  Helper::UpdateProtoCreates(update)[0] = this->c_data_.Proto();

  EXPECT_THAT(
      storage.ApplyUpdateProto(update),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("increasing")));
}

TYPED_TEST_P(AtomicConstraintTest, DeleteConstraintNotYetExtracted) {
  using IdType = TypeParam;
  using Helper = typename TestFixture::Helper;
  ModelStorage& storage = this->storage_;

  storage.DeleteAtomicConstraint(this->c_);
  EXPECT_EQ(storage.num_constraints<IdType>(), 1);
  EXPECT_THAT(storage.Constraints<IdType>(), UnorderedElementsAre(IdType(1)));
  EXPECT_EQ(storage.next_constraint_id<IdType>(), IdType(2));

  const ModelProto model_proto = storage.ExportModel();
  ASSERT_OK(ValidateModel(model_proto));
  ModelProto expected = this->base_proto_model();
  auto& proto_constraints = Helper::ModelProtoConstraints(expected);
  proto_constraints[1] = this->d_data_.Proto();
  EXPECT_THAT(model_proto, EquivToProto(expected));
}

TYPED_TEST_P(AtomicConstraintTest, DeleteConstraintAlreadyExtracted) {
  using IdType = TypeParam;
  using Helper = typename TestFixture::Helper;
  ModelStorage& storage = this->storage_;

  storage.AdvanceCheckpoint(this->update_tracker_);
  storage.DeleteAtomicConstraint(this->c_);
  EXPECT_EQ(storage.num_constraints<IdType>(), 1);
  EXPECT_THAT(storage.Constraints<IdType>(), UnorderedElementsAre(IdType(1)));
  EXPECT_EQ(storage.next_constraint_id<IdType>(), IdType(2));

  const ModelProto model_proto = storage.ExportModel();
  ASSERT_OK(ValidateModel(model_proto));
  ModelProto expected = this->base_proto_model();
  auto& proto_constraints = Helper::ModelProtoConstraints(expected);
  proto_constraints[1] = this->d_data_.Proto();
  EXPECT_THAT(model_proto, EquivToProto(expected));
}

TYPED_TEST_P(AtomicConstraintTest, UpdateDeleteConstraintNotYetExtracted) {
  // For this test, storage_.Checkpoint() is not called; so the clone must start
  // empty for the update to make sense.
  using Helper = typename TestFixture::Helper;
  ModelStorage& storage = this->storage_;

  ModelStorage clone(storage.name());
  storage.DeleteAtomicConstraint(this->c_);

  ModelUpdateProto expected = this->base_proto_update();
  auto& new_constraints = Helper::UpdateProtoCreates(expected);
  new_constraints[1] = this->d_data_.Proto();

  const std::optional<ModelUpdateProto> update =
      storage.ExportModelUpdate(this->update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone.ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone.ExportModel(), EquivToProto(storage.ExportModel()));
}

TYPED_TEST_P(AtomicConstraintTest, UpdateDeleteConstraintAlreadyExtracted) {
  using Helper = typename TestFixture::Helper;
  ModelStorage& storage = this->storage_;

  std::unique_ptr<ModelStorage> clone = storage.Clone();

  storage.AdvanceCheckpoint(this->update_tracker_);
  storage.DeleteAtomicConstraint(this->c_);
  ModelUpdateProto expected;
  Helper::UpdateProtoDeletes(expected).Add(this->c_.value());
  const std::optional<ModelUpdateProto> update =
      storage.ExportModelUpdate(this->update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EquivToProto(storage.ExportModel()));
}

TYPED_TEST_P(AtomicConstraintTest,
             DeleteAllVariablesAndConstraintsUpdateFromCheckpoint) {
  using Helper = typename TestFixture::Helper;
  ModelStorage& storage = this->storage_;

  std::unique_ptr<ModelStorage> clone = storage.Clone();

  storage.AdvanceCheckpoint(this->update_tracker_);
  storage.DeleteAtomicConstraint(this->c_);
  storage.DeleteAtomicConstraint(this->d_);
  storage.DeleteVariable(this->x_);
  storage.DeleteVariable(this->y_);
  ModelUpdateProto expected;
  expected.add_deleted_variable_ids(0);
  expected.add_deleted_variable_ids(1);
  {
    auto& deleted_ids = Helper::UpdateProtoDeletes(expected);
    deleted_ids.Add(this->c_.value());
    deleted_ids.Add(this->d_.value());
  }
  const std::optional<ModelUpdateProto> update =
      storage.ExportModelUpdate(this->update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(expected)));

  ASSERT_OK(clone->ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone->ExportModel(), EquivToProto(storage.ExportModel()));
}

TYPED_TEST_P(AtomicConstraintTest,
             DeleteAllVariablesAndConstraintsUpdateNoCheckpoint) {
  // For this test, storage_.Checkpoint() is not called; so the clone must start
  // empty for the update to make sense.
  ModelStorage& storage = this->storage_;

  ModelStorage clone(storage.name());

  storage.DeleteAtomicConstraint(this->c_);
  storage.DeleteAtomicConstraint(this->d_);
  storage.DeleteVariable(this->x_);
  storage.DeleteVariable(this->y_);
  const std::optional<ModelUpdateProto> update =
      storage.ExportModelUpdate(this->update_tracker_);
  ASSERT_THAT(update, Optional(EquivToProto(ModelUpdateProto())));

  ASSERT_OK(clone.ApplyUpdateProto(update.value()));
  EXPECT_THAT(clone.ExportModel(), EquivToProto(storage.ExportModel()));
}

TYPED_TEST_P(AtomicConstraintTest,
             DeleteAllVariablesAndConstraintsFromCheckpointModel) {
  ModelStorage& storage = this->storage_;

  storage.AdvanceCheckpoint(this->update_tracker_);
  storage.DeleteAtomicConstraint(this->c_);
  storage.DeleteAtomicConstraint(this->d_);
  storage.DeleteVariable(this->x_);
  storage.DeleteVariable(this->y_);
  ModelProto expected;
  expected.set_name("atomic_constraint_test");
  EXPECT_THAT(storage.ExportModel(), EquivToProto(expected));
}

TYPED_TEST_P(AtomicConstraintTest,
             DeleteAllVariablesAndConstraintsNoCheckpointModel) {
  ModelStorage& storage = this->storage_;

  storage.DeleteAtomicConstraint(this->c_);
  storage.DeleteAtomicConstraint(this->d_);
  storage.DeleteVariable(this->x_);
  storage.DeleteVariable(this->y_);
  ModelProto expected;
  expected.set_name("atomic_constraint_test");
  EXPECT_THAT(storage.ExportModel(), EquivToProto(expected));
}

REGISTER_TYPED_TEST_SUITE_P(
    AtomicConstraintTest, CreateOutput, ReadConstraints, ExportModel,
    FromModelProto, FromInvalidModelProto, CloneWithHoles, ClonePreservesNextId,
    CloneWithDuplicatedNames, ExportModelUpdate,
    ApplyModelUpdateWithReusedConstraintId, DeletedVariableIsDeletedFromStorage,
    DeleteConstraintNotYetExtracted, DeleteConstraintAlreadyExtracted,
    UpdateDeleteConstraintNotYetExtracted,
    UpdateDeleteConstraintAlreadyExtracted,
    DeleteAllVariablesAndConstraintsUpdateFromCheckpoint,
    DeleteAllVariablesAndConstraintsUpdateNoCheckpoint,
    DeleteAllVariablesAndConstraintsFromCheckpointModel,
    DeleteAllVariablesAndConstraintsNoCheckpointModel);

}  // namespace operations_research::math_opt

#endif  // ORTOOLS_MATH_OPT_STORAGE_ATOMIC_CONSTRAINT_TESTING_H_
