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

#include "ortools/math_opt/solvers/glpk/glpk_sparse_vector.h"

#include <optional>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;

// Returns a dense zero-based version of the input sparse vector.
std::vector<std::optional<double>> DenseVector(const GlpkSparseVector& v) {
  std::vector<std::optional<double>> dense;
  for (int i = 1; i <= v.capacity(); ++i) {
    dense.push_back(v.Get(i));
  }
  return dense;
}

TEST(GlpkSparseVectorTest, ZeroCapacity) {
  GlpkSparseVector empty(0);
  const GlpkSparseVector& const_ref = empty;

  EXPECT_EQ(const_ref.capacity(), 0);
  EXPECT_EQ(const_ref.size(), 0);
  EXPECT_NE(const_ref.indices(), nullptr);
  EXPECT_NE(const_ref.values(), nullptr);

  empty.Clear();

  EXPECT_EQ(const_ref.capacity(), 0);
  EXPECT_EQ(const_ref.size(), 0);
  EXPECT_NE(const_ref.indices(), nullptr);
  EXPECT_NE(const_ref.values(), nullptr);
}

TEST(GlpkSparseVectorTest, Set) {
  GlpkSparseVector v(5);

  EXPECT_THAT(DenseVector(v),
              ElementsAre(std::nullopt, std::nullopt, std::nullopt,
                          std::nullopt, std::nullopt));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 0);
  EXPECT_NE(v.indices(), nullptr);
  EXPECT_NE(v.values(), nullptr);

  v.Set(3, 2.5);

  EXPECT_THAT(DenseVector(v), ElementsAre(std::nullopt, std::nullopt, 2.5,
                                          std::nullopt, std::nullopt));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 1);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], 2.5);

  v.Set(1, 1.0);

  EXPECT_THAT(DenseVector(v),
              ElementsAre(1.0, std::nullopt, 2.5, std::nullopt, std::nullopt));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 2);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], 2.5);
  EXPECT_EQ(v.indices()[2], 1);
  EXPECT_EQ(v.values()[2], 1.0);

  v.Set(5, -1.0);

  EXPECT_THAT(DenseVector(v),
              ElementsAre(1.0, std::nullopt, 2.5, std::nullopt, -1.0));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 3);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], 2.5);
  EXPECT_EQ(v.indices()[2], 1);
  EXPECT_EQ(v.values()[2], 1.0);
  EXPECT_EQ(v.indices()[3], 5);
  EXPECT_EQ(v.values()[3], -1.0);

  v.Set(3, -6.0);

  EXPECT_THAT(DenseVector(v),
              ElementsAre(1.0, std::nullopt, -6.0, std::nullopt, -1.0));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 3);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], -6.0);
  EXPECT_EQ(v.indices()[2], 1);
  EXPECT_EQ(v.values()[2], 1.0);
  EXPECT_EQ(v.indices()[3], 5);
  EXPECT_EQ(v.values()[3], -1.0);
}

TEST(GlpkSparseVectorTest, SetClearSet) {
  GlpkSparseVector v(5);

  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 0);
  EXPECT_NE(v.indices(), nullptr);
  EXPECT_NE(v.values(), nullptr);

  v.Set(3, 2.5);
  v.Set(1, 1.0);
  v.Set(5, -1.0);
  v.Set(3, -6.0);

  EXPECT_THAT(DenseVector(v),
              ElementsAre(1.0, std::nullopt, -6.0, std::nullopt, -1.0));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 3);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], -6.0);
  EXPECT_EQ(v.indices()[2], 1);
  EXPECT_EQ(v.values()[2], 1.0);
  EXPECT_EQ(v.indices()[3], 5);
  EXPECT_EQ(v.values()[3], -1.0);

  v.Clear();

  EXPECT_THAT(DenseVector(v),
              ElementsAre(std::nullopt, std::nullopt, std::nullopt,
                          std::nullopt, std::nullopt));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 0);
  EXPECT_NE(v.indices(), nullptr);
  EXPECT_NE(v.values(), nullptr);

  v.Set(3, 2.5);

  EXPECT_THAT(DenseVector(v), ElementsAre(std::nullopt, std::nullopt, 2.5,
                                          std::nullopt, std::nullopt));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 1);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], 2.5);

  v.Set(2, -2.5);

  EXPECT_THAT(DenseVector(v),
              ElementsAre(std::nullopt, -2.5, 2.5, std::nullopt, std::nullopt));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 2);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], 2.5);
  EXPECT_EQ(v.indices()[2], 2);
  EXPECT_EQ(v.values()[2], -2.5);

  v.Set(3, 0.0);

  EXPECT_THAT(DenseVector(v),
              ElementsAre(std::nullopt, -2.5, 0.0, std::nullopt, std::nullopt));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 2);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], 0.0);
  EXPECT_EQ(v.indices()[2], 2);
  EXPECT_EQ(v.values()[2], -2.5);
}

