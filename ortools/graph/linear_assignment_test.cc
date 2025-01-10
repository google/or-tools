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

#include "ortools/graph/linear_assignment.h"

#include <cstdint>
#include <memory>
#include <random>
#include <vector>

#include "absl/random/distributions.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "gtest/gtest.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/gmock.h"
#include "ortools/graph/ebert_graph.h"
#include "ortools/graph/graph.h"

ABSL_DECLARE_FLAG(bool, assignment_stack_order);

namespace operations_research {

using ::testing::Eq;

template <typename AssignmentGraphType>
static ArcIndex CreateArcWithCost(
    NodeIndex tail, NodeIndex head, CostValue cost,
    AnnotatedGraphBuildManager<AssignmentGraphType>* builder,
    LinearSumAssignment<AssignmentGraphType>* assignment) {
  const ArcIndex arc = builder->AddArc(tail, head);
  assignment->SetArcCost(arc, cost);
  return arc;
}

// A little package containing everything the AnnotatedGraphBuildManager-based
// tests need to know about an assignment instance.
template <typename GraphType>
struct AssignmentProblemSetup {
  // The usual constructor, for normal tests where the graph is balanced.
  AssignmentProblemSetup(NodeIndex num_left_nodes, ArcIndex num_arcs,
                         bool optimize_layout)
      : builder(new AnnotatedGraphBuildManager<GraphType>(
            2 * num_left_nodes, num_arcs, optimize_layout)),
        assignment_scoped(
            new LinearSumAssignment<GraphType>(num_left_nodes, num_arcs)),
        assignment(assignment_scoped.get()),
        cycle_handler_scoped(assignment_scoped->ArcAnnotationCycleHandler()),
        cycle_handler(cycle_handler_scoped.get()) {}

  // A constructor with separate specification of the numbers of left and right
  // nodes, so the tests can set up graphs where the assignment solution is sure
  // to fail.
  AssignmentProblemSetup(NodeIndex num_left_nodes, NodeIndex num_right_nodes,
                         ArcIndex num_arcs, bool optimize_layout)
      : builder(new AnnotatedGraphBuildManager<GraphType>(
            num_left_nodes + num_right_nodes, num_arcs, optimize_layout)),
        assignment_scoped(
            new LinearSumAssignment<GraphType>(num_left_nodes, num_arcs)),
        assignment(assignment_scoped.get()),
        cycle_handler_scoped(assignment_scoped->ArcAnnotationCycleHandler()),
        cycle_handler(cycle_handler_scoped.get()) {}

  // This type is neither copyable nor movable.
  AssignmentProblemSetup(const AssignmentProblemSetup&) = delete;
  AssignmentProblemSetup& operator=(const AssignmentProblemSetup&) = delete;

  virtual ~AssignmentProblemSetup() { delete &assignment->Graph(); }

  void Finalize() {
    GraphType* graph = builder->Graph(cycle_handler);
    ASSERT_TRUE(graph != nullptr);
    assignment->SetGraph(graph);
  }

  AnnotatedGraphBuildManager<GraphType>* builder;

  std::unique_ptr<LinearSumAssignment<GraphType>> assignment_scoped;
  LinearSumAssignment<GraphType>* assignment;  // to avoid ".get()" everywhere

