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

#include "ortools/math_opt/storage/linear_constraint_storage.h"

#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/storage/model_storage_types.h"
#include "ortools/math_opt/storage/range.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::EquivToProto;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using Diff = LinearConstraintStorage::Diff;
using UpdateResult = LinearConstraintStorage::UpdateResult;

TEST(LinearConstraintStorageTest, Empty) {
  const LinearConstraintStorage storage;
  EXPECT_EQ(storage.size(), 0);
  EXPECT_FALSE(storage.contains(LinearConstraintId(0)));
  EXPECT_FALSE(storage.contains(LinearConstraintId(1)));
  EXPECT_EQ(storage.next_id().value(), 0);
  EXPECT_THAT(storage.LinearConstraints(), IsEmpty());
  EXPECT_THAT(storage.SortedLinearConstraints(), IsEmpty());
  EXPECT_THAT(storage.Proto(), Pair(EquivToProto(LinearConstraintsProto()),
                                    EquivToProto(SparseDoubleMatrixProto())));
}

TEST(LinearConstraintStorageTest, ReadSimpleConstraints) {
  LinearConstraintStorage storage;
  std::vector<Diff> diffs;
  const LinearConstraintId c = storage.Add(-1.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(2.0, 2.0, "d");
  const VariableId x(5);
  const VariableId y(6);
  storage.set_term(d, x, 3.0, collection_to_range(diffs));
  storage.set_term(d, y, 1.0, collection_to_range(diffs));
  EXPECT_EQ(storage.size(), 2);
  EXPECT_TRUE(storage.contains(c));
  EXPECT_TRUE(storage.contains(d));
  EXPECT_FALSE(storage.contains(LinearConstraintId(6)));

  EXPECT_EQ(storage.lower_bound(c), -1.0);
  EXPECT_EQ(storage.upper_bound(c), 1.0);
  EXPECT_EQ(storage.name(c), "c");

  EXPECT_EQ(storage.lower_bound(d), 2.0);
  EXPECT_EQ(storage.upper_bound(d), 2.0);
  EXPECT_EQ(storage.name(d), "d");

  auto terms = storage.matrix().Terms();
  absl::c_sort(terms);
  ASSERT_EQ(terms.size(), 2);
  EXPECT_EQ(terms[0], std::make_tuple(d, x, 3.0));
  EXPECT_EQ(terms[1], std::make_tuple(d, y, 1.0));

  EXPECT_EQ(storage.next_id().value(), 2);
  LinearConstraintsProto expected_constraints;
  expected_constraints.add_ids(0);
  expected_constraints.add_ids(1);
  expected_constraints.add_lower_bounds(-1.0);
  expected_constraints.add_lower_bounds(2.0);
  expected_constraints.add_upper_bounds(1.0);
  expected_constraints.add_upper_bounds(2.0);
  expected_constraints.add_names("c");
  expected_constraints.add_names("d");
  SparseDoubleMatrixProto expected_matrix;
  expected_matrix.add_row_ids(1);
  expected_matrix.add_row_ids(1);
  expected_matrix.add_column_ids(5);
  expected_matrix.add_column_ids(6);
  expected_matrix.add_coefficients(3.0);
  expected_matrix.add_coefficients(1.0);
  const auto [actual_constraints, actual_matrix] = storage.Proto();
  EXPECT_THAT(actual_constraints, EquivToProto(expected_constraints));
  EXPECT_THAT(actual_matrix, EquivToProto(expected_matrix));
}

TEST(LinearConstraintStorageTest, SetTerm) {
  LinearConstraintStorage storage;
  std::vector<Diff> diffs;
  const LinearConstraintId c = storage.Add(-1.0, 1.0, "c");
  const VariableId x(5);
  EXPECT_FALSE(storage.matrix().contains(c, x));
  storage.set_term(c, x, 3.0, collection_to_range(diffs));
  EXPECT_TRUE(storage.matrix().contains(c, x));
  storage.set_term(c, x, 4.0, collection_to_range(diffs));
  storage.set_term(c, x, 4.0, collection_to_range(diffs));
  storage.set_term(c, x, 0.0, collection_to_range(diffs));
  EXPECT_FALSE(storage.matrix().contains(c, x));
}

TEST(LinearConstraintStorageTest, ReadConstraintIds) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(-1.0, 1.0, "c");
  storage.ensure_next_id_at_least(LinearConstraintId(3));
  const LinearConstraintId d = storage.Add(2.0, 2.0, "d");
  EXPECT_EQ(d.value(), 3);
  const LinearConstraintId e = storage.Add(-3.0, -2.0, "e");
  EXPECT_THAT(storage.LinearConstraints(), UnorderedElementsAre(c, d, e));
  EXPECT_THAT(storage.SortedLinearConstraints(), ElementsAre(c, d, e));
}

