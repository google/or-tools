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

#include "ortools/util/cached_log.h"

#include <math.h>

#include <cstdint>

#include "gtest/gtest.h"

namespace {

TEST(CachedLogTest, LogAccess) {
  const int kSize = 100;
  operations_research::CachedLog cache;
  for (int64_t i = 1; i < kSize; ++i) {
    EXPECT_DOUBLE_EQ(log2(i), cache.Log2(i));
  }
  cache.Init(kSize / 2);
  for (int64_t i = 1; i < kSize; ++i) {
    EXPECT_DOUBLE_EQ(log2(i), cache.Log2(i));
  }
}

}  // namespace
