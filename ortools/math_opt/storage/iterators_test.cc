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

#include "ortools/math_opt/storage/iterators.h"

#include <memory>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;

TEST(MakeUpdateDataFieldIteratorTest, SimpleIteration) {
  struct S {
    explicit S(const int x) : x(x) {}
    int x;
  };
  struct FakeUpdateData {
    explicit FakeUpdateData(S s) : s(std::move(s)) {}
    S s;
  };
  std::vector<std::pair<int, std::unique_ptr<FakeUpdateData>>> update_trackers;
  update_trackers.emplace_back(3, std::make_unique<FakeUpdateData>(S(-3)));
  update_trackers.emplace_back(2, std::make_unique<FakeUpdateData>(S(-2)));

  const auto it_begin =
      MakeUpdateDataFieldIterator<&FakeUpdateData::s>(update_trackers.begin());
  const auto it_end =
      MakeUpdateDataFieldIterator<&FakeUpdateData::s>(update_trackers.end());
  for (auto it = it_begin; it != it_end; ++it) {
    S& s = *it;
    s.x *= -2;
  }

  std::vector<int> xs;
  for (const auto& [_, p] : update_trackers) {
    xs.push_back(p->s.x);
  }
  EXPECT_THAT(xs, ElementsAre(6, 4));
}

TEST(MakeUpdateDataFieldIteratorTest, SimpleRange) {
  struct S {
    explicit S(const int x) : x(x) {}
    int x;
  };
  struct FakeUpdateData {
    explicit FakeUpdateData(S s) : s(std::move(s)) {}
    S s;
  };
  std::vector<std::pair<int, std::unique_ptr<FakeUpdateData>>> update_trackers;
  update_trackers.emplace_back(3, std::make_unique<FakeUpdateData>(S(-3)));
  update_trackers.emplace_back(2, std::make_unique<FakeUpdateData>(S(-2)));

  for (S& s : MakeUpdateDataFieldRange<&FakeUpdateData::s>(update_trackers)) {
    s.x *= -2;
  }

  std::vector<int> xs;
  for (const auto& [_, p] : update_trackers) {
    xs.push_back(p->s.x);
  }
  EXPECT_THAT(xs, ElementsAre(6, 4));
}

}  // namespace
}  // namespace operations_research::math_opt
