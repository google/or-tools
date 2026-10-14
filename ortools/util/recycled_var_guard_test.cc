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

#include "ortools/util/recycled_var_guard.h"

#include <vector>

#include "absl/base/thread_annotations.h"
#include "gtest/gtest.h"

namespace operations_research::sat {
namespace {

class GoodClass {
 public:
  void UsesRecycledVar() {
    RecycledVarLock lock(recycled_guard_);
    recycled_.clear();
    recycled_.push_back(1);
    UsesRecycledVarToo(recycled_);
  }

  void UsesRecycledVarToo(std::vector<int>& recycled) { recycled.push_back(2); }

 private:
  [[no_unique_address]] RecycledVarGuard recycled_guard_;
  std::vector<int> recycled_ ABSL_GUARDED_BY(recycled_guard_);
};

TEST(GuardTest, GoodClass) {
  GoodClass good;
  good.UsesRecycledVar();
}

class BadClass {
 public:
  void UsesRecycledVar() {
    RecycledVarLock lock(recycled_guard_);
    recycled_.clear();
    recycled_.push_back(1);
    UsesRecycledVarToo();
  }

  void UsesRecycledVarToo() {
    RecycledVarLock lock(recycled_guard_);
    recycled_.clear();
    recycled_.push_back(2);
  }

 private:
  [[no_unique_address]] RecycledVarGuard recycled_guard_;
  std::vector<int> recycled_ ABSL_GUARDED_BY(recycled_guard_);
};

TEST(GuardTest, BadClass) {
  BadClass bad;
#ifndef NDEBUG
  EXPECT_DEATH(bad.UsesRecycledVar(), "recycled var is already being used");
#else
  bad.UsesRecycledVar();
#endif
}

TEST(SizeTest, ZeroCostInReleaseMode) {
#ifdef NDEBUG
  EXPECT_EQ(sizeof(GoodClass), sizeof(std::vector<int>));
  EXPECT_EQ(sizeof(BadClass), sizeof(std::vector<int>));
#endif
}

}  // namespace
}  // namespace operations_research::sat
