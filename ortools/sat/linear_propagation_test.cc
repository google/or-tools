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

#include "ortools/sat/linear_propagation.h"

#include <stdint.h>

#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

TEST(EnforcementPropagatorTest, BasicTest) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* propag = model.GetOrCreate<EnforcementPropagator>();
  sat_solver->SetNumVariables(10);

  const EnforcementId id1 = propag->Register(Literals({+1}));
  const EnforcementId id2 = propag->Register(Literals({+1, +2}));
  const EnforcementId id3 = propag->Register(Literals({-2}));

  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::CAN_PROPAGATE);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CANNOT_PROPAGATE);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE);

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+1));
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CAN_PROPAGATE);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE);

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+2));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::IS_FALSE);

  CHECK(sat_solver->ResetToLevelZero());
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::CAN_PROPAGATE);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CANNOT_PROPAGATE);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE);
}

TEST(EnforcementPropagatorTest, UntrailWork) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* propag = model.GetOrCreate<EnforcementPropagator>();
  sat_solver->SetNumVariables(10);

  const EnforcementId id1 = propag->Register(Literals({+1}));
  const EnforcementId id2 = propag->Register(Literals({+2}));
  const EnforcementId id3 = propag->Register(Literals({+3}));

  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::CAN_PROPAGATE);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CAN_PROPAGATE);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE);

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+1));
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::CAN_PROPAGATE);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE);

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+2));
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE);
  const int level = sat_solver->CurrentDecisionLevel();

  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+3));
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::IS_ENFORCED);

  sat_solver->Backtrack(level);
  EXPECT_EQ(propag->Status(id1), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id2), EnforcementStatus::IS_ENFORCED);
  EXPECT_EQ(propag->Status(id3), EnforcementStatus::CAN_PROPAGATE);
}

TEST(EnforcementPropagatorTest, AddingAtPositiveLevelTrue) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* propag = model.GetOrCreate<EnforcementPropagator>();
  sat_solver->SetNumVariables(10);

  EXPECT_TRUE(propag->Propagate(trail));
  sat_solver->EnqueueDecisionIfNotConflicting(Literal(+1));
  EXPECT_TRUE(propag->Propagate(trail));

  const EnforcementId id = propag->Register(std::vector<Literal>{+1});
  EXPECT_EQ(propag->Status(id), EnforcementStatus::IS_ENFORCED);

  sat_solver->Backtrack(0);
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id), EnforcementStatus::CAN_PROPAGATE);
}

TEST(EnforcementPropagatorTest, AddingAtPositiveLevelFalse) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* trail = model.GetOrCreate<Trail>();
  auto* propag = model.GetOrCreate<EnforcementPropagator>();
  sat_solver->SetNumVariables(10);

  EXPECT_TRUE(propag->Propagate(trail));
  sat_solver->EnqueueDecisionIfNotConflicting(Literal(-1));
  EXPECT_TRUE(propag->Propagate(trail));

  const EnforcementId id = propag->Register(std::vector<Literal>{+1});
  EXPECT_EQ(propag->Status(id), EnforcementStatus::IS_FALSE);

  sat_solver->Backtrack(0);
  EXPECT_TRUE(propag->Propagate(trail));
  EXPECT_EQ(propag->Status(id), EnforcementStatus::CAN_PROPAGATE);
}

// TEST copied from integer_expr test with little modif to use the new propag.
IntegerVariable AddWeightedSum(const absl::Span<const IntegerVariable> vars,
                               const absl::Span<const int> coeffs,
                               Model* model) {
  IntegerVariable sum = model->Add(NewIntegerVariable(-10000, 10000));
  std::vector<IntegerValue> c;
  std::vector<IntegerVariable> v;
  for (int i = 0; i < coeffs.size(); ++i) {
    c.push_back(IntegerValue(coeffs[i]));
    v.push_back(vars[i]);
  }
  c.push_back(IntegerValue(-1));
  v.push_back(sum);

  // <= sum
  auto* propag = model->GetOrCreate<LinearPropagator>();
  propag->AddConstraint({}, v, c, IntegerValue(0));

  // >= sum
  for (IntegerValue& ref : c) ref = -ref;
  propag->AddConstraint({}, v, c, IntegerValue(0));

  return sum;
}

void AddWeightedSumLowerOrEqual(const absl::Span<const IntegerVariable> vars,
                                const absl::Span<const int> coeffs, int64_t rhs,
                                Model* model) {
  std::vector<IntegerValue> c;
  std::vector<IntegerVariable> v;
  for (int i = 0; i < coeffs.size(); ++i) {
    c.push_back(IntegerValue(coeffs[i]));
    v.push_back(vars[i]);
  }
  auto* propag = model->GetOrCreate<LinearPropagator>();
  propag->AddConstraint({}, v, c, IntegerValue(rhs));
}

void AddWeightedSumLowerOrEqualReified(
    Literal equiv, const absl::Span<const IntegerVariable> vars,
    const absl::Span<const int> coeffs, int64_t rhs, Model* model) {
  std::vector<IntegerValue> c;
  std::vector<IntegerVariable> v;
  for (int i = 0; i < coeffs.size(); ++i) {
    c.push_back(IntegerValue(coeffs[i]));
    v.push_back(vars[i]);
  }
  auto* propag = model->GetOrCreate<LinearPropagator>();
  propag->AddConstraint({equiv}, v, c, IntegerValue(rhs));

  for (IntegerValue& ref : c) ref = -ref;
  propag->AddConstraint({equiv.Negated()}, v, c, IntegerValue(-rhs) - 1);
}

