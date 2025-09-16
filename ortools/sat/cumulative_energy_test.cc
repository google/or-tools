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

#include "ortools/sat/cumulative_energy.h"

#include <stdint.h>

#include <algorithm>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/sat/2d_orthogonal_packing_testing.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/diffn_util.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_helpers.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

// An instance is a set of energy tasks and a capacity.
struct EnergyTask {
  int start_min;
  int end_max;
  int energy_min;
  int energy_max;
  int duration_min;
  int duration_max;
  bool is_optional;
};

struct EnergyInstance {
  std::vector<EnergyTask> tasks;
  int capacity;
};

std::string InstanceDebugString(const EnergyInstance& instance) {
  std::string result;
  absl::StrAppend(&result, "Instance capacity:", instance.capacity, "\n");
  for (const EnergyTask& task : instance.tasks) {
    absl::StrAppend(&result, "[", task.start_min, ", ", task.end_max,
                    "] duration:", task.duration_min, "..", task.duration_max,
                    " energy:", task.energy_min, "..", task.energy_max,
                    " is_optional:", task.is_optional, "\n");
  }
  return result;
}

// Satisfiability using the constraint.
bool SolveUsingConstraint(const EnergyInstance& instance) {
  Model model;
  std::vector<IntervalVariable> intervals;
  std::vector<std::vector<LiteralValueValue>> decomposed_energies;
  for (const auto& task : instance.tasks) {
    if (task.is_optional) {
      const Literal is_present = Literal(model.Add(NewBooleanVariable()), true);
      const IntegerVariable start =
          model.Add(NewIntegerVariable(task.start_min, task.end_max));
      const IntegerVariable end =
          model.Add(NewIntegerVariable(task.start_min, task.end_max));
      const IntegerVariable duration =
          model.Add(NewIntegerVariable(task.duration_min, task.duration_max));
      intervals.push_back(
          model.Add(NewOptionalInterval(start, end, duration, is_present)));
    } else {
      intervals.push_back(model.Add(NewIntervalWithVariableSize(
          task.start_min, task.end_max, task.duration_min, task.duration_max)));
    }
    std::vector<Literal> energy_literals;
    std::vector<LiteralValueValue> energy_literals_values_values;
    for (int e = task.energy_min; e <= task.energy_max; ++e) {
      const Literal lit = Literal(model.Add(NewBooleanVariable()), true);
      energy_literals.push_back(lit);
      energy_literals_values_values.push_back({lit, e, 1});
    }
    model.Add(ExactlyOneConstraint(energy_literals));
    decomposed_energies.push_back(energy_literals_values_values);
  }

  const AffineExpression capacity(
      model.Add(ConstantIntegerVariable(instance.capacity)));

  auto* repo = model.GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* helper =
      repo->GetOrCreateHelper(/*enforcement_literals=*/{}, intervals);
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({}, helper, &model);
  demands_helper->OverrideDecomposedEnergies(decomposed_energies);
  model.TakeOwnership(demands_helper);

  AddCumulativeOverloadChecker(capacity, helper, demands_helper, &model);

  return SolveIntegerProblemWithLazyEncoding(&model) ==
         SatSolver::Status::FEASIBLE;
}

// One task by itself is infeasible.
TEST(CumulativeEnergyTest, UnfeasibleFixedCharacteristics) {
  EnergyInstance instance = {{{0, 100, 11, 11, 2, 2, false}}, 5};
  EXPECT_FALSE(SolveUsingConstraint(instance)) << InstanceDebugString(instance);
}

// Tasks are feasible iff all are at energy min.
TEST(CumulativeEnergyTest, FeasibleEnergyMin) {
  EnergyInstance instance = {{
                                 {-10, 10, 10, 15, 0, 20, false},
                                 {-10, 10, 15, 20, 0, 20, false},
                                 {-10, 10, 5, 10, 0, 20, false},
                             },
                             3};
  EXPECT_TRUE(SolveUsingConstraint(instance)) << InstanceDebugString(instance);
}

// Tasks are feasible iff optionals tasks are removed.
TEST(CumulativeEnergyTest, FeasibleRemoveOptionals) {
  EnergyInstance instance = {{
                                 {-10, 10, 1, 1, 1, 1, true},
                                 {-10, 10, 5, 10, 7, 7, true},
                                 {-10, 10, 10, 15, 0, 20, false},
                                 {-10, 10, 15, 20, 0, 20, false},
                                 {-10, 10, 5, 10, 0, 20, false},
                             },
                             3};
  EXPECT_TRUE(SolveUsingConstraint(instance)) << InstanceDebugString(instance);
}

