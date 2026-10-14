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

#include "ortools/routing/decision_builders.h"

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {
namespace {

TEST(IntVarDecisionBuilderTest, SetValuesFromTargets) {
  Solver solver("Test");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(10, 0, 10, "vars", &vars);
  const std::vector<int64_t> targets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  DecisionBuilder* const db = MakeSetValuesFromTargets(&solver, vars, targets);
  solver.NewSearch(db);
  solver.NextSolution();
  for (int i = 0; i < vars.size(); ++i) {
    EXPECT_EQ(targets[i], vars[i]->Value());
  }
  solver.EndSearch();
}

TEST(IntVarDecisionBuilderTest, SetImpossibleValuesFromTargets) {
  Solver solver("Test");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(10, 0, 10, "vars", &vars);
  const std::vector<int64_t> targets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  const std::vector<int64_t> expected = {2, 0, 1, 2, 3, 4, 5, 6, 7, 8};
  for (int i = 0; i < vars.size(); ++i) {
    vars[i]->RemoveValue(targets[i]);
    vars[i]->RemoveValue(targets[i] + 1);
  }
  DecisionBuilder* const db = MakeSetValuesFromTargets(&solver, vars, targets);
  solver.NewSearch(db);
  solver.NextSolution();
  for (int i = 0; i < vars.size(); ++i) {
    EXPECT_EQ(expected[i], vars[i]->Value());
  }
  solver.EndSearch();
}

TEST(IntVarDecisionBuilderTest, SetAllDiffValuesFromTargets) {
  Solver solver("Test");
  std::vector<IntVar*> vars;
  solver.MakeIntVarArray(10, 0, 10, "vars", &vars);
  solver.AddConstraint(solver.MakeAllDifferent(vars));
  const std::vector<int64_t> targets(10, 5);
  const std::vector<int64_t> expected = {5, 6, 4, 7, 3, 8, 2, 9, 1, 10};
  DecisionBuilder* const db = MakeSetValuesFromTargets(&solver, vars, targets);
  solver.NewSearch(db);
  solver.NextSolution();
  for (int i = 0; i < vars.size(); ++i) {
    EXPECT_EQ(expected[i], vars[i]->Value());
  }
  solver.EndSearch();
}

class ApplyClosureDB : public DecisionBuilder {
 public:
  explicit ApplyClosureDB(std::function<void(Solver*)> closure)
      : closure_(std::move(closure)) {}

  Decision* Next(Solver* s) override {
    if (applied_) return nullptr;
    s->SaveAndSetValue(&applied_, true);
    return s->MakeDecision(closure_, [](Solver*) {});
  }

 private:
  std::function<void(Solver* s)> closure_;
  bool applied_ = false;
};

class ApplyClosuresDB : public DecisionBuilder {
 public:
  explicit ApplyClosuresDB(std::vector<std::function<void(Solver*)>> closures)
      : closures_(std::move(closures)) {}

  Decision* Next(Solver* s) override {
    if (applied_) return nullptr;
    s->SaveAndSetValue(&applied_, true);
    if (index_ >= closures_.size()) return nullptr;
    return s->MakeDecision(closures_[index_], [](Solver*) {});
  }
  void SetIndex(int index) {
    index_ = index;
    applied_ = false;
  }

 private:
  std::vector<std::function<void(Solver* s)>> closures_;
  int index_ = 0;
  bool applied_ = false;
};

// RestoreDimensionValuesForUnchangedRoutes tests.
// GetParam() = true: injected solution scenario, RestoreXXX is not in the
// first solution's decision builder, so the first time its Next() is called,
// its AtSolution() has already been called.
// GetParam() = false: initial search scenario, RestoreXXX is in the
// first solution's decision builder, so its AtSolution() was never called
// before its Next() is called.
class TestWithParametrizedBoolean : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override { flag_ = GetParam(); }
  bool flag_;
};

// Tests that RestoreDimensionValuesForUnchangedRoutes decision builder
// does not restore stored cumul values for unchanged routes between different
// searches.
TEST_P(TestWithParametrizedBoolean,
       RestoreDimensionValuesForUnchangedRoutesInDifferentSearchesTest) {
  IndexManager manager(8, 2, {NodeIndex(0), NodeIndex(1)},
                       {NodeIndex(2), NodeIndex(3)});
  Model model(manager);
  ASSERT_EQ(model.Start(0), 0);
  ASSERT_EQ(model.Start(1), 1);
  ASSERT_EQ(model.End(0), 6);
  ASSERT_EQ(model.End(1), 7);
  const int zero = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 0; });
  const int one = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 1; });
  model.AddDimension(
      /*evaluator*/ one, /*slack_max*/ kint64max,
      /*capacity*/ kint64max, /*fix_start_cumul_to_zero*/ false,
      "dummyDimension");
  model.AddDimension(one, kint64max, 10, false, "tested");
  Dimension* dimension = model.GetMutableDimension("tested");
  std::vector<IntervalVar*> vehicle_breaks;
  std::vector<IntVar*> vehicle_break_times;  // SafeStart and SafeEnd.
  for (int vehicle = 0; vehicle < 2; ++vehicle) {
    IntervalVar* vehicle_break = model.solver()->MakeIntervalVar(
        /*start_min*/ 10, /*start_max*/ 20,
        /*duration_min*/ 1, /*duration_max*/ 5,
        /*end_min*/ 11, /*end_max*/ 25,
        /*optional*/ false, "vehicle_break");
    dimension->SetBreakIntervalsOfVehicle({vehicle_break}, vehicle, zero, zero);
    vehicle_breaks.push_back(vehicle_break);
    vehicle_break_times.push_back(vehicle_break->SafeStartExpr(0)->Var());
    vehicle_break_times.push_back(vehicle_break->SafeEndExpr(0)->Var());
  }
  model.CloseModel();

  // Make a decision builder that decides the routes,
  // calls RestoreDimensionValuesForUnchangedRoutes,
  // then assigns CumulVars at their minimum.
  // Routes in solution are 0 2 4 6 and 1 3 5 7.
  Solver* solver = model.solver();
  DecisionBuilder* apply_initial_paths =
      solver->RevAlloc(new ApplyClosureDB([&model](Solver*) {
        for (int node = 0; node < 6; ++node) {
          model.NextVar(node)->SetValue(node + 2);
        }
      }));
  DecisionBuilder* unchanged_db =
      MakeRestoreDimensionValuesForUnchangedRoutes(&model);
  DecisionBuilder* minimum_cumuls =
      solver->MakePhase(dimension->cumuls(), Solver::CHOOSE_FIRST_UNBOUND,
                        Solver::ASSIGN_MIN_VALUE);
  DecisionBuilder* minimum_breaks =
      solver->MakePhase(vehicle_break_times, Solver::CHOOSE_FIRST_UNBOUND,
                        Solver::ASSIGN_MIN_VALUE);
  // Whether GetParam() is true or not, unchanged_db will be notified when a
  // new solution is found.
  DecisionBuilder* minimum_db =
      GetParam()
          ? solver->Compose(apply_initial_paths, minimum_cumuls, minimum_breaks)
          : solver->Compose(apply_initial_paths, unchanged_db, minimum_cumuls,
                            minimum_breaks);

  // Make new search, check solution.
  solver->NewSearch(minimum_db, model.GetSearchMonitors());
  ASSERT_TRUE(solver->NextSolution());
  // First route, 0 2 4 6.
  EXPECT_EQ(dimension->CumulVar(0)->Value(), 0);
  EXPECT_EQ(dimension->SlackVar(0)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(2)->Value(), 1);
  EXPECT_EQ(dimension->SlackVar(2)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(4)->Value(), 2);
  EXPECT_EQ(dimension->SlackVar(4)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(6)->Value(), 3);
  EXPECT_EQ(vehicle_breaks[0]->StartMin(), 10);
  EXPECT_EQ(vehicle_breaks[0]->StartMax(), 10);
  EXPECT_EQ(vehicle_breaks[0]->EndMin(), 11);
  EXPECT_EQ(vehicle_breaks[0]->EndMax(), 11);
  // Second route, 1 3 5 7.
  EXPECT_EQ(dimension->CumulVar(1)->Value(), 0);
  EXPECT_EQ(dimension->SlackVar(1)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(3)->Value(), 1);
  EXPECT_EQ(dimension->SlackVar(3)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(5)->Value(), 2);
  EXPECT_EQ(dimension->SlackVar(5)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(7)->Value(), 3);
  EXPECT_EQ(vehicle_breaks[1]->StartMin(), 10);
  EXPECT_EQ(vehicle_breaks[1]->StartMax(), 10);
  EXPECT_EQ(vehicle_breaks[1]->EndMin(), 11);
  EXPECT_EQ(vehicle_breaks[1]->EndMax(), 11);
  solver->EndSearch();

  // Routes in next solution are 0 4 2 6 and 1 3 5 7.
  DecisionBuilder* apply_next_paths =
      solver->RevAlloc(new ApplyClosureDB([&model](Solver*) {
        model.NextVar(0)->SetValue(4);
        model.NextVar(4)->SetValue(2);
        model.NextVar(2)->SetValue(6);
        model.NextVar(1)->SetValue(3);
        model.NextVar(3)->SetValue(5);
        model.NextVar(5)->SetValue(7);
      }));
  DecisionBuilder* maximum_cumuls =
      solver->MakePhase(dimension->cumuls(), Solver::CHOOSE_FIRST_UNBOUND,
                        Solver::ASSIGN_MAX_VALUE);
  DecisionBuilder* maximum_breaks =
      solver->MakePhase(vehicle_break_times, Solver::CHOOSE_FIRST_UNBOUND,
                        Solver::ASSIGN_MAX_VALUE);
  DecisionBuilder* maximum_db = solver->Compose(apply_next_paths, unchanged_db,
                                                maximum_cumuls, maximum_breaks);
  solver->NewSearch(maximum_db, model.GetSearchMonitors());
  ASSERT_TRUE(solver->NextSolution());
  // First route is at max cumul and break values.
  EXPECT_EQ(dimension->CumulVar(0)->Value(), 7);
  EXPECT_EQ(dimension->SlackVar(0)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(4)->Value(), 8);
  EXPECT_EQ(dimension->SlackVar(4)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(2)->Value(), 9);
  EXPECT_EQ(dimension->SlackVar(2)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(6)->Value(), 10);
  EXPECT_EQ(vehicle_breaks[0]->StartMin(), 20);
  EXPECT_EQ(vehicle_breaks[0]->StartMax(), 20);
  EXPECT_EQ(vehicle_breaks[0]->EndMin(), 25);
  EXPECT_EQ(vehicle_breaks[0]->EndMax(), 25);
  // Second route has not changed but is at max cumul and break values due to
  // the new search.
  EXPECT_EQ(dimension->CumulVar(1)->Value(), 7);
  EXPECT_EQ(dimension->SlackVar(1)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(3)->Value(), 8);
  EXPECT_EQ(dimension->SlackVar(3)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(5)->Value(), 9);
  EXPECT_EQ(dimension->SlackVar(5)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(7)->Value(), 10);
  EXPECT_EQ(vehicle_breaks[1]->StartMin(), 20);
  EXPECT_EQ(vehicle_breaks[1]->StartMax(), 20);
  EXPECT_EQ(vehicle_breaks[1]->EndMin(), 25);
  EXPECT_EQ(vehicle_breaks[1]->EndMax(), 25);
  solver->EndSearch();
}

