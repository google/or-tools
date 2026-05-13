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

#include "ortools/math_opt/elemental/elemental.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/status.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/derived_data.h"
#include "ortools/math_opt/elemental/diff.h"
#include "ortools/math_opt/elemental/elemental_matcher.h"
#include "ortools/math_opt/elemental/elements.h"
#include "ortools/math_opt/elemental/symmetry.h"
#include "ortools/math_opt/elemental/testing.h"

namespace operations_research::math_opt {

class ElementalTestPeer {
 public:
  static const Diff& GetDiffRef(Elemental::DiffHandle diff) {
    Diff* result = diff.diffs_.Get(diff.id());
    CHECK(result != nullptr);
    return *result;
  }
};

namespace {

using ::testing::AnyOf;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;
using DiePolicy = Elemental::DiePolicy;

constexpr double kInf = std::numeric_limits<double>::infinity();

////////////////////////////////////////////////////////////////////////////////
// Name tests
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, NoNames) {
  Elemental elemental;
  EXPECT_EQ(elemental.model_name(), "");
  EXPECT_EQ(elemental.primary_objective_name(), "");
}

TEST(ElementalTest, HaveNames) {
  Elemental elemental("my_model", "my_obj");
  EXPECT_EQ(elemental.model_name(), "my_model");
  EXPECT_EQ(elemental.primary_objective_name(), "my_obj");
}

////////////////////////////////////////////////////////////////////////////////
// Elements
////////////////////////////////////////////////////////////////////////////////

// Typed tests do not work with non-type template parameters, so we need to
// wrap the values with a type. See
// 8947385822189977600
template <ElementType e>
class ElementTypeProxy {
 public:
  static constexpr ElementType kType = e;
};

template <typename Proxy>
class ElementTest : public testing::Test {
 public:
  constexpr static ElementType kElementType = Proxy::kType;
};

// Returns a `std::tuple<ElementTypeProxy<kElements[0]>, ...,
// ElementTypeProxy<kElements[kElements.size() - 1]>>`
template <int... I>
constexpr auto MakeProxyTuple(std::integer_sequence<int, I...>) {
  return std::make_tuple(ElementTypeProxy<kElements[I]>{}...);
}

// After C++20, this can be replaced by a lambda. C++17 does not allow lambdas
// in unevaluated contexts.
struct AllElementTypesCpp17Helper {
  template <typename... Args>
  auto operator()(Args...) const {
    return ::testing::Types<Args...>{};
  }
};

// A ::testing::Types<ElementTypeProxy<ElementType::kVariable>,
//                    ElementTypeProxy<ElementType::kLinearConstraint>,
//                    ...>;
// with a value for each ElementType in kElements.
using AllElementTypes = decltype(std::apply(
    AllElementTypesCpp17Helper{},
    MakeProxyTuple(std::make_integer_sequence<int, kElements.size()>())));

TYPED_TEST_SUITE(ElementTest, AllElementTypes);

TYPED_TEST(ElementTest, EmptyElements) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  EXPECT_FALSE(elemental.ElementExists(ElementId<kElementType>(0)));
  EXPECT_FALSE(elemental.ElementExistsUntyped(kElementType, 0));
  EXPECT_EQ(elemental.NumElements(kElementType), 0);
  EXPECT_EQ(elemental.NextElementId(kElementType), 0);
  EXPECT_THAT(elemental.AllElements<kElementType>(), IsEmpty());
  EXPECT_THAT(elemental.AllElementsUntyped(kElementType), IsEmpty());
}

TYPED_TEST(ElementTest, AddElements) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  using Id = ElementId<kElementType>;
  const Id x = elemental.AddElement<kElementType>("x");
  const int64_t y = elemental.AddElementUntyped(kElementType, "y");
  EXPECT_EQ(x, Id(0));
  EXPECT_EQ(y, 1);

  EXPECT_TRUE(elemental.ElementExists(x));
  EXPECT_FALSE(elemental.ElementExists(ElementId<kElementType>(2)));
  EXPECT_EQ(elemental.NumElements(kElementType), 2);
  EXPECT_EQ(elemental.NextElementId(kElementType), 2);
  EXPECT_THAT(elemental.GetElementNameUntyped(kElementType, y),
              IsOkAndHolds("y"));
  EXPECT_THAT(elemental.AllElements<kElementType>(),
              UnorderedElementsAre(x, Id(y)));
}

TYPED_TEST(ElementTest, EnsureNextElementIdAtLeastLargeIdHasEffect) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  elemental.EnsureNextElementIdAtLeastUntyped(kElementType, 4);
  const auto x = elemental.AddElement<kElementType>("x");
  EXPECT_EQ(x, ElementId<kElementType>(4));
  EXPECT_THAT(elemental.AllElements<kElementType>(), UnorderedElementsAre(x));
}

