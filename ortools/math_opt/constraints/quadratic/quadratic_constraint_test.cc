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

#include "ortools/math_opt/constraints/quadratic/quadratic_constraint.h"

#include <sstream>
#include <string>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/math_opt/storage/sparse_matrix.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(QuadraticConstraintTest, OutputStreaming) {
  ModelStorage storage;
  const QuadraticConstraint q(
      &storage,
      storage.AddAtomicConstraint(QuadraticConstraintData{.name = "q"}));
  const QuadraticConstraint anonymous(
      &storage, storage.AddAtomicConstraint(QuadraticConstraintData{}));

  auto to_string = [](QuadraticConstraint c) {
    std::ostringstream oss;
    oss << c;
    return oss.str();
  };

  EXPECT_EQ(to_string(q), "q");
  EXPECT_EQ(to_string(anonymous),
            absl::StrCat("__quad_con#", anonymous.id(), "__"));
}

TEST(QuadraticConstraintTest, Accessors) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  SparseSymmetricMatrix quadratic_terms;
  quadratic_terms.set(x.typed_id(), y.typed_id(), 4.0);
  quadratic_terms.set(y.typed_id(), y.typed_id(), 5.0);
  const QuadraticConstraintData data{
      .lower_bound = 1.0,
      .upper_bound = 2.0,
      .linear_terms = SparseCoefficientMap({{x.typed_id(), 3.0}}),
      .quadratic_terms = quadratic_terms,
      .name = "q",
  };
  const QuadraticConstraint c(&storage, storage.AddAtomicConstraint(data));
  EXPECT_EQ(c.lower_bound(), 1.0);
  EXPECT_EQ(c.upper_bound(), 2.0);
  EXPECT_EQ(c.name(), "q");

  EXPECT_TRUE(c.is_linear_coefficient_nonzero(x));
  EXPECT_FALSE(c.is_linear_coefficient_nonzero(y));
  EXPECT_EQ(c.linear_coefficient(x), 3.0);
  EXPECT_EQ(c.linear_coefficient(y), 0.0);

  EXPECT_FALSE(c.is_quadratic_coefficient_nonzero(x, x));
  EXPECT_TRUE(c.is_quadratic_coefficient_nonzero(x, y));
  EXPECT_TRUE(c.is_quadratic_coefficient_nonzero(y, x));
  EXPECT_TRUE(c.is_quadratic_coefficient_nonzero(y, y));
  EXPECT_EQ(c.quadratic_coefficient(x, x), 0.0);
  EXPECT_EQ(c.quadratic_coefficient(x, y), 4.0);
  EXPECT_EQ(c.quadratic_coefficient(y, x), 4.0);
  EXPECT_EQ(c.quadratic_coefficient(y, y), 5.0);
}

TEST(QuadraticConstraintTest, Equality) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));

  const QuadraticConstraint c(
      &storage, storage.AddAtomicConstraint(QuadraticConstraintData{
                    .upper_bound = 1.5, .name = "upper_bounded"}));
  const QuadraticConstraint d(
      &storage, storage.AddAtomicConstraint(QuadraticConstraintData{
                    .lower_bound = 0.5, .name = "lower_bounded"}));

  // `d2` is another `QuadraticConstraint` that points the same constraint in
  // the indexed storage. It should compares == to `d`.
  const QuadraticConstraint d2(d.storage(), d.typed_id());

  // `e` has identical data as `d`. It should not compares equal to `d`, though.
  const QuadraticConstraint e(
      &storage, storage.AddAtomicConstraint(QuadraticConstraintData{
                    .lower_bound = 0.5, .name = "lower_bounded"}));

  EXPECT_TRUE(c == c);
  EXPECT_FALSE(c == d);
  EXPECT_TRUE(d == d2);
  EXPECT_FALSE(d == e);
  EXPECT_FALSE(c != c);
  EXPECT_TRUE(c != d);
  EXPECT_FALSE(d != d2);
  EXPECT_TRUE(d != e);
}

TEST(QuadraticConstraintTest, NonzeroVariables) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Variable z(&storage, storage.AddVariable("z"));
  const Variable w(&storage, storage.AddVariable("w"));
  SparseSymmetricMatrix quadratic_terms;
  quadratic_terms.set(y.typed_id(), z.typed_id(), 5.0);
  const QuadraticConstraintData data{
      .lower_bound = 1.0,
      .upper_bound = 2.0,
      .linear_terms = SparseCoefficientMap({{x.typed_id(), 3.0}}),
      .quadratic_terms = quadratic_terms,
      .name = "q",
  };
  const QuadraticConstraint q(&storage, storage.AddAtomicConstraint(data));

  EXPECT_THAT(q.NonzeroVariables(), UnorderedElementsAre(x, y, z));
}

TEST(QuadraticConstraintTest, AsBoundedQuadraticExpression) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Variable z(&storage, storage.AddVariable("z"));
  const Variable w(&storage, storage.AddVariable("w"));
  SparseSymmetricMatrix quadratic_terms;
  quadratic_terms.set(y.typed_id(), z.typed_id(), 5.0);
  const QuadraticConstraintData data{
      .lower_bound = 1.0,
      .upper_bound = 2.0,
      .linear_terms = SparseCoefficientMap({{x.typed_id(), 3.0}}),
      .quadratic_terms = quadratic_terms,
      .name = "q",
  };
  const QuadraticConstraint q(&storage, storage.AddAtomicConstraint(data));

  const BoundedQuadraticExpression expr = q.AsBoundedQuadraticExpression();
  EXPECT_EQ(expr.lower_bound, 1.0);
  EXPECT_EQ(expr.upper_bound, 2.0);
  EXPECT_EQ(expr.expression.offset(), 0.0);
  EXPECT_THAT(expr.expression.linear_terms(),
              UnorderedElementsAre(Pair(x, 3.0)));
  EXPECT_THAT(expr.expression.quadratic_terms(),
              UnorderedElementsAre(Pair(QuadraticTermKey(y, z), 5.0)));
}

TEST(QuadraticConstraintTest, ToString) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  const QuadraticConstraint c = model.AddQuadraticConstraint(
      1.0 <= -2 * x + 3 * y - 4.0 * x * x + 5.0 * x * y + 6.0 <= 7.0);
  EXPECT_EQ(c.ToString(), "-5 ≤ -4*x² + 5*x*y - 2*x + 3*y ≤ 1");

  model.DeleteVariable(y);
  EXPECT_EQ(c.ToString(), "-5 ≤ -4*x² - 2*x ≤ 1");

  model.DeleteQuadraticConstraint(c);
  EXPECT_EQ(c.ToString(), kDeletedConstraintDefaultDescription);
}

TEST(QuadraticConstraintTest, NameAfterDeletion) {
  Model model;
  const Variable x = model.AddVariable("x");
  const QuadraticConstraint c =
      model.AddQuadraticConstraint(2 * x * x >= 1, "c");
  ASSERT_EQ(c.name(), "c");

  model.DeleteQuadraticConstraint(c);
  EXPECT_EQ(c.name(), kDeletedConstraintDefaultDescription);
}

}  // namespace
}  // namespace operations_research::math_opt
