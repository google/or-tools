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

#include "ortools/math_opt/constraints/second_order_cone/second_order_cone_constraint.h"

#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(SecondOrderConeConstraintTest, Accessors) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Variable z(&storage, storage.AddVariable("z"));
  const SecondOrderConeConstraintData data{
      .upper_bound = {.coeffs = SparseCoefficientMap({{x.typed_id(), 1.0}}),
                      .offset = 2.0},
      .arguments_to_norm = {{.coeffs =
                                 SparseCoefficientMap({{y.typed_id(), 3.0}}),
                             .offset = 4.0},
                            {.coeffs =
                                 SparseCoefficientMap({{z.typed_id(), 5.0}}),
                             .offset = 6.0}},
      .name = "soc",
  };
  const SecondOrderConeConstraint c(&storage,
                                    storage.AddAtomicConstraint(data));
  EXPECT_EQ(c.name(), "soc");
  EXPECT_EQ(c.storage(), &storage);
  {
    const LinearExpression ub = c.UpperBound();
    EXPECT_EQ(ub.offset(), 2.0);
    EXPECT_THAT(ub.terms(), UnorderedElementsAre(Pair(x, 1.0)));
  }
  {
    const std::vector<LinearExpression> args = c.ArgumentsToNorm();
    ASSERT_EQ(args.size(), 2);
    EXPECT_EQ(args[0].offset(), 4.0);
    EXPECT_THAT(args[0].terms(), UnorderedElementsAre(Pair(y, 3.0)));
    EXPECT_EQ(args[1].offset(), 6.0);
    EXPECT_THAT(args[1].terms(), UnorderedElementsAre(Pair(z, 5.0)));
  }
}

TEST(SecondOrderConeConstraintTest, Equality) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));

  const SecondOrderConeConstraint c(
      &storage, storage.AddAtomicConstraint(SecondOrderConeConstraintData{
                    .upper_bound = {.offset = 1.0}, .name = "c"}));
  const SecondOrderConeConstraint d(
      &storage, storage.AddAtomicConstraint(SecondOrderConeConstraintData{
                    .upper_bound = {.offset = 2.0}, .name = "d"}));

  // `d2` is another `SecondOrderConstraint` that points the same constraint in
  // the indexed storage. It should compares == to `d`.
  const SecondOrderConeConstraint d2(d.storage(), d.typed_id());

  // `e` has identical data as `d`. It should not compares equal to `d`, though.
  const SecondOrderConeConstraint e(
      &storage, storage.AddAtomicConstraint(SecondOrderConeConstraintData{
                    .upper_bound = {.offset = 2.0}, .name = "d"}));

  EXPECT_TRUE(c == c);
  EXPECT_FALSE(c == d);
  EXPECT_TRUE(d == d2);
  EXPECT_FALSE(d == e);
  EXPECT_FALSE(c != c);
  EXPECT_TRUE(c != d);
  EXPECT_FALSE(d != d2);
  EXPECT_TRUE(d != e);
}

TEST(SecondOrderConeConstraintTest, NonzeroVariables) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Variable z(&storage, storage.AddVariable("z"));
  const SecondOrderConeConstraintData data{
      .upper_bound = {.coeffs = SparseCoefficientMap({{x.typed_id(), 1.0}}),
                      .offset = 2.0},
      .arguments_to_norm = {{.coeffs =
                                 SparseCoefficientMap({{y.typed_id(), 3.0}}),
                             .offset = 4.0},
                            {.coeffs =
                                 SparseCoefficientMap({{z.typed_id(), 5.0}}),
                             .offset = 6.0}},
      .name = "soc",
  };
  const SecondOrderConeConstraint c(&storage,
                                    storage.AddAtomicConstraint(data));

  EXPECT_THAT(c.NonzeroVariables(), UnorderedElementsAre(x, y, z));
}

TEST(SecondOrderConeConstraintTest, ToString) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Variable z(&storage, storage.AddVariable("z"));
  const SecondOrderConeConstraintData data{
      .upper_bound = {.coeffs = SparseCoefficientMap({{x.typed_id(), 1.0}}),
                      .offset = 2.0},
      .arguments_to_norm = {{.coeffs =
                                 SparseCoefficientMap({{y.typed_id(), 3.0}}),
                             .offset = 4.0},
                            {.coeffs =
                                 SparseCoefficientMap({{z.typed_id(), 5.0}}),
                             .offset = 6.0}},
      .name = "soc",
  };
  const SecondOrderConeConstraint c(&storage,
                                    storage.AddAtomicConstraint(data));

  EXPECT_EQ(c.ToString(), "||{3*y + 4, 5*z + 6}||₂ ≤ x + 2");

  storage.DeleteAtomicConstraint(c.typed_id());
  EXPECT_EQ(c.ToString(), kDeletedConstraintDefaultDescription);
}

TEST(SecondOrderConeConstraintTest, OutputStreaming) {
  ModelStorage storage;
  const SecondOrderConeConstraint q(
      &storage,
      storage.AddAtomicConstraint(SecondOrderConeConstraintData{.name = "q"}));
  const SecondOrderConeConstraint anonymous(
      &storage,
      storage.AddAtomicConstraint(SecondOrderConeConstraintData{.name = ""}));

  auto to_string = [](SecondOrderConeConstraint c) {
    std::ostringstream oss;
    oss << c;
    return oss.str();
  };

  EXPECT_EQ(to_string(q), "q");
  EXPECT_EQ(to_string(anonymous),
            absl::StrCat("__soc_con#", anonymous.id(), "__"));
}

TEST(SecondOrderConeConstraintTest, NameAfterDeletion) {
  ModelStorage storage;
  const SecondOrderConeConstraintData data{.name = "soc"};
  const SecondOrderConeConstraint c(&storage,
                                    storage.AddAtomicConstraint(data));

  ASSERT_EQ(c.name(), "soc");

  storage.DeleteAtomicConstraint(c.typed_id());
  EXPECT_EQ(c.name(), kDeletedConstraintDefaultDescription);
}

}  // namespace
}  // namespace operations_research::math_opt