TYPED_TEST(ElementTest, EnsureNextElementIdAtLeastSmallIdNoEffect) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  const auto x = elemental.AddElement<kElementType>("x");

  elemental.EnsureNextElementIdAtLeast(x);

  const auto y = elemental.AddElement<kElementType>("y");
  EXPECT_EQ(y, ElementId<kElementType>(1));
  EXPECT_THAT(elemental.AllElements<kElementType>(),
              UnorderedElementsAre(x, y));
}

TYPED_TEST(ElementTest, DeleteElement) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  const auto x = elemental.AddElement<kElementType>("x");
  const auto y = elemental.AddElement<kElementType>("y");

  EXPECT_TRUE(elemental.DeleteElement(y));

  EXPECT_THAT(elemental.AllElements<kElementType>(), UnorderedElementsAre(x));
  EXPECT_EQ(elemental.NumElements(kElementType), 1);
  EXPECT_EQ(elemental.NextElementId(kElementType), 2);
}

TYPED_TEST(ElementTest, DeleteElementTwiceNoEffect) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;

  const auto x = elemental.AddElement<kElementType>("x");
  const auto y = elemental.AddElement<kElementType>("y");
  EXPECT_TRUE(elemental.DeleteElement(y));

  EXPECT_FALSE(elemental.DeleteElement(y));

  EXPECT_EQ(elemental.NextElementId(kElementType), 2);
  EXPECT_THAT(elemental.AllElements<kElementType>(), UnorderedElementsAre(x));
  EXPECT_EQ(elemental.NumElements(kElementType), 1);
}

TYPED_TEST(ElementTest, DeleteElementNotInModelNoEffect) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  const auto x = elemental.AddElement<kElementType>("x");

  EXPECT_FALSE(elemental.DeleteElement(ElementId<kElementType>(3)));

  EXPECT_EQ(elemental.NextElementId(kElementType), 1);
  EXPECT_THAT(elemental.AllElements<kElementType>(), UnorderedElementsAre(x));
  EXPECT_EQ(elemental.NumElements(kElementType), 1);
}

////////////////////////////////////////////////////////////////////////////////
// Element Diff tests.
////////////////////////////////////////////////////////////////////////////////

TYPED_TEST(ElementTest, DiffOnEmptyModel) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  EXPECT_EQ(diff.checkpoint(kElementType), 0);
  EXPECT_THAT(diff.deleted_elements(kElementType), IsEmpty());
}

TYPED_TEST(ElementTest, DiffAddElementAfterCheckpointNoEffect) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.AddElement<kElementType>("x");
  EXPECT_EQ(diff.checkpoint(kElementType), 0);
  EXPECT_THAT(diff.deleted_elements(kElementType), IsEmpty());
}

TYPED_TEST(ElementTest, DiffAdvanceTracksNewlyAddedElements) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  const Elemental::DiffHandle diff_handle = elemental.AddDiff();
  const Diff& diff = ElementalTestPeer::GetDiffRef(diff_handle);
  const auto x = elemental.AddElement<kElementType>("x");

  elemental.Advance(diff_handle);
  EXPECT_EQ(diff.checkpoint(kElementType), 1);

  elemental.DeleteElement(x);
  EXPECT_THAT(diff.deleted_elements(kElementType),
              UnorderedElementsAre(x.value()));
}

TYPED_TEST(ElementTest, DiffDeleteElementIsTracked) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  elemental.AddElement<kElementType>("x");
  const auto y = elemental.AddElement<kElementType>("y");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  EXPECT_EQ(diff.checkpoint(kElementType), 2);
  EXPECT_THAT(diff.deleted_elements(kElementType), IsEmpty());
  elemental.DeleteElement(y);
  EXPECT_EQ(diff.checkpoint(kElementType), 2);
  EXPECT_THAT(diff.deleted_elements(kElementType),
              UnorderedElementsAre(y.value()));
}

TYPED_TEST(ElementTest, DiffDuplicateDeleteElementIsNotTracked) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;
  elemental.AddElement<kElementType>("x");
  const auto y = elemental.AddElement<kElementType>("y");
  elemental.DeleteElement(y);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());

  // second delete for y, should not be tracked.
  elemental.DeleteElement(y);

  EXPECT_THAT(diff.deleted_elements(kElementType), IsEmpty());
}

