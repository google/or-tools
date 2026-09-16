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

#include "ortools/util/dense_set.h"

#include <type_traits>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"

namespace operations_research {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

DEFINE_STRONG_INT_TYPE(TestStrongInt, int);

template <typename T>
class DenseSetTest : public testing::Test {};

using SetTypes =
    ::testing::Types<DenseSet<int>, DenseSet<TestStrongInt>,
                     UnsafeDenseSet<int>, UnsafeDenseSet<TestStrongInt>>;

TYPED_TEST_SUITE(DenseSetTest, SetTypes);

TYPED_TEST(DenseSetTest, TestInsert) {
  TypeParam set;
  if (!TypeParam::kAutoResize) set.reserve(2);

  auto [it, inserted] = set.insert(static_cast<TypeParam::value_type>(1));

  EXPECT_TRUE(inserted);
  EXPECT_TRUE(set.contains(static_cast<TypeParam::value_type>(1)));
  EXPECT_EQ(*it, static_cast<TypeParam::value_type>(1));
  EXPECT_EQ(set.size(), 1);
  EXPECT_THAT(set, ElementsAre(static_cast<TypeParam::value_type>(1)));
  EXPECT_THAT(set.values(), ElementsAre(static_cast<TypeParam::value_type>(1)));
}

TYPED_TEST(DenseSetTest, TestInsertDuplicate) {
  TypeParam set;
  if (!TypeParam::kAutoResize) set.reserve(2);

  auto [it, inserted] = set.insert(static_cast<TypeParam::value_type>(1));
  auto [it2, inserted2] = set.insert(static_cast<TypeParam::value_type>(1));

  EXPECT_TRUE(inserted);
  EXPECT_FALSE(inserted2);
  EXPECT_EQ(it, it2);
}

TYPED_TEST(DenseSetTest, TestFindPresent) {
  TypeParam set;
  if (!TypeParam::kAutoResize) set.reserve(2);
  set.insert(static_cast<TypeParam::value_type>(1));

  auto it = set.find(static_cast<TypeParam::value_type>(1));

  EXPECT_EQ(*it, static_cast<TypeParam::value_type>(1));
}

TYPED_TEST(DenseSetTest, TestFindAbsent) {
  TypeParam set;
  if (!TypeParam::kAutoResize) set.reserve(3);
  set.insert(static_cast<TypeParam::value_type>(1));

  auto it = set.find(static_cast<TypeParam::value_type>(2));

  EXPECT_EQ(it, set.end());
}

TYPED_TEST(DenseSetTest, TestEraseValue) {
  TypeParam set;
  if (!TypeParam::kAutoResize) set.reserve(2001);
  set.insert(static_cast<TypeParam::value_type>(1));

  EXPECT_EQ(set.erase(static_cast<TypeParam::value_type>(0)), 0);
  EXPECT_EQ(set.erase(static_cast<TypeParam::value_type>(2000)), 0);
  EXPECT_TRUE(set.contains(static_cast<TypeParam::value_type>(1)));
  EXPECT_EQ(set.erase(static_cast<TypeParam::value_type>(1)), 1);
  EXPECT_FALSE(set.contains(static_cast<TypeParam::value_type>(1)));
  EXPECT_EQ(set.size(), 0);
  EXPECT_THAT(set, IsEmpty());
  EXPECT_THAT(set.values(), IsEmpty());
}

TYPED_TEST(DenseSetTest, TestEraseIterator) {
  TypeParam set;
  if (!TypeParam::kAutoResize) set.reserve(2);
  auto [it, inserted] = set.insert(static_cast<TypeParam::value_type>(1));

  set.erase(it);

  EXPECT_EQ(set.size(), 0);
  EXPECT_THAT(set, IsEmpty());
  EXPECT_THAT(set.values(), IsEmpty());
}

TYPED_TEST(DenseSetTest, TestClear) {
  TypeParam set;
  if (!TypeParam::kAutoResize) set.reserve(3);
  set.insert(static_cast<TypeParam::value_type>(1));
  set.insert(static_cast<TypeParam::value_type>(2));

  set.clear();

  EXPECT_EQ(set.size(), 0);
  EXPECT_THAT(set, IsEmpty());
  EXPECT_THAT(set.values(), IsEmpty());
}

TYPED_TEST(DenseSetTest, TestReserve) {
  TypeParam set;

  set.reserve(100);

  EXPECT_EQ(set.size(), 0);
  EXPECT_THAT(set, IsEmpty());
  EXPECT_THAT(set.values(), IsEmpty());
  EXPECT_EQ(set.capacity(), 100);
}

TYPED_TEST(DenseSetTest, TestConstIterators) {
  TypeParam set;
  if (!TypeParam::kAutoResize) set.reserve(2);
  auto [it, inserted] = set.insert(static_cast<TypeParam::value_type>(1));

  // Check that `*it = TypeParam::value_type();` would not compile.
  EXPECT_FALSE((std::is_assignable<decltype(*it), TypeParam>::value));
}

}  // namespace
}  // namespace operations_research
