// Copyright 2010-2024 Google LLC
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

#include "ortools/sat/linear_constraint_manager.h"

#include <algorithm>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/glop/variables_info.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;
using ::testing::EndsWith;
using ::testing::StartsWith;
using ::testing::UnorderedElementsAre;
using ConstraintIndex = LinearConstraintManager::ConstraintIndex;

TEST(LinearConstraintSymmetrizerTest, BasicFolding) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable a = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable b = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable c = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable sum_xy = model.Add(NewIntegerVariable(-20, 20));
  const IntegerVariable sum_abc = model.Add(NewIntegerVariable(-30, 30));
  auto* symmetrizer = model.GetOrCreate<LinearConstraintSymmetrizer>();
  symmetrizer->AddSymmetryOrbit(sum_xy, {x, y});
  symmetrizer->AddSymmetryOrbit(sum_abc, {a, b, c});

  LinearConstraintBuilder builder(0, 10);
  builder.AddTerm(x, IntegerValue(2));
  builder.AddTerm(y, IntegerValue(1));
  builder.AddTerm(z, IntegerValue(3));
  builder.AddTerm(a, IntegerValue(2));
  builder.AddTerm(c, IntegerValue(5));
  LinearConstraint ct = builder.Build();
  symmetrizer->FoldLinearConstraint(&ct);

  // We will scale by 6 (one orbit of size 2 and one of size 3).
  builder.Clear();
  builder.AddTerm(z, IntegerValue(6 * 3));
  builder.AddTerm(sum_xy, IntegerValue(6 / 2 * (2 + 1)));
  builder.AddTerm(sum_abc, IntegerValue(6 / 3 * (2 + 5)));
  LinearConstraint expected = builder.BuildConstraint(0, 6 * 10);
  EXPECT_EQ(ct.DebugString(), expected.DebugString());
}

TEST(LinearConstraintSymmetrizerTest, FoldingWithSumVariableOriginallyPresent) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable sum_xy = model.Add(NewIntegerVariable(-20, 20));
  auto* symmetrizer = model.GetOrCreate<LinearConstraintSymmetrizer>();
  symmetrizer->AddSymmetryOrbit(sum_xy, {x, y});

  LinearConstraintBuilder builder(0, 10);
  builder.AddTerm(x, IntegerValue(2));
  builder.AddTerm(y, IntegerValue(1));
  builder.AddTerm(z, IntegerValue(3));
  builder.AddTerm(sum_xy, IntegerValue(7));
  LinearConstraint ct = builder.Build();
  symmetrizer->FoldLinearConstraint(&ct);

  // We will scale by 2 the original sum_xy, and by 1 the one coming from the
  // orbit with coeff (2 + 1) from x and y.
  builder.Clear();
  builder.AddTerm(z, IntegerValue(2 * 3));
  builder.AddTerm(sum_xy, IntegerValue(2 * 7 + 2 / 2 * (2 + 1)));
  LinearConstraint expected = builder.BuildConstraint(0, 2 * 10);
  EXPECT_EQ(ct.DebugString(), expected.DebugString());
}

TEST(LinearConstraintManagerTest, DuplicateDetection) {
  Model model;
  LinearConstraintManager manager(&model);
  const IntegerVariable x = model.Add(NewIntegerVariable(-10, 10));

  LinearConstraintBuilder ct_one(IntegerValue(0), IntegerValue(10));
  ct_one.AddTerm(x, IntegerValue(2));
  manager.Add(ct_one.Build());

  LinearConstraintBuilder ct_two(IntegerValue(-4), IntegerValue(6));
  ct_two.AddTerm(NegationOf(x), IntegerValue(-2));
  manager.Add(ct_two.Build());

  EXPECT_EQ(manager.AllConstraints().size(), 1);
  EXPECT_EQ(manager.AllConstraints().front().constraint.DebugString(),
            "0 <= 1*X0 <= 3");
}

