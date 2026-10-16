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

#include "ortools/math_opt/storage/sparse_matrix.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {
namespace {

using ::testing::EquivToProto;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ::testing::no_adl::FieldsAre;

TEST(SparseSymmetricMatrixTest, EmptyMatrix) {
  const SparseSymmetricMatrix matrix;
  EXPECT_EQ(matrix.get(VariableId(2), VariableId(4)), 0.0);
  EXPECT_THAT(matrix.RelatedVariables(VariableId(2)), IsEmpty());
  EXPECT_THAT(matrix.Variables(), IsEmpty());
  EXPECT_THAT(matrix.Terms(VariableId(2)), IsEmpty());
  EXPECT_EQ(matrix.nonzeros(), 0);
  EXPECT_THAT(matrix.Proto(), EquivToProto(SparseDoubleMatrixProto()));
}

TEST(SparseSymmetricMatrixTest, SimpleMatrix) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  const VariableId c(7);
  EXPECT_TRUE(matrix.set(a, b, 3.5));
  EXPECT_TRUE(matrix.set(c, b, 4.5));
  EXPECT_TRUE(matrix.set(a, a, 5.5));

  EXPECT_EQ(matrix.get(a, b), 3.5);
  EXPECT_EQ(matrix.get(b, a), 3.5);
  EXPECT_EQ(matrix.get(c, b), 4.5);
  EXPECT_EQ(matrix.get(b, c), 4.5);
  EXPECT_EQ(matrix.get(a, a), 5.5);
  EXPECT_EQ(matrix.get(b, b), 0.0);
  EXPECT_EQ(matrix.get(a, c), 0.0);
  EXPECT_EQ(matrix.get(c, a), 0.0);

