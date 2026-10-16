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

// Unit tests for TopologicalSorter.

#include "ortools/graph/topologicalsorter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numeric>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "gtest/gtest.h"
#include "ortools/base/dump_vars.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/strong_int.h"
#include "ortools/graph/generic_graph.h"
#include "ortools/graph/graph.h"
#include "ortools/util/random_subset.h"

namespace util {
namespace {

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAre;
using ::testing::status::IsOkAndHolds;
using ::testing::status::StatusIs;
using ::util::graph::GenericGraph;

DEFINE_STRONG_INT_TYPE(StrongNodeId, int32_t);
DEFINE_STRONG_INT_TYPE(StrongArcId, int32_t);

template <class TopoSorter>
class TopologicalSorterTest : public ::testing::Test {
 protected:
  TopoSorter sorter_;
  bool cyclic_;
};

typedef ::testing::Types<TopologicalSorter<std::string, false>,
                         TopologicalSorter<std::string, true>>
    TopoSorterTypes;
TYPED_TEST_SUITE(TopologicalSorterTest, TopoSorterTypes);

template <class TopoSorter>
class TopologicalSorterDeathTest : public TopologicalSorterTest<TopoSorter> {};
TYPED_TEST_SUITE(TopologicalSorterDeathTest, TopoSorterTypes);

TYPED_TEST(TopologicalSorterTest, TestSimpleChain) {
  // A simple chain:  A->B->C->D
  this->sorter_.AddEdge("A", "B");
  this->sorter_.AddEdge("B", "C");
  this->sorter_.AddEdge("C", "D");

  EXPECT_EQ(1, this->sorter_.GetCurrentFringeSize());

  std::string next;
  EXPECT_TRUE(this->sorter_.GetNext(&next, &this->cyclic_));
  EXPECT_EQ(next, "A");
  EXPECT_FALSE(this->cyclic_);
  EXPECT_TRUE(this->sorter_.GetNext(&next, &this->cyclic_));
  EXPECT_EQ(next, "B");
  EXPECT_FALSE(this->cyclic_);
  EXPECT_TRUE(this->sorter_.GetNext(&next, &this->cyclic_));
  EXPECT_EQ(next, "C");
  EXPECT_FALSE(this->cyclic_);
  EXPECT_TRUE(this->sorter_.GetNext(&next, &this->cyclic_));
  EXPECT_EQ(next, "D");
  EXPECT_FALSE(this->cyclic_);

  EXPECT_FALSE(this->sorter_.GetNext(&next, &this->cyclic_));
  EXPECT_FALSE(this->cyclic_);
}

TYPED_TEST(TopologicalSorterTest, TestJoinedEntryChains) {
  // Two chains joined where edges enter a node.
  // A---->B->C
  // X---/
  this->sorter_.AddEdge("A", "B");
  this->sorter_.AddEdge("B", "C");
  this->sorter_.AddEdge("X", "B");

  EXPECT_EQ(2, this->sorter_.GetCurrentFringeSize());

  std::string first;
  CHECK(this->sorter_.GetNext(&first, &this->cyclic_));
  CHECK((first == "A") || (first == "X"));

  std::string second;
  CHECK(this->sorter_.GetNext(&second, &this->cyclic_));
  CHECK(first != second);
  CHECK((second == "A") || (second == "X"));

  EXPECT_EQ(1, this->sorter_.GetCurrentFringeSize());

  std::string next;
  CHECK(this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK_EQ(next, "B");
  CHECK(this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK_EQ(next, "C");

  CHECK(!this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK(!this->cyclic_);
}

TYPED_TEST(TopologicalSorterTest, TestJoinedExitChains) {
  // Two chains joined where edges exit a node.
  // A->B->C----->X
  //       \----->Y
  this->sorter_.AddEdge("A", "B");
  this->sorter_.AddEdge("B", "C");
  this->sorter_.AddEdge("C", "X");
  this->sorter_.AddEdge("C", "Y");

  EXPECT_EQ(1, this->sorter_.GetCurrentFringeSize());

  std::string next;
  CHECK(this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK_EQ(next, "A");
  CHECK(this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK_EQ(next, "B");
  CHECK(this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK_EQ(next, "C");

  EXPECT_EQ(2, this->sorter_.GetCurrentFringeSize());

  std::string first;
  CHECK(this->sorter_.GetNext(&first, &this->cyclic_));
  CHECK((first == "X") || (first == "Y"));

  std::string second;
  CHECK(this->sorter_.GetNext(&second, &this->cyclic_));
  CHECK((second == "X") || (second == "Y"));
  CHECK(first != second);

  EXPECT_EQ(0, this->sorter_.GetCurrentFringeSize());

  CHECK(!this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK(!this->cyclic_);
}

TYPED_TEST(TopologicalSorterTest, TestDisconnectedChains) {
  // Disconnected combination of two chains.
  // A1->A2->A3
  // X1->X2->X3
  this->sorter_.AddEdge("A1", "A2");
  this->sorter_.AddEdge("A2", "A3");
  this->sorter_.AddEdge("X1", "X2");
  this->sorter_.AddEdge("X2", "X3");

  // Push the expected order of nodes on each chain.
  std::vector<std::string> chain1;
  chain1.push_back("A3");
  chain1.push_back("A2");
  chain1.push_back("A1");
  std::vector<std::string> chain2;
  chain2.push_back("X3");
  chain2.push_back("X2");
  chain2.push_back("X1");

  // Now receive the 6 nodes and make sure that they are in order on
  // each chain.
  std::string next;
  for (int i = 0; i < 6; ++i) {
    CHECK(this->sorter_.GetNext(&next, &this->cyclic_));
    if (!chain1.empty() && (next == chain1.back())) {
      chain1.pop_back();
    } else if (!chain2.empty() && (next == chain2.back())) {
      chain2.pop_back();
    } else {
      LOG(FATAL) << "Unexpected node " << next;
    }
  }
  CHECK(!this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK(!this->cyclic_);
}

TYPED_TEST(TopologicalSorterTest, TestSimpleChainWithDisconnectedNode) {
  // A simple chain with one disconnected node:
  //   A->B->C    D
  // Throw in extra calls to AddNode() and mix up the ordering a
  // little bit.
  this->sorter_.AddEdge("A", "B");
  this->sorter_.AddNode("A");
  this->sorter_.AddEdge("B", "C");
  this->sorter_.AddNode("D");
  this->sorter_.AddNode("B");
  this->sorter_.AddNode("C");

  // Push the expected order of nodes on the two chains.
  std::vector<std::string> chain1;
  chain1.push_back("C");
  chain1.push_back("B");
  chain1.push_back("A");
  std::vector<std::string> chain2;
  chain2.push_back("D");

  // Now receive the 4 nodes and make sure that they are in order on
  // each chain.
  std::string next;
  for (int i = 0; i < 4; ++i) {
    CHECK(this->sorter_.GetNext(&next, &this->cyclic_));
    if (!chain1.empty() && (next == chain1.back())) {
      chain1.pop_back();
    } else if (!chain2.empty() && (next == chain2.back())) {
      chain2.pop_back();
    } else {
      LOG(FATAL) << "Unexpected node " << next;
    }
  }
  CHECK(!this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK(!this->cyclic_);
}

TYPED_TEST(TopologicalSorterTest, TestCycleSimple) {
  std::string next;

  // A trivial cycle (traversal will not start).
  // A1 --> A2 --> A3 ---
  // /|\                |
  //  -------------------
  this->sorter_.AddEdge("A1", "A2");
  this->sorter_.AddEdge("A2", "A3");
  this->sorter_.AddEdge("A3", "A1");
  CHECK(!this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK(this->cyclic_);
}

TYPED_TEST(TopologicalSorterTest, TestCycleLessSimple) {
  std::string next;

  // A more complicated cycle (we will traverse the nodes outside the
  // cycle before failing).
  // A1 --> A2 --> A3 ---------
  //        A4---/            |
  //       /|\                |
  //        -------------------
  this->sorter_.AddEdge("A1", "A2");
  this->sorter_.AddEdge("A2", "A3");
  this->sorter_.AddEdge("A4", "A3");
  this->sorter_.AddEdge("A3", "A4");
  for (int i = 0; i < 2; ++i) {
    CHECK(this->sorter_.GetNext(&next, &this->cyclic_));
    CHECK(!this->cyclic_);
  }
  CHECK(!this->sorter_.GetNext(&next, &this->cyclic_));
  CHECK(this->cyclic_);
}

TYPED_TEST(TopologicalSorterTest, EmptyGraph) {
  std::vector<std::string> cycle;
  std::string next;
  EXPECT_FALSE(this->sorter_.TraversalStarted());
  EXPECT_FALSE(this->sorter_.GetNext(&next, &this->cyclic_, &cycle));
  EXPECT_TRUE(this->sorter_.TraversalStarted());
  EXPECT_EQ(0, this->sorter_.GetCurrentFringeSize());
  EXPECT_FALSE(this->cyclic_);
  EXPECT_EQ(0, cycle.size());
}

TYPED_TEST(TopologicalSorterTest, StartTraversalSeveralTimes) {
  EXPECT_FALSE(this->sorter_.TraversalStarted());
  this->sorter_.StartTraversal();
  EXPECT_TRUE(this->sorter_.TraversalStarted());
  std::string next;
  this->sorter_.GetNext(&next, &this->cyclic_);
  EXPECT_TRUE(this->sorter_.TraversalStarted());
  this->sorter_.StartTraversal();
  EXPECT_TRUE(this->sorter_.TraversalStarted());
}

TYPED_TEST(TopologicalSorterTest, GetNextVlog) {
  absl::SetVLogLevel("topologicalsorter", 2);
  this->sorter_.AddEdge("Yo", "Yo");
  std::string next;
  EXPECT_FALSE(this->sorter_.GetNext(&next, &this->cyclic_));
  EXPECT_TRUE(this->cyclic_);
}

TYPED_TEST(TopologicalSorterDeathTest, AddNodeAfterTraversalStarted) {
  this->sorter_.AddNode("Yo");
  std::string next;
  this->sorter_.GetNext(&next, &this->cyclic_);
  EXPECT_DEATH(this->sorter_.AddNode("Yo"),
               "Cannot add nodes after starting traversal");
}

TYPED_TEST(TopologicalSorterDeathTest, AddEdgeAfterTraversalStarted) {
  this->sorter_.StartTraversal();
  EXPECT_DEATH(this->sorter_.AddEdge("Hop", "Hap"),
               "Cannot add edges after starting traversal");
}

TEST(TopologicalSorterStableTest, StableSort) {
  // Make a graph with layers where a node in each layer is connected
  // to 3 nodes in the following 2 layers.
  TopologicalSorter<std::pair<int, int>, true> sorter;
  for (int layer = 0; layer < 102; ++layer) {
    for (int i = 0; i < 20; ++i) {
      sorter.AddNode(std::pair<int, int>(layer, i));
    }
  }
  for (int layer = 0; layer < 100; ++layer) {
    for (int i = 0; i < 20; ++i) {
      sorter.AddEdge(std::pair<int, int>(layer, i),
                     std::pair<int, int>(layer + 1, i * (layer + 1) % 20));
      sorter.AddEdge(std::pair<int, int>(layer, i),
                     std::pair<int, int>(layer + 1, 3 * i * (layer + 1) % 20));
      sorter.AddEdge(std::pair<int, int>(layer, i),
                     std::pair<int, int>(layer + 2, 5 * i * (layer + 1) % 20));
    }
  }

  bool cyclic = false;
  std::pair<int, int> next;
  for (int layer = 0; layer < 102; ++layer) {
    for (int i = 0; i < 20; ++i) {
      EXPECT_TRUE(sorter.GetNext(&next, &cyclic));
      EXPECT_FALSE(cyclic);
      std::pair<int, int> expected(layer, i);
      // The order that nodes are returned should be the original order.
      EXPECT_EQ(expected, next);
    }
  }
  EXPECT_FALSE(sorter.GetNext(&next, &cyclic));
  EXPECT_FALSE(cyclic);
}

// Object to use in UserSpecifiedHash test below.
struct HashableObject {
  int x;
};

struct HashableObjectEq {
  bool operator()(const HashableObject& obj1,
                  const HashableObject& obj2) const {
    return obj1.x == obj2.x;
  }
};

struct HashableObjectHasher {
  size_t operator()(const HashableObject& obj) const {
    return std::hash<int>{}(obj.x);
  }
};

TEST(TopologicalSorterHashTest, UserSpecifiedHash) {
  TopologicalSorter<HashableObject, false, HashableObjectHasher,
                    HashableObjectEq>
      sorter;
  HashableObject obj1 = {1};
  HashableObject obj2 = {2};
  HashableObject obj3 = {3};
  sorter.AddEdge(obj2, obj3);
  sorter.AddEdge(obj3, obj1);
  HashableObject next;
  bool cyclic;
  EXPECT_TRUE(sorter.GetNext(&next, &cyclic));
  EXPECT_EQ(2, next.x);
  EXPECT_TRUE(sorter.GetNext(&next, &cyclic));
  EXPECT_EQ(3, next.x);
  EXPECT_TRUE(sorter.GetNext(&next, &cyclic));
  EXPECT_EQ(1, next.x);
  EXPECT_FALSE(sorter.GetNext(&next, &cyclic));
}

TEST(DenseIntTopologicalSorterTest, ConstructorWithSizeHintAddsNodes) {
  DenseIntTopologicalSorter int_sorter(4);
  int_sorter.AddNode(1);
  int num_nodes_extracted = 0;
  int next;
  bool cyclic;
  while (int_sorter.GetNext(&next, &cyclic)) {
    ++num_nodes_extracted;
  }
  EXPECT_EQ(4, num_nodes_extracted);
}

TEST(DenseIntTopologicalSorterTest, ConstructorWithSizeHintAddsNodesStable) {
  DenseIntStableTopologicalSorter int_sorter(4);
  int_sorter.AddNode(1);
  int num_nodes_extracted = 0;
  int next;
  bool cyclic;
  while (int_sorter.GetNext(&next, &cyclic)) {
    ++num_nodes_extracted;
  }
  EXPECT_EQ(4, num_nodes_extracted);
}

TEST(DenseIntTopologicalCycleTest, EmptyGraph) {
  int num_nodes = 0;
  std::vector<std::pair<int, int>> arcs({});

  std::vector<int> cycle = FindCycleInDenseIntGraph(num_nodes, arcs);

  EXPECT_EQ(0, cycle.size());
}

TEST(DenseIntTopologicalCycleTest, EmptyArcs) {
  int num_nodes = 6;
  std::vector<std::pair<int, int>> arcs({});

  std::vector<int> cycle = FindCycleInDenseIntGraph(num_nodes, arcs);

  EXPECT_EQ(0, cycle.size());
}

TEST(DenseIntTopologicalCycleTest, NoCycles) {
  int num_nodes = 4;
  std::vector<std::pair<int, int>> arcs({{0, 1}, {1, 2}, {1, 3}});

  std::vector<int> cycle = FindCycleInDenseIntGraph(num_nodes, arcs);

  EXPECT_EQ(0, cycle.size());
}

TEST(DenseIntTopologicalCycleTest, NoCyclesMultiedge) {
  std::vector<std::vector<int>> arcs({{1}, {2, 2}, {}});
  EXPECT_THAT(graph::FindCycleInGraph(arcs), IsOkAndHolds(ElementsAre()));
}

TEST(DenseIntTopologicalCycleTest, HasCycle) {
  int num_nodes = 5;
  std::vector<std::pair<int, int>> arcs(
      {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {4, 2}});

  std::vector<int> cycle = FindCycleInDenseIntGraph(num_nodes, arcs);

  std::vector<int> expected({2, 3, 4});
  EXPECT_EQ(expected, cycle);
}

TEST(FindCycleInGraphTest, SingletonCycle) {
  EXPECT_THAT(graph::FindCycleInGraph(std::vector<std::vector<int>>{{0}}),
              IsOkAndHolds(ElementsAre(0)));
}

TEST(FindCycleTest, Empty) {
  EXPECT_THAT(graph::FindCycle(GenericGraph<int>::FromEdges({})), IsEmpty());
}

TEST(FindCycleTest, SimpleCycle) {
  EXPECT_THAT(graph::FindCycle(GenericGraph<std::string>::FromEdges(
                  {{"A", "B"}, {"B", "C"}, {"C", "A"}})),
              UnorderedElementsAre("A", "B", "C"));
}

TEST(FindCycleTest, SingleNodeCycle) {
  constexpr int kNode = 2'012'345'678;
  EXPECT_THAT(graph::FindCycle(GenericGraph<int>::FromEdges({{kNode, kNode}})),
              UnorderedElementsAre(kNode));
}

// Generate a random double X in [0, infinite) such that P(X < k) = 1 - 1/(k+1)
// and return it if it's lower than upper_bound, or try again otherwise.
// This yields a double in [0, upper_bound) which is heavily skewed towards
// lower values, but has a Theta(1/upper_bound) probability of being greater
// than upper_bound/2. The mean value is close to Log(upper_bound) - 1
// when upper_bound is large.
// Details:
// To draw a number x in [0, +infinite) such that P(x <= k) = 1 - 1/(k+1),
// we pick x = 1/(1-RandomDouble()) - 1, where RandomDouble() is uniformly
// picked at random in [0, 1).
// Proof: do the math!
double RandDoubleSkewed(double upper_bound, absl::BitGenRef rng) {
  double ret;
  do {
    ret = 1 / (1 - absl::Uniform<double>(rng, 0, 1)) - 1;
  } while (ret >= upper_bound);
  return ret;
}

TEST(RandDoubleSkewedTest, Trial) {
  absl::BitGen my_random;
  const int kNum = 100000;
  const double kUpperBound = 1000;
  double sum = 0.0;
  int num_larger_than_tenth = 0;
  for (int i = 0; i < kNum; ++i) {
    const double r = RandDoubleSkewed(kUpperBound, my_random);
    ASSERT_LT(r, kUpperBound);
    ASSERT_GE(r, 0);
    sum += r;
    if (r > 0.1 * kUpperBound) {
      ++num_larger_than_tenth;
    }
  }
  const double expected_mean = log(static_cast<double>(kUpperBound)) - 1.0;
  EXPECT_GE(sum / kNum, expected_mean / 2);
  EXPECT_LE(sum / kNum, expected_mean * 2);
  const double kExpectedProportionLargerThanTenth =
      1.0 / (0.1 * kUpperBound + 1);
  const double expected_num_larger_than_tenth =
      kExpectedProportionLargerThanTenth * kNum;
  EXPECT_GE(num_larger_than_tenth, expected_num_larger_than_tenth / 2);
  EXPECT_LE(num_larger_than_tenth, expected_num_larger_than_tenth * 2);
}

// The RemoveDuplicates() function gets called rarely enough, in the
// randomized tests below, that bugs in it may avoid detection by the
// randomized test below. So we test it thoroughly here.
TEST(RemoveDuplicatesTest, LargeRandomizedTest) {
  absl::BitGen my_random;

  // Build a large test case with N vectors of various sizes in [0, N)
  // (with average size around log(N)).  Each vector is filled with
  // possibly repeating random integers picked either from [0, N) or
  // from [0, RandUniform(size)), yielding vectors with various sizes
  // and various degrees of duplication.
  //
  // We keep a copy of those vectors with their duplicates removed in
  // a canonical way, to compare them with the output.
  const int kNumVectors = 1000000;
  std::vector<
      ::util::internal::DenseIntTopologicalSorterTpl<true>::AdjacencyList>
      vectors(kNumVectors);
  std::vector<
      ::util::internal::DenseIntTopologicalSorterTpl<true>::AdjacencyList>
      vectors_without_duplicates(kNumVectors);
  int expected_num_duplicates = 0;
  for (int i = 0; i < kNumVectors; ++i) {
    const int size = static_cast<int>(RandDoubleSkewed(kNumVectors, my_random));
    const int range = absl::Bernoulli(my_random, 1.0 / 2)
                          ? kNumVectors
                          : absl::Uniform(my_random, 0, size);
    for (int j = 0; j < size; ++j) {
      vectors[i].push_back(absl::Uniform(my_random, 0, range));
    }
    vectors_without_duplicates[i] = vectors[i];
    gtl::STLSortAndRemoveDuplicates(&vectors_without_duplicates[i]);
    expected_num_duplicates +=
        vectors[i].size() - vectors_without_duplicates[i].size();
  }

  const int num_duplicates =
      ::util::internal::DenseIntTopologicalSorterTpl<true>::RemoveDuplicates(
          &vectors, 0);
  EXPECT_EQ(expected_num_duplicates, num_duplicates);
  for (int i = 0; i < vectors.size(); ++i) {
    std::sort(vectors[i].begin(), vectors[i].end());
    ASSERT_EQ(vectors_without_duplicates[i].size(), vectors[i].size());
    ASSERT_TRUE(vectors_without_duplicates[i] == vectors[i]) << i;
  }
}

// Create an array of distinct random strings, with size taken from an
// exponential distribution with a given mean.
std::vector<std::string> CreateRandomStringArray(absl::BitGenRef my_random,
                                                 int num_strings,
                                                 int avg_string_size) {
  absl::flat_hash_set<std::string> string_set;
  // Make sure we push into the vector in random order.
  // If we iterate string_set afterwards we will have the elements in hash
  // order, which means that a subsequence of them will have dependent hash
  // bits and cause node_order below to have bad hash performance.
  std::vector<std::string> result;
  string_set.rehash(num_strings);
  while (string_set.size() < num_strings) {
    std::string str(static_cast<int>(absl::Exponential<double>(my_random) *
                                     avg_string_size),
                    'A');
    for (int i = 0; i < str.size(); ++i) {
      str[i] = ' ' + absl::Uniform<char>(my_random, 0, 64);
    }
    if (string_set.insert(str).second) {
      result.push_back(std::move(str));
    }
  }
  return result;
}

// Create an array of the integers [0 .. array_size-1], randomly shuffled.
std::vector<int> CreateRandomDenseIntArray(absl::BitGenRef my_random,
                                           int size) {
  std::vector<int> array;
  array.reserve(size);
  for (int i = 0; i < size; ++i) {
    array.push_back(i);
  }
  std::shuffle(array.begin(), array.end(), my_random);
  return array;
}

// Create a random DAG on the nodes 0..num_nodes-1, by drawing a topological
// order at random, then adding arcs that obey the order.
// Edges are taken at random, and may be repeated or not.
std::vector<std::pair<int, int>> CreateRandomDAG(
    absl::BitGenRef my_random, int num_nodes, int num_edges,
    bool allow_repeated_edges,
    // If non-null, the topological order of the nodes is written here.
    std::vector<int>* topo_order = nullptr) {
  if (num_edges <= 0) {
    return std::vector<std::pair<int, int>>();
  }
  CHECK_GE(num_nodes, 2)
      << "Cannot generate random DAG with at least one edge and"
      << " less than 2 nodes.";

  // Create a random order, which will be an underlying topological order.
  std::vector<int> tmp;
  std::vector<int>& nodes_topologically_sorted =
      topo_order == nullptr ? tmp : *topo_order;
  nodes_topologically_sorted.assign(num_nodes, -1);
  for (int i = 0; i < num_nodes; ++i) {
    nodes_topologically_sorted[i] = i;
  }
  std::shuffle(nodes_topologically_sorted.begin(),
               nodes_topologically_sorted.end(), my_random);

  if (allow_repeated_edges) {
    std::vector<std::pair<int, int>> edges;
    edges.reserve(num_edges);
    while (edges.size() < num_edges) {
      const int from = absl::Uniform(my_random, 0, num_nodes);
      const int to = absl::Uniform(my_random, 0, num_nodes);
      if (to <= from) {
        continue;
      }
      edges.push_back(std::make_pair(nodes_topologically_sorted[from],
                                     nodes_topologically_sorted[to]));
    }
    return edges;
  } else {
    CHECK_LE(static_cast<int64_t>(num_edges),
             static_cast<int64_t>(num_nodes) * (num_nodes - 1) / 2)
        << "Too many edges for " << num_nodes << " nodes.";
    absl::flat_hash_set<std::pair<int, int>> edges(num_edges);
    while (edges.size() < num_edges) {
      const int from = absl::Uniform(my_random, 0, num_nodes);
      const int to = absl::Uniform(my_random, 0, num_nodes);
      if (to <= from) {
        continue;
      }
      edges.insert(std::make_pair(nodes_topologically_sorted[from],
                                  nodes_topologically_sorted[to]));
    }
    return std::vector<std::pair<int, int>>(edges.begin(), edges.end());
  }
}

// Create a random multigraph (i.e. a graph where edges can be repeated)
// with the given number of nodes and edges, and with at least one cycle
// of the given size. This cycle is created first, and then random edges
// are added.
std::vector<std::pair<int, int>> CreateRandomCyclicMultiGraph(
    absl::BitGenRef my_random, int num_nodes, int num_edges, int cycle_size) {
  CHECK_GE(cycle_size, 1);  // A cycle of size 1 is a self-loop X->X.
  CHECK_GE(num_edges, cycle_size);
  CHECK_LE(cycle_size, num_nodes);
  std::vector<std::pair<int, int>> edges;
  // Create the cycle.
  for (int i = 1; i < cycle_size; ++i) {
    edges.push_back(std::make_pair(i - 1, i));
  }
  edges.push_back(std::make_pair(cycle_size - 1, 0));
  // Add remaining edges.
  while (edges.size() < num_edges) {
    edges.push_back(std::make_pair(absl::Uniform(my_random, 0, num_nodes),
                                   absl::Uniform(my_random, 0, num_nodes)));
  }
  // Shuffle the cyclic edges.
  std::shuffle(edges.begin(), edges.end(), my_random);
  return edges;
}

template <class TopoSorter>
class TopologicalSorterRandomizedTest : public testing::Test {
 protected:
  absl::BitGen my_random_;
};
TYPED_TEST_SUITE(TopologicalSorterRandomizedTest, TopoSorterTypes);

TYPED_TEST(TopologicalSorterRandomizedTest, DAG) {
  // Try to exercise the code thoroughly, by generating random test cases
  // that should cover all corner cases (from empty nodes to full DAGs,
  // with possibly over-repeated edges), and with a volume that should
  // detect all hidden bugs.
  const int kMaxNumNodes = 20000;
  const int kMaxNumEdges = 20000;
  const int kAvgStringSize = 20;
  std::vector<std::string> nodes =
      CreateRandomStringArray(this->my_random_, kMaxNumNodes, kAvgStringSize);
  for (int num_nodes = 1; num_nodes <= kMaxNumNodes;
       num_nodes += 1 + num_nodes / 2 /* num_nodes *= 1.5 */) {
    for (int num_edges = 0; num_edges <= kMaxNumEdges;
         num_edges += 1 + num_edges / 2) {
      // Always allow repeated edges if we have to, otherwise pick a
      // random bool and use that.
      const bool allow_repeated_edges =
          num_edges > (num_nodes * (num_nodes - 1)) / 2
              ? true
              : absl::Bernoulli(this->my_random_, 1.0 / 2);
      if (num_edges > 0 && num_nodes == 1) {
        continue;
      }
      // Create random edges, forming a DAG, and add them.
      std::vector<std::pair<int, int>> edges = CreateRandomDAG(
          this->my_random_, num_nodes, num_edges, allow_repeated_edges);
      // This typename will be the TopologicalSorter with stable_sort
      // enabled or disabled.
      gtest_TypeParam_ topo_sorter;
      for (int i = 0; i < num_nodes; ++i) {
        topo_sorter.AddNode(nodes[i]);
      }
      for (int i = 0; i < edges.size(); ++i) {
        topo_sorter.AddEdge(nodes[edges[i].first], nodes[edges[i].second]);
      }
      std::vector<std::string> cyclic_nodes;
      // Do a full traversal.
      int num_iterations = 0;
      bool cyclic = false;
      absl::flat_hash_map<std::string, int> node_order;
      std::string node_popped;
      while (topo_sorter.GetNext(&node_popped, &cyclic, &cyclic_nodes)) {
        ASSERT_FALSE(cyclic);
        ASSERT_TRUE(
            gtl::InsertIfNotPresent(&node_order, node_popped, num_iterations))
            << "Duplicate node popped: " << node_popped;
        ++num_iterations;
      }
      ASSERT_EQ(num_nodes, num_iterations);
      ASSERT_FALSE(cyclic);
      ASSERT_EQ(0, cyclic_nodes.size());
      // Verify the graph was indeed topologically sorted!
      int num_edges_badly_sorted = 0;
      for (int i = 0; i < edges.size(); ++i) {
        if (node_order.at(nodes[edges[i].first]) >
            node_order.at(nodes[edges[i].second])) {
          ++num_edges_badly_sorted;
        }
      }
      EXPECT_EQ(0, num_edges_badly_sorted)
          << num_edges_badly_sorted << " edges (over " << edges.size()
          << " in total) were badly sorted in a random DAG with " << num_nodes
          << " nodes and " << num_edges << " edges, "
          << (allow_repeated_edges ? "allowing" : "disallowing")
          << " repeated edges.";
    }
  }
}

TYPED_TEST(TopologicalSorterRandomizedTest, CyclicGraph) {
  // Another randomized, high-volume test but this time the graphs are cyclic.
  const int kMaxNumNodes = 10000;
  const int kMaxNumEdges = 10000;
  const int kAvgStringSize = 20;
  std::vector<std::string> nodes =
      CreateRandomStringArray(this->my_random_, kMaxNumNodes, kAvgStringSize);
  for (int num_nodes = 1; num_nodes <= kMaxNumNodes; num_nodes *= 10) {
    for (int num_edges = 1; num_edges <= kMaxNumEdges; num_edges *= 10) {
      // Create a cyclic graph with an initial cycle of length 1, then 2,
      // then min(num_nodes, num_edges).
      std::vector<int> cycle_sizes;
      {
        cycle_sizes.push_back(1);
        const int l = std::min(num_nodes, num_edges);
        if (l > 1) {
          cycle_sizes.push_back(l);
        }
        if (l > 2) {
          cycle_sizes.push_back(2);
        }
      }
      for (int cycle_size : cycle_sizes) {
        // See the similar code in the RandomizedDAGTest.
        std::vector<std::pair<int, int>> edges = CreateRandomCyclicMultiGraph(
            this->my_random_, num_nodes, num_edges, cycle_size);
        // This typename will be the TopologicalSorter with stable_sort
        // enabled or disabled.
        gtest_TypeParam_ topo_sorter;
        for (const std::pair<int, int>& edge : edges) {
          topo_sorter.AddEdge(nodes[edge.first], nodes[edge.second]);
        }
        std::vector<std::string> cyclic_nodes;
        // Do a full traversal.
        int num_iterations = 0;
        bool cyclic = false;
        absl::flat_hash_set<std::string> nodes_popped;
        std::string node_popped;
        while (topo_sorter.GetNext(&node_popped, &cyclic, &cyclic_nodes)) {
          ASSERT_FALSE(cyclic);
          ASSERT_TRUE(nodes_popped.insert(node_popped).second);
          ++num_iterations;
        }
        ASSERT_LT(num_iterations, num_nodes);
        ASSERT_TRUE(cyclic);
        ASSERT_GE(cyclic_nodes.size(), 1);
        // Verify that all edges of the cycle are edges of the graph.
        absl::flat_hash_set<std::pair<std::string, std::string>> cycle_edge_set;
        for (int i = 0; i < cyclic_nodes.size(); ++i) {
          cycle_edge_set.insert(std::make_pair(
              cyclic_nodes[i], cyclic_nodes[(i + 1) % cyclic_nodes.size()]));
        }
        ASSERT_EQ(cyclic_nodes.size(), cycle_edge_set.size());
        int num_hits = 0;
        for (const std::pair<int, int>& edge : edges) {
          auto find_result = cycle_edge_set.find(
              std::make_pair(nodes[edge.first], nodes[edge.second]));
          if (find_result != cycle_edge_set.end()) {
            ++num_hits;
            cycle_edge_set.erase(find_result);
          }
        }
        ASSERT_EQ(cyclic_nodes.size(), num_hits);
      }
    }
  }
}

// Tests for the simplified API.

// For non-stable sorts, we cannot precisely guarantee output order.  Validate
// that the order is consistent with the constraints instead.
//
// Returns "" if output_order is a valid topological order of the graph
// with nodes "init_order" and directed edges "arcs", or an error string if
// "output_order" is not a valid topological ordering.  Will CHECK fail if
// ("init_order", "arcs") does not define a valid graph.
//
// Runs in O(num_nodes + num_edges)
std::string ValidateTopologicalOrder(const std::vector<int>& init_order,
                                     absl::Span<const std::pair<int, int>> arcs,
                                     const std::vector<int>& output_order) {
  // step one: make sure the input is good, i.e. init_order has no duplicates
  // and node referenced in arcs is a member of init_order.
  absl::flat_hash_set<int> input_set(init_order.begin(), init_order.end());
  CHECK_EQ(input_set.size(), init_order.size());
  for (const std::pair<int, int>& arc : arcs) {
    CHECK_EQ(input_set.count(arc.first), 1);
    CHECK_EQ(input_set.count(arc.second), 1);
  }
  if (init_order.size() != output_order.size()) {
    return "output_order not init_order permutation: " +
           DUMP_VARS(output_order.size(), init_order.size()).str();
  }
  absl::flat_hash_set<int> output_set(output_order.begin(), output_order.end());
  if (input_set != output_set) {
    LOG(INFO) << DUMP_VARS(output_order, init_order);
    return "output_order not init_order permutation: elements don't match (see "
           "logs).";
  }
  absl::flat_hash_map<int, int> output_position;
  for (int i = 0; i < output_order.size(); ++i) {
    output_position[output_order[i]] = i;
  }
  for (const std::pair<int, int>& arc : arcs) {
    int pos_first = gtl::FindOrDie(output_position, arc.first);
    int pos_second = gtl::FindOrDie(output_position, arc.second);
    if (pos_first > pos_second) {
      return absl::StrCat("Violated topology constraint: position[", arc.first,
                          "] = ", pos_first, " should be before position[",
                          arc.second, "] = ", pos_second);
    }
  }
  return "";
}

TEST(ValidateTopologicalOrderTest, Empty) {
  EXPECT_EQ(ValidateTopologicalOrder({}, {}, {}), "");
}

TEST(ValidateTopologicalOrderTest, SimpleGraph) {
  std::vector<int> nodes({8, 4, 10, 6});
  std::vector<std::pair<int, int>> arcs({{4, 8}, {8, 10}, {6, 10}});
  // Check the valid orders
  EXPECT_EQ(ValidateTopologicalOrder(nodes, arcs, {6, 4, 8, 10}), "");
  EXPECT_EQ(ValidateTopologicalOrder(nodes, arcs, {4, 6, 8, 10}), "");
  EXPECT_EQ(ValidateTopologicalOrder(nodes, arcs, {4, 8, 6, 10}), "");

  // Check some invalid orders
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {8, 4, 6, 10}),
              HasSubstr("Violated topology constraint"));
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {4, 8, 10, 6}),
              HasSubstr("Violated topology constraint"));
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {6, 8, 4, 10}),
              HasSubstr("Violated topology constraint"));

  // Check if the output is not a permutation
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {}),
              HasSubstr("output_order not init_order permutation"));
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {8}),
              HasSubstr("output_order not init_order permutation"));
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {7}),
              HasSubstr("output_order not init_order permutation"));
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {8, 8}),
              HasSubstr("output_order not init_order permutation"));
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {8, 4, 5, 10}),
              HasSubstr("output_order not init_order permutation"));
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {8, 4, 10, 10}),
              HasSubstr("output_order not init_order permutation"));
  EXPECT_THAT(ValidateTopologicalOrder(nodes, arcs, {8, 4, 6, 10, 10}),
              HasSubstr("output_order not init_order permutation"));
}

