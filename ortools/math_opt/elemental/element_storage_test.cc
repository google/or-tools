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

#include "ortools/math_opt/elemental/element_storage.h"

#include <cstdint>

#include "absl/status/status.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;

TEST(ElementStorageTest, EmptyModelGetters) {
  ElementStorage elements;
  EXPECT_EQ(elements.size(), 0);
  EXPECT_EQ(elements.next_id(), 0);
  EXPECT_THAT(elements.AllIds(), IsEmpty());
  EXPECT_FALSE(elements.Exists(0));
  EXPECT_FALSE(elements.Exists(1));
  EXPECT_FALSE(elements.Exists(-1));
}

TEST(ElementStorageTest, AddElement) {
  ElementStorage elements;
  EXPECT_EQ(elements.Add("x"), 0);
  EXPECT_EQ(elements.Add("y"), 1);
  EXPECT_EQ(elements.size(), 2);
  EXPECT_EQ(elements.next_id(), 2);
  EXPECT_THAT(elements.AllIds(), UnorderedElementsAre(0, 1));
  EXPECT_FALSE(elements.Exists(2));
  EXPECT_FALSE(elements.Exists(-1));

  ASSERT_TRUE(elements.Exists(0));
  ASSERT_TRUE(elements.Exists(1));
  EXPECT_THAT(elements.GetName(0), IsOkAndHolds("x"));
  EXPECT_THAT(elements.GetName(1), IsOkAndHolds("y"));
  EXPECT_THAT(elements.GetName(-42),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ElementStorageTest, AddElementDuplicateName) {
  ElementStorage elements;
  EXPECT_EQ(elements.Add("xyx"), 0);
  EXPECT_EQ(elements.Add("xyx"), 1);
  EXPECT_EQ(elements.size(), 2);
  EXPECT_EQ(elements.next_id(), 2);
  EXPECT_THAT(elements.AllIds(), UnorderedElementsAre(0, 1));

  ASSERT_TRUE(elements.Exists(0));
  ASSERT_TRUE(elements.Exists(1));
  EXPECT_THAT(elements.GetName(0), IsOkAndHolds("xyx"));
  EXPECT_THAT(elements.GetName(1), IsOkAndHolds("xyx"));
}

TEST(ElementStorageTest, DeleteElement) {
  ElementStorage elements;
  const int64_t x = elements.Add("x");
  const int64_t y = elements.Add("y");
  const int64_t z = elements.Add("z");

  EXPECT_TRUE(elements.Erase(x));
  EXPECT_TRUE(elements.Erase(z));
  EXPECT_EQ(elements.size(), 1);
  EXPECT_EQ(elements.next_id(), 3);
  EXPECT_THAT(elements.AllIds(), UnorderedElementsAre(y));
  EXPECT_FALSE(elements.Exists(x));
  EXPECT_FALSE(elements.Exists(z));
  EXPECT_FALSE(elements.Exists(3));
  EXPECT_FALSE(elements.Exists(-1));

  ASSERT_TRUE(elements.Exists(y));
  EXPECT_THAT(elements.GetName(y), IsOkAndHolds("y"));
  EXPECT_THAT(elements.GetName(x),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ElementStorageTest, DeleteInvalidIdNoEffect) {
  ElementStorage elements;
  const int64_t x = elements.Add("x");
  const int64_t y = elements.Add("y");
  const int64_t z = elements.Add("z");

  EXPECT_FALSE(elements.Erase(-2));
  EXPECT_FALSE(elements.Erase(5));

  EXPECT_EQ(elements.size(), 3);
  EXPECT_EQ(elements.next_id(), 3);
  EXPECT_THAT(elements.AllIds(), UnorderedElementsAre(x, y, z));
}

TEST(ElementStorageTest, DeleteTwiceNoAdditionalEffect) {
  ElementStorage elements;
  const int64_t x = elements.Add("x");
  const int64_t y = elements.Add("y");
  const int64_t z = elements.Add("z");

  EXPECT_TRUE(elements.Erase(y));
  EXPECT_FALSE(elements.Erase(y));

  EXPECT_EQ(elements.size(), 2);
  EXPECT_EQ(elements.next_id(), 3);
  EXPECT_THAT(elements.AllIds(), UnorderedElementsAre(x, z));
}

TEST(ElementStorageTest, EnsureNextIdAtLeastIncreasesNextId) {
  ElementStorage elements;
  elements.EnsureNextIdAtLeast(5);
  EXPECT_EQ(elements.next_id(), 5);
  EXPECT_EQ(elements.Add("x"), 5);
  EXPECT_THAT(elements.AllIds(), UnorderedElementsAre(5));
}

TEST(ElementStorageTest, EnsureNextIdAtLeastNoEffect) {
  ElementStorage elements;
  elements.Add("x");
  elements.EnsureNextIdAtLeast(0);
  EXPECT_EQ(elements.next_id(), 1);
  EXPECT_EQ(elements.Add("y"), 1);
  EXPECT_THAT(elements.AllIds(), UnorderedElementsAre(0, 1));
}

void BM_AddElements(benchmark::State& state) {
  const int n = state.range(0);
  for (auto s : state) {
    ElementStorage storage;
    for (int i = 0; i < n; ++i) {
      storage.Add("");
    }
    benchmark::DoNotOptimize(storage);
  }
}

BENCHMARK(BM_AddElements)->Arg(100)->Arg(10000);

void BM_Exists(benchmark::State& state) {
  const int n = state.range(0);
  ElementStorage storage;
  for (int i = 0; i < n; ++i) {
    storage.Add("");
  }
  for (auto s : state) {
    for (int i = 0; i < 2 * n; ++i) {
      bool e = storage.Exists(i);
      benchmark::DoNotOptimize(e);
    }
  }
}

BENCHMARK(BM_Exists)->Arg(100)->Arg(10000);

}  // namespace
}  // namespace operations_research::math_opt
