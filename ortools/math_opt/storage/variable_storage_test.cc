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

#include "ortools/math_opt/storage/variable_storage.h"

#include <algorithm>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/range.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::EquivToProto;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

TEST(VariableStorageTest, Empty) {
  const VariableStorage storage;
  EXPECT_EQ(storage.size(), 0);
  EXPECT_FALSE(storage.contains(VariableId(0)));
  EXPECT_FALSE(storage.contains(VariableId(1)));
  EXPECT_EQ(storage.next_id().value(), 0);
  EXPECT_THAT(storage.Variables(), IsEmpty());
  EXPECT_THAT(storage.SortedVariables(), IsEmpty());
  EXPECT_THAT(storage.VariablesFrom(VariableId(0)), IsEmpty());
  EXPECT_THAT(storage.Proto(), EquivToProto(VariablesProto()));
}

TEST(VariableStorageTest, Nonempty) {
  VariableStorage storage;
  const VariableId x = storage.Add(1.0, 3.0, false, "x");
  const VariableId y = storage.Add(2.0, 2.0, true, "y");

  EXPECT_EQ(storage.lower_bound(x), 1.0);
  EXPECT_EQ(storage.upper_bound(x), 3.0);
  EXPECT_FALSE(storage.is_integer(x));
  EXPECT_EQ(storage.name(x), "x");

  EXPECT_EQ(storage.lower_bound(y), 2.0);
  EXPECT_EQ(storage.upper_bound(y), 2.0);
  EXPECT_TRUE(storage.is_integer(y));
  EXPECT_EQ(storage.name(y), "y");

  EXPECT_EQ(storage.size(), 2);
  EXPECT_TRUE(storage.contains(x));
  EXPECT_TRUE(storage.contains(y));
  EXPECT_FALSE(storage.contains(std::max(x, y).Next()));
  EXPECT_EQ(storage.next_id().value(), 2);
  EXPECT_THAT(storage.Variables(), UnorderedElementsAre(x, y));
  EXPECT_THAT(storage.SortedVariables(), ElementsAre(x, y));
  EXPECT_THAT(storage.VariablesFrom(x), ElementsAre(x, y));
  EXPECT_THAT(storage.VariablesFrom(y), ElementsAre(y));

  VariablesProto expected;
  expected.add_ids(0);
  expected.add_ids(1);
  expected.add_lower_bounds(1.0);
  expected.add_lower_bounds(2.0);
  expected.add_upper_bounds(3.0);
  expected.add_upper_bounds(2.0);
  expected.add_integers(false);
  expected.add_integers(true);
  expected.add_names("x");
  expected.add_names("y");

  EXPECT_THAT(storage.Proto(), EquivToProto(expected));
}

TEST(VariableStorageTest, Setters) {
  std::vector<VariableStorage::Diff> diffs;
  VariableStorage storage;
  const VariableId x = storage.Add(1.0, 3.0, false, "x");
  const VariableId y = storage.Add(2.0, 2.0, true, "y");

  storage.set_lower_bound(x, -1.0, collection_to_range(diffs));
  storage.set_upper_bound(y, 5.0, collection_to_range(diffs));
  storage.set_integer(x, true, collection_to_range(diffs));

  EXPECT_EQ(storage.lower_bound(x), -1.0);
  EXPECT_EQ(storage.upper_bound(x), 3.0);
  EXPECT_EQ(storage.is_integer(x), true);

  EXPECT_EQ(storage.lower_bound(y), 2.0);
  EXPECT_EQ(storage.upper_bound(y), 5.0);
  EXPECT_EQ(storage.is_integer(y), true);

  storage.set_integer(y, false, collection_to_range(diffs));
  EXPECT_EQ(storage.is_integer(x), true);
  EXPECT_FALSE(storage.is_integer(y));

  VariablesProto expected;
  expected.add_ids(0);
  expected.add_ids(1);
  expected.add_lower_bounds(-1.0);
  expected.add_lower_bounds(2.0);
  expected.add_upper_bounds(3.0);
  expected.add_upper_bounds(5.0);
  expected.add_integers(true);
  expected.add_integers(false);
  expected.add_names("x");
  expected.add_names("y");

  EXPECT_THAT(storage.Proto(), EquivToProto(expected));
}

