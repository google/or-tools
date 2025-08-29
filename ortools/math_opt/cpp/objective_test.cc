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

#include "ortools/math_opt/cpp/objective.h"

#include <optional>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/model_storage_types.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(ObjectiveTest, Accessors) {
  ModelStorage storage(/*model_name=*/"", /*primary_objective_name=*/"primary");
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));

  const Objective primary = Objective::Primary(&storage);
  storage.set_objective_priority(primary.typed_id(), 1);
  storage.set_objective_offset(primary.typed_id(), 2.0);
  storage.set_linear_objective_coefficient(primary.typed_id(), x.typed_id(),
                                           3.0);
  const Objective secondary = Objective::Auxiliary(
      &storage, storage.AddAuxiliaryObjective(12, "secondary"));
  storage.set_maximize(secondary.typed_id());
  storage.set_quadratic_objective_coefficient(secondary.typed_id(),
                                              x.typed_id(), y.typed_id(), 4.0);

  EXPECT_EQ(primary.id(), std::nullopt);
  EXPECT_EQ(secondary.id(), 0);

  EXPECT_EQ(primary.typed_id(), kPrimaryObjectiveId);
  EXPECT_EQ(secondary.typed_id(), AuxiliaryObjectiveId(0));

  EXPECT_EQ(primary.storage(), &storage);
  EXPECT_EQ(secondary.storage(), &storage);

  EXPECT_TRUE(primary.is_primary());
  EXPECT_FALSE(secondary.is_primary());

  EXPECT_FALSE(primary.maximize());
  EXPECT_TRUE(secondary.maximize());

  EXPECT_EQ(primary.priority(), 1);
  EXPECT_EQ(secondary.priority(), 12);

  EXPECT_EQ(primary.name(), "primary");
  EXPECT_EQ(secondary.name(), "secondary");

  EXPECT_EQ(primary.offset(), 2.0);
  EXPECT_EQ(secondary.offset(), 0.0);
  EXPECT_EQ(primary.num_linear_terms(), 1);
  EXPECT_EQ(secondary.num_linear_terms(), 0);
  EXPECT_EQ(primary.num_quadratic_terms(), 0);
  EXPECT_EQ(secondary.num_quadratic_terms(), 1);

  EXPECT_EQ(primary.coefficient(x), 3.0);
  EXPECT_EQ(secondary.coefficient(x), 0.0);

  EXPECT_EQ(primary.coefficient(x, y), 0.0);
  EXPECT_EQ(secondary.coefficient(x, y), 4.0);

  EXPECT_TRUE(primary.is_coefficient_nonzero(x));
  EXPECT_FALSE(secondary.is_coefficient_nonzero(x));

  EXPECT_FALSE(primary.is_coefficient_nonzero(x, y));
  EXPECT_TRUE(secondary.is_coefficient_nonzero(x, y));
}

TEST(ObjectiveTest, NameAfterDeletion) {
  ModelStorage storage;
  const Objective o = Objective::Auxiliary(
      &storage, storage.AddAuxiliaryObjective(12, "secondary"));

  ASSERT_EQ(o.name(), "secondary");

  storage.DeleteAuxiliaryObjective(o.typed_id().value());
  EXPECT_EQ(o.name(), kDeletedObjectiveDefaultDescription);
}

TEST(ObjectiveTest, Equality) {
  ModelStorage storage(/*model_name=*/"", /*primary_objective_name=*/"primary");
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));

  const Objective c = Objective::Primary(&storage);
  const Objective d = Objective::Auxiliary(
      &storage, storage.AddAuxiliaryObjective(12, "secondary"));

  // `d2` is another `Objective` that points the same auxiliary objective in the
  // indexed storage. It should compares == to `d`.
  const Objective d2 = Objective::Auxiliary(d.storage(), d.typed_id().value());

  // `e` has identical data as `d`. It should not compares equal to `d`, though.
  const Objective e = Objective::Auxiliary(
      &storage, storage.AddAuxiliaryObjective(12, "secondary"));

  EXPECT_TRUE(c == c);
  EXPECT_FALSE(c == d);
  EXPECT_TRUE(d == d2);
  EXPECT_FALSE(d == e);
  EXPECT_FALSE(c != c);
  EXPECT_TRUE(c != d);
  EXPECT_FALSE(d != d2);
  EXPECT_TRUE(d != e);
}