  const VariableId d(1);
  EXPECT_EQ(matrix.get(a, d), 0.0);
  EXPECT_EQ(matrix.get(d, d), 0.0);

  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(a, b));
  EXPECT_THAT(matrix.RelatedVariables(b), UnorderedElementsAre(a, c));
  EXPECT_THAT(matrix.RelatedVariables(c), UnorderedElementsAre(b));
  EXPECT_THAT(matrix.RelatedVariables(d), IsEmpty());
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b, c));

  EXPECT_THAT(matrix.Terms(a),
              UnorderedElementsAre(Pair(a, 5.5), Pair(b, 3.5)));
  EXPECT_THAT(matrix.Terms(b),
              UnorderedElementsAre(Pair(a, 3.5), Pair(c, 4.5)));
  EXPECT_THAT(matrix.Terms(c), UnorderedElementsAre(Pair(b, 4.5)));
  EXPECT_THAT(matrix.Terms(d), IsEmpty());

  EXPECT_EQ(matrix.nonzeros(), 3);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(2);
  expected.add_coefficients(5.5);

  expected.add_row_ids(2);
  expected.add_column_ids(4);
  expected.add_coefficients(3.5);

  expected.add_row_ids(4);
  expected.add_column_ids(7);
  expected.add_coefficients(4.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, SetTwiceDistinct) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  EXPECT_TRUE(matrix.set(a, b, 3.5));
  EXPECT_TRUE(matrix.set(a, b, 4.5));
  EXPECT_EQ(matrix.get(a, b), 4.5);
  EXPECT_EQ(matrix.get(b, a), 4.5);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(b));
  EXPECT_THAT(matrix.RelatedVariables(b), UnorderedElementsAre(a));
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b));
  EXPECT_EQ(matrix.nonzeros(), 1);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(4);
  expected.add_coefficients(4.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, SetTwiceRepeat) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  EXPECT_TRUE(matrix.set(a, b, 3.5));
  EXPECT_FALSE(matrix.set(a, b, 3.5));
  EXPECT_EQ(matrix.get(a, b), 3.5);
  EXPECT_EQ(matrix.get(b, a), 3.5);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(b));
  EXPECT_THAT(matrix.RelatedVariables(b), UnorderedElementsAre(a));
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b));
  EXPECT_EQ(matrix.nonzeros(), 1);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(4);
  expected.add_coefficients(3.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, SetTwiceDelete) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  EXPECT_TRUE(matrix.set(a, b, 3.5));
  EXPECT_TRUE(matrix.set(a, b, 0.0));
  EXPECT_EQ(matrix.get(a, b), 0.0);
  EXPECT_EQ(matrix.get(b, a), 0.0);
  EXPECT_THAT(matrix.RelatedVariables(a), IsEmpty());
  EXPECT_THAT(matrix.RelatedVariables(b), IsEmpty());
  EXPECT_THAT(matrix.Variables(), IsEmpty());
  EXPECT_EQ(matrix.nonzeros(), 0);
  // The deletion leaves the map completely empty so you can an immediate
  // compaction.
  SparseDoubleMatrixProto expected;
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, DeleteWithAndWithoutCompact) {
  SparseSymmetricMatrix matrix;
  const VariableId a(1);
  const VariableId b(3);
  const VariableId c(5);
  const VariableId d(7);
  const VariableId e(9);

  EXPECT_TRUE(matrix.set(a, b, 2.0));
  EXPECT_TRUE(matrix.set(a, c, 4.0));
  EXPECT_TRUE(matrix.set(a, d, 6.0));
  EXPECT_TRUE(matrix.set(a, e, 8.0));

  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(b, c, d, e));
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b, c, d, e));
  EXPECT_EQ(matrix.nonzeros(), 4);

  // First delete does not compact, only 25% of space is wasted.
  EXPECT_TRUE(matrix.set(a, c, 0.0));
  EXPECT_EQ(matrix.get(a, c), 0.0);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(b, d, e));
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b, d, e));
  EXPECT_EQ(matrix.nonzeros(), 3);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);
  {
    SparseDoubleMatrixProto expected;
    expected.add_row_ids(1);
    expected.add_column_ids(3);
    expected.add_coefficients(2.0);
    expected.add_row_ids(1);
    expected.add_column_ids(7);
    expected.add_coefficients(6.0);
    expected.add_row_ids(1);
    expected.add_column_ids(9);
    expected.add_coefficients(8.0);
    EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
  }

  // Second delete compacts, 50% of space is wasted.
  EXPECT_TRUE(matrix.set(a, e, 0.0));
  EXPECT_EQ(matrix.get(a, e), 0.0);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(b, d));
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b, d));
  EXPECT_EQ(matrix.nonzeros(), 2);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 2);
  {
    SparseDoubleMatrixProto expected;
    expected.add_row_ids(1);
    expected.add_column_ids(3);
    expected.add_coefficients(2.0);
    expected.add_row_ids(1);
    expected.add_column_ids(7);
    expected.add_coefficients(6.0);
    EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
  }
}

