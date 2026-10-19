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

#include "ortools/math_opt/storage/atomic_constraint_storage.h"

#include <stdint.h>

#include <initializer_list>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/algorithms/auto_radix_sort.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/storage/atomic_constraint_storage_test.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/range.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::EquivToProto;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

DEFINE_STRONG_INT_TYPE(TestId, int64_t);
using TestConstraintProto = AtomicConstraintStorageTestProto;
using TestUpdateProto = AtomicConstraintStorageTestUpdateProto;

struct TestData {
  using IdType = TestId;
  using ProtoType = TestConstraintProto;
  using UpdatesProtoType = TestUpdateProto;

  std::vector<VariableId> RelatedVariables() const {
    return {variables.begin(), variables.end()};
  }
  void DeleteVariable(VariableId var) { variables.erase(var); }
  ProtoType Proto() const {
    ProtoType proto;
    for (const VariableId var : variables) {
      proto.add_ids(var.value());
    }
    AutoRadixSort(*proto.mutable_ids());
    return proto;
  }
  static TestData FromProto(const ProtoType& in_proto) {
    return {.variables = {in_proto.ids().begin(), in_proto.ids().end()}};
  }

  absl::flat_hash_set<VariableId> variables;
};

using TestStorage = AtomicConstraintStorage<TestData>;
using TestDiff = TestStorage::Diff;

TestConstraintProto ConstraintProto(std::initializer_list<int> raw_ids) {
  std::vector<int> ids(raw_ids.begin(), raw_ids.end());
  AutoRadixSort(ids);
  TestConstraintProto proto;
  for (const int id : ids) {
    proto.add_ids(id);
  }
  return proto;
}

TEST(AtomicConstraintStorageTest, Empty) {
  const TestStorage storage;
  EXPECT_EQ(storage.size(), 0);
  EXPECT_FALSE(storage.contains(TestId(0)));
  EXPECT_FALSE(storage.contains(TestId(1)));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(0)), IsEmpty());
  EXPECT_EQ(storage.next_id(), TestId(0));
  EXPECT_THAT(storage.Constraints(), IsEmpty());
  EXPECT_THAT(storage.SortedConstraints(), IsEmpty());
  EXPECT_THAT(storage.Proto(), IsEmpty());
}

TEST(AtomicConstraintStorageTest, ReadSimpleConstraints) {
  TestStorage storage;
  const TestId c = storage.AddConstraint(TestData());
  const TestId d =
      storage.AddConstraint(TestData{.variables = {VariableId(1)}});
  EXPECT_EQ(storage.size(), 2);
  EXPECT_TRUE(storage.contains(c));
  EXPECT_TRUE(storage.contains(d));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(0)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(1)),
              UnorderedElementsAre(TestId(d)));
  EXPECT_FALSE(storage.contains(TestId(2)));

  EXPECT_THAT(storage.Constraints(), UnorderedElementsAre(c, d));
  EXPECT_THAT(storage.SortedConstraints(), ElementsAre(c, d));

  EXPECT_EQ(storage.next_id().value(), d.value() + 1);
  EXPECT_THAT(
      storage.Proto(),
      UnorderedElementsAre(Pair(0, EquivToProto(ConstraintProto({}))),
                           Pair(1, EquivToProto(ConstraintProto({1})))));
}

TEST(AtomicConstraintStorageTest, ReadSimpleConstraintsFromMap) {
  TestStorage storage;
  google::protobuf::Map<int64_t, TestConstraintProto> constraints;
  const TestId c(0);
  const TestId d(2);
  constraints[c.value()];
  constraints[d.value()].add_ids(1);
  storage.AddConstraints(constraints);

  EXPECT_EQ(storage.size(), 2);
  EXPECT_TRUE(storage.contains(c));
  EXPECT_FALSE(storage.contains(TestId(1)));
  EXPECT_TRUE(storage.contains(d));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(0)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(1)),
              UnorderedElementsAre(TestId(d)));
  EXPECT_FALSE(storage.contains(TestId(3)));

  EXPECT_THAT(storage.Constraints(), UnorderedElementsAre(c, d));
  EXPECT_THAT(storage.SortedConstraints(), ElementsAre(c, d));

  EXPECT_EQ(storage.next_id().value(), d.value() + 1);
  EXPECT_THAT(
      storage.Proto(),
      UnorderedElementsAre(Pair(0, EquivToProto(ConstraintProto({}))),
                           Pair(2, EquivToProto(ConstraintProto({1})))));
}