TEST(ValidateTopologicalOrderDeathTest, NodesBad) {
  EXPECT_DEATH(
      {
        ValidateTopologicalOrder(/*init_order=*/{4, 8, 4}, /*arcs=*/{{4, 8}},
                                 /*output_order*/ {4, 8});
      },
      "");
}

TEST(ValidateTopologicalOrderDeathTest, ArcsBad) {
  EXPECT_DEATH(
      {
        ValidateTopologicalOrder(/*init_order=*/{4, 8, 10}, /*arcs=*/{{4, 5}},
                                 /*output_order*/ {4, 8, 10});
      },
      "");
  EXPECT_DEATH(
      {
        ValidateTopologicalOrder(/*init_order=*/{4, 8, 10}, /*arcs=*/{{12, 10}},
                                 /*output_order*/ {4, 8, 10});
      },
      "");
}

TEST(SimpleTopologicalSortTest, DenseIntTopologicalSortBasic) {
  std::vector<int> order;
  ASSERT_TRUE(DenseIntTopologicalSort(
      /*num_nodes=*/3, /*arcs=*/{{1, 2}, {2, 0}}, &order));
  ASSERT_THAT(order, ElementsAre(1, 2, 0));
  ASSERT_THAT(DenseIntTopologicalSortOrDie(
                  /*num_nodes=*/3, /*arcs=*/{{1, 2}, {2, 0}}),
              ElementsAre(1, 2, 0));
}