  std::unique_ptr<PermutationCycleHandler<typename GraphType::ArcIndex>>
      cycle_handler_scoped;
  PermutationCycleHandler<typename GraphType::ArcIndex>* cycle_handler;
};

// A fixture template to collect the types of graphs on which we want to base
// the LinearSumAssignment template instances that we test in ways that do not
// require dynamic graphs. All such tests use GraphBuildManager objects to get
// their underlying graphs; they cannot invoke the LinearSumAssignment<>::
// OptimizeGraphLayout() method, because it cannot be used on static graphs.
template <typename GraphType>
class LinearSumAssignmentTestWithGraphBuilder : public ::testing::Test {};

typedef ::testing::Types<EbertGraph<int16_t, int16_t>,
                         EbertGraph<int16_t, ArcIndex>,
                         EbertGraph<NodeIndex, int16_t>, StarGraph,
                         util::ListGraph<>, util::ReverseArcListGraph<>>
    GraphTypesForAssignmentTestingWithGraphBuilder;

TYPED_TEST_SUITE(LinearSumAssignmentTestWithGraphBuilder,
                 GraphTypesForAssignmentTestingWithGraphBuilder);

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, OptimumMatching0) {
  AssignmentProblemSetup<TypeParam> setup(1, 1, false);
  CreateArcWithCost<TypeParam>(0, 1, 0, setup.builder, setup.assignment);
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  EXPECT_EQ(0, setup.assignment->GetCost());
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, OptimumMatching1) {
  // A problem instance containing a node with no incident arcs.
  AssignmentProblemSetup<TypeParam> setup(2, 1, false);
  // We need the graph to include an arc that mentions the largest-indexed node
  // in order to get better test coverage. Without that node used in an arc,
  // infeasibility is detected very early because the number of nodes in the
  // graph isn't twice the stated number of left-side nodes. With that node
  // mentioned, the number of nodes in the graph alone is not enough to
  // establish infeasibility, so more code runs.
  CreateArcWithCost<TypeParam>(1, 3, 0, setup.builder, setup.assignment);
  setup.Finalize();
  EXPECT_FALSE(setup.assignment->ComputeAssignment());
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, OptimumMatching2) {
  AssignmentProblemSetup<TypeParam> setup(2, 4, false);
  CreateArcWithCost<TypeParam>(0, 2, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 3, 2, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 2, 3, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 3, 4, setup.builder, setup.assignment);
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  EXPECT_EQ(4, setup.assignment->GetCost());
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, OptimumMatching3) {
  AssignmentProblemSetup<TypeParam> setup(4, 10, false);
  // Create arcs with tail nodes out of order to ensure that we test a case in
  // which the cost values must be nontrivially permuted if a static graph is
  // the underlying representation.
  CreateArcWithCost<TypeParam>(0, 5, 19, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 6, 47, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 7, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 4, 41, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 4, 60, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 5, 15, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 7, 60, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 4, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 6, 13, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 7, 41, setup.builder, setup.assignment);
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  EXPECT_EQ(0 + 13 + 15 + 0, setup.assignment->GetCost());
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, OptimumMatching4) {
  AssignmentProblemSetup<TypeParam> setup(4, 10, false);
  CreateArcWithCost<TypeParam>(0, 4, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 4, 60, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 5, 15, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 7, 60, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 4, 41, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 6, 13, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 7, 41, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 5, 19, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 6, 47, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 7, 0, setup.builder, setup.assignment);
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  EXPECT_EQ(0 + 13 + 15 + 0, setup.assignment->GetCost());
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, OptimumMatching5) {
  AssignmentProblemSetup<TypeParam> setup(4, 10, false);
  CreateArcWithCost<TypeParam>(0, 4, 60, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 5, 15, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 7, 60, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 4, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 4, 41, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 6, 13, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 7, 41, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 5, 19, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 6, 47, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 7, 0, setup.builder, setup.assignment);
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  EXPECT_EQ(0 + 13 + 15 + 0, setup.assignment->GetCost());
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, OptimumMatching6) {
  AssignmentProblemSetup<TypeParam> setup(4, 10, false);
  CreateArcWithCost<TypeParam>(0, 4, 41, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 6, 13, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 7, 41, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 4, 60, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 5, 15, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 7, 60, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 4, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 5, 19, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 6, 47, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 7, 0, setup.builder, setup.assignment);
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  EXPECT_EQ(0 + 13 + 15 + 0, setup.assignment->GetCost());
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, ZeroCostArcs) {
  AssignmentProblemSetup<TypeParam> setup(4, 10, false);
  CreateArcWithCost<TypeParam>(0, 4, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 6, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(0, 7, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 4, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 5, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(1, 7, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(2, 4, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 5, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 6, 0, setup.builder, setup.assignment);
  CreateArcWithCost<TypeParam>(3, 7, 0, setup.builder, setup.assignment);
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  EXPECT_EQ(0, setup.assignment->GetCost());
}

// A helper function template for checking that we got the optimum assignment we
// expected.
template <typename GraphType>
static void VerifyAssignment(const LinearSumAssignment<GraphType>& a,
                             const NodeIndex expected_right_side[]) {
  for (typename LinearSumAssignment<GraphType>::BipartiteLeftNodeIterator
           node_it(a);
       node_it.Ok(); node_it.Next()) {
    const NodeIndex left_node = node_it.Index();
    const NodeIndex right_node = a.GetMate(left_node);
    EXPECT_EQ(expected_right_side[left_node], right_node);
  }
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, OptimumMatching7) {
  const int kMatrixHeight = 4;
  const int kMatrixWidth = 4;
  // 4x4 problem taken from algorithms/hungarian_test.cc; to eliminate test
  // flakiness, kCost[0][1] is modified so the optimum assignment is unique.
  const int64_t kCost[kMatrixHeight][kMatrixWidth] = {{90, 76, 75, 80},
                                                      {35, 85, 55, 65},
                                                      {125, 95, 90, 105},
                                                      {45, 110, 95, 115}};
  AssignmentProblemSetup<TypeParam> setup(kMatrixHeight,
                                          kMatrixHeight * kMatrixWidth, false);
  for (int i = 0; i < kMatrixHeight; ++i) {
    for (int j = 0; j < kMatrixWidth; ++j) {
      CreateArcWithCost<TypeParam>(i, j + kMatrixHeight, kCost[i][j],
                                   setup.builder, setup.assignment);
    }
  }
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  const NodeIndex kExpectedAssignment[] = {7, 6, 5, 4};
  VerifyAssignment(*setup.assignment, kExpectedAssignment);
  EXPECT_EQ(80 + 55 + 95 + 45, setup.assignment->GetCost());
}

// Tests additional small assignment problems, and also tests that the
// assignment object can be reused to solve a modified version of the original
// problem.
TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder,
           OptimumMatching8WithObjectReuse) {
  const int kMatrixHeight = 4;
  const int kMatrixWidth = 4;
  // 4x4 problem taken from algorithms/hungarian_test.cc; costs are negated to
  // find the assignment whose cost is maximum with non-negated costs.
  const int64_t kCost[kMatrixHeight][kMatrixWidth] = {{-90, -75, -75, -80},
                                                      {-35, 100, -55, -65},
                                                      {-125, -95, -90, -105},
                                                      {-45, -110, -95, -115}};
  AssignmentProblemSetup<TypeParam> setup(kMatrixHeight,
                                          kMatrixHeight * kMatrixWidth, false);
  // Index of the arc we will remember and modify.
  ArcIndex cost_100_arc = TypeParam::kNilArc;
  for (int i = 0; i < kMatrixHeight; ++i) {
    for (int j = 0; j < kMatrixWidth; ++j) {
      ArcIndex new_arc = CreateArcWithCost(i, j + kMatrixHeight, kCost[i][j],
                                           setup.builder, setup.assignment);
      if (kCost[i][j] == 100) {
        cost_100_arc = new_arc;
      }
    }
  }
  setup.Finalize();
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  const NodeIndex kExpectedAssignment1[] = {6, 7, 4, 5};
  VerifyAssignment(*setup.assignment, kExpectedAssignment1);
  EXPECT_EQ(-75 + -65 + -125 + -110, setup.assignment->GetCost());

  // For static graphs which cannot be built without the possibility of
  // permuting the arcs, it is important that we have supplied the arcs in such
  // an order that cost_100_arc still refers to the arc that has cost 100.
  ASSERT_EQ(100, setup.assignment->ArcCost(cost_100_arc));
  setup.assignment->SetArcCost(cost_100_arc, -85);
  EXPECT_TRUE(setup.assignment->ComputeAssignment());
  const NodeIndex kExpectedAssignment2[] = {6, 5, 4, 7};
  VerifyAssignment(*setup.assignment, kExpectedAssignment2);
  EXPECT_EQ(-75 + -85 + -125 + -115, setup.assignment->GetCost());
}

TYPED_TEST(LinearSumAssignmentTestWithGraphBuilder, InfeasibleProblems) {
  // No arcs in the graph at all.
  AssignmentProblemSetup<TypeParam> setup0(1, 1, false);
  setup0.Finalize();
  EXPECT_FALSE(setup0.assignment->ComputeAssignment());

  // Unbalanced graph: 4 nodes on the left, 2 on the right.
  AssignmentProblemSetup<TypeParam> setup1(4, 2, 4, false);
  CreateArcWithCost<TypeParam>(0, 4, 0, setup1.builder, setup1.assignment);
  CreateArcWithCost<TypeParam>(1, 4, 2, setup1.builder, setup1.assignment);
  CreateArcWithCost<TypeParam>(2, 5, 3, setup1.builder, setup1.assignment);
  CreateArcWithCost<TypeParam>(3, 5, 4, setup1.builder, setup1.assignment);
  setup1.Finalize();
  EXPECT_FALSE(setup1.assignment->ComputeAssignment());

  // Unbalanced graph: 2 nodes on the left, 4 on the right.
  AssignmentProblemSetup<TypeParam> setup2(2, 4, 4, false);
  CreateArcWithCost<TypeParam>(0, 2, 0, setup2.builder, setup2.assignment);
  CreateArcWithCost<TypeParam>(1, 3, 2, setup2.builder, setup2.assignment);
  CreateArcWithCost<TypeParam>(0, 4, 3, setup2.builder, setup2.assignment);
  CreateArcWithCost<TypeParam>(1, 5, 4, setup2.builder, setup2.assignment);
  setup2.Finalize();
  EXPECT_FALSE(setup2.assignment->ComputeAssignment());

  // Balanced graph with no perfect matching.
  AssignmentProblemSetup<TypeParam> setup3(3, 5, false);
  CreateArcWithCost<TypeParam>(0, 3, 0, setup3.builder, setup3.assignment);
  CreateArcWithCost<TypeParam>(1, 3, 2, setup3.builder, setup3.assignment);
  CreateArcWithCost<TypeParam>(2, 3, 3, setup3.builder, setup3.assignment);
  CreateArcWithCost<TypeParam>(0, 4, 4, setup3.builder, setup3.assignment);
  CreateArcWithCost<TypeParam>(0, 5, 5, setup3.builder, setup3.assignment);
  setup3.Finalize();
  EXPECT_FALSE(setup3.assignment->ComputeAssignment());

  // Another balanced graph with no perfect matching, but with plenty
  // of in/out degree for each node.
  AssignmentProblemSetup<TypeParam> setup4(5, 12, false);
  CreateArcWithCost<TypeParam>(0, 5, 0, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(0, 6, 2, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(1, 5, 3, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(1, 6, 4, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(2, 5, 4, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(2, 6, 4, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(3, 7, 4, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(3, 8, 4, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(3, 9, 4, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(4, 7, 4, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(4, 8, 4, setup4.builder, setup4.assignment);
  CreateArcWithCost<TypeParam>(4, 9, 4, setup4.builder, setup4.assignment);
  setup4.Finalize();
  EXPECT_FALSE(setup4.assignment->ComputeAssignment());
}

// A helper function template for setting up assignment problems based on
// dynamic graph types without an interposed GraphBuildManager.
template <typename DynamicGraphType, typename AssignmentGraphType>
static ArcIndex CreateArcWithCost(
    NodeIndex tail, NodeIndex head, CostValue cost, DynamicGraphType* graph,
    LinearSumAssignment<AssignmentGraphType>* assignment) {
  ArcIndex arc = graph->AddArc(tail, head);
  assignment->SetArcCost(arc, cost);
  return arc;
}

// An empty fixture template to collect the types of graphs on which we want to
// base the LinearSumAssignment template instances that we test in ways that
// require dynamic graphs. The only such tests are the ones that call the
// LinearSumAssignment<>::OptimizeGraphLayout() method.
template <typename GraphType>
class LinearSumAssignmentTestWithDynamicGraph : public ::testing::Test {};

typedef ::testing::Types<EbertGraph<int16_t, int16_t>,
                         EbertGraph<int16_t, ArcIndex>,
                         EbertGraph<NodeIndex, ArcIndex>>
    DynamicGraphTypesForAssignmentTesting;

TYPED_TEST_SUITE(LinearSumAssignmentTestWithDynamicGraph,
                 DynamicGraphTypesForAssignmentTesting);

// The EpsilonOptimal test and the PrecisionWarning test cannot be parameterized
// by the type of the underlying graph because doing so is not supported by the
// FRIEND_TEST() macro used in the LinearSumAssignment class template to grant
// these tests access to private methods of LinearSumAssignment.
TEST(LinearSumAssignmentFriendTest, EpsilonOptimal) {
  StarGraph g(4, 4);
  LinearSumAssignment<StarGraph> a(g, 2);
  CreateArcWithCost(0, 2, 0, &g, &a);
  CreateArcWithCost(0, 3, 2, &g, &a);
  CreateArcWithCost(1, 2, 3, &g, &a);
  CreateArcWithCost(1, 3, 4, &g, &a);
  a.FinalizeSetup();  // needed to initialize epsilon_
  EXPECT_TRUE(a.EpsilonOptimal());
}

// TODO(user): The following test is too slow to be run by default with
// non-optimized builds, but it has actually found a bug so I won't delete it
// entirely. Figure out a way to get it run regularly in optimized build mode
// without bogging down the normal set of fastbuild tests people need to run.
#if LARGE
TEST(LinearSumAssignmentPrecisionTest, PrecisionWarning) {
  const NodeIndex kNumLeftNodes = 10000000;
  util::ListGraph<> g(2 * kNumLeftNodes, 2 * kNumLeftNodes);
  LinearSumAssignment<util::ListGraph<>> a(g, kNumLeftNodes);
  int64_t node_count = 0;
  for (NodeIndex left_node = 0; node_count < kNumLeftNodes;
       ++node_count, ++left_node) {
    CreateArcWithCost(left_node, kNumLeftNodes + left_node, kNumLeftNodes, &g,
                      &a);
  }
  EXPECT_FALSE(a.FinalizeSetup());
  // This is a simple problem so we should be able to solve it despite the large
  // arc costs.
  EXPECT_TRUE(a.ComputeAssignment());
}
#endif  // LARGE

class MacholWien
    : public ::testing::TestWithParam<::testing::tuple<NodeIndex, bool>> {};

// The following test computes assignments on the instances described in:
// Robert E. Machol and Michael Wien, "Errata: A Hard Assignment Problem",
// Operations Research, vol. 25, p. 364, 1977.
// http://www.jstor.org/stable/169842
//
// Such instances proved difficult for the Hungarian method.
//
// The test parameter specifies the problem size and the stack/queue active node
// list flag.
TEST_P(MacholWien, SolveHardProblem) {
  using Graph = ::util::CompleteBipartiteGraph<>;
  NodeIndex n = ::testing::get<0>(GetParam());
  absl::SetFlag(&FLAGS_assignment_stack_order, ::testing::get<1>(GetParam()));
  Graph graph(n, n);
  LinearSumAssignment<Graph> assignment(graph, n);
  for (NodeIndex i = 0; i < n; ++i) {
    for (NodeIndex j = 0; j < n; ++j) {
      const ArcIndex arc = graph.GetArc(i, n + j);
      assignment.SetArcCost(arc, i * j);
    }
  }
  EXPECT_TRUE(assignment.ComputeAssignment());
  for (LinearSumAssignment<Graph>::BipartiteLeftNodeIterator node_it(
           assignment);
       node_it.Ok(); node_it.Next()) {
    const NodeIndex left_node = node_it.Index();
    const NodeIndex right_node = assignment.GetMate(left_node);
    EXPECT_EQ(2 * n - 1, left_node + right_node);
  }
}

#if LARGE
// Without optimizing compilation, a 1000x1000 Machol-Wien problem takes too
// long to solve as a even a large test. A non-optimized run of the following
// counts as an enormous test. With optimization it is merely medium in size.
INSTANTIATE_TEST_SUITE_P(
    MacholWienProblems, MacholWien,
    ::testing::Combine(::testing::Values(10,    /* trivial */
                                         100,   /* less trivial */
                                         1000), /* moderate */
                       ::testing::Bool()));
#else   // LARGE
INSTANTIATE_TEST_CASE_P(MacholWienProblems, MacholWien,
                        ::testing::Combine(::testing::Values(10,    // trivial
                                                             100),  // less
                                                                    // trivial
                                           ::testing::Bool()));
#endif  // LARGE

// Helper function for random-assignment benchmarks.
template <typename GraphType, bool optimize_layout>
void ConstructRandomAssignment(
    const int left_nodes, const int average_degree, const int cost_limit,
    std::unique_ptr<GraphType>* graph,
    std::unique_ptr<LinearSumAssignment<GraphType>>* assignment) {
  const int kNodes = 2 * left_nodes;
  const int kArcs = left_nodes * average_degree;
  const int kRandomSeed = 0;
  std::mt19937 randomizer(kRandomSeed);
  AnnotatedGraphBuildManager<GraphType>* builder =
      new AnnotatedGraphBuildManager<GraphType>(kNodes, kArcs, optimize_layout);
  assignment->reset(new LinearSumAssignment<GraphType>(left_nodes, kArcs));
  for (int i = 0; i < kArcs; ++i) {
    const int left = absl::Uniform(randomizer, 0, left_nodes);
    const int right = left_nodes + absl::Uniform(randomizer, 0, left_nodes);
    const CostValue cost = absl::Uniform(randomizer, 0, cost_limit);
    CreateArcWithCost(left, right, cost, builder, assignment->get());
  }
  std::unique_ptr<PermutationCycleHandler<ArcIndex>> cycle_handler(
      assignment->get()->ArcAnnotationCycleHandler());
  graph->reset(builder->Graph(cycle_handler.get()));
  assignment->get()->SetGraph(graph->get());
}

// Same as ConstructRandomAssignment, but for the new API.
template <typename GraphType>
void ConstructRandomAssignmentForNewGraphApi(
    const int left_nodes, const int average_degree, const int cost_limit,
    std::unique_ptr<GraphType>* graph,
    std::unique_ptr<LinearSumAssignment<GraphType>>* assignment) {
  const int kNodes = 2 * left_nodes;
  const int kArcs = left_nodes * average_degree;
  const int kRandomSeed = 0;
  std::mt19937 randomizer(kRandomSeed);
  std::vector<CostValue> arc_costs;
  arc_costs.reserve(kArcs);
  graph->reset(new GraphType(kNodes, kArcs));
  for (int i = 0; i < kArcs; ++i) {
    const int left = absl::Uniform(randomizer, 0, left_nodes);
    const int right = left_nodes + absl::Uniform(randomizer, 0, left_nodes);
    const CostValue cost = absl::Uniform(randomizer, 0, cost_limit);
    graph->get()->AddArc(left, right);
    arc_costs.push_back(cost);
  }

  // Finalize the graph.
  std::vector<typename GraphType::ArcIndex> permutation;
  graph->get()->Build(&permutation);
  util::Permute(permutation, &arc_costs);

  // Create the assignment.
  assignment->reset(
      new LinearSumAssignment<GraphType>(*(graph->get()), left_nodes));
  for (int arc = 0; arc < kArcs; ++arc) {
    assignment->get()->SetArcCost(arc, arc_costs[arc]);
  }
}

// Benchmark function for assignment-problem construction only, no solution.
template <typename GraphType, bool optimize_layout>
void BM_ConstructRandomAssignmentProblem(benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const CostValue kCostLimit = 1000000;
  for (auto _ : state) {
    std::unique_ptr<GraphType> graph;
    std::unique_ptr<LinearSumAssignment<GraphType>> assignment;
    ConstructRandomAssignment<GraphType, optimize_layout>(
        kLeftNodes, kAverageDegree, kCostLimit, &graph, &assignment);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE2(BM_ConstructRandomAssignmentProblem, StarGraph, false);
BENCHMARK_TEMPLATE2(BM_ConstructRandomAssignmentProblem, StarGraph, true);

template <typename GraphType>
void BM_ConstructRandomAssignmentProblemWithNewGraphApi(
    benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const CostValue kCostLimit = 1000000;
  for (auto _ : state) {
    std::unique_ptr<GraphType> graph;
    std::unique_ptr<LinearSumAssignment<GraphType>> assignment;
    ConstructRandomAssignmentForNewGraphApi<GraphType>(
        kLeftNodes, kAverageDegree, kCostLimit, &graph, &assignment);
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE(BM_ConstructRandomAssignmentProblemWithNewGraphApi,
                   util::ListGraph<>);
BENCHMARK_TEMPLATE(BM_ConstructRandomAssignmentProblemWithNewGraphApi,
                   util::StaticGraph<>);

// Benchmark function for assignment-problem solution only, with
// problem-construction timing excluded.
template <typename GraphType, bool optimize_layout>
void BM_SolveRandomAssignmentProblem(benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const CostValue kCostLimit = 1000000;
  std::unique_ptr<GraphType> graph;
  std::unique_ptr<LinearSumAssignment<GraphType>> assignment;
  ConstructRandomAssignment<GraphType, optimize_layout>(
      kLeftNodes, kAverageDegree, kCostLimit, &graph, &assignment);
  for (auto _ : state) {
    assignment->ComputeAssignment();
    EXPECT_EQ(65415697, assignment->GetCost());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE2(BM_SolveRandomAssignmentProblem, StarGraph, false);
BENCHMARK_TEMPLATE2(BM_SolveRandomAssignmentProblem, StarGraph, true);

template <typename GraphType>
void BM_SolveRandomAssignmentProblemWithNewGraphApi(benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const CostValue kCostLimit = 1000000;
  std::unique_ptr<GraphType> graph;
  std::unique_ptr<LinearSumAssignment<GraphType>> assignment;
  ConstructRandomAssignmentForNewGraphApi<GraphType>(
      kLeftNodes, kAverageDegree, kCostLimit, &graph, &assignment);
  for (auto _ : state) {
    assignment->ComputeAssignment();
    EXPECT_EQ(65415697, assignment->GetCost());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE(BM_SolveRandomAssignmentProblemWithNewGraphApi,
                   util::ListGraph<>);
BENCHMARK_TEMPLATE(BM_SolveRandomAssignmentProblemWithNewGraphApi,
                   util::StaticGraph<>);

// Benchmark function for assignment-problem construction and solution, with
// problem-construction timing included.
template <typename GraphType, bool optimize_layout>
void BM_ConstructAndSolveRandomAssignmentProblem(benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const CostValue kCostLimit = 1000000;
  for (auto _ : state) {
    std::unique_ptr<GraphType> graph;
    std::unique_ptr<LinearSumAssignment<GraphType>> assignment;
    ConstructRandomAssignment<GraphType, optimize_layout>(
        kLeftNodes, kAverageDegree, kCostLimit, &graph, &assignment);
    assignment->ComputeAssignment();
    EXPECT_EQ(65415697, assignment->GetCost());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE2(BM_ConstructAndSolveRandomAssignmentProblem, StarGraph,
                    false);
BENCHMARK_TEMPLATE2(BM_ConstructAndSolveRandomAssignmentProblem, StarGraph,
                    true);

template <typename GraphType>
void BM_ConstructAndSolveRandomAssignmentProblemWithNewGraphApi(
    benchmark::State& state) {
  const int kLeftNodes = 10000;
  const int kAverageDegree = 250;
  const CostValue kCostLimit = 1000000;
  for (auto _ : state) {
    std::unique_ptr<GraphType> graph;
    std::unique_ptr<LinearSumAssignment<GraphType>> assignment;
    ConstructRandomAssignmentForNewGraphApi<GraphType>(
        kLeftNodes, kAverageDegree, kCostLimit, &graph, &assignment);
    assignment->ComputeAssignment();
    EXPECT_EQ(65415697, assignment->GetCost());
  }
  state.SetItemsProcessed(static_cast<int64_t>(state.max_iterations) *
                          kLeftNodes * kAverageDegree);
}

BENCHMARK_TEMPLATE(BM_ConstructAndSolveRandomAssignmentProblemWithNewGraphApi,
                   util::ListGraph<>);
BENCHMARK_TEMPLATE(BM_ConstructAndSolveRandomAssignmentProblemWithNewGraphApi,
                   util::StaticGraph<>);

// The order of initializing the edges in the graph made the difference between
// finding an optimal assignment and erroneously failing to finding one because
// an unlucky order of edges could cause price reductions greater than the slack
// relabeling amount.
class ReorderedGraphTest : public testing::Test {
 public:
  struct Edge {
    size_t from_node;
    size_t to_node;
    int64_t cost;
  };

  ReorderedGraphTest() {}

  void TestMe(const size_t left_nodes, absl::Span<const Edge> ordered_edges) {
    std::vector<int64_t> edge_costs;
    typedef util::StaticGraph<size_t, size_t> GraphType;
    GraphType graph(2 * left_nodes, ordered_edges.size());

    for (const auto& edge : ordered_edges) {
      graph.AddArc(/* tail= */ edge.from_node,
                   /* head= */ edge.to_node);
      edge_costs.push_back(edge.cost);
    }
    ASSERT_THAT(edge_costs.size(), Eq(graph.num_arcs()));

    {
      std::vector<typename GraphType::ArcIndex> arc_permutation;
      graph.Build(&arc_permutation);
      util::Permute(arc_permutation, &edge_costs);
    }

    LinearSumAssignment<GraphType> assignment(graph, left_nodes);
    for (size_t arc = 0; arc != edge_costs.size(); ++arc) {
      assignment.SetArcCost(arc, edge_costs[arc]);
    }
    EXPECT_TRUE(assignment.FinalizeSetup());
    EXPECT_TRUE(assignment.ComputeAssignment());
  }
};

// The edge orderings in this test were solved correctly.
TEST_F(ReorderedGraphTest, Bug64485671PassingOrder) {
  TestMe(2 /* left_nodes */, {
                                 {0, 3, 393217},
                                 {1, 3, 393217},
                                 {1, 2, 393216},
                                 {0, 2, 163840},
                             });
  TestMe(2 /* left_nodes */, {
                                 {0, 3, 20},
                                 {1, 3, 20},
                                 {1, 2, 19},
                                 {0, 2, 0},
                             });
}

// The edge orderings in this test incorrectly triggered infeasibility
// detection. These are the same edge costs as in the above "PassingOrder" test,
// just given in a different order.
TEST_F(ReorderedGraphTest, Bug64485671FailingOrder) {
  TestMe(/*left_nodes=*/2, {
                               {0, 3, 393217},
                               {1, 2, 393216},
                               {0, 2, 163840},
                               {1, 3, 393217},
                           });
  TestMe(/*left_nodes=*/2, {
                               {0, 3, 20},
                               {1, 2, 19},
                               {0, 2, 0},
                               {1, 3, 20},
                           });
}

}  // namespace operations_research
