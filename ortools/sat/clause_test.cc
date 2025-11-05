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

#include "ortools/sat/clause.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <optional>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/random/random.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/util/strong_integers.h"

namespace operations_research {
namespace sat {
namespace {

using ::testing::ElementsAre;

template <typename... Args>
auto LiteralsAre(Args... literals) {
  return ::testing::ElementsAre(Literal(literals)...);
}

template <typename... Args>
auto UnorderedLiteralsAre(Args... literals) {
  return ::testing::UnorderedElementsAre(Literal(literals)...);
}

TEST(SatClauseTest, BasicAllocation) {
  std::unique_ptr<SatClause> clause(SatClause::Create(Literals({+1, -2, +4})));
  EXPECT_EQ(3, clause->size());
  EXPECT_EQ(Literal(+1), clause->FirstLiteral());
  EXPECT_EQ(Literal(-2), clause->SecondLiteral());
}

struct TestSatClause {
#ifdef _MSC_VER
  // MSVC doesn't allow to overflow at the word boundary.
  unsigned int is_learned : 1;
  unsigned int is_attached : 1;
#else
  bool is_learned : 1;
  bool is_attached : 1;
#endif
  unsigned int size : 30;

  // We test that Literal literals[0]; does not increase the size.
};

TEST(SatClauseTest, ClassSize) {
  EXPECT_EQ(4, sizeof(TestSatClause));
  EXPECT_EQ(4, sizeof(SatClause));
}

BinaryClause MakeBinaryClause(int a, int b) {
  return BinaryClause(Literal(a), Literal(b));
}

TEST(BinaryClauseManagerTest, BasicTest) {
  BinaryClauseManager manager;
  manager.Add(MakeBinaryClause(+2, +3));
  manager.Add(MakeBinaryClause(+3, +2));  // dup
  manager.Add(MakeBinaryClause(+1, +4));
  manager.Add(MakeBinaryClause(+5, +4));
  manager.Add(MakeBinaryClause(+4, +1));  // dup
  manager.Add(MakeBinaryClause(+2, +3));  // dup
  EXPECT_EQ(3, manager.NumClauses());
  EXPECT_THAT(manager.newly_added(),
              ElementsAre(MakeBinaryClause(+2, +3), MakeBinaryClause(+1, +4),
                          MakeBinaryClause(+5, +4)));
  manager.ClearNewlyAdded();
  EXPECT_TRUE(manager.newly_added().empty());
  manager.Add(MakeBinaryClause(-1, +2));
  EXPECT_EQ(4, manager.NumClauses());
  EXPECT_THAT(manager.newly_added(), ElementsAre(MakeBinaryClause(-1, +2)));
}

TEST(BinaryImplicationGraphTest, BasicUnsatSccTest) {
  Model model;
  model.GetOrCreate<Trail>()->Resize(10);
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  graph->Resize(10);
  // These are implications.
  graph->AddBinaryClause(Literal(+1).Negated(), Literal(+2));
  graph->AddBinaryClause(Literal(+2).Negated(), Literal(+3));
  graph->AddBinaryClause(Literal(+3).Negated(), Literal(-1));
  graph->AddBinaryClause(Literal(-1).Negated(), Literal(+4));
  graph->AddBinaryClause(Literal(+4).Negated(), Literal(+1));
  EXPECT_FALSE(graph->DetectEquivalences());
}

TEST(BinaryImplicationGraphTest, IssueFoundOnMipLibTest) {
  Model model;
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  model.GetOrCreate<SatSolver>()->SetNumVariables(10);
  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, -3, -4})));
  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, +3, +4, +5})));
  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, -2, -3, +4, +5})));
  EXPECT_TRUE(graph->DetectEquivalences());
}

