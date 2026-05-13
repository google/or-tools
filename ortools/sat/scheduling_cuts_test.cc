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

#include "ortools/sat/scheduling_cuts.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "absl/base/log_severity.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cuts.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/linear_constraint_manager.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::EndsWith;
using ::testing::StartsWith;

TEST(CumulativeEnergyCutGenerator, TestCutTimeTableGenerator) {
  Model model;

  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 3));
  const IntegerVariable end1 = model.Add(NewIntegerVariable(7, 10));
  const IntegerVariable size1 = model.Add(NewIntegerVariable(7, 7));
  const IntervalVariable i1 = model.Add(NewInterval(start1, end1, size1));

  const BooleanVariable b = model.Add(NewBooleanVariable());
  const IntegerVariable b_view = model.Add(NewIntegerVariable(0, 1));
  auto* integer_encoder = model.GetOrCreate<IntegerEncoder>();
  integer_encoder->AssociateToIntegerEqualValue(Literal(b, true), b_view,
                                                IntegerValue(1));

  const IntegerVariable start2 = model.Add(NewIntegerVariable(3, 6));
  const IntegerVariable end2 = model.Add(NewIntegerVariable(10, 13));
  const IntegerVariable size2 = model.Add(NewIntegerVariable(7, 7));
  const IntervalVariable i2 =
      model.Add(NewOptionalInterval(start2, end2, size2, Literal(b, true)));

  const IntegerVariable demand1 = model.Add(NewIntegerVariable(5, 10));
  const IntegerVariable demand2 = model.Add(NewIntegerVariable(3, 10));
  const IntegerVariable capacity = model.Add(NewIntegerVariable(10, 10));
  SchedulingConstraintHelper* helper =
      model.GetOrCreate<IntervalsRepository>()->GetOrCreateHelper({i1, i2});
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({demand1, demand2}, helper, &model);
  model.TakeOwnership(demands_helper);
  CutGenerator cumulative = CreateCumulativeTimeTableCutGenerator(
      helper, demands_helper, capacity, &model);
  LinearConstraintManager* const manager =
      model.GetOrCreate<LinearConstraintManager>();
  const IntegerVariable num_vars =
      model.GetOrCreate<IntegerTrail>()->NumIntegerVariables();

  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(num_vars.value() * 2, 0.0);
  lp_values[start1] = 3.0;     // x0
  lp_values[end1] = 10.0;      // x1
  lp_values[size1] = 7.0;      // x2
  lp_values[b_view] = 1.0;     // x3
  lp_values[start2] = 6.0;     // x4
  lp_values[end2] = 13.0;      // x5
  lp_values[size2] = 7.0;      // x6
  lp_values[demand1] = 8.0;    // x7
  lp_values[demand2] = 7.0;    // x8
  lp_values[capacity] = 10.0;  // x9

  cumulative.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());

  // 3*X3 1*X7 -1*X9 <= 0 -> Normalized to 3*X3 1*X7 <= 10
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              EndsWith("3*X3 1*X7 <= 10"));
}