void SetLpValue(IntegerVariable v, double value, Model* model) {
  auto& values = *model->GetOrCreate<ModelLpValues>();
  const int needed_size = 1 + std::max(v.value(), NegationOf(v).value());
  if (needed_size > values.size()) values.resize(needed_size, 0.0);
  values[v] = value;
  values[NegationOf(v)] = -value;
}

TEST(LinearConstraintManagerTest, DuplicateDetectionCuts) {
  Model model;
  LinearConstraintManager manager(&model);
  const IntegerVariable x = model.Add(NewIntegerVariable(-10, 10));
  SetLpValue(x, -4.0, &model);

  LinearConstraintBuilder ct_one(IntegerValue(0), IntegerValue(10));
  ct_one.AddTerm(x, IntegerValue(2));
  manager.AddCut(ct_one.Build(), "Cut");

  LinearConstraintBuilder ct_two(IntegerValue(-4), IntegerValue(6));
  ct_two.AddTerm(NegationOf(x), IntegerValue(-2));
  manager.AddCut(ct_two.Build(), "Cut");

  // The second cut is more restrictive so it counts.
  EXPECT_EQ(manager.num_cuts(), 2);

  EXPECT_EQ(manager.AllConstraints().size(), 1);
  EXPECT_EQ(manager.AllConstraints().front().constraint.DebugString(),
            "0 <= 1*X0 <= 3");
}

TEST(LinearConstraintManagerTest, DuplicateDetectionCauseLpChange) {
  Model model;
  LinearConstraintManager manager(&model);
  const IntegerVariable x = model.Add(NewIntegerVariable(-10, 10));
  SetLpValue(x, 0.0, &model);

  LinearConstraintBuilder ct_one(IntegerValue(0), IntegerValue(10));
  ct_one.AddTerm(x, IntegerValue(2));
  manager.Add(ct_one.Build());

  manager.AddAllConstraintsToLp();
  EXPECT_THAT(manager.LpConstraints(),
              UnorderedElementsAre(ConstraintIndex(0)));
  glop::BasisState state;
  state.statuses.resize(glop::ColIndex(1));
  EXPECT_FALSE(manager.ChangeLp(&state));

  // Adding the second constraint will cause a bound change, so ChangeLp() will
  // returns true even if the constraint is satisfied.
  LinearConstraintBuilder ct_two(IntegerValue(-4), IntegerValue(6));
  ct_two.AddTerm(x, IntegerValue(2));
  manager.Add(ct_two.Build());
  EXPECT_TRUE(manager.ChangeLp(&state));

  EXPECT_EQ(manager.AllConstraints().size(), 1);
  EXPECT_EQ(manager.AllConstraints().front().constraint.DebugString(),
            "0 <= 1*X0 <= 3");
}

