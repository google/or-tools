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

#include "ortools/routing/constraints.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solver_test_util.h"
#include "ortools/constraint_solver/interval.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/types.h"

namespace operations_research::routing {
namespace {

class ModelPropagationTest : public testing::Test {
 protected:
  void SetUp() override {
    IndexManager manager(num_nodes_, num_vehicles_, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);
    const int64_t start = model_->Start(tested_vehicle_);
    const int64_t end = model_->End(tested_vehicle_);
    std::vector<int64_t> path{start, 3, 2, 4, 1, end};
    for (int position = 1; position < path.size(); ++position) {
      model_->NextVar(path[position - 1])->SetValue(path[position]);
    }
  }

  const int num_nodes_ = 10;
  const int num_vehicles_ = 2;
  const int tested_vehicle_ = 1;
  std::unique_ptr<Model> model_;

  // To test propagation, we must be within search.
  class TestDecisionBuilder : public DecisionBuilder {
   public:
    explicit TestDecisionBuilder(std::function<Decision*(Solver*)> callback)
        : callback_(std::move(callback)) {}
    Decision* Next(Solver* solver) override { return callback_(solver); }

   private:
    std::function<Decision*(Solver*)> callback_;
  };
};

// The PathSpansAndTotalSlacks constraint must fix the value of total_slack
// variables of routes that have their fixed_transit, start and end variables
// bound.
TEST_F(ModelPropagationTest, SpanAndTotalSlackFixedWhenStartEndFixed) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  std::function<Decision*(Solver*)> callback =
      [&](Solver* solver) -> Decision* {
    const int duration_callback =
        model_->RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
    model_->AddDimension(duration_callback, kint64max, kint64max, false,
                         "time");
    Dimension* const time = model_->GetMutableDimension("time");

    spans[tested_vehicle_] = solver->MakeIntVar(0, 100);
    total_slacks[tested_vehicle_] = solver->MakeIntVar(0, 100);

    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);
    model_->CloseModel();

    const int64_t start = model_->Start(tested_vehicle_);
    const int64_t end = model_->End(tested_vehicle_);
    time->CumulVar(start)->SetValue(3);
    time->CumulVar(end)->SetValue(10);

    EXPECT_TRUE(spans[tested_vehicle_]->Bound());
    EXPECT_EQ(spans[tested_vehicle_]->Value(),
              /*end*/ 10 - /*start*/ 3);
    EXPECT_TRUE(total_slacks[tested_vehicle_]->Bound());
    EXPECT_EQ(total_slacks[tested_vehicle_]->Value(),
              /*end*/ 10 - /*start*/ 3 - /*fixed_transits*/ 5);
    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  Solver* solver = model_->solver();
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

class PathSpansAndTotalSlacksAndBreaksTest
    : public ModelPropagationTest,
      public ::testing::WithParamInterface<int> {
 protected:
  void SetUp() override {
    ModelPropagationTest::SetUp();
    const int duration_callback =
        model_->RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
    model_->AddDimension(duration_callback, kint64max, kint64max, false,
                         "time");
    // First interval must be inside route, second one depends on the scenario.
    break_intervals_.push_back(
        model_->solver()->MakeFixedDurationIntervalVar(3, 9, 2, false, ""));
    break_intervals_.push_back(
        model_->solver()->MakeFixedDurationIntervalVar(0, 14, 2, false, ""));
    Dimension* const time = model_->GetMutableDimension("time");
    std::vector<int64_t> visits(time->cumuls().size(), 0);
    time->SetBreakIntervalsOfVehicle(break_intervals_, tested_vehicle_, visits);
    model_->CloseModel();
  }

  std::vector<IntervalVar*> break_intervals_;
  struct Scenario {
    int64_t initial_start_min;
    int64_t initial_end_max;
    int64_t result_start_min;
    int64_t result_end_max;
    int64_t result_route_min;
    int64_t result_route_max;
  };
  std::vector<Scenario> scenarios_ = {
      {0, 14, 0, 14, 0, 14},  // Interval can be either side.
      {0, 11, 0, 4, 2, 14},   // Interval must be before route.
      {3, 14, 10, 14, 0, 12}  // Interval must be after route.
  };
};

TEST_P(PathSpansAndTotalSlacksAndBreaksTest, BreaksPropagation) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  Solver* solver = model_->solver();
  std::function<Decision*(Solver*)> callback = [&](Solver* solver) {
    const int s = GetParam();
    break_intervals_[1]->SetStartMin(scenarios_[s].initial_start_min);
    break_intervals_[1]->SetEndMax(scenarios_[s].initial_end_max);

    spans[tested_vehicle_] = solver->MakeIntVar(0, 15);
    total_slacks[tested_vehicle_] = solver->MakeIntVar(0, 3);
    model_->AddToAssignment(total_slacks[tested_vehicle_]);

    Dimension* const time = model_->GetMutableDimension("time");
    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);

