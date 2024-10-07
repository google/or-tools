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

#include "ortools/sat/work_assignment.h"

#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {
namespace {

TEST(ProtoTrailTest, PushLevel) {
  ProtoTrail p;
  p.PushLevel({0, 0}, 0, 1);

  EXPECT_EQ(p.MaxLevel(), 1);
  EXPECT_EQ(p.Decision(1), ProtoLiteral(0, 0));
  EXPECT_EQ(p.ObjectiveLb(1), 0);
}

TEST(ProtoTrailTest, AddImplications) {
  ProtoTrail p;
  p.PushLevel({0, 0}, 0, 1);
  p.PushLevel({1, 0}, 1, 2);
  p.PushLevel({2, 0}, 2, 3);
  p.PushLevel({3, 0}, 2, 4);

  p.AddImplication(2, {5, 0});
  p.AddImplication(3, {6, 0});

  EXPECT_THAT(p.Implications(2), testing::ElementsAre(ProtoLiteral(5, 0)));
  EXPECT_THAT(p.Implications(3), testing::ElementsAre(ProtoLiteral(6, 0)));
  p.SetLevelImplied(3);
  EXPECT_THAT(p.Implications(2),
              testing::UnorderedElementsAre(
                  ProtoLiteral(5, 0), ProtoLiteral(2, 0), ProtoLiteral(6, 0)));
}

TEST(ProtoTrailTest, SetLevel1Implied) {
  ProtoTrail p;
  p.PushLevel({0, 0}, 0, 1);
  p.PushLevel({1, 0}, 1, 2);
  p.PushLevel({2, 0}, 2, 3);

  p.SetLevelImplied(1);

  EXPECT_THAT(p.NodeIds(0), testing::ElementsAre(1));
  EXPECT_THAT(p.NodeIds(1), testing::ElementsAre(2));
  EXPECT_THAT(p.NodeIds(2), testing::ElementsAre(3));
  EXPECT_EQ(p.MaxLevel(), 2);
  EXPECT_EQ(p.Decision(1), ProtoLiteral(1, 0));
  EXPECT_EQ(p.Decision(2), ProtoLiteral(2, 0));
  EXPECT_EQ(p.ObjectiveLb(1), 1);
  EXPECT_EQ(p.ObjectiveLb(2), 2);
}

TEST(ProtoTrailTest, SetMidLevelImplied) {
  ProtoTrail p;
  p.PushLevel({0, 0}, 0, 1);
  p.PushLevel({1, 0}, 1, 2);
  p.PushLevel({2, 0}, 2, 3);

  p.SetLevelImplied(2);

  EXPECT_THAT(p.NodeIds(0), testing::IsEmpty());
  EXPECT_THAT(p.NodeIds(1), testing::ElementsAre(1, 2));
  EXPECT_THAT(p.NodeIds(2), testing::ElementsAre(3));
  EXPECT_EQ(p.MaxLevel(), 2);
  EXPECT_EQ(p.Decision(1), ProtoLiteral(0, 0));
  EXPECT_EQ(p.Decision(2), ProtoLiteral(2, 0));
  EXPECT_EQ(p.ObjectiveLb(1), 1);
  EXPECT_EQ(p.ObjectiveLb(2), 2);
}

TEST(ProtoTrailTest, SetFinalLevelImplied) {
  ProtoTrail p;
  p.PushLevel({0, 0}, 0, 1);
  p.PushLevel({1, 0}, 1, 2);
  p.PushLevel({2, 0}, 2, 3);

  p.SetLevelImplied(3);

  EXPECT_THAT(p.NodeIds(0), testing::IsEmpty());
  EXPECT_THAT(p.NodeIds(1), testing::ElementsAre(1));
  EXPECT_THAT(p.NodeIds(2), testing::ElementsAre(2, 3));
  EXPECT_EQ(p.MaxLevel(), 2);
  EXPECT_EQ(p.Decision(1), ProtoLiteral(0, 0));
  EXPECT_EQ(p.Decision(2), ProtoLiteral(1, 0));
  EXPECT_EQ(p.ObjectiveLb(1), 0);
  EXPECT_EQ(p.ObjectiveLb(2), 2);
}

TEST(ProtoTrailTest, SetMultiLevelImplied) {
  ProtoTrail p;
  p.PushLevel({0, 0}, 0, 1);
  p.PushLevel({1, 0}, 1, 2);
  p.PushLevel({2, 0}, 2, 3);

  p.SetLevelImplied(3);
  p.SetLevelImplied(1);

  EXPECT_EQ(p.MaxLevel(), 1);
  EXPECT_EQ(p.Decision(1), ProtoLiteral(1, 0));
  EXPECT_EQ(p.ObjectiveLb(1), 2);
}

TEST(ProtoTrailTest, Clear) {
  ProtoTrail p;
  p.PushLevel({0, 0}, 0, 1);
  p.PushLevel({1, 0}, 1, 2);
  p.PushLevel({2, 0}, 2, 3);

  p.Clear();

  EXPECT_EQ(p.MaxLevel(), 0);
}

class SharedTreeSolveTest : public testing::TestWithParam<absl::string_view> {
 public:
  SatParameters GetParams() {
    SatParameters params;
    params.set_num_workers(4);
    params.set_shared_tree_num_workers(4);
    params.set_cp_model_presolve(false);
    params.MergeFrom(
        google::protobuf::contrib::parse_proto::ParseTextProtoOrDie(
            GetParam()));
    return params;
  }
};
INSTANTIATE_TEST_SUITE_P(
    SharedTreeParams, SharedTreeSolveTest,
    testing::Values("shared_tree_worker_enable_trail_sharing:false",
                    "shared_tree_worker_enable_trail_sharing:true"));

TEST_P(SharedTreeSolveTest, SmokeTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var + 5 * bool_var);
  Model model;
  SatParameters params = GetParams();
  model.Add(NewSatParameters(params));