TEST(SparseSymmetricMatrixTest, DeleteThenAddSameVarBeforeCompact) {
  SparseSymmetricMatrix matrix;
  const VariableId a(1);
  const VariableId b(3);
  const VariableId c(5);
  const VariableId d(7);
  const VariableId e(9);

  EXPECT_TRUE(matrix.set(a, b, 2.0));
  EXPECT_TRUE(matrix.set(a, c, 4.0));
  EXPECT_TRUE(matrix.set(a, d, 6.0));
  EXPECT_TRUE(matrix.set(a, e, 8.0));

  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(b, c, d, e));
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b, c, d, e));
  EXPECT_EQ(matrix.nonzeros(), 4);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);

  // First delete does not compact, only 25% of space is wasted.
  EXPECT_TRUE(matrix.set(a, c, 0.0));
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(b, d, e));
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b, d, e));
  EXPECT_EQ(matrix.nonzeros(), 3);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);

  EXPECT_TRUE(matrix.set(a, c, 12.0));
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(b, c, d, e));
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, b, c, d, e));
  EXPECT_EQ(matrix.nonzeros(), 4);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);

  SparseDoubleMatrixProto expected;
  expected.add_row_ids(1);
  expected.add_column_ids(3);
  expected.add_coefficients(2.0);
  expected.add_row_ids(1);
  expected.add_column_ids(5);
  expected.add_coefficients(12.0);
  expected.add_row_ids(1);
  expected.add_column_ids(7);
  expected.add_coefficients(6.0);
  expected.add_row_ids(1);
  expected.add_column_ids(9);
  expected.add_coefficients(8.0);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, DeleteNotPresent) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  EXPECT_TRUE(matrix.set(a, a, 3.5));
  EXPECT_FALSE(matrix.set(a, b, 0.0));
  EXPECT_EQ(matrix.get(a, a), 3.5);
  EXPECT_EQ(matrix.get(a, b), 0.0);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(a));
  EXPECT_THAT(matrix.RelatedVariables(b), IsEmpty());
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a));
  EXPECT_EQ(matrix.nonzeros(), 1);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 1);

  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(2);
  expected.add_coefficients(3.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, DeleteVariableNotPresent) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  EXPECT_TRUE(matrix.set(a, a, 3.5));
  matrix.Delete(b);
  EXPECT_EQ(matrix.get(a, a), 3.5);
  EXPECT_EQ(matrix.get(a, b), 0.0);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(a));
  EXPECT_THAT(matrix.RelatedVariables(b), IsEmpty());
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a));
  EXPECT_EQ(matrix.nonzeros(), 1);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 1);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(2);
  expected.add_coefficients(3.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, DeleteVariablePresentCompact) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  EXPECT_TRUE(matrix.set(a, a, 3.5));
  EXPECT_TRUE(matrix.set(a, b, 4.5));
  matrix.Delete(b);
  EXPECT_EQ(matrix.get(a, a), 3.5);
  EXPECT_EQ(matrix.get(a, b), 0.0);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(a));
  EXPECT_THAT(matrix.RelatedVariables(b), IsEmpty());
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a));
  EXPECT_EQ(matrix.nonzeros(), 1);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 1);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(2);
  expected.add_coefficients(3.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, DeleteVariablePresentNoCompact) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  const VariableId c(6);
  const VariableId d(8);
  const VariableId e(10);
  EXPECT_TRUE(matrix.set(a, a, 3.5));
  EXPECT_TRUE(matrix.set(a, b, 4.5));
  EXPECT_TRUE(matrix.set(a, c, 5.5));
  EXPECT_TRUE(matrix.set(a, d, 6.5));
  EXPECT_TRUE(matrix.set(a, e, 7.5));
  matrix.Delete(b);
  EXPECT_EQ(matrix.get(a, a), 3.5);
  EXPECT_EQ(matrix.get(a, b), 0.0);
  EXPECT_EQ(matrix.get(a, c), 5.5);
  EXPECT_EQ(matrix.get(a, d), 6.5);
  EXPECT_EQ(matrix.get(a, e), 7.5);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(a, c, d, e));
  EXPECT_THAT(matrix.RelatedVariables(b), IsEmpty());
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, c, d, e));
  EXPECT_EQ(matrix.nonzeros(), 4);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 5);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(2);
  expected.add_coefficients(3.5);

  expected.add_row_ids(2);
  expected.add_column_ids(6);
  expected.add_coefficients(5.5);

  expected.add_row_ids(2);
  expected.add_column_ids(8);
  expected.add_coefficients(6.5);

  expected.add_row_ids(2);
  expected.add_column_ids(10);
  expected.add_coefficients(7.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, DeleteVariableTermAlreadyDeleted) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  const VariableId c(6);
  const VariableId d(8);
  const VariableId e(10);
  EXPECT_TRUE(matrix.set(a, a, 3.5));
  EXPECT_TRUE(matrix.set(a, b, 4.5));
  EXPECT_TRUE(matrix.set(a, c, 5.5));
  EXPECT_TRUE(matrix.set(a, d, 6.5));
  EXPECT_TRUE(matrix.set(a, e, 7.5));
  matrix.set(a, b, 0.5);
  matrix.Delete(b);
  EXPECT_EQ(matrix.get(a, a), 3.5);
  EXPECT_EQ(matrix.get(a, b), 0.0);
  EXPECT_EQ(matrix.get(a, c), 5.5);
  EXPECT_EQ(matrix.get(a, d), 6.5);
  EXPECT_EQ(matrix.get(a, e), 7.5);
  EXPECT_THAT(matrix.RelatedVariables(a), UnorderedElementsAre(a, c, d, e));
  EXPECT_THAT(matrix.RelatedVariables(b), IsEmpty());
  EXPECT_THAT(matrix.Variables(), UnorderedElementsAre(a, c, d, e));
  EXPECT_EQ(matrix.nonzeros(), 4);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 5);
}