TEST(AtomicConstraintStorageTest, DeleteConstraintIds) {
  std::vector<TestDiff> diffs;
  TestStorage storage;
  const TestId c = storage.AddConstraint(TestData());
  const TestId d =
      storage.AddConstraint(TestData{.variables = {VariableId(1)}});
  ASSERT_TRUE(storage.contains(d));
  storage.Delete(d, collection_to_range(diffs));
  EXPECT_FALSE(storage.contains(d));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(0)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(1)), IsEmpty());
  EXPECT_THAT(storage.Constraints(), UnorderedElementsAre(c));
  EXPECT_THAT(storage.SortedConstraints(), ElementsAre(c));

  const TestId e = storage.AddConstraint(TestData());
  EXPECT_TRUE(storage.contains(e));
  EXPECT_THAT(storage.Constraints(), UnorderedElementsAre(c, e));
  EXPECT_THAT(storage.SortedConstraints(), ElementsAre(c, e));
  EXPECT_THAT(
      storage.Proto(),
      UnorderedElementsAre(Pair(c.value(), EquivToProto(ConstraintProto({}))),
                           Pair(e.value(), EquivToProto(ConstraintProto({})))));

  storage.Delete(c, collection_to_range(diffs));
  EXPECT_FALSE(storage.contains(c));
  EXPECT_THAT(storage.Constraints(), UnorderedElementsAre(e));
  EXPECT_THAT(storage.SortedConstraints(), ElementsAre(e));
  EXPECT_THAT(
      storage.Proto(),
      UnorderedElementsAre(Pair(e.value(), EquivToProto(ConstraintProto({})))));
}

TEST(AtomicConstraintStorageTest, DeleteVariableIds) {
  TestStorage storage;
  const TestId c = storage.AddConstraint(
      TestData{.variables = {VariableId(2), VariableId(1)}});
  const TestId d = storage.AddConstraint(
      TestData{.variables = {VariableId(3), VariableId(1)}});
  ASSERT_THAT(storage.Proto(),
              UnorderedElementsAre(
                  Pair(c.value(), EquivToProto(ConstraintProto({1, 2}))),
                  Pair(d.value(), EquivToProto(ConstraintProto({1, 3})))));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(0)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(1)),
              UnorderedElementsAre(TestId(c), TestId(d)));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(2)),
              UnorderedElementsAre(TestId(c)));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(3)),
              UnorderedElementsAre(TestId(d)));

  storage.DeleteVariable(VariableId(0));
  EXPECT_THAT(storage.Proto(),
              UnorderedElementsAre(
                  Pair(c.value(), EquivToProto(ConstraintProto({1, 2}))),
                  Pair(d.value(), EquivToProto(ConstraintProto({1, 3})))));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(0)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(1)),
              UnorderedElementsAre(TestId(c), TestId(d)));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(2)),
              UnorderedElementsAre(TestId(c)));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(3)),
              UnorderedElementsAre(TestId(d)));

  storage.DeleteVariable(VariableId(1));
  EXPECT_THAT(storage.Proto(),
              UnorderedElementsAre(
                  Pair(c.value(), EquivToProto(ConstraintProto({2}))),
                  Pair(d.value(), EquivToProto(ConstraintProto({3})))));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(0)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(1)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(2)),
              UnorderedElementsAre(TestId(c)));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(3)),
              UnorderedElementsAre(TestId(d)));

  storage.DeleteVariable(VariableId(2));
  EXPECT_THAT(storage.Proto(),
              UnorderedElementsAre(
                  Pair(c.value(), EquivToProto(ConstraintProto({}))),
                  Pair(d.value(), EquivToProto(ConstraintProto({3})))));
  EXPECT_THAT(storage.RelatedConstraints(VariableId(0)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(1)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(2)), IsEmpty());
  EXPECT_THAT(storage.RelatedConstraints(VariableId(3)),
              UnorderedElementsAre(TestId(d)));
}

TEST(AtomicConstraintStorageTest, EmptyDiff) {
  const TestStorage storage;
  const TestDiff diff(storage);
  EXPECT_TRUE(storage.diff_is_empty(diff));
  EXPECT_EQ(diff.checkpoint, TestId(0));
  EXPECT_THAT(storage.Update(diff), EquivToProto(TestUpdateProto()));
}

TEST(AtomicConstraintDiffTest, NewConstraint) {
  TestStorage storage;
  const VariableId v(0);
  const TestDiff diff(storage);
  const TestId c = storage.AddConstraint(TestData{.variables = {v}});

  EXPECT_FALSE(storage.diff_is_empty(diff));
  EXPECT_EQ(diff.checkpoint, c);
  TestUpdateProto expected;
  (*expected.mutable_new_constraints())[0] = ConstraintProto({0});
  EXPECT_THAT(storage.Update(diff), EquivToProto(expected));
}

TEST(AtomicConstraintDiffTest, ConstraintFromBeforeCheckpoint) {
  TestStorage storage;
  const VariableId v(0);
  TestDiff diff(storage);
  const TestId c = storage.AddConstraint(TestData{.variables = {v}});
  storage.AdvanceCheckpointInDiff(diff);

  EXPECT_TRUE(storage.diff_is_empty(diff));
  EXPECT_EQ(diff.checkpoint.value(), c.value() + 1);
  EXPECT_THAT(storage.Update(diff), EquivToProto(TestUpdateProto()));
}

TEST(AtomicConstraintDiffTest, DeletedConstraint) {
  TestStorage storage;
  const VariableId v(0);
  const TestId c = storage.AddConstraint(TestData{.variables = {v}});
  std::vector<TestDiff> diffs = {TestDiff(storage)};
  storage.Delete(c, collection_to_range(diffs));

  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
  EXPECT_EQ(diffs[0].checkpoint.value(), c.value() + 1);
  TestUpdateProto expected;
  expected.add_deleted_constraint_ids(0);
  EXPECT_THAT(storage.Update(diffs[0]), EquivToProto(expected));
}

}  // namespace
}  // namespace operations_research::math_opt