// This instance was problematic.
TEST(CumulativeEnergyTest, Problematic1) {
  EnergyInstance instance = {{
                                 {2, 18, 6, 7, 5, 10, false},
                                 {2, 25, 6, 9, 14, 17, false},
                                 {-4, 19, 6, 9, 10, 20, false},
                                 {-9, 7, 6, 15, 9, 16, false},
                                 {-1, 19, 6, 12, 6, 14, false},
                             },
                             1};
  EXPECT_TRUE(SolveUsingConstraint(instance)) << InstanceDebugString(instance);
}

// Satisfiability using a naive model: one task per unit of energy.
// Force energy-based reasoning in Cumulative() and add symmetry breaking,
// or the solver has a much harder time.
bool SolveUsingNaiveModel(const EnergyInstance& instance) {
  Model model;
  std::vector<IntervalVariable> intervals;
  std::vector<AffineExpression> consumptions;
  IntegerVariable one = model.Add(ConstantIntegerVariable(1));
  auto* intervals_repository = model.GetOrCreate<IntervalsRepository>();
  auto* precedences = model.GetOrCreate<PrecedencesPropagator>();

  for (const auto& task : instance.tasks) {
    if (task.is_optional) {
      const Literal is_present = Literal(model.Add(NewBooleanVariable()), true);
      for (int i = 0; i < task.energy_min; i++) {
        const IntegerVariable start =
            model.Add(NewIntegerVariable(task.start_min, task.end_max));
        const IntegerVariable end =
            model.Add(NewIntegerVariable(task.start_min, task.end_max));

        intervals.push_back(
            model.Add(NewOptionalInterval(start, end, one, is_present)));
        consumptions.push_back(AffineExpression(IntegerValue(1)));
      }
    } else {
      IntegerVariable first_start = kNoIntegerVariable;
      IntegerVariable previous_start = kNoIntegerVariable;
      for (int i = 0; i < task.energy_min; i++) {
        IntervalVariable interval =
            model.Add(NewInterval(task.start_min, task.end_max, 1));
        intervals.push_back(interval);
        consumptions.push_back(AffineExpression(IntegerValue(1)));
        const AffineExpression start_expr =
            intervals_repository->Start(interval);
        CHECK_EQ(start_expr.coeff, 1);
        CHECK_EQ(start_expr.constant, 0);
        CHECK_NE(start_expr.var, kNoIntegerVariable);
        const IntegerVariable start = start_expr.var;
        if (previous_start != kNoIntegerVariable) {
          precedences->AddPrecedence(previous_start, start);
        } else {
          first_start = start;
        }
        previous_start = start;
      }
      // start[last] <= start[0] + duration_max - 1
      if (previous_start != kNoIntegerVariable) {
        precedences->AddPrecedenceWithOffset(previous_start, first_start,
                                             -task.duration_max + 1);
      }
    }
  }

  SatParameters params =
      model.Add(NewSatParameters("use_overload_checker_in_cumulative:true"));
  model.Add(Cumulative(/*enforcement_literals=*/{}, intervals, consumptions,
                       AffineExpression(IntegerValue(instance.capacity))));

  return SolveIntegerProblemWithLazyEncoding(&model) ==
         SatSolver::Status::FEASIBLE;
}

// Generates random instances, fill the schedule to try and make a tricky case.
EnergyInstance GenerateRandomInstance(int num_tasks,
                                      absl::BitGenRef randomizer) {
  const int capacity = absl::Uniform(randomizer, 1, 12);
  std::vector<EnergyTask> tasks;
  for (int i = 0; i < num_tasks; i++) {
    int start_min = absl::Uniform(randomizer, -10, 10);
    int duration_min = absl::Uniform(randomizer, 1, 21);
    int duration_max = absl::Uniform(randomizer, 1, 21);
    if (duration_min > duration_max) std::swap(duration_min, duration_max);
    int end_max = start_min + duration_max + absl::Uniform(randomizer, 0, 10);
    int energy_min = (capacity * 30) / num_tasks;
    int energy_max = energy_min + absl::Uniform(randomizer, 1, 10);
    tasks.push_back({start_min, end_max, energy_min, energy_max, duration_min,
                     duration_max, false});
  }

  return {tasks, capacity};
}