TEST(SparseSymmetricMatrixTest, Clear) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  EXPECT_TRUE(matrix.set(a, b, 3.5));
  matrix.Clear();
  EXPECT_EQ(matrix.get(a, b), 0.0);
  EXPECT_THAT(matrix.RelatedVariables(a), IsEmpty());
  EXPECT_THAT(matrix.RelatedVariables(b), IsEmpty());
  EXPECT_THAT(matrix.Variables(), IsEmpty());
  EXPECT_EQ(matrix.nonzeros(), 0);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 0);
  SparseDoubleMatrixProto expected;
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseSymmetricMatrixTest, AllTerms) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  EXPECT_TRUE(matrix.set(a, b, 3.5));
  EXPECT_TRUE(matrix.set(a, a, 2.5));
  auto all_terms = matrix.Terms();
  EXPECT_THAT(all_terms,
              UnorderedElementsAre(FieldsAre(a, a, 2.5), FieldsAre(a, b, 3.5)));
}

TEST(SparseSymmetricMatrixTest, AllTermsSkipsZeros) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  const VariableId c(6);
  EXPECT_TRUE(matrix.set(a, b, 3.5));
  EXPECT_TRUE(matrix.set(a, a, 2.5));
  EXPECT_TRUE(matrix.set(b, b, 4.5));
  EXPECT_TRUE(matrix.set(b, c, 1.5));
  EXPECT_TRUE(matrix.set(a, a, 0.0));
  // No compaction, only 25% zeros.
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);
  EXPECT_EQ(matrix.nonzeros(), 3);
  auto all_terms = matrix.Terms();
  EXPECT_THAT(all_terms,
              UnorderedElementsAre(FieldsAre(a, b, 3.5), FieldsAre(b, b, 4.5),
                                   FieldsAre(b, c, 1.5)));
}

