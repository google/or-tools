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

#include "ortools/math_opt/elemental/diff.h"

#include <array>
#include <cstdint>

#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {
namespace {

using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;

std::array<int64_t, kNumElements> MakeUniformCheckpoint(int64_t id) {
  std::array<int64_t, kNumElements> result;
  result.fill(id);
  return result;
}

////////////////////////////////////////////////////////////////////////////////
// Element tests
////////////////////////////////////////////////////////////////////////////////

TEST(DiffTest, InitDiffElementsEmpty) {
  Diff diff;
  EXPECT_EQ(diff.checkpoint(ElementType::kVariable), 0);
  EXPECT_THAT(diff.deleted_elements(ElementType::kVariable), IsEmpty());
}

TEST(DiffTest, DeleteElementAfterCheckpointNoEffect) {
  Diff diff;
  diff.DeleteElement(ElementType::kVariable, 2);
  EXPECT_THAT(diff.deleted_elements(ElementType::kVariable), IsEmpty());
}

TEST(DiffTest, DeletesTrackedBelowCheckpoint) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(5));
  EXPECT_EQ(diff.checkpoint(ElementType::kVariable), 5);
  diff.DeleteElement(ElementType::kVariable, 3);
  diff.DeleteElement(ElementType::kVariable, 1);
  diff.DeleteElement(ElementType::kVariable, 8);
  diff.DeleteElement(ElementType::kVariable, 5);
  EXPECT_THAT(diff.deleted_elements(ElementType::kVariable),
              UnorderedElementsAre(3, 1));
}

TEST(DiffTest, AdvanceClearsDeletedElements) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(5));
  diff.DeleteElement(ElementType::kVariable, 3);
  EXPECT_THAT(diff.deleted_elements(ElementType::kVariable),
              UnorderedElementsAre(3));
  diff.Advance(MakeUniformCheckpoint(5));
  EXPECT_THAT(diff.deleted_elements(ElementType::kVariable), IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// Attr0 Tests
////////////////////////////////////////////////////////////////////////////////

TEST(DiffTest, InitBoolAttr0Empty) {
  Diff diff;
  EXPECT_THAT(diff.modified_keys(BoolAttr0::kMaximize), IsEmpty());
}

TEST(DiffTest, SetBoolAttr0ModifiedIsModified) {
  Diff diff;
  diff.SetModified(BoolAttr0::kMaximize, AttrKey());
  EXPECT_THAT(diff.modified_keys(BoolAttr0::kMaximize),
              UnorderedElementsAre(AttrKey()));
}

TEST(DiffTest, BoolAttr0AdvanceClearsModification) {
  Diff diff;
  diff.SetModified(BoolAttr0::kMaximize, AttrKey());
  diff.Advance(MakeUniformCheckpoint(0));
  EXPECT_THAT(diff.modified_keys(BoolAttr0::kMaximize), IsEmpty());
}

// Repeat all tests for double, they are short enough.

TEST(DiffTest, InitDoubleAttr0Empty) {
  Diff diff;
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset), IsEmpty());
}

TEST(DiffTest, SetDoubleAttr0ModifiedIsModified) {
  Diff diff;
  diff.SetModified(DoubleAttr0::kObjOffset, AttrKey());
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset),
              UnorderedElementsAre(AttrKey()));
}

TEST(DiffTest, DoubleAttr0AdvanceClearsModification) {
  Diff diff;
  diff.SetModified(DoubleAttr0::kObjOffset, AttrKey());
  diff.Advance(MakeUniformCheckpoint(0));
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset), IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// Attr1 Tests
////////////////////////////////////////////////////////////////////////////////

TEST(DiffTest, InitBoolAttr1Empty) {
  Diff diff;
  EXPECT_THAT(diff.modified_keys(BoolAttr1::kVarInteger), IsEmpty());
}

TEST(DiffTest, SetBoolAttr1ModifiedBeforeCheckpointIsModified) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(1));
  diff.SetModified(BoolAttr1::kVarInteger, AttrKey(0));
  EXPECT_THAT(diff.modified_keys(BoolAttr1::kVarInteger),
              UnorderedElementsAre(AttrKey(0)));
}

TEST(DiffTest, SetBoolAttr1ModifiedAtleastCheckpointNotTracked) {
  Diff diff;
  diff.SetModified(BoolAttr1::kVarInteger, AttrKey(0));
  EXPECT_THAT(diff.modified_keys(BoolAttr1::kVarInteger), IsEmpty());
}

