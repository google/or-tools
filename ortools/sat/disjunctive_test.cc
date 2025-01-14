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

#include "ortools/sat/disjunctive.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/precedences.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

// TODO(user): Add tests for variable duration intervals! The code is trickier
// to get right in this case.

// Macros to improve the test readability below.
#define MIN_START(v) IntegerValue(v)
#define MIN_DURATION(v) IntegerValue(v)

TEST(TaskSetTest, AddEntry) {
  TaskSet tasks(1000);
  std::mt19937 random(12345);
  for (int i = 0; i < 1000; ++i) {
    tasks.AddEntry({i, MIN_START(absl::Uniform(random, 0, 1000)),
                    MIN_DURATION(absl::Uniform(random, 0, 100))});
  }
  EXPECT_TRUE(
      std::is_sorted(tasks.SortedTasks().begin(), tasks.SortedTasks().end()));
}

TEST(TaskSetTest, EndMinOnEmptySet) {
  TaskSet tasks(0);
  int critical_index;
  EXPECT_EQ(kMinIntegerValue,
            tasks.ComputeEndMin(/*task_to_ignore=*/-1, &critical_index));
  EXPECT_EQ(kMinIntegerValue, tasks.ComputeEndMin());
}

TEST(TaskSetTest, EndMinBasicTest) {
  TaskSet tasks(3);
  int critical_index;
  tasks.AddEntry({0, MIN_START(2), MIN_DURATION(3)});
  tasks.AddEntry({1, MIN_START(2), MIN_DURATION(3)});
  tasks.AddEntry({2, MIN_START(2), MIN_DURATION(3)});
  EXPECT_EQ(11, tasks.ComputeEndMin(/*task_to_ignore=*/-1, &critical_index));
  EXPECT_EQ(11, tasks.ComputeEndMin());
  EXPECT_EQ(0, critical_index);
}

TEST(TaskSetTest, EndMinWithNegativeValue) {
  TaskSet tasks(3);
  int critical_index;
  tasks.AddEntry({0, MIN_START(-5), MIN_DURATION(1)});
  tasks.AddEntry({1, MIN_START(-6), MIN_DURATION(2)});
  tasks.AddEntry({2, MIN_START(-7), MIN_DURATION(3)});
  EXPECT_EQ(-1, tasks.ComputeEndMin(/*task_to_ignore=*/-1, &critical_index));
  EXPECT_EQ(-1, tasks.ComputeEndMin());
  EXPECT_EQ(0, critical_index);
}

TEST(TaskSetTest, EndMinLimitCase) {
  TaskSet tasks(3);
  int critical_index;
  tasks.AddEntry({0, MIN_START(2), MIN_DURATION(3)});
  tasks.AddEntry({1, MIN_START(2), MIN_DURATION(3)});
  tasks.AddEntry({2, MIN_START(8), MIN_DURATION(5)});
  EXPECT_EQ(8, tasks.ComputeEndMin(/*task_to_ignore=*/2, &critical_index));
  EXPECT_EQ(0, critical_index);
  EXPECT_EQ(13, tasks.ComputeEndMin(/*task_to_ignore=*/-1, &critical_index));
  EXPECT_EQ(2, critical_index);
}

TEST(TaskSetTest, IgnoringTheLastEntry) {
  TaskSet tasks(3);
  int critical_index;
  tasks.AddEntry({0, MIN_START(2), MIN_DURATION(3)});
  tasks.AddEntry({1, MIN_START(7), MIN_DURATION(3)});
  EXPECT_EQ(10, tasks.ComputeEndMin(/*task_to_ignore=*/-1, &critical_index));
  EXPECT_EQ(5, tasks.ComputeEndMin(/*task_to_ignore=*/1, &critical_index));
}

#define MIN_START(v) IntegerValue(v)
#define MIN_DURATION(v) IntegerValue(v)