TEST(SparseSymmetricMatrixTest, Update) {
  SparseSymmetricMatrix matrix;
  const VariableId a(2);
  const VariableId b(4);
  const VariableId c(6);
  EXPECT_TRUE(matrix.set(a, a, 2.5));
  EXPECT_TRUE(matrix.set(a, b, 3.5));
  EXPECT_TRUE(matrix.set(b, c, 4.5));
  {
    const absl::flat_hash_set<VariableId> deleted_variables = {};
    const std::vector<VariableId> new_variables = {VariableId(8),
                                                   VariableId(9)};
    const SparseDoubleMatrixProto expected;
    EXPECT_THAT(matrix.Update(deleted_variables, new_variables, {}),
                EquivToProto(expected));
  }

  {
    // Because 1 is not in variables, it is like the variable has been deleted
    // so it is excluded.
    const absl::flat_hash_set<VariableId> deleted_variables = {VariableId(1)};
    const std::vector<VariableId> new_variables = {VariableId(8),
                                                   VariableId(9)};
    const SparseDoubleMatrixProto expected;
    EXPECT_THAT(
        matrix.Update(deleted_variables, new_variables, {{VariableId(1), a}}),
        EquivToProto(expected));
  }

  {
    SparseDoubleMatrixProto expected;
    expected.add_row_ids(2);
    expected.add_column_ids(2);
    expected.add_coefficients(2.5);
    expected.add_row_ids(2);
    expected.add_column_ids(4);
    expected.add_coefficients(3.5);
    const absl::flat_hash_set<VariableId> deleted_variables = {};
    const std::vector<VariableId> new_variables = {VariableId(8),
                                                   VariableId(9)};
    EXPECT_THAT(
        matrix.Update(deleted_variables, new_variables, {{a, a}, {a, b}}),
        EquivToProto(expected));
  }

  {
    SparseDoubleMatrixProto expected;
    expected.add_row_ids(2);
    expected.add_column_ids(2);
    expected.add_coefficients(2.5);
    expected.add_row_ids(4);
    expected.add_column_ids(6);
    expected.add_coefficients(4.5);
    const absl::flat_hash_set<VariableId> deleted_variables = {};
    const std::vector<VariableId> new_variables = {VariableId(6)};
    EXPECT_THAT(matrix.Update(deleted_variables, new_variables, {{a, a}}),
                EquivToProto(expected));
  }

  {
    SparseDoubleMatrixProto expected;
    expected.add_row_ids(2);
    expected.add_column_ids(4);
    expected.add_coefficients(3.5);
    expected.add_row_ids(4);
    expected.add_column_ids(6);
    expected.add_coefficients(4.5);
    const absl::flat_hash_set<VariableId> deleted_variables = {};
    const std::vector<VariableId> new_variables = {
        VariableId(3), VariableId(4), VariableId(5), VariableId(6)};
    EXPECT_THAT(matrix.Update(deleted_variables, new_variables, {}),
                EquivToProto(expected));
  }
}

////////////////////////////////////////////////////////////////////////////////
// SparseMatrix
////////////////////////////////////////////////////////////////////////////////

using TestMatrix = SparseMatrix<LinearConstraintId, VariableId>;

TEST(SparseMatrixTest, EmptyMatrix) {
  const TestMatrix matrix;
  EXPECT_EQ(matrix.get(LinearConstraintId(2), VariableId(4)), 0.0);
  EXPECT_THAT(matrix.row(LinearConstraintId(2)), IsEmpty());
  EXPECT_THAT(matrix.column(VariableId(4)), IsEmpty());
  EXPECT_EQ(matrix.nonzeros(), 0);
  EXPECT_THAT(matrix.Proto(), EquivToProto(SparseDoubleMatrixProto()));
}

