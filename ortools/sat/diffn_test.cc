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

#include "ortools/sat/diffn.h"

#include <vector>

#include "absl/strings/str_join.h"
#include "gtest/gtest.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

// Counts how many ways we can put two square of minimal size 1 in an n x n
// square.
//
// For n = 1, infeasible.
// For n = 2, should be 4 * 3.
// For n = 3:
//  - 9 * 8 for two size 1.
//  - 4 * 5 for size 2 + size 1. Times 2 for the permutation.
int CountAllTwoBoxesSolutions(int n) {
  Model model;
  std::vector<IntervalVariable> x;
  std::vector<IntervalVariable> y;
  for (int i = 0; i < 2; ++i) {
    // Create a square shaped box of minimum size 1.
    const IntegerVariable size = model.Add(NewIntegerVariable(1, n));
    x.push_back(
        model.Add(NewInterval(model.Add(NewIntegerVariable(0, n)),
                              model.Add(NewIntegerVariable(0, n)), size)));
    y.push_back(
        model.Add(NewInterval(model.Add(NewIntegerVariable(0, n)),
                              model.Add(NewIntegerVariable(0, n)), size)));
  }

  // The cumulative relaxation adds extra variables that are not complextly
  // fixed. So to not count too many solution with our code here, we disable
  // that. Note that alternativelly, we could have used the cp_model.proto API
  // to do the same, and that should works even with this on.
  AddNonOverlappingRectangles(x, y, &model);

  int num_solutions_found = 0;
  auto* integer_trail = model.GetOrCreate<IntegerTrail>();
  auto* repository = model.GetOrCreate<IntervalsRepository>();
  auto start_value = [repository, integer_trail](IntervalVariable i) {
    return integer_trail->LowerBound(repository->Start(i)).value();
  };
  auto end_value = [repository, integer_trail](IntervalVariable i) {
    return integer_trail->LowerBound(repository->End(i)).value();
  };
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Display the first few solutions.
    if (num_solutions_found < 30) {
      LOG(INFO) << "R1: " << start_value(x[0]) << "," << start_value(y[0])
                << " " << end_value(x[0]) << "," << end_value(y[0])
                << " R2: " << start_value(x[1]) << "," << start_value(y[1])
                << " " << end_value(x[1]) << "," << end_value(y[1]);
    }

    num_solutions_found++;
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }
  return num_solutions_found;
}

TEST(NonOverlappingRectanglesTest, SimpleCounting) {
  EXPECT_EQ(CountAllTwoBoxesSolutions(1), 0);
  EXPECT_EQ(CountAllTwoBoxesSolutions(2), 3 * 4);
  EXPECT_EQ(CountAllTwoBoxesSolutions(3), 9 * 8 + 4 * 5 * 2);
  EXPECT_EQ(CountAllTwoBoxesSolutions(4),
            /*2 1x1 square*/ 16 * 15 +
                /*2 2x2 square*/ 2 * (5 + 3 + 4 + 4) +
                /*3x3 and 1x1*/ 2 * 4 * 7 +
                /*2x2 amd 1x1*/ 2 * 9 * 12);
}

TEST(NonOverlappingRectanglesTest, SimpleCountingWithOptional) {
  Model model;
  IntervalsRepository* interval_repository =
      model.GetOrCreate<IntervalsRepository>();
  std::vector<IntervalVariable> x;
  std::vector<IntervalVariable> y;
  const Literal l1(model.Add(NewBooleanVariable()), true);
  x.push_back(interval_repository->CreateInterval(
      IntegerValue(0), IntegerValue(5), IntegerValue(5), l1.Index(), false));
  y.push_back(interval_repository->CreateInterval(
      IntegerValue(0), IntegerValue(2), IntegerValue(2), l1.Index(), false));

  const Literal l2(model.Add(NewBooleanVariable()), true);
  x.push_back(interval_repository->CreateInterval(
      IntegerValue(4), IntegerValue(6), IntegerValue(2), l2.Index(), false));
  y.push_back(interval_repository->CreateInterval(
      IntegerValue(3), IntegerValue(4), IntegerValue(1), l2.Index(), false));

  // The cumulative relaxation adds extra variables that are not completely
  // fixed. So to not count too many solution with our code here, we disable
  // that. Note that alternatively, we could have used the cp_model.proto API
  // to do the same, and that should works even with this on.
  // TODO(user): Fix and run with add_cumulative_relaxation = true.
  AddNonOverlappingRectangles(x, y, &model);

  int num_solutions_found = 0;
  while (true) {
    const SatSolver::Status status =
        SolveIntegerProblemWithLazyEncoding(&model);
    if (status != SatSolver::Status::FEASIBLE) break;

    // Display the first few solutions.
    if (num_solutions_found < 30) {
      LOG(INFO) << "R1: " << interval_repository->IsPresent(x[0]) << " "
                << " R2: " << interval_repository->IsPresent(x[1]) << " ";
    }

    num_solutions_found++;
    model.Add(ExcludeCurrentSolutionAndBacktrack());
  }
  EXPECT_EQ(4, num_solutions_found);
}

TEST(NonOverlappingRectanglesTest, CountSolutionsWithZeroAreaBoxes) {
  CpModelBuilder cp_model;
  IntVar v1 = cp_model.NewIntVar({1, 2});
  IntVar v2 = cp_model.NewIntVar({0, 1});
  IntervalVar x1 = cp_model.NewIntervalVar(2, v2, 2 + v2);
  IntervalVar x2 = cp_model.NewFixedSizeIntervalVar(1, 2);
  IntervalVar y1 = cp_model.NewIntervalVar(1, v1, v1 + 1);
  IntervalVar y2 = cp_model.NewFixedSizeIntervalVar(2, 0);
  NoOverlap2DConstraint diffn = cp_model.AddNoOverlap2D();
  diffn.AddRectangle(x1, y1);
  diffn.AddRectangle(x2, y2);

  Model model;
  SatParameters* params = model.GetOrCreate<SatParameters>();
  params->set_enumerate_all_solutions(true);
  params->set_keep_all_feasible_solutions_in_presolve(true);
  int count = 0;
  model.Add(
      NewFeasibleSolutionObserver([&count](const CpSolverResponse& response) {
        LOG(INFO) << absl::StrJoin(response.solution(), " ");
        ++count;
      }));
  const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
  EXPECT_EQ(response.status(), CpSolverStatus::OPTIMAL);
  EXPECT_EQ(count, 2);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