TEST(SimpleTopologicalSortTest, DenseIntStableTopologicalSortBasic) {
  std::vector<int> order;
  ASSERT_TRUE(DenseIntStableTopologicalSort(
      /*num_nodes=*/3, /*arcs=*/{{1, 2}, {2, 0}}, &order));
  ASSERT_THAT(order, ElementsAre(1, 2, 0));
  ASSERT_THAT(DenseIntStableTopologicalSortOrDie(
                  /*num_nodes=*/3, /*arcs=*/{{1, 2}, {2, 0}}),
              ElementsAre(1, 2, 0));
}

TEST(SimpleTopologicalSortTest, TopologicalSortBasic) {
  std::vector<std::string> order;
  ASSERT_TRUE(TopologicalSort(
      /*nodes=*/std::vector<std::string>({"a", "b", "c"}),
      /*arcs=*/{{"b", "c"}, {"c", "a"}}, &order));
  ASSERT_THAT(order, ElementsAre("b", "c", "a"));
  ASSERT_THAT(TopologicalSortOrDie(
                  /*nodes=*/std::vector<std::string>({"a", "b", "c"}),
                  /*arcs=*/{{"b", "c"}, {"c", "a"}}),
              ElementsAre("b", "c", "a"));
}

TEST(SimpleTopologicalSortTest, TopologicalSortCyclicInput) {
  std::vector<std::string> order;
  std::vector<std::string> cycle;
  EXPECT_FALSE(TopologicalSort(
      /*nodes=*/std::vector<std::string>({"a", "b", "c", "d"}),
      /*arcs=*/{{"b", "c"}, {"c", "d"}, {"d", "b"}}, &order, &cycle));
  EXPECT_THAT(cycle,
              AnyOf(ElementsAre("b", "c", "d"), ElementsAre("c", "d", "b"),
                    ElementsAre("d", "b", "c")));
}

