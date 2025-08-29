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

#include "ortools/math_opt/cpp/linear_constraint.h"

#include <limits>
#include <sstream>
#include <string>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/matchers.h"
#include "ortools/math_opt/cpp/model.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research {
namespace math_opt {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

constexpr double kInf = std::numeric_limits<double>::infinity();

TEST(LinearConstraintTest, OutputStreaming) {
  ModelStorage storage;
  const LinearConstraint c(&storage, storage.AddLinearConstraint("c"));
  const LinearConstraint anonymous(&storage, storage.AddLinearConstraint());

  auto to_string = [](LinearConstraint c) {
    std::ostringstream oss;
    oss << c;
    return oss.str();
  };

  EXPECT_EQ(to_string(c), "c");
  EXPECT_EQ(to_string(anonymous),
            absl::StrCat("__lin_con#", anonymous.id(), "__"));
}

TEST(LinearConstraintTest, Accessors) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  {
    const LinearConstraintId c_id = storage.AddLinearConstraint(
        /*lower_bound=*/-kInf, /*upper_bound=*/1.5, "upper_bounded");
    const LinearConstraint c(&storage, c_id);
    storage.set_linear_constraint_coefficient(c_id, x.typed_id(), 1.0);
    storage.set_linear_constraint_coefficient(c_id, y.typed_id(), 1.0);

    EXPECT_EQ(c.name(), "upper_bounded");
    EXPECT_EQ(c.id(), c_id.value());
    EXPECT_EQ(c.typed_id(), c_id);
    EXPECT_EQ(c.lower_bound(), -kInf);
    EXPECT_EQ(c.upper_bound(), 1.5);

    EXPECT_DOUBLE_EQ(c.coefficient(x), 1.0);
    EXPECT_DOUBLE_EQ(c.coefficient(y), 1.0);

    EXPECT_TRUE(c.is_coefficient_nonzero(x));
    EXPECT_TRUE(c.is_coefficient_nonzero(y));
  }
  {
    const LinearConstraintId c_id = storage.AddLinearConstraint(
        /*lower_bound=*/0.5, /*upper_bound=*/kInf, "lower_bounded");
    const LinearConstraint c(&storage, c_id);
    storage.set_linear_constraint_coefficient(c_id, y.typed_id(), 2.0);

    EXPECT_EQ(c.name(), "lower_bounded");
    EXPECT_EQ(c.id(), 1);
    EXPECT_EQ(c.lower_bound(), 0.5);
    EXPECT_EQ(c.upper_bound(), kInf);

    EXPECT_DOUBLE_EQ(c.coefficient(x), 0.0);
    EXPECT_DOUBLE_EQ(c.coefficient(y), 2.0);

    EXPECT_FALSE(c.is_coefficient_nonzero(x));
    EXPECT_TRUE(c.is_coefficient_nonzero(y));
  }
}

TEST(LinearConstraintTest, AsBoundedExpression) {
  // We don't use the IsNearlyEquivalent(BoundedLinearExpression) matcher in
  // tests as we want to test that the offset of
  // BoundedLinearExpression.expression is indeed 0 in returned values.

  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");
  {
    const LinearConstraint c = model.AddLinearConstraint(
        /*lower_bound=*/-kInf, /*upper_bound=*/1.5, "upper_bounded");
    model.set_coefficient(c, x, 1.0);
    model.set_coefficient(c, y, 2.0);

    const BoundedLinearExpression c_bounded_expr =
        c.AsBoundedLinearExpression();
    EXPECT_EQ(c_bounded_expr.lower_bound, -kInf);
    EXPECT_EQ(c_bounded_expr.upper_bound, 1.5);
    EXPECT_THAT(c_bounded_expr.expression, IsIdentical(x + 2.0 * y));
  }
  {
    const LinearConstraint c = model.AddLinearConstraint(
        /*lower_bound=*/0.5, /*upper_bound=*/kInf, "lower_bounded");
    model.set_coefficient(c, y, 2.0);

    const BoundedLinearExpression c_bounded_expr =
        c.AsBoundedLinearExpression();
    EXPECT_EQ(c_bounded_expr.lower_bound, 0.5);
    EXPECT_EQ(c_bounded_expr.upper_bound, kInf);
    // TODO(b/290378193): remove the `+ 0.0` when the IsIdentical() overload
    // issue is fixed (see bug for details).
    EXPECT_THAT(c_bounded_expr.expression, IsIdentical(2.0 * y + 0.0));
  }
  {
    const LinearConstraint c =
        model.AddLinearConstraint(3.5 <= 5.0 * x + 2.0 * y + 1.0 <= 4.25);
    const BoundedLinearExpression c_bounded_expr =
        c.AsBoundedLinearExpression();
    EXPECT_EQ(c_bounded_expr.lower_bound, 3.5 - 1.0);
    EXPECT_EQ(c_bounded_expr.upper_bound, 4.25 - 1.0);
    EXPECT_THAT(c_bounded_expr.expression, IsIdentical(5.0 * x + 2.0 * y));
  }
}

TEST(LinearConstraintTest, ToString) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  const LinearConstraint c =
      model.AddLinearConstraint(1.0 <= -2 * x + 3 * y + 4.0 <= 5.0);
  EXPECT_EQ(c.ToString(), "-3 ≤ -2*x + 3*y ≤ 1");

  model.DeleteVariable(x);
  EXPECT_EQ(c.ToString(), "-3 ≤ 3*y ≤ 1");
}

TEST(LinearConstraintTest, Equality) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));

  const LinearConstraint c(&storage, storage.AddLinearConstraint(
                                         /*lower_bound=*/-kInf,
                                         /*upper_bound=*/1.5, "upper_bounded"));
  storage.set_linear_constraint_coefficient(c.typed_id(), x.typed_id(), 1.0);
  storage.set_linear_constraint_coefficient(c.typed_id(), y.typed_id(), 1.0);

  const LinearConstraint d(
      &storage,
      storage.AddLinearConstraint(/*lower_bound=*/0.5,
                                  /*upper_bound=*/kInf, "lower_bounded"));
  storage.set_linear_constraint_coefficient(d.typed_id(), y.typed_id(), 2.0);

  // `d2` is another `LinearConstraint` that points the same constraint in the
  // indexed storage. It should compares == to `d`.
  const LinearConstraint d2(d.storage(), d.typed_id());

  // `e` has identical data as `d`. It should not compares equal to `d` tough.
  const LinearConstraint e(
      &storage,
      storage.AddLinearConstraint(/*lower_bound=*/0.5,
                                  /*upper_bound=*/kInf, "lower_bounded"));
  storage.set_linear_constraint_coefficient(d.typed_id(), y.typed_id(), 2.0);

  EXPECT_TRUE(c == c);
  EXPECT_FALSE(c == d);
  EXPECT_TRUE(d == d2);
  EXPECT_FALSE(d == e);
  EXPECT_FALSE(c != c);
  EXPECT_TRUE(c != d);
  EXPECT_FALSE(d != d2);
  EXPECT_TRUE(d != e);
}

TEST(LinearConstraintTest, NameAfterDeletion) {
  Model model;
  const Variable x = model.AddVariable("x");
  const LinearConstraint c = model.AddLinearConstraint(2 * x >= 1, "c");
  ASSERT_EQ(c.name(), "c");

  model.DeleteLinearConstraint(c);
  EXPECT_EQ(c.name(), kDeletedConstraintDefaultDescription);
}

}  // namespace
}  // namespace math_opt
}  // namespace operations_research
