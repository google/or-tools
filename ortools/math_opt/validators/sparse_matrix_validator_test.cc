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

#include "ortools/math_opt/validators/sparse_matrix_validator.h"

#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/text_format.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/model_summary.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::HasSubstr;
using ::testing::status::StatusIs;

IdNameBiMap MakeBiMap(const std::initializer_list<int64_t> ids) {
  IdNameBiMap bimap;
  for (const int64_t id : ids) {
    CHECK_OK(bimap.Insert(id, ""));
  }
  return bimap;
}

SparseDoubleMatrixProto ParseMatrix(const absl::string_view model) {
  SparseDoubleMatrixProto result;
  CHECK(google::protobuf::TextFormat::ParseFromString(model, &result));
  return result;
}

SparseDoubleMatrixProto SimpleSparseMatrix() {
  return ParseMatrix(R"pb(
    row_ids: [ 3, 3, 8 ]
    column_ids: [ 3, 5, 5 ]
    coefficients: [ 4.0, 3.0, 2.0 ]
  )pb");
}

auto SparseMatrixError(const absl::string_view reason) {
  return StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr(reason));
}

TEST(SparseMatrixValidTest, Ok) {
  const SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  EXPECT_OK(SparseMatrixValid(matrix));
}

TEST(SparseMatrixValidTest, RowMismatch) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.add_row_ids(7);
  EXPECT_THAT(SparseMatrixValid(matrix), SparseMatrixError("equal to"));
}

TEST(SparseMatrixValidTest, ColumnMismatch) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.add_column_ids(7);
  EXPECT_THAT(SparseMatrixValid(matrix), SparseMatrixError("equal to"));
}

TEST(SparseMatrixValidTest, CoefficientsMismatch) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.add_coefficients(7.0);
  EXPECT_THAT(SparseMatrixValid(matrix), SparseMatrixError("equal to"));
}

TEST(SparseMatrixValidTest, NegativeRowId) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.set_row_ids(0, -1);
  EXPECT_THAT(SparseMatrixValid(matrix), SparseMatrixError("nonnegative"));
}

TEST(SparseMatrixValidTest, NegativeColumnId) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.set_column_ids(2, -3);
  EXPECT_THAT(SparseMatrixValid(matrix), SparseMatrixError("nonnegative"));
}

TEST(SparseMatrixValidTest, UpperTriangularOk) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.set_row_ids(2, 5);
  EXPECT_OK(SparseMatrixValid(matrix, /*enforce_upper_triangular=*/true));
}

TEST(SparseMatrixValidTest, LowerTriangular) {
  const SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  EXPECT_THAT(SparseMatrixValid(matrix, /*enforce_upper_triangular=*/true),
              SparseMatrixError("lower triangular entry"));
}

TEST(SparseMatrixValidTest, DecreasingRowIds) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.set_row_ids(1, 9);
  EXPECT_THAT(SparseMatrixValid(matrix), SparseMatrixError("nondecreasing"));
}

TEST(SparseMatrixValidTest, DecreasingColumnIdsInRow) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.set_column_ids(1, 2);
  EXPECT_THAT(SparseMatrixValid(matrix),
              SparseMatrixError("strictly increasing"));
}

TEST(SparseMatrixValidTest, SameColumnIdsInRow) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.set_column_ids(1, 3);
  EXPECT_THAT(SparseMatrixValid(matrix),
              SparseMatrixError("strictly increasing"));
}

TEST(SparseMatrixValidTest, NanCoefficient) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.set_coefficients(2, std::nan(""));
  EXPECT_THAT(SparseMatrixValid(matrix), SparseMatrixError("without NaN"));
}

TEST(SparseMatrixValidTest, InfCoefficient) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  matrix.set_coefficients(1, std::numeric_limits<double>::infinity());
  EXPECT_THAT(SparseMatrixValid(matrix),
              SparseMatrixError("finite coefficients"));
}

TEST(SparseMatrixIdsAreKnownTest, Ok) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  EXPECT_OK(
      SparseMatrixIdsAreKnown(matrix, MakeBiMap({3, 8}), MakeBiMap({3, 5, 8})));
}

TEST(SparseMatrixIdsAreKnownTest, RowUniverseMismatch) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  EXPECT_THAT(
      SparseMatrixIdsAreKnown(matrix, MakeBiMap({3}), MakeBiMap({3, 5, 8})),
      SparseMatrixError("Unknown row_id"));
}

TEST(SparseMatrixIdsAreKnownTest, ColumnUniverseMismatch) {
  SparseDoubleMatrixProto matrix = SimpleSparseMatrix();
  EXPECT_THAT(
      SparseMatrixIdsAreKnown(matrix, MakeBiMap({3, 8}), MakeBiMap({3, 8})),
      SparseMatrixError("Unknown column_id"));
}

}  // namespace
}  // namespace operations_research::math_opt