    // Full route has domain [0, 14), transits take 10 and both breaks 2,
    // so both breaks could go in. Only total slack can prevent it.
    const int64_t start = model_->Start(tested_vehicle_);
    const int64_t end = model_->End(tested_vehicle_);
    time->CumulVar(start)->SetMin(0);
    time->CumulVar(end)->SetMax(14);

    EXPECT_GE(break_intervals_[1]->StartMin(), scenarios_[s].result_start_min);
    EXPECT_LE(break_intervals_[1]->EndMax(), scenarios_[s].result_end_max);
    EXPECT_GE(time->CumulVar(start)->Min(), scenarios_[s].result_route_min);
    EXPECT_LE(time->CumulVar(end)->Max(), scenarios_[s].result_route_max);
    EXPECT_EQ(total_slacks[tested_vehicle_]->Min(), 2);  // Forced interval.
    EXPECT_EQ(spans[tested_vehicle_]->Min(), 10 + 2);

    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

INSTANTIATE_TEST_SUITE_P(PathSpansAndTotalSlacksAndBreaksTestScenario,
                         PathSpansAndTotalSlacksAndBreaksTest,
                         ::testing::Range(0, 3));

TEST_F(ModelPropagationTest, PathSpansPropagatesStartEndToSpan) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  const int duration_callback =
      model_->RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
  model_->AddDimension(duration_callback, kint64max, kint64max, false, "time");
  model_->CloseModel();

  std::function<Decision*(Solver*)> callback = [&](Solver* solver) {
    spans[tested_vehicle_] = solver->MakeIntVar(0, 20);

    Dimension* const time = model_->GetMutableDimension("time");
    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);

    // Setting start and end should fix the span.
    const int64_t start = model_->Start(tested_vehicle_);
    const int64_t end = model_->End(tested_vehicle_);
    time->CumulVar(start)->SetValue(1);
    time->CumulVar(end)->SetValue(16);

    EXPECT_TRUE(spans[tested_vehicle_]->Bound());
    EXPECT_EQ(spans[tested_vehicle_]->Value(), 16 - 1);

    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  Solver* solver = model_->solver();
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

TEST_F(ModelPropagationTest, PathSpansPropagatesSpanMaxToStartMinEndMax) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  const int duration_callback =
      model_->RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
  model_->AddDimension(duration_callback, kint64max, kint64max, false, "time");
  model_->CloseModel();

  std::function<Decision*(Solver*)> callback = [&](Solver* solver) {
    spans[tested_vehicle_] = solver->MakeIntVar(0, 30);
    Dimension* const time = model_->GetMutableDimension("time");
    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);
    // Setting the span max should fix both start and end.
    const int64_t start = model_->Start(tested_vehicle_);
    const int64_t end = model_->End(tested_vehicle_);
    time->CumulVar(start)->SetRange(0, 5);
    time->CumulVar(end)->SetRange(25, 30);
    spans[tested_vehicle_]->SetMax(20);

    EXPECT_TRUE(time->CumulVar(start)->Bound());
    EXPECT_EQ(time->CumulVar(start)->Value(), 5);
    EXPECT_TRUE(time->CumulVar(end)->Bound());
    EXPECT_EQ(time->CumulVar(end)->Value(), 25);

    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  Solver* solver = model_->solver();
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

TEST_F(ModelPropagationTest, PathSpansPropagatesSpanMinToStartMaxEndMin) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  const int duration_callback =
      model_->RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
  model_->AddDimension(duration_callback, kint64max, kint64max, false, "time");
  model_->CloseModel();

  std::function<Decision*(Solver*)> callback = [&](Solver* solver) {
    spans[tested_vehicle_] = solver->MakeIntVar(0, 40);
    Dimension* const time = model_->GetMutableDimension("time");
    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);

    // Setting the span min should fix both start and end.
    const int64_t start = model_->Start(tested_vehicle_);
    const int64_t end = model_->End(tested_vehicle_);
    time->CumulVar(start)->SetRange(0, 5);
    time->CumulVar(end)->SetRange(25, 30);
    spans[tested_vehicle_]->SetMin(30);