TEST(LinearConstraintManagerTest, OnlyAddInfeasibleConstraints) {
  Model model;
  LinearConstraintManager manager(&model);
  const IntegerVariable x = model.Add(NewIntegerVariable(-10, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(-10, 10));
  SetLpValue(x, 0.0, &model);
  SetLpValue(y, 0.0, &model);

  LinearConstraintBuilder ct_one(IntegerValue(0), IntegerValue(10));
  ct_one.AddTerm(x, IntegerValue(2));
  ct_one.AddTerm(y, IntegerValue(3));
  manager.Add(ct_one.Build());

  LinearConstraintBuilder ct_two(IntegerValue(-4), IntegerValue(6));
  ct_two.AddTerm(x, IntegerValue(3));
  ct_one.AddTerm(y, IntegerValue(2));
  manager.Add(ct_two.Build());

  EXPECT_TRUE(manager.LpConstraints().empty());
  EXPECT_EQ(manager.AllConstraints().size(), 2);

  // All constraints satisfy this, so no change.
  glop::BasisState state;
  state.statuses.resize(glop::ColIndex(2));  // Content is not relevant.
  EXPECT_FALSE(manager.ChangeLp(&state));
  EXPECT_FALSE(manager.ChangeLp(&state));

  SetLpValue(x, -1.0, &model);
  EXPECT_TRUE(manager.ChangeLp(&state));
  EXPECT_THAT(manager.LpConstraints(),
              UnorderedElementsAre(ConstraintIndex(0)));
  EXPECT_EQ(state.statuses.size(), glop::ColIndex(3));  // State was resized.
  EXPECT_EQ(state.statuses[glop::ColIndex(2)], glop::VariableStatus::BASIC);

  // Note that we keep the first constraint even if the value of 4.0 make it
  // satisfied.
  SetLpValue(x, 4.0, &model);
  EXPECT_TRUE(manager.ChangeLp(&state));
  EXPECT_THAT(manager.LpConstraints(),
              UnorderedElementsAre(ConstraintIndex(0), ConstraintIndex(1)));
  EXPECT_EQ(state.statuses.size(), glop::ColIndex(4));  // State was resized.
  EXPECT_EQ(state.statuses[glop::ColIndex(3)], glop::VariableStatus::BASIC);
}

TEST(LinearConstraintManagerTest, OnlyAddOrthogonalConstraints) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_min_orthogonality_for_lp_constraints(
      0.8);
  LinearConstraintManager manager(&model);
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  SetLpValue(x, 1.0, &model);
  SetLpValue(y, 1.0, &model);
  SetLpValue(z, 1.0, &model);

  LinearConstraintBuilder ct_one(IntegerValue(0), IntegerValue(11));
  ct_one.AddTerm(x, IntegerValue(3));
  ct_one.AddTerm(y, IntegerValue(-4));
  manager.Add(ct_one.Build());

  LinearConstraintBuilder ct_two(IntegerValue(-4), IntegerValue(2));
  ct_two.AddTerm(z, IntegerValue(-5));
  manager.Add(ct_two.Build());

  LinearConstraintBuilder ct_three(IntegerValue(0), IntegerValue(14));
  ct_three.AddTerm(x, IntegerValue(5));
  ct_three.AddTerm(y, IntegerValue(5));
  ct_three.AddTerm(z, IntegerValue(5));
  manager.Add(ct_three.Build());

  EXPECT_TRUE(manager.LpConstraints().empty());
  EXPECT_EQ(manager.AllConstraints().size(), 3);

  // First Call. Last constraint does not satisfy the orthogonality criteria.
  glop::BasisState state;
  EXPECT_TRUE(manager.ChangeLp(&state));
  EXPECT_THAT(manager.LpConstraints(),
              UnorderedElementsAre(ConstraintIndex(0), ConstraintIndex(1)));

  // Second Call. Only the last constraint is considered. The other two
  // constraints are already added.
  EXPECT_TRUE(manager.ChangeLp(&state));
  EXPECT_THAT(manager.LpConstraints(),
              UnorderedElementsAre(ConstraintIndex(0), ConstraintIndex(1),
                                   ConstraintIndex(2)));
}

TEST(LinearConstraintManagerTest, RemoveIneffectiveCuts) {
  Model model;
  model.GetOrCreate<SatParameters>()->set_max_consecutive_inactive_count(0);

  LinearConstraintManager manager(&model);
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  SetLpValue(x, 1.0, &model);
  SetLpValue(y, 1.0, &model);

  LinearConstraintBuilder ct_one(IntegerValue(0), IntegerValue(11));
  ct_one.AddTerm(x, IntegerValue(3));
  ct_one.AddTerm(y, IntegerValue(-4));
  manager.AddCut(ct_one.Build(), "Cut");

  EXPECT_TRUE(manager.LpConstraints().empty());
  EXPECT_EQ(manager.AllConstraints().size(), 1);

  // First Call. The constraint is added to LP.
  glop::BasisState state;
  EXPECT_TRUE(manager.ChangeLp(&state));
  EXPECT_THAT(manager.LpConstraints(),
              UnorderedElementsAre(ConstraintIndex(0)));

  // Second Call. Constraint is inactive and hence removed.
  state.statuses.resize(glop::ColIndex(2 + manager.LpConstraints().size()));
  state.statuses[glop::ColIndex(2)] = glop::VariableStatus::BASIC;
  EXPECT_TRUE(manager.ChangeLp(&state));
  EXPECT_TRUE(manager.LpConstraints().empty());
  EXPECT_EQ(state.statuses.size(), glop::ColIndex(2));
}

