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

#include "ortools/math_opt/storage/objective_storage.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "google/protobuf/map.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"

namespace operations_research::math_opt {

using ::testing::ElementsAre;
using ::testing::EquivToProto;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::no_adl::FieldsAre;

using Diff = ObjectiveStorage::Diff;

testing::Matcher<
    std::pair<ObjectiveProto, google::protobuf::Map<int64_t, ObjectiveProto>>>
ObjectivesEquivTo(
    const ObjectiveProto& primary_obj,
    absl::Span<const std::pair<int64_t, ObjectiveProto>> auxiliary_objs) {
  std::vector<testing::Matcher<std::pair<int64_t, ObjectiveProto>>>
      auxiliary_expected;
  for (const auto& [id, obj] : auxiliary_objs) {
    auxiliary_expected.push_back(Pair(id, EquivToProto(obj)));
  }
  return Pair(EquivToProto(primary_obj), testing::UnorderedElementsAreArray(
                                             std::move(auxiliary_expected)));
}

testing::Matcher<
    std::pair<ObjectiveUpdatesProto, AuxiliaryObjectivesUpdatesProto>>
ObjUpdateEquivTo(const ObjectiveUpdatesProto& primary_update,
                 const AuxiliaryObjectivesUpdatesProto& auxiliary_update) {
  return Pair(EquivToProto(primary_update), EquivToProto(auxiliary_update));
}

TEST(ObjectiveStorageTest, ReadEmpty) {
  ObjectiveStorage storage;
  EXPECT_FALSE(storage.maximize(kPrimaryObjectiveId));
  EXPECT_EQ(storage.priority(kPrimaryObjectiveId), 0);
  EXPECT_EQ(storage.offset(kPrimaryObjectiveId), 0.0);
  EXPECT_THAT(storage.linear_terms(kPrimaryObjectiveId), IsEmpty());
  EXPECT_THAT(storage.quadratic_terms(kPrimaryObjectiveId).Terms(), IsEmpty());
  {
    VariableId x(0);
    EXPECT_EQ(storage.linear_term(kPrimaryObjectiveId, x), 0.0);
    EXPECT_EQ(storage.quadratic_term(kPrimaryObjectiveId, x, x), 0.0);
  }

  EXPECT_EQ(storage.num_auxiliary_objectives(), 0);
  EXPECT_FALSE(storage.contains(AuxiliaryObjectiveId(0)));
  EXPECT_FALSE(storage.contains(AuxiliaryObjectiveId(1)));
  EXPECT_EQ(storage.next_id(), AuxiliaryObjectiveId(0));
  EXPECT_THAT(storage.AuxiliaryObjectives(), IsEmpty());
  EXPECT_THAT(storage.SortedAuxiliaryObjectives(), IsEmpty());

  EXPECT_THAT(storage.Proto(), ObjectivesEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SimplePrimaryObjective) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const VariableId x(0);
  const VariableId y(2);
  const VariableId z(3);
  const auto diffs_range = collection_to_range(diffs);
  storage.set_maximize(kPrimaryObjectiveId, true, diffs_range);
  storage.set_priority(kPrimaryObjectiveId, 12, diffs_range);
  storage.set_offset(kPrimaryObjectiveId, 2.0, diffs_range);
  storage.set_linear_term(kPrimaryObjectiveId, y, 3.0, diffs_range);
  storage.set_linear_term(kPrimaryObjectiveId, z, 4.0, diffs_range);
  storage.set_quadratic_term(kPrimaryObjectiveId, x, y, 5.0, diffs_range);
  storage.set_quadratic_term(kPrimaryObjectiveId, z, z, 6.0, diffs_range);

  EXPECT_TRUE(storage.maximize(kPrimaryObjectiveId));
  EXPECT_EQ(storage.priority(kPrimaryObjectiveId), 12);
  EXPECT_EQ(storage.offset(kPrimaryObjectiveId), 2.0);
  EXPECT_THAT(storage.linear_terms(kPrimaryObjectiveId),
              UnorderedElementsAre(Pair(y, 3.0), Pair(z, 4.0)));
  EXPECT_THAT(storage.quadratic_terms(kPrimaryObjectiveId).Terms(),
              UnorderedElementsAre(FieldsAre(x, y, 5.0), FieldsAre(z, z, 6.0)));
  EXPECT_EQ(storage.linear_term(kPrimaryObjectiveId, x), 0.0);
  EXPECT_EQ(storage.linear_term(kPrimaryObjectiveId, y), 3.0);
  EXPECT_EQ(storage.quadratic_term(kPrimaryObjectiveId, x, x), 0.0);
  EXPECT_EQ(storage.quadratic_term(kPrimaryObjectiveId, x, y), 5.0);
  EXPECT_EQ(storage.name(kPrimaryObjectiveId), "");

  ObjectiveProto expected;
  expected.set_maximize(true);
  expected.set_priority(12);
  expected.set_offset(2.0);
  expected.mutable_linear_coefficients()->add_ids(2);
  expected.mutable_linear_coefficients()->add_values(3.0);
  expected.mutable_linear_coefficients()->add_ids(3);
  expected.mutable_linear_coefficients()->add_values(4.0);

  expected.mutable_quadratic_coefficients()->add_row_ids(0);
  expected.mutable_quadratic_coefficients()->add_column_ids(2);
  expected.mutable_quadratic_coefficients()->add_coefficients(5.0);

  expected.mutable_quadratic_coefficients()->add_row_ids(3);
  expected.mutable_quadratic_coefficients()->add_column_ids(3);
  expected.mutable_quadratic_coefficients()->add_coefficients(6.0);
  EXPECT_THAT(storage.Proto(), ObjectivesEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, PrimaryObjectiveName) {
  ObjectiveStorage storage("primary");
  EXPECT_EQ(storage.name(kPrimaryObjectiveId), "primary");
  EXPECT_EQ(storage.Proto().first.name(), "primary");
}

TEST(ObjectiveStorageTest, SimpleAuxiliaryObjective) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(
      /*priority=*/12, /*name=*/"c");
  const AuxiliaryObjectiveId d = storage.AddAuxiliaryObjective(
      /*priority=*/42, /*name=*/"d");
  const VariableId x(5);
  const VariableId y(6);
  storage.set_maximize(d, true, collection_to_range(diffs));
  storage.set_offset(c, 5.0, collection_to_range(diffs));
  storage.set_linear_term(d, x, 3.0, collection_to_range(diffs));
  storage.set_linear_term(d, y, 1.0, collection_to_range(diffs));
  storage.set_quadratic_term(c, x, y, 6.0, collection_to_range(diffs));
  storage.set_quadratic_term(d, y, y, 7.0, collection_to_range(diffs));

  EXPECT_FALSE(storage.maximize(c));
  EXPECT_TRUE(storage.maximize(d));
  EXPECT_EQ(storage.priority(c), 12);
  EXPECT_EQ(storage.priority(d), 42);
  EXPECT_EQ(storage.offset(c), 5.0);
  EXPECT_EQ(storage.offset(d), 0.0);
  EXPECT_THAT(storage.linear_terms(c), IsEmpty());
  EXPECT_THAT(storage.linear_terms(d),
              UnorderedElementsAre(Pair(x, 3.0), Pair(y, 1.0)));
  EXPECT_THAT(storage.quadratic_terms(c).Terms(),
              UnorderedElementsAre(FieldsAre(x, y, 6.0)));
  EXPECT_THAT(storage.quadratic_terms(d).Terms(),
              UnorderedElementsAre(FieldsAre(y, y, 7.0)));
  EXPECT_EQ(storage.linear_term(c, x), 0.0);
  EXPECT_EQ(storage.linear_term(c, y), 0.0);
  EXPECT_EQ(storage.linear_term(d, x), 3.0);
  EXPECT_EQ(storage.linear_term(d, y), 1.0);
  EXPECT_EQ(storage.quadratic_term(c, x, x), 0.0);
  EXPECT_EQ(storage.quadratic_term(c, x, y), 6.0);
  EXPECT_EQ(storage.quadratic_term(c, y, x), 6.0);
  EXPECT_EQ(storage.quadratic_term(c, y, y), 0.0);
  EXPECT_EQ(storage.quadratic_term(d, x, x), 0.0);
  EXPECT_EQ(storage.quadratic_term(d, x, y), 0.0);
  EXPECT_EQ(storage.quadratic_term(d, y, x), 0.0);
  EXPECT_EQ(storage.quadratic_term(d, y, y), 7.0);
  EXPECT_EQ(storage.name(c), "c");
  EXPECT_EQ(storage.name(d), "d");
  EXPECT_EQ(storage.num_auxiliary_objectives(), 2);
  EXPECT_TRUE(storage.contains(c));
  EXPECT_TRUE(storage.contains(d));
  EXPECT_FALSE(storage.contains(AuxiliaryObjectiveId(2)));
  EXPECT_EQ(storage.next_id(), AuxiliaryObjectiveId(2));
  EXPECT_THAT(storage.AuxiliaryObjectives(), UnorderedElementsAre(c, d));
  EXPECT_THAT(storage.SortedAuxiliaryObjectives(), ElementsAre(c, d));

  ObjectiveProto c_proto;
  c_proto.set_maximize(false);
  c_proto.set_priority(12);
  c_proto.set_offset(5.0);
  c_proto.mutable_quadratic_coefficients()->add_row_ids(x.value());
  c_proto.mutable_quadratic_coefficients()->add_column_ids(y.value());
  c_proto.mutable_quadratic_coefficients()->add_coefficients(6.0);
  c_proto.set_name("c");

  ObjectiveProto d_proto;
  d_proto.set_maximize(true);
  d_proto.set_priority(42);
  d_proto.set_offset(0.0);
  d_proto.mutable_linear_coefficients()->add_ids(x.value());
  d_proto.mutable_linear_coefficients()->add_ids(y.value());
  d_proto.mutable_linear_coefficients()->add_values(3.0);
  d_proto.mutable_linear_coefficients()->add_values(1.0);
  d_proto.mutable_quadratic_coefficients()->add_row_ids(y.value());
  d_proto.mutable_quadratic_coefficients()->add_column_ids(y.value());
  d_proto.mutable_quadratic_coefficients()->add_coefficients(7.0);
  d_proto.set_name("d");
  EXPECT_THAT(storage.Proto(), ObjectivesEquivTo({}, {{c.value(), c_proto},
                                                      {d.value(), d_proto}}));
}

TEST(AuxiliaryObjectiveStorageTest, EnsureNextIdAtLeast) {
  ObjectiveStorage storage;
  EXPECT_EQ(storage.next_id(), AuxiliaryObjectiveId(0));
  storage.ensure_next_id_at_least(AuxiliaryObjectiveId(6));
  EXPECT_EQ(storage.next_id(), AuxiliaryObjectiveId(6));
}

TEST(ObjectiveStorageTest, SetObjectiveDirectionChangedMaximize) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_maximize(kPrimaryObjectiveId, true, collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.set_direction_update(true);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}),
              Pair(EquivToProto(expected),
                   EquivToProto(AuxiliaryObjectivesUpdatesProto())));
}

