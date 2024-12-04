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

#include "ortools/sat/cumulative.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

// RcpspInstance contains the data to define an instance of the Resource
// Constrained Project Scheduling Problem (RCPSP). We only consider a restricted
// variant of the RCPSP which is the problem of scheduling a set of
// non-premptive tasks that consume a given quantity of a resource without
// exceeding the resource's capacity. We assume that the duration of a task, its
// demand, and the resource capacity are fixed.
struct RcpspInstance {
  RcpspInstance() : capacity(0), min_start(0), max_end(0) {}
  std::vector<int64_t> durations;
  std::vector<bool> optional;
  std::vector<int64_t> demands;
  int64_t capacity;
  int64_t min_start;
  int64_t max_end;
  std::string DebugString() const {
    std::string result = "RcpspInstance {\n";
    result += " demands:   {" + absl::StrJoin(demands, ", ") + "}\n";
    result += " durations: {" + absl::StrJoin(durations, ", ") + "}\n";
    result += " optional:  {" + absl::StrJoin(optional, ", ") + "}\n";
    result += " min_start: " + absl::StrCat(min_start) + "\n";
    result += " max_end:   " + absl::StrCat(max_end) + "\n";
    result += " capacity:  " + absl::StrCat(capacity) + "\n}";
    return result;
  }
};

// Generates a random RcpspInstance with num_tasks tasks such that:
// - the duration of a task is a fixed random number in
//   [min_duration, max_durations];
// - tasks can be optional if enable_optional is true;
// - the demand of a task is a fixed random number in [min_demand, max_demand];
// - the resource capacity is a fixed random number in
//   [max_demand - 1, max_capacity]. This allows the capacity to be lower than
//   the highest demand to generate trivially unfeasible instances.
// - the energy (i.e. surface) of the resource is 120% of the total energy of
//   the tasks. This allows the generation of infeasible instances.
RcpspInstance GenerateRandomInstance(int num_tasks, int min_duration,
                                     int max_duration, int min_demand,
                                     int max_demand, int max_capacity,
                                     int min_start, bool enable_optional) {
  absl::BitGen random;
  RcpspInstance instance;
  int energy = 0;

  // Generate task demands and durations.
  int max_of_all_durations = 0;
  for (int t = 0; t < num_tasks; ++t) {
    const int duration = absl::Uniform(random, min_duration, max_duration + 1);
    const int demand = absl::Uniform(random, min_demand, max_demand + 1);
    energy += duration * demand;
    max_of_all_durations = std::max(max_of_all_durations, duration);
    instance.durations.push_back(duration);
    instance.demands.push_back(demand);
    instance.optional.push_back(enable_optional &&
                                absl::Bernoulli(random, 0.5));
  }

  // Generate the resource capacity.
  instance.capacity = absl::Uniform(random, max_demand, max_capacity + 1);

  // Generate the time window.
  instance.min_start = min_start;
  instance.max_end =
      min_start +
      std::max(static_cast<int>(std::round(energy * 1.2 / instance.capacity)),
               max_of_all_durations);
  return instance;
}

template <class Cumulative>
int CountAllSolutions(const RcpspInstance& instance, SatParameters parameters,
                      const Cumulative& cumulative) {
  Model model;
  parameters.set_use_disjunctive_constraint_in_cumulative(false);
  model.GetOrCreate<SatSolver>()->SetParameters(parameters);

  DCHECK_EQ(instance.demands.size(), instance.durations.size());
  DCHECK_LE(instance.min_start, instance.max_end);

  const int num_tasks = instance.demands.size();
  std::vector<IntervalVariable> intervals(num_tasks);
  std::vector<AffineExpression> demands(num_tasks);
  const AffineExpression capacity = IntegerValue(instance.capacity);

  for (int t = 0; t < num_tasks; ++t) {
    if (instance.optional[t]) {
      const Literal is_present = Literal(model.Add(NewBooleanVariable()), true);
      intervals[t] =
          model.Add(NewOptionalInterval(instance.min_start, instance.max_end,
                                        instance.durations[t], is_present));
    } else {
      intervals[t] = model.Add(NewInterval(instance.min_start, instance.max_end,
                                           instance.durations[t]));
    }
    demands[t] = IntegerValue(instance.demands[t]);
  }

  model.Add(cumulative(intervals, demands, capacity, nullptr));

  // Make sure that every Boolean variable is considered as a decision variable
  // to be fixed.
  if (parameters.search_branching() == SatParameters::FIXED_SEARCH) {
    SatSolver* sat_solver = model.GetOrCreate<SatSolver>();
    for (int i = 0; i < sat_solver->NumVariables(); ++i) {
      model.Add(
          NewIntegerVariableFromLiteral(Literal(BooleanVariable(i), true)));
    }
  }

  int num_solutions_found = 0;
  // Loop until there is no remaining solution to find.
  while (true) {
    // Try to find a solution.
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    // Leave the loop if there is no solution left.
    if (status != SatSolver::Status::FEASIBLE) break;
    num_solutions_found++;
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }

  return num_solutions_found;
}