TYPED_TEST(ElementTest, DeleteDiffSuccess) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental;

  elemental.AddElement<kElementType>("x");
  const auto y = elemental.AddElement<kElementType>("y");

  const Elemental::DiffHandle diff1_handle = elemental.AddDiff();
  const Elemental::DiffHandle diff2_handle = elemental.AddDiff();

  EXPECT_TRUE(elemental.DeleteDiff(diff1_handle));
  elemental.DeleteElement(y);

  EXPECT_THAT(ElementalTestPeer::GetDiffRef(diff2_handle)
                  .deleted_elements(kElementType),
              UnorderedElementsAre(y.value()));
}

TYPED_TEST(ElementTest, DeleteDiffWrongModelNoEffect) {
  constexpr ElementType kElementType = TestFixture::kElementType;
  Elemental elemental1;

  Elemental elemental2;

  elemental1.AddElement<kElementType>("x");
  const auto y = elemental1.AddElement<kElementType>("y");

  const Elemental::DiffHandle diff1_handle = elemental1.AddDiff();
  const Elemental::DiffHandle diff2_handle = elemental2.AddDiff();

  ASSERT_FALSE(elemental1.DeleteDiff(diff2_handle));
  elemental1.DeleteElement(y);

  EXPECT_THAT(ElementalTestPeer::GetDiffRef(diff1_handle)
                  .deleted_elements(kElementType),
              UnorderedElementsAre(y.value()));
  EXPECT_THAT(ElementalTestPeer::GetDiffRef(diff2_handle)
                  .deleted_elements(kElementType),
              IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// Attr0
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, Attr0GetSet) {
  Elemental elemental;
  EXPECT_EQ(elemental.GetAttr(DoubleAttr0::kObjOffset, AttrKey()), 0.0);
  EXPECT_FALSE(elemental.AttrIsNonDefault(DoubleAttr0::kObjOffset, AttrKey()));

  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 3.4);

  EXPECT_EQ(elemental.GetAttr(DoubleAttr0::kObjOffset, AttrKey()), 3.4);
  EXPECT_TRUE(elemental.AttrIsNonDefault(DoubleAttr0::kObjOffset, AttrKey()));
}

TEST(ElementalTest, Attr0Clear) {
  Elemental elemental;
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 3.4);

  elemental.AttrClear(DoubleAttr0::kObjOffset);

  EXPECT_EQ(elemental.GetAttr(DoubleAttr0::kObjOffset, AttrKey()), 0.0);
  EXPECT_FALSE(elemental.AttrIsNonDefault(DoubleAttr0::kObjOffset, AttrKey()));
}

////////////////////////////////////////////////////////////////////////////////
// Attr1
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, Attr1GetSet) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  EXPECT_EQ(elemental.GetAttr(DoubleAttr1::kVarUb, AttrKey(x)), kInf);
  EXPECT_FALSE(elemental.AttrIsNonDefault(DoubleAttr1::kVarUb, AttrKey(x)));
  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr1::kVarUb), IsEmpty());
  EXPECT_EQ(elemental.AttrNumNonDefaults(DoubleAttr1::kVarUb), 0);
  EXPECT_THAT((elemental.Slice<0>(DoubleAttr1::kVarUb, x.value())), IsEmpty());

  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 3.4);

  EXPECT_EQ(elemental.GetAttr(DoubleAttr1::kVarUb, AttrKey(x)), 3.4);
  EXPECT_TRUE(elemental.AttrIsNonDefault(DoubleAttr1::kVarUb, AttrKey(x)));
  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr1::kVarUb),
              UnorderedElementsAre(AttrKey(0)));
  EXPECT_EQ(elemental.AttrNumNonDefaults(DoubleAttr1::kVarUb), 1);
  EXPECT_THAT((elemental.Slice<0>(DoubleAttr1::kVarUb, x.value())),
              UnorderedElementsAre(AttrKey(x)));
}

TEST(ElementalTest, Attr1Clear) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 3.4);

  elemental.AttrClear(DoubleAttr1::kVarUb);

  EXPECT_EQ(elemental.GetAttr(DoubleAttr1::kVarUb, AttrKey(x)), kInf);
  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr1::kVarUb), IsEmpty());
}

TEST(ElementalTest, Attr1RespondsToElementDeletion) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 3.4);

  elemental.DeleteElement(x);

  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr1::kVarUb), IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// Attr2
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, Attr2GetSet) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");

  EXPECT_EQ(elemental.GetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x)), 0.0);
  EXPECT_FALSE(
      elemental.AttrIsNonDefault(DoubleAttr2::kLinConCoef, AttrKey(c, x)));
  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr2::kLinConCoef), IsEmpty());
  EXPECT_EQ(elemental.AttrNumNonDefaults(DoubleAttr2::kLinConCoef), 0);

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.4);

  EXPECT_EQ(elemental.GetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x)), 3.4);
  EXPECT_TRUE(
      elemental.AttrIsNonDefault(DoubleAttr2::kLinConCoef, AttrKey(c, x)));
  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
  EXPECT_EQ(elemental.AttrNumNonDefaults(DoubleAttr2::kLinConCoef), 1);
}