// Tests that the DisjunctiveConstraint propagate how expected on the
// given input. Returns false if a conflict is detected (i.e. no feasible
// solution).
struct TaskWithDuration {
  int min_start;
  int max_end;
  int min_duration;
};
struct Task {
  int min_start;
  int max_end;
};
bool TestDisjunctivePropagation(absl::Span<const TaskWithDuration> input,
                                absl::Span<const Task> expected,
                                int expected_num_enqueues) {
  Model model;
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntervalsRepository* intervals = model.GetOrCreate<IntervalsRepository>();

  const int kStart(0);
  const int kHorizon(10000);

  std::vector<IntervalVariable> ids;
  for (const TaskWithDuration& task : input) {
    const IntervalVariable i =
        model.Add(NewInterval(kStart, kHorizon, task.min_duration));
    ids.push_back(i);
    std::vector<Literal> no_literal_reason;
    std::vector<IntegerLiteral> no_integer_reason;
    EXPECT_TRUE(integer_trail->Enqueue(
        intervals->Start(i).GreaterOrEqual(IntegerValue(task.min_start)),
        no_literal_reason, no_integer_reason));
    EXPECT_TRUE(
        integer_trail->Enqueue(intervals->End(i).LowerOrEqual(task.max_end),
                               no_literal_reason, no_integer_reason));
  }

  // Propagate properly the other bounds of the intervals.
  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());

  const int initial_num_enqueues = integer_trail->num_enqueues();
  AddDisjunctive(ids, &model);
  if (!model.GetOrCreate<SatSolver>()->Propagate()) return false;
  CHECK_EQ(input.size(), expected.size());
  for (int i = 0; i < input.size(); ++i) {
    EXPECT_EQ(expected[i].min_start,
              integer_trail->LowerBound(intervals->Start(ids[i])))
        << "task #" << i;
    EXPECT_EQ(expected[i].max_end,
              integer_trail->UpperBound(intervals->End(ids[i])))
        << "task #" << i;
  }

  // The *2 is because there is one Enqueue() for the start and end variable.
  EXPECT_EQ(expected_num_enqueues + initial_num_enqueues,
            integer_trail->num_enqueues());
  return true;
}

// 01234567890
// (----      )
// (    ------)
TEST(DisjunctiveConstraintTest, NoPropagation) {
  EXPECT_TRUE(TestDisjunctivePropagation({{0, 10, 4}, {0, 10, 6}},
                                         {{0, 10}, {0, 10}}, 0));
}

// 01234567890
// (----      )
// (   -------)
TEST(DisjunctiveConstraintTest, Overload) {
  EXPECT_FALSE(TestDisjunctivePropagation({{0, 10, 4}, {0, 10, 7}}, {}, 0));
}

// 01234567890123456789
// (-----        )
//  (        -----)
//   (  ------  )
TEST(DisjunctiveConstraintTest, OverloadFromVilimPhd) {
  EXPECT_FALSE(
      TestDisjunctivePropagation({{0, 13, 5}, {1, 14, 5}, {2, 12, 6}}, {}, 0));
}

// 0123456789012345678901234567890123456789
//     (             [----        )
//      (---     )
//      (     ---)
//              (-----)
//
// TODO(user): The problem with this test is that the other propagators do
// propagate the same bound, but in 2 steps, whereas the edge finding do that in
// one. To properly test this, we need to add options to deactivate some of
// the propagations.
TEST(DisjunctiveConstraintTest, EdgeFindingFromVilimPhd) {
  EXPECT_TRUE(TestDisjunctivePropagation(
      {{4, 30, 4}, {5, 13, 3}, {5, 13, 3}, {13, 18, 5}},
      {{18, 30}, {5, 13}, {5, 13}, {13, 18}}, /*expected_num_enqueues=*/2));
}

// 0123456789012345678901234567890123456789
// (-----------              )
//  (                ----------)
//     (      --     ] )
TEST(DisjunctiveConstraintTest, NotLastFromVilimPhd) {
  EXPECT_TRUE(TestDisjunctivePropagation({{0, 25, 11}, {1, 27, 10}, {4, 20, 2}},
                                         {{0, 25}, {1, 27}, {4, 17}}, 1));
}