TEST(CumulativeEnergyCutGenerator, SameDemand) {
  Model model;

  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 3));
  const IntegerVariable end1 = model.Add(NewIntegerVariable(7, 10));
  const IntegerVariable size1 = model.Add(NewIntegerVariable(7, 7));
  const IntervalVariable i1 = model.Add(NewInterval(start1, end1, size1));

  const IntegerVariable start2 = model.Add(NewIntegerVariable(3, 6));
  const IntegerVariable end2 = model.Add(NewIntegerVariable(10, 13));
  const IntegerVariable size2 = model.Add(NewIntegerVariable(7, 7));
  const IntervalVariable i2 = model.Add(NewInterval(start2, end2, size2));

  const IntegerVariable start3 = model.Add(NewIntegerVariable(4, 8));
  const IntegerVariable end3 = model.Add(NewIntegerVariable(11, 15));
  const IntegerVariable size3 = model.Add(NewIntegerVariable(7, 7));
  const IntervalVariable i3 = model.Add(NewInterval(start3, end3, size3));

  const IntegerVariable demand = model.Add(NewIntegerVariable(5, 10));
  const IntegerVariable demand2 = model.Add(NewIntegerVariable(5, 10));
  const IntegerVariable capacity = model.Add(NewIntegerVariable(10, 10));

  LinearExpression e1;
  e1.vars.push_back(demand);
  e1.coeffs.push_back(IntegerValue(7));
  LinearExpression e2;
  e2.vars.push_back(demand2);
  e2.coeffs.push_back(IntegerValue(7));

  SchedulingConstraintHelper* helper =
      model.GetOrCreate<IntervalsRepository>()->GetOrCreateHelper({i1, i2, i3});
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({demand, demand, demand2}, helper, &model);
  model.TakeOwnership(demands_helper);

  CutGenerator cumulative = CreateCumulativeEnergyCutGenerator(
      helper, demands_helper, capacity, std::optional<AffineExpression>(),
      &model);
  LinearConstraintManager* const manager =
      model.GetOrCreate<LinearConstraintManager>();
  const IntegerVariable num_vars =
      model.GetOrCreate<IntegerTrail>()->NumIntegerVariables();

  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(num_vars.value() * 2, 0.0);
  lp_values[start1] = 3.0;     // x0
  lp_values[end1] = 10.0;      // x1
  lp_values[size1] = 7.0;      // x2
  lp_values[start2] = 6.0;     // x3
  lp_values[end2] = 13.0;      // x4
  lp_values[size2] = 7.0;      // x5
  lp_values[start3] = 6.0;     // x6
  lp_values[end3] = 13.0;      // x7
  lp_values[size3] = 7.0;      // x8
  lp_values[demand] = 8.0;     // x9
  lp_values[demand2] = 8.0;    // x10
  lp_values[capacity] = 10.0;  // x11

  cumulative.generate_cuts(manager);
  ASSERT_EQ(5, manager->num_cuts());

  // CumulativeEnergy cut.
  EXPECT_THAT(
      manager->AllConstraints()[LinearConstraintManager::ConstraintIndex(0)]
          .constraint.DebugString(),
      EndsWith("1*X9 <= 5"));
  EXPECT_THAT(
      manager->AllConstraints()[LinearConstraintManager::ConstraintIndex(1)]
          .constraint.DebugString(),
      EndsWith("1*X9 1*X10 <= 10"));
  EXPECT_THAT(
      manager->AllConstraints()[LinearConstraintManager::ConstraintIndex(2)]
          .constraint.DebugString(),
      EndsWith("3*X9 2*X10 <= 30"));
  EXPECT_THAT(
      manager->AllConstraints()[LinearConstraintManager::ConstraintIndex(3)]
          .constraint.DebugString(),
      EndsWith("5*X9 2*X10 <= 40"));
  EXPECT_THAT(
      manager->AllConstraints()[LinearConstraintManager::ConstraintIndex(4)]
          .constraint.DebugString(),
      EndsWith("2*X9 3*X10 <= 30"));
}

TEST(CumulativeEnergyCutGenerator, SameDemandTimeTableGenerator) {
  Model model;

  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 3));
  const IntegerVariable end1 = model.Add(NewIntegerVariable(7, 10));
  const IntegerVariable size1 = model.Add(NewIntegerVariable(7, 7));
  const IntervalVariable i1 = model.Add(NewInterval(start1, end1, size1));

  const IntegerVariable start2 = model.Add(NewIntegerVariable(3, 6));
  const IntegerVariable end2 = model.Add(NewIntegerVariable(10, 13));
  const IntegerVariable size2 = model.Add(NewIntegerVariable(7, 7));
  const IntervalVariable i2 = model.Add(NewInterval(start2, end2, size2));

  const IntegerVariable start3 = model.Add(NewIntegerVariable(4, 8));
  const IntegerVariable end3 = model.Add(NewIntegerVariable(11, 15));
  const IntegerVariable size3 = model.Add(NewIntegerVariable(7, 7));
  const IntervalVariable i3 = model.Add(NewInterval(start3, end3, size3));

  const IntegerVariable demand = model.Add(NewIntegerVariable(5, 10));
  const IntegerVariable demand2 = model.Add(NewIntegerVariable(5, 10));
  const IntegerVariable capacity = model.Add(NewIntegerVariable(10, 10));

  SchedulingConstraintHelper* helper =
      model.GetOrCreate<IntervalsRepository>()->GetOrCreateHelper({i1, i2, i3});
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({demand, demand, demand2}, helper, &model);
  model.TakeOwnership(demands_helper);
  CutGenerator cumulative = CreateCumulativeTimeTableCutGenerator(
      helper, demands_helper, capacity, &model);
  LinearConstraintManager* const manager =
      model.GetOrCreate<LinearConstraintManager>();
  const IntegerVariable num_vars =
      model.GetOrCreate<IntegerTrail>()->NumIntegerVariables();

  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(num_vars.value() * 2, 0.0);
  lp_values[start1] = 3.0;     // x0
  lp_values[end1] = 10.0;      // x1
  lp_values[size1] = 7.0;      // x2
  lp_values[start2] = 6.0;     // x3
  lp_values[end2] = 13.0;      // x4
  lp_values[size2] = 7.0;      // x5
  lp_values[start3] = 6.0;     // x6
  lp_values[end3] = 13.0;      // x7
  lp_values[size3] = 7.0;      // x8
  lp_values[demand] = 8.0;     // x9
  lp_values[demand2] = 8.0;    // x10
  lp_values[capacity] = 10.0;  // x11

  cumulative.generate_cuts(manager);
  ASSERT_EQ(2, manager->num_cuts());

  // 1*X9 1*X9 <= X11 -> Normalized to 1*X9 <= 5
  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              EndsWith("1*X9 <= 5"));
  // 1*X9 1*X10 <= X11 -> Normalized to 1*X9 1*X10 <= 10
  EXPECT_THAT(manager->AllConstraints().back().constraint.DebugString(),
              EndsWith("1*X9 1*X10 <= 10"));
}