TEST(SimpleTopologicalSortTest, StableTopologicalSortBasic) {
  std::vector<std::string> order;
  ASSERT_TRUE(StableTopologicalSort(
      /*nodes=*/std::vector<std::string>({"a", "b", "c"}),
      /*arcs=*/{{"b", "c"}, {"c", "a"}}, &order));
  ASSERT_THAT(order, ElementsAre("b", "c", "a"));
  ASSERT_THAT(StableTopologicalSortOrDie(
                  /*nodes=*/std::vector<std::string>({"a", "b", "c"}),
                  /*arcs=*/{{"b", "c"}, {"c", "a"}}),
              ElementsAre("b", "c", "a"));
}

TEST(SimpleTopologicalSortTest, StableTopologicalSortCyclicInput) {
  std::vector<std::string> order;
  std::vector<std::string> cycle;
  EXPECT_FALSE(StableTopologicalSort(
      /*nodes=*/std::vector<std::string>({"a", "b", "c", "d"}),
      /*arcs=*/{{"b", "c"}, {"c", "d"}, {"d", "b"}}, &order, &cycle));
  // TODO(user): Establish the stability of cycle detection following the
  // suggestions in cl/492049817.
  EXPECT_THAT(cycle,
              AnyOf(ElementsAre("b", "c", "d"), ElementsAre("c", "d", "b"),
                    ElementsAre("d", "b", "c")));
}

