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

#include "ortools/math_opt/constraints/util/model_util.h"

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_int.h"
#include "ortools/math_opt/constraints/quadratic/storage.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/storage/linear_expression_data.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"

namespace operations_research::math_opt {
namespace {

using ::testing::ElementsAre;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using TestConstraint = QuadraticConstraint;
using TestData = QuadraticConstraintData;
using TestConstraintId = QuadraticConstraintId;

TEST(ModelUtilTest, ToLinearExpression) {
  const VariableId u(0);
  const VariableId v(1);

  LinearExpressionData data{.offset = 4.0};
  data.coeffs.set(u, 2.0);
  data.coeffs.set(v, 3.0);
  const ModelStorage storage;
  const LinearExpression expr = ToLinearExpression(storage, data);
  EXPECT_EQ(expr.storage(), &storage);
  EXPECT_EQ(expr.offset(), 4.0);
  EXPECT_THAT(expr.terms(),
              UnorderedElementsAre(Pair(Variable(&storage, u), 2.0),
                                   Pair(Variable(&storage, v), 3.0)));
}

TEST(ModelUtilTest, FromLinearExpression) {
  Model model;
  const Variable x = model.AddVariable();
  const Variable y = model.AddVariable();
  const LinearExpression expr = 2.0 * x + 3.0 * y + 4.0;

  const auto [coeffs, offset] = FromLinearExpression(expr);
  EXPECT_THAT(coeffs.terms(), UnorderedElementsAre(Pair(x.typed_id(), 2.0),
                                                   Pair(y.typed_id(), 3.0)));
  EXPECT_EQ(offset, 4.0);
}

TEST(ModelUtilTest, AtomicConstraints) {
  ModelStorage storage;
  const TestConstraintId c = storage.AddAtomicConstraint(TestData());
  const TestConstraintId d = storage.AddAtomicConstraint(TestData());

  EXPECT_THAT(AtomicConstraints<TestConstraint>(storage),
              UnorderedElementsAre(TestConstraint(&storage, c),
                                   TestConstraint(&storage, d)));
}

TEST(ModelUtilTest, SortedAtomicConstraints) {
  ModelStorage storage;
  const TestConstraintId c = storage.AddAtomicConstraint(TestData());
  const TestConstraintId d = storage.AddAtomicConstraint(TestData());

  EXPECT_THAT(
      SortedAtomicConstraints<TestConstraint>(storage),
      ElementsAre(TestConstraint(&storage, c), TestConstraint(&storage, d)));
}

}  // namespace
}  // namespace operations_research::math_opt
