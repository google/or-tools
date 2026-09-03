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

#include "ortools/lp_data/permutation.h"

#include <stdio.h>

#include <cstdint>
#include <random>
#include <utility>

#include "absl/random/random.h"
#include "gtest/gtest.h"
#include "ortools/lp_data/lp_types.h"

namespace operations_research {
namespace glop {
namespace {

TEST(PermutationTest, Basic) {
  const RowIndex kSize(4);
  RowPermutation row_perm(kSize);
  row_perm[RowIndex(0)] = RowIndex(3);
  row_perm[RowIndex(1)] = RowIndex(2);
  row_perm[RowIndex(2)] = RowIndex(0);
  row_perm[RowIndex(3)] = RowIndex(1);
  DenseColumn from(kSize, 0.0);
  from[RowIndex(0)] = 0.0;
  from[RowIndex(1)] = 1.0;
  from[RowIndex(2)] = 2.0;
  from[RowIndex(3)] = 3.0;
  DenseColumn to(kSize, 0.0);
  ApplyPermutation(row_perm, from, &to);
  EXPECT_EQ(0.0, to[RowIndex(3)]);
  EXPECT_EQ(1.0, to[RowIndex(2)]);
  EXPECT_EQ(2.0, to[RowIndex(0)]);
  EXPECT_EQ(3.0, to[RowIndex(1)]);
}

TEST(PermutationTest, InversePermutation) {
  const RowIndex kSize(200);
  RowPermutation perm(kSize);
  perm.PopulateRandomly();
  EXPECT_TRUE(perm.Check());
  RowPermutation inverse_perm(kSize);
  inverse_perm.PopulateFromInverse(perm);
  EXPECT_TRUE(inverse_perm.Check());
  DenseColumn column(kSize, 0.0);
  for (RowIndex row(0); row < kSize; ++row) {
    column[row] = row.value() + 1.0;
  }
  DenseColumn permuted(kSize, 0.0);
  DenseColumn recovered(kSize, 0.0);
  ApplyPermutation(perm, column, &permuted);
  ApplyPermutation(inverse_perm, permuted, &recovered);
  for (RowIndex row(0); row < kSize; ++row) {
    EXPECT_EQ(column[row], recovered[row]);
  }
  ApplyInversePermutation(perm, permuted, &recovered);
  for (RowIndex row(0); row < kSize; ++row) {
    EXPECT_EQ(column[row], recovered[row]);
  }
}

TEST(PermutationTest, PermutationSignature) {
  const RowIndex kSize(200);
  RowPermutation perm(kSize);
  perm.PopulateFromIdentity();
  std::mt19937 random(0);
  double signature = 1.0;
  const int kIterations = 200;
  for (int i = 0; i < kIterations; ++i) {
    RowIndex row1 = RowIndex(absl::Uniform<int32_t>(random, 0, kSize.value()));
    RowIndex row2(0);
    do {
      row2 = RowIndex(absl::Uniform<int32_t>(random, 0, kSize.value()));
    } while (row1 == row2);
    using std::swap;
    swap(perm[row1], perm[row2]);
    signature = -signature;
    EXPECT_EQ(signature, perm.ComputeSignature());
  }
}

}  // namespace
}  // namespace glop
}  // namespace operations_research