// Compare constraint to naive model.
TEST(CumulativeEnergyTest, CompareToNaiveModel) {
  const int num_tests = 10;
  std::mt19937 randomizer(12345);
  for (int test = 0; test < num_tests; test++) {
    EnergyInstance instance =
        GenerateRandomInstance(absl::Uniform(randomizer, 4, 7), randomizer);
    bool result_constraint = SolveUsingConstraint(instance);
    bool result_naive = SolveUsingNaiveModel(instance);
    EXPECT_EQ(result_naive, result_constraint) << InstanceDebugString(instance);
    LOG(INFO) << result_constraint;
  }
}

struct CumulativeTasks {
  int64_t duration;
  int64_t demand;
  int64_t min_start;
  int64_t max_end;
};

enum class PropagatorChoice {
  OVERLOAD,
  OVERLOAD_DFF,
};
bool TestOverloadCheckerPropagation(
    absl::Span<const CumulativeTasks> tasks, int capacity_min_before,
    int capacity_min_after, int capacity_max,
    PropagatorChoice propagator_choice = PropagatorChoice::OVERLOAD) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* precedences =
      model.GetOrCreate<PrecedencesPropagator>();

  const int num_tasks = tasks.size();
  std::vector<IntervalVariable> interval_vars(num_tasks);
  std::vector<AffineExpression> demands(num_tasks);
  const AffineExpression capacity =
      AffineExpression(integer_trail->AddIntegerVariable(
          IntegerValue(capacity_min_before), IntegerValue(capacity_max)));

  // Build the task variables.
  for (int t = 0; t < num_tasks; ++t) {
    interval_vars[t] = model.Add(
        NewInterval(tasks[t].min_start, tasks[t].max_end, tasks[t].duration));
    demands[t] = AffineExpression(IntegerValue(tasks[t].demand));
  }

  // Propagate properly the other bounds of the intervals.
  EXPECT_TRUE(precedences->Propagate());

  // Propagator responsible for filtering the capacity variable.
  auto* repo = model.GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* helper =
      repo->GetOrCreateHelper(/*enforcement_literals=*/{}, interval_vars);
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper(demands, helper, &model);
  model.TakeOwnership(demands_helper);

  if (propagator_choice == PropagatorChoice::OVERLOAD) {
    AddCumulativeOverloadChecker(capacity, helper, demands_helper, &model);
  } else if (propagator_choice == PropagatorChoice::OVERLOAD_DFF) {
    AddCumulativeOverloadCheckerDff(capacity, helper, demands_helper, &model);
  } else {
    LOG(FATAL) << "Unknown propagator choice!";
  }

  // Check initial satisfiability.
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  if (!sat_solver->Propagate()) return false;

  // Check capacity.
  EXPECT_EQ(capacity_min_after, integer_trail->LowerBound(capacity));
  return true;
}

// This is a trivially infeasible instance.
TEST(OverloadCheckerTest, UNSAT1) {
  EXPECT_FALSE(
      TestOverloadCheckerPropagation({{4, 2, 0, 7}, {4, 2, 0, 7}}, 2, 2, 2));
}

// This is an infeasible instance on which timetabling finds nothing. The
// overload checker finds the contradiction.
TEST(OverloadCheckerTest, UNSAT2) {
  EXPECT_FALSE(TestOverloadCheckerPropagation(
      {{4, 2, 0, 8}, {4, 2, 0, 8}, {4, 2, 0, 8}}, 2, 2, 2));
}

// This is the same instance as in UNSAT1 but here the capacity can increase.
TEST(OverloadCheckerTest, IncreaseCapa1) {
  EXPECT_TRUE(
      TestOverloadCheckerPropagation({{4, 2, 2, 9}, {4, 2, 2, 9}}, 2, 3, 10));
}

// This is an instance in which tasks can be perfectly packed in a rectangle of
// size 5 to 6. OverloadChecker increases the capacity from 3 to 5.
TEST(OverloadCheckerTest, IncreaseCapa2) {
  EXPECT_TRUE(TestOverloadCheckerPropagation({{5, 2, 2, 8},
                                              {2, 3, 2, 8},
                                              {2, 1, 2, 8},
                                              {1, 3, 2, 8},
                                              {1, 3, 2, 8},
                                              {3, 2, 2, 8}},
                                             3, 5, 10));
}

// This is an instance in which OverloadChecker increases the capacity.
TEST(OverloadCheckerTest, IncreaseCapa3) {
  EXPECT_TRUE(TestOverloadCheckerPropagation(
      {{1, 3, 3, 6}, {1, 3, 3, 6}, {1, 1, 3, 8}}, 0, 2, 10));
}