TEST(SparseMatrixTest, SimpleMatrix) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const LinearConstraintId c(5);

  const VariableId x(6);
  const VariableId y(8);
  const VariableId z(1);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));

  EXPECT_EQ(matrix.get(a, x), 3.5);
  EXPECT_TRUE(matrix.contains(a, x));
  EXPECT_EQ(matrix.get(a, y), 4.5);
  EXPECT_TRUE(matrix.contains(a, y));
  EXPECT_EQ(matrix.get(b, y), 5.5);
  EXPECT_TRUE(matrix.contains(b, y));
  EXPECT_EQ(matrix.get(b, x), 0.0);
  EXPECT_FALSE(matrix.contains(b, x));

  EXPECT_EQ(matrix.get(a, z), 0.0);
  EXPECT_FALSE(matrix.contains(a, z));
  EXPECT_EQ(matrix.get(b, z), 0.0);
  EXPECT_FALSE(matrix.contains(b, z));
  EXPECT_EQ(matrix.get(c, x), 0.0);
  EXPECT_FALSE(matrix.contains(c, x));
  EXPECT_EQ(matrix.get(c, y), 0.0);
  EXPECT_FALSE(matrix.contains(c, y));
  EXPECT_EQ(matrix.get(c, z), 0.0);
  EXPECT_FALSE(matrix.contains(c, z));

  EXPECT_THAT(matrix.row(a), UnorderedElementsAre(x, y));
  EXPECT_THAT(matrix.row(b), UnorderedElementsAre(y));
  EXPECT_THAT(matrix.row(c), IsEmpty());

  EXPECT_THAT(matrix.RowTerms(a),
              UnorderedElementsAre(Pair(x, 3.5), Pair(y, 4.5)));
  EXPECT_THAT(matrix.RowTerms(b), UnorderedElementsAre(Pair(y, 5.5)));
  EXPECT_THAT(matrix.RowTerms(c), IsEmpty());

  EXPECT_THAT(matrix.column(x), UnorderedElementsAre(a));
  EXPECT_THAT(matrix.column(y), UnorderedElementsAre(a, b));
  EXPECT_THAT(matrix.column(z), IsEmpty());

  EXPECT_THAT(matrix.ColumnTerms(x), UnorderedElementsAre(Pair(a, 3.5)));
  EXPECT_THAT(matrix.ColumnTerms(y),
              UnorderedElementsAre(Pair(a, 4.5), Pair(b, 5.5)));
  EXPECT_THAT(matrix.ColumnTerms(z), IsEmpty());
  auto terms = matrix.Terms();
  EXPECT_THAT(terms,
              UnorderedElementsAre(FieldsAre(a, x, 3.5), FieldsAre(a, y, 4.5),
                                   FieldsAre(b, y, 5.5)));

  EXPECT_EQ(matrix.nonzeros(), 3);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(6);
  expected.add_coefficients(3.5);

  expected.add_row_ids(2);
  expected.add_column_ids(8);
  expected.add_coefficients(4.5);

  expected.add_row_ids(4);
  expected.add_column_ids(8);
  expected.add_coefficients(5.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseMatrixTest, SetTwiceDistinct) {
  TestMatrix matrix;
  const LinearConstraintId c(4);
  const VariableId x(2);

  EXPECT_TRUE(matrix.set(c, x, 3.5));
  EXPECT_TRUE(matrix.set(c, x, 4.5));
  EXPECT_EQ(matrix.get(c, x), 4.5);
  EXPECT_EQ(matrix.get(c, x), 4.5);
  EXPECT_THAT(matrix.RowTerms(c), UnorderedElementsAre(Pair(x, 4.5)));
  EXPECT_THAT(matrix.ColumnTerms(x), UnorderedElementsAre(Pair(c, 4.5)));
  EXPECT_EQ(matrix.nonzeros(), 1);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(4);
  expected.add_column_ids(2);
  expected.add_coefficients(4.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseMatrixTest, SetTwiceRepeat) {
  TestMatrix matrix;
  const LinearConstraintId c(4);
  const VariableId x(2);
  EXPECT_TRUE(matrix.set(c, x, 3.5));
  EXPECT_FALSE(matrix.set(c, x, 3.5));
  EXPECT_EQ(matrix.get(c, x), 3.5);
  EXPECT_EQ(matrix.get(c, x), 3.5);
  EXPECT_THAT(matrix.RowTerms(c), UnorderedElementsAre(Pair(x, 3.5)));
  EXPECT_THAT(matrix.ColumnTerms(x), UnorderedElementsAre(Pair(c, 3.5)));
  EXPECT_EQ(matrix.nonzeros(), 1);
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(4);
  expected.add_column_ids(2);
  expected.add_coefficients(3.5);
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseMatrixTest, SetTwiceDelete) {
  TestMatrix matrix;
  const LinearConstraintId c(4);
  const VariableId x(2);
  EXPECT_TRUE(matrix.set(c, x, 3.5));
  EXPECT_TRUE(matrix.contains(c, x));
  EXPECT_TRUE(matrix.set(c, x, 0.0));
  EXPECT_EQ(matrix.get(c, x), 0.0);
  EXPECT_EQ(matrix.get(c, x), 0.0);
  EXPECT_FALSE(matrix.contains(c, x));
  EXPECT_THAT(matrix.RowTerms(c), IsEmpty());
  EXPECT_THAT(matrix.ColumnTerms(x), IsEmpty());
  EXPECT_EQ(matrix.nonzeros(), 0);
  SparseDoubleMatrixProto expected;
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseMatrixTest, SetZeroWhenZero) {
  TestMatrix matrix;
  const LinearConstraintId c(4);
  const VariableId x(2);
  EXPECT_FALSE(matrix.set(c, x, 0.0));
  EXPECT_FALSE(matrix.contains(c, x));
  EXPECT_EQ(matrix.get(c, x), 0.0);
  EXPECT_THAT(matrix.RowTerms(c), IsEmpty());
  EXPECT_THAT(matrix.ColumnTerms(x), IsEmpty());
  EXPECT_EQ(matrix.nonzeros(), 0);
  SparseDoubleMatrixProto expected;
  EXPECT_THAT(matrix.Proto(), EquivToProto(expected));
}

TEST(SparseMatrixTest, DeleteRow) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const VariableId x(6);
  const VariableId y(8);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));
  matrix.DeleteRow(a);
  EXPECT_FALSE(matrix.contains(a, x));
  EXPECT_FALSE(matrix.contains(a, y));
  EXPECT_FALSE(matrix.contains(b, x));
  EXPECT_EQ(matrix.get(b, y), 5.5);
  EXPECT_THAT(matrix.row(a), IsEmpty());
  EXPECT_THAT(matrix.RowTerms(b), UnorderedElementsAre(Pair(y, 5.5)));
  EXPECT_THAT(matrix.column(x), IsEmpty());
  EXPECT_THAT(matrix.ColumnTerms(y), UnorderedElementsAre(Pair(b, 5.5)));
}