TEST(SimpleTopologicalSortTest, TopologicalSortAllInputOrderings) {
  // Warning: runs 8! ~= 40k sorts, one for each permutation
  std::vector<int> nodes({0, 2, 3, 5, 7, 8, 9, 11});
  std::vector<std::pair<int, int>> arcs({{9, 5}, {2, 5}, {5, 8}, {8, 7}});
  do {
    std::vector<int> order;
    ASSERT_TRUE(TopologicalSort(nodes, arcs, &order));
    ASSERT_EQ(ValidateTopologicalOrder(nodes, arcs, order), "")
        << "input: " << absl::StrJoin(nodes, ",");
    ASSERT_EQ(ValidateTopologicalOrder(nodes, arcs,
                                       TopologicalSortOrDie(nodes, arcs)),
              "")
        << "input: " << absl::StrJoin(nodes, ",");
  } while (std::next_permutation(nodes.begin(), nodes.end()));
}

TEST(SimpleTopologicalSortTest, StableTopologicalSortMediumExample) {
  std::vector<std::pair<int, int>> arcs({{9, 5}, {2, 5}, {5, 8}, {8, 7}});
  std::vector<int> order;
  ASSERT_TRUE(StableTopologicalSort(
      /*nodes=*/{0, 2, 3, 5, 7, 8, 9, 11}, arcs, &order));
  ASSERT_THAT(order, ElementsAre(0, 2, 3, 9, 5, 8, 7, 11));
  ASSERT_THAT(StableTopologicalSortOrDie(
                  /*nodes=*/{0, 2, 3, 5, 7, 8, 9, 11}, arcs),
              ElementsAre(0, 2, 3, 9, 5, 8, 7, 11));

  ASSERT_TRUE(StableTopologicalSort(
      /*nodes=*/{11, 2, 3, 5, 7, 8, 9, 0}, arcs, &order));
  ASSERT_THAT(order, ElementsAre(11, 2, 3, 9, 5, 8, 7, 0));
  ASSERT_THAT(StableTopologicalSortOrDie(
                  /*nodes=*/{11, 2, 3, 5, 7, 8, 9, 0}, arcs),
              ElementsAre(11, 2, 3, 9, 5, 8, 7, 0));

  ASSERT_TRUE(StableTopologicalSort(
      /*nodes=*/{0, 5, 7, 8, 2, 3, 9, 11}, arcs, &order));
  ASSERT_THAT(order, ElementsAre(0, 2, 3, 9, 5, 8, 7, 11));
  ASSERT_THAT(StableTopologicalSortOrDie(
                  /*nodes=*/{0, 5, 7, 8, 2, 3, 9, 11}, arcs),
              ElementsAre(0, 2, 3, 9, 5, 8, 7, 11));

  ASSERT_TRUE(StableTopologicalSort(
      /*nodes=*/{0, 5, 7, 8, 9, 2, 3, 11}, arcs, &order));
  ASSERT_THAT(order, ElementsAre(0, 9, 2, 5, 8, 7, 3, 11));
  ASSERT_THAT(StableTopologicalSortOrDie(
                  /*nodes=*/{0, 5, 7, 8, 9, 2, 3, 11}, arcs),
              ElementsAre(0, 9, 2, 5, 8, 7, 3, 11));

  ASSERT_TRUE(StableTopologicalSort(
      /*nodes=*/{0, 5, 7, 8, 9, 3, 2, 11}, arcs, &order));
  ASSERT_THAT(order, ElementsAre(0, 9, 3, 2, 5, 8, 7, 11));
  ASSERT_THAT(StableTopologicalSortOrDie(
                  /*nodes=*/{0, 5, 7, 8, 9, 3, 2, 11}, arcs),
              ElementsAre(0, 9, 3, 2, 5, 8, 7, 11));
}

