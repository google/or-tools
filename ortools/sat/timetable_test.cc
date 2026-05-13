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

#include "ortools/sat/timetable.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "absl/container/btree_map.h"
#include "absl/log/check.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/sat/all_different.h"
#include "ortools/sat/cumulative.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/scheduling_helpers.h"

namespace operations_research {
namespace sat {
namespace {

struct CumulativeTasks {
  int min_duration;
  int min_demand;
  int min_start;
  int max_end;
};

struct Task {
  int min_start;
  int max_end;
};

bool TestTimeTablingPropagation(absl::Span<const CumulativeTasks> tasks,
                                absl::Span<const Task> expected, int capacity) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  PrecedencesPropagator* precedences =
      model.GetOrCreate<PrecedencesPropagator>();
  IntervalsRepository* intervals = model.GetOrCreate<IntervalsRepository>();

  const int num_tasks = tasks.size();
  std::vector<IntervalVariable> interval_vars(num_tasks);
  std::vector<AffineExpression> start_exprs(num_tasks);
  std::vector<AffineExpression> duration_exprs(num_tasks);
  std::vector<AffineExpression> end_exprs(num_tasks);
  std::vector<AffineExpression> demands(num_tasks);
  const AffineExpression capacity_expr =
      AffineExpression(IntegerValue(capacity));

  const int kStart(0);
  const int kHorizon(10000);

  for (int t = 0; t < num_tasks; ++t) {
    const CumulativeTasks& task = tasks[t];
    // Build the task variables.
    interval_vars[t] =
        model.Add(NewInterval(kStart, kHorizon, task.min_duration));
    start_exprs[t] = intervals->Start(interval_vars[t]);
    end_exprs[t] = intervals->End(interval_vars[t]);
    demands[t] = AffineExpression(IntegerValue(task.min_demand));

    // Set task initial minimum starting time.
    std::vector<Literal> no_literal_reason;
    std::vector<IntegerLiteral> no_integer_reason;
    EXPECT_TRUE(
        integer_trail->Enqueue(start_exprs[t].GreaterOrEqual(task.min_start),
                               no_literal_reason, no_integer_reason));
    // Set task initial maximum ending time.
    EXPECT_TRUE(integer_trail->Enqueue(end_exprs[t].LowerOrEqual(task.max_end),
                                       no_literal_reason, no_integer_reason));
  }

  // Propagate properly the other bounds of the intervals.
  EXPECT_TRUE(precedences->Propagate());

  auto* repo = model.GetOrCreate<IntervalsRepository>();
  SchedulingConstraintHelper* helper = repo->GetOrCreateHelper(interval_vars);
  SchedulingDemandHelper* demands_helper =
      model.TakeOwnership(new SchedulingDemandHelper(demands, helper, &model));

  // Propagator responsible for filtering start variables.
  TimeTablingPerTask timetabling(capacity_expr, helper, demands_helper, &model);
  timetabling.RegisterWith(model.GetOrCreate<GenericLiteralWatcher>());

  // Check initial satisfiability
  if (!model.GetOrCreate<SatSolver>()->Propagate()) return false;

  // Check consistency of data.
  CHECK_EQ(num_tasks, expected.size());

