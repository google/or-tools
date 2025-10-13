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

#include "ortools/math_opt/elemental/tagged_id.h"

#include <array>
#include <cstdint>
#include <ostream>
#include <utility>

#include "absl/hash/hash_testing.h"
#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/testing/stream.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;

using TestIntId = TaggedId<84>;

enum class TestEnum { kValue0 };

std::ostream& operator<<(std::ostream& ostr, const TestEnum& e) {
  switch (e) {
    case TestEnum::kValue0:
      ostr << "kValue0";
      return ostr;
  }
}

using TestEnumId = TaggedId<TestEnum::kValue0>;

TEST(TestIntId, Valid) {
  TestIntId id(42);
  EXPECT_TRUE(id.IsValid());
  EXPECT_EQ(id.value(), 42);
  EXPECT_EQ(id.tag_value(), 84);
  EXPECT_EQ(absl::StrCat(id), "84{42}");
  EXPECT_EQ(StreamToString(id), "84{42}");
}

TEST(TestEnumId, Valid) {
  TestEnumId var(42);
  EXPECT_TRUE(var.IsValid());
  EXPECT_EQ(var.value(), 42);
  EXPECT_EQ(var.tag_value(), TestEnum::kValue0);
  EXPECT_EQ(absl::StrCat(var), "kValue0{42}");
  EXPECT_EQ(StreamToString(var), "kValue0{42}");
}

TEST(TestIntId, Invalid) {
  TestIntId var;
  EXPECT_FALSE(var.IsValid());
  EXPECT_EQ(var.tag_value(), 84);
  EXPECT_EQ(absl::StrCat(var), "84{invalid}");
}

TEST(TestEnumId, Invalid) {
  TestEnumId var;
  EXPECT_FALSE(var.IsValid());
  EXPECT_EQ(var.tag_value(), TestEnum::kValue0);
  EXPECT_EQ(absl::StrCat(var), "kValue0{invalid}");
}

TEST(TestIntId, Hashing) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      TestIntId(1),
      TestIntId(2),
      TestIntId(),
  }));
}

TEST(TestEnumId, Hashing) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      TestEnumId(1),
      TestEnumId(2),
      TestEnumId(),
  }));
}

TEST(TaggedIdsVectorTest, Works) {
  TaggedIdsVector<TestEnum::kValue0> ids({1, 2, 3});
  EXPECT_EQ(ids.size(), 3);
  EXPECT_EQ(ids[0], TestEnumId(1));
  EXPECT_EQ(ids[1], TestEnumId(2));
  EXPECT_EQ(ids[2], TestEnumId(3));
  EXPECT_THAT(ids, ElementsAre(TestEnumId(1), TestEnumId(2), TestEnumId(3)));

  // Test move ctor.
  TaggedIdsVector<TestEnum::kValue0> ids2 = std::move(ids);
  EXPECT_THAT(ids2, ElementsAre(TestEnumId(1), TestEnumId(2), TestEnumId(3)));
}

TEST(TaggedIdsConstViewTest, Works) {
  std::array<int64_t, 3> values = {1, 2, 3};
  TaggedIdsConstView<TestEnum::kValue0, std::array<int64_t, 3>> ids(&values);
  EXPECT_EQ(ids.size(), 3);
  EXPECT_EQ(ids[0], TestEnumId(1));
  EXPECT_EQ(ids[1], TestEnumId(2));
  EXPECT_EQ(ids[2], TestEnumId(3));
  EXPECT_THAT(ids, ElementsAre(TestEnumId(1), TestEnumId(2), TestEnumId(3)));
}
}  // namespace

}  // namespace operations_research::math_opt