TEST(ObjectiveStorageTest, SetObjectiveDirectionChangedMinimize) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  storage.set_maximize(kPrimaryObjectiveId, true, collection_to_range(diffs));
  diffs.push_back(Diff(storage, VariableId(0)));
  storage.set_maximize(kPrimaryObjectiveId, false, collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.set_direction_update(false);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, SetObjectiveDirectionUnchanged) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_maximize(kPrimaryObjectiveId, false, collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, SetAuxiliaryObjectiveDirection) {
  // We only integration test, other logic is tested on the primary objective.
  ObjectiveStorage storage;
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(12, "");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_maximize(c, true, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  (*expected.mutable_objective_updates())[c.value()].set_direction_update(true);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

TEST(ObjectiveStorageDiffTest, CreateAuxiliaryObjectiveThenSetDirection) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  storage.set_maximize(c, true, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  (*expected.mutable_new_objectives())[c.value()].set_maximize(true);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

TEST(ObjectiveStorageTest, SetObjectivePriorityChanged) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_priority(kPrimaryObjectiveId, 12, collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.set_priority_update(12);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, SetObjectivePriorityUnchanged) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_priority(kPrimaryObjectiveId, 0, collection_to_range(diffs));
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SetAuxiliaryObjectivePriority) {
  // We only integration test, other logic is tested on the primary objective.
  ObjectiveStorage storage;
  const AuxiliaryObjectiveId c =
      storage.AddAuxiliaryObjective(/*priority=*/12, "");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_priority(c, 73, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  (*expected.mutable_objective_updates())[c.value()].set_priority_update(73);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

TEST(AuxiliaryObjectiveStorageDiffTest,
     CreateAuxiliaryObjectiveThenSetPriority) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  const AuxiliaryObjectiveId c =
      storage.AddAuxiliaryObjective(/*priority=*/12, "");
  storage.set_priority(c, 73, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  (*expected.mutable_new_objectives())[c.value()].set_priority(73);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

TEST(ObjectiveStorageTest, SetObjectiveOffsetChanged) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_offset(kPrimaryObjectiveId, 3.0, collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.set_offset_update(3.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, SetObjectiveOffsetUnchanged) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_offset(kPrimaryObjectiveId, 0.0, collection_to_range(diffs));
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SetAuxiliaryObjectiveOffset) {
  // We only integration test, other logic is tested on the primary objective.
  ObjectiveStorage storage;
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_offset(c, 3.0, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  (*expected.mutable_objective_updates())[c.value()].set_offset_update(3.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

TEST(AuxiliaryObjectiveStorageDiffTest, CreateAuxiliaryObjectiveThenSetOffset) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  storage.set_offset(c, 3.0, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  (*expected.mutable_new_objectives())[c.value()].set_offset(3.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

////////////////////////////////////////////////////////////////////////////////
// Update and linear terms
////////////////////////////////////////////////////////////////////////////////

TEST(ObjectiveStorageTest, SetLinearVarAfterCheckpoint) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.mutable_linear_coefficients()->add_ids(0);
  expected.mutable_linear_coefficients()->add_values(2.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {x}),
              ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, SetLinearVarAfterCheckpointToZero) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_linear_term(kPrimaryObjectiveId, x, 0.0,
                          collection_to_range(diffs));
  EXPECT_THAT(storage.Update(diffs[0], {}, {x}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SetLinearVarAfterCheckpointThenDelete) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  storage.DeleteVariable(x, collection_to_range(diffs));
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SetLinearVarBeforeCheckpointWasZeroAndChanged) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, VariableId(1))};
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.mutable_linear_coefficients()->add_ids(0);
  expected.mutable_linear_coefficients()->add_values(2.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest,
     SetLinearVarBeforeCheckpointWasNonzeroAndChangedToNonzero) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const VariableId x(0);
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  diffs.push_back(Diff(storage, VariableId(1)));
  storage.set_linear_term(kPrimaryObjectiveId, x, 4.0,
                          collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.mutable_linear_coefficients()->add_ids(0);
  expected.mutable_linear_coefficients()->add_values(4.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest,
     SetLinearVarBeforeCheckpointWasNonzeroAndChangedToZero) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const VariableId x(0);
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  diffs.push_back(Diff(storage, VariableId(1)));
  storage.set_linear_term(kPrimaryObjectiveId, x, 0.0,
                          collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.mutable_linear_coefficients()->add_ids(0);
  expected.mutable_linear_coefficients()->add_values(0.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, SetLinearVarBeforeCheckpointThenDelete) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, VariableId(1))};
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  storage.DeleteVariable(x, collection_to_range(diffs));
  EXPECT_THAT(storage.Update(diffs[0], {x}, {}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SetAuxiliaryObjectiveLinearCoefficient) {
  // We only integration test, other logic is tested on the primary objective.
  ObjectiveStorage storage;
  const VariableId x(0);
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_linear_term(c, x, 2.0, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  {
    auto& update = (*expected.mutable_objective_updates())[c.value()];
    update.mutable_linear_coefficients()->add_ids(0);
    update.mutable_linear_coefficients()->add_values(2.0);
  }
  EXPECT_THAT(storage.Update(diffs[0], {}, {x}),
              ObjUpdateEquivTo({}, expected));
}

TEST(ObjectiveStorageTest, CreateAuxiliaryObjectiveThenSetLinearCoefficient) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  storage.set_linear_term(c, x, 2.0, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  {
    auto& update = (*expected.mutable_new_objectives())[c.value()];
    update.mutable_linear_coefficients()->add_ids(0);
    update.mutable_linear_coefficients()->add_values(2.0);
  }
  EXPECT_THAT(storage.Update(diffs[0], {}, {x}),
              ObjUpdateEquivTo({}, expected));
}

////////////////////////////////////////////////////////////////////////////////
// Update and quadratic terms
////////////////////////////////////////////////////////////////////////////////

TEST(ObjectiveStorageTest, SetQuadraticVarAfterCheckpoint) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 2.0,
                             collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.mutable_quadratic_coefficients()->add_row_ids(0);
  expected.mutable_quadratic_coefficients()->add_column_ids(0);
  expected.mutable_quadratic_coefficients()->add_coefficients(2.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {x}),
              ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, SetQuadraticVarAfterCheckpointToZero) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 0.0,
                             collection_to_range(diffs));
  EXPECT_THAT(storage.Update(diffs[0], {}, {x}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SetQuadraticVarAfterCheckpointThenDelete) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 2.0,
                             collection_to_range(diffs));
  storage.DeleteVariable(x, collection_to_range(diffs));
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SetQuadraticVarBeforeCheckpointWasZeroAndChanged) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, VariableId(1))};
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 2.0,
                             collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.mutable_quadratic_coefficients()->add_row_ids(0);
  expected.mutable_quadratic_coefficients()->add_column_ids(0);
  expected.mutable_quadratic_coefficients()->add_coefficients(2.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest,
     SetQuadraticVarBeforeCheckpointWasNonzeroAndChangedToNonzero) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const VariableId x(0);
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 2.0,
                             collection_to_range(diffs));
  diffs.push_back(Diff(storage, VariableId(1)));
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 4.0,
                             collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.mutable_quadratic_coefficients()->add_row_ids(0);
  expected.mutable_quadratic_coefficients()->add_column_ids(0);
  expected.mutable_quadratic_coefficients()->add_coefficients(4.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest,
     SetQuadraticVarBeforeCheckpointWasNonzeroAndChangedToZero) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const VariableId x(0);
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 2.0,
                             collection_to_range(diffs));
  diffs.push_back(Diff(storage, VariableId(1)));
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 0.0,
                             collection_to_range(diffs));
  ObjectiveUpdatesProto expected;
  expected.mutable_quadratic_coefficients()->add_row_ids(0);
  expected.mutable_quadratic_coefficients()->add_column_ids(0);
  expected.mutable_quadratic_coefficients()->add_coefficients(0.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, SetQuadraticVarBeforeCheckpointThenDelete) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, VariableId(1))};
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 2.0,
                             collection_to_range(diffs));
  storage.DeleteVariable(x, collection_to_range(diffs));
  EXPECT_THAT(storage.Update(diffs[0], {x}, {}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, SetAuxiliaryObjectiveQuadraticCoefficient) {
  // We only integration test, other logic is tested on the primary objective.
  ObjectiveStorage storage;
  const VariableId x(0);
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_quadratic_term(c, x, x, 2.0, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  {
    auto& update = (*expected.mutable_objective_updates())[c.value()];
    update.mutable_quadratic_coefficients()->add_row_ids(x.value());
    update.mutable_quadratic_coefficients()->add_column_ids(x.value());
    update.mutable_quadratic_coefficients()->add_coefficients(2.0);
  }
  EXPECT_THAT(storage.Update(diffs[0], {}, {x}),
              ObjUpdateEquivTo({}, expected));
}

TEST(ObjectiveStorageTest,
     CreateAuxiliaryObjectiveThenSetQuadraticCoefficient) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  storage.set_quadratic_term(c, x, x, 2.0, collection_to_range(diffs));
  AuxiliaryObjectivesUpdatesProto expected;
  {
    auto& update = (*expected.mutable_new_objectives())[c.value()];
    update.mutable_quadratic_coefficients()->add_row_ids(x.value());
    update.mutable_quadratic_coefficients()->add_column_ids(x.value());
    update.mutable_quadratic_coefficients()->add_coefficients(2.0);
  }
  EXPECT_THAT(storage.Update(diffs[0], {}, {x}),
              ObjUpdateEquivTo({}, expected));
}

////////////////////////////////////////////////////////////////////////////////
// Update and clear
////////////////////////////////////////////////////////////////////////////////

TEST(ObjectiveStorageTest, ClearExistingPrimaryObjectiveData) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const VariableId x(0);
  storage.set_offset(kPrimaryObjectiveId, 4.0, collection_to_range(diffs));
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 3.0,
                             collection_to_range(diffs));
  diffs.push_back(Diff(storage, VariableId(1)));
  storage.Clear(kPrimaryObjectiveId, collection_to_range(diffs));

  EXPECT_THAT(storage.Proto(), ObjectivesEquivTo({}, {}));

  ObjectiveUpdatesProto expected;
  expected.set_offset_update(0.0);
  expected.mutable_linear_coefficients()->add_ids(0);
  expected.mutable_linear_coefficients()->add_values(0.0);
  expected.mutable_quadratic_coefficients()->add_row_ids(0);
  expected.mutable_quadratic_coefficients()->add_column_ids(0);
  expected.mutable_quadratic_coefficients()->add_coefficients(0.0);
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo(expected, {}));
}

