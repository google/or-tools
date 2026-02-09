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

#include "ortools/sat/sat_inprocessing.h"

#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"

namespace operations_research {
namespace sat {
namespace {

TEST(InprocessingTest, ClauseCleanupWithFixedVariables) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* clause_manager = model.GetOrCreate<ClauseManager>();
  auto* inprocessing = model.GetOrCreate<Inprocessing>();

  // Lets add some clauses.
  sat_solver->SetNumVariables(100);
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, +2, +3, +4})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, -2, -3, +5})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+2, -2, -3, +1, +1})));

  // Nothing fixed, we don't even look at the clause.
  const bool log_info = true;
  EXPECT_TRUE(inprocessing->DetectEquivalencesAndStamp(false, log_info));
  EXPECT_TRUE(inprocessing->RemoveFixedAndEquivalentVariables(log_info));
  {
    const auto& all_clauses = clause_manager->AllClausesInCreationOrder();
    EXPECT_EQ(all_clauses.size(), 3);
    EXPECT_EQ(all_clauses[2]->AsSpan(), Literals({+2, -2, -3, +1, +1}));
  }

  // Lets fix 3.
  CHECK(sat_solver->AddUnitClause(Literal(+3)));
  EXPECT_TRUE(sat_solver->FinishPropagation());
  EXPECT_TRUE(inprocessing->DetectEquivalencesAndStamp(false, log_info));
  EXPECT_TRUE(inprocessing->RemoveFixedAndEquivalentVariables(log_info));
  {
    const auto& all_clauses = clause_manager->AllClausesInCreationOrder();
    EXPECT_EQ(all_clauses.size(), 3);
    EXPECT_EQ(all_clauses[0]->AsSpan(), Literals({}));  // +3 true.
    EXPECT_EQ(all_clauses[1]->AsSpan(), Literals({+1, -2, +5}));
    EXPECT_EQ(all_clauses[2]->AsSpan(), Literals({}));  // trivially true.
  }
}

TEST(InprocessingTest, ClauseCleanupWithEquivalence) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* clause_manager = model.GetOrCreate<ClauseManager>();
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  auto* inprocessing = model.GetOrCreate<Inprocessing>();

  // Lets add some clauses.
  sat_solver->SetNumVariables(100);
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, +2, +5, +4})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, -2, -3, +5})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+2, +6, -3, +1, +1})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+2, +6, -3, +1, -5})));

  // Lets make 3 and 5 equivalent.
  implication_graph->AddBinaryClause(Literal(-3), Literal(+5));
  implication_graph->AddBinaryClause(Literal(+3), Literal(-5));

  const bool log_info = true;
  EXPECT_TRUE(inprocessing->DetectEquivalencesAndStamp(false, log_info));
  EXPECT_TRUE(inprocessing->RemoveFixedAndEquivalentVariables(log_info));
  {
    const auto& all_clauses = clause_manager->AllClausesInCreationOrder();
    EXPECT_EQ(all_clauses.size(), 4);
    EXPECT_EQ(all_clauses[0]->AsSpan(), Literals({+1, +2, +3, +4}));
    EXPECT_EQ(all_clauses[1]->AsSpan(), Literals({}));
    EXPECT_EQ(all_clauses[3]->AsSpan(), Literals({+2, +6, -3, +1}));

    // Note that the +1 +1 is not simplified because this clause do not
    // need to be rewritten otherwise and we assume initial simplification.
    EXPECT_EQ(all_clauses[2]->AsSpan(), Literals({+2, +6, -3, +1, +1}));
  }
}

TEST(InprocessingTest, ClauseSubsumptionAndStrengthening) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* clause_manager = model.GetOrCreate<ClauseManager>();
  auto* inprocessing = model.GetOrCreate<Inprocessing>();

  // Lets add some clauses.
  // Note that the order currently matter for what is left.
  //
  // Note that currently the binary clauses are not reprocessed.
  // TODO(user): Maybe we should so that we always end up with a reduced set.
  sat_solver->SetNumVariables(100);
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, +3, +2})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, +2, -3})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, +3, +2})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, -2, -3})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+2, +6, -3, +1, +1})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({-3, +6, +2, +1, -5})));

  const bool log_info = true;
  EXPECT_TRUE(inprocessing->DetectEquivalencesAndStamp(false, log_info));
  EXPECT_TRUE(inprocessing->SubsumeAndStrenghtenRound(log_info));

  // This function remove empty clauses.
  const auto& all_clauses = clause_manager->AllClausesInCreationOrder();
  EXPECT_GE(all_clauses.size(), 0);

  // We added {+1, +2} and {+1, -3} here.
  // TODO(user): make sure we don't add twice the implications.
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  EXPECT_EQ(implication_graph->ComputeNumImplicationsForLog(), 4);
  EXPECT_EQ(implication_graph->Implications(Literal(-1)).size(), 2);
  EXPECT_THAT(implication_graph->Implications(Literal(-1)),
              ::testing::UnorderedElementsAre(Literal(+2), Literal(-3)));
}