TEST(GlpkSparseVectorTest, Load) {
  GlpkSparseVector v(5);

  v.Load([](int* const indices, double* const values) {
    indices[1] = 3;
    values[1] = 0.0;
    indices[2] = 1;
    values[2] = 5.25;
    indices[3] = 5;
    values[3] = -2.0;
    return 3;
  });

  EXPECT_THAT(DenseVector(v),
              ElementsAre(5.25, std::nullopt, 0.0, std::nullopt, -2.0));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 3);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], 0.0);
  EXPECT_EQ(v.indices()[2], 1);
  EXPECT_EQ(v.values()[2], 5.25);
  EXPECT_EQ(v.indices()[3], 5);
  EXPECT_EQ(v.values()[3], -2.0);
}

TEST(GlpkSparseVectorTest, SetLoad) {
  GlpkSparseVector v(5);

  v.Set(2, 5.0);
  v.Set(3, -8.0);

  v.Load([](int* const indices, double* const values) {
    indices[1] = 3;
    values[1] = 0.0;
    indices[2] = 1;
    values[2] = 5.25;
    indices[3] = 5;
    values[3] = -2.0;
    return 3;
  });

  EXPECT_THAT(DenseVector(v),
              ElementsAre(5.25, std::nullopt, 0.0, std::nullopt, -2.0));
  EXPECT_EQ(v.capacity(), 5);
  EXPECT_EQ(v.size(), 3);
  ASSERT_NE(v.indices(), nullptr);
  ASSERT_NE(v.values(), nullptr);
  EXPECT_EQ(v.indices()[1], 3);
  EXPECT_EQ(v.values()[1], 0.0);
  EXPECT_EQ(v.indices()[2], 1);
  EXPECT_EQ(v.values()[2], 5.25);
  EXPECT_EQ(v.indices()[3], 5);
  EXPECT_EQ(v.values()[3], -2.0);
}

TEST(GlpkSparseVectorDeathTest, NegativeCapacity) {
  EXPECT_DEATH(GlpkSparseVector(-1), "capacity");
}

TEST(GlpkSparseVectorDeathTest, Get) {
  GlpkSparseVector v(5);
  v.Set(2, 3.2);

  EXPECT_DEATH(v.Get(0), "index");
  EXPECT_DEATH(v.Get(6), "index");
}

TEST(GlpkSparseVectorDeathTest, Set) {
  GlpkSparseVector v(5);
  v.Set(2, 3.2);

  EXPECT_DEATH(v.Set(0, 1.0), "index");
  EXPECT_DEATH(v.Set(6, 3.25), "index");
}

TEST(GlpkSparseVectorDeathTest, Load) {
  GlpkSparseVector v(5);
  v.Set(2, 3.2);

  EXPECT_DEATH(
      v.Load([](int* const indices, double* const values) { return -1; }),
      "size");
  EXPECT_DEATH(
      v.Load([](int* const indices, double* const values) { return 6; }),
      "size");
  EXPECT_DEATH(v.Load([](int* const indices, double* const values) {
    indices[1] = 0;
    return 1;
  }),
               "index");
  EXPECT_DEATH(v.Load([](int* const indices, double* const values) {
    indices[1] = 6;
    return 1;
  }),
               "index");
  EXPECT_DEATH(v.Load([](int* const indices, double* const values) {
    indices[1] = 3;
    indices[2] = 3;
    return 2;
  }),
               "duplicated");
}

}  // namespace
}  // namespace operations_research::math_opt
