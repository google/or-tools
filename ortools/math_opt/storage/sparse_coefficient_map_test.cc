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

#include "ortools/math_opt/storage/sparse_coefficient_map.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {
namespace {

using ::testing::EqualsProto;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(SparseCoefficientMapTest, SettingAndGetting) {
  const VariableId u(0);
  const VariableId v(1);

  SparseCoefficientMap map;
  EXPECT_EQ(map.get(u), 0.0);
  EXPECT_EQ(map.get(v), 0.0);
  EXPECT_THAT(map.terms(), IsEmpty());

  EXPECT_FALSE(map.set(u, 0.0));
  EXPECT_TRUE(map.set(u, 1.0));
  EXPECT_FALSE(map.set(u, 1.0));
  EXPECT_EQ(map.get(u), 1.0);
  EXPECT_EQ(map.get(v), 0.0);
  EXPECT_THAT(map.terms(), UnorderedElementsAre(Pair(u, 1.0)));

  EXPECT_TRUE(map.set(v, 2.0));
  EXPECT_EQ(map.get(u), 1.0);
  EXPECT_EQ(map.get(v), 2.0);
  EXPECT_THAT(map.terms(), UnorderedElementsAre(Pair(u, 1.0), Pair(v, 2.0)));

  EXPECT_TRUE(map.set(u, 0.0));
  EXPECT_EQ(map.get(u), 0.0);
  EXPECT_FALSE(map.set(u, 0.0));
  EXPECT_EQ(map.get(v), 2.0);
  EXPECT_THAT(map.terms(), UnorderedElementsAre(Pair(v, 2.0)));
}

TEST(SparseCoefficientMapTest, FromMap) {
  const VariableId u(0);
  const VariableId v(1);
  const SparseCoefficientMap map({{u, 1.0}, {v, 0.0}});

  EXPECT_EQ(map.get(u), 1.0);
  EXPECT_EQ(map.get(v), 0.0);
  EXPECT_THAT(map.terms(), UnorderedElementsAre(Pair(u, 1.0)));
}

TEST(SparseCoefficientMapTest, Erase) {
  const VariableId u(0);
  const VariableId v(1);
  const VariableId w(3);
  SparseCoefficientMap map({{u, 1.0}, {v, 3.0}, {w, 5.0}});
  EXPECT_THAT(map.terms(),
              UnorderedElementsAre(Pair(u, 1.0), Pair(v, 3.0), Pair(w, 5.0)));
  map.erase(v);
  EXPECT_EQ(map.get(v), 0.0);
  EXPECT_THAT(map.terms(), UnorderedElementsAre(Pair(u, 1.0), Pair(w, 5.0)));
}

TEST(SparseCoefficientMapTest, Clear) {
  const VariableId u(0);
  const VariableId v(1);
  const VariableId w(3);
  SparseCoefficientMap map({{u, 1.0}, {v, 3.0}, {w, 5.0}});
  EXPECT_THAT(map.terms(),
              UnorderedElementsAre(Pair(u, 1.0), Pair(v, 3.0), Pair(w, 5.0)));
  map.clear();
  EXPECT_EQ(map.get(u), 0.0);
  EXPECT_EQ(map.get(v), 0.0);
  EXPECT_EQ(map.get(w), 0.0);
  EXPECT_THAT(map.terms(), IsEmpty());
}

TEST(SparseCoefficientMapTest, Proto) {
  const SparseCoefficientMap map(
      {{VariableId(4), 1.0}, {VariableId(1), -1.0}, {VariableId(12), -4.0}});
  SparseDoubleVectorProto expected;
  expected.add_ids(1);
  expected.add_ids(4);
  expected.add_ids(12);
  expected.add_values(-1.0);
  expected.add_values(1.0);
  expected.add_values(-4.0);
  EXPECT_THAT(map.Proto(), EqualsProto(expected));
}

}  // namespace
}  // namespace operations_research::math_opt