TEST(ElementalTest, Attr2Clear) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.4);

  elemental.AttrClear(DoubleAttr2::kLinConCoef);

  EXPECT_EQ(elemental.GetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x)), 0.0);
  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, Attr2RespondsToElementDeletionKey0) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.4);

  elemental.DeleteElement(c);

  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, Attr2RespondsToElementDeletionKey1) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 3.4);

  elemental.DeleteElement(x);

  EXPECT_THAT(elemental.AttrNonDefaults(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, SymmetricAttr2) {
  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  Elemental elemental;
  const VariableId x0 = elemental.AddElement<ElementType::kVariable>("x1");
  const VariableId x1 = elemental.AddElement<ElementType::kVariable>("x2");
  const VariableId x2 = elemental.AddElement<ElementType::kVariable>("x3");

  const auto q01 = Key(x0, x1);
  const auto q21 = Key(x2, x1);
  const auto q12 = Key(x1, x2);

  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, q01, 42.0);
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, q21, 43.0);
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, q12, 44.0);

  EXPECT_EQ(elemental.AttrNumNonDefaults(SymmetricDoubleAttr2::kObjQuadCoef),
            2);

  EXPECT_THAT(elemental.AttrNonDefaults(SymmetricDoubleAttr2::kObjQuadCoef),
              UnorderedElementsAre(q01, q12));
}

TEST(ElementalTest, SymmetricAttr2RespondsToElementDeletionKey0) {
  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, x), 1.0);
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, y), 2.0);
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(y, y), 3.0);

  elemental.DeleteElement(x);

  EXPECT_THAT(elemental.AttrNonDefaults(SymmetricDoubleAttr2::kObjQuadCoef),
              UnorderedElementsAre(Key(y, y)));
}

TEST(ElementalTest, SymmetricAttr2RespondsToElementDeletionKey1) {
  using Key = AttrKeyFor<SymmetricDoubleAttr2>;
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, x), 1.0);
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(x, y), 2.0);
  elemental.SetAttr(SymmetricDoubleAttr2::kObjQuadCoef, Key(y, y), 3.0);

  elemental.DeleteElement(y);

  EXPECT_THAT(elemental.AttrNonDefaults(SymmetricDoubleAttr2::kObjQuadCoef),
              UnorderedElementsAre(Key(x, x)));
}

TEST(ElementalTest, Attr2Slice) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused1");
  elemental.AddElement<ElementType::kVariable>("unused2");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("y");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const LinearConstraintId d =
      elemental.AddElement<ElementType::kLinearConstraint>("d");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, y), 2.0);
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(d, y), 3.0);

  EXPECT_THAT((elemental.Slice<0>(DoubleAttr2::kLinConCoef, c.value())),
              UnorderedElementsAre(AttrKey(c, x), AttrKey(c, y)));
  EXPECT_THAT((elemental.Slice<0>(DoubleAttr2::kLinConCoef, d.value())),
              UnorderedElementsAre(AttrKey(d, y)));
  EXPECT_THAT((elemental.Slice<1>(DoubleAttr2::kLinConCoef, x.value())),
              UnorderedElementsAre(AttrKey(c, x)));
  EXPECT_THAT((elemental.Slice<1>(DoubleAttr2::kLinConCoef, y.value())),
              UnorderedElementsAre(AttrKey(c, y), AttrKey(d, y)));
}

////////////////////////////////////////////////////////////////////////////////
// Element-valued attributes.
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, ElementValuedAttr) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("");
  const VariableId y = elemental.AddElement<ElementType::kVariable>("");
  const IndicatorConstraintId ic1 =
      elemental.AddElement<ElementType::kIndicatorConstraint>("");
  const IndicatorConstraintId ic2 =
      elemental.AddElement<ElementType::kIndicatorConstraint>("");
  const IndicatorConstraintId ic3 =
      elemental.AddElement<ElementType::kIndicatorConstraint>("");

  {
    const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
    elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(ic1), x);
    elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(ic2), x);
    elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(ic2), y);
    elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(ic3), x);
    EXPECT_THAT(diff.modified_keys(VariableAttr1::kIndConIndicator),
                UnorderedElementsAre(AttrKey(ic1), AttrKey(ic2), AttrKey(ic3)));
  }

  {
    const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
    // Deleting `x` clears the attribute for `ic1` and `ic3`, which both
    // reference `x`.
    elemental.DeleteElement(x);
    EXPECT_THAT(elemental.AttrNonDefaults(VariableAttr1::kIndConIndicator),
                UnorderedElementsAre(AttrKey(ic2)));
    // It also informs the diffs that the attributes referencing `x` were
    // modified.
    EXPECT_THAT(diff.modified_keys(VariableAttr1::kIndConIndicator),
                UnorderedElementsAre(AttrKey(ic1), AttrKey(ic3)));
  }
}

