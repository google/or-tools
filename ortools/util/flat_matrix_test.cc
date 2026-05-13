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

#include "ortools/util/flat_matrix.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/log/check.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/util/random_engine.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

TEST(FlatMatrixTest, ConstructionAndSizeGetters) {
  FlatMatrix<int> m(2, 3);
  EXPECT_EQ(m.num_rows(), 2);
  EXPECT_EQ(m.num_cols(), 3);
}

TEST(FlatMatrixTest, ZeroRows) {
  FlatMatrix<int64_t> m(0, 42);
  EXPECT_EQ(m.num_rows(), 0);
  EXPECT_EQ(m.num_cols(), 42);
}

TEST(FlatMatrixTest, ZeroCols) {
  FlatMatrix<std::string> m(42, 0);
  EXPECT_EQ(m.num_rows(), 42);
  EXPECT_EQ(m.num_cols(), 0);
}

TEST(FlatMatrixTest, ZeroRowsAndZeroCols) {
  FlatMatrix<char> m(0, 0);
  EXPECT_EQ(m.num_rows(), 0);
  EXPECT_EQ(m.num_cols(), 0);
}

TEST(FlatMatrixTest, DefaultConstructor) {
  FlatMatrix<bool> m;
  EXPECT_EQ(m.num_rows(), 0);
  EXPECT_EQ(m.num_cols(), 0);
}

TEST(FlatMatrixTest, ElementsAreZeroedAtConstruction) {
  FlatMatrix<int> m(2, 3);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      ASSERT_EQ(m[row][col], 0) << DUMP_VARS(row, col);
    }
  }
}

TEST(FlatMatrixTest, InitializeWithNonDefault) {
  FlatMatrix<std::string> m(5, 4, "Hello");
  for (int row = 0; row < 5; ++row) {
    for (int col = 0; col < 4; ++col) {
      ASSERT_EQ(m[row][col], "Hello") << DUMP_VARS(row, col);
    }
  }
}

TEST(FlatMatrixTest, WriteAndReadElements) {
  FlatMatrix<int> m(2, 3);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      m[row][col] = 100 * row + 10 * col;
    }
  }
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      EXPECT_EQ(m[row][col], 100 * row + 10 * col) << DUMP_VARS(row, col);
    }
  }
}

TEST(FlatMatrixTest, CopyAndMoveOperators) {
  FlatMatrix<int> m(2, 3);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      m[row][col] = 100 * row + 10 * col;
    }
  }
  // We chain a copy and a move to reduce the amount of test code.
  FlatMatrix<int> copied_construction = m;
  FlatMatrix<int> copied_assigned(4, 5);
  copied_assigned = m;
  FlatMatrix<int> moved_construction = std::move(copied_construction);
  FlatMatrix<int> moved_assigned(4, 5);
  moved_assigned = std::move(copied_assigned);

  ASSERT_EQ(moved_construction.num_rows(), 2);
  ASSERT_EQ(moved_construction.num_cols(), 3);
  ASSERT_EQ(moved_assigned.num_rows(), 2);
  ASSERT_EQ(moved_assigned.num_cols(), 3);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      ASSERT_EQ(moved_construction[row][col], 100 * row + 10 * col)
          << DUMP_VARS(row, col);
      ASSERT_EQ(moved_assigned[row][col], 100 * row + 10 * col)
          << DUMP_VARS(row, col);
    }
  }
}

TEST(FlatMatrixTest, MoveDoesPerformRealMove) {
  // Move-construct a ~7M element matrix a million times: if it wasn't a move,
  // it would be far too slow and need huge amounts of memory.
  std::vector<std::unique_ptr<FlatMatrix<double>>> matrices(1'000'000);
  matrices[0] = std::make_unique<FlatMatrix<double>>(2345, 3456);
  (*matrices[0])[2344][3455] = 42.42;
  for (int i = 1; i < 1000000; ++i) {
    matrices[i] =
        std::make_unique<FlatMatrix<double>>(std::move(*matrices[i - 1]));
  }
  EXPECT_EQ((*matrices.back())[2344][3455], 42.42);
}

TEST(FlatMatrixTest, RowIteration) {
  FlatMatrix<int> m(2, 3);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      m[row][col] = 100 * row + 10 * col;
    }
  }
  EXPECT_THAT(m[1], ElementsAre(100, 110, 120));
}

TEST(FlatMatrixTest, RowIterationOnEmptyRow) {
  FlatMatrix<int> m(3, 0);
  EXPECT_THAT(m[2], IsEmpty());
}

TEST(FlatMatrixTest, AllElementsConst) {
  FlatMatrix<int> m(2, 3);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      m[row][col] = 100 * row + 10 * col;
    }
  }
  const FlatMatrix<int> const_m = m;
  EXPECT_THAT(const_m.all_elements(), ElementsAre(0, 10, 20, 100, 110, 120));
}

TEST(FlatMatrixTest, AllElementsMutable) {
  FlatMatrix<int> m(2, 3);
  int value = 10;
  for (int& i : m.all_elements()) {
    i = value++;
  }
  EXPECT_THAT(m.all_elements(), ElementsAre(10, 11, 12, 13, 14, 15));
}

TEST(FlatMatrixTest, AllElementsInlineMutation) {
  FlatMatrix<int> m(2, 3);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      m[row][col] = 100 * row + 10 * col;
    }
  }
  for (int& i : m.all_elements()) {
    i *= 10;
  }
  EXPECT_THAT(m.all_elements(), ElementsAre(0, 100, 200, 1000, 1100, 1200));
}

TEST(FlatMatrixTest, AllElementsEmptyMatrix) {
  FlatMatrix<int> m;
  EXPECT_THAT(m.all_elements(), ElementsAre());
}

TEST(FlatMatrixTest, RowIterator) {
  FlatMatrix<int> m(2, 3);
  for (int row = 0; row < 2; ++row) {
    for (int col = 0; col < 3; ++col) {
      m[row][col] = 100 * row + 10 * col;
    }
  }
  EXPECT_THAT(m.rows(),
              ElementsAre(ElementsAre(0, 10, 20), ElementsAre(100, 110, 120)));
}

TEST(FlatMatrixTest, RowIteratorOnEmptyMatrix) {
  FlatMatrix<int> m;
  EXPECT_THAT(m.rows(), ElementsAre());
}

TEST(FlatMatrixTest, RowIteratorOnZeroColumnMatrix) {
  FlatMatrix<int> m(3, 0);
  EXPECT_THAT(m.rows(),
              ElementsAre(ElementsAre(), ElementsAre(), ElementsAre()));
}

}  // namespace
}  // namespace operations_research