// 0123456789012345678901234567890123456789
// (-----        )
//  (        -----)
//        (---       )
//           [ <- the new bound for the third task.
TEST(DisjunctiveConstraintTest, DetectablePrecedenceFromVilimPhd) {
  EXPECT_TRUE(TestDisjunctivePropagation({{0, 13, 5}, {1, 14, 5}, {7, 17, 3}},
                                         {{0, 13}, {1, 14}, {10, 17}}, 1));
}

TEST(DisjunctiveConstraintTest, Precedences) {
  Model model;
  Trail* trail = model.GetOrCreate<Trail>();
  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* precedences = model.GetOrCreate<PrecedencesPropagator>();
  auto* relations = model.GetOrCreate<PrecedenceRelations>();
  auto* intervals = model.GetOrCreate<IntervalsRepository>();

  const auto add_affine_coeff_one_precedence = [&](const AffineExpression e1,
                                                   const AffineExpression& e2) {
    CHECK_NE(e1.var, kNoIntegerVariable);
    CHECK_EQ(e1.coeff, 1);
    CHECK_NE(e2.var, kNoIntegerVariable);
    CHECK_EQ(e2.coeff, 1);
    precedences->AddPrecedenceWithOffset(e1.var, e2.var,
                                         e1.constant - e2.constant);
    relations->Add(e1.var, e2.var, e1.constant - e2.constant);
  };

  const int kStart(0);
  const int kHorizon(10000);

  std::vector<IntervalVariable> ids;
  ids.push_back(model.Add(NewInterval(kStart, kHorizon, 10)));
  ids.push_back(model.Add(NewInterval(kStart, kHorizon, 10)));
  ids.push_back(model.Add(NewInterval(kStart, kHorizon, 10)));
  AddDisjunctive(ids, &model);

  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  for (const IntervalVariable i : ids) {
    EXPECT_EQ(0, integer_trail->LowerBound(intervals->Start(i)));
  }

  // Now with the precedences.
  add_affine_coeff_one_precedence(intervals->End(ids[0]),
                                  intervals->Start(ids[2]));
  add_affine_coeff_one_precedence(intervals->End(ids[1]),
                                  intervals->Start(ids[2]));
  EXPECT_TRUE(precedences->Propagate(trail));
  EXPECT_EQ(10, integer_trail->LowerBound(intervals->Start(ids[2])));

  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_EQ(20, integer_trail->LowerBound(intervals->Start(ids[2])));
}

// This test should enumerate all the permutation of kNumIntervals elements.
// It used to fail before CL 134067105.
TEST(SchedulingTest, Permutations) {
  static const int kNumIntervals = 4;
  Model model;
  std::vector<IntervalVariable> intervals;
  for (int i = 0; i < kNumIntervals; ++i) {
    const IntervalVariable interval =
        model.Add(NewInterval(0, kNumIntervals, 1));
    intervals.push_back(interval);
  }
  AddDisjunctive(intervals, &model);

  IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  IntervalsRepository* repository = model.GetOrCreate<IntervalsRepository>();
  std::vector<std::vector<int>> solutions;
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Add the solution.
    std::vector<int> solution(kNumIntervals, -1);
    for (int i = 0; i < intervals.size(); ++i) {
      const IntervalVariable interval = intervals[i];
      const int64_t start_time =
          integer_trail->LowerBound(repository->Start(interval)).value();
      DCHECK_GE(start_time, 0);
      DCHECK_LT(start_time, kNumIntervals);
      solution[start_time] = i;
    }
    solutions.push_back(solution);
    LOG(INFO) << "Found solution: {" << absl::StrJoin(solution, ", ") << "}.";

    // Loop to the next solution.
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  // Test that we do have all the permutations (but in a random order).
  std::sort(solutions.begin(), solutions.end());
  std::vector<int> expected(kNumIntervals);
  std::iota(expected.begin(), expected.end(), 0);
  for (int i = 0; i < solutions.size(); ++i) {
    EXPECT_EQ(expected, solutions[i]);
    if (i + 1 < solutions.size()) {
      EXPECT_TRUE(std::next_permutation(expected.begin(), expected.end()));
    } else {
      // We enumerated all the permutations.
      EXPECT_FALSE(std::next_permutation(expected.begin(), expected.end()));
    }
  }
}