TEST(LinearConstraintStorageTest, DeleteConstraintIds) {
  LinearConstraintStorage storage;
  std::vector<Diff> diffs;
  const LinearConstraintId c = storage.Add(-1.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(2.0, 2.0, "d");
  EXPECT_TRUE(storage.contains(d));
  storage.Delete(d, collection_to_range(diffs));
  EXPECT_FALSE(storage.contains(d));
  const LinearConstraintId e = storage.Add(-3.0, -2.0, "e");
  EXPECT_EQ(e.value(), 2);
  EXPECT_THAT(storage.LinearConstraints(), UnorderedElementsAre(c, e));
  EXPECT_THAT(storage.SortedLinearConstraints(), ElementsAre(c, e));
  LinearConstraintsProto expected_constraints;
  expected_constraints.add_ids(0);
  expected_constraints.add_ids(2);
  expected_constraints.add_lower_bounds(-1.0);
  expected_constraints.add_lower_bounds(-3.0);
  expected_constraints.add_upper_bounds(1.0);
  expected_constraints.add_upper_bounds(-2.0);
  expected_constraints.add_names("c");
  expected_constraints.add_names("e");
  SparseDoubleMatrixProto expected_matrix;
  const auto [actual_constraints, actual_matrix] = storage.Proto();
  EXPECT_THAT(actual_constraints, EquivToProto(expected_constraints));
  EXPECT_THAT(actual_matrix, EquivToProto(SparseDoubleMatrixProto()));
}

TEST(LinearConstraintStorageTest, DeleteConstraintWithCoefficients) {
  LinearConstraintStorage storage;
  std::vector<Diff> diffs;
  const LinearConstraintId c = storage.Add(-1.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(2.0, 2.0, "d");
  const VariableId x(5);
  const VariableId y(6);
  storage.set_term(c, x, 3.0, collection_to_range(diffs));
  storage.set_term(c, y, 2.0, collection_to_range(diffs));
  storage.set_term(d, y, 1.0, collection_to_range(diffs));
  storage.Delete(c, collection_to_range(diffs));
  EXPECT_FALSE(storage.matrix().contains(c, x));
  EXPECT_FALSE(storage.matrix().contains(c, y));
  EXPECT_FALSE(storage.matrix().contains(d, x));
  EXPECT_TRUE(storage.matrix().contains(d, y));
  EXPECT_EQ(storage.matrix().get(d, y), 1.0);
  LinearConstraintsProto expected_constraints;
  expected_constraints.add_ids(1);
  expected_constraints.add_lower_bounds(2.0);
  expected_constraints.add_upper_bounds(2.0);
  expected_constraints.add_names("d");
  SparseDoubleMatrixProto expected_matrix;
  expected_matrix.add_row_ids(1);
  expected_matrix.add_column_ids(6);
  expected_matrix.add_coefficients(1.0);
  const auto [actual_constraints, actual_matrix] = storage.Proto();
  EXPECT_THAT(actual_constraints, EquivToProto(expected_constraints));
  EXPECT_THAT(actual_matrix, EquivToProto(expected_matrix));
}

TEST(LinearConstraintStorageTest, DeleteVariableWithCoefficients) {
  LinearConstraintStorage storage;
  std::vector<Diff> diffs;
  const LinearConstraintId c = storage.Add(-1.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(2.0, 2.0, "d");
  const VariableId x(5);
  const VariableId y(6);
  storage.set_term(c, x, 3.0, collection_to_range(diffs));
  storage.set_term(c, y, 2.0, collection_to_range(diffs));
  storage.set_term(d, y, 1.0, collection_to_range(diffs));
  storage.DeleteVariable(y, collection_to_range(diffs));
  EXPECT_TRUE(storage.matrix().contains(c, x));
  EXPECT_FALSE(storage.matrix().contains(c, y));
  EXPECT_FALSE(storage.matrix().contains(d, x));
  EXPECT_FALSE(storage.matrix().contains(d, y));
  EXPECT_EQ(storage.matrix().get(c, x), 3.0);
  LinearConstraintsProto expected_constraints;
  expected_constraints.add_ids(0);
  expected_constraints.add_ids(1);
  expected_constraints.add_lower_bounds(-1.0);
  expected_constraints.add_upper_bounds(1.0);
  expected_constraints.add_lower_bounds(2.0);
  expected_constraints.add_upper_bounds(2.0);
  expected_constraints.add_names("c");
  expected_constraints.add_names("d");
  SparseDoubleMatrixProto expected_matrix;
  expected_matrix.add_row_ids(0);
  expected_matrix.add_column_ids(5);
  expected_matrix.add_coefficients(3.0);
  const auto [actual_constraints, actual_matrix] = storage.Proto();
  EXPECT_THAT(actual_constraints, EquivToProto(expected_constraints));
  EXPECT_THAT(actual_matrix, EquivToProto(expected_matrix));
}

TEST(LinearConstraintDiffTest, EmptyDiff) {
  const LinearConstraintStorage storage;
  const Diff diff(storage, VariableId(0));
  EXPECT_TRUE(storage.diff_is_empty(diff));
  EXPECT_EQ(diff.checkpoint.value(), 0);
  UpdateResult result = storage.Update(diff, {}, {});
  EXPECT_THAT(result.deleted, IsEmpty());
  EXPECT_THAT(result.creates, EquivToProto(LinearConstraintsProto()));
  EXPECT_THAT(result.updates, EquivToProto(LinearConstraintUpdatesProto()));
  EXPECT_THAT(result.matrix_updates, EquivToProto(SparseDoubleMatrixProto()));
}

TEST(LinearConstraintDiffTest, BoundUpdates) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(0.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(-1.0, 2.0, "d");
  const LinearConstraintId e = storage.Add(-4.0, 5.0, "e");

  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  EXPECT_EQ(diffs[0].checkpoint.value(), 3);
  storage.set_lower_bound(c, -10.0, collection_to_range(diffs));
  storage.set_lower_bound(e, -11.0, collection_to_range(diffs));
  storage.set_upper_bound(d, 10.0, collection_to_range(diffs));
  storage.set_upper_bound(e, 11.0, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
  UpdateResult result = storage.Update(diffs[0], {}, {});
  LinearConstraintUpdatesProto expected;
  expected.mutable_lower_bounds()->add_ids(0);
  expected.mutable_lower_bounds()->add_ids(2);
  expected.mutable_lower_bounds()->add_values(-10.0);
  expected.mutable_lower_bounds()->add_values(-11.0);
  expected.mutable_upper_bounds()->add_ids(1);
  expected.mutable_upper_bounds()->add_ids(2);
  expected.mutable_upper_bounds()->add_values(10.0);
  expected.mutable_upper_bounds()->add_values(11.0);
  EXPECT_THAT(result.deleted, IsEmpty());
  EXPECT_THAT(result.creates, EquivToProto(LinearConstraintsProto()));
  EXPECT_THAT(result.updates, EquivToProto(expected));
  EXPECT_THAT(result.matrix_updates, EquivToProto(SparseDoubleMatrixProto()));
}

TEST(LinearConstraintDiffTest, BoundUpdateAfterCheckpoint) {
  LinearConstraintStorage storage;
  storage.Add(0.0, 1.0, "c");
  storage.Add(-1.0, 2.0, "d");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  const LinearConstraintId e = storage.Add(-4.0, 5.0, "e");
  storage.set_lower_bound(e, -10.0, collection_to_range(diffs));
  storage.set_upper_bound(e, 10.0, collection_to_range(diffs));
  UpdateResult result = storage.Update(diffs[0], {}, {});

  LinearConstraintsProto expected_creates;
  expected_creates.add_lower_bounds(-10.0);
  expected_creates.add_upper_bounds(10.0);
  expected_creates.add_ids(2);
  expected_creates.add_names("e");
  EXPECT_THAT(result.deleted, IsEmpty());
  EXPECT_THAT(result.creates, EquivToProto(expected_creates));
  EXPECT_THAT(result.updates, EquivToProto(LinearConstraintUpdatesProto()));
  EXPECT_THAT(result.matrix_updates, EquivToProto(SparseDoubleMatrixProto()));
}

TEST(LinearConstraintDiffTest, Creates) {
  LinearConstraintStorage storage;
  storage.Add(0.0, 1.0, "c");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  storage.Add(-1.0, 2.0, "d");
  storage.ensure_next_id_at_least(LinearConstraintId(4));
  storage.Add(-4.0, 5.0, "e");
  UpdateResult result = storage.Update(diffs[0], {}, {});
  LinearConstraintsProto expected;
  expected.add_ids(1);
  expected.add_ids(4);
  expected.add_lower_bounds(-1.0);
  expected.add_lower_bounds(-4.0);
  expected.add_upper_bounds(2.0);
  expected.add_upper_bounds(5.0);
  expected.add_names("d");
  expected.add_names("e");
  EXPECT_THAT(result.creates, EquivToProto(expected));
  EXPECT_THAT(result.updates, EquivToProto(LinearConstraintUpdatesProto()));
  EXPECT_THAT(result.deleted, IsEmpty());
  EXPECT_THAT(result.matrix_updates, EquivToProto(SparseDoubleMatrixProto()));
}

TEST(LinearConstraintDiffTest, Deletes) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(0.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(-1.0, 2.0, "d");
  storage.Add(-4.0, 5.0, "e");
  const LinearConstraintId f = storage.Add(0.0, 1.0, "f");
  std::vector<Diff> diffs;
  storage.Delete(d, collection_to_range(diffs));
  diffs.push_back(Diff(storage, VariableId(0)));
  const LinearConstraintId g = storage.Add(0.0, 1.0, "g");
  storage.Delete(c, collection_to_range(diffs));
  storage.Delete(f, collection_to_range(diffs));
  storage.Delete(g, collection_to_range(diffs));
  UpdateResult result = storage.Update(diffs[0], {}, {});
  LinearConstraintsProto expected;
  expected.add_ids(1);
  expected.add_ids(4);
  expected.add_lower_bounds(-1.0);
  expected.add_lower_bounds(-4.0);
  expected.add_upper_bounds(2.0);
  expected.add_upper_bounds(5.0);
  expected.add_names("d");
  expected.add_names("e");
  EXPECT_THAT(result.creates, EquivToProto(LinearConstraintsProto()));
  EXPECT_THAT(result.updates, EquivToProto(LinearConstraintUpdatesProto()));
  EXPECT_THAT(result.deleted, ElementsAre(c.value(), f.value()));
  EXPECT_THAT(result.matrix_updates, EquivToProto(SparseDoubleMatrixProto()));
}

TEST(LinearConstraintDiffTest, MatrixWithNewVariables) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(0.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(-1.0, 2.0, "d");
  const VariableId x(10);
  const VariableId y(11);
  std::vector<Diff> diffs;
  storage.set_term(c, y, 1.0, collection_to_range(diffs));
  const VariableId variable_checkpoint(12);
  diffs.push_back(Diff(storage, variable_checkpoint));

  const VariableId z(12);
  const LinearConstraintId e = storage.Add(0.0, 1.0, "e");
  storage.set_term(c, z, 2.0, collection_to_range(diffs));
  storage.set_term(d, x, 4.0, collection_to_range(diffs));
  storage.set_term(e, y, 3.0, collection_to_range(diffs));
  storage.set_term(e, z, 6.0, collection_to_range(diffs));
  UpdateResult result = storage.Update(diffs[0], {}, {z});
  SparseDoubleMatrixProto matrix_expected;
  matrix_expected.add_row_ids(c.value());
  matrix_expected.add_column_ids(z.value());
  matrix_expected.add_coefficients(2.0);
  matrix_expected.add_row_ids(d.value());
  matrix_expected.add_column_ids(x.value());
  matrix_expected.add_coefficients(4.0);
  matrix_expected.add_row_ids(e.value());
  matrix_expected.add_column_ids(y.value());
  matrix_expected.add_coefficients(3.0);
  matrix_expected.add_row_ids(e.value());
  matrix_expected.add_column_ids(z.value());
  matrix_expected.add_coefficients(6.0);
  LinearConstraintsProto expected_creates;
  expected_creates.add_ids(2);
  expected_creates.add_lower_bounds(0.0);
  expected_creates.add_upper_bounds(1.0);
  expected_creates.add_names("e");
  EXPECT_THAT(result.creates, EquivToProto(expected_creates));
  EXPECT_THAT(result.updates, EquivToProto(LinearConstraintUpdatesProto()));
  EXPECT_THAT(result.deleted, IsEmpty());
  EXPECT_THAT(result.matrix_updates, EquivToProto(matrix_expected));
}

TEST(LinearConstraintDiffTest, MatrixWithDeletedVariables) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(0.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(-1.0, 2.0, "d");
  const VariableId x(10);
  const VariableId y(11);
  const VariableId variable_checkpoint(12);
  std::vector<Diff> diffs = {Diff(storage, variable_checkpoint)};
  storage.set_term(c, x, 2.0, collection_to_range(diffs));
  storage.set_term(c, y, 4.0, collection_to_range(diffs));
  storage.set_term(d, x, 3.0, collection_to_range(diffs));
  absl::flat_hash_set<VariableId> deleted_variables = {x};
  UpdateResult result = storage.Update(diffs[0], deleted_variables, {});
  SparseDoubleMatrixProto matrix_expected;
  matrix_expected.add_row_ids(c.value());
  matrix_expected.add_column_ids(y.value());
  matrix_expected.add_coefficients(4.0);
  EXPECT_THAT(result.creates, EquivToProto(LinearConstraintsProto()));
  EXPECT_THAT(result.updates, EquivToProto(LinearConstraintUpdatesProto()));
  EXPECT_THAT(result.deleted, IsEmpty());
  EXPECT_THAT(result.matrix_updates, EquivToProto(matrix_expected));
}

TEST(LinearConstraintDiffTest, AdvanceCheckpoint) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(0.0, 1.0, "c");
  const LinearConstraintId d = storage.Add(0.0, 1.0, "d");
  const VariableId x(10);
  const VariableId variable_checkpoint(11);
  std::vector<Diff> diffs = {Diff(storage, variable_checkpoint)};
  storage.set_lower_bound(c, -10.0, collection_to_range(diffs));
  storage.set_upper_bound(c, 10.0, collection_to_range(diffs));
  storage.set_term(c, x, 1.0, collection_to_range(diffs));
  storage.set_term(d, x, 2.0, collection_to_range(diffs));
  storage.Delete(d, collection_to_range(diffs));
  const LinearConstraintId e = storage.Add(0.0, 1.0, "e");
  EXPECT_EQ(diffs[0].checkpoint.value(), 2);
  UpdateResult result = storage.Update(diffs[0], {}, {});
  SparseDoubleMatrixProto matrix_expected;
  matrix_expected.add_row_ids(c.value());
  matrix_expected.add_column_ids(x.value());
  matrix_expected.add_coefficients(1.0);
  LinearConstraintsProto expected_creates;
  expected_creates.add_ids(e.value());
  expected_creates.add_lower_bounds(0.0);
  expected_creates.add_upper_bounds(1.0);
  expected_creates.add_names("e");
  LinearConstraintUpdatesProto expected_updates;
  expected_updates.mutable_lower_bounds()->add_ids(c.value());
  expected_updates.mutable_lower_bounds()->add_values(-10.0);
  expected_updates.mutable_upper_bounds()->add_ids(c.value());
  expected_updates.mutable_upper_bounds()->add_values(10.0);
  EXPECT_THAT(result.creates, EquivToProto(expected_creates));
  EXPECT_THAT(result.updates, EquivToProto(expected_updates));
  EXPECT_THAT(result.deleted, ElementsAre(d.value()));
  EXPECT_THAT(result.matrix_updates, EquivToProto(matrix_expected));
  storage.AdvanceCheckpointInDiff(variable_checkpoint, diffs[0]);
  EXPECT_EQ(diffs[0].checkpoint.value(), 3);
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  UpdateResult result2 = storage.Update(diffs[0], {}, {});
  EXPECT_THAT(result2.creates, EquivToProto(LinearConstraintsProto()));
  EXPECT_THAT(result2.updates, EquivToProto(LinearConstraintUpdatesProto()));
  EXPECT_THAT(result2.deleted, IsEmpty());
  EXPECT_THAT(result2.matrix_updates, EquivToProto(SparseDoubleMatrixProto()));
}

TEST(LinearConstraintDiffTest, LowerBoundNotEmpty) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(0.0, 1.0, "c");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.set_lower_bound(c, -10.0, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(LinearConstraintDiffTest, UpperBoundNotEmpty) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(0.0, 1.0, "c");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.set_upper_bound(c, 10.0, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(LinearConstraintDiffTest, MatrixNotEmpty) {
  LinearConstraintStorage storage;
  const LinearConstraintId c = storage.Add(0.0, 1.0, "c");
  const VariableId x(0);
  const VariableId var_checkpoint(1);
  std::vector<Diff> diffs = {Diff(storage, var_checkpoint)};
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.set_term(c, x, 1.0, collection_to_range(diffs));
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

TEST(LinearConstraintDiffTest, NewConstraintNotEmpty) {
  LinearConstraintStorage storage;
  storage.Add(0.0, 1.0, "c");
  std::vector<Diff> diffs = {Diff(storage, VariableId(0))};
  EXPECT_TRUE(storage.diff_is_empty(diffs[0]));
  storage.Add(0.0, 1.0, "d");
  EXPECT_FALSE(storage.diff_is_empty(diffs[0]));
}

}  // namespace
}  // namespace operations_research::math_opt