TEST(VariableStorageTest, EnsureNextId) {
  VariableStorage storage;
  storage.ensure_next_id_at_least(VariableId(3));
  const VariableId x = storage.Add(1.0, 3.0, false, "x");
  EXPECT_EQ(x.value(), 3);

  storage.ensure_next_id_at_least(VariableId(2));
  const VariableId y = storage.Add(2.0, 2.0, true, "y");
  EXPECT_EQ(y.value(), 4);

  EXPECT_FALSE(storage.contains(VariableId(1)));
  EXPECT_TRUE(storage.contains(VariableId(3)));
  EXPECT_TRUE(storage.contains(VariableId(4)));
  EXPECT_FALSE(storage.contains(VariableId(5)));

  EXPECT_EQ(storage.size(), 2);
  EXPECT_EQ(storage.next_id().value(), 5);
  EXPECT_THAT(storage.Variables(), UnorderedElementsAre(x, y));
  EXPECT_THAT(storage.SortedVariables(), ElementsAre(x, y));
  EXPECT_THAT(storage.VariablesFrom(VariableId(1)), ElementsAre(x, y));

  VariablesProto expected;
  expected.add_ids(3);
  expected.add_ids(4);
  expected.add_lower_bounds(1.0);
  expected.add_lower_bounds(2.0);
  expected.add_upper_bounds(3.0);
  expected.add_upper_bounds(2.0);
  expected.add_integers(false);
  expected.add_integers(true);
  expected.add_names("x");
  expected.add_names("y");

  EXPECT_THAT(storage.Proto(), EquivToProto(expected));
}

TEST(VariableStorageTest, Delete) {
  std::vector<VariableStorage::Diff> diffs;
  VariableStorage storage;
  const VariableId x = storage.Add(1.0, 3.0, false, "x");
  const VariableId y = storage.Add(2.0, 2.0, true, "y");
  const VariableId z = storage.Add(-1.0, 0.0, false, "z");
  EXPECT_EQ(storage.size(), 3);
  storage.Delete(y, collection_to_range(diffs));
  EXPECT_EQ(storage.size(), 2);
  EXPECT_TRUE(storage.contains(x));
  EXPECT_FALSE(storage.contains(y));
  EXPECT_TRUE(storage.contains(z));

  EXPECT_EQ(storage.next_id().value(), 3);
  EXPECT_THAT(storage.Variables(), UnorderedElementsAre(x, z));
  EXPECT_THAT(storage.SortedVariables(), ElementsAre(x, z));
  EXPECT_THAT(storage.VariablesFrom(y), ElementsAre(z));
  EXPECT_THAT(storage.VariablesFrom(x), ElementsAre(x, z));

  VariablesProto expected;
  expected.add_ids(0);
  expected.add_ids(2);
  expected.add_lower_bounds(1.0);
  expected.add_lower_bounds(-1.0);
  expected.add_upper_bounds(3.0);
  expected.add_upper_bounds(0.0);
  expected.add_integers(false);
  expected.add_integers(false);
  expected.add_names("x");
  expected.add_names("z");

  EXPECT_THAT(storage.Proto(), EquivToProto(expected));
}

TEST(VariableDiffTest, Empty) {
  const VariableStorage storage;
  const VariableStorage::Diff diff(storage);
  EXPECT_EQ(diff.checkpoint.value(), 0);
  EXPECT_TRUE(storage.diff_is_empty(diff));
  EXPECT_THAT(diff.deleted, IsEmpty());
}

TEST(VariableDiffTest, NotEmpty) {
  VariableStorage storage;
  const VariableId x = storage.Add(0.0, 1.0, false, "x");
  std::vector<VariableStorage::Diff> diffs = {VariableStorage::Diff(storage)};
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.set_lower_bound(x, -1.0, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));

  storage.AdvanceCheckpointInDiff(diffs[0]);
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.set_upper_bound(x, 2.0, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));

  storage.AdvanceCheckpointInDiff(diffs[0]);
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.set_integer(x, true, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));

  storage.AdvanceCheckpointInDiff(diffs[0]);
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.Add(0.0, 1.0, false, "y");
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(VariableDiffTest, SetToExistingValueNoDiff) {
  VariableStorage storage;
  const VariableId x = storage.Add(0.0, 1.0, false, "x");
  std::vector<VariableStorage::Diff> diffs = {VariableStorage::Diff(storage)};
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.set_lower_bound(x, 0.0, collection_to_range(diffs));
  storage.set_upper_bound(x, 1.0, collection_to_range(diffs));
  storage.set_integer(x, false, collection_to_range(diffs));
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
}

TEST(VariableDiffTest, DeleteAfterCheckpoint) {
  VariableStorage storage;
  storage.Add(0.0, 1.0, false, "x");
  std::vector<VariableStorage::Diff> diffs = {VariableStorage::Diff(storage)};
  const VariableId y = storage.Add(0.0, 1.0, false, "y");
  storage.Delete(y, collection_to_range(diffs));
  EXPECT_THAT(diffs[0].deleted, IsEmpty());
}

TEST(VariableDiffTest, EmptyExport) {
  const VariableStorage variables;
  const VariableStorage::Diff diff(variables);
  const VariableStorage::UpdateResult update_result = variables.Update(diff);
  EXPECT_THAT(update_result.deleted, IsEmpty());
  EXPECT_THAT(update_result.creates, EquivToProto(VariablesProto()));
  EXPECT_THAT(update_result.updates, EquivToProto(VariableUpdatesProto()));
}