    EXPECT_TRUE(time->CumulVar(start)->Bound());
    EXPECT_EQ(time->CumulVar(start)->Value(), 0);
    EXPECT_TRUE(time->CumulVar(end)->Bound());
    EXPECT_EQ(time->CumulVar(end)->Value(), 30);

    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  Solver* solver = model_->solver();
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

TEST_F(ModelPropagationTest, PathSpansPropagatesTransitsToSpan) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  const int duration_callback =
      model_->RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
  model_->AddDimension(duration_callback, kint64max, kint64max, false, "time");
  model_->CloseModel();

  std::function<Decision*(Solver*)> callback = [&](Solver* solver) {
    spans[tested_vehicle_] = solver->MakeIntVar(0, 20);

    Dimension* const time = model_->GetMutableDimension("time");
    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);

    // Path is start 3 2 4 1 end, fixing all transits should fix the span.
    const int64_t start = model_->Start(tested_vehicle_);
    time->TransitVar(start)->SetValue(3);
    time->TransitVar(3)->SetValue(3);
    time->TransitVar(2)->SetValue(3);
    time->TransitVar(4)->SetValue(3);
    time->TransitVar(1)->SetValue(3);

    EXPECT_TRUE(spans[tested_vehicle_]->Bound());
    EXPECT_EQ(spans[tested_vehicle_]->Value(), 5 * 3);

    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  Solver* solver = model_->solver();
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

TEST_F(ModelPropagationTest, PathSpansPropagatesSpanMaxToTransitsMax) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  const int duration_callback =
      model_->RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
  model_->AddDimension(duration_callback, kint64max, kint64max, false, "time");
  model_->CloseModel();

  std::function<Decision*(Solver*)> callback = [&](Solver* solver) {
    spans[tested_vehicle_] = solver->MakeIntVar(0, 20);
    Dimension* const time = model_->GetMutableDimension("time");
    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);

    // Path is start 3 2 4 1 end, setting a span max should fix the transits.
    const int64_t start = model_->Start(tested_vehicle_);
    time->TransitVar(start)->SetRange(2, 3);
    time->TransitVar(3)->SetRange(2, 3);
    time->TransitVar(2)->SetRange(2, 3);
    time->TransitVar(4)->SetRange(2, 3);
    time->TransitVar(1)->SetRange(2, 3);
    spans[tested_vehicle_]->SetMax(10);

    EXPECT_TRUE(time->TransitVar(start)->Bound());
    EXPECT_TRUE(time->TransitVar(3)->Bound());
    EXPECT_TRUE(time->TransitVar(2)->Bound());
    EXPECT_TRUE(time->TransitVar(4)->Bound());
    EXPECT_TRUE(time->TransitVar(1)->Bound());
    EXPECT_EQ(time->TransitVar(start)->Value(), 2);
    EXPECT_EQ(time->TransitVar(3)->Value(), 2);
    EXPECT_EQ(time->TransitVar(2)->Value(), 2);
    EXPECT_EQ(time->TransitVar(4)->Value(), 2);
    EXPECT_EQ(time->TransitVar(1)->Value(), 2);
    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  Solver* solver = model_->solver();
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

TEST_F(ModelPropagationTest, PathSpansPropagatesSpanMinToTransitsMin) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  const int duration_callback =
      model_->RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
  model_->AddDimension(duration_callback, kint64max, kint64max, false, "time");
  model_->CloseModel();

  std::function<Decision*(Solver*)> callback = [&](Solver* solver) {
    spans[tested_vehicle_] = solver->MakeIntVar(0, 20);
    Dimension* const time = model_->GetMutableDimension("time");
    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);

    // Path is start 3 2 4 1 end, setting a span min should fix the transits.
    const int64_t start = model_->Start(tested_vehicle_);
    time->TransitVar(start)->SetRange(2, 3);
    time->TransitVar(3)->SetRange(2, 3);
    time->TransitVar(2)->SetRange(2, 3);
    time->TransitVar(4)->SetRange(2, 3);
    time->TransitVar(1)->SetRange(2, 3);
    spans[tested_vehicle_]->SetMin(15);

