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

#include "ortools/sat/work_assignment.h"

#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/parse_text_proto.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_checker.h"
#include "ortools/sat/cp_model_loader.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/synchronization.h"

namespace operations_research {
namespace sat {
namespace {
using testing::Pair;
using testing::UnorderedElementsAre;

TEST(SharedTreeEncoderTest, ImportAndSplit) {
  SharedTreeEncoder encoder(nullptr);

  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));

  EXPECT_EQ(encoder.Size(), 3);
  EXPECT_EQ(encoder.GetNode(NodeId(2))->shared().decision, ProtoLiteral(0, 1));
  EXPECT_EQ(encoder.GetNode(NodeId(3))->shared().decision,
            ProtoLiteral(0, 1).Negated());
}

TEST(SharedTreeEncoderTest, ProcessNewBoundPropagatesUp) {
  SharedTreeEncoder encoder(nullptr);
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));

  encoder.ImportNode({.id = NodeId(2), .objective_lb = 2});
  encoder.ImportNode({.id = NodeId(3), .objective_lb = 2});

  EXPECT_EQ(encoder.GetNode(NodeId(1))->shared().objective_lb, 2);
}

TEST(SharedTreeEncoderTest, AddImplication) {
  CpModelBuilder model_builder;
  auto branch_var = model_builder.NewBoolVar();
  auto implied_var = model_builder.NewBoolVar();
  Model model;
  LoadVariables(model_builder.Build(), false, &model);
  SharedTreeEncoder encoder(nullptr, model.GetOrCreate<CpModelMapping>(),
                            model.GetOrCreate<IntegerEncoder>());
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.ImportNode({.id = NodeId(2),
                      .parent_id = NodeId(1),
                      .decision = ProtoLiteral(branch_var.index(), 1)});
  const Literal lit =
      model.GetOrCreate<CpModelMapping>()->Literal(implied_var.index());

  EXPECT_TRUE(encoder.GetNode(NodeId(2))->AddImplication(lit));
  EXPECT_THAT(encoder.GetNode(NodeId(2))->implications(),
              testing::ElementsAre(lit));
  EXPECT_THAT(encoder.GetNode(NodeId(2))->shared().var_lower_bounds,
              testing::ElementsAre(
                  testing::Pair(implied_var.index(), IntegerValue(1))));
}

TEST(SharedTreeEncoderTest, ImpliedNodeClosesSibling) {
  SharedTreeEncoder encoder(nullptr);
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  ASSERT_FALSE(encoder.GetNode(NodeId(3))->shared().is_closed());

  encoder.ImportNode({.id = NodeId(2), .objective_lb = 10, .is_implied = true});

  EXPECT_TRUE(encoder.GetNode(NodeId(3))->shared().is_closed());
  EXPECT_EQ(encoder.GetNode(NodeId(1))->shared().objective_lb, 10);
}

TEST(SharedTreeEncoderTest, ReasonLiterals) {
  SharedTreeEncoder encoder(nullptr);
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder.SplitNode(NodeId(2), ProtoLiteral(1, 1), NodeId(4));

  EXPECT_THAT(encoder.GetNode(NodeId(4))->NegatedDecisions(),
              testing::ElementsAre(Literal(BooleanVariable(1), false),
                                   Literal(BooleanVariable(0), false)));
  EXPECT_THAT(encoder.GetNode(NodeId(5))->NegatedDecisions(),
              testing::ElementsAre(Literal(BooleanVariable(1), true),
                                   Literal(BooleanVariable(0), false)));
  EXPECT_THAT(encoder.GetNode(NodeId(2))->NegatedDecisions(),
              testing::ElementsAre(Literal(BooleanVariable(0), false)));
}

