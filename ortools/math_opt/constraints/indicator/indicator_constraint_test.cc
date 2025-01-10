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

#include "ortools/math_opt/constraints/indicator/indicator_constraint.h"

#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Optional;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(IndicatorConstraintTest, Accessors) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Variable z(&storage, storage.AddVariable("z"));
  SparseCoefficientMap coeffs;
  coeffs.set(x.typed_id(), 2.0);
  coeffs.set(y.typed_id(), 3.0);
  const IndicatorConstraintData data{
      .lower_bound = -1.0,
      .upper_bound = 1.0,
      .linear_terms = std::move(coeffs),
      .indicator = z.typed_id(),
      .activate_on_zero = true,
      .name = "c",
  };
  const IndicatorConstraint c(&storage, storage.AddAtomicConstraint(data));
  EXPECT_EQ(c.name(), "c");
  EXPECT_THAT(c.indicator_variable(), Optional(z));
  const BoundedLinearExpression constraint = c.ImpliedConstraint();
  EXPECT_EQ(constraint.lower_bound, -1.0);
  EXPECT_EQ(constraint.upper_bound, 1.0);
  EXPECT_EQ(constraint.expression.offset(), 0.0);
  EXPECT_THAT(constraint.expression.terms(),
              UnorderedElementsAre(Pair(x, 2.0), Pair(y, 3.0)));
  EXPECT_TRUE(c.activate_on_zero());
}

TEST(IndicatorConstraintTest, Equality) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));

  const IndicatorConstraint c(
      &storage, storage.AddAtomicConstraint(
                    IndicatorConstraintData{.upper_bound = 1.0, .name = "c"}));
  const IndicatorConstraint d(
      &storage, storage.AddAtomicConstraint(
                    IndicatorConstraintData{.upper_bound = 2.0, .name = "d"}));

  // `d2` is another `IndicatorConstraint` that points the same constraint in
  // the indexed storage. It should compares == to `d`.
  const IndicatorConstraint d2(d.storage(), d.typed_id());

  // `e` has identical data as `d`. It should not compares equal to `d`, though.
  const IndicatorConstraint e(
      &storage, storage.AddAtomicConstraint(
                    IndicatorConstraintData{.upper_bound = 2.0, .name = "d"}));

  EXPECT_TRUE(c == c);
  EXPECT_FALSE(c == d);
  EXPECT_TRUE(d == d2);
  EXPECT_FALSE(d == e);
  EXPECT_FALSE(c != c);
  EXPECT_TRUE(c != d);
  EXPECT_FALSE(d != d2);
  EXPECT_TRUE(d != e);
}

TEST(IndicatorConstraintTest, NonzeroVariables) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Variable z(&storage, storage.AddVariable("z"));
  SparseCoefficientMap coeffs;
  coeffs.set(x.typed_id(), 2.0);
  coeffs.set(y.typed_id(), 3.0);
  const IndicatorConstraintData data{
      .lower_bound = -1.0,
      .upper_bound = 1.0,
      .linear_terms = std::move(coeffs),
      .indicator = z.typed_id(),
      .name = "c",
  };
  const IndicatorConstraint c(&storage, storage.AddAtomicConstraint(data));

  EXPECT_THAT(c.NonzeroVariables(), UnorderedElementsAre(x, y, z));
}

TEST(IndicatorConstraintTest, IndicatorGetter) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddVariable("y");
  const IndicatorConstraint c = model.AddIndicatorConstraint(x, y <= 1);

  EXPECT_THAT(c.indicator_variable(), Optional(x));

  model.DeleteVariable(x);
  EXPECT_EQ(c.indicator_variable(), std::nullopt);
}

TEST(IndicatorConstraintTest, ToString) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const Variable y = model.AddVariable("y");

  const IndicatorConstraint c = model.AddIndicatorConstraint(
      x, -3.0 <= 2 * y + 1 <= 3.0, /*activate_on_zero=*/false, "c");
  EXPECT_EQ(c.ToString(), "x = 1 ⇒ -4 ≤ 2*y ≤ 2");

  const IndicatorConstraint d = model.AddIndicatorConstraint(
      x, -2 * y == 3, /*activate_on_zero=*/true, "d");
  EXPECT_EQ(d.ToString(), "x = 0 ⇒ -2*y = 3");

  model.DeleteVariable(x);
  EXPECT_EQ(c.ToString(), "[unset indicator variable] ⇒ -4 ≤ 2*y ≤ 2");

  model.DeleteIndicatorConstraint(c);
  EXPECT_EQ(c.ToString(), kDeletedConstraintDefaultDescription);
}

TEST(IndicatorConstraintTest, OutputStreaming) {
  ModelStorage storage;
  const IndicatorConstraint q(
      &storage,
      storage.AddAtomicConstraint(IndicatorConstraintData{.name = "q"}));
  const IndicatorConstraint anonymous(
      &storage,
      storage.AddAtomicConstraint(IndicatorConstraintData{.name = ""}));

  auto to_string = [](IndicatorConstraint c) {
    std::ostringstream oss;
    oss << c;
    return oss.str();
  };

  EXPECT_EQ(to_string(q), "q");
  EXPECT_EQ(to_string(anonymous),
            absl::StrCat("__indic_con#", anonymous.id(), "__"));
}

TEST(IndicatorConstraintTest, NameAfterDeletion) {
  Model model;
  const Variable x = model.AddBinaryVariable("x");
  const IndicatorConstraint c =
      model.AddIndicatorConstraint(x, x >= 1, /*activate_on_zero=*/false, "c");
  ASSERT_EQ(c.name(), "c");

  model.DeleteIndicatorConstraint(c);
  EXPECT_EQ(c.name(), kDeletedConstraintDefaultDescription);
}

}  // namespace
}  // namespace operations_research::math_opt