    EXPECT_TRUE(time->TransitVar(start)->Bound());
    EXPECT_TRUE(time->TransitVar(3)->Bound());
    EXPECT_TRUE(time->TransitVar(2)->Bound());
    EXPECT_TRUE(time->TransitVar(4)->Bound());
    EXPECT_TRUE(time->TransitVar(1)->Bound());
    EXPECT_EQ(time->TransitVar(start)->Value(), 3);
    EXPECT_EQ(time->TransitVar(3)->Value(), 3);
    EXPECT_EQ(time->TransitVar(2)->Value(), 3);
    EXPECT_EQ(time->TransitVar(4)->Value(), 3);
    EXPECT_EQ(time->TransitVar(1)->Value(), 3);
    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  Solver* solver = model_->solver();
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

TEST_F(ModelPropagationTest, PathSpanSynchronizesSpanAndTotalSlack) {
  std::vector<IntVar*> spans(num_vehicles_);
  std::vector<IntVar*> total_slacks(num_vehicles_);
  const int duration_callback =
      model_->RegisterTransitCallback([](int64_t, int64_t) { return 2LL; });
  model_->AddDimension(duration_callback, kint64max, kint64max, false, "time");
  model_->CloseModel();

  std::function<Decision*(Solver*)> callback = [&](Solver* solver) {
    spans[tested_vehicle_] = solver->MakeIntVar(0, 100);
    total_slacks[tested_vehicle_] = solver->MakeIntVar(0, 100);
    Dimension* const time = model_->GetMutableDimension("time");
    Constraint* ct = MakePathSpansAndTotalSlacks(time, spans, total_slacks);
    solver->AddConstraint(ct);

    // Path is start 3 2 4 1 end, sum of fixed transits is 10,
    // so span == total_slack + 10.
    // Test synchronization span -> total_slack
    spans[tested_vehicle_]->SetRange(20, 80);
    EXPECT_EQ(total_slacks[tested_vehicle_]->Min(), 10);
    EXPECT_EQ(total_slacks[tested_vehicle_]->Max(), 70);

    // Test synchronization total_slacks -> span
    total_slacks[tested_vehicle_]->SetRange(20, 60);
    EXPECT_EQ(spans[tested_vehicle_]->Min(), 30);
    EXPECT_EQ(spans[tested_vehicle_]->Max(), 70);
    return nullptr;
  };

  // Expect decision to be nullptr, so this should be a solution.
  Solver* solver = model_->solver();
  DecisionBuilder* tester = solver->RevAlloc(new TestDecisionBuilder(callback));
  EXPECT_TRUE(solver->Solve(tester));
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint, NoVehicleActive,
                    solver, true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 1, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(3, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {20, 10, 30};
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, transit_vars, active_vars, vehicle_active_vars,
        vehicle_capacities, /*max_active_vehicles=*/0,
        enforce_active_vehicles));
    for (IntVar* vehicle_active_var : vehicle_active_vars) {
      EXPECT_EQ(vehicle_active_var->Min(), 0);
      EXPECT_EQ(vehicle_active_var->Max(), 0);
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint, HighMaxActiveVehicles,
                    solver, true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 1, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(3, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {20, 10, 30};
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, transit_vars, active_vars, vehicle_active_vars,
        vehicle_capacities, /*max_active_vehicles=*/10,
        enforce_active_vehicles));
    for (IntVar* vehicle_active_var : vehicle_active_vars) {
      EXPECT_EQ(vehicle_active_var->Min(), 0);
      EXPECT_EQ(vehicle_active_var->Max(), 1);
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    NoNodeActiveOnActiveVehicle, solver, true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 1, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(3, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {5, 5, 5};
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, transit_vars, active_vars, vehicle_active_vars,
        vehicle_capacities, /*max_active_vehicles=*/1,
        enforce_active_vehicles));
    for (IntVar* vehicle_active_var : vehicle_active_vars) {
      EXPECT_EQ(vehicle_active_var->Min(), 0);
      EXPECT_EQ(vehicle_active_var->Max(), 1);
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    AllNodesActiveOneActiveVehicle, solver, true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 1, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    for (IntVar* active_var : active_vars) {
      active_var->SetValue(1);
    }
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(3, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {5, 15, 5};
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, transit_vars, active_vars, vehicle_active_vars,
        vehicle_capacities, /*max_active_vehicles=*/1,
        enforce_active_vehicles));