TEST(ElementalTest, ElementValuedAttrClear) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("");
  const IndicatorConstraintId ic =
      elemental.AddElement<ElementType::kIndicatorConstraint>("");

  elemental.SetAttr(VariableAttr1::kIndConIndicator, AttrKey(ic), x);

  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  EXPECT_THAT(diff.modified_keys(VariableAttr1::kIndConIndicator), IsEmpty());

  elemental.AttrClear(VariableAttr1::kIndConIndicator);
  EXPECT_THAT(diff.modified_keys(VariableAttr1::kIndConIndicator),
              UnorderedElementsAre(AttrKey(ic)));

  // Deleting `x` does not clear the attribute for `ic`, since that attribute no
  // longer exists.
  elemental.DeleteElement(x);
  EXPECT_THAT(diff.modified_keys(VariableAttr1::kIndConIndicator),
              UnorderedElementsAre(AttrKey(ic)));
}

////////////////////////////////////////////////////////////////////////////////
// Diff Attr Tests.
//
// For each of Attr0, Attr1, Attr2, we need to test five cases for modifications
// before the checkpoint:
//  1. Default -> Non-Default
//  2. Default -> Default
//  3. Non-Default -> Default
//  4. Non-Default -> Same Non-Default
//  5. Non-Default -> Different Non-Default
//
// We also must test that calling Advance() clears the modified set.
//
// For attr1 and attr2, we must also check that:
//   * Modifications where at least one key is after the checkpoint are not
//     saved.
//   * Modifications on elements that are deleted are removed:
//     - when the attribute was in a non-default state for this element at
//       deletion time.
//     - when the attribute was in a non-default state for this element at
//       deletion time (we do not get 100% success here for attr2).
//
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, ModifiedKeysThatExistAttr0) {
  Elemental elemental;
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  EXPECT_THAT(elemental.ModifiedKeysThatExist(DoubleAttr0::kObjOffset, diff),
              IsEmpty());
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 4.0);
  EXPECT_THAT(elemental.ModifiedKeysThatExist(DoubleAttr0::kObjOffset, diff),
              UnorderedElementsAre(AttrKey()));
}

TEST(ElementalTest, ModifiedKeysThatExistAttr1) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  EXPECT_THAT(elemental.ModifiedKeysThatExist(DoubleAttr1::kVarLb, diff),
              IsEmpty());
  elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(x), 4.0);
  EXPECT_THAT(elemental.ModifiedKeysThatExist(DoubleAttr1::kVarLb, diff),
              UnorderedElementsAre(AttrKey(x)));
  elemental.DeleteElement(x);
  EXPECT_THAT(elemental.ModifiedKeysThatExist(DoubleAttr1::kVarLb, diff),
              IsEmpty());
}

TEST(ElementalTest, ModifiedKeysThatExistAttr2) {
  Elemental elemental;
  // Ensure the values of x and c are different.
  elemental.AddElement<ElementType::kVariable>("");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  EXPECT_THAT(elemental.ModifiedKeysThatExist(DoubleAttr2::kLinConCoef, diff),
              IsEmpty());
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 4.0);
  EXPECT_THAT(elemental.ModifiedKeysThatExist(DoubleAttr2::kLinConCoef, diff),
              UnorderedElementsAre(AttrKey(c, x)));
  // This is the hard case, if we set the value to zero before deleting x, then
  // we will fail to delete it from the diff.
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 0.0);
  elemental.DeleteElement(x);

  // Here, we see that we failed to delete it.
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));

  EXPECT_THAT(elemental.ModifiedKeysThatExist(DoubleAttr2::kLinConCoef, diff),
              IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// DiffAttr0 Test
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, DiffAttr0DefaultToDefaultNotModified) {
  Elemental elemental;
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 0.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset), IsEmpty());
}

TEST(ElementalTest, DiffAttr0DefaultToNonDefaultModified) {
  Elemental elemental;
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset),
              UnorderedElementsAre(AttrKey()));
}

TEST(ElementalTest, DiffAttr0NonDefaultToNonDefaultNotModified) {
  Elemental elemental;
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset), IsEmpty());
}

TEST(ElementalTest, DiffAttr0NonDefaultToNonDefaultModified) {
  Elemental elemental;
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 2.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset),
              UnorderedElementsAre(AttrKey()));
}