TEST(BinaryImplicationGraphTest, DetectEquivalences) {
  // We take a bunch of random permutations, equivalence classes will be cycles.
  // We make sure the representative of x and not(x) are always negation of
  // each other.
  absl::BitGen random;
  for (int num_passes = 0; num_passes < 10; ++num_passes) {
    Model model;
    auto* graph = model.GetOrCreate<BinaryImplicationGraph>();

    const int size = 1000;
    model.GetOrCreate<SatSolver>()->SetNumVariables(size);
    std::vector<int> permutation(size);
    std::iota(permutation.begin(), permutation.end(), 0);
    std::shuffle(permutation.begin(), permutation.end(), random);
    for (int i = 0; i < size; ++i) {
      // i => permutation[i].
      const int signed_value = i + 1;
      const int signed_image = permutation[i] + 1;
      graph->AddBinaryClause(Literal(signed_value).Negated(),
                             Literal(signed_image));
    }

    EXPECT_TRUE(graph->DetectEquivalences());
    int num_classes = 0;
    for (int i = 0; i < size; ++i) {
      const int signed_value = i + 1;
      if (graph->RepresentativeOf(Literal(signed_value)) ==
          Literal(signed_value)) {
        ++num_classes;
      } else {
        EXPECT_EQ(graph->RepresentativeOf(Literal(signed_value)).Negated(),
                  graph->RepresentativeOf(Literal(signed_value).Negated()));
      }
    }

    // It is unlikely that std::shuffle() produce the identity permutation, so
    // this is not flaky and shows that there is some detection going on.
    EXPECT_GT(num_classes, 0);
    EXPECT_LT(num_classes, size);
  }
}

TEST(BinaryImplicationGraphTest, DetectEquivalencesWithAtMostOnes) {
  Model model;
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  model.GetOrCreate<SatSolver>()->SetNumVariables(10);

  // 2 and 4 are equivalent. They actually must be true in this setting.
  EXPECT_TRUE(graph->AddAtMostOne(Literals({1, 2, 3})));
  EXPECT_TRUE(graph->AddAtMostOne(Literals({1, 4, 3})));
  graph->AddBinaryClause(Literal(1), Literal(4));
  graph->AddBinaryClause(Literal(3), Literal(2));

  const auto& assignment = model.GetOrCreate<Trail>()->Assignment();
  EXPECT_TRUE(graph->DetectEquivalences());
  EXPECT_TRUE(assignment.LiteralIsTrue(Literal(2)));
  EXPECT_TRUE(assignment.LiteralIsTrue(Literal(4)));
  EXPECT_TRUE(assignment.LiteralIsFalse(Literal(1)));
  EXPECT_TRUE(assignment.LiteralIsFalse(Literal(3)));
}

TEST(BinaryImplicationGraphTest, TransitiveReduction) {
  Model model;
  model.GetOrCreate<SatSolver>()->SetNumVariables(10);
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();

  for (BooleanVariable i(0); i < 10; ++i) {
    for (BooleanVariable j(i + 1); j < 10; ++j) {
      // i => j
      graph->AddBinaryClause(Literal(i, false), Literal(j, true));
    }

    // These trivial clauses are filtered.
    graph->AddBinaryClause(Literal(i, false), Literal(i, true));
  }

  EXPECT_EQ(graph->ComputeNumImplicationsForLog(), 10 * 9);
  EXPECT_TRUE(graph->ComputeTransitiveReduction());
  EXPECT_EQ(graph->ComputeNumImplicationsForLog(), 9 * 2);
}

// This basically just test our DCHECKs.
TEST(BinaryImplicationGraphTest, BasicRandomTransitiveReduction) {
  Model model;
  const int num_vars = 200;
  model.GetOrCreate<SatSolver>()->SetNumVariables(num_vars);
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();

  // We add a lot of a => b (we might not have a DAG).
  absl::BitGen random;
  int num_added = 0;
  for (int i = 0; i < 10'000; ++i) {
    const BooleanVariable a(absl::Uniform<int>(random, 0, num_vars));
    const BooleanVariable b(absl::Uniform<int>(random, 0, num_vars));
    if (a == b) continue;
    ++num_added;
    graph->AddImplication(Literal(a, true), Literal(b, true));
  }

  EXPECT_EQ(graph->ComputeNumImplicationsForLog(), 2 * num_added);
  EXPECT_TRUE(graph->ComputeTransitiveReduction());
  EXPECT_LT(graph->ComputeNumImplicationsForLog(), num_added);
}