TEST(CumulativeTimeDecompositionTest, AllPermutations) {
  RcpspInstance instance;
  instance.demands = {1, 1, 1, 1, 1};
  instance.durations = {1, 1, 1, 1, 1};
  instance.optional = {false, false, false, false, false};
  instance.capacity = 1;
  instance.min_start = 0;
  instance.max_end = 5;
  ASSERT_EQ(120, CountAllSolutions(instance, {}, CumulativeTimeDecomposition));
}

TEST(CumulativeTimeDecompositionTest, FindAll) {
  RcpspInstance instance;
  instance.demands = {1, 1, 1, 1, 4, 4};
  instance.durations = {1, 2, 3, 3, 3, 3};
  instance.optional = {false, false, false, false, false, false};
  instance.capacity = 4;
  instance.min_start = 0;
  instance.max_end = 11;
  ASSERT_EQ(2040, CountAllSolutions(instance, {}, CumulativeTimeDecomposition));
  ASSERT_EQ(2040, CountAllSolutions(instance, {}, CumulativeUsingReservoir));
}

TEST(CumulativeTimeDecompositionTest, OptionalTasks1) {
  RcpspInstance instance;
  instance.demands = {3, 3, 3};
  instance.durations = {1, 1, 1};
  instance.optional = {true, true, true};
  instance.capacity = 7;
  instance.min_start = 0;
  instance.max_end = 2;
  ASSERT_EQ(25, CountAllSolutions(instance, {}, Cumulative));
  ASSERT_EQ(25, CountAllSolutions(instance, {}, CumulativeUsingReservoir));
}

// Up to two tasks can be scheduled at the same time.
TEST(CumulativeTimeDecompositionTest, OptionalTasks2) {
  RcpspInstance instance;
  instance.demands = {3, 3, 3};
  instance.durations = {3, 3, 3};
  instance.optional = {true, true, true};
  instance.capacity = 7;
  instance.min_start = 0;
  instance.max_end = 3;
  ASSERT_EQ(7, CountAllSolutions(instance, {}, CumulativeTimeDecomposition));
  ASSERT_EQ(7, CountAllSolutions(instance, {}, CumulativeUsingReservoir));
}

TEST(CumulativeTimeDecompositionTest, RegressionTest1) {
  RcpspInstance instance;
  instance.demands = {5, 4, 1};
  instance.durations = {1, 1, 2};
  instance.optional = {false, false, false};
  instance.capacity = 5;
  instance.min_start = 0;
  instance.max_end = 2;
  ASSERT_EQ(0, CountAllSolutions(instance, {}, CumulativeTimeDecomposition));
}

// Cumulative was pruning too many solutions on that instance.
TEST(CumulativeTimeDecompositionTest, RegressionTest2) {
  SatParameters parameters;
  parameters.set_use_overload_checker_in_cumulative(false);
  parameters.set_use_timetable_edge_finding_in_cumulative(false);
  RcpspInstance instance;
  instance.demands = {4, 4, 3};
  instance.durations = {2, 2, 3};
  instance.optional = {true, true, true};
  instance.capacity = 6;
  instance.min_start = 0;
  instance.max_end = 5;
  ASSERT_EQ(
      22, CountAllSolutions(instance, parameters, CumulativeTimeDecomposition));
}