TEST(ObjectiveTest, AsLinearExpression) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Objective o = Objective::Primary(&storage);
  storage.set_objective_offset(o.typed_id(), 1.0);
  storage.set_linear_objective_coefficient(o.typed_id(), x.typed_id(), 2.0);

  const LinearExpression o_expr = o.AsLinearExpression();
  EXPECT_EQ(o_expr.offset(), 1.0);
  EXPECT_THAT(o_expr.terms(), UnorderedElementsAre(Pair(x, 2.0)));
}

TEST(ObjectiveDeathTest, QuadraticObjectiveAsLinearExpression) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Objective o =
      Objective::Auxiliary(&storage, storage.AddAuxiliaryObjective(12));
  storage.set_quadratic_objective_coefficient(o.typed_id(), x.typed_id(),
                                              x.typed_id(), 1.0);
  EXPECT_DEATH(o.AsLinearExpression(), "quadratic");
}

TEST(ObjectiveTest, AsQuadraticExpression) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Objective o = Objective::Primary(&storage);
  storage.set_objective_offset(o.typed_id(), 1.0);
  storage.set_linear_objective_coefficient(o.typed_id(), x.typed_id(), 2.0);
  storage.set_quadratic_objective_coefficient(o.typed_id(), x.typed_id(),
                                              x.typed_id(), 3.0);

  const QuadraticExpression o_expr = o.AsQuadraticExpression();
  EXPECT_EQ(o_expr.offset(), 1.0);
  EXPECT_THAT(o_expr.linear_terms(), UnorderedElementsAre(Pair(x, 2.0)));
  EXPECT_THAT(o_expr.quadratic_terms(),
              UnorderedElementsAre(Pair(QuadraticTermKey(x, x), 3.0)));
}

TEST(ObjectiveTest, ToString) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Objective o = Objective::Auxiliary(
      &storage, storage.AddAuxiliaryObjective(12, "secondary"));
  storage.set_objective_offset(o.typed_id(), 1.0);
  storage.set_linear_objective_coefficient(o.typed_id(), x.typed_id(), 2.0);
  storage.set_quadratic_objective_coefficient(o.typed_id(), x.typed_id(),
                                              x.typed_id(), 3.0);

  EXPECT_EQ(o.ToString(), "3*x² + 2*x + 1");

  storage.DeleteAuxiliaryObjective(o.typed_id().value());
  EXPECT_EQ(o.ToString(), kDeletedObjectiveDefaultDescription);
}

TEST(ObjectiveTest, OutputStreaming) {
  ModelStorage storage(/*model_name=*/"", /*primary_objective_name=*/"primary");
  const Variable x(&storage, storage.AddVariable("x"));
  const Objective primary = Objective::Primary(&storage);
  const Objective secondary = Objective::Auxiliary(
      &storage, storage.AddAuxiliaryObjective(12, "secondary"));

  auto to_string = [](Objective c) {
    std::ostringstream oss;
    oss << c;
    return oss.str();
  };

  EXPECT_EQ(to_string(primary), "primary");
  EXPECT_EQ(to_string(secondary), "secondary");
}

TEST(ObjectiveTest, OutputStreamingEmptyName) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Objective primary = Objective::Primary(&storage);
  const Objective secondary =
      Objective::Auxiliary(&storage, storage.AddAuxiliaryObjective(12));

  auto to_string = [](Objective c) {
    std::ostringstream oss;
    oss << c;
    return oss.str();
  };

  EXPECT_EQ(to_string(primary), "__primary_obj__");
  EXPECT_EQ(to_string(secondary),
            absl::StrCat("__aux_obj#", secondary.id().value(), "__"));
}

TEST(ObjectiveDeathTest, CoefficientDifferentModel) {
  ModelStorage storage_a;
  ModelStorage storage_b;

  const Variable x_a = Variable(&storage_a, storage_a.AddVariable("x"));
  const Variable y_b = Variable(&storage_b, storage_b.AddVariable("y"));
  const Objective o_b = Objective::Primary(&storage_b);

  EXPECT_DEATH(o_b.coefficient(x_a), "another model");
  EXPECT_DEATH(o_b.coefficient(x_a, y_b), "another model");
  EXPECT_DEATH(o_b.coefficient(y_b, x_a), "another model");
  EXPECT_DEATH(o_b.is_coefficient_nonzero(x_a), "another model");
  EXPECT_DEATH(o_b.is_coefficient_nonzero(x_a, y_b), "another model");
  EXPECT_DEATH(o_b.is_coefficient_nonzero(y_b, x_a), "another model");
}

}  // namespace
}  // namespace operations_research::math_opt