TEST(ElementalTest, DiffAttr0NonDefaultToDefaultModified) {
  Elemental elemental;
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 0.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset),
              UnorderedElementsAre(AttrKey()));
}

TEST(ElementalTest, DiffAttr0AdvanceClearsModified) {
  Elemental elemental;
  const Elemental::DiffHandle diff_handle = elemental.AddDiff();
  const Diff& diff = ElementalTestPeer::GetDiffRef(diff_handle);
  elemental.SetAttr(DoubleAttr0::kObjOffset, AttrKey(), 1.0);

  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset),
              UnorderedElementsAre(AttrKey()));
  elemental.Advance(diff_handle);
  EXPECT_THAT(diff.modified_keys(DoubleAttr0::kObjOffset), IsEmpty());
}

////////////////////////////////////////////////////////////////////////////////
// DiffAttr1 Test
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, DiffAttr1DefaultToDefaultNotModified) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), kInf);
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb), IsEmpty());
}

TEST(ElementalTest, DiffAttr1DefaultToNonDefaultModified) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb),
              UnorderedElementsAre(AttrKey(x)));
}

TEST(ElementalTest, DiffAttr1NonDefaultToNonDefaultNotModified) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb), IsEmpty());
}

TEST(ElementalTest, DiffAttr1NonDefaultToNonDefaultModified) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 2.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb),
              UnorderedElementsAre(AttrKey(x)));
}

TEST(ElementalTest, DiffAttr1NonDefaultToDefaultModified) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), kInf);
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb),
              UnorderedElementsAre(AttrKey(x)));
}

TEST(ElementalTest, DiffAttr1AdvanceClearsModified) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Elemental::DiffHandle diff_handle = elemental.AddDiff();
  const Diff& diff = ElementalTestPeer::GetDiffRef(diff_handle);
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);

  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb),
              UnorderedElementsAre(AttrKey(x)));
  elemental.Advance(diff_handle);
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb), IsEmpty());
}

TEST(ElementalTest, DiffAttr1ModificationAfterCheckpointNotSaved) {
  Elemental elemental;
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb), IsEmpty());
}

TEST(ElementalTest, DiffAttr1DeleteModifiedAttributeAtNonDefault) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);

  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb),
              UnorderedElementsAre(AttrKey(x)));
  EXPECT_TRUE(elemental.DeleteElement(x));
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb), IsEmpty());
}

TEST(ElementalTest, DiffAttr1DeleteModifiedAttributeAtDefault) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), kInf);

  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb),
              UnorderedElementsAre(AttrKey(x)));
  EXPECT_TRUE(elemental.DeleteElement(x));
  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb), IsEmpty());
}

TEST(ElementalTest, DiffAttr1ClearModifies) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.SetAttr(DoubleAttr1::kVarUb, AttrKey(x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());

  elemental.AttrClear(DoubleAttr1::kVarUb);

  EXPECT_THAT(diff.modified_keys(DoubleAttr1::kVarUb),
              UnorderedElementsAre(AttrKey(x)));
}

////////////////////////////////////////////////////////////////////////////////
// DiffAttr2 Test
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, DiffAttr2DefaultToDefaultNotModified) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 0.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, DiffAttr2DefaultToNonDefaultModified) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
}

TEST(ElementalTest, DiffAttr2NonDefaultToNonDefaultNotModified) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, DiffAttr2NonDefaultToNonDefaultModified) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 2.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
}

TEST(ElementalTest, DiffAttr2NonDefaultToDefaultModified) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 0.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
}

TEST(ElementalTest, DiffAttr2AdvanceClearsModified) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Elemental::DiffHandle diff_handle = elemental.AddDiff();
  const Diff& diff = ElementalTestPeer::GetDiffRef(diff_handle);
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);

  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
  elemental.Advance(diff_handle);
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, DiffAttr2ModificationKey1AfterCheckpointNotSaved) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, DiffAttr2ModificationKey2AfterCheckpointNotSaved) {
  Elemental elemental;
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");

  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, DiffAttr2DeleteFirstKeyModifiedAttributeAtNonDefault) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);

  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
  EXPECT_TRUE(elemental.DeleteElement(c));
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, DiffAttr2DeleteSecondKeyModifiedAttributeAtNonDefault) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);

  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
  EXPECT_TRUE(elemental.DeleteElement(x));
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef), IsEmpty());
}

TEST(ElementalTest, DiffAttr2DeleteKey1ModifiedAttributeAtDefault) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 0.0);

  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
  EXPECT_TRUE(elemental.DeleteElement(c));
  // This is the hard case, the current implementation fails to delete (c, x).
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              AnyOf(IsEmpty(), UnorderedElementsAre(AttrKey(c, x))));
}