  CpSolverResponse response = SolveCpModel(model_builder.Build(), &model);

  EXPECT_EQ(model.GetOrCreate<SharedTreeManager>()->NumWorkers(),
            params.shared_tree_num_workers());
  ASSERT_EQ(response.status(), OPTIMAL)
      << "Validation: " << ValidateCpModel(model_builder.Build());
  EXPECT_EQ(response.objective_value(), 5 + 3);
  EXPECT_EQ(SolutionBooleanValue(response, bool_var), true);
  EXPECT_EQ(SolutionIntegerValue(response, int_var), 3);
}

TEST_P(SharedTreeSolveTest, FeasiblePidgeonHoleSmokeTest) {
  CpModelBuilder model_builder;
  const int pidgeons = 10;
  const int holes = 10;
  std::vector<LinearExpr> count_per_hole(holes);
  IntVar max_pidgeon_hole_product =
      model_builder.NewIntVar({0, pidgeons * holes});
  for (int i = 0; i < pidgeons; ++i) {
    LinearExpr count_per_pidgeon;
    for (int j = 0; j < holes; ++j) {
      auto var = model_builder.NewBoolVar();
      count_per_hole[j] += LinearExpr(var);
      count_per_pidgeon += LinearExpr(var);
      model_builder
          .AddGreaterOrEqual(max_pidgeon_hole_product, (i + 1) * (j + 1))
          .OnlyEnforceIf(var);
    }
    model_builder.AddEquality(count_per_pidgeon, 1);
  }
  for (const auto& count : count_per_hole) {
    model_builder.AddLessOrEqual(count, 1);
  }
  Model model;
  SatParameters params = GetParams();
  model.Add(NewSatParameters(params));

  CpSolverResponse response = SolveCpModel(model_builder.Build(), &model);

  EXPECT_EQ(model.GetOrCreate<SharedTreeManager>()->NumWorkers(), 4);
  EXPECT_EQ(response.status(), OPTIMAL);
}

