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

#include "ortools/math_opt/elemental/safe_attr_ops.h"

#include <cstdint>

#include "absl/status/status.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/elemental/attr_key.h"
#include "ortools/math_opt/elemental/attributes.h"
#include "ortools/math_opt/elemental/elemental.h"
#include "ortools/math_opt/elemental/elements.h"

namespace operations_research::math_opt {

namespace {

using testing::HasSubstr;
using testing::status::IsOk;
using testing::status::StatusIs;

TEST(AttrOp, Attr0) {
  Elemental e;

  constexpr int attr = static_cast<int>(BoolAttr0::kMaximize);

  // Get.
  EXPECT_THAT(AttrOp<BoolAttr0>::Get(&e, attr, AttrKey()), IsOk());
  // Get, bad attr.
  EXPECT_THAT(
      AttrOp<BoolAttr0>::Get(&e, -1, AttrKey()),
      StatusIs(absl::StatusCode::kInvalidArgument, "invalid attribute -1"));

  // Set.
  EXPECT_THAT(AttrOp<BoolAttr0>::Set(&e, attr, AttrKey(), true), IsOk());
  // Set, bad attr.
  EXPECT_THAT(
      AttrOp<BoolAttr0>::Set(&e, -1, AttrKey(), true),
      StatusIs(absl::StatusCode::kInvalidArgument, "invalid attribute -1"));

  // IsNonDefault.
  EXPECT_THAT(AttrOp<BoolAttr0>::IsNonDefault(&e, attr, AttrKey()), IsOk());
  // IsNonDefault, bad attr.
  EXPECT_THAT(
      AttrOp<BoolAttr0>::IsNonDefault(&e, -1, AttrKey()),
      StatusIs(absl::StatusCode::kInvalidArgument, "invalid attribute -1"));

  // NumNonDefault.
  EXPECT_THAT(AttrOp<BoolAttr0>::NumNonDefaults(&e, attr), IsOk());
  // NumNonDefault, bad attr.
  EXPECT_THAT(
      AttrOp<BoolAttr0>::NumNonDefaults(&e, -1),
      StatusIs(absl::StatusCode::kInvalidArgument, "invalid attribute -1"));

  // GetNonDefaults.
  EXPECT_THAT(AttrOp<BoolAttr0>::GetNonDefaults(&e, attr), IsOk());
  // GetNonDefaults, bad attr.
  EXPECT_THAT(
      AttrOp<BoolAttr0>::GetNonDefaults(&e, -1),
      StatusIs(absl::StatusCode::kInvalidArgument, "invalid attribute -1"));
}

TEST(AttrOp, Attr1) {
  Elemental e;

  const VariableId x = e.AddElement<ElementType::kVariable>("x");

  constexpr int attr = static_cast<int>(DoubleAttr1::kVarLb);

  // Get.
  EXPECT_THAT(AttrOp<DoubleAttr1>::Get(&e, attr, AttrKey(x)), IsOk());
  // Get, bad key.
  EXPECT_THAT(AttrOp<DoubleAttr1>::Get(&e, attr, AttrKey(-1)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("-1"), HasSubstr("variable"))));

  // Set.
  EXPECT_THAT(AttrOp<DoubleAttr1>::Set(&e, attr, AttrKey(x), true), IsOk());
  // Set, bad key.
  EXPECT_THAT(AttrOp<DoubleAttr1>::Set(&e, attr, AttrKey(-1), true),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("-1"), HasSubstr("variable"))));

  // IsNonDefault.
  EXPECT_THAT(AttrOp<DoubleAttr1>::IsNonDefault(&e, attr, AttrKey(x)), IsOk());
  // IsNonDefault, bad key.
  EXPECT_THAT(AttrOp<DoubleAttr1>::IsNonDefault(&e, attr, AttrKey(-1)),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       AllOf(HasSubstr("-1"), HasSubstr("variable"))));
}
}  // namespace

}  // namespace operations_research::math_opt
