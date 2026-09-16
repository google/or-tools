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

#include "ortools/util/affine_relation.h"

#include "gtest/gtest.h"

namespace operations_research {
namespace {

TEST(AffineRelationTest, Empty) {
  AffineRelation r;
  EXPECT_EQ(r.Get(10), AffineRelation::Relation(10, 1, 0));
}

TEST(AffineRelationTest, CoeffOfMagnitudeOne) {
  AffineRelation r;
  r.TryAdd(1, 0, /*coeff=*/1, /*offset=*/1);    // r1 = r0 + 1
  r.TryAdd(2, 0, /*coeff=*/1, /*offset=*/2);    // r2 = r0 + 2
  r.TryAdd(3, 0, /*coeff=*/-1, /*offset=*/3);   // r3 = -r0 + 3
  r.TryAdd(4, 0, /*coeff=*/-1, /*offset=*/4);   // r4 = -r0 + 4
  r.TryAdd(5, 2, /*coeff=*/-1, /*offset=*/-3);  // r5 = -r2 - 3 = -r0 - 5
  r.TryAdd(6, 3, /*coeff=*/-1, /*offset=*/-3);  // r6 = -r3 - 3 = -r0 - 8
  r.TryAdd(7, 4, /*coeff=*/-1, /*offset=*/-3);  // r7 = -r4 - 3 =  r0 - 7

  EXPECT_EQ(r.Get(0), AffineRelation::Relation(0, 1, 0));
  EXPECT_EQ(r.Get(1), AffineRelation::Relation(0, 1, 1));
  EXPECT_EQ(r.Get(2), AffineRelation::Relation(0, 1, 2));
  EXPECT_EQ(r.Get(3), AffineRelation::Relation(0, -1, 3));
  EXPECT_EQ(r.Get(4), AffineRelation::Relation(0, -1, 4));
  EXPECT_EQ(r.Get(5), AffineRelation::Relation(0, -1, -1 * 2 - 3));
  EXPECT_EQ(r.Get(6), AffineRelation::Relation(0, 1, -1 * 3 - 3));
  EXPECT_EQ(r.Get(7), AffineRelation::Relation(0, 1, -1 * 4 - 3));
}

TEST(AffineRelationTest, ChainOfMultiple) {
  AffineRelation r;
  EXPECT_TRUE(r.TryAdd(1, 0, /*coeff=*/2, /*offset=*/1));
  EXPECT_TRUE(r.TryAdd(2, 1, /*coeff=*/2, /*offset=*/1));
  EXPECT_TRUE(r.TryAdd(3, 2, /*coeff=*/2, /*offset=*/1));
  EXPECT_TRUE(r.TryAdd(4, 3, /*coeff=*/2, /*offset=*/1));

  for (int i = 0; i < 4; ++i) {
    EXPECT_EQ(r.Get(i), AffineRelation::Relation(0, 1 << i, (1 << i) - 1));
  }

  EXPECT_TRUE(r.TryAdd(6, 5, /*coeff=*/3, /*offset=*/0));

  // We can't add any of the following.
  EXPECT_FALSE(r.TryAdd(1, 0, 2, 1));
  EXPECT_FALSE(r.TryAdd(3, 4, 2, 1));
  EXPECT_FALSE(r.TryAdd(1, 6, 1, 0));
  EXPECT_FALSE(r.TryAdd(6, 1, 1, 0));

  // But we can connect the root of the chain.
  EXPECT_TRUE(r.TryAdd(0, 6, 1, 0));
}

}  // namespace
}  // namespace operations_research