TEST(CumulativeEnergyCutGenerator, DetectedPrecedence) {
  Model model;
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();

  const IntegerValue one(1);
  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 3));
  const IntegerValue size1(3);
  const IntervalVariable i1 = intervals_repository->CreateInterval(
      start1, AffineExpression(start1, one, size1), AffineExpression(size1),
      kNoLiteralIndex, /*add_linear_relation=*/false);

  const IntegerVariable start2 = model.Add(NewIntegerVariable(1, 5));
  const IntegerValue size2(4);
  const IntervalVariable i2 = intervals_repository->CreateInterval(
      start2, AffineExpression(start2, one, size2), AffineExpression(size2),
      kNoLiteralIndex, /*add_linear_relation=*/false);
  CutGenerator disjunctive = CreateNoOverlapPrecedenceCutGenerator(
      intervals_repository->GetOrCreateHelper({
          i1,
          i2,
      }),
      &model);
  LinearConstraintManager* const manager =
      model.GetOrCreate<LinearConstraintManager>();
  const IntegerVariable num_vars =
      model.GetOrCreate<IntegerTrail>()->NumIntegerVariables();

  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(num_vars.value() * 2, 0.0);
  lp_values[start1] = 0.0;
  lp_values[NegationOf(start1)] = 0.0;
  lp_values[start2] = 2.0;
  lp_values[NegationOf(start2)] = -2.0;

  disjunctive.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());

  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              EndsWith("1*X0 -1*X1 <= -3"));
}

TEST(CumulativeEnergyCutGenerator, DetectedPrecedenceRev) {
  Model model;
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();

  const IntegerValue one(1);
  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 3));
  const IntegerValue size1(3);
  const IntervalVariable i1 = intervals_repository->CreateInterval(
      start1, AffineExpression(start1, one, size1), AffineExpression(size1),
      kNoLiteralIndex, /*add_linear_relation=*/false);

  const IntegerVariable start2 = model.Add(NewIntegerVariable(1, 5));
  const IntegerValue size2(4);
  const IntervalVariable i2 = intervals_repository->CreateInterval(
      start2, AffineExpression(start2, one, size2), AffineExpression(size2),
      kNoLiteralIndex, /*add_linear_relation=*/false);

  CutGenerator disjunctive = CreateNoOverlapPrecedenceCutGenerator(
      intervals_repository->GetOrCreateHelper({
          i2,
          i1,
      }),
      &model);
  LinearConstraintManager* const manager =
      model.GetOrCreate<LinearConstraintManager>();
  const IntegerVariable num_vars =
      model.GetOrCreate<IntegerTrail>()->NumIntegerVariables();

  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(num_vars.value() * 2, 0.0);
  lp_values[start1] = 0.0;
  lp_values[NegationOf(start1)] = 0.0;
  lp_values[start2] = 2.0;
  lp_values[NegationOf(start2)] = -2.0;

  disjunctive.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());

  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              EndsWith("1*X0 -1*X1 <= -3"));
}