TEST(SharedTreeEncoderTest, CloseNodeClosesDescendants) {
  SharedTreeEncoder encoder(nullptr);
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder.SplitNode(NodeId(2), ProtoLiteral(1, 1), NodeId(4));

  encoder.CloseNode(NodeId(2), {});

  EXPECT_TRUE(encoder.GetNode(NodeId(2))->shared().is_closed());
  EXPECT_TRUE(encoder.GetNode(NodeId(4))->shared().is_closed());
  EXPECT_TRUE(encoder.GetNode(NodeId(5))->shared().is_closed());
}

TEST(SharedTreeEncoderTest, CloseNodeAndSiblingClosesParent) {
  SharedTreeEncoder encoder(nullptr);
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  ASSERT_FALSE(encoder.GetNode(NodeId(1))->shared().is_closed());

  encoder.CloseNode(NodeId(2), {});
  encoder.CloseNode(NodeId(3), {});

  EXPECT_TRUE(encoder.GetNode(NodeId(1))->shared().is_closed());
}

TEST(SharedTreeEncoderTest, SyncNodesOnPathClosesDescendants) {
  SharedTreeEncoder encoder1(nullptr);
  SharedTreeEncoder encoder2(nullptr);
  encoder1.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder2.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder1.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder2.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder1.SplitNode(NodeId(2), ProtoLiteral(1, 1), NodeId(4));
  encoder2.SplitNode(NodeId(2), ProtoLiteral(1, 1), NodeId(4));
  encoder1.CloseNode(NodeId(2), {});

  encoder1.SyncNodesOnPath(NodeId(2), encoder2);

  EXPECT_TRUE(encoder2.GetNode(NodeId(2))->shared().is_closed());
  EXPECT_TRUE(encoder2.GetNode(NodeId(4))->shared().is_closed());
  EXPECT_TRUE(encoder2.GetNode(NodeId(5))->shared().is_closed());
}

TEST(SharedTreeEncoderTest, SyncNodesOnPathClosesParent) {
  SharedTreeEncoder encoder1(nullptr);
  SharedTreeEncoder encoder2(nullptr);
  encoder1.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder2.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder1.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder2.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder1.CloseNode(NodeId(2), {});
  encoder2.CloseNode(NodeId(3), {});

  encoder1.SyncNodesOnPath(NodeId(2), encoder2);

  EXPECT_TRUE(encoder2.GetNode(NodeId(1))->shared().is_closed());
}

TEST(SharedTreeEncoderTest, SyncNodesOnPathImpliedNode) {
  SharedTreeEncoder encoder1(nullptr);
  SharedTreeEncoder encoder2(nullptr);
  encoder1.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder2.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder1.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder2.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder1.CloseNode(NodeId(3), {});

  encoder1.SyncNodesOnPath(NodeId(2), encoder2);

  EXPECT_TRUE(encoder2.GetNode(NodeId(2))->shared().is_implied);
  EXPECT_TRUE(encoder2.GetNode(NodeId(3))->shared().is_closed());
}

TEST(SharedTreeEncoderTest, SyncNodesOnPathHandlesImpliedNodes) {
  SatParameters params;
  params.set_output_lrat_proof(true);
  params.set_check_lrat_proof(true);
  params.set_debug_crash_if_lrat_check_fails(true);
  Model model;
  model.Add(NewSatParameters(params));
  auto lrat = LratProofHandler::MaybeCreate(
      params, model.GetOrCreate<SharedLratProofStatus>(),
      model.GetOrCreate<SharedStatistics>());
  SharedTreeEncoder shared(lrat.get());
  SharedTreeEncoder worker(lrat.get());
  shared.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  shared.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  shared.SplitNode(NodeId(2), ProtoLiteral(1, 1), NodeId(4));

  shared.SyncNodesOnPath(NodeId(4), worker);
  ClausePtr node2_implied_clause =
      NewClausePtr({Literal(BooleanVariable(0), true)});
  lrat->AddProblemClause(node2_implied_clause);
  ClausePtr node4_implied_clause = NewClausePtr(
      {Literal(BooleanVariable(1), true), Literal(BooleanVariable(0), false)});
  lrat->AddProblemClause(node4_implied_clause);
  shared.CloseNode(NodeId(3), {node2_implied_clause});
  worker.CloseNode(NodeId(5), {node4_implied_clause});
  shared.SyncNodesOnPath(NodeId(4), worker);

  EXPECT_THAT(
      shared.GetNode(NodeId(1))->reason_clauses(),
      UnorderedElementsAre(
          Pair(Literal(BooleanVariable(0), true), testing::_),
          testing::Pair(Literal(BooleanVariable(1), true), testing::_)));
  EXPECT_THAT(worker.GetNode(NodeId(1))->reason_clauses(),
              UnorderedElementsAre(
                  Pair(Literal(BooleanVariable(0), true), testing::_),
                  Pair(Literal(BooleanVariable(1), true), testing::_)));
}