// We generate a random 2-SAT problem, and check that the propagation is
// unchanged whether or not the graph is reduced.
TEST(BinaryImplicationGraph, RandomTransitiveReduction) {
  // These leads to a not trivial 2-SAT space with more than just all zero
  // and all 1 as solution.
  const int num_variables = 100;
  const int num_constraints = 200;

  Model model1;
  Model model2;
  auto* sat1 = model1.GetOrCreate<SatSolver>();
  auto* sat2 = model2.GetOrCreate<SatSolver>();
  sat1->SetNumVariables(num_variables);
  sat2->SetNumVariables(num_variables);

  absl::BitGen random;
  for (int i = 0; i < num_constraints; ++i) {
    // Because we only use positive literal, we are never UNSAT.
    const Literal a =
        Literal(BooleanVariable(absl::Uniform(random, 0, num_variables)), true);
    const Literal b =
        Literal(BooleanVariable(absl::Uniform(random, 0, num_variables)), true);

    // a => b.
    sat1->AddBinaryClause(a.Negated(), b);
    sat2->AddBinaryClause(a.Negated(), b);
    sat2->AddBinaryClause(a.Negated(), b);
  }

  auto* graph2 = model2.GetOrCreate<BinaryImplicationGraph>();
  EXPECT_TRUE(graph2->ComputeTransitiveReduction());
  EXPECT_TRUE(sat1->Propagate());
  EXPECT_TRUE(sat2->Propagate());

  absl::flat_hash_set<LiteralIndex> propagated;
  for (BooleanVariable var(0); var < num_variables; ++var) {
    sat1->Backtrack(0);
    sat2->Backtrack(0);
    sat1->EnqueueDecisionIfNotConflicting(Literal(var, true));
    sat2->EnqueueDecisionIfNotConflicting(Literal(var, true));
    EXPECT_EQ(sat1->LiteralTrail().Index(), sat2->LiteralTrail().Index());
    propagated.clear();
    for (int i = 0; i < sat1->LiteralTrail().Index(); ++i) {
      propagated.insert(sat1->LiteralTrail()[i].Index());
    }
    for (int i = 0; i < sat2->LiteralTrail().Index(); ++i) {
      EXPECT_TRUE(propagated.contains(sat2->LiteralTrail()[i].Index()));
    }
  }
}

TEST(BinaryImplicationGraphTest, BasicCliqueDetection) {
  std::vector<std::vector<Literal>> at_most_ones;
  at_most_ones.push_back({Literal(+1), Literal(+2)});
  at_most_ones.push_back({Literal(+1), Literal(+3)});
  at_most_ones.push_back({Literal(+2), Literal(+3)});

  Model model;
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  graph->Resize(10);
  model.GetOrCreate<Trail>()->Resize(10);
  for (const std::vector<Literal>& at_most_one : at_most_ones) {
    EXPECT_TRUE(graph->AddAtMostOne(at_most_one));
  }
  graph->TransformIntoMaxCliques(&at_most_ones);
  EXPECT_THAT(at_most_ones[0], LiteralsAre(+1, +2, +3));
  EXPECT_TRUE(at_most_ones[1].empty());
  EXPECT_TRUE(at_most_ones[2].empty());
}

TEST(BinaryImplicationGraphTest, CliqueDetectionAndDuplicates) {
  std::vector<std::vector<Literal>> at_most_ones;
  at_most_ones.push_back({Literal(+1), Literal(+2)});
  at_most_ones.push_back({Literal(+2), Literal(+2)});

  Model model;
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  model.GetOrCreate<SatSolver>()->SetNumVariables(10);
  for (const std::vector<Literal>& at_most_one : at_most_ones) {
    EXPECT_TRUE(graph->AddAtMostOne(at_most_one));
  }

  // Here we do not change the clique.
  graph->TransformIntoMaxCliques(&at_most_ones);
  EXPECT_THAT(at_most_ones,
              ElementsAre(LiteralsAre(+1, +2), LiteralsAre(+2, +2)));

  // Clique detection call the SCC which will see that 2 must be false...
  const auto& assignment = model.GetOrCreate<Trail>()->Assignment();
  EXPECT_FALSE(assignment.LiteralIsAssigned(Literal(1)));
  EXPECT_TRUE(assignment.LiteralIsFalse(Literal(2)));
}

TEST(BinaryImplicationGraphTest, AddAtMostOneWithDuplicates) {
  Model model;
  auto* trail = model.GetOrCreate<Trail>();
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  trail->Resize(10);
  graph->Resize(10);
  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, +3, +2})));
  EXPECT_TRUE(trail->Assignment().LiteralIsFalse(Literal(+2)));
}

TEST(BinaryImplicationGraphTest, AddAtMostOneWithTriples) {
  Model model;
  auto* trail = model.GetOrCreate<Trail>();
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  trail->Resize(10);
  graph->Resize(10);
  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, +3, +2, +2})));
  EXPECT_TRUE(trail->Assignment().LiteralIsFalse(Literal(+2)));
}

TEST(BinaryImplicationGraphTest, AddAtMostOneCornerCase) {
  Model model;
  auto* trail = model.GetOrCreate<Trail>();
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  trail->Resize(10);
  graph->Resize(10);
  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, +3, +2, -2})));
  EXPECT_TRUE(trail->Assignment().LiteralIsFalse(Literal(+1)));
  EXPECT_TRUE(trail->Assignment().LiteralIsFalse(Literal(+2)));
  EXPECT_TRUE(trail->Assignment().LiteralIsFalse(Literal(+3)));
}

