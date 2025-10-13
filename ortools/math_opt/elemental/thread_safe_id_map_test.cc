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

#include "ortools/math_opt/elemental/thread_safe_id_map.h"

#include <cstdint>
#include <memory>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

TEST(ThreadSafeSmallMapTest, Empty) {
  ThreadSafeIdMap<int> m;
  EXPECT_THAT(m.UpdateAndGetAll(), IsEmpty());
  EXPECT_EQ(m.Get(0), nullptr);
  EXPECT_EQ(m.Size(), 0);
}

TEST(ThreadSafeSmallMapTest, InsertAndGet) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  EXPECT_EQ(x, 0);
  EXPECT_THAT(m.Get(x), Pointee(17));
  EXPECT_EQ(m.Size(), 1);
  EXPECT_THAT(m.GetAll(), UnorderedElementsAre(Pair(x, Pointee(17))));
}

TEST(ThreadSafeSmallMapTest, UpdateAndGet) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  EXPECT_THAT(m.UpdateAndGet(x), Pointee(17));
  EXPECT_EQ(m.Size(), 1);
  EXPECT_THAT(m.GetAll(), UnorderedElementsAre(Pair(x, Pointee(17))));
}

TEST(ThreadSafeSmallMapTest, UpdateAndGetNotPresentIsNull) {
  ThreadSafeIdMap<int> m;
  EXPECT_EQ(m.UpdateAndGet(0), nullptr);
}

TEST(ThreadSafeSmallMapTest, UpdateAndGetAll) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  const int64_t y = m.Insert(std::make_unique<int>(33));
  EXPECT_THAT(m.UpdateAndGetAll(),
              UnorderedElementsAre(Pair(x, Pointee(17)), Pair(y, Pointee(33))));
  EXPECT_EQ(m.Size(), 2);
}

TEST(ThreadSafeSmallMapTest, GetAfterUpdate) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  m.UpdateAndGetAll();
  EXPECT_THAT(m.Get(x), Pointee(17));
}

TEST(ThreadSafeSmallMapTest, EraseBeforeUpdate) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  EXPECT_TRUE(m.Erase(x));
  EXPECT_EQ(m.Get(x), nullptr);
  EXPECT_EQ(m.Size(), 0);
  EXPECT_THAT(m.GetAll(), IsEmpty());
  EXPECT_THAT(m.UpdateAndGetAll(), IsEmpty());
}

TEST(ThreadSafeSmallMapTest, EraseAfterUpdate) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  m.UpdateAndGet(x);
  EXPECT_TRUE(m.Erase(x));
  EXPECT_EQ(m.Get(x), nullptr);
  EXPECT_EQ(m.Size(), 0);
  EXPECT_THAT(m.GetAll(), IsEmpty());
  EXPECT_THAT(m.UpdateAndGetAll(), IsEmpty());
}

TEST(ThreadSafeSmallMapTest, EraseTwiceBeforeUpdate) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  EXPECT_TRUE(m.Erase(x));
  EXPECT_FALSE(m.Erase(x));
  EXPECT_THAT(m.GetAll(), IsEmpty());
}

TEST(ThreadSafeSmallMapTest, EraseTwiceAfterUpdate) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  m.UpdateAndGet(x);
  EXPECT_TRUE(m.Erase(x));
  EXPECT_FALSE(m.Erase(x));
  EXPECT_THAT(m.GetAll(), IsEmpty());
}

TEST(ThreadSafeSmallMapTest, EraseHasPendingInsertEraseOther) {
  ThreadSafeIdMap<int> m;
  const int64_t x = m.Insert(std::make_unique<int>(17));
  m.UpdateAndGetAll();
  const int64_t y = m.Insert(std::make_unique<int>(33));
  EXPECT_TRUE(m.Erase(x));
  EXPECT_THAT(m.Get(y), Pointee(Eq(33)));
  EXPECT_EQ(m.Size(), 1);
}

}  // namespace
}  // namespace operations_research::math_opt