TEST(CumulativeEnergyCutGenerator, DisjunctionOnStart) {
  Model model;
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();

  const IntegerValue one(1);
  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 5));
  const IntegerValue size1(3);
  const IntervalVariable i1 = intervals_repository->CreateInterval(
      start1, AffineExpression(start1, one, size1), AffineExpression(size1),
      kNoLiteralIndex, /*add_linear_relation=*/false);

  const IntegerVariable start2 = model.Add(NewIntegerVariable(1, 5));
  const IntegerValue size2(4);
  const IntervalVariable i2 = intervals_repository->CreateInterval(
      start2, AffineExpression(start2, one, size2), AffineExpression(size2),
      kNoLiteralIndex, /*add_linear_relation=*/false);

  CutGenerator disjunctive = CreateNoOverlapPrecedenceCutGenerator(
      intervals_repository->GetOrCreateHelper({
          i2,
          i1,
      }),
      &model);
  LinearConstraintManager* const manager =
      model.GetOrCreate<LinearConstraintManager>();
  const IntegerVariable num_vars =
      model.GetOrCreate<IntegerTrail>()->NumIntegerVariables();

  auto& lp_values = *model.GetOrCreate<ModelLpValues>();
  lp_values.resize(num_vars.value() * 2, 0.0);
  lp_values[start1] = 0.0;
  lp_values[NegationOf(start1)] = 0.0;
  lp_values[start2] = 2.0;
  lp_values[NegationOf(start2)] = -2.0;

  disjunctive.generate_cuts(manager);
  ASSERT_EQ(1, manager->num_cuts());

  EXPECT_THAT(manager->AllConstraints().front().constraint.DebugString(),
              StartsWith("15 <= 2*X0 5*X1"));
}