// This is a trivially infeasible instance with negative times.
TEST(OverloadCheckerTest, UNSATNeg1) {
  EXPECT_FALSE(
      TestOverloadCheckerPropagation({{4, 2, -7, 0}, {4, 2, -7, 0}}, 2, 2, 2));
}

// This is an infeasible instance with negative times on which timetabling finds
// nothing. The overload checker finds the contradiction.
TEST(OverloadCheckerTest, UNSATNeg2) {
  EXPECT_FALSE(TestOverloadCheckerPropagation(
      {{4, 2, -4, 4}, {4, 2, -4, 4}, {4, 2, -4, 4}}, 2, 2, 2));
}

// This is the same instance as in UNSATNeg1 but here the capacity can increase.
TEST(OverloadCheckerTest, IncreaseCapaNeg1) {
  EXPECT_TRUE(TestOverloadCheckerPropagation({{4, 2, -10, -3}, {4, 2, -10, -3}},
                                             2, 3, 10));
}

// This is an instance with negative times in which tasks can be perfectly
// packed in a rectangle of size 5 to 6. OverloadChecker increases the capacity
// from 3 to 5.
TEST(OverloadCheckerTest, IncreaseCapaNeg2) {
  EXPECT_TRUE(TestOverloadCheckerPropagation({{5, 2, -2, 4},
                                              {2, 3, -2, 4},
                                              {2, 1, -2, 4},
                                              {1, 3, -2, 4},
                                              {1, 3, -2, 4},
                                              {3, 2, -2, 4}},
                                             3, 5, 10));
}

// This is an instance with negative times in which OverloadChecker increases
// the capacity.
TEST(OverloadCheckerTest, IncreaseCapaNeg3) {
  EXPECT_TRUE(TestOverloadCheckerPropagation(
      {{1, 3, -3, 0}, {1, 3, -3, 0}, {1, 1, -3, 2}}, 0, 2, 10));
}

