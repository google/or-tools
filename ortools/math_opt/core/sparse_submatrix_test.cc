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

#include "ortools/math_opt/core/sparse_submatrix.h"

#include <cstdint>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/core/sparse_collection_matchers.h"
#include "ortools/math_opt/core/sparse_vector.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/sparse_containers.pb.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

TEST(SparseSubmatrixByRowsTest, EmptyMatrix) {
  EXPECT_THAT(SparseSubmatrixByRows(/*matrix=*/{},
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/std::nullopt),
              IsEmpty());
  EXPECT_THAT(SparseSubmatrixByRows(/*matrix=*/{},
                                    /*start_row_id=*/4,
                                    /*end_row_id=*/15,
                                    /*start_col_id=*/3,
                                    /*end_col_id=*/58),
              IsEmpty());
}

TEST(SparseSubmatrixByRowsTest, NonEmptyMatrix) {
  // The matrix data are:
  //
  //  |0 1 2 3 4 5 6
  // -+-------------
  // 0|2 - - - 3 4 -
  // 1|- - - - - - -
  // 2|- 5 - 1 - - 3
  // 3|9 - - 8 - - 7
  const SparseDoubleMatrixProto matrix = MakeSparseDoubleMatrix({
      {0, 0, 2.0},  //
      {0, 4, 3.0},  //
      {0, 5, 4.0},  //
      {2, 1, 5.0},  //
      {2, 3, 1.0},  //
      {2, 6, 3.0},  //
      {3, 0, 9.0},  //
      {3, 3, 8.0},  //
      {3, 6, 7.0}   //
  });

  // The whole matrix.
  EXPECT_THAT(
      SparseSubmatrixByRows(matrix,
                            /*start_row_id=*/0,
                            /*end_row_id=*/std::nullopt,
                            /*start_col_id=*/0,
                            /*end_col_id=*/std::nullopt),
      ElementsAre(
          Pair(0, ElementsAre(Pair(0, 2.0), Pair(4, 3.0), Pair(5, 4.0))),
          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0), Pair(6, 3.0))),
          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0), Pair(6, 7.0)))));

  // Some selected rows.
  EXPECT_THAT(
      SparseSubmatrixByRows(matrix,
                            /*start_row_id=*/1,
                            /*end_row_id=*/std::nullopt,
                            /*start_col_id=*/0,
                            /*end_col_id=*/std::nullopt),
      ElementsAre(
          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0), Pair(6, 3.0))),
          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(
      SparseSubmatrixByRows(matrix,
                            /*start_row_id=*/2,
                            /*end_row_id=*/std::nullopt,
                            /*start_col_id=*/0,
                            /*end_col_id=*/std::nullopt),
      ElementsAre(
          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0), Pair(6, 3.0))),
          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/3,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(
                  3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/4,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/std::nullopt),
              IsEmpty());
  EXPECT_THAT(
      SparseSubmatrixByRows(matrix,
                            /*start_row_id=*/0,
                            /*end_row_id=*/4,
                            /*start_col_id=*/0,
                            /*end_col_id=*/std::nullopt),
      ElementsAre(
          Pair(0, ElementsAre(Pair(0, 2.0), Pair(4, 3.0), Pair(5, 4.0))),
          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0), Pair(6, 3.0))),
          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(
      SparseSubmatrixByRows(matrix,
                            /*start_row_id=*/0,
                            /*end_row_id=*/3,
                            /*start_col_id=*/0,
                            /*end_col_id=*/std::nullopt),
      ElementsAre(
          Pair(0, ElementsAre(Pair(0, 2.0), Pair(4, 3.0), Pair(5, 4.0))),
          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0), Pair(6, 3.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/2,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(
                  0, ElementsAre(Pair(0, 2.0), Pair(4, 3.0), Pair(5, 4.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/1,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(
                  0, ElementsAre(Pair(0, 2.0), Pair(4, 3.0), Pair(5, 4.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/0,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/std::nullopt),
              IsEmpty());
  EXPECT_THAT(
      SparseSubmatrixByRows(matrix,
                            /*start_row_id=*/2,
                            /*end_row_id=*/std::nullopt,
                            /*start_col_id=*/0,
                            /*end_col_id=*/std::nullopt),
      ElementsAre(
          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0), Pair(6, 3.0))),
          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/2,
                                    /*end_row_id=*/3,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(
                  2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0), Pair(6, 3.0)))));

  // Some selected columns.
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/1,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(0, ElementsAre(Pair(4, 3.0), Pair(5, 4.0))),
                          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0),
                                              Pair(6, 3.0))),
                          Pair(3, ElementsAre(Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/2,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(0, ElementsAre(Pair(4, 3.0), Pair(5, 4.0))),
                          Pair(2, ElementsAre(Pair(3, 1.0), Pair(6, 3.0))),
                          Pair(3, ElementsAre(Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/3,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(0, ElementsAre(Pair(4, 3.0), Pair(5, 4.0))),
                          Pair(2, ElementsAre(Pair(3, 1.0), Pair(6, 3.0))),
                          Pair(3, ElementsAre(Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/4,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(0, ElementsAre(Pair(4, 3.0), Pair(5, 4.0))),
                          Pair(2, ElementsAre(Pair(6, 3.0))),
                          Pair(3, ElementsAre(Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/5,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(0, ElementsAre(Pair(5, 4.0))),
                          Pair(2, ElementsAre(Pair(6, 3.0))),
                          Pair(3, ElementsAre(Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/6,
                                    /*end_col_id=*/std::nullopt),
              ElementsAre(Pair(2, ElementsAre(Pair(6, 3.0))),
                          Pair(3, ElementsAre(Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/7,
                                    /*end_col_id=*/std::nullopt),
              IsEmpty());
  EXPECT_THAT(
      SparseSubmatrixByRows(matrix,
                            /*start_row_id=*/0,
                            /*end_row_id=*/std::nullopt,
                            /*start_col_id=*/0,
                            /*end_col_id=*/7),
      ElementsAre(
          Pair(0, ElementsAre(Pair(0, 2.0), Pair(4, 3.0), Pair(5, 4.0))),
          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0), Pair(6, 3.0))),
          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0), Pair(6, 7.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/6),
              ElementsAre(Pair(0, ElementsAre(Pair(0, 2.0), Pair(4, 3.0),
                                              Pair(5, 4.0))),
                          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0))),
                          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/5),
              ElementsAre(Pair(0, ElementsAre(Pair(0, 2.0), Pair(4, 3.0))),
                          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0))),
                          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/4),
              ElementsAre(Pair(0, ElementsAre(Pair(0, 2.0))),
                          Pair(2, ElementsAre(Pair(1, 5.0), Pair(3, 1.0))),
                          Pair(3, ElementsAre(Pair(0, 9.0), Pair(3, 8.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/3),
              ElementsAre(Pair(0, ElementsAre(Pair(0, 2.0))),
                          Pair(2, ElementsAre(Pair(1, 5.0))),
                          Pair(3, ElementsAre(Pair(0, 9.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/2),
              ElementsAre(Pair(0, ElementsAre(Pair(0, 2.0))),
                          Pair(2, ElementsAre(Pair(1, 5.0))),
                          Pair(3, ElementsAre(Pair(0, 9.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/1),
              ElementsAre(Pair(0, ElementsAre(Pair(0, 2.0))),
                          Pair(3, ElementsAre(Pair(0, 9.0)))));
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/0,
                                    /*end_col_id=*/0),
              IsEmpty());
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/0,
                                    /*end_row_id=*/std::nullopt,
                                    /*start_col_id=*/2,
                                    /*end_col_id=*/6),
              ElementsAre(Pair(0, ElementsAre(Pair(4, 3.0), Pair(5, 4.0))),
                          Pair(2, ElementsAre(Pair(3, 1.0))),
                          Pair(3, ElementsAre(Pair(3, 8.0)))));

  // Some selected rows and columns.
  EXPECT_THAT(SparseSubmatrixByRows(matrix,
                                    /*start_row_id=*/1,
                                    /*end_row_id=*/3,
                                    /*start_col_id=*/2,
                                    /*end_col_id=*/6),
              ElementsAre(Pair(2, ElementsAre(Pair(3, 1.0)))));
}

// Returns a vector of pairs which .second is a view on the SparseVector.
//
// This makes the test simpler by being able to use ElementsAre().
std::vector<std::pair<int64_t, SparseVectorView<double>>> ToViews(
    absl::Span<const std::pair<int64_t, SparseVector<double>>> pairs) {
  std::vector<std::pair<int64_t, SparseVectorView<double>>> views;
  for (const auto& [id, sparse_vector] : pairs) {
    views.emplace_back(id, MakeView(sparse_vector));
  }
  return views;
}

TEST(TransposeSparseSubmatrixTest, EmptyMatrix) {
  EXPECT_THAT(ToViews(TransposeSparseSubmatrix(/*submatrix_by_rows=*/{})),
              IsEmpty());
}

TEST(TransposeSparseSubmatrixTest, NonEmptyMatrix) {
  // The matrix data are:
  //
  //  |0 1 2 3 4 5 6
  // -+-------------
  // 0|2 - - - 3 4 -
  // 1|- - - - - - -
  // 2|- 5 - 1 - - 3
  // 3|9 - - 8 - - 7
  const SparseDoubleMatrixProto matrix = MakeSparseDoubleMatrix({
      {0, 0, 2.0},  //
      {0, 4, 3.0},  //
      {0, 5, 4.0},  //
      {2, 1, 5.0},  //
      {2, 3, 1.0},  //
      {2, 6, 3.0},  //
      {3, 0, 9.0},  //
      {3, 3, 8.0},  //
      {3, 6, 7.0}   //
  });
  const SparseSubmatrixRowsView submatrix_by_rows =
      SparseSubmatrixByRows(matrix,
                            /*start_row_id=*/0,
                            /*end_row_id=*/std::nullopt,
                            /*start_col_id=*/0,
                            /*end_col_id=*/std::nullopt);

  EXPECT_THAT(ToViews(TransposeSparseSubmatrix(submatrix_by_rows)),
              ElementsAre(Pair(0, ElementsAre(Pair(0, 2.0), Pair(3, 9.0))),
                          Pair(1, ElementsAre(Pair(2, 5.0))),
                          Pair(3, ElementsAre(Pair(2, 1.0), Pair(3, 8.0))),
                          Pair(4, ElementsAre(Pair(0, 3.0))),
                          Pair(5, ElementsAre(Pair(0, 4.0))),
                          Pair(6, ElementsAre(Pair(2, 3.0), Pair(3, 7.0)))));
}

}  // namespace
}  // namespace operations_research::math_opt