TEST(SharedTreeEncoderTest, CloseSubtreeLratProof) {
  SatParameters params;
  params.set_output_lrat_proof(true);
  params.set_check_lrat_proof(true);
  params.set_debug_crash_if_lrat_check_fails(true);
  Model model;
  model.Add(NewSatParameters(params));
  auto lrat = LratProofHandler::MaybeCreate(
      params, model.GetOrCreate<SharedLratProofStatus>(),
      model.GetOrCreate<SharedStatistics>());
  SharedTreeEncoder encoder(lrat.get());
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder.SplitNode(NodeId(2), ProtoLiteral(1, 1), NodeId(4));

  // Close leaf Node 4
  ClausePtr clause4 = NewClausePtr(
      {Literal(BooleanVariable(1), false), Literal(BooleanVariable(0), false)});
  lrat->AddProblemClause(clause4);
  encoder.CloseNode(NodeId(4), {clause4});
  // And node 5.
  ClausePtr clause5 = NewClausePtr(
      {Literal(BooleanVariable(1), true), Literal(BooleanVariable(0), false)});
  lrat->AddProblemClause(clause5);
  encoder.CloseNode(NodeId(5), {clause5});
  // Close Node 3 to fully exhaust root
  ClausePtr clause3 = NewClausePtr({Literal(BooleanVariable(0), true)});
  lrat->AddProblemClause(clause3);
  encoder.CloseNode(NodeId(3), {clause3});

  EXPECT_TRUE(encoder.GetNode(NodeId(1))->shared().is_closed());
  EXPECT_EQ(lrat->Check(), SharedLratProofStatus::VALID);
}

TEST(SharedTreeEncoderTest, SyncNodesOnPathClosesLratProofWithInvalidLeaf) {
  SatParameters params;
  params.set_output_lrat_proof(true);
  params.set_check_lrat_proof(true);
  params.set_debug_crash_if_lrat_check_fails(
      false);  // Don't crash, return INVALID
  params.set_shared_tree_num_workers(4);
  Model model;
  model.Add(NewSatParameters(params));
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  auto lrat1 = LratProofHandler::MaybeCreate(
      params, model.GetOrCreate<SharedLratProofStatus>(),
      model.GetOrCreate<SharedStatistics>());
  SharedTreeEncoder worker1(lrat1.get());
  auto lrat2 = LratProofHandler::MaybeCreate(
      params, model.GetOrCreate<SharedLratProofStatus>(),
      model.GetOrCreate<SharedStatistics>());
  SharedTreeEncoder worker2(lrat2.get());
  std::vector<ProtoLiteral> phase;

  shared_tree_manager->ReplaceTree(kNoNodeId, worker1, phase);
  shared_tree_manager->TrySplitTree(NodeId(1), {ProtoLiteral(0, 1)}, worker1);
  ClausePtr clause2 = NewClausePtr({Literal(BooleanVariable(0), false)});
  lrat1->AddProblemClause(clause2);
  worker1.CloseNode(NodeId(2), {clause2});
  ClausePtr clause3 = NewClausePtr({Literal(BooleanVariable(0), true)});
  lrat1->AddProblemClause(clause3);
  worker1.CloseNode(NodeId(3), {clause3});
  shared_tree_manager->SyncTree(NodeId(2), worker1);
  shared_tree_manager->SyncTree(NodeId(3), worker1);

  EXPECT_EQ(lrat1->Check(), SharedLratProofStatus::VALID);
  EXPECT_FALSE(shared_tree_manager->SyncTree(kNoNodeId, worker2));
  EXPECT_EQ(lrat2->Check(), SharedLratProofStatus::VALID);
}