// ============================================================================
// Random tests with comparison with a simple time-decomposition encoding.
// ============================================================================

void AddDisjunctiveTimeDecomposition(absl::Span<const IntervalVariable> vars,
                                     Model* model) {
  const int num_tasks = vars.size();
  IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
  IntegerEncoder* encoder = model->GetOrCreate<IntegerEncoder>();
  IntervalsRepository* repository = model->GetOrCreate<IntervalsRepository>();

  // Compute time range.
  IntegerValue min_start = kMaxIntegerValue;
  IntegerValue max_end = kMinIntegerValue;
  for (int t = 0; t < num_tasks; ++t) {
    const AffineExpression start = repository->Start(vars[t]);
    const AffineExpression end = repository->End(vars[t]);
    min_start = std::min(min_start, integer_trail->LowerBound(start));
    max_end = std::max(max_end, integer_trail->UpperBound(end));
  }

  // Add a constraint for each point of time.
  for (IntegerValue time = min_start; time <= max_end; ++time) {
    std::vector<Literal> presence_at_time;
    for (const IntervalVariable var : vars) {
      const AffineExpression start = repository->Start(var);
      const AffineExpression end = repository->End(var);

      const IntegerValue start_min = integer_trail->LowerBound(start);
      const IntegerValue end_max = integer_trail->UpperBound(end);
      if (end_max <= time || time < start_min) continue;

      // This will be true iff interval is present at time.
      // TODO(user): we actually only need one direction of the equivalence.
      presence_at_time.push_back(
          Literal(model->Add(NewBooleanVariable()), true));

      std::vector<Literal> presence_condition;
      presence_condition.push_back(encoder->GetOrCreateAssociatedLiteral(
          start.LowerOrEqual(IntegerValue(time))));
      presence_condition.push_back(encoder->GetOrCreateAssociatedLiteral(
          end.GreaterOrEqual(IntegerValue(time + 1))));
      if (repository->IsOptional(var)) {
        presence_condition.push_back(repository->PresenceLiteral(var));
      }
      model->Add(ReifiedBoolAnd(presence_condition, presence_at_time.back()));
    }
    model->Add(AtMostOneConstraint(presence_at_time));

    // Abort if UNSAT.
    if (model->GetOrCreate<SatSolver>()->ModelIsUnsat()) return;
  }
}

struct OptionalTasksWithDuration {
  int min_start;
  int max_end;
  int duration;
  bool is_optional;
};

// TODO(user): we never generate zero duration for now.
std::vector<OptionalTasksWithDuration> GenerateRandomInstance(
    int num_tasks, absl::BitGenRef randomizer) {
  std::vector<OptionalTasksWithDuration> instance;
  for (int i = 0; i < num_tasks; ++i) {
    OptionalTasksWithDuration task;
    task.min_start = absl::Uniform(randomizer, 0, 10);
    task.max_end = absl::Uniform(randomizer, 0, 10);
    if (task.min_start > task.max_end) std::swap(task.min_start, task.max_end);
    if (task.min_start == task.max_end) ++task.max_end;
    task.duration =
        1 + absl::Uniform(randomizer, 0, task.max_end - task.min_start - 1);
    task.is_optional = absl::Bernoulli(randomizer, 1.0 / 2);
    instance.push_back(task);
  }
  return instance;
}

int CountAllSolutions(
    absl::Span<const OptionalTasksWithDuration> instance,
    const std::function<void(const std::vector<IntervalVariable>&, Model*)>&
        add_disjunctive) {
  Model model;
  std::vector<IntervalVariable> intervals;
  for (const OptionalTasksWithDuration& task : instance) {
    if (task.is_optional) {
      const Literal is_present = Literal(model.Add(NewBooleanVariable()), true);
      intervals.push_back(model.Add(NewOptionalInterval(
          task.min_start, task.max_end, task.duration, is_present)));
    } else {
      intervals.push_back(
          model.Add(NewInterval(task.min_start, task.max_end, task.duration)));
    }
  }
  add_disjunctive(intervals, &model);

  int num_solutions_found = 0;
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;
    num_solutions_found++;
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }
  return num_solutions_found;
}

