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

#include "ortools/sat/scheduling_helpers.h"

#include <vector>

#include "gtest/gtest.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

TEST(SchedulingConstraintHelperTest, PushConstantBoundWithOptionalIntervals) {
  Model model;
  auto* repo = model.GetOrCreate<IntervalsRepository>();

  const AffineExpression start(IntegerValue(0));
  const AffineExpression size(IntegerValue(10));
  const AffineExpression end(IntegerValue(10));

  Literal presence2 = Literal(model.Add(NewBooleanVariable()), true);
  IntervalVariable inter1 =
      repo->CreateInterval(start, end, size, kNoLiteralIndex, false);
  IntervalVariable inter2 =
      repo->CreateInterval(start, end, size, presence2.Index(), false);

  SchedulingConstraintHelper* helper =
      repo->GetOrCreateHelper({inter1, inter2});

  EXPECT_TRUE(helper->IncreaseStartMin(1, IntegerValue(20)));
  EXPECT_FALSE(model.Get(Value(presence2)));
}

TEST(SchedulingDemandHelperTest, EnergyInWindow) {
  Model model;

  const AffineExpression start(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size(model.Add(NewIntegerVariable(2, 4)));
  const AffineExpression end(model.Add(NewIntegerVariable(0, 10)));
  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable inter =
      repo->CreateInterval(start, end, size, kNoLiteralIndex, false);

  const AffineExpression demand(model.Add(NewIntegerVariable(2, 4)));

  SchedulingConstraintHelper* helper = repo->GetOrCreateHelper({inter});
  SchedulingDemandHelper demands_helper({demand}, helper, &model);
  demands_helper.CacheAllEnergyValues();
  EXPECT_EQ(demands_helper.EnergyMin(0), IntegerValue(4));

  const Literal alt1 = Literal(model.Add(NewBooleanVariable()), true);
  const Literal alt2 = Literal(model.Add(NewBooleanVariable()), true);
  demands_helper.OverrideDecomposedEnergies(
      {{{alt1, IntegerValue(2), IntegerValue(4)},
        {alt2, IntegerValue(4), IntegerValue(2)}}});
  demands_helper.CacheAllEnergyValues();
  EXPECT_EQ(demands_helper.EnergyMin(0), IntegerValue(8));

  EXPECT_EQ(0, demands_helper.EnergyMinInWindow(0, 8, 2));
  EXPECT_EQ(8, demands_helper.EnergyMinInWindow(0, 0, 10));
  EXPECT_EQ(0, demands_helper.EnergyMinInWindow(0, 2, 10));
  EXPECT_EQ(0, demands_helper.EnergyMinInWindow(0, 0, 8));
  EXPECT_EQ(4, demands_helper.EnergyMinInWindow(0, 0, 9));
}

TEST(SchedulingDemandHelperTest, EnergyInWindowTakeIntoAccountWindowSize) {
  Model model;

  const AffineExpression start(model.Add(NewIntegerVariable(0, 4)));
  const AffineExpression size(model.Add(NewIntegerVariable(6, 8)));
  const AffineExpression end(model.Add(NewIntegerVariable(0, 10)));
  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable inter =
      repo->CreateInterval(start, end, size, kNoLiteralIndex, false);

  const AffineExpression demand(model.Add(NewIntegerVariable(6, 8)));

  SchedulingConstraintHelper* helper = repo->GetOrCreateHelper({inter});
  SchedulingDemandHelper demands_helper({demand}, helper, &model);
  demands_helper.CacheAllEnergyValues();

  const Literal alt1 = Literal(model.Add(NewBooleanVariable()), true);
  const Literal alt2 = Literal(model.Add(NewBooleanVariable()), true);
  demands_helper.OverrideDecomposedEnergies(
      {{{alt1, IntegerValue(8), IntegerValue(6)},
        {alt2, IntegerValue(6), IntegerValue(8)}}});
  demands_helper.CacheAllEnergyValues();
  EXPECT_EQ(demands_helper.EnergyMin(0), IntegerValue(48));

  EXPECT_EQ(6, demands_helper.EnergyMinInWindow(0, 5, 6));
}

TEST(SchedulingDemandHelperTest, LinearizedDemandWithAffineExpression) {
  Model model;

  const AffineExpression start(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size(model.Add(NewIntegerVariable(2, 10)));
  const AffineExpression end(model.Add(NewIntegerVariable(0, 10)));
  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable inter =
      repo->CreateInterval(start, end, size, kNoLiteralIndex, false);

  const AffineExpression demand(
      AffineExpression(model.Add(NewIntegerVariable(2, 10)), 2, 5));

  SchedulingConstraintHelper* helper = repo->GetOrCreateHelper({inter});
  SchedulingDemandHelper demands_helper({demand}, helper, &model);
  demands_helper.CacheAllEnergyValues();

  LinearConstraintBuilder builder(&model);
  ASSERT_TRUE(demands_helper.AddLinearizedDemand(0, &builder));
  EXPECT_EQ(builder.BuildExpression().DebugString(), "2*X3 + 5");
}

TEST(SchedulingDemandHelperTest, LinearizedDemandWithDecomposedEnergy) {
  Model model;

  const AffineExpression start(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size(model.Add(NewIntegerVariable(2, 10)));
  const AffineExpression end(model.Add(NewIntegerVariable(0, 10)));
  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable inter =
      repo->CreateInterval(start, end, size, kNoLiteralIndex, false);

  const AffineExpression demand(model.Add(NewIntegerVariable(2, 10)));

  SchedulingConstraintHelper* helper = repo->GetOrCreateHelper({inter});
  SchedulingDemandHelper demands_helper({demand}, helper, &model);
  demands_helper.CacheAllEnergyValues();
  EXPECT_EQ(demands_helper.EnergyMin(0), IntegerValue(4));

  const Literal alt1 = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var1(model.Add(NewIntegerVariable(0, 1)));
  model.GetOrCreate<IntegerEncoder>()->AssociateToIntegerEqualValue(
      alt1, var1, IntegerValue(1));

  const Literal alt2 = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var2(model.Add(NewIntegerVariable(0, 1)));
  model.GetOrCreate<IntegerEncoder>()->AssociateToIntegerEqualValue(
      alt2, var2, IntegerValue(1));
  demands_helper.OverrideDecomposedEnergies(
      {{{alt1, IntegerValue(2), IntegerValue(4)},
        {alt2, IntegerValue(4), IntegerValue(2)}}});
  demands_helper.CacheAllEnergyValues();
  LinearConstraintBuilder builder(&model);
  ASSERT_TRUE(demands_helper.AddLinearizedDemand(0, &builder));
  EXPECT_EQ(builder.BuildExpression().DebugString(), "4*X4 2*X5");
}

TEST(SchedulingDemandHelperTest, FilteredDecomposedEnergy) {
  Model model;
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();

  const AffineExpression start(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size(model.Add(NewIntegerVariable(2, 10)));
  const AffineExpression end(model.Add(NewIntegerVariable(0, 10)));
  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable inter =
      repo->CreateInterval(start, end, size, kNoLiteralIndex, false);

  const AffineExpression demand(model.Add(NewIntegerVariable(2, 10)));

  SchedulingConstraintHelper* helper = repo->GetOrCreateHelper({inter});
  SchedulingDemandHelper demands_helper({demand}, helper, &model);
  demands_helper.CacheAllEnergyValues();
  EXPECT_EQ(demands_helper.EnergyMin(0), IntegerValue(4));

  const std::vector<LiteralValueValue> no_energy;
  EXPECT_EQ(demands_helper.FilteredDecomposedEnergy(0), no_energy);

  const Literal alt1 = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var1(model.Add(NewIntegerVariable(0, 1)));
  encoder->AssociateToIntegerEqualValue(alt1, var1, IntegerValue(1));

  const Literal alt2 = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var2(model.Add(NewIntegerVariable(0, 1)));
  encoder->AssociateToIntegerEqualValue(alt2, var2, IntegerValue(1));
  const std::vector<LiteralValueValue> energy = {
      {alt1, IntegerValue(2), IntegerValue(4)},
      {alt2, IntegerValue(4), IntegerValue(2)}};
  demands_helper.OverrideDecomposedEnergies({energy});
  demands_helper.CacheAllEnergyValues();
  EXPECT_EQ(demands_helper.FilteredDecomposedEnergy(0), energy);

  EXPECT_EQ(sat_solver->EnqueueDecisionAndBackjumpOnConflict(alt1.Negated()),
            0);
  const std::vector<LiteralValueValue> filtered_energy = {
      {alt2, IntegerValue(4), IntegerValue(2)}};
  EXPECT_EQ(demands_helper.FilteredDecomposedEnergy(0), filtered_energy);
  EXPECT_EQ(demands_helper.DecomposedEnergies()[0], energy);
}

TEST(SchedulingDemandHelperTest, FilteredDecomposedEnergyWithFalseLiteral) {
  Model model;
  IntegerEncoder* encoder = model.GetOrCreate<IntegerEncoder>();

  const AffineExpression start(model.Add(NewIntegerVariable(0, 10)));
  const AffineExpression size(model.Add(NewIntegerVariable(2, 10)));
  const AffineExpression end(model.Add(NewIntegerVariable(0, 10)));
  auto* repo = model.GetOrCreate<IntervalsRepository>();
  const IntervalVariable inter =
      repo->CreateInterval(start, end, size, kNoLiteralIndex, false);

  const AffineExpression demand(model.Add(NewIntegerVariable(2, 10)));

  SchedulingConstraintHelper* helper = repo->GetOrCreateHelper({inter});
  SchedulingDemandHelper demands_helper({demand}, helper, &model);
  demands_helper.CacheAllEnergyValues();
  EXPECT_EQ(demands_helper.EnergyMin(0), IntegerValue(4));

  const std::vector<LiteralValueValue> no_energy;
  EXPECT_EQ(demands_helper.FilteredDecomposedEnergy(0), no_energy);

  const Literal alt1 = encoder->GetFalseLiteral();
  const IntegerVariable var1(model.Add(NewIntegerVariable(0, 1)));
  model.GetOrCreate<IntegerEncoder>()->AssociateToIntegerEqualValue(
      alt1, var1, IntegerValue(1));

  const Literal alt2 = Literal(model.Add(NewBooleanVariable()), true);
  const IntegerVariable var2(model.Add(NewIntegerVariable(0, 1)));
  encoder->AssociateToIntegerEqualValue(alt2, var2, IntegerValue(1));
  const std::vector<LiteralValueValue> energy = {
      {alt1, IntegerValue(2), IntegerValue(4)},
      {alt2, IntegerValue(4), IntegerValue(2)}};
  demands_helper.OverrideDecomposedEnergies({energy});
  demands_helper.CacheAllEnergyValues();
  const std::vector<LiteralValueValue> filtered_energy = {
      {alt2, IntegerValue(4), IntegerValue(2)}};
  EXPECT_EQ(demands_helper.DecomposedEnergies()[0], filtered_energy);
  EXPECT_EQ(demands_helper.FilteredDecomposedEnergy(0), filtered_energy);
  EXPECT_EQ(0, model.GetOrCreate<SatSolver>()->CurrentDecisionLevel());
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