TEST(SharedTreeEncoderTest, CloseNodeDeletesReasonClauses) {
  SatParameters params;
  params.set_output_lrat_proof(true);
  params.set_check_lrat_proof(true);
  Model model;
  model.Add(NewSatParameters(params));
  auto lrat = LratProofHandler::MaybeCreate(
      params, model.GetOrCreate<SharedLratProofStatus>(),
      model.GetOrCreate<SharedStatistics>());
  SharedTreeEncoder encoder(lrat.get());
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  ClausePtr clause = NewClausePtr({Literal(BooleanVariable(0), false)});
  lrat->AddProblemClause(clause);
  std::vector<ClausePtr> proof = {clause};
  Literal implied_literal = Literal(BooleanVariable(1), true);
  encoder.GetNode(NodeId(2))->AddImplication(implied_literal);
  encoder.GetNode(NodeId(2))->ExportInferredReasonClause(implied_literal,
                                                         proof);
  ASSERT_EQ(encoder.GetNode(NodeId(2))->reason_clauses().size(), 1);

  encoder.CloseNode(NodeId(2), {});

  EXPECT_TRUE(encoder.GetNode(NodeId(2))->reason_clauses().empty());
}

class SharedTreeSolveTest : public testing::TestWithParam<absl::string_view> {
 public:
  SatParameters GetParams() {
    SatParameters params;
    params.set_num_workers(4);
    params.set_shared_tree_num_workers(4);
    params.set_cp_model_presolve(false);
    const SatParameters to_merge =
        google::protobuf::contrib::parse_proto::ParseTextProtoOrDie(GetParam());
    params.MergeFrom(to_merge);
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

TEST_P(SharedTreeSolveTest, FeasiblePigeonHoleSmokeTest) {
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

TEST_P(SharedTreeSolveTest, InfeasiblePigeonHoleSmokeTest) {
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

TEST_P(SharedTreeSolveTest, InfeasiblePigeonHolePureSatLratTest) {
  SatParameters params =
      google::protobuf::contrib::parse_proto::ParseTextProtoOrDie(
          "symmetry_level:1,"
          "num_workers:6,"
          "shared_tree_num_workers:6,"
          "find_clauses_that_are_exactly_one:false,"
          "debug_crash_if_lrat_check_fails:true,"
          "check_lrat_proof:true,"
          "output_lrat_proof:true,"
          "check_merged_lrat_proof:true,"
          "inprocessing_use_sat_sweeping:false,"
          "cp_model_pure_sat_presolve:true");
  // We use pure SAT encoding of the pigeon-hole problem.
  const int pigeons = 5;
  const int holes = 4;
  CpModelBuilder model_builder;
  std::vector<std::vector<BoolVar>> x(pigeons, std::vector<BoolVar>(holes));
  for (int i = 0; i < pigeons; ++i) {
    for (int j = 0; j < holes; ++j) {
      x[i][j] = model_builder.NewBoolVar();
    }
  }
  // Each pigeon is in at least one hole.
  for (int i = 0; i < pigeons; ++i) {
    std::vector<BoolVar> clause;
    for (int j = 0; j < holes; ++j) {
      clause.push_back(x[i][j]);
    }
    model_builder.AddBoolOr(clause);
  }
  // Each hole contains at most one pigeon.
  for (int j = 0; j < holes; ++j) {
    for (int i1 = 0; i1 < pigeons; ++i1) {
      for (int i2 = i1 + 1; i2 < pigeons; ++i2) {
        model_builder.AddBoolOr({x[i1][j].Not(), x[i2][j].Not()});
      }
    }
  }
  Model model;
  model.Add(NewSatParameters(params));

  CpSolverResponse response = SolveCpModel(model_builder.Build(), &model);

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
  SharedTreeEncoder shared_trail(nullptr);
  std::vector<ProtoLiteral> phase;
  NodeId leaf_id =
      shared_tree_manager->ReplaceTree(kNoNodeId, shared_trail, phase);
  shared_tree_manager->TrySplitTree(leaf_id, {{-1, 0}}, shared_trail);

  EXPECT_EQ(shared_trail.Size(), 3);
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
  SharedTreeEncoder shared_trail(nullptr);
  std::vector<ProtoLiteral> phase;
  NodeId leaf_id =
      shared_tree_manager->ReplaceTree(kNoNodeId, shared_trail, phase);
  shared_tree_manager->TrySplitTree(leaf_id, {{-1, 0}}, shared_trail);

  shared_tree_manager->Restart();
  shared_tree_manager->ReplaceTree(kNoNodeId, shared_trail, phase);

  EXPECT_EQ(shared_trail.Size(), 1);
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
  SharedTreeEncoder shared_trail(nullptr);
  shared_trail.ImportNode({.id = NodeId(1), .decision = ProtoLiteral()});
  NodeId leaf_id =
      shared_tree_manager->TrySplitTree(NodeId(1), {{-1, 0}}, shared_trail);

  shared_trail.CloseNode(NodeId(2), {});
  shared_tree_manager->SyncTree(leaf_id, shared_trail);

  std::vector<ProtoLiteral> phase;
  shared_tree_manager->ReplaceTree(leaf_id, shared_trail, phase);
  shared_tree_manager->Restart();

  shared_tree_manager->ReplaceTree(kNoNodeId, shared_trail, phase);

  EXPECT_EQ(shared_trail.Size(), 1);
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
  SharedTreeEncoder trail1(nullptr);
  SharedTreeEncoder trail2(nullptr);

  std::vector<ProtoLiteral> phase;
  NodeId root_id = shared_tree_manager->ReplaceTree(kNoNodeId, trail1, phase);
  NodeId leaf_id1 =
      shared_tree_manager->TrySplitTree(root_id, {{-1, 0}}, trail1);
  NodeId leaf_id2 = shared_tree_manager->ReplaceTree(kNoNodeId, trail2, phase);

  EXPECT_EQ(trail1.GetNormalizedLevelStartNodes(leaf_id1).size(), 2);
  EXPECT_EQ(trail2.GetNormalizedLevelStartNodes(leaf_id2).size(), 2);
  const auto* leaf1 = trail1.GetNode(leaf_id1);
  const auto* leaf2 = trail2.GetNode(leaf_id2);
  EXPECT_EQ(leaf1->shared().decision, leaf2->shared().decision.Negated());
  EXPECT_EQ(leaf1->decoded_decision(), leaf2->decoded_decision().Negated());
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
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  SharedTreeEncoder trail1(nullptr);
  SharedTreeEncoder trail2(nullptr);
  std::vector<ProtoLiteral> phase;
  NodeId root_id = shared_tree_manager->ReplaceTree(kNoNodeId, trail1, phase);
  NodeId leaf_id1 =
      shared_tree_manager->TrySplitTree(root_id, {{-1, 0}}, trail1);
  NodeId leaf_id2 = shared_tree_manager->ReplaceTree(kNoNodeId, trail2, phase);
  trail1.ImportNode({.id = leaf_id1, .objective_lb = 2});
  trail2.ImportNode({.id = leaf_id2, .objective_lb = 1});
  shared_tree_manager->SyncTree(leaf_id1, trail1);
  shared_tree_manager->SyncTree(leaf_id2, trail2);
  // This call should not be necessary, investigate.
  response_manager->UpdateInnerObjectiveBounds("test", IntegerValue(1),
                                               IntegerValue(100));

  // Reject this split because it is not at the global lower bound.
  EXPECT_EQ(shared_tree_manager->TrySplitTree(leaf_id1, {{int_var.index(), 3}},
                                              trail1),
            leaf_id1);

  EXPECT_EQ(response_manager->GetInnerObjectiveLowerBound(), 1);
}

TEST(SharedTreeManagerTest, DiscrepancySplitTestOneLeafPerWorker) {
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
  params.set_shared_tree_balance_tolerance(0);
  params.set_cp_model_presolve(false);
  params.set_shared_tree_split_strategy(
      SatParameters::SPLIT_STRATEGY_DISCREPANCY);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  SharedTreeEncoder trail1(nullptr);
  SharedTreeEncoder trail2(nullptr);

  trail1.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  trail2.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});

  NodeId leaf_id1 = shared_tree_manager->TrySplitTree(NodeId(1),
                                                      {{-1, 0},
                                                       {int_var.index(), 3},
                                                       {int_var.index(), 4},
                                                       {int_var.index(), 5}},
                                                      trail1);
  EXPECT_EQ(trail1.GetNormalizedLevelStartNodes(leaf_id1).size(), 4);

  std::vector<ProtoLiteral> phase;
  NodeId leaf_id2 = shared_tree_manager->ReplaceTree(kNoNodeId, trail2, phase);

  NodeId leaf_id2_new = shared_tree_manager->TrySplitTree(
      leaf_id2, {{int_var.index(), 3}, {int_var.index(), 5}}, trail2);
  EXPECT_EQ(trail2.GetNormalizedLevelStartNodes(leaf_id2_new).size(), 3);

  EXPECT_EQ(shared_tree_manager->MaxPathDepth(), 3);
  EXPECT_EQ(shared_tree_manager->NumNodes(), 9);
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
  params.set_shared_tree_open_leaves_per_worker(2.5);
  params.set_cp_model_presolve(false);
  params.set_shared_tree_split_strategy(
      SatParameters::SPLIT_STRATEGY_DISCREPANCY);
  params.set_shared_tree_balance_tolerance(0);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  SharedTreeEncoder trail1(nullptr);
  SharedTreeEncoder trail2(nullptr);

  trail1.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  trail2.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});