TEST(ElementalTest, DiffAttr2DeleteKey2ModifiedAttributeAtDefault) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 0.0);

  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
  EXPECT_TRUE(elemental.DeleteElement(x));
  // This is the hard case, the current implementation fails to delete (c, x).
  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              AnyOf(IsEmpty(), UnorderedElementsAre(AttrKey(c, x))));
}

TEST(ElementalTest, DiffAttr2ClearModifies) {
  Elemental elemental;
  elemental.AddElement<ElementType::kVariable>("unused");
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  const LinearConstraintId c =
      elemental.AddElement<ElementType::kLinearConstraint>("c");
  elemental.SetAttr(DoubleAttr2::kLinConCoef, AttrKey(c, x), 1.0);
  const Diff& diff = ElementalTestPeer::GetDiffRef(elemental.AddDiff());

  elemental.AttrClear(DoubleAttr2::kLinConCoef);

  EXPECT_THAT(diff.modified_keys(DoubleAttr2::kLinConCoef),
              UnorderedElementsAre(AttrKey(c, x)));
}

////////////////////////////////////////////////////////////////////////////////
// Policy Tests
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, StatusPolicy) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  EXPECT_OK(elemental.GetAttr<Elemental::StatusPolicy>(DoubleAttr1::kVarLb,
                                                       AttrKey(x)));

  EXPECT_THAT(elemental.GetAttr<Elemental::StatusPolicy>(DoubleAttr1::kVarLb,
                                                         AttrKey(-1)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "no element with id -1 for element type variable"));
  EXPECT_THAT(
      (elemental.Slice<0, Elemental::StatusPolicy>(DoubleAttr1::kVarLb, 4)),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("no element with id 4")));
  EXPECT_THAT((elemental.GetSliceSize<0, Elemental::StatusPolicy>(
                  DoubleAttr1::kVarLb, 4)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("no element with id 4")));
}

TEST(ElementalTest, UBPolicy) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.GetAttr<Elemental::UBPolicy>(DoubleAttr1::kVarLb, AttrKey(x));
  EXPECT_THAT(
      (elemental.Slice<0, Elemental::UBPolicy>(DoubleAttr1::kVarLb, x.value())),
      IsEmpty());
  EXPECT_EQ((elemental.GetSliceSize<0, Elemental::UBPolicy>(DoubleAttr1::kVarLb,
                                                            x.value())),
            0);

  // We cannot test the error path as it's UB.
}

TEST(ElementalDeathTest, DiePolicy) {
  Elemental elemental;
  const VariableId x = elemental.AddElement<ElementType::kVariable>("x");
  elemental.GetAttr(DoubleAttr1::kVarLb, AttrKey(x));

  EXPECT_DEATH(elemental.GetAttr(DoubleAttr1::kVarLb, AttrKey(-1)),
               "no element with id -1 for element type variable");
  EXPECT_DEATH(
      (elemental.Slice<0, Elemental::DiePolicy>(DoubleAttr1::kVarLb, 4)),
      "no element with id 4");
  EXPECT_DEATH(
      (elemental.GetSliceSize<0, Elemental::DiePolicy>(DoubleAttr1::kVarLb, 4)),
      "no element with id 4");
}

template <int dimension, typename Policy = Elemental::DiePolicy,
          typename Symmetry = NoSymmetry>
void BM_RandomGet(benchmark::State& state) {
  const int n = state.range(0);
  Elemental elemental;
  // Create a model with n variables and n constraints, attributes on all
  // variables and all (variable x constraint).
  std::vector<VariableId> vars;
  std::vector<LinearConstraintId> constraints;
  for (int i = 0; i < n; ++i) {
    vars.push_back(elemental.AddElement<ElementType::kVariable>(""));
    constraints.push_back(
        elemental.AddElement<ElementType::kLinearConstraint>(""));
  }
  elemental.SetAttr(BoolAttr0::kMaximize, AttrKey(), true);
  for (int i = 0; i < n; ++i) {
    elemental.SetAttr(DoubleAttr1::kVarLb, AttrKey(vars[i]), 43.0);
    for (int j = 0; j < n; ++j) {
      elemental.SetAttr(DoubleAttr2::kLinConCoef,
                        AttrKey(vars[i], constraints[j]), 42.0);
    }
  }
  constexpr int kNumKeys = 1000;
  const auto keys = MakeRandomAttrKeys<dimension, NoSymmetry>(kNumKeys, n);
  for (auto s : state) {
    for (const auto& key : keys) {
      if constexpr (dimension == 0) {
        auto v = elemental.GetAttr<Policy>(BoolAttr0::kMaximize, key);
        benchmark::DoNotOptimize(v);
      } else if constexpr (dimension == 1) {
        auto v = elemental.GetAttr<Policy>(DoubleAttr1::kVarLb, key);
        benchmark::DoNotOptimize(v);
      } else if constexpr (dimension == 2) {
        auto v = elemental.GetAttr<Policy>(DoubleAttr2::kLinConCoef, key);
        benchmark::DoNotOptimize(v);
      }
    }
  }
}
BENCHMARK(BM_RandomGet<0>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<1>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<2>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<0, Elemental::StatusPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<1, Elemental::StatusPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<2, Elemental::StatusPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<0, Elemental::UBPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<1, Elemental::UBPolicy>)->Arg(1)->Arg(10)->Arg(100);
BENCHMARK(BM_RandomGet<2, Elemental::UBPolicy>)->Arg(1)->Arg(10)->Arg(100);