TEST(BinaryImplicationGraphTest, LargeAtMostOnePropagation) {
  const int kNumVariables = 1e6;

  Model model;
  auto* trail = model.GetOrCreate<Trail>();
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  trail->Resize(kNumVariables);
  graph->Resize(kNumVariables);

  std::vector<Literal> large_at_most_one;
  for (int i = 0; i < kNumVariables; ++i) {
    large_at_most_one.push_back(Literal(BooleanVariable(i), true));
  }
  EXPECT_TRUE(graph->AddAtMostOne(large_at_most_one));

  const Literal decision = Literal(BooleanVariable(42), true);
  trail->SetDecisionLevel(1);
  trail->EnqueueSearchDecision(Literal(decision));
  EXPECT_TRUE(graph->Propagate(trail));

  const auto& assignment = trail->Assignment();
  for (int i = 0; i < kNumVariables; ++i) {
    const Literal l = Literal(BooleanVariable(i), true);
    if (i == 42) {
      EXPECT_TRUE(assignment.LiteralIsTrue(l));
    } else {
      EXPECT_TRUE(assignment.LiteralIsFalse(l));
      EXPECT_EQ(trail->Reason(l.Variable()),
                absl::Span<const Literal>({decision.Negated()}));
    }
  }
}

TEST(BinaryImplicationGraphTest, HeuristicAmoPartition) {
  const int kNumVariables = 1e6;

  Model model;
  auto* trail = model.GetOrCreate<Trail>();
  model.GetOrCreate<SatParameters>()->set_at_most_one_max_expansion_size(2);
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  trail->Resize(kNumVariables);
  graph->Resize(kNumVariables);

  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, +3, +4})));
  EXPECT_TRUE(graph->AddAtMostOne(Literals({+4, +5, +6, +7})));

  std::vector<Literal> literals = Literals({+1, +2, +5, +6, +7});
  EXPECT_THAT(graph->HeuristicAmoPartition(&literals),
              ElementsAre(UnorderedLiteralsAre(+5, +6, +7),
                          UnorderedLiteralsAre(+1, +2)));

  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, +6, +7})));
  EXPECT_THAT(graph->HeuristicAmoPartition(&literals),
              ElementsAre(LiteralsAre(+6, +7, +2, +1)));
}

TEST(BinaryImplicationGraphTest, RandomImpliedLiteral) {
  Model model;
  auto* trail = model.GetOrCreate<Trail>();
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  trail->Resize(100);
  graph->Resize(100);

  graph->AddImplication(Literal(+1), Literal(+6));
  graph->AddImplication(Literal(+1), Literal(+7));
  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, +4, +5})));

  absl::flat_hash_set<LiteralIndex> seen;
  for (int i = 0; i < 100; ++i) {
    seen.insert(graph->RandomImpliedLiteral(Literal(+1)));
  }
  EXPECT_THAT(seen, UnorderedLiteralsAre(-2, -4, -5, +6, +7));
}

TEST(BinaryImplicationGraphTest, DetectEquivalencePropagateThings) {
  Model model;
  auto* trail = model.GetOrCreate<Trail>();
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  trail->Resize(10);
  graph->Resize(10);

  EXPECT_TRUE(graph->AddAtMostOne(Literals({+1, +2, +3, +4})));
  EXPECT_TRUE(graph->AddAtMostOne(Literals({-2, -1, +3, +4})));
  EXPECT_TRUE(graph->AddAtMostOne(Literals({-4, -1, +2, +3})));
  EXPECT_TRUE(graph->AddAtMostOne(Literals({-3, -1, +2, +4})));
  EXPECT_TRUE(graph->DetectEquivalences());
}

void TryAmoEquivalences(absl::Span<const std::vector<int>> cliques) {
  Model model;
  auto* trail = model.GetOrCreate<Trail>();
  auto* graph = model.GetOrCreate<BinaryImplicationGraph>();
  trail->Resize(1000);
  graph->Resize(1000);
  for (const auto& clique : cliques) {
    std::vector<Literal> literals;
    for (const int i : clique) {
      literals.push_back(Literal(i));
    }
    if (!graph->AddAtMostOne(literals)) {
      return;
    }
  }
  graph->DetectEquivalences();
}

}  // namespace
}  // namespace sat
}  // namespace operations_research
