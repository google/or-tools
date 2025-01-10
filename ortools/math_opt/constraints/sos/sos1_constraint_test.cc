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

#include "ortools/math_opt/constraints/sos/sos1_constraint.h"

#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/math_opt/constraints/util/model_util.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/storage/linear_expression_data.h"
#include "ortools/math_opt/storage/model_storage.h"
#include "ortools/math_opt/storage/sparse_coefficient_map.h"
#include "ortools/util/fp_roundtrip_conv_testing.h"

namespace operations_research::math_opt {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(Sos1ConstraintTest, Accessors) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Sos1ConstraintData data(
      {{.coeffs = SparseCoefficientMap({{x.typed_id(), 1.0}}), .offset = 0.0},
       {.coeffs =
            SparseCoefficientMap({{x.typed_id(), 0.5}, {y.typed_id(), 0.5}}),
        .offset = -1.0}},
      {3.0, 2.0}, "c");
  const Sos1Constraint c(&storage, storage.AddAtomicConstraint(data));
  EXPECT_EQ(c.name(), "c");
  ASSERT_EQ(c.num_expressions(), 2);
  EXPECT_THAT(c.Expression(0).terms(), UnorderedElementsAre(Pair(x, 1.0)));
  EXPECT_EQ(c.Expression(0).offset(), 0.0);
  EXPECT_TRUE(c.has_weights());
  EXPECT_EQ(c.weight(0), 3.0);
  EXPECT_THAT(c.Expression(1).terms(),
              UnorderedElementsAre(Pair(x, 0.5), Pair(y, 0.5)));
  EXPECT_EQ(c.Expression(1).offset(), -1.0);
  EXPECT_EQ(c.weight(1), 2.0);
}

TEST(Sos1ConstraintTest, Equality) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));

  const Sos1Constraint c(
      &storage, storage.AddAtomicConstraint(Sos1ConstraintData({}, {}, "c")));
  const Sos1Constraint d(
      &storage, storage.AddAtomicConstraint(Sos1ConstraintData({}, {}, "d")));

  // `d2` is another `Sos1Constraint` that points the same constraint in the
  // indexed storage. It should compares == to `d`.
  const Sos1Constraint d2(d.storage(), d.typed_id());

  // `e` has identical data as `d`. It should not compares equal to `d`, though.
  const Sos1Constraint e(
      &storage, storage.AddAtomicConstraint(Sos1ConstraintData({}, {}, "d")));

  EXPECT_TRUE(c == c);
  EXPECT_FALSE(c == d);
  EXPECT_TRUE(d == d2);
  EXPECT_FALSE(d == e);
  EXPECT_FALSE(c != c);
  EXPECT_TRUE(c != d);
  EXPECT_FALSE(d != d2);
  EXPECT_TRUE(d != e);
}

TEST(Sos1ConstraintTest, NonzeroVariables) {
  ModelStorage storage;
  const Variable x(&storage, storage.AddVariable("x"));
  const Variable y(&storage, storage.AddVariable("y"));
  const Variable z(&storage, storage.AddVariable("z"));
  const Variable w(&storage, storage.AddVariable("w"));
  const Sos1ConstraintData data(
      {{.coeffs = SparseCoefficientMap({{z.typed_id(), 1.0}}), .offset = 0.0},
       {.coeffs =
            SparseCoefficientMap({{x.typed_id(), 0.5}, {y.typed_id(), 0.5}}),
        .offset = -1.0}},
      {3.0, 2.0}, "c");
  const Sos1Constraint c(&storage, storage.AddAtomicConstraint(data));

  EXPECT_THAT(c.NonzeroVariables(), UnorderedElementsAre(x, y, z));
}

TEST(Sos1ConstraintTest, ToString) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Variable y = model.AddVariable("y");

  const Sos1Constraint c =
      model.AddSos1Constraint({x, y}, {3.0, kRoundTripTestNumber}, "c");
  EXPECT_EQ(c.ToString(), absl::StrCat("{x, y} is SOS1 with weights {3, ",
                                       kRoundTripTestNumberStr, "}"));

  const Sos1Constraint d = model.AddSos1Constraint({2 * y - 1, 1.0}, {}, "d");
  EXPECT_EQ(d.ToString(), "{2*y - 1, 1} is SOS1");

  model.DeleteSos1Constraint(c);
  EXPECT_EQ(c.ToString(), kDeletedConstraintDefaultDescription);
}

TEST(Sos1ConstraintTest, OutputStreaming) {
  ModelStorage storage;
  const Sos1Constraint q(
      &storage, storage.AddAtomicConstraint(Sos1ConstraintData({}, {}, "q")));
  const Sos1Constraint anonymous(
      &storage, storage.AddAtomicConstraint(Sos1ConstraintData({}, {}, "")));

  auto to_string = [](Sos1Constraint c) {
    std::ostringstream oss;
    oss << c;
    return oss.str();
  };

  EXPECT_EQ(to_string(q), "q");
  EXPECT_EQ(to_string(anonymous),
            absl::StrCat("__sos1_con#", anonymous.id(), "__"));
}

TEST(Sos1ConstraintTest, NameAfterDeletion) {
  Model model;
  const Variable x = model.AddVariable("x");
  const Sos1Constraint c = model.AddSos1Constraint({x}, {}, "c");
  ASSERT_EQ(c.name(), "c");

  model.DeleteSos1Constraint(c);
  EXPECT_EQ(c.name(), kDeletedConstraintDefaultDescription);
}

}  // namespace
}  // namespace operations_research::math_opt