TEST(LinearConstraintManagerTest, ObjectiveParallelism) {
  Model model;
  LinearConstraintManager manager(&model);
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  SetLpValue(x, 1.0, &model);
  SetLpValue(y, 1.0, &model);
  SetLpValue(z, 1.0, &model);

  manager.SetObjectiveCoefficient(x, IntegerValue(1));
  manager.SetObjectiveCoefficient(y, IntegerValue(1));

  LinearConstraintBuilder ct_one(IntegerValue(0), IntegerValue(0));
  ct_one.AddTerm(z, IntegerValue(-1));
  manager.Add(ct_one.Build());

  LinearConstraintBuilder ct_two(IntegerValue(0), IntegerValue(2));
  ct_two.AddTerm(x, IntegerValue(1));
  ct_two.AddTerm(y, IntegerValue(1));
  ct_two.AddTerm(z, IntegerValue(1));
  manager.Add(ct_two.Build());

  EXPECT_TRUE(manager.LpConstraints().empty());
  EXPECT_EQ(manager.AllConstraints().size(), 2);

  // Last constraint is more parallel to the objective.
  glop::BasisState state;
  EXPECT_TRUE(manager.ChangeLp(&state));
  // scores: efficacy, orthogonality, obj_para, total
  // ct_one:        1,             1,        0,     2
  // ct_two:   0.5774,             1,   0.8165, 2.394

  EXPECT_THAT(manager.LpConstraints(),
              ElementsAre(ConstraintIndex(1), ConstraintIndex(0)));
}

TEST(LinearConstraintManagerTest, SimplificationRemoveFixedVariable) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 5));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  SetLpValue(x, 0.0, &model);
  SetLpValue(y, 0.0, &model);
  SetLpValue(z, 0.0, &model);

  LinearConstraintManager manager(&model);

  {
    LinearConstraintBuilder ct(IntegerValue(0), IntegerValue(11));
    ct.AddTerm(x, IntegerValue(3));
    ct.AddTerm(y, IntegerValue(-4));
    ct.AddTerm(z, IntegerValue(7));
    manager.Add(ct.Build());
  }

  const LinearConstraintManager::ConstraintIndex index(0);
  EXPECT_EQ("0 <= 3*X0 -4*X1 7*X2 <= 11",
            manager.AllConstraints()[index].constraint.DebugString());

  // ChangeLp will trigger the simplification.
  EXPECT_TRUE(model.GetOrCreate<IntegerTrail>()->Enqueue(
      IntegerLiteral::GreaterOrEqual(y, IntegerValue(5)), {}, {}));
  glop::BasisState state;
  EXPECT_TRUE(manager.ChangeLp(&state));
  EXPECT_EQ(1, manager.num_shortened_constraints());
  EXPECT_EQ("20 <= 3*X0 7*X2 <= 31",
            manager.AllConstraints()[index].constraint.DebugString());

  // We also test that the constraint equivalence work with the change.
  // Adding a constraint equiv to the new one is detected.
  {
    LinearConstraintBuilder ct(IntegerValue(0), IntegerValue(21));
    ct.AddTerm(x, IntegerValue(3));
    ct.AddTerm(z, IntegerValue(7));
    manager.Add(ct.Build());
  }
  EXPECT_EQ(manager.AllConstraints().size(), 1);
  EXPECT_EQ("20 <= 3*X0 7*X2 <= 21",
            manager.AllConstraints()[index].constraint.DebugString());
}