TEST(SimpleTopologicalSortTest, DenseIntTopologicalSortMediumExample) {
  std::vector<std::pair<int, int>> arcs({{6, 5}, {2, 5}, {5, 3}, {3, 1}});
  std::vector<int> order;
  ASSERT_TRUE(DenseIntTopologicalSort(
      /*num_nodes=*/9, arcs, &order));
  ASSERT_EQ(ValidateTopologicalOrder(/*init_order=*/{0, 1, 2, 3, 4, 5, 6, 7, 8},
                                     arcs, order),
            "");
  ASSERT_EQ(
      ValidateTopologicalOrder(/*init_order=*/{0, 1, 2, 3, 4, 5, 6, 7, 8}, arcs,
                               DenseIntTopologicalSortOrDie(
                                   /*num_nodes=*/9, arcs)),
      "");
}

TEST(SimpleTopologicalSortTest, DenseIntStableTopologicalSortMediumExample) {
  std::vector<int> order;
  ASSERT_TRUE(DenseIntStableTopologicalSort(
      /*num_nodes=*/9, /*arcs=*/{{6, 5}, {2, 5}, {5, 3}, {3, 1}}, &order));
  ASSERT_THAT(order, ElementsAre(0, 2, 4, 6, 5, 3, 1, 7, 8));
  ASSERT_THAT(DenseIntStableTopologicalSortOrDie(
                  /*num_nodes=*/9, /*arcs=*/{{6, 5}, {2, 5}, {5, 3}, {3, 1}}),
              ElementsAre(0, 2, 4, 6, 5, 3, 1, 7, 8));
}

