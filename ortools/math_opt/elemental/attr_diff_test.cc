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

#include "ortools/math_opt/elemental/attr_diff.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/symmetry.h"

namespace operations_research::math_opt {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

////////////////////////////////////////////////////////////////////////////////
// AttrDiff<0>
////////////////////////////////////////////////////////////////////////////////

TEST(AttrDiff0Test, InitNotModified) {
  AttrDiff<0, NoSymmetry> diff;
  EXPECT_THAT(diff.modified_keys(), IsEmpty());
}

TEST(AttrDiff0Test, SetModified) {
  AttrDiff<0, NoSymmetry> diff;
  diff.SetModified(AttrKey());
  EXPECT_THAT(diff.modified_keys(), UnorderedElementsAre(AttrKey()));
}

TEST(AttrDiff0Test, Advance) {
  AttrDiff<0, NoSymmetry> diff;
  diff.SetModified(AttrKey());
  diff.Advance();
  EXPECT_THAT(diff.modified_keys(), IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// Attr1Diff
////////////////////////////////////////////////////////////////////////////////

TEST(AttrDiff1Test, InitNotModified) {
  AttrDiff<1, NoSymmetry> diff;
  EXPECT_THAT(diff.modified_keys(), IsEmpty());
}

TEST(AttrDiff1Test, SetModified) {
  AttrDiff<1, NoSymmetry> diff;
  diff.SetModified(AttrKey(2));
  diff.SetModified(AttrKey(5));
  diff.SetModified(AttrKey(6));
  EXPECT_THAT(diff.modified_keys(),
              UnorderedElementsAre(AttrKey(2), AttrKey(5), AttrKey(6)));
}

TEST(AttrDiff1Test, Advance) {
  AttrDiff<1, NoSymmetry> diff;
  diff.SetModified(AttrKey(2));
  diff.SetModified(AttrKey(5));

  diff.Advance();
  EXPECT_THAT(diff.modified_keys(), IsEmpty());
}

TEST(AttrDiff1Test, EraseIsModifiedGetsRemoved) {
  AttrDiff<1, NoSymmetry> diff;
  diff.SetModified(AttrKey(2));
  diff.SetModified(AttrKey(5));
  diff.SetModified(AttrKey(6));

  diff.Erase(AttrKey(5));
  EXPECT_THAT(diff.modified_keys(),
              UnorderedElementsAre(AttrKey(2), AttrKey(6)));
}

TEST(AttrDiff1Test, EraseNotModifiedNoEffect) {
  AttrDiff<1, NoSymmetry> diff;
  diff.SetModified(AttrKey(2));
  diff.SetModified(AttrKey(5));

  diff.Erase(AttrKey(1));
  EXPECT_THAT(diff.modified_keys(),
              UnorderedElementsAre(AttrKey(2), AttrKey(5)));
}

////////////////////////////////////////////////////////////////////////////////
// Attr2Diff
////////////////////////////////////////////////////////////////////////////////

TEST(AttrDiffTest2, InitNotModified) {
  AttrDiff<2, NoSymmetry> diff;
  EXPECT_THAT(diff.modified_keys(), IsEmpty());
}

TEST(AttrDiffTest2, SetModified) {
  AttrDiff<2, NoSymmetry> diff;
  diff.SetModified(AttrKey(2, 4));
  diff.SetModified(AttrKey(5, 2));
  diff.SetModified(AttrKey(2, 5));
  diff.SetModified(AttrKey(6, 6));
  EXPECT_THAT(diff.modified_keys(),
              UnorderedElementsAre(AttrKey(2, 4), AttrKey(5, 2), AttrKey(2, 5),
                                   AttrKey(6, 6)));
}

TEST(AttrDiffTest2, Advance) {
  AttrDiff<2, NoSymmetry> diff;
  diff.SetModified(AttrKey(2, 3));
  diff.SetModified(AttrKey(2, 8));

  diff.Advance();
  EXPECT_THAT(diff.modified_keys(), IsEmpty());
}

TEST(AttrDiffTest2, EraseIsModifiedGetsRemoved) {
  AttrDiff<2, NoSymmetry> diff;
  diff.SetModified(AttrKey(2, 5));
  diff.SetModified(AttrKey(4, 3));
  diff.SetModified(AttrKey(3, 4));
  diff.SetModified(AttrKey(6, 6));

  EXPECT_THAT(diff.modified_keys(),
              UnorderedElementsAre(AttrKey(2, 5), AttrKey(3, 4), AttrKey(4, 3),
                                   AttrKey(6, 6)));

  diff.Erase(AttrKey(4, 3));
  EXPECT_THAT(
      diff.modified_keys(),
      UnorderedElementsAre(AttrKey(2, 5), AttrKey(3, 4), AttrKey(6, 6)));
}

TEST(AttrDiffTest2, EraseIsModifiedGetsRemovedSymmetric) {
  using Diff = AttrDiff<2, ElementSymmetry<0, 1>>;
  using Key = Diff::Key;
  Diff diff;
  diff.SetModified(Key(2, 5));
  diff.SetModified(Key(4, 3));
  diff.SetModified(Key(3, 4));  // Noop, same as (4,3).
  diff.SetModified(Key(6, 6));

  EXPECT_THAT(diff.modified_keys(),
              UnorderedElementsAre(Key(2, 5), Key(3, 4), Key(6, 6)));

  diff.Erase(Key(4, 3));
  EXPECT_THAT(diff.modified_keys(), UnorderedElementsAre(Key(2, 5), Key(6, 6)));
}

TEST(AttrDiffTest2, EraseNotModifiedNoEffect) {
  AttrDiff<2, NoSymmetry> diff;
  diff.SetModified(AttrKey(2, 5));
  diff.SetModified(AttrKey(6, 6));

  diff.Erase(AttrKey(1, 3));
  EXPECT_THAT(diff.modified_keys(),
              UnorderedElementsAre(AttrKey(2, 5), AttrKey(6, 6)));
}

}  // namespace operations_research::math_opt