  NodeId leaf_id1 = shared_tree_manager->TrySplitTree(
      NodeId(1), {{-1, 0}, {int_var.index(), 3}, {int_var.index(), 5}}, trail1);
  EXPECT_EQ(trail1.GetNormalizedLevelStartNodes(leaf_id1).size(), 4);

  std::vector<ProtoLiteral> phase;
  NodeId leaf_id2 = shared_tree_manager->ReplaceTree(kNoNodeId, trail2, phase);

  NodeId leaf_id2_new = shared_tree_manager->TrySplitTree(
      leaf_id2, {{int_var.index(), 3}, {int_var.index(), 5}}, trail2);
  EXPECT_EQ(trail2.GetNormalizedLevelStartNodes(leaf_id2_new).size(), 3);

  EXPECT_EQ(shared_tree_manager->MaxPathDepth(), 3);
  EXPECT_EQ(shared_tree_manager->NumNodes(), 9);
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
  params.set_shared_tree_balance_tolerance(0);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  SharedTreeEncoder trail1(nullptr);
  SharedTreeEncoder trail2(nullptr);
  std::vector<ProtoLiteral> phase;
  NodeId root_id = shared_tree_manager->ReplaceTree(kNoNodeId, trail1, phase);

  NodeId leaf_id1 = shared_tree_manager->TrySplitTree(root_id,
                                                      {{int_var.index(), 3},
                                                       {int_var.index(), 2},
                                                       {int_var.index(), 1},
                                                       {int_var.index(), 0}},
                                                      trail1);
  EXPECT_EQ(shared_tree_manager->MaxPathDepth(), 3);
  EXPECT_EQ(trail1.GetNormalizedLevelStartNodes(leaf_id1).size(), 4);