std::string InstanceDebugString(
    absl::Span<const OptionalTasksWithDuration> instance) {
  std::string result;
  for (const OptionalTasksWithDuration& task : instance) {
    absl::StrAppend(&result, "[", task.min_start, ", ", task.max_end,
                    "] duration:", task.duration,
                    " is_optional:", task.is_optional, "\n");
  }
  return result;
}

TEST(DisjunctiveTest, RandomComparisonWithSimpleEncoding) {
  std::mt19937 randomizer(12345);
  const int num_tests = DEBUG_MODE ? 100 : 1000;
  for (int test = 0; test < num_tests; ++test) {
    const int num_tasks = absl::Uniform(randomizer, 1, 6);
    const std::vector<OptionalTasksWithDuration> instance =
        GenerateRandomInstance(num_tasks, randomizer);
    EXPECT_EQ(CountAllSolutions(instance, AddDisjunctiveTimeDecomposition),
              CountAllSolutions(instance, AddDisjunctive))
        << InstanceDebugString(instance);
    EXPECT_EQ(
        CountAllSolutions(instance, AddDisjunctive),
        CountAllSolutions(instance, AddDisjunctiveWithBooleanPrecedencesOnly))
        << InstanceDebugString(instance);
  }
}

TEST(DisjunctiveTest, TwoIntervalsTest) {
  // All the way to put 2 intervals of size 4 and 3 in [0,9]. There is just
  // two non-busy unit interval, so:
  // - 2 possibilities with 1 hole of size 2 at beginning
  // - 2 possibilities with 1 hole of size 2 at the end.
  // - 2 possibilities with 1 hole of size 2 in the middle.
  // - 2 possibilities with 2 holes around the interval of size 3.
  // - 2 possibilities with 2 holes around the interval of size 4.
  // - 2 possibilities with 2 holes on both extremities.
  std::vector<OptionalTasksWithDuration> instance;
  instance.push_back({0, 9, 4, false});
  instance.push_back({0, 9, 3, false});
  EXPECT_EQ(12, CountAllSolutions(instance, AddDisjunctive));
}

TEST(DisjunctiveTest, Precedences) {
  Model model;

  std::vector<IntervalVariable> ids;
  ids.push_back(model.Add(NewInterval(0, 7, 3)));
  ids.push_back(model.Add(NewInterval(0, 7, 2)));
  AddDisjunctive(ids, &model);

  const IntegerVariable var = model.Add(NewIntegerVariable(0, 10));
  IntervalsRepository* intervals = model.GetOrCreate<IntervalsRepository>();
  model.Add(
      AffineCoeffOneLowerOrEqualWithOffset(intervals->End(ids[0]), var, 5));
  model.Add(
      AffineCoeffOneLowerOrEqualWithOffset(intervals->End(ids[1]), var, 4));

  EXPECT_TRUE(model.GetOrCreate<SatSolver>()->Propagate());
  EXPECT_EQ(model.Get(LowerBound(var)), (3 + 2) + std::min(4, 5));
}

TEST(DisjunctiveTest, OptionalIntervalsWithLinkedPresence) {
  Model model;
  const Literal alternative = Literal(model.Add(NewBooleanVariable()), true);

  std::vector<IntervalVariable> intervals;
  intervals.push_back(model.Add(NewOptionalInterval(0, 6, 3, alternative)));
  intervals.push_back(model.Add(NewOptionalInterval(0, 6, 2, alternative)));
  intervals.push_back(
      model.Add(NewOptionalInterval(0, 6, 4, alternative.Negated())));
  AddDisjunctive(intervals, &model);

  int num_solutions_found = 0;
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;
    num_solutions_found++;
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }
  EXPECT_EQ(num_solutions_found, /*alternative*/ 6 + /*!alternative*/ 3);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