TEST_P(SharedTreeSolveTest, InfeasiblePidgeonHoleSmokeTest) {
  CpModelBuilder model_builder;
  const int pidgeons = 10;
  const int holes = 9;
  std::vector<LinearExpr> count_per_hole(holes);
  IntVar max_pidgeon_hole_product =
      model_builder.NewIntVar({0, pidgeons * holes});
  for (int i = 0; i < pidgeons; ++i) {
    LinearExpr count_per_pidgeon;
    for (int j = 0; j < holes; ++j) {
      auto var = model_builder.NewBoolVar();
      count_per_hole[j] += LinearExpr(var);
      count_per_pidgeon += LinearExpr(var);
      model_builder
          .AddGreaterOrEqual(max_pidgeon_hole_product, (i + 1) * (j + 1))
          .OnlyEnforceIf(var);
    }
    model_builder.AddEquality(count_per_pidgeon, 1);
  }
  for (const auto& count : count_per_hole) {
    model_builder.AddLessOrEqual(count, 1);
  }
  Model model;
  SatParameters params = GetParams();
  model.Add(NewSatParameters(params));

  CpSolverResponse response = SolveCpModel(model_builder.Build(), &model);

  EXPECT_EQ(model.GetOrCreate<SharedTreeManager>()->NumWorkers(), 4);
  EXPECT_EQ(response.status(), INFEASIBLE);
}

TEST(SharedTreeManagerTest, SplitTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail shared_trail;

  shared_tree_manager->ProposeSplit(shared_trail, {-1, 0});

  EXPECT_EQ(shared_trail.MaxLevel(), 1);
}

TEST(SharedTreeManagerTest, RestartTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail shared_trail;

  shared_tree_manager->ProposeSplit(shared_trail, {-1, 0});
  shared_tree_manager->Restart();
  shared_tree_manager->SyncTree(shared_trail);

  EXPECT_EQ(shared_trail.MaxLevel(), 0);
}

TEST(SharedTreeManagerTest, RestartTestWithLevelZeroImplications) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail shared_trail;

  shared_tree_manager->ProposeSplit(shared_trail, {-1, 0});
  shared_tree_manager->CloseTree(shared_trail, 1);
  shared_tree_manager->SyncTree(shared_trail);
  shared_tree_manager->ReplaceTree(shared_trail);
  shared_tree_manager->Restart();
  shared_tree_manager->SyncTree(shared_trail);

  EXPECT_EQ(shared_trail.NodeIds(0).size(), 0);
  EXPECT_EQ(shared_trail.MaxLevel(), 0);
}

TEST(SharedTreeManagerTest, SharedBranchingTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(2);
  params.set_shared_tree_num_workers(2);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail trail1, trail2;

  shared_tree_manager->ProposeSplit(trail1, {-1, 0});
  shared_tree_manager->ReplaceTree(trail2);

  EXPECT_EQ(trail1.MaxLevel(), 1);
  EXPECT_EQ(trail2.MaxLevel(), 1);
  EXPECT_EQ(trail1.Decision(1), trail2.Decision(1).Negated());
}

TEST(SharedTreeManagerTest, ObjectiveLbSplitTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_cp_model_presolve(false);
  params.set_shared_tree_split_strategy(
      SatParameters::SPLIT_STRATEGY_OBJECTIVE_LB);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail trail1, trail2;

  shared_tree_manager->ProposeSplit(trail1, {-1, 0});
  ASSERT_EQ(trail1.MaxLevel(), 1);
  trail1.SetObjectiveLb(1, 2);
  shared_tree_manager->SyncTree(trail1);
  shared_tree_manager->ReplaceTree(trail2);
  ASSERT_EQ(trail2.MaxLevel(), 1);
  trail2.SetObjectiveLb(1, 1);
  shared_tree_manager->SyncTree(trail2);
  // Reject this split because it is not at the global lower bound.
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 3});

  EXPECT_EQ(response_manager->GetInnerObjectiveLowerBound(), 1);
  EXPECT_EQ(shared_tree_manager->NumNodes(), 3);
}

TEST(SharedTreeManagerTest, DiscrepancySplitTestOneLeafPerWorker) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_shared_tree_open_leaves_per_worker(1);
  params.set_cp_model_presolve(false);
  params.set_shared_tree_split_strategy(
      SatParameters::SPLIT_STRATEGY_DISCREPANCY);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail trail1, trail2;

  shared_tree_manager->ProposeSplit(trail1, {-1, 0});
  shared_tree_manager->SyncTree(trail1);
  shared_tree_manager->ReplaceTree(trail2);
  shared_tree_manager->ProposeSplit(trail2, {int_var.index(), 3});
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 3});
  // Reject this split: 2 depth + 1 discrepancy is not minimal.
  shared_tree_manager->ProposeSplit(trail2, {int_var.index(), 5});
  // Reject this split: 2 depth  + 0 discrepancy is not minimal.
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 5});

  EXPECT_EQ(trail1.MaxLevel(), 2);
  EXPECT_EQ(trail2.MaxLevel(), 2);
  EXPECT_EQ(shared_tree_manager->NumNodes(), 7);
}