TEST(SimpleTopologicalSortTest, DenseIntTopologicalSortCyclicInput) {
  int num_nodes = 3;
  std::vector<std::pair<int, int>> arcs({{1, 2}, {2, 1}});
  std::vector<int> order;
  ASSERT_FALSE(DenseIntTopologicalSort(num_nodes, arcs, &order));
}

TEST(SimpleTopologicalSortTest, DenseIntTopologicalSortEmptyInput) {
  std::vector<int> topo_order;
  ASSERT_TRUE(DenseIntTopologicalSort(0, {}, &topo_order));
  EXPECT_THAT(topo_order, ElementsAre());
}

TEST(DenseIntTopologicalSortTest, RandomizedStressTest) {
  // This test generates many small graphs, so that we hopefully cover all
  // corner cases.
  constexpr int kNumTests = DEBUG_MODE ? 100'000 : 200'000;
  absl::BitGen random;
  std::vector<int> topo_order;
  std::vector<int> nodes;
  for (int t = 0; t < kNumTests; ++t) {
    const int num_nodes = absl::LogUniform(random, 2, 16);
    const int num_edges = absl::LogUniform(random, 1, 32);
    std::vector<std::pair<int, int>> edges =
        CreateRandomDAG(random, num_nodes, num_edges, true);
    ASSERT_TRUE(DenseIntTopologicalSort(num_nodes, edges, &topo_order));
    nodes.assign(num_nodes, 0);
    std::iota(nodes.begin(), nodes.end(), 0);
    ASSERT_EQ(ValidateTopologicalOrder(/*init_order=*/nodes, edges, topo_order),
              "");
  }
}

TEST(DenseIntTopologicalSortTest, SelfArc) {
  std::vector<int> topo_order;
  EXPECT_FALSE(DenseIntTopologicalSort(1, {{0, 0}}, &topo_order));
}

TEST(FastTopologicalSortTest, RandomizedStressTest) {
  constexpr int kNumTests = DEBUG_MODE ? 100'000 : 200'000;
  absl::BitGen random;
  for (int t = 0; t < kNumTests; ++t) {
    const int num_nodes = absl::LogUniform(random, 2, 16);
    const int num_edges = absl::LogUniform(random, 1, 32);
    const std::vector<int> ref_topo_order =
        CreateRandomDenseIntArray(random, num_nodes);

    // Draw the out-degrees first. The 'last' node in the topological order
    // can't have outgoing edges: we fix that here by moving its out-degree to
    // another node.
    std::vector<int> out_degree =
        operations_research::RandomSplit(num_edges, num_nodes, random);
    out_degree[ref_topo_order[absl::Uniform(random, 0, num_nodes - 1)]] +=
        out_degree[ref_topo_order.back()];
    out_degree[ref_topo_order.back()] = 0;

    // Build the adjacency lists, respecting the reference topo order.
    std::vector<std::vector<int>> adj(num_nodes);
    for (int i = 0; i < num_nodes; ++i) {
      const int from = ref_topo_order[i];
      adj[from].assign(out_degree[from], 0);
      for (int& neigh : adj[from]) {
        neigh = ref_topo_order[absl::Uniform(random, i + 1, num_nodes)];
      }
    }

    // Run FastTopologicalSort() and verify its validity.
    ASSERT_OK_AND_ASSIGN(std::vector<int> topo_order,
                         util::graph::FastTopologicalSort(adj));

    // Verify the basic validity of the output, i.e. it's a permutation of the
    // node indices.
    ASSERT_EQ(topo_order.size(), num_nodes);
    std::vector<int> index(num_nodes, -1);
    for (int i = 0; i < num_nodes; ++i) {
      ASSERT_GE(topo_order[i], 0);
      ASSERT_LT(topo_order[i], num_nodes);
      ASSERT_EQ(index[topo_order[i]], -1) << "Duplicate: " << topo_order[i];
      index[topo_order[i]] = i;
    }
    // Verify that it's a topological order by checking all arcs.
    for (int from = 0; from < num_nodes; ++from) {
      for (int to : adj[from]) {
        ASSERT_GE(index[to], index[from]);
      }
    }
  }
}

TEST(FastTopologicalSortTest, NegativeHeadInAdj) {
  EXPECT_THAT(
      util::graph::FastTopologicalSort(std::vector<std::vector<int>>{{-1}})
          .status(),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid arc"), HasSubstr("-1"))));
}

TEST(FastTopologicalSortTest, ArcHeadOutOfBounds) {
  EXPECT_THAT(
      util::graph::FastTopologicalSort(std::vector<std::vector<int>>{{1}})
          .status(),
      StatusIs(absl::StatusCode::kInvalidArgument,
               AllOf(HasSubstr("Invalid arc"), HasSubstr("1"))));
}

TEST(FastTopologicalSortTest, SelfArc) {
  EXPECT_THAT(
      util::graph::FastTopologicalSort(std::vector<std::vector<int>>{{0}})
          .status(),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("has a cycle")));
}

TEST(FastTopologicalSortTest, Cycle) {
  EXPECT_THAT(
      util::graph::FastTopologicalSort(std::vector<std::vector<int>>{{1}, {0}})
          .status(),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("has a cycle")));
}