  for (int t = 0; t < num_tasks; ++t) {
    // Check starting time.
    EXPECT_EQ(expected[t].min_start, integer_trail->LowerBound(start_exprs[t]))
        << "task #" << t;
    // Check ending time.
    EXPECT_EQ(expected[t].max_end, integer_trail->UpperBound(end_exprs[t]))
        << "task #" << t;
  }
  return true;
}

// This is an infeasible instance on which the edge finder finds nothing.
// Cumulative Time Table finds the contradiction.
TEST(TimeTablingPropagation, UNSAT) {
  EXPECT_FALSE(TestTimeTablingPropagation({{3, 2, 0, 4}, {3, 2, 1, 5}}, {}, 3));
}

// This is an instance on Time Table pushes a task.
TEST(TimeTablingPropagation, TimeTablePush1) {
  EXPECT_TRUE(TestTimeTablingPropagation({{1, 2, 1, 2}, {3, 2, 0, 10}},
                                         {{1, 2}, {2, 10}}, 3));
}

// This is an instance on Time Table pushes a task.
TEST(TimeTablingPropagation, TimeTablePush2) {
  EXPECT_TRUE(
      TestTimeTablingPropagation({{1, 2, 1, 2}, {1, 2, 3, 4}, {3, 2, 0, 10}},
                                 {{1, 2}, {3, 4}, {4, 10}}, 3));
}

// This is an instance on which Time Table pushes a task.
// Here the two first tasks have the following profile:
// usage ^
//     2 |    **
//     1 |  **--**
//     0 |**------******************> time
//        0 1 2 3 4 5 6
// The interval [2, 3] has a profile too high to accommodate the third task.
TEST(TimeTablingPropagation, TimeTablePush3) {
  EXPECT_TRUE(
      TestTimeTablingPropagation({{3, 1, 0, 4}, {3, 1, 1, 5}, {3, 2, 0, 10}},
                                 {{0, 4}, {1, 5}, {3, 10}}, 3));
}

// This is an instance on which Time Table pushes a task.
// Similar to TimeTablePush3, but the two small tasks have the same profile.
TEST(TimeTablingPropagation, TimeTablePush4) {
  EXPECT_TRUE(
      TestTimeTablingPropagation({{4, 1, 0, 5}, {3, 1, 1, 4}, {3, 2, 0, 10}},
                                 {{0, 5}, {1, 4}, {4, 10}}, 3));
}

// Regression test: there used to be a bug when no profile delta corresponded
// to the start time of a task.
TEST(TimeTablingPropagation, RegressionTest) {
  EXPECT_TRUE(TestTimeTablingPropagation({{3, 1, 0, 3}, {2, 1, 2, 5}},
                                         {{0, 3}, {3, 5}}, 1));
}

// Regression test: there used to be a bug that caused Timetabling to stop
// before reaching its fixed-point.
TEST(TimeTablingPropagation, FixedPoint) {
  EXPECT_TRUE(TestTimeTablingPropagation(
      {{1, 1, 0, 1}, {4, 1, 0, 8}, {2, 1, 1, 5}, {1, 1, 1, 5}},
      {{0, 1}, {3, 8}, {1, 4}, {1, 4}}, 1));
}

// Regression test: there used to be a bug when two back to back
// tasks were exceeding the capacity in the partial sum.
TEST(TimeTablingPropagation, PartialSumBug) {
  EXPECT_TRUE(TestTimeTablingPropagation({{510, 142, 0, 510},
                                          {268, 130, 242, 510},
                                          {74, 147, 510, 584},
                                          {197, 204, 584, 781},
                                          {72, 138, 781, 853},
                                          {170, 231, 853, 1023},
                                          {181, 131, 1023, 1204}},
                                         {{0, 510},
                                          {242, 510},
                                          {510, 584},
                                          {584, 781},
                                          {781, 853},
                                          {853, 1023},
                                          {1023, 1204}},
                                         315));
}

// TODO(user): build automatic FindAll tests for the cumulative constraint.
// Test that we find all the solutions.
TEST(TimeTablingSolve, FindAll) {
  // Instance.
  const std::vector<int> durations = {1, 2, 3, 3, 3, 3};
  const std::vector<int> demands = {1, 1, 1, 1, 4, 4};
  const int capacity = 4;
  const int horizon = 11;

  Model model;
  std::vector<IntervalVariable> intervals(durations.size());
  std::vector<AffineExpression> demand_exprs(durations.size());
  const AffineExpression capacity_expr =
      AffineExpression(IntegerValue(capacity));

  for (int i = 0; i < durations.size(); ++i) {
    intervals[i] = model.Add(NewInterval(0, horizon, durations[i]));
    demand_exprs[i] = AffineExpression(IntegerValue(demands[i]));
  }

  model.Add(Cumulative(intervals, demand_exprs, capacity_expr));

  int num_solutions_found = 0;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* repository = model.GetOrCreate<IntervalsRepository>();
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Add the solution.
    std::vector<int> solution(durations.size());
    for (int i = 0; i < intervals.size(); ++i) {
      solution[i] =
          integer_trail->LowerBound(repository->Start(intervals[i])).value();
    }
    num_solutions_found++;
    LOG(INFO) << "Found solution: {" << absl::StrJoin(solution, ", ") << "}.";

    // Loop to the next solution.
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  // Test that we have the right number of solutions.
  EXPECT_EQ(num_solutions_found, 2040);
}

TEST(TimeTablingSolve, FindAllWithVaryingCapacity) {
  // Instance.
  const std::vector<int> durations = {1, 2, 3};
  const std::vector<int> demands = {1, 2, 3};
  const int horizon = 6;

  // Collect the number of solution for each capacity value.
  int sum = 0;
  for (const int capacity : {3, 4, 5}) {
    Model model;
    std::vector<IntervalVariable> intervals(durations.size());
    std::vector<AffineExpression> demand_exprs(durations.size());
    const AffineExpression capacity_expr =
        AffineExpression(IntegerValue(capacity));

    for (int i = 0; i < durations.size(); ++i) {
      intervals[i] = model.Add(NewInterval(0, horizon, durations[i]));
      demand_exprs[i] = AffineExpression(IntegerValue(demands[i]));
    }

    model.Add(Cumulative(intervals, demand_exprs, capacity_expr));

    int num_solutions_found = 0;
    auto* integer_trail = model.GetOrCreate<IntegerTrail>();
    auto* repository = model.GetOrCreate<IntervalsRepository>();
    while (true) {
      const SatSolver::Status status =
          SolveIntegerProblemWithLazyEncoding(&model);
      if (status != SatSolver::Status::FEASIBLE) break;

      // Add the solution.
      std::vector<int> solution(durations.size());
      for (int i = 0; i < intervals.size(); ++i) {
        solution[i] =
            integer_trail->LowerBound(repository->Start(intervals[i])).value();
      }
      num_solutions_found++;
      LOG(INFO) << "Found solution: {" << absl::StrJoin(solution, ", ") << "}.";

      // Loop to the next solution.
      model.Add(ExcludeCurrentSolutionAndBacktrack());
    }

    LOG(INFO) << "capacity: " << capacity
              << " num_solutions: " << num_solutions_found;
    sum += num_solutions_found;
  }

  // Now solve with a varying capacity.
  Model model;
  std::vector<IntervalVariable> intervals(durations.size());
  std::vector<AffineExpression> demand_exprs(durations.size());
  const AffineExpression capacity_expr =
      AffineExpression(model.Add(NewIntegerVariable(0, 5)));

  for (int i = 0; i < durations.size(); ++i) {
    intervals[i] = model.Add(NewInterval(0, horizon, durations[i]));
    demand_exprs[i] = AffineExpression(IntegerValue(demands[i]));
  }

  model.Add(Cumulative(intervals, demand_exprs, capacity_expr));

  int num_solutions_found = 0;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* repository = model.GetOrCreate<IntervalsRepository>();
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Add the solution.
    std::vector<int> solution(durations.size());
    for (int i = 0; i < intervals.size(); ++i) {
      solution[i] =
          integer_trail->LowerBound(repository->Start(intervals[i])).value();
    }
    num_solutions_found++;
    LOG(INFO) << "Found solution: {" << absl::StrJoin(solution, ", ") << "}.";

    // Loop to the next solution.
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  // Test that we have the right number of solutions.
  EXPECT_EQ(num_solutions_found, sum);
}

TEST(TimeTablingSolve, FindAllWithOptionals) {
  // Instance.
  // Up to two tasks can be scheduled at the same time.
  const std::vector<int> durations = {3, 3, 3};
  const std::vector<int> demands = {2, 2, 2};
  const int capacity = 5;
  const int horizon = 3;
  const int num_solutions = 7;

  Model model;
  std::vector<IntervalVariable> intervals(durations.size());
  std::vector<AffineExpression> demand_exprs(durations.size());
  std::vector<Literal> is_present_literals(durations.size());
  const AffineExpression capacity_expr =
      AffineExpression(IntegerValue(capacity));

  for (int i = 0; i < durations.size(); ++i) {
    is_present_literals[i] = Literal(model.Add(NewBooleanVariable()), true);
    intervals[i] = model.Add(
        NewOptionalInterval(0, horizon, durations[i], is_present_literals[i]));
    demand_exprs[i] = AffineExpression(IntegerValue(demands[i]));
  }

  model.Add(Cumulative(intervals, demand_exprs, capacity_expr));

  int num_solutions_found = 0;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* repository = model.GetOrCreate<IntervalsRepository>();
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Add the solution.
    std::vector<int> solution(durations.size());
    for (int i = 0; i < intervals.size(); ++i) {
      if (model.Get(Value(is_present_literals[i]))) {
        solution[i] =
            integer_trail->LowerBound(repository->Start(intervals[i])).value();
      } else {
        solution[i] = -1;
      }
    }
    num_solutions_found++;
    LOG(INFO) << "Found solution: {" << absl::StrJoin(solution, ", ") << "}.";

    // Loop to the next solution.
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  // Test that we have the right number of solutions.
  EXPECT_EQ(num_solutions_found, num_solutions);
}

// This construct a reservoir corresponding to a well behaved parenthesis
// sequence.
TEST(ReservoirTest, FindAllParenthesis) {
  const int n = 3;
  const int size = 2 * n;

  Model model;
  std::vector<IntegerVariable> vars(size);
  std::vector<AffineExpression> times(size);
  std::vector<AffineExpression> deltas(size);
  for (int i = 0; i < size; ++i) {
    vars[i] = model.Add(NewIntegerVariable(0, size - 1));
    times[i] = vars[i];
    deltas[i] = IntegerValue((i % 2 == 1) ? -1 : 1);
  }
  const Literal true_lit =
      model.GetOrCreate<IntegerEncoder>()->GetTrueLiteral();
  std::vector<Literal> all_true(size, true_lit);

  model.Add(AllDifferentOnBounds(vars));
  AddReservoirConstraint(times, deltas, all_true, 0, size, &model);

  absl::btree_map<std::string, int> sequence_to_count;
  int num_solutions_found = 0;
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Add the solution.
    std::string parenthesis_sequence(size, ' ');
    for (int i = 0; i < size; ++i) {
      const int v = model.Get(Value(vars[i]));
      parenthesis_sequence[v] = (i % 2 == 0) ? '(' : ')';
    }
    sequence_to_count[parenthesis_sequence]++;
    num_solutions_found++;

    // Loop to the next solution.
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  // To help debug the code.
  for (const auto entry : sequence_to_count) {
    LOG(INFO) << entry.first << " : " << entry.second;
  }
  LOG(INFO) << "decisions: " << model.GetOrCreate<SatSolver>()->num_branches();
  LOG(INFO) << "conflicts: " << model.GetOrCreate<SatSolver>()->num_failures();

  // Test that we have the right number of solutions.
  //
  // The catalan number n, which is 5 for n equal five, count the number of well
  // formed parathesis sequence. But we have to multiply this by the permutation
  // for the open and closing parenthesis that are matched to their positions:
  // n!.
  EXPECT_EQ(num_solutions_found, 5 * 6 * 6);
}

// Now some might be absent.
TEST(ReservoirTest, FindAllParenthesisWithOptionality) {
  const int n = 2;
  const int size = 2 * n;

  Model model;
  std::vector<IntegerVariable> vars(size);
  std::vector<AffineExpression> times(size);
  std::vector<AffineExpression> deltas(size);
  std::vector<Literal> present(size);
  for (int i = 0; i < size; ++i) {
    vars[i] = model.Add(NewIntegerVariable(0, size - 1));
    times[i] = vars[i];
    deltas[i] = IntegerValue((i % 2 == 1) ? -1 : 1);
    present[i] = Literal(model.Add(NewBooleanVariable()), true);
  }

  model.Add(AllDifferentOnBounds(vars));
  AddReservoirConstraint(times, deltas, present, 0, size, &model);

  absl::btree_map<std::string, int> sequence_to_count;
  int num_solutions_found = 0;
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Add the solution.
    std::string parenthesis_sequence(size, '_');
    for (int i = 0; i < size; ++i) {
      if (model.Get(Value(present[i])) == 0) continue;
      const int v = model.Get(Value(vars[i]));
      parenthesis_sequence[v] = (i % 2 == 0) ? '(' : ')';
    }
    sequence_to_count[parenthesis_sequence]++;
    num_solutions_found++;

    // Loop to the next solution.
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  // To help debug the code.
  for (const auto entry : sequence_to_count) {
    LOG(INFO) << entry.first << " : " << entry.second;
  }
  LOG(INFO) << "decisions: " << model.GetOrCreate<SatSolver>()->num_branches();
  LOG(INFO) << "conflicts: " << model.GetOrCreate<SatSolver>()->num_failures();

  // Test that we have the right number of solutions.
  EXPECT_EQ(num_solutions_found, 184);
}

// Enumerate all fixed sequence of [-1, +1] with a partial sum >= 0 and <= 1.
TEST(ReservoirTest, VariableLevelChange) {
  Model model;
  const int size = 8;
  std::vector<AffineExpression> times(size);
  std::vector<AffineExpression> deltas(size);
  for (int i = 0; i < size; ++i) {
    times[i] = IntegerValue(i);
    deltas[i] = model.Add(NewIntegerVariable(-1, 1));
  }
  const Literal true_lit =
      model.GetOrCreate<IntegerEncoder>()->GetTrueLiteral();
  std::vector<Literal> all_true(size, true_lit);

  const int min_level = 0;
  const int max_level = 1;
  AddReservoirConstraint(times, deltas, all_true, min_level, max_level, &model);

  absl::btree_map<std::string, int> sequence_to_count;
  int num_solutions_found = 0;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Add the solution.
    // Test that it is a valid one.
    int sum = 0;
    std::vector<int> values;
    for (int i = 0; i < size; ++i) {
      values.push_back(integer_trail->LowerBound(deltas[i]).value());
      sum += values.back();
      EXPECT_GE(sum, min_level);
      EXPECT_LE(sum, max_level);
    }
    sequence_to_count[absl::StrJoin(values, ",")]++;
    num_solutions_found++;

    // Loop to the next solution.
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  // To help debug the code.
  for (const auto entry : sequence_to_count) {
    LOG(INFO) << entry.first << " : " << entry.second;
  }
  LOG(INFO) << "decisions: " << model.GetOrCreate<SatSolver>()->num_branches();
  LOG(INFO) << "conflicts: " << model.GetOrCreate<SatSolver>()->num_failures();

  // Test that we have the right number of solutions.
  // For each subset of non-zero position, the value are fixed, it must
  // be an alternating sequence starting at 1.
  EXPECT_EQ(num_solutions_found, 1 << size);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