    for (int expected_inactive : {0, 2}) {
      IntVar* vehicle_active_var = vehicle_active_vars[expected_inactive];
      EXPECT_EQ(vehicle_active_var->Min(), 0);
      EXPECT_EQ(vehicle_active_var->Max(), 0);
    }
    if (enforce_active_vehicles) {
      for (int expected_active : {1}) {
        IntVar* vehicle_active_var = vehicle_active_vars[expected_active];
        EXPECT_EQ(vehicle_active_var->Min(), 1);
        EXPECT_EQ(vehicle_active_var->Max(), 1);
      }
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint, AllNodesActive, solver,
                    true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 1, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    for (IntVar* active_var : active_vars) {
      active_var->SetValue(1);
    }
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(3, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {4, 6, 1};
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, transit_vars, active_vars, vehicle_active_vars,
        vehicle_capacities, /*max_active_vehicles=*/2,
        enforce_active_vehicles));

    for (int expected_inactive : {2}) {
      IntVar* vehicle_active_var = vehicle_active_vars[expected_inactive];
      EXPECT_EQ(vehicle_active_var->Min(), 0);
      EXPECT_EQ(vehicle_active_var->Max(), 0);
    }
    if (enforce_active_vehicles) {
      for (int expected_active : {1}) {
        IntVar* vehicle_active_var = vehicle_active_vars[expected_active];
        EXPECT_EQ(vehicle_active_var->Min(), 1);
        EXPECT_EQ(vehicle_active_var->Max(), 1);
      }
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    AllNodesActiveAllVehiclePairsMaybeActive, solver, true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 1, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    for (IntVar* active_var : active_vars) {
      active_var->SetValue(1);
    }
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(3, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {5, 10, 5};
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, transit_vars, active_vars, vehicle_active_vars,
        vehicle_capacities, /*max_active_vehicles=*/2,
        enforce_active_vehicles));
    for (IntVar* vehicle_active_var : vehicle_active_vars) {
      EXPECT_EQ(vehicle_active_var->Min(), 0);
      EXPECT_EQ(vehicle_active_var->Max(), 1);
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    AllNodesActiveAllNeedsAllVehicles, solver, true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 1, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    for (IntVar* active_var : active_vars) {
      active_var->SetValue(1);
    }
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(2, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {5, 5};
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, transit_vars, active_vars, vehicle_active_vars,
        vehicle_capacities, /*max_active_vehicles=*/2,
        enforce_active_vehicles));
    if (enforce_active_vehicles) {
      for (IntVar* vehicle_active_var : vehicle_active_vars) {
        EXPECT_EQ(vehicle_active_var->Min(), 1);
        EXPECT_EQ(vehicle_active_var->Max(), 1);
      }
    }
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint, MissingCapacity,
                    solver, false) {
  std::vector<IntVar*> transit_vars;
  solver->MakeIntVarArray(10, 1, 100, &transit_vars);
  std::vector<IntVar*> active_vars;
  solver->MakeBoolVarArray(10, &active_vars);
  for (IntVar* active_var : active_vars) {
    active_var->SetValue(1);
  }
  std::vector<IntVar*> vehicle_active_vars;
  solver->MakeBoolVarArray(3, &vehicle_active_vars);
  std::vector<int64_t> vehicle_capacities = {3, 5, 4};
  solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
      solver, std::move(transit_vars), std::move(active_vars),
      std::move(vehicle_active_vars), std::move(vehicle_capacities),
      /*max_active_vehicles=*/2));
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    MissingCapacityAllVehiclesActive, solver, false) {
  std::vector<IntVar*> transit_vars;
  solver->MakeIntVarArray(10, 1, 100, &transit_vars);
  std::vector<IntVar*> active_vars;
  solver->MakeBoolVarArray(10, &active_vars);
  for (IntVar* active_var : active_vars) {
    active_var->SetValue(1);
  }
  std::vector<IntVar*> vehicle_active_vars;
  solver->MakeBoolVarArray(3, &vehicle_active_vars);
  std::vector<int64_t> vehicle_capacities = {1, 5, 3};
  solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
      solver, std::move(transit_vars), std::move(active_vars),
      std::move(vehicle_active_vars), std::move(vehicle_capacities),
      /*max_active_vehicles=*/3));
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    MissingCapacityDueToInactiveVehicle, solver, false) {
  std::vector<IntVar*> transit_vars;
  solver->MakeIntVarArray(10, 1, 100, &transit_vars);
  std::vector<IntVar*> active_vars;
  solver->MakeBoolVarArray(10, &active_vars);
  for (IntVar* active_var : active_vars) {
    active_var->SetValue(1);
  }
  std::vector<IntVar*> vehicle_active_vars;
  solver->MakeBoolVarArray(3, &vehicle_active_vars);
  vehicle_active_vars[1]->SetValue(0);
  std::vector<int64_t> vehicle_capacities = {3, 15, 4};
  solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
      solver, std::move(transit_vars), std::move(active_vars),
      std::move(vehicle_active_vars), std::move(vehicle_capacities),
      /*max_active_vehicles=*/1));
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    MissingCapacityDueToSeveralInactiveVehicles, solver,
                    false) {
  std::vector<IntVar*> transit_vars;
  solver->MakeIntVarArray(10, 1, 100, &transit_vars);
  std::vector<IntVar*> active_vars;
  solver->MakeBoolVarArray(10, &active_vars);
  for (IntVar* active_var : active_vars) {
    active_var->SetValue(1);
  }
  std::vector<IntVar*> vehicle_active_vars;
  solver->MakeBoolVarArray(5, &vehicle_active_vars);
  vehicle_active_vars[1]->SetValue(0);
  vehicle_active_vars[3]->SetValue(0);
  std::vector<int64_t> vehicle_capacities = {3, 15, 4, 15, 1};
  solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
      solver, std::move(transit_vars), std::move(active_vars),
      std::move(vehicle_active_vars), std::move(vehicle_capacities),
      /*max_active_vehicles=*/4));
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    AllNodesActiveSomeVehiclesInactive, solver, true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 1, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    for (IntVar* active_var : active_vars) {
      active_var->SetValue(1);
    }
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(4, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {8, 2, 1, 8};
    vehicle_active_vars[0]->SetValue(0);
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, transit_vars, active_vars, vehicle_active_vars,
        vehicle_capacities, /*max_active_vehicles=*/2,
        enforce_active_vehicles));

    for (int expected_inactive : {2}) {
      IntVar* vehicle_active_var = vehicle_active_vars[expected_inactive];
      EXPECT_EQ(vehicle_active_var->Min(), 0);
      EXPECT_EQ(vehicle_active_var->Max(), 0);
    }
    // One of vehicle 0 and 3 is active (they both have capacity 8).
    int count = 0;
    for (int expected_active : {0, 3}) {
      if (vehicle_active_vars[expected_active]->Min() == 1) ++count;
    }
    EXPECT_EQ(count, enforce_active_vehicles ? 1 : 0);
  }
  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint, AllVehiclesInactive,
                    solver, false) {
  std::vector<IntVar*> transit_vars;
  solver->MakeIntVarArray(10, 1, 100, &transit_vars);
  std::vector<IntVar*> active_vars;
  solver->MakeBoolVarArray(10, &active_vars);
  for (IntVar* active_var : active_vars) {
    active_var->SetValue(1);
  }
  std::vector<IntVar*> vehicle_active_vars;
  solver->MakeBoolVarArray(3, &vehicle_active_vars);
  std::vector<int64_t> vehicle_capacities = {10, 10, 10};
  vehicle_active_vars[0]->SetValue(0);
  vehicle_active_vars[1]->SetValue(0);
  vehicle_active_vars[2]->SetValue(0);
  solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
      solver, std::move(transit_vars), std::move(active_vars),
      std::move(vehicle_active_vars), std::move(vehicle_capacities),
      /*max_active_vehicles=*/2));

  return nullptr;
}