TEST(FastTopologicalSortTest, TieBreakingOrderIsLifo) {
  // This is the graph: all arcs point downwards.
  //
  // [7]  [5]  [6]     We expect 5, 6 and 7 to be enqueued, in this order, at
  //  |.--´|    |      the start.
  // [3]  [4]  [2]     Then it should be 4, then 2, then 3.
  //     .´|`..´|      Beyond the initial fringe, it's the order inside the
  //   .´  |.´`.|      adjacency lists that matters, not the node indices. This
  // [1]  [0]  [8]     makes the last nodes be 1, then 8, then 0, in this order
  //                   because 8 appears first in 2's adjacency list, which is
  //                   the 'unblocking' parent (4 has been popped first).
  //
  // So the expected order is: 5, 6, 7, 4, 2, 3, 1, 8, 0.
  const std::vector<std::vector<int>> adj = {{},     {},  {8, 0}, {}, {0, 1, 8},
                                             {3, 4}, {2}, {3},    {}};
  EXPECT_THAT(util::graph::FastTopologicalSort(adj),
              IsOkAndHolds(ElementsAre(5, 6, 7, 4, 2, 3, 1, 8, 0)));
}

TEST(FastTopologicalSortTest, TieBreakingOrderGoldenTest) {
  // The TieBreakingOrderIsLifo unit test above is still readable, but it
  // doesn't strongly enforce that the order is stable. This unit test does the
  // opposite: it creates a large graph with many possible topo orders, on
  // purpose, and verifies that the overall output order is the golden one.
  // It's not readable, but a failure of this test indicates that clients who
  // rely on the order in case of ties might break.
  constexpr int kNumNodes = 1'000'000;
  constexpr int kNumArcs = 3'000'000;
  // Each node has a given level, and arcs only go to a strictly increasing
  // level. The multiplicity of node per level ensure an exponentially large
  // number of valid topo orders.
  constexpr int kNumLevels = 1000;

  std::vector<std::vector<int>> nodes_per_level(kNumLevels);
  std::mt19937 random;  // Deterministic, platform-independent, never changes.
  // NOTE(user): Below I used random() % N instead of absl::Uniform(...) to
  // avoid depending on the implementation of absl::Uniform(), which could
  // change, whereas mt19937 won't.
  auto random_element = [&random](absl::Span<const int> v) {
    return v[random() % v.size()];
  };
  for (int node = 0; node < kNumNodes; ++node) {
    nodes_per_level[random() % kNumLevels].push_back(node);
  }
  std::vector<std::vector<int>> adj(kNumNodes);
  for (int a = 0; a < kNumArcs; ++a) {
    int level_from = random() % kNumLevels;
    int level_to = random() % kNumLevels;
    if (level_from > level_to) {
      std::swap(level_from, level_to);
    } else if (level_from == level_to) {
      if (level_from == 0) {
        ++level_to;
      } else {
        --level_from;
      }
    }
    DCHECK_LT(level_from, level_to);
    adj[random_element(nodes_per_level[level_from])].push_back(
        random_element(nodes_per_level[level_to]));
  }

  ASSERT_OK_AND_ASSIGN(const std::vector<int> topo_order,
                       util::graph::FastTopologicalSort(adj));
  const auto mix = [](uint64_t h, uint64_t v) -> uint64_t {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
  };
  uint64_t fprint = 0;
  for (uint64_t node : topo_order) {
    fprint = mix(fprint, node);
  }
  EXPECT_EQ(fprint, 18312497875112251437ULL);
}

TEST(SimpleTopologicalSortTest, StrongIndexType) {
  util::ListGraph<StrongNodeId, StrongArcId> graph;
  graph.AddArc(StrongNodeId(0), StrongNodeId(1));
  graph.AddArc(StrongNodeId(1), StrongNodeId(2));
  ASSERT_OK_AND_ASSIGN(const std::vector<StrongNodeId> order,
                       util::graph::FastTopologicalSort(graph));
  EXPECT_THAT(order,
              ElementsAre(StrongNodeId(0), StrongNodeId(1), StrongNodeId(2)));
}

TEST(SimpleTopologicalSortTest, FindCycleStrongIndexType) {
  util::ListGraph<StrongNodeId, StrongArcId> graph;
  graph.AddArc(StrongNodeId(0), StrongNodeId(1));
  graph.AddArc(StrongNodeId(1), StrongNodeId(0));
  ASSERT_OK_AND_ASSIGN(const std::vector<StrongNodeId> cycle,
                       util::graph::FindCycleInGraph(graph));
  EXPECT_THAT(cycle, UnorderedElementsAre(StrongNodeId(0), StrongNodeId(1)));
}

TEST(SimpleTopologicalSortTest, EmptyInput) {
  ASSERT_OK_AND_ASSIGN(
      const std::vector<int> order,
      util::graph::FastTopologicalSort(std::vector<std::vector<int>>{}));
  EXPECT_THAT(order, ElementsAre());
}

TEST(SimpleTopologicalSortDeathTest, DenseIntTopologicalSortOrDieCyclicInput) {
  int num_nodes = 3;
  std::vector<std::pair<int, int>> arcs({{1, 2}, {2, 1}});
  EXPECT_DEATH(
      { DenseIntTopologicalSortOrDie(num_nodes, arcs); }, "Found cycle:");
}

TEST(SimpleTopologicalSortDeathTest, DenseIntStableTopologicalSortCyclicInput) {
  int num_nodes = 3;
  std::vector<std::pair<int, int>> arcs({{1, 2}, {2, 1}});
  std::vector<int> order;
  ASSERT_FALSE(DenseIntStableTopologicalSort(num_nodes, arcs, &order));
}

TEST(SimpleTopologicalSortDeathTest,
     DenseIntStableTopologicalSortOrDieCyclicInput) {
  int num_nodes = 3;
  std::vector<std::pair<int, int>> arcs({{1, 2}, {2, 1}});
  EXPECT_DEATH(
      { DenseIntStableTopologicalSortOrDie(num_nodes, arcs); }, "Found cycle:");
}

TEST(SimpleTopologicalSortDeathTest, TopologicalSortCyclicInput) {
  std::vector<std::string> nodes({"a", "b", "c"});
  std::vector<std::pair<std::string, std::string>> arcs(
      {{"b", "c"}, {"c", "b"}});
  std::vector<std::string> order;
  ASSERT_FALSE(TopologicalSort(nodes, arcs, &order));
}

TEST(SimpleTopologicalSortDeathTest, TopologicalSortOrDieCyclicInput) {
  std::vector<std::string> nodes({"a", "b", "c"});
  std::vector<std::pair<std::string, std::string>> arcs(
      {{"b", "c"}, {"c", "b"}});
  EXPECT_DEATH({ TopologicalSortOrDie(nodes, arcs); }, "Found cycle:");
}

TEST(SimpleTopologicalSortDeathTest, StableTopologicalSortCyclicInput) {
  std::vector<std::string> nodes({"a", "b", "c"});
  std::vector<std::pair<std::string, std::string>> arcs(
      {{"b", "c"}, {"c", "b"}});
  std::vector<std::string> order;
  ASSERT_FALSE(StableTopologicalSort(nodes, arcs, &order));
}

TEST(SimpleTopologicalSortDeathTest, StableTopologicalSortOrDieCyclicInput) {
  std::vector<std::string> nodes({"a", "b", "c"});
  std::vector<std::pair<std::string, std::string>> arcs(
      {{"b", "c"}, {"c", "b"}});
  EXPECT_DEATH({ StableTopologicalSortOrDie(nodes, arcs); }, "Found cycle:");
}

}  // namespace
}  // namespace util