TEST(OverloadCheckerTest, OptionalTaskPropagatedToAbsent) {
  Model model;
  const Literal is_present = Literal(model.Add(NewBooleanVariable()), true);

  // TODO(user): Fix the code! the propagation is dependent on the order of
  // tasks. If we use the proper theta-lambda tree, this will be fixed.
  const IntervalVariable i2 = model.Add(NewInterval(0, 10, /*size=*/8));
  const IntervalVariable i1 =
      model.Add(NewOptionalInterval(0, 10, /*size=*/8, is_present));

  auto* repo = model.GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* helper =
      repo->GetOrCreateHelper(/*enforcement_literals=*/{}, {i1, i2});
  const AffineExpression cte(IntegerValue(2));
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({cte, cte}, helper, &model);
  model.TakeOwnership(demands_helper);

  AddCumulativeOverloadChecker(cte, helper, demands_helper, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_FALSE(model.Get(Value(is_present)));
}

TEST(OverloadCheckerTest, OptionalTaskMissedPropagationCase) {
  Model model;
  const Literal is_present = Literal(model.Add(NewBooleanVariable()), true);
  const IntervalVariable i1 =
      model.Add(NewOptionalInterval(0, 10, /*size=*/8, is_present));
  const IntervalVariable i2 =
      model.Add(NewOptionalInterval(0, 10, /*size=*/8, is_present));

  auto* repo = model.GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* helper =
      repo->GetOrCreateHelper(/*enforcement_literals=*/{}, {i1, i2});
  const AffineExpression cte(IntegerValue(2));
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper({cte, cte}, helper, &model);
  model.TakeOwnership(demands_helper);

  AddCumulativeOverloadChecker(cte, helper, demands_helper, &model);
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_FALSE(model.GetOrCreate<Trail>()->Assignment().VariableIsAssigned(
      is_present.Variable()));
}

TEST(OverloadCheckerDffTest, DffIsNeeded) {
  const std::vector<CumulativeTasks> tasks = {
      {.duration = 10, .demand = 5, .min_start = 0, .max_end = 22},
      {.duration = 10, .demand = 5, .min_start = 0, .max_end = 22},
      {.duration = 10, .demand = 5, .min_start = 0, .max_end = 22},
      {.duration = 10, .demand = 5, .min_start = 0, .max_end = 22},
  };
  EXPECT_FALSE(TestOverloadCheckerPropagation(tasks, /*capacity_min_before=*/9,
                                              /*capacity_min_after=*/9,
                                              /*capacity_max=*/9,
                                              PropagatorChoice::OVERLOAD_DFF));
}

TEST(OverloadCheckerDffTest, NoConflictRandomFeasibleProblem) {
  absl::BitGen random;
  for (int i = 0; i < 100; ++i) {
    const std::vector<Rectangle> rectangles = GenerateNonConflictingRectangles(
        absl::Uniform<int>(random, 6, 20), random);
    Rectangle bounding_box;
    for (const auto& item : rectangles) {
      bounding_box.x_min = std::min(bounding_box.x_min, item.x_min);
      bounding_box.x_max = std::max(bounding_box.x_max, item.x_max);
      bounding_box.y_min = std::min(bounding_box.y_min, item.y_min);
      bounding_box.y_max = std::max(bounding_box.y_max, item.y_max);
    }
    const std::vector<RectangleInRange> range_items =
        MakeItemsFromRectangles(rectangles, 0.3, random);
    std::vector<CumulativeTasks> tasks(range_items.size());

    for (int i = 0; i < range_items.size(); ++i) {
      tasks[i] = {.duration = range_items[i].x_size.value(),
                  .demand = range_items[i].y_size.value(),
                  .min_start = range_items[i].bounding_area.x_min.value(),
                  .max_end = range_items[i].bounding_area.x_max.value()};
    }
    EXPECT_TRUE(TestOverloadCheckerPropagation(
        tasks, /*capacity_min_before=*/bounding_box.SizeY().value(),
        /*capacity_min_after=*/bounding_box.SizeY().value(),
        /*capacity_max=*/bounding_box.SizeY().value(),
        PropagatorChoice::OVERLOAD_DFF));
  }
}

bool TestIsAfterCumulative(absl::Span<const CumulativeTasks> tasks,
                           int capacity_max, int expected_end_min) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* precedences =
      model.GetOrCreate<PrecedencesPropagator>();

  const int num_tasks = tasks.size();
  std::vector<IntervalVariable> interval_vars(num_tasks);
  std::vector<AffineExpression> demands(num_tasks);
  const AffineExpression capacity =
      AffineExpression(integer_trail->AddIntegerVariable(
          IntegerValue(capacity_max), IntegerValue(capacity_max)));

  // Build the task variables.
  std::vector<int> subtasks;
  for (int t = 0; t < num_tasks; ++t) {
    interval_vars[t] = model.Add(
        NewInterval(tasks[t].min_start, tasks[t].max_end, tasks[t].duration));
    demands[t] = AffineExpression(IntegerValue(tasks[t].demand));
    subtasks.push_back(t);
  }

  // Propagate properly the other bounds of the intervals.
  EXPECT_TRUE(precedences->Propagate());

  // Propagator responsible for filtering the capacity variable.
  auto* repo = model.GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* helper =
      repo->GetOrCreateHelper(/*enforcement_literals=*/{}, interval_vars);
  SchedulingDemandHelper* demands_helper =
      new SchedulingDemandHelper(demands, helper, &model);
  model.TakeOwnership(demands_helper);

  const IntegerVariable var =
      integer_trail->AddIntegerVariable(IntegerValue(0), IntegerValue(100));

  std::vector<IntegerValue> offsets(subtasks.size(), IntegerValue(0));
  CumulativeIsAfterSubsetConstraint* propag =
      new CumulativeIsAfterSubsetConstraint(var, capacity, subtasks, offsets,
                                            helper, demands_helper, &model);
  propag->RegisterWith(model.GetOrCreate<GenericLiteralWatcher>());
  model.TakeOwnership(propag);

  // Check initial satisfiability.
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  if (!sat_solver->Propagate()) return false;

  // Check bound
  EXPECT_EQ(expected_end_min, integer_trail->LowerBound(var));
  return true;
}

// We detect that the interval cannot overlap.
TEST(IsAfterCumulativeTest, BasicCase1) {
  // duration, demand, start_min, end_max
  EXPECT_TRUE(TestIsAfterCumulative({{4, 2, 0, 8}, {4, 2, 0, 10}},
                                    /*capacity_max=*/3,
                                    /*expected_end_min=*/8));
}

// Now, one interval can overlap. It is also after the other, so the best bound
// we get is not that great: energy = 2 + 8 + 8 = 18, with capa = 3, we get 6.
//
// TODO(user): Maybe we can do more advanced reasoning to recover the 8 here.
TEST(IsAfterCumulativeTest, BasicCase2) {
  // duration, demand, start_min, end_max.
  EXPECT_TRUE(TestIsAfterCumulative({{2, 1, 3, 8}, {4, 2, 0, 8}, {4, 2, 0, 10}},
                                    /*capacity_max=*/3,
                                    /*expected_end_min=*/6));
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