TEST_IN_PROPAGATION(NumActiveVehiclesCapacityConstraint,
                    AllVehiclesInactiveNoDemand, solver, true) {
  for (bool enforce_active_vehicles : {true, false}) {
    std::vector<IntVar*> transit_vars;
    solver->MakeIntVarArray(10, 0, 100, &transit_vars);
    std::vector<IntVar*> active_vars;
    solver->MakeBoolVarArray(10, &active_vars);
    for (IntVar* active_var : active_vars) {
      active_var->SetValue(1);
    }
    std::vector<IntVar*> vehicle_active_vars;
    solver->MakeBoolVarArray(3, &vehicle_active_vars);
    std::vector<int64_t> vehicle_capacities = {10, 10, 10};
    vehicle_active_vars[0]->SetValue(0);
    vehicle_active_vars[1]->SetValue(0);
    vehicle_active_vars[2]->SetValue(0);
    solver->AddConstraint(MakeNumActiveVehiclesCapacityConstraint(
        solver, std::move(transit_vars), std::move(active_vars),
        std::move(vehicle_active_vars), std::move(vehicle_capacities),
        /*max_active_vehicles=*/2, enforce_active_vehicles));
  }
  return nullptr;
}

// TODO(user): Add unit tests for remaining constraints.

}  // namespace
}  // namespace operations_research::routing