INSTANTIATE_TEST_SUITE_P(RestoreDimensionValuesForUnchangedRoutesScenario,
                         TestWithParametrizedBoolean, ::testing::Bool());

// Tests that RestoreDimensionValuesForUnchangedRoutes decision builder
// restores the same cumuls for unchanged routes as the previous solution's.
// We test this by changing a route between the first and second solution.
// We use a secondary decision builder that minimizes the cumuls values to
// get the first solution, whereas we use one that maximizes cumuls to get
// the second solution. The unchanged route has its cumuls free to go
// between low and high values, but when Restore*UnchangedRoutes is used, it
// forces those cumuls to keep the low values of the first solution in the
// second solution.
TEST(TestRestoreDimensionValuesForUnchangedRoutes, ActiveInSingleSearch) {
  IndexManager manager(8, 2, {NodeIndex(0), NodeIndex(1)},
                       {NodeIndex(2), NodeIndex(3)});
  Model model(manager);
  ASSERT_EQ(model.Start(0), 0);
  ASSERT_EQ(model.Start(1), 1);
  ASSERT_EQ(model.End(0), 6);
  ASSERT_EQ(model.End(1), 7);
  const int zero = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 0; });
  const int one = model.RegisterTransitCallback(
      [](int64_t, int64_t) -> int64_t { return 1; });
  model.AddDimension(
      /*evaluator*/ one, /*slack_max*/ kint64max,
      /*capacity*/ kint64max, /*fix_start_cumul_to_zero*/ false,
      "dummyDimension");
  model.AddDimension(one, kint64max, 10, false, "tested");
  Dimension* dimension = model.GetMutableDimension("tested");
  std::vector<IntervalVar*> vehicle_breaks;
  std::vector<IntVar*> vehicle_break_times;  // SafeStart and SafeEnd.
  for (int vehicle = 0; vehicle < 2; ++vehicle) {
    IntervalVar* vehicle_break = model.solver()->MakeIntervalVar(
        /*start_min*/ 10, /*start_max*/ 20,
        /*duration_min*/ 1, /*duration_max*/ 5,
        /*end_min*/ 11, /*end_max*/ 25,
        /*optional*/ false, "vehicle_break");
    dimension->SetBreakIntervalsOfVehicle({vehicle_break}, vehicle, zero, zero);
    vehicle_breaks.push_back(vehicle_break);
    vehicle_break_times.push_back(vehicle_break->SafeStartExpr(0)->Var());
    vehicle_break_times.push_back(vehicle_break->SafeEndExpr(0)->Var());
  }
  model.CloseModel();

  // Make decision builders that decides the routes,
  // calls RestoreDimensionValuesForUnchangedRoutes,
  // then assigns CumulVars and breaks.
  // The first solution has routes 0 2 4 6 and 1 3 5 7, and minimizes cumuls
  // and breaks times. The second solution has routes 0 4 2 6 and 1 3 5 7, and
  // maximizes cumuls and breaks times.
  Solver* solver = model.solver();
  ApplyClosuresDB* apply_paths = solver->RevAlloc(
      new ApplyClosuresDB({[&model](Solver*) {
                             for (int node = 0; node < 6; ++node) {
                               model.NextVar(node)->SetValue(node + 2);
                             }
                           },
                           [&model](Solver*) {
                             model.NextVar(0)->SetValue(4);
                             model.NextVar(4)->SetValue(2);
                             model.NextVar(2)->SetValue(6);
                             model.NextVar(1)->SetValue(3);
                             model.NextVar(3)->SetValue(5);
                             model.NextVar(5)->SetValue(7);
                           }}));
  ApplyClosuresDB* set_cumuls_and_breaks = solver->RevAlloc(
      new ApplyClosuresDB({[dimension, &vehicle_break_times](Solver*) {
                             for (IntVar* cumul : dimension->cumuls()) {
                               cumul->SetValue(cumul->Min());
                             }
                             for (IntVar* break_time : vehicle_break_times) {
                               break_time->SetValue(break_time->Min());
                             }
                           },
                           [dimension, &vehicle_break_times](Solver*) {
                             for (IntVar* cumul : dimension->cumuls()) {
                               cumul->SetValue(cumul->Max());
                             }
                             for (IntVar* break_time : vehicle_break_times) {
                               break_time->SetValue(break_time->Max());
                             }
                           }}));
  DecisionBuilder* unchanged_db =
      MakeRestoreDimensionValuesForUnchangedRoutes(&model);

  // Starts a new search without objective monitors in order to get the next
  // feasible solution when calling NextSolution(), instead of the "best" one.
  // Other monitors are necessary, in particular the ones set by
  // MakeRestoreDimensionValuesForUnchangedRoutes.
  std::vector<SearchMonitor*> monitors;
  for (SearchMonitor* monitor : model.GetSearchMonitors()) {
    if (dynamic_cast<BaseObjectiveMonitor*>(monitor)) continue;
    monitors.push_back(monitor);
  }
  solver->NewSearch(
      solver->Compose(apply_paths, unchanged_db,
                      solver->MakeSolveOnce(set_cumuls_and_breaks)),
      monitors);
  ASSERT_TRUE(solver->NextSolution());
  // First route, 0 2 4 6.
  EXPECT_EQ(dimension->CumulVar(0)->Value(), 0);
  EXPECT_EQ(dimension->SlackVar(0)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(2)->Value(), 1);
  EXPECT_EQ(dimension->SlackVar(2)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(4)->Value(), 2);
  EXPECT_EQ(dimension->SlackVar(4)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(6)->Value(), 3);
  EXPECT_EQ(vehicle_breaks[0]->StartMin(), 10);
  EXPECT_EQ(vehicle_breaks[0]->StartMax(), 10);
  EXPECT_EQ(vehicle_breaks[0]->EndMin(), 11);
  EXPECT_EQ(vehicle_breaks[0]->EndMax(), 11);
  // Second route, 1 3 5 7.
  EXPECT_EQ(dimension->CumulVar(1)->Value(), 0);
  EXPECT_EQ(dimension->SlackVar(1)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(3)->Value(), 1);
  EXPECT_EQ(dimension->SlackVar(3)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(5)->Value(), 2);
  EXPECT_EQ(dimension->SlackVar(5)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(7)->Value(), 3);
  EXPECT_EQ(vehicle_breaks[1]->StartMin(), 10);
  EXPECT_EQ(vehicle_breaks[1]->StartMax(), 10);
  EXPECT_EQ(vehicle_breaks[1]->EndMin(), 11);
  EXPECT_EQ(vehicle_breaks[1]->EndMax(), 11);
  apply_paths->SetIndex(1);
  set_cumuls_and_breaks->SetIndex(1);
  ASSERT_TRUE(solver->NextSolution());
  // First route, 0 4 2 6.
  // First route is at max cumul and break values.
  EXPECT_EQ(dimension->CumulVar(0)->Value(), 7);
  EXPECT_EQ(dimension->SlackVar(0)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(4)->Value(), 8);
  EXPECT_EQ(dimension->SlackVar(4)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(2)->Value(), 9);
  EXPECT_EQ(dimension->SlackVar(2)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(6)->Value(), 10);
  EXPECT_EQ(vehicle_breaks[0]->StartMin(), 20);
  EXPECT_EQ(vehicle_breaks[0]->StartMax(), 20);
  EXPECT_EQ(vehicle_breaks[0]->EndMin(), 25);
  EXPECT_EQ(vehicle_breaks[0]->EndMax(), 25);
  // Second route, 1 3 5 7.
  // Second route has the same cumuls and break values.
  EXPECT_EQ(dimension->CumulVar(1)->Value(), 0);
  EXPECT_EQ(dimension->SlackVar(1)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(3)->Value(), 1);
  EXPECT_EQ(dimension->SlackVar(3)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(5)->Value(), 2);
  EXPECT_EQ(dimension->SlackVar(5)->Value(), 0);
  EXPECT_EQ(dimension->CumulVar(7)->Value(), 3);
  EXPECT_EQ(vehicle_breaks[1]->StartMin(), 10);
  EXPECT_EQ(vehicle_breaks[1]->StartMax(), 10);
  EXPECT_EQ(vehicle_breaks[1]->EndMin(), 11);
  EXPECT_EQ(vehicle_breaks[1]->EndMax(), 11);
  solver->EndSearch();
}

// TODO(user): Add unit tests for remaining DecisionBuilders.

}  // namespace
}  // namespace operations_research::routing