  shared_tree_manager->SyncTree(leaf_id1, trail1);

  NodeId leaf_id2 = shared_tree_manager->ReplaceTree(kNoNodeId, trail2, phase);

  NodeId leaf_id2_new = shared_tree_manager->TrySplitTree(
      leaf_id2, {{int_var.index(), 5}, {int_var.index(), 4}}, trail2);
  EXPECT_EQ(trail2.GetNormalizedLevelStartNodes(leaf_id2_new).size(), 3);

  EXPECT_EQ(shared_tree_manager->MaxPathDepth(), 3);
  EXPECT_EQ(shared_tree_manager->NumNodes(), 9);
}

TEST(SharedTreeManagerTest, BalancedSplitTest) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  model_builder.AddLessOrEqual(int_var, 3).OnlyEnforceIf(bool_var);
  model_builder.Maximize(int_var);
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_shared_tree_open_leaves_per_worker(2);
  params.set_cp_model_presolve(false);
  params.set_shared_tree_split_strategy(
      SatParameters::SPLIT_STRATEGY_BALANCED_TREE);
  params.set_shared_tree_balance_tolerance(0);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* response_manager = model.GetOrCreate<SharedResponseManager>();
  response_manager->InitializeObjective(model_builder.Build());
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  SharedTreeEncoder trail1(nullptr);
  SharedTreeEncoder trail2(nullptr);

  trail1.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  trail2.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});

  NodeId leaf_id1 = shared_tree_manager->TrySplitTree(NodeId(1),
                                                      {{int_var.index(), 3},
                                                       {int_var.index(), 2},
                                                       {int_var.index(), 1},
                                                       {int_var.index(), 0}},
                                                      trail1);
  EXPECT_EQ(trail1.GetNormalizedLevelStartNodes(leaf_id1).size(), 4);

  std::vector<ProtoLiteral> phase;
  NodeId leaf_id2 = shared_tree_manager->ReplaceTree(kNoNodeId, trail2, phase);

  NodeId leaf_id2_new =
      shared_tree_manager->TrySplitTree(leaf_id2,
                                        {{int_var.index(), 6},
                                         {int_var.index(), 5},
                                         {int_var.index(), 4},
                                         {int_var.index(), 3}},
                                        trail2);
  EXPECT_EQ(trail2.GetNormalizedLevelStartNodes(leaf_id2_new).size(), 4);

  EXPECT_EQ(shared_tree_manager->MaxPathDepth(), 3);
  EXPECT_EQ(shared_tree_manager->NumNodes(), 11);
}