TEST(DiffTest, BoolAttr1AdvanceClearsModification) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(1));
  diff.SetModified(BoolAttr1::kVarInteger, AttrKey(0));
  diff.Advance(MakeUniformCheckpoint(1));
  EXPECT_THAT(diff.modified_keys(BoolAttr1::kVarInteger), IsEmpty());
}

TEST(DiffTest, EraseElementForBoolAttr1IsNoLongerTracked) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(1));
  diff.SetModified(BoolAttr1::kVarInteger, AttrKey(0));

  EXPECT_THAT(diff.modified_keys(BoolAttr1::kVarInteger),
              UnorderedElementsAre(AttrKey(0)));

  diff.EraseKeysForAttr(BoolAttr1::kVarInteger, {AttrKey(0)});

  EXPECT_THAT(diff.modified_keys(BoolAttr1::kVarInteger), IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// Repeat all tests for DoubleAttr1, not ideal
////////////////////////////////////////////////////////////////////////////////

TEST(DiffTest, InitDoubleAttr1Empty) {
  Diff diff;
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kLinConUb), IsEmpty());
}

TEST(DiffTest, SetDoubleAttr1ModifiedBeforeCheckpointIsModified) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(1));
  diff.SetModified(DoubleAttr1::kLinConUb, AttrKey(0));
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kLinConUb),
              UnorderedElementsAre(AttrKey(0)));
}

TEST(DiffTest, SetDoubleAttr1ModifiedAtleastCheckpointNotTracked) {
  Diff diff;
  diff.SetModified(DoubleAttr1::kLinConUb, AttrKey(0));
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kLinConUb), IsEmpty());
}

TEST(DiffTest, DoubleAttr1AdvanceClearsModification) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(1));
  diff.SetModified(DoubleAttr1::kLinConUb, AttrKey(0));
  diff.Advance(MakeUniformCheckpoint(1));
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kLinConUb), IsEmpty());
}

TEST(DiffTest, EraseElementForDoubleAttr1IsNoLongerTracked) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(1));
  diff.SetModified(DoubleAttr1::kLinConUb, AttrKey(0));

  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kLinConUb),
              UnorderedElementsAre(AttrKey(0)));

  diff.EraseKeysForAttr(DoubleAttr1::kLinConUb, {AttrKey(0)});

  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kLinConUb), IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// Attr2 Tests
//
// UpdateAttr2OnFirstElementDeleted and UpdateAttr2OnSecondElementDeleted are a
// bit under tested.
////////////////////////////////////////////////////////////////////////////////

TEST(DiffTest, InitDoubleAttr2Empty) {
  Diff diff;
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(DiffTest, SetDoubleAttr2ModifiedBothKeysBeforeCheckpointIsModified) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(2));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(1, 0));
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(1, 0)));
}

TEST(DiffTest, SetDoubleAttr2ModifiedFirstKeyAtleastCheckpointNotTracked) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(2));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(4, 0));
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(DiffTest, SetDoubleAttr2ModifiedSecondtKeyAtleastCheckpointNotTracked) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(2));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(0, 4));
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(DiffTest, DoubleAttr2AdvanceClearsModification) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(1));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(0, 0));
  diff.Advance(MakeUniformCheckpoint(1));
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(DiffTest, EraseFirstElementForDoubleAttr2IsNoLongerTracked) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(5));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(1, 0));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(1, 2));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(1, 4));

  EXPECT_THAT(
      diff.modified_keys(DoubleAttr2::kLinConCoef),
      UnorderedElementsAre(AttrKey(1, 0), AttrKey(1, 2), AttrKey(1, 4)));

  diff.EraseKeysForAttr(
      DoubleAttr2::kLinConCoef,
      absl::Span<const AttrKey<2>>({AttrKey(1, 0), AttrKey(1, 4)}));

  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(1, 2)));
}

TEST(DiffTest, EraseSecondElementForDoubleAttr2IsNoLongerTracked) {
  Diff diff;
  diff.Advance(MakeUniformCheckpoint(5));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(0, 1));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(2, 1));
  diff.SetModified(DoubleAttr2::kLinConCoef, AttrKey(4, 1));

  EXPECT_THAT(
      diff.modified_keys(DoubleAttr2::kLinConCoef),
      UnorderedElementsAre(AttrKey(0, 1), AttrKey(2, 1), AttrKey(4, 1)));

  diff.EraseKeysForAttr(
      DoubleAttr2::kLinConCoef,
      absl::Span<const AttrKey<2>>({AttrKey(0, 1), AttrKey(4, 1)}));

  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(2, 1)));
}

}  // namespace
}  // namespace operations_research::math_opt