bool CheckCumulative(const SatParameters& parameters,
                     const RcpspInstance& instance) {
  const int64_t num_solutions_ref =
      CountAllSolutions(instance, parameters, CumulativeTimeDecomposition);
  const int64_t num_solutions_test =
      CountAllSolutions(instance, parameters, Cumulative);
  if (num_solutions_ref != num_solutions_test) {
    LOG(INFO) << "Want: " << num_solutions_ref
              << " solutions, got: " << num_solutions_test << " solutions.";
    LOG(INFO) << instance.DebugString();
    return false;
  }
  const int64_t num_solutions_reservoir =
      CountAllSolutions(instance, parameters, CumulativeUsingReservoir);
  if (num_solutions_ref != num_solutions_reservoir) {
    LOG(INFO) << "Want: " << num_solutions_ref
              << " solutions, got: " << num_solutions_reservoir
              << " solutions.";
    LOG(INFO) << instance.DebugString();
    return false;
  }
  return true;
}

// Checks that the cumulative constraint performs trivial propagation by
// updating the capacity and demand variables.
TEST(CumulativeTest, CapacityAndDemand) {
  Model model;
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();
  const IntervalVariable interval = model.Add(NewInterval(-1000, 1000, 1));
  const IntegerVariable demand = model.Add(NewIntegerVariable(5, 15));
  const IntegerVariable capacity = model.Add(NewIntegerVariable(0, 10));
  const IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  model.Add(Cumulative({interval}, {AffineExpression(demand)},
                       AffineExpression(capacity)));
  ASSERT_TRUE(sat_solver->Propagate());
  ASSERT_EQ(integer_trail->LowerBound(capacity), 5);
  ASSERT_EQ(integer_trail->UpperBound(capacity), 10);
  ASSERT_EQ(integer_trail->LowerBound(demand), 5);
  ASSERT_EQ(integer_trail->UpperBound(demand), 10);
}

// Checks that the cumulative constraint adpats the demand of the task to
// prevent the capacity overload.
TEST(CumulativeTest, CapacityAndZeroDemand) {
  Model model;
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();
  const IntegerVariable start = model.Add(NewIntegerVariable(-1000, 1000));
  const IntegerVariable size = model.Add(NewIntegerVariable(0, 10));
  const IntegerVariable end = model.Add(NewIntegerVariable(-1000, 1000));
  const IntervalVariable interval = model.Add(NewInterval(start, end, size));
  const IntegerVariable demand = model.Add(NewIntegerVariable(11, 15));
  const IntegerVariable capacity = model.Add(NewIntegerVariable(0, 10));
  const IntegerTrail* integer_trail = model.GetOrCreate<IntegerTrail>();
  model.Add(Cumulative({interval}, {AffineExpression(demand)},
                       AffineExpression(capacity)));
  ASSERT_TRUE(sat_solver->Propagate());
  ASSERT_EQ(integer_trail->LowerBound(capacity), 0);
  ASSERT_EQ(integer_trail->UpperBound(capacity), 10);
  ASSERT_EQ(integer_trail->LowerBound(demand), 11);
  ASSERT_EQ(integer_trail->UpperBound(demand), 15);
  ASSERT_EQ(integer_trail->UpperBound(size), 0);
}

// Checks that the cumulative constraint removes the task to prevent the
// capacity overload.
TEST(CumulativeTest, CapacityAndOptionalTask) {
  Model model;
  SatSolver* sat_solver = model.GetOrCreate<SatSolver>();
  const Literal l = Literal(model.Add(NewBooleanVariable()), true);
  const IntervalVariable interval =
      model.Add(NewOptionalInterval(-1000, 1000, 1, l));
  const IntegerVariable demand = model.Add(ConstantIntegerVariable(15));
  const IntegerVariable capacity = model.Add(ConstantIntegerVariable(10));
  model.Add(Cumulative({interval}, {AffineExpression(demand)},
                       AffineExpression(capacity)));
  ASSERT_TRUE(sat_solver->Propagate());
  ASSERT_FALSE(model.Get(Value(l)));
}