TEST(SharedTreeManagerTest, CloseTreeTest) {
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  SharedTreeEncoder trail1(nullptr);
  SharedTreeEncoder trail2(nullptr);

  trail1.ImportNode({.id = NodeId(1), .decision = ProtoLiteral()});
  trail2.ImportNode({.id = NodeId(1), .decision = ProtoLiteral()});

  NodeId leaf_id1 =
      shared_tree_manager->TrySplitTree(NodeId(1), {{-1, 0}, {1, 0}}, trail1);
  EXPECT_EQ(trail1.Size(), 5);

  std::vector<ProtoLiteral> phase;
  NodeId leaf_id2 = shared_tree_manager->ReplaceTree(kNoNodeId, trail2, phase);

  trail1.CloseNode(NodeId(2), {});
  shared_tree_manager->SyncTree(leaf_id1, trail1);

  shared_tree_manager->ReplaceTree(leaf_id1, trail1, phase);

  EXPECT_EQ(trail1.Size(), 0);
  EXPECT_EQ(trail2.Size(), 3);

  EXPECT_EQ(trail2.GetNode(leaf_id2)->shared().decision, ProtoLiteral(0, 1));
}

TEST(SharedTreeManagerTest, TrailSharing) {
  CpModelBuilder model_builder;
  auto bool_var = model_builder.NewBoolVar();
  auto int_var = model_builder.NewIntVar({0, 7});
  auto bool_phase_var = model_builder.NewBoolVar();
  model_builder.AddLessOrEqual(int_var, 6)
      .OnlyEnforceIf({bool_var, bool_phase_var});
  model_builder.Maximize(int_var + bool_phase_var);
  Model model;
  SatParameters params;
  params.set_num_workers(4);
  params.set_shared_tree_num_workers(4);
  params.set_cp_model_presolve(false);
  model.Add(NewSatParameters(params));
  LoadVariables(model_builder.Build(), false, &model);
  auto* shared_tree_manager = model.GetOrCreate<SharedTreeManager>();
  SharedTreeEncoder trail1(nullptr, model.GetOrCreate<CpModelMapping>(),
                           model.GetOrCreate<IntegerEncoder>());
  SharedTreeEncoder trail2(nullptr, model.GetOrCreate<CpModelMapping>(),
                           model.GetOrCreate<IntegerEncoder>());
  trail1.ImportNode({.id = NodeId(1), .decision = ProtoLiteral()});
  trail2.ImportNode({.id = NodeId(1), .decision = ProtoLiteral()});
  NodeId leaf_id1 = shared_tree_manager->TrySplitTree(
      NodeId(1), {ProtoLiteral(0, 1)}, trail1);
  Literal lit_impl1 = Literal(BooleanVariable(1), true);
  trail1.GetNode(NodeId(2))->AddImplication(lit_impl1);
  shared_tree_manager->SyncTree(leaf_id1, trail1);
  std::vector<ProtoLiteral> phase = {ProtoLiteral(2, 1)};

  NodeId leaf_id1_new =
      shared_tree_manager->ReplaceTree(leaf_id1, trail1, phase);
  std::vector<ProtoLiteral> phase2;
  NodeId leaf_id2 = shared_tree_manager->ReplaceTree(kNoNodeId, trail2, phase2);

  EXPECT_EQ(trail1.Size(), 3);
  EXPECT_EQ(trail2.Size(), 3);
  EXPECT_THAT(trail2.GetNode(leaf_id2)->implications(),
              testing::ElementsAre(lit_impl1));
  EXPECT_THAT(phase2, testing::ElementsAre(ProtoLiteral(2, 1)));
  EXPECT_TRUE(trail1.GetNode(leaf_id1_new)->implications().empty());
}

