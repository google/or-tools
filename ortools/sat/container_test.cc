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

#include "ortools/sat/container.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {
namespace {

using testing::ElementsAre;
using testing::SizeIs;
using testing::UnorderedElementsAre;

TEST(LiteralsOrOffsetsTest, Basic) {
  LiteralsOrOffsets container;
  container.PushBackLiteral(Literal(1));
  container.InsertOffset(2);
  container.PushBackLiteral(Literal(3));
  container.InsertOffset(4);
  container.PushBackLiteral(Literal(5));

  EXPECT_EQ(container.num_literals(), 3);
  EXPECT_EQ(container.num_offsets(), 2);

  EXPECT_THAT(container.literals(),
              ElementsAre(Literal(1), Literal(3), Literal(5)));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(2, 4));
}

TEST(LiteralsOrOffsetsTest, Holes) {
  LiteralsOrOffsets container;

  // LLLOO
  container.PushBackLiteral(Literal(11));
  container.PushBackLiteral(Literal(12));
  container.PushBackLiteral(Literal(13));
  container.InsertOffset(21);
  container.InsertOffset(22);
  EXPECT_THAT(container.literals(),
              ElementsAre(Literal(11), Literal(12), Literal(13)));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(21, 22));

  // LLLLOO
  container.PushBackLiteral(Literal());
  EXPECT_THAT(container.literals(),
              ElementsAre(Literal(11), Literal(12), Literal(13), Literal()));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(21, 22));

  // LL..OO
  container.RemoveLiteralsIf(
      [](Literal l) { return l == Literal(11) || l == Literal(13); });
  EXPECT_THAT(container.literals(), ElementsAre(Literal(12), Literal()));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(21, 22));

  // LLL.OO
  container.PushBackLiteral(Literal());
  EXPECT_THAT(container.literals(),
              ElementsAre(Literal(12), Literal(), Literal()));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(21, 22));

  // LLL.
  container.ClearOffsets();
  EXPECT_THAT(container.literals(),
              ElementsAre(Literal(12), Literal(), Literal()));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre());

  // LLLL
  container.PushBackLiteral(Literal(14));
  EXPECT_THAT(container.literals(),
              ElementsAre(Literal(12), Literal(), Literal(), Literal(14)));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre());

  // LLLL0
  container.InsertOffset(23);
  EXPECT_THAT(container.literals(),
              ElementsAre(Literal(12), Literal(), Literal(), Literal(14)));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(23));
}

TEST(LiteralsOrOffsetsTest, Capacity) {
  LiteralsOrOffsets container;
  for (int i = 0; i < 10; ++i) container.PushBackLiteral(Literal());
  //  container.TruncateLiterals(10);
  container.InsertOffset(2);
  EXPECT_THAT(container.literals(), SizeIs(10));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(2));
  EXPECT_GE(container.capacity(), 11);

  container.ClearLiterals();
  EXPECT_THAT(container.literals(), SizeIs(0));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(2));
  EXPECT_GE(container.capacity(), 11);

  for (int i = 0; i < 9; ++i) container.PushBackLiteral(Literal());
  //  container.TruncateLiterals(9);
  EXPECT_THAT(container.literals(), SizeIs(9));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(2));
  EXPECT_GE(container.capacity(), 10);

  container.ClearLiterals();
  container.ShrinkToFit();
  EXPECT_THAT(container.literals(), SizeIs(0));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(2));
  EXPECT_THAT(container.capacity(), LiteralsOrOffsets::kInlineElements);
}

TEST(LiteralsOrOffsetsTest, SortLiteralsAndRemoveDuplicates) {
  LiteralsOrOffsets container;
  container.PushBackLiteral(Literal(1));
  container.PushBackLiteral(Literal(2));
  container.PushBackLiteral(Literal(3));
  container.PushBackLiteral(Literal(1));
  container.PushBackLiteral(Literal(2));
  container.PushBackLiteral(Literal(3));
  container.PushBackLiteral(Literal(4));
  container.PushBackLiteral(Literal(2));
  container.InsertOffset(15);
  container.InsertOffset(16);
  container.InsertOffset(17);
  container.SortLiteralsAndRemoveDuplicates();
  EXPECT_THAT(container.literals(),
              ElementsAre(Literal(1), Literal(2), Literal(3), Literal(4)));
  EXPECT_THAT(container.offsets(), UnorderedElementsAre(15, 16, 17));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