TEST(SharedTreeManagerTest, DiscrepancySplitTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(2);
  params.set_shared_tree_num_workers(2);
  params.set_shared_tree_open_leaves_per_worker(2);
  params.set_cp_model_presolve(false);
  params.set_shared_tree_split_strategy(
      SatParameters::SPLIT_STRATEGY_DISCREPANCY);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail trail1, trail2;

  shared_tree_manager->ProposeSplit(trail1, {-1, 0});
  shared_tree_manager->SyncTree(trail1);
  shared_tree_manager->ReplaceTree(trail2);
  shared_tree_manager->ProposeSplit(trail2, {int_var.index(), 3});
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 3});
  // Reject this split: 2 depth + 1 discrepancy is not minimal.
  shared_tree_manager->ProposeSplit(trail2, {int_var.index(), 5});
  // Reject this split: 2 depth  + 0 discrepancy is not minimal.
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 5});

  EXPECT_EQ(trail1.MaxLevel(), 2);
  EXPECT_EQ(trail2.MaxLevel(), 2);
  EXPECT_EQ(shared_tree_manager->NumNodes(), 7);
}

TEST(SharedTreeManagerTest, BalancedSplitTestOneLeafPerWorker) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(5);
  params.set_shared_tree_num_workers(5);
  params.set_shared_tree_open_leaves_per_worker(1);
  params.set_cp_model_presolve(false);
  params.set_shared_tree_split_strategy(
      SatParameters::SPLIT_STRATEGY_BALANCED_TREE);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail trail1, trail2;

  shared_tree_manager->ProposeSplit(trail1, {-1, 0});
  shared_tree_manager->SyncTree(trail1);
  shared_tree_manager->ReplaceTree(trail2);
  shared_tree_manager->SyncTree(trail2);
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 3});
  // Reject this split because it creates an unbalanced tree
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 5});
  shared_tree_manager->ProposeSplit(trail2, {int_var.index(), 3});

  EXPECT_EQ(shared_tree_manager->NumNodes(), 7);
  EXPECT_EQ(trail1.MaxLevel(), 2);
  EXPECT_EQ(trail2.MaxLevel(), 2);
}

TEST(SharedTreeManagerTest, BalancedSplitTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(3);
  params.set_shared_tree_num_workers(3);
  params.set_shared_tree_open_leaves_per_worker(2);
  params.set_cp_model_presolve(false);
  params.set_shared_tree_split_strategy(
      SatParameters::SPLIT_STRATEGY_BALANCED_TREE);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail trail1, trail2;

  shared_tree_manager->ProposeSplit(trail1, {-1, 0});
  shared_tree_manager->SyncTree(trail1);
  shared_tree_manager->ReplaceTree(trail2);
  shared_tree_manager->SyncTree(trail2);
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 3});
  // Reject this split because it creates an unbalanced tree
  shared_tree_manager->ProposeSplit(trail1, {int_var.index(), 5});
  shared_tree_manager->ProposeSplit(trail2, {int_var.index(), 3});

  EXPECT_EQ(shared_tree_manager->NumNodes(), 7);
  EXPECT_EQ(trail1.MaxLevel(), 2);
  EXPECT_EQ(trail2.MaxLevel(), 2);
}

TEST(SharedTreeManagerTest, CloseTreeTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  ProtoTrail trail1, trail2, trail3;
  shared_tree_manager->ProposeSplit(trail1, {-1, 0});
  shared_tree_manager->ReplaceTree(trail2);
  shared_tree_manager->ProposeSplit(trail1, {1, 0});
  shared_tree_manager->CloseTree(trail1, 1);
  shared_tree_manager->ReplaceTree(trail1);

  EXPECT_EQ(trail1.MaxLevel(), 0);
  EXPECT_EQ(trail2.MaxLevel(), 1);
  EXPECT_EQ(trail2.Decision(1), ProtoLiteral(0, 1));
}
// TODO(user): Test objective propagation.
}  // namespace
}  // namespace sat
}  // namespace operations_research