TEST(SparseMatrixTest, DeleteColumn) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const VariableId x(6);
  const VariableId y(8);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));
  matrix.DeleteColumn(x);
  EXPECT_FALSE(matrix.contains(a, x));
  EXPECT_EQ(matrix.get(a, y), 4.5);
  EXPECT_FALSE(matrix.contains(b, x));
  EXPECT_EQ(matrix.get(b, y), 5.5);
  EXPECT_THAT(matrix.RowTerms(a), UnorderedElementsAre(Pair(y, 4.5)));
  EXPECT_THAT(matrix.RowTerms(b), UnorderedElementsAre(Pair(y, 5.5)));
  EXPECT_THAT(matrix.column(x), IsEmpty());
  EXPECT_THAT(matrix.ColumnTerms(y),
              UnorderedElementsAre(Pair(a, 4.5), Pair(b, 5.5)));
}

TEST(SparseMatrixTest, Clear) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const VariableId x(6);
  const VariableId y(8);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));
  matrix.Clear();
  EXPECT_FALSE(matrix.contains(a, x));
  EXPECT_FALSE(matrix.contains(a, y));
  EXPECT_FALSE(matrix.contains(b, x));
  EXPECT_FALSE(matrix.contains(b, y));
  EXPECT_THAT(matrix.row(a), IsEmpty());
  EXPECT_THAT(matrix.RowTerms(b), IsEmpty());
  EXPECT_THAT(matrix.column(x), IsEmpty());
  EXPECT_THAT(matrix.ColumnTerms(y), IsEmpty());
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 0);
}