TEST(ObjectiveStorageTest, ClearExistingAuxiliaryObjectiveData) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const VariableId x(0);
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  diffs.push_back(Diff(storage, VariableId(1)));
  storage.set_offset(c, 4.0, collection_to_range(diffs));
  storage.set_linear_term(c, x, 2.0, collection_to_range(diffs));
  storage.set_quadratic_term(c, x, x, 3.0, collection_to_range(diffs));
  storage.Clear(c, collection_to_range(diffs));

  EXPECT_THAT(storage.Proto(), ObjectivesEquivTo({}, {{c.value(), {}}}));

  AuxiliaryObjectivesUpdatesProto expected;
  {
    auto& update = (*expected.mutable_objective_updates())[c.value()];
    update.set_offset_update(0.0);
    update.mutable_linear_coefficients()->add_ids(0);
    update.mutable_linear_coefficients()->add_values(0.0);
    update.mutable_quadratic_coefficients()->add_row_ids(0);
    update.mutable_quadratic_coefficients()->add_column_ids(0);
    update.mutable_quadratic_coefficients()->add_coefficients(0.0);
  }
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

TEST(ObjectiveStorageTest, ClearNewPrimaryObjectiveData) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  storage.set_offset(kPrimaryObjectiveId, 4.0, collection_to_range(diffs));
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 3.0,
                             collection_to_range(diffs));

  storage.Clear(kPrimaryObjectiveId, collection_to_range(diffs));

  EXPECT_THAT(storage.Proto(), ObjectivesEquivTo({}, {}));
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, {}));
}