TEST(SharedTreeEncoderTest, SetObjectiveLowerBound) {
  SharedTreeEncoder encoder(nullptr);
  encoder.ImportNode(
      {.id = NodeId(1), .decision = ProtoLiteral(), .objective_lb = 0});
  encoder.SplitNode(NodeId(1), ProtoLiteral(0, 1), NodeId(2));
  encoder.SplitNode(NodeId(2), ProtoLiteral(1, 1), NodeId(4));
  encoder.SplitNode(NodeId(3), ProtoLiteral(2, 1), NodeId(6));
  encoder.SplitNode(NodeId(5), ProtoLiteral(3, 1), NodeId(8));
  encoder.ImportNode(
      {.id = NodeId(5), .parent_id = NodeId(2), .is_implied = true});

  encoder.GetNode(NodeId(6))->SetObjectiveLowerBound(IntegerValue(20));
  encoder.GetNode(NodeId(7))->SetObjectiveLowerBound(IntegerValue(15));
  encoder.GetNode(NodeId(8))->SetObjectiveLowerBound(IntegerValue(25));
  encoder.GetNode(NodeId(9))->SetObjectiveLowerBound(IntegerValue(12));

  EXPECT_TRUE(encoder.GetNode(NodeId(5))->shared().is_implied);
  EXPECT_TRUE(encoder.GetNode(NodeId(4))->shared().is_closed());
  EXPECT_EQ(encoder.GetNode(NodeId(3))->shared().objective_lb, 15);
  EXPECT_EQ(encoder.GetNode(NodeId(2))->shared().objective_lb, 12);
  EXPECT_EQ(encoder.GetNode(NodeId(1))->shared().objective_lb, 12);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