TEST(StampingSimplifierTest, StampConstruction) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  auto* simplifier = model.GetOrCreate<StampingSimplifier>();

  // Lets add some clauses.
  // Note that the order currently matter for what is left.
  sat_solver->SetNumVariables(100);
  implication_graph->AddImplication(Literal(+1), Literal(+2));
  implication_graph->AddImplication(Literal(+1), Literal(+3));
  implication_graph->AddImplication(Literal(+1), Literal(+4));
  implication_graph->AddImplication(Literal(+2), Literal(+5));
  implication_graph->AddImplication(Literal(+2), Literal(+6));
  implication_graph->AddImplication(Literal(+3), Literal(+7));
  implication_graph->AddImplication(Literal(+4), Literal(+6));

  EXPECT_TRUE(implication_graph->DetectEquivalences(true));

  // Lets test some implications.
  simplifier->SampleTreeAndFillParent();
  simplifier->ComputeStamps();
  EXPECT_TRUE(simplifier->ImplicationIsInTree(Literal(+1), Literal(+2)));
  EXPECT_TRUE(simplifier->ImplicationIsInTree(Literal(+1), Literal(+5)));
  EXPECT_TRUE(simplifier->ImplicationIsInTree(Literal(+1), Literal(+6)));
  EXPECT_TRUE(simplifier->ImplicationIsInTree(Literal(+1), Literal(+7)));
  EXPECT_TRUE(simplifier->ImplicationIsInTree(Literal(-7), Literal(-3)));
}

TEST(StampingSimplifierTest, BasicSimplification) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* clause_manager = model.GetOrCreate<ClauseManager>();
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  auto* simplifier = model.GetOrCreate<StampingSimplifier>();

  // Lets add some clauses.
  // Note that the order currently matter for what is left.
  sat_solver->SetNumVariables(100);
  implication_graph->AddImplication(Literal(+1), Literal(+2));
  implication_graph->AddImplication(Literal(+1), Literal(+3));
  implication_graph->AddImplication(Literal(+1), Literal(+4));
  implication_graph->AddImplication(Literal(+2), Literal(+5));
  implication_graph->AddImplication(Literal(+2), Literal(+6));
  implication_graph->AddImplication(Literal(+3), Literal(+7));
  implication_graph->AddImplication(Literal(+4), Literal(+6));

  EXPECT_TRUE(implication_graph->DetectEquivalences(true));

  // Lets add some clause that should be simplifiable
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, +7, +8, +9})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, -6, +8, +9})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({-3, -7, +8, +9})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({-3, +7, +8, +9})));

  // Lets test some implications.
  EXPECT_TRUE(simplifier->DoOneRound(/*log_info=*/true));

  // Results. I cover all 4 possibilities, 2 strenghtening for clause 0 and 2,
  // one subsumption for clause 3 and nothing for clause 1.
  const auto& all_clauses = clause_manager->AllClausesInCreationOrder();
  EXPECT_EQ(all_clauses.size(), 4);
  EXPECT_EQ(all_clauses[0]->AsSpan(), Literals({+7, +8, +9}));
  EXPECT_EQ(all_clauses[1]->AsSpan(), Literals({+1, -6, +8, +9}));
  EXPECT_EQ(all_clauses[2]->AsSpan(), Literals({-3, +8, +9}));
  EXPECT_EQ(all_clauses[3]->AsSpan(), Literals({}));
}

TEST(BlockedClauseSimplifierTest, BasicSimplification) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* clause_manager = model.GetOrCreate<ClauseManager>();
  auto* implication_graph = model.GetOrCreate<BinaryImplicationGraph>();
  auto* simplifier = model.GetOrCreate<BlockedClauseSimplifier>();

  // Lets add some clauses.
  // Note that the order currently matter for what is left.
  sat_solver->SetNumVariables(100);
  implication_graph->AddImplication(Literal(+1), Literal(-7));
  implication_graph->AddImplication(Literal(+1), Literal(-8));
  implication_graph->AddImplication(Literal(+1), Literal(-9));

  // Lets add some clause that should be blocked
  EXPECT_TRUE(clause_manager->AddClause(Literals({-1, +7, -8, +9})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, +7, +8, +9})));

  simplifier->DoOneRound(/*log_info=*/true);

  clause_manager->DeleteRemovedClauses();
  const auto& all_clauses = clause_manager->AllClausesInCreationOrder();
  EXPECT_EQ(all_clauses.size(), 0);
}

TEST(BoundedVariableEliminationTest, BasicSimplification) {
  Model model;
  auto* sat_solver = model.GetOrCreate<SatSolver>();
  auto* clause_manager = model.GetOrCreate<ClauseManager>();
  auto* simplifier = model.GetOrCreate<BoundedVariableElimination>();

  // Lets add some clauses.
  sat_solver->SetNumVariables(100);
  EXPECT_TRUE(clause_manager->AddClause(Literals({+1, +2, +3, +7})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+3, +4, +5, +7})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({-1, +4, +5, -7})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+3, -2, +5, -7})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+2, +4, -3, -7})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+2, +4, -5, -7})));
  EXPECT_TRUE(clause_manager->AddClause(Literals({+2, +3, -4, -7})));

  simplifier->DoOneRound(/*log_info=*/true);

  // The problem is so simple that everything should be simplified.
  clause_manager->DeleteRemovedClauses();
  const auto& all_clauses = clause_manager->AllClausesInCreationOrder();
  EXPECT_EQ(all_clauses.size(), 0);
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