TEST(LinearConstraintManagerTest, SimplificationStrenghtenUb) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  LinearConstraintManager manager(&model);

  LinearConstraintBuilder ct(IntegerValue(-100), IntegerValue(30 + 70 - 5));
  ct.AddTerm(x, IntegerValue(3));
  ct.AddTerm(y, IntegerValue(-8));
  ct.AddTerm(z, IntegerValue(7));
  manager.Add(ct.Build());

  const LinearConstraintManager::ConstraintIndex index(0);
  EXPECT_EQ(2, manager.num_coeff_strenghtening());
  EXPECT_THAT(manager.AllConstraints()[index].constraint.DebugString(),
              EndsWith("3*X0 -5*X1 5*X2 <= 75"));
}

TEST(LinearConstraintManagerTest, SimplificationStrenghtenLb) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  LinearConstraintManager manager(&model);

  LinearConstraintBuilder ct(IntegerValue(-75), IntegerValue(1000));
  ct.AddTerm(x, IntegerValue(3));
  ct.AddTerm(y, IntegerValue(-8));
  ct.AddTerm(z, IntegerValue(7));
  manager.Add(ct.Build());

  const LinearConstraintManager::ConstraintIndex index(0);
  EXPECT_EQ(2, manager.num_coeff_strenghtening());
  EXPECT_THAT(manager.AllConstraints()[index].constraint.DebugString(),
              StartsWith("-45 <= 3*X0 -5*X1 5*X2"));
}

TEST(LinearConstraintManagerTest, AdvancedStrenghtening1) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  LinearConstraintManager manager(&model);

  LinearConstraintBuilder ct(IntegerValue(16), IntegerValue(1000));
  ct.AddTerm(x, IntegerValue(15));
  ct.AddTerm(y, IntegerValue(9));
  ct.AddTerm(z, IntegerValue(14));
  manager.Add(ct.Build());

  const LinearConstraintManager::ConstraintIndex index(0);
  EXPECT_EQ(3, manager.num_coeff_strenghtening());
  EXPECT_THAT(manager.AllConstraints()[index].constraint.DebugString(),
              StartsWith("2 <= 1*X0 1*X1 1*X2"));
}

TEST(LinearConstraintManagerTest, AdvancedStrenghtening2) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  LinearConstraintManager manager(&model);

  LinearConstraintBuilder ct(IntegerValue(16), IntegerValue(1000));
  ct.AddTerm(x, IntegerValue(15));
  ct.AddTerm(y, IntegerValue(7));
  ct.AddTerm(z, IntegerValue(14));
  manager.Add(ct.Build());

  const LinearConstraintManager::ConstraintIndex index(0);
  EXPECT_EQ(2, manager.num_coeff_strenghtening());
  EXPECT_THAT(manager.AllConstraints()[index].constraint.DebugString(),
              StartsWith("16 <= 9*X0 7*X1 9*X2"));
}

TEST(LinearConstraintManagerTest, AdvancedStrenghtening3) {
  Model model;
  const IntegerVariable x = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable y = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable z = model.Add(NewIntegerVariable(0, 10));
  LinearConstraintManager manager(&model);

  LinearConstraintBuilder ct(IntegerValue(5), IntegerValue(1000));
  ct.AddTerm(x, IntegerValue(5));
  ct.AddTerm(y, IntegerValue(5));
  ct.AddTerm(z, IntegerValue(4));
  manager.Add(ct.Build());

  // TODO(user): Technically, because the 5 are "enforcement" the inner
  // constraint is 4*X2 >= 5 which can be rewriten and X2 >= 2, and we could
  // instead have 2X0 + 2X1 + X2 >= 2 which should be tighter.
  const LinearConstraintManager::ConstraintIndex index(0);
  EXPECT_EQ(1, manager.num_coeff_strenghtening());
  EXPECT_THAT(manager.AllConstraints()[index].constraint.DebugString(),
              StartsWith("5 <= 5*X0 5*X1 3*X2"));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