////////////////////////////////////////////////////////////////////////////////
// Other operations (e.g. AddDiff, Clone, Advance)
////////////////////////////////////////////////////////////////////////////////

TEST(ElementalTest, AddAndDeleteDiff) {
  Elemental e;
  EXPECT_EQ(e.NumDiffs(), 0);
  Elemental::DiffHandle h = e.AddDiff();
  EXPECT_EQ(e.NumDiffs(), 1);
  EXPECT_TRUE(e.DeleteDiff(h));
  EXPECT_EQ(e.NumDiffs(), 0);
  EXPECT_FALSE(e.DeleteDiff(h));
  EXPECT_EQ(e.NumDiffs(), 0);
}

TEST(ElementalTest, AdvanceWrongElemental) {
  Elemental e1;
  Elemental::DiffHandle h = e1.AddDiff();
  Elemental e2;
  EXPECT_FALSE(e2.Advance(h));
}

TEST(ElementalTest, AdvanceOnDeletedDiff) {
  Elemental e;
  Elemental::DiffHandle h = e.AddDiff();
  e.DeleteDiff(h);
  EXPECT_FALSE(e.Advance(h));
}

TEST(ElementalTest, CloneEmptyModel) {
  Elemental e1("mod", "obj");
  const Elemental e2 = e1.Clone();
  EXPECT_THAT(e2, EquivToElemental(e1));
}

TEST(ElementalTest, CloneSimpleModel) {
  Elemental e1("mod", "obj");
  e1.AddElement<ElementType::kVariable>("x");
  e1.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);
  e1.SetAttr(DoubleAttr1::kVarUb, AttrKey(0), 5.0);

  const Elemental e2 = e1.Clone();
  EXPECT_THAT(e2, EquivToElemental(e1));
}

TEST(ElementalTest, CloneRenameModel) {
  Elemental orig("mod");
  orig.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);

  const Elemental clone = orig.Clone("mod2");

  Elemental expected("mod2");
  expected.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);
  EXPECT_THAT(clone, EquivToElemental(expected));
}

TEST(ElementalTest, CloneModelWithDiffs) {
  Elemental orig("mod");
  orig.AddDiff();
  orig.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);

  const Elemental clone = orig.Clone("mod2");

  Elemental expected("mod2");
  expected.SetAttr(DoubleAttr0::kObjOffset, {}, 4.0);
  EXPECT_THAT(clone, EquivToElemental(expected));
  EXPECT_EQ(clone.NumDiffs(), 0);
}

void BM_DeleteElement(benchmark::State& state) {
  const int n = state.range(0);
  constexpr int kNumKeys = 100;
  constexpr auto kAttr = DoubleAttr2::kLinConCoef;
  const auto keys = MakeRandomAttrKeys<2, NoSymmetry>(kNumKeys, n);
  for (auto s : state) {
    state.PauseTiming();
    auto elemental = std::make_unique<Elemental>();
    for (int i = 0; i < n; ++i) {
      elemental->AddElement<ElementType::kVariable>("");
      elemental->AddElement<ElementType::kLinearConstraint>("");
    }
    for (int v = 0; v < n; ++v) {
      for (int c = 0; c < n; ++c) {
        elemental->SetAttr(kAttr, AttrKey(c, v), 42.0);
      }
    }
    state.ResumeTiming();
    for (int v = 0; v < n; ++v) {
      elemental->DeleteElement(VariableId(v));
    }
    state.PauseTiming();
    CHECK_EQ(elemental->AttrNonDefaults(kAttr).size(), 0);
    elemental.reset();
    state.ResumeTiming();
  }
}
BENCHMARK(BM_DeleteElement)->Arg(10)->Arg(100);

}  // namespace
}  // namespace operations_research::math_opt