TEST(ComputeMinSumOfEndMinsTest, CombinationOf3) {
  Model model;
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();

  IntegerValue one(1);
  IntegerValue two(2);

  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 10));
  const IntegerValue size1(3);
  const IntervalVariable i1 = intervals_repository->CreateInterval(
      start1, AffineExpression(start1, one, size1), size1, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  const IntegerVariable start2 = model.Add(NewIntegerVariable(0, 10));
  const IntegerValue size2(4);
  const IntervalVariable i2 = intervals_repository->CreateInterval(
      start2, AffineExpression(start2, one, size2), size2, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  const IntegerVariable start3 = model.Add(NewIntegerVariable(0, 10));
  const IntegerValue size3(5);
  const IntervalVariable i3 = intervals_repository->CreateInterval(
      start3, AffineExpression(start3, one, size3), size3, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  SchedulingConstraintHelper* helper =
      model.GetOrCreate<IntervalsRepository>()->GetOrCreateHelper({i1, i2, i3});
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({two, one, one}, helper, &model);
  model.TakeOwnership(demands_helper);
  CompletionTimeEvent e1(0, helper, demands_helper);
  CompletionTimeEvent e2(1, helper, demands_helper);
  CompletionTimeEvent e3(2, helper, demands_helper);
  const std::vector<CompletionTimeEvent> events = {e1, e2, e3};

  double min_sum_of_end_mins = 0;
  double min_sum_of_weighted_end_mins = 0;
  CtExhaustiveHelper ct_helper;
  ct_helper.Init(events, &model);
  bool cut_use_precedences = false;
  int exploration_credit = 1000;
  ASSERT_EQ(ComputeMinSumOfWeightedEndMins(
                events, two, 0.01, 0.01, ct_helper, min_sum_of_end_mins,
                min_sum_of_weighted_end_mins, cut_use_precedences,
                exploration_credit),
            CompletionTimeExplorationStatus::FINISHED);
  EXPECT_EQ(min_sum_of_end_mins, 17);
  EXPECT_EQ(min_sum_of_weighted_end_mins, 86);
  EXPECT_FALSE(cut_use_precedences);
}

TEST(ComputeMinSumOfEndMinsTest, CombinationOf3ConstraintStart) {
  Model model;
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();

  IntegerValue one(1);
  IntegerValue two(2);

  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 3));
  const IntegerValue size1(3);
  const IntervalVariable i1 = intervals_repository->CreateInterval(
      start1, AffineExpression(start1, one, size1), size1, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  const IntegerVariable start2 = model.Add(NewIntegerVariable(0, 10));
  const IntegerValue size2(4);
  const IntervalVariable i2 = intervals_repository->CreateInterval(
      start2, AffineExpression(start2, one, size2), size2, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  const IntegerVariable start3 = model.Add(NewIntegerVariable(0, 10));
  const IntegerValue size3(5);
  const IntervalVariable i3 = intervals_repository->CreateInterval(
      start3, AffineExpression(start3, one, size3), size3, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  SchedulingConstraintHelper* helper =
      model.GetOrCreate<IntervalsRepository>()->GetOrCreateHelper({i1, i2, i3});
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({two, one, one}, helper, &model);
  model.TakeOwnership(demands_helper);

  CompletionTimeEvent e1(0, helper, demands_helper);
  CompletionTimeEvent e2(1, helper, demands_helper);
  CompletionTimeEvent e3(2, helper, demands_helper);
  const std::vector<CompletionTimeEvent> events = {e1, e2, e3};

  double min_sum_of_end_mins = 0;
  double min_sum_of_weighted_end_mins = 0;
  CtExhaustiveHelper ct_helper;
  ct_helper.Init(events, &model);
  bool cut_use_precedences = false;
  int exploration_credit = 1000;

  ASSERT_EQ(ComputeMinSumOfWeightedEndMins(
                events, two, 0.01, 0.01, ct_helper, min_sum_of_end_mins,
                min_sum_of_weighted_end_mins, cut_use_precedences,
                exploration_credit),
            CompletionTimeExplorationStatus::FINISHED);
  EXPECT_EQ(min_sum_of_end_mins, 18);
  EXPECT_EQ(min_sum_of_weighted_end_mins, 86);
}

TEST(ComputeMinSumOfEndMinsTest, Abort) {
  Model model;
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();

  IntegerValue one(1);
  IntegerValue two(2);

  const IntegerVariable start1 = model.Add(NewIntegerVariable(0, 3));
  const IntegerValue size1(3);
  const IntervalVariable i1 = intervals_repository->CreateInterval(
      start1, AffineExpression(start1, one, size1), size1, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  const IntegerVariable start2 = model.Add(NewIntegerVariable(0, 10));
  const IntegerValue size2(4);
  const IntervalVariable i2 = intervals_repository->CreateInterval(
      start2, AffineExpression(start2, one, size2), size2, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  const IntegerVariable start3 = model.Add(NewIntegerVariable(0, 10));
  const IntegerValue size3(5);
  const IntervalVariable i3 = intervals_repository->CreateInterval(
      start3, AffineExpression(start3, one, size3), size3, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  SchedulingConstraintHelper* helper =
      model.GetOrCreate<IntervalsRepository>()->GetOrCreateHelper({i1, i2, i3});
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({two, one, one}, helper, &model);
  model.TakeOwnership(demands_helper);

  CompletionTimeEvent e1(0, helper, demands_helper);
  CompletionTimeEvent e2(1, helper, demands_helper);
  CompletionTimeEvent e3(2, helper, demands_helper);
  const std::vector<CompletionTimeEvent> events = {e1, e2, e3};

  double min_sum_of_end_mins = 0;
  double min_sum_of_weighted_end_mins = 0;
  CtExhaustiveHelper ct_helper;
  ct_helper.Init(events, &model);
  bool cut_use_precedences = false;
  int exploration_credit = 2;

  ASSERT_EQ(ComputeMinSumOfWeightedEndMins(
                events, two, 0.01, 0.01, ct_helper, min_sum_of_end_mins,
                min_sum_of_weighted_end_mins, cut_use_precedences,
                exploration_credit),
            CompletionTimeExplorationStatus::ABORTED);
}

TEST(ComputeMinSumOfEndMinsTest, Infeasible) {
  Model model;
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();

  IntegerValue one(1);
  IntegerValue two(2);

  const IntegerVariable start1 = model.Add(NewIntegerVariable(1, 3));
  const IntegerValue size1(3);
  const IntervalVariable i1 = intervals_repository->CreateInterval(
      start1, AffineExpression(start1, one, size1), size1, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  const IntegerVariable start2 = model.Add(NewIntegerVariable(0, 3));
  const IntegerValue size2(4);
  const IntervalVariable i2 = intervals_repository->CreateInterval(
      start2, AffineExpression(start2, one, size2), size2, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  const IntegerVariable start3 = model.Add(NewIntegerVariable(0, 3));
  const IntegerValue size3(5);
  const IntervalVariable i3 = intervals_repository->CreateInterval(
      start3, AffineExpression(start3, one, size3), size3, kNoLiteralIndex,
      /*add_linear_relation=*/false);

  SchedulingConstraintHelper* helper =
      model.GetOrCreate<IntervalsRepository>()->GetOrCreateHelper({i1, i2, i3});
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({two, one, one}, helper, &model);
  model.TakeOwnership(demands_helper);

  CompletionTimeEvent e1(0, helper, demands_helper);
  CompletionTimeEvent e2(1, helper, demands_helper);
  CompletionTimeEvent e3(2, helper, demands_helper);
  const std::vector<CompletionTimeEvent> events = {e1, e2, e3};

  double min_sum_of_end_mins = 0;
  double min_sum_of_weighted_end_mins = 0;
  CtExhaustiveHelper ct_helper;
  ct_helper.Init(events, &model);
  bool cut_use_precedences = false;
  int exploration_credit = 1000;
  ASSERT_EQ(ComputeMinSumOfWeightedEndMins(
                events, two, 0.01, 0.01, ct_helper, min_sum_of_end_mins,
                min_sum_of_weighted_end_mins, cut_use_precedences,
                exploration_credit),
            CompletionTimeExplorationStatus::NO_VALID_PERMUTATION);
}

double ExactMakespan(absl::Span<const int> sizes, std::vector<int>& demands,
                     int capacity) {
  const int64_t kHorizon = 1000;
  CpModelBuilder builder;
  LinearExpr obj;
  CumulativeConstraint cumul = builder.AddCumulative(capacity);
  for (int i = 0; i < sizes.size(); ++i) {
    IntVar s = builder.NewIntVar({0, kHorizon});
    IntervalVar v = builder.NewFixedSizeIntervalVar(s, sizes[i]);
    obj += s + sizes[i];
    cumul.AddDemand(v, demands[i]);
  }
  builder.Minimize(obj);
  const CpSolverResponse response =
      SolveWithParameters(builder.Build(), "num_search_workers:8");
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  return response.objective_value();
}

double ExactMakespanBruteForce(absl::Span<const int> sizes,
                               std::vector<int>& demands, int capacity) {
  const int64_t kHorizon = 1000;
  Model model;
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();
  IntegerValue one(1);

  std::vector<IntervalVariable> intervals;
  for (int i = 0; i < sizes.size(); ++i) {
    const IntegerVariable start = model.Add(NewIntegerVariable(0, kHorizon));
    const IntegerValue size(sizes[i]);
    const IntervalVariable interval = intervals_repository->CreateInterval(
        start, AffineExpression(start, one, size), size, kNoLiteralIndex,
        /*add_linear_relation=*/false);
    intervals.push_back(interval);
  }

  SchedulingConstraintHelper* helper =
      model.GetOrCreate<IntervalsRepository>()->GetOrCreateHelper(intervals);
  std::vector<AffineExpression> demands_expr;
  for (int i = 0; i < demands.size(); ++i) {
    demands_expr.push_back(AffineExpression(demands[i]));
  }
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper(demands_expr, helper, &model);
  model.TakeOwnership(demands_helper);

  std::vector<CompletionTimeEvent> events;
  for (int i = 0; i < demands.size(); ++i) {
    CompletionTimeEvent e(i, helper, demands_helper);
    events.push_back(e);
  }

  double min_sum_of_end_mins = 0;
  double min_sum_of_weighted_end_mins = 0;
  CtExhaustiveHelper ct_helper;
  ct_helper.Init(events, &model);
  bool cut_use_precedences = false;
  int exploration_credit = 10000;
  EXPECT_EQ(ComputeMinSumOfWeightedEndMins(
                events, capacity, 0.01, 0.01, ct_helper, min_sum_of_end_mins,
                min_sum_of_weighted_end_mins, cut_use_precedences,
                exploration_credit),
            CompletionTimeExplorationStatus::FINISHED);
  return min_sum_of_end_mins;
}

TEST(ComputeMinSumOfEndMinsTest, RandomCases) {
  absl::BitGen random;
  const int kNumTests = DEBUG_MODE ? 50 : 500;
  const int kNumTasks = 7;
  for (int loop = 0; loop < kNumTests; ++loop) {
    const int capacity = absl::Uniform<int>(random, 10, 30);
    std::vector<int> sizes;
    std::vector<int> demands;
    for (int t = 0; t < kNumTasks; ++t) {
      sizes.push_back(absl::Uniform<int>(random, 2, 15));
      demands.push_back(absl::Uniform<int>(random, 1, capacity));
    }

    EXPECT_NEAR(ExactMakespan(sizes, demands, capacity),
                ExactMakespanBruteForce(sizes, demands, capacity), 1e-6);
  }
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