TEST(VariableDiffTest, ExportCreatesAll) {
  VariableStorage variables;
  const VariableStorage::Diff diff(variables);
  variables.Add(1.0, 3.0, false, "x");
  variables.Add(2.0, 2.0, true, "y");

  const VariableStorage::UpdateResult update_result = variables.Update(diff);
  EXPECT_THAT(update_result.deleted, IsEmpty());
  VariablesProto expected;
  expected.add_ids(0);
  expected.add_ids(1);
  expected.add_lower_bounds(1.0);
  expected.add_lower_bounds(2.0);
  expected.add_upper_bounds(3.0);
  expected.add_upper_bounds(2.0);
  expected.add_integers(false);
  expected.add_integers(true);
  expected.add_names("x");
  expected.add_names("y");
  EXPECT_THAT(update_result.creates, EquivToProto(expected));
  EXPECT_THAT(update_result.updates, EquivToProto(VariableUpdatesProto()));
}
TEST(VariableDiffTest, ExportCreatesSome) {
  VariableStorage variables;
  variables.Add(1.0, 3.0, false, "x");
  const VariableStorage::Diff diff(variables);
  variables.Add(2.0, 2.0, true, "y");
  const VariableStorage::UpdateResult update_result = variables.Update(diff);
  EXPECT_THAT(update_result.deleted, IsEmpty());
  VariablesProto expected;
  expected.add_ids(1);
  expected.add_lower_bounds(2.0);
  expected.add_upper_bounds(2.0);
  expected.add_integers(true);
  expected.add_names("y");
  EXPECT_THAT(update_result.creates, EquivToProto(expected));
  EXPECT_THAT(update_result.updates, EquivToProto(VariableUpdatesProto()));
}
TEST(VariableDiffTest, ExportCreatesNone) {
  VariableStorage variables;
  variables.Add(1.0, 3.0, false, "x");
  variables.Add(2.0, 2.0, true, "y");
  const VariableStorage::Diff diff(variables);
  const VariableStorage::UpdateResult update_result = variables.Update(diff);
  EXPECT_THAT(update_result.deleted, IsEmpty());
  EXPECT_THAT(update_result.creates, EquivToProto(VariablesProto()));
  EXPECT_THAT(update_result.updates, EquivToProto(VariableUpdatesProto()));
}

TEST(VariableDiffTest, ExportDeletes) {
  VariableStorage variables;
  for (int i = 0; i < 5; ++i) {
    variables.Add(0, 1, false, "");
  }
  std::vector<VariableStorage::Diff> diffs = {VariableStorage::Diff(variables)};
  variables.Delete(VariableId(1), collection_to_range(diffs));
  variables.Delete(VariableId(2), collection_to_range(diffs));

  const VariableStorage::UpdateResult update_result =
      variables.Update(diffs[0]);
  EXPECT_THAT(update_result.deleted, ElementsAre(1, 2));
  EXPECT_THAT(update_result.creates, EquivToProto(VariablesProto()));
  EXPECT_THAT(update_result.updates, EquivToProto(VariableUpdatesProto()));
}

TEST(VariableDiffTest, ExportUpdates) {
  VariableStorage variables;
  VariableId x = variables.Add(0.0, 1.0, false, "x");
  VariableId y = variables.Add(2.0, 3.0, true, "y");
  std::vector<VariableStorage::Diff> diffs = {VariableStorage::Diff(variables)};
  VariableId z = variables.Add(4.0, 5.0, false, "z");
  variables.set_lower_bound(x, -1.0, collection_to_range(diffs));
  variables.set_lower_bound(y, -1.0, collection_to_range(diffs));
  variables.set_lower_bound(z, -1.0, collection_to_range(diffs));

  variables.set_upper_bound(x, 10.0, collection_to_range(diffs));
  variables.set_upper_bound(z, 10.0, collection_to_range(diffs));

  variables.set_integer(y, false, collection_to_range(diffs));
  variables.set_integer(z, true, collection_to_range(diffs));

  const VariableStorage::UpdateResult update_result =
      variables.Update(diffs[0]);
  EXPECT_THAT(update_result.deleted, IsEmpty());
  VariablesProto creates;
  creates.add_ids(2);
  creates.add_lower_bounds(-1.0);
  creates.add_upper_bounds(10.0);
  creates.add_integers(true);
  creates.add_names("z");
  EXPECT_THAT(update_result.creates, EquivToProto(creates));
  VariableUpdatesProto updates;
  updates.mutable_lower_bounds()->add_ids(0);
  updates.mutable_lower_bounds()->add_ids(1);
  updates.mutable_lower_bounds()->add_values(-1.0);
  updates.mutable_lower_bounds()->add_values(-1.0);

  updates.mutable_upper_bounds()->add_ids(0);
  updates.mutable_upper_bounds()->add_values(10.0);

  updates.mutable_integers()->add_ids(1);
  updates.mutable_integers()->add_values(false);
  EXPECT_THAT(update_result.updates, EquivToProto(updates));
}

}  // namespace
}  // namespace operations_research::math_opt