TEST(SparseMatrixTest, UpdateAllNew) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const VariableId x(6);
  const VariableId y(8);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));

  const SparseDoubleMatrixProto actual =
      matrix.Update({}, {a, b}, {}, {x, y}, {});
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_row_ids(2);
  expected.add_row_ids(4);
  expected.add_column_ids(6);
  expected.add_column_ids(8);
  expected.add_column_ids(8);
  expected.add_coefficients(3.5);
  expected.add_coefficients(4.5);
  expected.add_coefficients(5.5);

  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(SparseMatrixTest, SomeNew) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const LinearConstraintId c(5);
  const VariableId x(6);
  const VariableId y(8);
  const VariableId z(10);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));
  EXPECT_TRUE(matrix.set(a, z, 6.5));
  EXPECT_TRUE(matrix.set(c, x, 7.5));

  const SparseDoubleMatrixProto actual =
      matrix.Update({}, {b, c}, {}, {y, z}, {});
  SparseDoubleMatrixProto expected;
  expected.add_row_ids(2);
  expected.add_column_ids(10);
  expected.add_coefficients(6.5);
  expected.add_row_ids(4);
  expected.add_column_ids(8);
  expected.add_coefficients(5.5);
  expected.add_row_ids(5);
  expected.add_column_ids(6);
  expected.add_coefficients(7.5);
  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(SparseMatrixTest, Empty) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const VariableId x(6);
  const VariableId y(8);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));

  const SparseDoubleMatrixProto actual = matrix.Update({}, {}, {}, {}, {});
  SparseDoubleMatrixProto expected;
  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(SparseMatrixTest, Dirty) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const VariableId x(6);
  const VariableId y(8);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));

  const SparseDoubleMatrixProto actual =
      matrix.Update({}, {}, {}, {}, {{a, x}, {b, y}});
  SparseDoubleMatrixProto expected;

  expected.add_row_ids(2);
  expected.add_row_ids(4);
  expected.add_column_ids(6);
  expected.add_column_ids(8);
  expected.add_coefficients(3.5);
  expected.add_coefficients(5.5);

  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(SparseMatrixTest, DirtyAndDeleted) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const LinearConstraintId c(5);
  const VariableId x(6);
  const VariableId y(8);
  const VariableId z(10);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));

  const SparseDoubleMatrixProto actual =
      matrix.Update({c}, {}, {z}, {}, {{a, x}, {b, y}, {c, x}, {a, z}});
  SparseDoubleMatrixProto expected;

  expected.add_row_ids(2);
  expected.add_row_ids(4);
  expected.add_column_ids(6);
  expected.add_column_ids(8);
  expected.add_coefficients(3.5);
  expected.add_coefficients(5.5);

  EXPECT_THAT(actual, EquivToProto(expected));
}

TEST(SparseMatrixTest, CompactsOnEntryDeletion) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const VariableId x(6);
  const VariableId y(8);
  const VariableId z(10);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(a, y, 4.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));
  EXPECT_TRUE(matrix.set(b, z, 6.5));
  EXPECT_EQ(matrix.nonzeros(), 4);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);
  matrix.set(a, x, 0.0);
  EXPECT_EQ(matrix.nonzeros(), 3);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);
  matrix.set(a, y, 0.0);
  EXPECT_EQ(matrix.nonzeros(), 2);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 2);
}

TEST(SparseMatrixTest, CompactsOnRowDeletion) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const LinearConstraintId c(5);
  const VariableId x(6);
  const VariableId y(8);
  const VariableId z(10);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));
  EXPECT_TRUE(matrix.set(b, z, 6.5));
  EXPECT_TRUE(matrix.set(c, y, 4.5));
  EXPECT_EQ(matrix.nonzeros(), 4);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);
  matrix.DeleteRow(a);
  EXPECT_EQ(matrix.nonzeros(), 3);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);
  matrix.DeleteRow(c);
  EXPECT_EQ(matrix.nonzeros(), 2);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 2);
}

TEST(SparseMatrixTest, CompactsOnColumnDeletion) {
  TestMatrix matrix;
  const LinearConstraintId a(2);
  const LinearConstraintId b(4);
  const LinearConstraintId c(5);
  const VariableId x(6);
  const VariableId y(8);
  const VariableId z(10);
  EXPECT_TRUE(matrix.set(a, x, 3.5));
  EXPECT_TRUE(matrix.set(b, y, 5.5));
  EXPECT_TRUE(matrix.set(b, z, 6.5));
  EXPECT_TRUE(matrix.set(c, y, 4.5));
  EXPECT_EQ(matrix.nonzeros(), 4);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);
  matrix.DeleteColumn(x);
  EXPECT_EQ(matrix.nonzeros(), 3);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 4);
  matrix.DeleteColumn(z);
  EXPECT_EQ(matrix.nonzeros(), 2);
  EXPECT_EQ(matrix.impl_detail_matrix_storage_size(), 2);
}

}  // namespace
}  // namespace operations_research::math_opt