TEST(ObjectiveStorageTest, ClearNewAuxiliaryObjectiveData) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, x)};
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  storage.set_offset(c, 4.0, collection_to_range(diffs));
  storage.set_linear_term(c, x, 2.0, collection_to_range(diffs));
  storage.set_quadratic_term(c, x, x, 3.0, collection_to_range(diffs));

  storage.Clear(c, collection_to_range(diffs));

  EXPECT_THAT(storage.Proto(), ObjectivesEquivTo({}, {{c.value(), {}}}));
  AuxiliaryObjectivesUpdatesProto expected;
  (*expected.mutable_new_objectives())[c.value()];
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

////////////////////////////////////////////////////////////////////////////////
// diff_is_empty
////////////////////////////////////////////////////////////////////////////////

TEST(ObjectiveStorageTest, EmptyDiffEmpty) {
  ObjectiveStorage storage;
  EXPECT_TRUE(storage.diff_is_empty(Diff(storage, VariableId(0))));
}

TEST(ObjectiveStorageTest, DiffIsNotEmptyDirection) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_maximize(kPrimaryObjectiveId, true, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(ObjectiveStorageTest, DiffIsNotEmptyPriority) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_priority(kPrimaryObjectiveId, 12, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(ObjectiveStorageTest, DiffIsNotEmptyOffset) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_offset(kPrimaryObjectiveId, 3.0, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(ObjectiveStorageTest, DiffIsNotEmptyLinearCoefficient) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, VariableId(1))};
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(ObjectiveStorageTest, DiffIsNotEmptyQuadCoefficient) {
  ObjectiveStorage storage;
  const VariableId x(0);
  std::vector<Diff> diffs = {Diff(storage, VariableId(1))};
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 2.0,
                             collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(ObjectiveStorageTest, DiffIsNotEmptyAuxiliaryUpdate) {
  // We only integration test, other logic is tested on the primary objective.
  ObjectiveStorage storage;
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.set_maximize(c, true, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(ObjectiveStorageTest, DiffIsNotEmptyDeletedAuxiliaryObjective) {
  ObjectiveStorage storage;
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.Delete(c, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(ObjectiveStorageTest, DiffIsNotEmptyNewAuxiliaryObjective) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.AddAuxiliaryObjective(0, "");
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

////////////////////////////////////////////////////////////////////////////////
// Advance checkpoint
////////////////////////////////////////////////////////////////////////////////

TEST(ObjectiveStorageTest, AdvanceCheckpoint) {
  ObjectiveStorage storage;
  const VariableId x(0);
  const VariableId y(1);
  VariableId checkpoint = y;
  std::vector<Diff> diffs = {Diff(storage, checkpoint)};
  storage.set_linear_term(kPrimaryObjectiveId, x, 2.0,
                          collection_to_range(diffs));
  storage.set_quadratic_term(kPrimaryObjectiveId, x, x, 4.0,
                             collection_to_range(diffs));
  storage.set_offset(kPrimaryObjectiveId, 6.0, collection_to_range(diffs));
  storage.set_maximize(kPrimaryObjectiveId, true, collection_to_range(diffs));
  storage.set_priority(kPrimaryObjectiveId, 12, collection_to_range(diffs));
  storage.set_linear_term(kPrimaryObjectiveId, y, 3.0,
                          collection_to_range(diffs));
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "c");
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
  checkpoint = VariableId(2);
  storage.AdvanceCheckpointInDiff(checkpoint, diffs[0]);
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  EXPECT_EQ(diffs[0].objective_checkpoint, AuxiliaryObjectiveId(c.value() + 1));
  EXPECT_EQ(diffs[0].variable_checkpoint, checkpoint);

  storage.Delete(c, collection_to_range(diffs));
  const AuxiliaryObjectiveId d = storage.AddAuxiliaryObjective(0, "e");

  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
  storage.AdvanceCheckpointInDiff(checkpoint, diffs[0]);
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  EXPECT_EQ(diffs[0].objective_checkpoint, AuxiliaryObjectiveId(d.value() + 1));
  EXPECT_EQ(diffs[0].variable_checkpoint, checkpoint);
}

////////////////////////////////////////////////////////////////////////////////
// Auxiliary objectives
////////////////////////////////////////////////////////////////////////////////

TEST(ObjectiveStorageTest, DeleteAuxiliaryObjectives) {
  ObjectiveStorage storage;
  std::vector<Diff> diffs;
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  const AuxiliaryObjectiveId d = storage.AddAuxiliaryObjective(0, "");
  diffs.push_back(Diff(storage, VariableId(0)));
  storage.Delete(d, collection_to_range(diffs));

  EXPECT_THAT(storage.Proto(), ObjectivesEquivTo({}, {{c.value(), {}}}));

  AuxiliaryObjectivesUpdatesProto expected;
  expected.add_deleted_objective_ids(d.value());
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

TEST(ObjectiveStorageDiffTest, CreateAuxiliaryObjectives) {
  ObjectiveStorage storage;
  const AuxiliaryObjectiveId c = storage.AddAuxiliaryObjective(0, "");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  const AuxiliaryObjectiveId d = storage.AddAuxiliaryObjective(0, "");
  storage.ensure_next_id_at_least(AuxiliaryObjectiveId(4));
  const AuxiliaryObjectiveId e = storage.AddAuxiliaryObjective(0, "");

  EXPECT_THAT(storage.Proto(),
              ObjectivesEquivTo(
                  {}, {{c.value(), {}}, {d.value(), {}}, {e.value(), {}}}));

  AuxiliaryObjectivesUpdatesProto expected;
  (*expected.mutable_new_objectives())[d.value()];
  (*expected.mutable_new_objectives())[e.value()];
  EXPECT_THAT(storage.Update(diffs[0], {}, {}), ObjUpdateEquivTo({}, expected));
}

}  // namespace operations_research::math_opt