// A simple macro to make the code more readable.
#define EXPECT_BOUNDS_EQ(var, lb, ub)        \
  EXPECT_EQ(model.Get(LowerBound(var)), lb); \
  EXPECT_EQ(model.Get(UpperBound(var)), ub)

TEST(WeightedSumTest, LevelZeroPropagation) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(4, 9)),
                                    model.Add(NewIntegerVariable(-7, -2)),
                                    model.Add(NewIntegerVariable(3, 8))};

  const IntegerVariable sum = AddWeightedSum(vars, {1, -2, 3}, &model);
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_EQ(model.Get(LowerBound(sum)), 4 + 2 * 2 + 3 * 3);
  EXPECT_EQ(model.Get(UpperBound(sum)), 9 + 2 * 7 + 3 * 8);

  // Setting this leave only a slack of 2.
  model.Add(LowerOrEqual(sum, 19));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(vars[0], 4, 6);    // coeff = 1, slack = 2
  EXPECT_BOUNDS_EQ(vars[1], -3, -2);  // coeff = 2, slack = 1
  EXPECT_BOUNDS_EQ(vars[2], 3, 3);    // coeff = 3, slack = 0
}

// This one used to fail before CL 139204507.
TEST(WeightedSumTest, LevelZeroPropagationWithNegativeNumbers) {
  Model model;
  std::vector<IntegerVariable> vars{model.Add(NewIntegerVariable(-5, 0)),
                                    model.Add(NewIntegerVariable(-6, 0)),
                                    model.Add(NewIntegerVariable(-4, 0))};

  const IntegerVariable sum = AddWeightedSum(vars, {3, 3, 3}, &model);
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_EQ(model.Get(LowerBound(sum)), -15 * 3);
  EXPECT_EQ(model.Get(UpperBound(sum)), 0);

  // Setting this leave only a slack of 5 which is not an exact multiple of 3.
  model.Add(LowerOrEqual(sum, -40));
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_BOUNDS_EQ(vars[0], -5, -4);
  EXPECT_BOUNDS_EQ(vars[1], -6, -5);
  EXPECT_BOUNDS_EQ(vars[2], -4, -3);
}

TEST(WeightedSumLowerOrEqualTest, UnaryRounding) {
  Model model;
  IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  const std::vector<int> coeffs = {-100};
  AddWeightedSumLowerOrEqual({var}, coeffs, -320, &model);
  EXPECT_EQ(SatSolver::FEASIBLE, model.GetOrCreate<SatSolver>()->Solve());
  EXPECT_EQ(model.Get(LowerBound(var)), 4);
}

TEST(ReifiedWeightedSumLeTest, ReifToBoundPropagation) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddWeightedSumLowerOrEqualReified(r, {var}, {1}, 6, &model);
  EXPECT_EQ(
      SatSolver::FEASIBLE,
      model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions({r}));
  EXPECT_BOUNDS_EQ(var, 4, 6);
  EXPECT_EQ(SatSolver::FEASIBLE,
            model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions(
                {r.Negated()}));
  EXPECT_BOUNDS_EQ(var, 7, 9);  // The associated literal (x <= 6) is false.
}

TEST(ReifiedWeightedSumLeTest, ReifToBoundPropagationWithNegatedCoeff) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(-9, 9));
  AddWeightedSumLowerOrEqualReified(r, {var}, {-3}, 7, &model);
  EXPECT_EQ(
      SatSolver::FEASIBLE,
      model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions({r}));
  EXPECT_BOUNDS_EQ(var, -2, 9);
  EXPECT_EQ(SatSolver::FEASIBLE,
            model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions(
                {r.Negated()}));
  EXPECT_BOUNDS_EQ(var, -9, -3);  // The associated literal (x >= -2) is false.
}

TEST(ReifiedWeightedSumGeTest, ReifToBoundPropagation) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddWeightedSumLowerOrEqualReified(r, {var}, {-1}, -6, &model);
  EXPECT_EQ(
      SatSolver::FEASIBLE,
      model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions({r}));
  EXPECT_BOUNDS_EQ(var, 6, 9);
  EXPECT_EQ(SatSolver::FEASIBLE,
            model.GetOrCreate<SatSolver>()->ResetAndSolveWithGivenAssumptions(
                {r.Negated()}));
  EXPECT_BOUNDS_EQ(var, 4, 5);
}

TEST(ReifiedWeightedSumTest, BoundToReifTrueLe) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddWeightedSumLowerOrEqualReified(r, {var}, {1}, 9, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_TRUE(model.Get(Value(r)));
}

TEST(ReifiedWeightedSumTest, BoundToReifFalseLe) {
  Model model;
  const Literal r = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var = model.Add(NewIntegerVariable(4, 9));
  AddWeightedSumLowerOrEqualReified(r, {var}, {1}, 3, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_FALSE(model.Get(Value(r)));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