// Cumulative was pruning too many solutions on that instance.
TEST(CumulativeTest, RegressionTest1) {
  SatParameters parameters;
  parameters.set_use_overload_checker_in_cumulative(false);
  parameters.set_use_timetable_edge_finding_in_cumulative(false);
  RcpspInstance instance;
  instance.demands = {4, 4, 3};
  instance.durations = {2, 2, 3};
  instance.optional = {true, true, true};
  instance.capacity = 6;
  instance.min_start = 0;
  instance.max_end = 5;
  ASSERT_EQ(22, CountAllSolutions(instance, parameters, Cumulative));
}

// Cumulative was pruning too many solutions on that instance.
TEST(CumulativeTest, RegressionTest2) {
  SatParameters parameters;
  parameters.set_use_overload_checker_in_cumulative(false);
  parameters.set_use_timetable_edge_finding_in_cumulative(false);
  RcpspInstance instance;
  instance.demands = {5, 4};
  instance.durations = {4, 4};
  instance.optional = {true, true};
  instance.capacity = 6;
  instance.min_start = 0;
  instance.max_end = 7;
  ASSERT_EQ(9, CountAllSolutions(instance, parameters, Cumulative));
}

// ========================================================================
// All the test belows check that the cumulative propagator finds the exact
// same number of solutions than its time point decomposition.
// ========================================================================

// Param1: Number of tasks.
// Param3: Enable overload checking.
// Param4: Enable timetable edge finding.
typedef ::testing::tuple<int, bool, bool> CumulativeTestParams;

class RandomCumulativeTest
    : public ::testing::TestWithParam<CumulativeTestParams> {
 protected:
  int GetNumTasks() { return ::testing::get<0>(GetParam()); }

  SatParameters GetSatParameters() {
    SatParameters parameters;
    parameters.set_use_disjunctive_constraint_in_cumulative(false);
    parameters.set_use_overload_checker_in_cumulative(
        ::testing::get<1>(GetParam()));
    parameters.set_use_timetable_edge_finding_in_cumulative(
        ::testing::get<2>(GetParam()));
    return parameters;
  }
};

class FastRandomCumulativeTest : public RandomCumulativeTest {};
class SlowRandomCumulativeTest : public RandomCumulativeTest {};

TEST_P(FastRandomCumulativeTest, FindAll) {
  ASSERT_TRUE(CheckCumulative(
      GetSatParameters(),
      GenerateRandomInstance(GetNumTasks(), 1, 4, 1, 5, 7, 0, false)));
}

TEST_P(FastRandomCumulativeTest, FindAllNegativeTime) {
  ASSERT_TRUE(CheckCumulative(
      GetSatParameters(),
      GenerateRandomInstance(GetNumTasks(), 1, 4, 1, 5, 7, -100, false)));
}

TEST_P(SlowRandomCumulativeTest, FindAllZeroDuration) {
  ASSERT_TRUE(CheckCumulative(
      GetSatParameters(),
      GenerateRandomInstance(GetNumTasks(), 0, 4, 1, 5, 7, 0, false)));
}

TEST_P(SlowRandomCumulativeTest, FindAllZeroDemand) {
  ASSERT_TRUE(CheckCumulative(
      GetSatParameters(),
      GenerateRandomInstance(GetNumTasks(), 1, 4, 0, 5, 7, 0, false)));
}

TEST_P(SlowRandomCumulativeTest, FindAllOptionalTasks) {
  ASSERT_TRUE(CheckCumulative(
      GetSatParameters(),
      GenerateRandomInstance(GetNumTasks(), 1, 4, 0, 5, 7, 0, true)));
}

INSTANTIATE_TEST_SUITE_P(
    All, FastRandomCumulativeTest,
    ::testing::Combine(::testing::Range(3, DEBUG_MODE ? 4 : 6),
                       ::testing::Bool(), ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    All, SlowRandomCumulativeTest,
    ::testing::Combine(::testing::Range(3, DEBUG_MODE ? 4 : 5),
                       ::testing::Bool(), ::testing::Bool()));

}  // namespace
}  // namespace sat
}  // namespace operations_research
