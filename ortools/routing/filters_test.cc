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

#include "ortools/routing/filters.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/functional/bind_front.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/span.h"
#include "google/protobuf/duration.pb.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"
#include "ortools/base/log_severity.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/local_search.h"
#include "ortools/constraint_solver/search_stats.pb.h"
#include "ortools/routing/enums.pb.h"
#include "ortools/routing/filter_committables.h"
#include "ortools/routing/filter_test_utils.h"
#include "ortools/routing/index_manager.h"
#include "ortools/routing/parameters.h"
#include "ortools/routing/parameters.pb.h"
#include "ortools/routing/path_state.h"
#include "ortools/routing/routing.h"
#include "ortools/routing/routing_test_utils.h"
#include "ortools/routing/types.h"
#include "ortools/util/optional_boolean.pb.h"
#include "ortools/util/piecewise_linear_function.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/sorted_interval_list.h"
#include "ortools/util/testing_utils.h"

namespace operations_research::routing {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;

namespace {

using Attributes = Model::ResourceGroup::Attributes;
using VisitTypePolicy = Model::VisitTypePolicy;

TEST(PathState, ConstantAccessorsAreCorrect1) {
  constexpr int kNumNodes = 5;
  const PathState state(kNumNodes, {1, 2}, {3, 4});
  EXPECT_EQ(kNumNodes, state.NumNodes());
  EXPECT_EQ(2, state.NumPaths());
  EXPECT_EQ(state.Start(0), 1);
  EXPECT_EQ(state.Start(1), 2);
  EXPECT_EQ(state.End(0), 3);
  EXPECT_EQ(state.End(1), 4);
}

TEST(PathState, ConstantAccessorsAreCorrect2) {
  constexpr int kNumNodes = 7;
  const PathState state(kNumNodes, {1, 2, 0}, {3, 4, 6});
  EXPECT_EQ(kNumNodes, state.NumNodes());
  EXPECT_EQ(3, state.NumPaths());
  EXPECT_EQ(state.Start(0), 1);
  EXPECT_EQ(state.Start(1), 2);
  EXPECT_EQ(state.Start(2), 0);
  EXPECT_EQ(state.End(0), 3);
  EXPECT_EQ(state.End(1), 4);
  EXPECT_EQ(state.End(2), 6);
}

TEST(PathState, ConstantAccessorsAreCorrect3) {
  constexpr int kNumNodes = 0;
  const PathState state(kNumNodes, {}, {});
  EXPECT_EQ(kNumNodes, state.NumNodes());
  EXPECT_EQ(0, state.NumPaths());
}

void PathStatePathsAndNewLoopsEquals(
    const PathState* state, absl::Span<const std::vector<int>> expected_paths,
    const std::set<int>& expected_loops) {
  ASSERT_EQ(state->NumPaths(), expected_paths.size());
  // Test Chains() iterators.
  for (int p = 0; p < expected_paths.size(); ++p) {
    int i = 0;
    for (const auto chain : state->Chains(p)) {
      int expected_committed_index = state->CommittedIndex(chain.First());
      for (const auto node : chain) {
        EXPECT_EQ(node, expected_paths[p][i])
            << "in path " << p << " at index " << i;
        EXPECT_EQ(state->CommittedIndex(node), expected_committed_index);
        ++i;
        ++expected_committed_index;
      }
    }
    EXPECT_EQ(i, expected_paths[p].size()) << "in path " << p;
  }
  // Test Nodes() iterators.
  for (int p = 0; p < expected_paths.size(); ++p) {
    const auto [begin_index, end_index] = state->CommittedPathRange(p);
    EXPECT_EQ(begin_index, state->CommittedIndex(state->Start(p)));
    EXPECT_EQ(end_index - 1, state->CommittedIndex(state->End(p)));
    EXPECT_LT(begin_index, end_index - 1);
    int i = 0;
    for (const int node : state->Nodes(p)) {
      EXPECT_EQ(node, expected_paths[p][i])
          << "in path " << p << " at index " << i;
      ++i;
    }
    EXPECT_EQ(i, expected_paths[p].size()) << "in path " << p;
  }
  // Test ChangedLoops().
  EXPECT_EQ(
      std::set<int>(state->ChangedLoops().begin(), state->ChangedLoops().end()),
      expected_loops);
}

void PathStateChangePaths(PathState* state,
                          absl::Span<const std::vector<int>> paths,
                          absl::Span<const int> loops) {
  const int num_nodes = state->NumNodes();
  std::vector<bool> node_is_in_paths(num_nodes, false);
  std::vector<PathState::ChainBounds> path_chains;
  for (int p = 0; p < paths.size(); ++p) {
    // Make chains out of path, merging nodes with consecutive indices.
    path_chains.clear();
    int prev_end_index = -1;
    for (const int node : paths[p]) {
      const int index = state->CommittedIndex(node);
      if (index == prev_end_index) {
        path_chains.back().end_index = index + 1;
      } else {
        path_chains.emplace_back(index, index + 1);
      }
      prev_end_index = (state->Path(node) < 0) ? -1 : (index + 1);
    }
    // Only change path if there is an actual change.
    if (path_chains.size() > 1) state->ChangePath(p, path_chains);
    for (const int node : paths[p]) node_is_in_paths[node] = true;
  }
  for (const int loop : loops) {
    EXPECT_FALSE(node_is_in_paths[loop]) << "Loop " << loop << " is in paths.";
  }
  state->ChangeLoops(loops);
}

// New loops must contain all nodes in old_paths that are not in new_paths,
// and may contain unassigned nodes or nodes in neither paths.
std::unique_ptr<Assignment> MakeDeltaFromPaths(
    Solver* solver, const std::vector<IntVar*>& nexts,
    absl::Span<const std::vector<int>> old_paths,
    absl::Span<const std::vector<int>> new_paths,
    absl::Span<const int> new_loops) {
  const int num_nodes = nexts.size();
  std::vector<bool> is_new_loop(num_nodes, false);
  const int num_paths = old_paths.size();
  CHECK_EQ(num_paths, new_paths.size());
  // Find next-based representation of old_paths.
  std::vector<int> old_next_values(num_nodes);
  std::iota(old_next_values.begin(), old_next_values.end(), 0);
  for (int p = 0; p < num_paths; ++p) {
    for (int i = 1; i < old_paths[p].size(); ++i) {
      const int from = old_paths[p][i - 1];
      const int to = old_paths[p][i];
      is_new_loop[from] = true;
      old_next_values[from] = to;
    }
  }
  // Fill delta with next-based difference from old_paths to new_paths.
  auto delta = std::make_unique<Assignment>(solver);
  for (int p = 0; p < num_paths; ++p) {
    for (int i = 1; i < new_paths[p].size(); ++i) {
      const int from = new_paths[p][i - 1];
      const int to = new_paths[p][i];
      is_new_loop[from] = false;
      if (old_next_values[from] != to) {
        IntVarElement* element = delta->FastAdd(nexts[from]);
        element->SetValue(to);
        element->Activate();
      }
    }
  }
  // Add new loops.
  std::vector<bool> node_is_in_new_loops(num_nodes, false);
  for (const int loop : new_loops) {
    node_is_in_new_loops[loop] = true;
    IntVarElement* element = delta->FastAdd(nexts[loop]);
    element->SetValue(loop);
    element->Activate();
  }
  // is_new_loop[node] implies node_in_new_loops[node].
  for (int node = 0; node < num_nodes; ++node) {
    EXPECT_TRUE(!is_new_loop[node] || node_is_in_new_loops[node]);
  }
  return delta;
}

TEST(PathState, ChangeIterators) {
  PathState state(3, {0}, {1});
  PathStateChangePaths(&state, {{0, 2, 1}}, {});
  PathStatePathsAndNewLoopsEquals(&state, {{0, 2, 1}}, {});
  state.Commit();
  PathStateChangePaths(&state, {{0, 1}}, {2});
  PathStatePathsAndNewLoopsEquals(&state, {{0, 1}}, {2});
}

TEST(PathState, ChangeIteratorsWithUnassignedNodes) {
  PathState state(5, {0}, {1});
  PathStateChangePaths(&state, {{0, 2, 1}}, {3});
  PathStatePathsAndNewLoopsEquals(&state, {{0, 2, 1}}, {3});
  state.Commit();
  PathStateChangePaths(&state, {{0, 1}}, {2});
  PathStatePathsAndNewLoopsEquals(&state, {{0, 1}}, {2});
  state.Commit();
  PathStateChangePaths(&state, {{0, 2, 1}}, {4});
  PathStatePathsAndNewLoopsEquals(&state, {{0, 2, 1}}, {4});
}

TEST(PathState, AccessorsInitialState) {
  const std::vector<int> starts{0, 5, 2};
  const std::vector<int> ends{1, 4, 3};
  PathState state(10, starts, ends);
  // There should be exactly one chain per path, starts[path]..ends[path].
  for (int path = 0; path < 3; ++path) {
    for (const auto chain : state.Chains(path)) {
      EXPECT_EQ(chain.First(), starts[path]);
      EXPECT_EQ(chain.Last(), ends[path]);
    }
  }
}

TEST(PathState, AccessorsRunningState) {
  const std::vector<int> starts{0, 5, 2};
  const std::vector<int> ends{1, 4, 3};
  PathState state(10, starts, ends);
  // Iterate this many times, to exercise FullCommit() and Reset().
  for (int iter = 0; iter < 100; ++iter) {
    {
      PathStateChangePaths(&state, {{0, 8, 1}, {5, 7, 6, 4}, {2, 3}}, {});
      const std::vector<std::vector<int>> expected_new_chains{
          {0}, {8}, {1}, {5}, {7}, {6}, {4}, {2, 3}};
      int num_chain = 0;
      for (int path = 0; path < 3; ++path) {
        for (const auto chain : state.Chains(path)) {
          EXPECT_EQ(chain.First(), expected_new_chains[num_chain].front());
          EXPECT_EQ(chain.Last(), expected_new_chains[num_chain].back());
          ++num_chain;
        }
      }
      EXPECT_TRUE(state.ChangedLoops().empty());
      std::vector<int> expected_paths{0, 0, 2, 2, 1, 1};
      expected_paths.resize(10, PathState::kUnassigned);
      for (int node = 0; node < 10; ++node) {
        EXPECT_EQ(state.Path(node), expected_paths[node]);
      }
    }
    {
      state.Commit();
      // There should be exactly one chain per path, starts[path]..ends[path].
      for (int path = 0; path < 3; ++path) {
        for (const auto chain : state.Chains(path)) {
          EXPECT_EQ(chain.First(), starts[path]);
          EXPECT_EQ(chain.Last(), ends[path]);
        }
      }
      EXPECT_TRUE(state.ChangedLoops().empty());
      const std::vector<int> expected_paths{0, 0, 2, 2, 1,
                                            1, 1, 1, 0, PathState::kUnassigned};
      for (int node = 0; node < 10; ++node) {
        EXPECT_EQ(state.Path(node), expected_paths[node]);
      }
    }
    {
      // Swap 8 with 7->6.
      const std::vector<std::vector<int>> new_paths{
          {0, 7, 6, 1}, {5, 8, 4}, {2, 3}};
      PathStateChangePaths(&state, new_paths, {9});
      const std::vector<std::vector<int>> expected_new_chains{
          {0}, {7, 6}, {1}, {5}, {8}, {4}, {2, 3}};
      int num_chain = 0;
      for (int path = 0; path < 3; ++path) {
        for (const auto chain : state.Chains(path)) {
          EXPECT_EQ(chain.First(), expected_new_chains[num_chain].front());
          EXPECT_EQ(chain.Last(), expected_new_chains[num_chain].back());
          ++num_chain;
        }
      }
      ASSERT_EQ(state.ChangedLoops().size(), 1);
      EXPECT_EQ(state.ChangedLoops()[0], 9);
      // Same expected paths as before change.
      const std::vector<int> expected_paths{0, 0, 2, 2, 1,
                                            1, 1, 1, 0, PathState::kUnassigned};
      for (int node = 0; node < 10; ++node) {
        EXPECT_EQ(state.Path(node), expected_paths[node]);
      }
    }
    {
      state.Commit();
      // There should be exactly one chain per path, starts[path]..ends[path].
      for (int path = 0; path < 3; ++path) {
        for (const auto chain : state.Chains(path)) {
          EXPECT_EQ(chain.First(), starts[path]);
          EXPECT_EQ(chain.Last(), ends[path]);
        }
      }
      EXPECT_TRUE(state.ChangedLoops().empty());
      const std::vector<int> expected_paths{0, 0, 2, 2, 1,
                                            1, 0, 0, 1, PathState::kLoop};
      for (int node = 0; node < 10; ++node) {
        EXPECT_EQ(state.Path(node), expected_paths[node]);
      }
    }
    state.Reset();
  }
}

TEST(PathState, ChainRangeDropElements) {
  using Chain = PathState::Chain;
  using ChainRange = PathState::ChainRange;
  PathState state(6, {5}, {0});
  PathStateChangePaths(&state, {{5, 4, 3, 2, 1, 0}}, {});
  EXPECT_EQ(state.ChangedPaths().size(), 1);
  ChainRange range = state.Chains(0);
  {
    int expected_value = 3;
    for (const Chain chain : range.DropFirstChain().DropFirstChain()) {
      EXPECT_EQ(chain.First(), expected_value);
      --expected_value;
    }
    EXPECT_EQ(expected_value, -1);
  }
  {
    int expected_value = 5;
    for (const Chain chain : range.DropLastChain().DropLastChain()) {
      EXPECT_EQ(chain.First(), expected_value);
      --expected_value;
    }
    EXPECT_EQ(expected_value, 1);
  }
  ChainRange empty = range.DropLastChain()
                         .DropLastChain()
                         .DropLastChain()
                         .DropFirstChain()
                         .DropFirstChain()
                         .DropFirstChain();
  EXPECT_FALSE(empty.begin() != empty.end());
  for ([[maybe_unused]] const Chain chain : empty) {
    ADD_FAILURE() << "Chain range is supposed to be empty.";
  }
  {
    // DropLastChain() should not change the range.
    ChainRange empty2 = empty.DropLastChain();
    EXPECT_FALSE(empty2.begin() != empty2.end());
    for ([[maybe_unused]] const Chain chain : empty2) {
      ADD_FAILURE() << "Chain range is supposed to be empty.";
    }
  }
  {
    // DropFirstChain() should not change the range.
    ChainRange empty3 = empty.DropFirstChain();
    EXPECT_FALSE(empty3.begin() != empty3.end());
    for ([[maybe_unused]] const Chain chain : empty3) {
      ADD_FAILURE() << "Chain range is supposed to be empty.";
    }
  }
}

void PathStateNumChainsVectorEquals(const PathState* state,
                                    absl::Span<const int> num_chains) {
  for (int path = 0; path < num_chains.size(); ++path) {
    int chain_count = 0;
    for (const auto chain [[maybe_unused]] : state->Chains(path)) ++chain_count;
    EXPECT_EQ(chain_count, num_chains[path]) << "in path " << path;
  }
}

// Note: changed nodes are the tails of changed arcs.
void CheckPathStateChangedNodesEqualsSet(
    const PathState* state, absl::Span<const int> expected_changed_nodes) {
  // Make set of expected changed nodes.
  const std::set<int> expected_changed_nodes_set(expected_changed_nodes.begin(),
                                                 expected_changed_nodes.end());
  ASSERT_EQ(expected_changed_nodes.size(), expected_changed_nodes_set.size());
  // Generate set of state's changed nodes, compare to expected.
  std::vector<int> changed_nodes;
  for (const int path : state->ChangedPaths()) {
    const int path_end = state->End(path);
    for (const auto& chain : state->Chains(path)) {
      if (chain.Last() == path_end) continue;
      changed_nodes.push_back(chain.Last());
    }
  }
  changed_nodes.insert(changed_nodes.end(), state->ChangedLoops().begin(),
                       state->ChangedLoops().end());
  EXPECT_EQ(changed_nodes.size(), expected_changed_nodes.size());
  EXPECT_EQ(std::set<int>(changed_nodes.begin(), changed_nodes.end()),
            expected_changed_nodes_set);
}

void CheckPathStateChangedPathsEqualsSet(
    const PathState* state, absl::Span<const int> expected_changed_paths) {
  // Make set of expected changed paths.
  const std::set<int> expected_changed_paths_set(expected_changed_paths.begin(),
                                                 expected_changed_paths.end());
  ASSERT_EQ(expected_changed_paths.size(), expected_changed_paths_set.size());
  // Generate set of state's changed paths, compare to expected.
  const int num_changed_paths =
      std::distance(state->ChangedPaths().begin(), state->ChangedPaths().end());
  EXPECT_EQ(num_changed_paths, expected_changed_paths_set.size());
  std::set<int> changed{state->ChangedPaths().begin(),
                        state->ChangedPaths().end()};
  EXPECT_EQ(changed, expected_changed_paths_set);
}

// Tests the difference between old (current) paths and new paths by
// changing the state, reverting, changing, and committing.
// In between operations, it checks that the PathState has expected
// paths, number of chains, changed set of nodes and paths.
void PathStateChangeRevertChangeCommit(
    PathState* const state, LocalSearchFilter* filter,
    const std::vector<IntVar*>* nexts,
    absl::Span<const std::vector<int>> old_paths,
    absl::Span<const std::vector<int>> new_paths,
    const std::set<int>& new_loops,  // Loops in new that are not in old.
    absl::Span<const int> expected_num_chains,
    absl::Span<const int> expected_changed_nodes,
    absl::Span<const int> expected_changed_paths) {
  std::unique_ptr<Assignment> delta = nullptr;
  const std::vector<int> loops{new_loops.begin(), new_loops.end()};
  {
    // Test difference between old and new paths.
    if (filter == nullptr) {
      PathStateChangePaths(state, new_paths, loops);
    } else {
      delta = MakeDeltaFromPaths((*nexts)[0]->solver(), *nexts, old_paths,
                                 new_paths, loops);
      filter->Relax(delta.get(), nullptr);
    }
    SCOPED_TRACE("After changing paths.");
    PathStatePathsAndNewLoopsEquals(state, new_paths, new_loops);
    PathStateNumChainsVectorEquals(state, expected_num_chains);
    CheckPathStateChangedNodesEqualsSet(state, expected_changed_nodes);
    CheckPathStateChangedPathsEqualsSet(state, expected_changed_paths);
  }
  {
    // Revert, test that we got old_paths back and cleared changes.
    if (filter == nullptr) {
      state->Revert();
    } else {
      filter->Revert();
    }
    SCOPED_TRACE("After reverting paths.");
    PathStatePathsAndNewLoopsEquals(state, old_paths, {});
    PathStateNumChainsVectorEquals(state, {1, 1, 1, 1});
    CheckPathStateChangedNodesEqualsSet(state, {});
    CheckPathStateChangedPathsEqualsSet(state, {});
  }
  {
    // Change again.
    if (filter == nullptr) {
      PathStateChangePaths(state, new_paths, loops);
    } else {
      filter->Relax(delta.get(), nullptr);
    }
    SCOPED_TRACE("After changing paths from revert.");
    PathStatePathsAndNewLoopsEquals(state, new_paths, new_loops);
    PathStateNumChainsVectorEquals(state, expected_num_chains);
    CheckPathStateChangedNodesEqualsSet(state, expected_changed_nodes);
    CheckPathStateChangedPathsEqualsSet(state, expected_changed_paths);
  }
  {
    // Commit, test that we have new paths and cleared changes.
    if (filter == nullptr) {
      state->Commit();
    } else {
      filter->Commit(nullptr, delta.get());
    }
    SCOPED_TRACE("After committing paths.");
    PathStatePathsAndNewLoopsEquals(state, new_paths, {});
    PathStateNumChainsVectorEquals(state, {1, 1, 1, 1});
    CheckPathStateChangedNodesEqualsSet(state, {});
    CheckPathStateChangedPathsEqualsSet(state, {});
  }
}

// Concatenate vector with itself k-1 times, k >= 1,
// and add i*offset to copy i (i from 0 to k-1).
std::vector<int> DuplicateAndOffsetVector(absl::Span<const int> old_vector,
                                          int num_copies, int offset) {
  CHECK_GE(num_copies, 1);
  std::vector<int> new_vector;
  const int num_values = old_vector.size();
  new_vector.resize(num_values * num_copies);
  for (int i = 0; i < num_copies; ++i) {
    for (int j = 0; j < num_values; ++j) {
      new_vector[i * num_values + j] = old_vector[j] + i * offset;
    }
  }
  return new_vector;
}

// Concatenates a vector of vectors with itself k-1 times, k >= 1,
// and add i*offset to values inside vectors of copy i (i from 0 to k-1).
std::vector<std::vector<int>> DuplicateAndOffsetVectors(
    absl::Span<const std::vector<int>> old_vectors, int num_copies,
    int offset) {
  CHECK_GE(num_copies, 1);
  std::vector<std::vector<int>> new_vectors;
  const int num_vectors = old_vectors.size();
  new_vectors.resize(num_vectors * num_copies);
  for (int i = 0; i < num_copies; ++i) {
    for (int j = 0; j < num_vectors; ++j) {
      new_vectors[i * num_vectors + j] = old_vectors[j];
      for (int& value : new_vectors[i * num_vectors + j]) {
        value += i * offset;
      }
    }
  }
  return new_vectors;
}

enum class FilterOrState { kChangeWithFilter, kChangeWithState };

using PathStateTest =
    ::testing::TestWithParam<::testing::tuple<int, FilterOrState>>;

// Test various consecutive operations. This looks like it could be cut into
// several tests, but one of the goals is to ensure that consecutive operations
// keep the state correct.
TEST_P(PathStateTest, VariousOperations) {
  // Make a PathState. Ownership might be given to a PathStateFilter.
  constexpr int kNumNodes = 18;
  const int kNumPaths = 4;
  const auto [kCopies, kFilterOrState] = GetParam();
  const std::vector<int> starts =
      DuplicateAndOffsetVector({1, 3, 5, 7}, kCopies, kNumNodes);
  const std::vector<int> ends =
      DuplicateAndOffsetVector({6, 8, 10, 12}, kCopies, kNumNodes);
  auto path_state_ownership =
      std::make_unique<PathState>(kNumNodes * kCopies, starts, ends);
  PathState* path_state = path_state_ownership.get();
  // Make a PathStateFilter.
  Solver solver("test");
  std::vector<IntVar*> nexts;
  solver.MakeIntVarArray(kNumNodes * kCopies, 0, kNumNodes * kCopies, &nexts);
  LocalSearchFilter* filter = nullptr;
  if (kFilterOrState == FilterOrState::kChangeWithFilter) {
    MakePathStateFilter(&solver, std::move(path_state_ownership), nexts);
  }

  std::vector<std::vector<int>> old_paths = DuplicateAndOffsetVectors(
      {{1, 6}, {3, 8}, {5, 10}, {7, 12}}, kCopies, kNumNodes);
  {
    SCOPED_TRACE("Initial empty paths state.");
    PathStatePathsAndNewLoopsEquals(path_state, old_paths, {});
    PathStateNumChainsVectorEquals(
        path_state, DuplicateAndOffsetVector({1, 1, 1, 1}, kCopies, 0));
    CheckPathStateChangedNodesEqualsSet(path_state, {});
    CheckPathStateChangedPathsEqualsSet(path_state, {});
  }

  std::vector<std::vector<int>> new_paths = old_paths;
  std::vector<int> num_chains;
  std::vector<int> changed_nodes;
  std::vector<int> changed_paths;
  {
    SCOPED_TRACE("Add some nodes to paths, to make a variety of changes.");
    old_paths = new_paths;
    new_paths = DuplicateAndOffsetVectors(
        {{1, 4, 6}, {3, 11, 8}, {5, 9, 14, 10}, {7, 12}}, kCopies, kNumNodes);
    num_chains = DuplicateAndOffsetVector({3, 3, 4, 1}, kCopies, 0);
    changed_nodes =
        DuplicateAndOffsetVector({1, 4, 3, 11, 5, 9, 14}, kCopies, kNumNodes);
    changed_paths = DuplicateAndOffsetVector({0, 1, 2}, kCopies, kNumPaths);
    PathStateChangeRevertChangeCommit(path_state, filter, &nexts, old_paths,
                                      new_paths, {}, num_chains, changed_nodes,
                                      changed_paths);
  }
  {
    SCOPED_TRACE("Insert nodes after start, before end, between other nodes");
    old_paths = new_paths;
    new_paths = DuplicateAndOffsetVectors(
        {{1, 0, 4, 6}, {3, 11, 13, 8}, {5, 9, 15, 14, 10}, {7, 16, 17, 12}},
        kCopies, kNumNodes);
    num_chains = DuplicateAndOffsetVector({3, 3, 3, 4}, kCopies, 0);
    changed_nodes = DuplicateAndOffsetVector({1, 0, 11, 13, 9, 15, 7, 16, 17},
                                             kCopies, kNumNodes);
    changed_paths = DuplicateAndOffsetVector({0, 1, 2, 3}, kCopies, kNumPaths);
    PathStateChangeRevertChangeCommit(path_state, filter, &nexts, old_paths,
                                      new_paths, {}, num_chains, changed_nodes,
                                      changed_paths);
  }
  {
    SCOPED_TRACE("Remove nodes after start, before end, between other nodes.");
    old_paths = new_paths;
    new_paths = DuplicateAndOffsetVectors(
        {{1, 0, 6}, {3, 13, 8}, {5, 9, 14, 10}, {7, 12}}, kCopies, kNumNodes);
    num_chains = DuplicateAndOffsetVector({2, 2, 2, 2}, kCopies, 0);
    changed_nodes = DuplicateAndOffsetVector({0, 4, 3, 11, 9, 15, 7, 16, 17},
                                             kCopies, kNumNodes);
    changed_paths = DuplicateAndOffsetVector({0, 1, 2, 3}, kCopies, kNumPaths);
    const std::vector<int> new_loops =
        DuplicateAndOffsetVector({4, 11, 15, 16, 17}, kCopies, kNumNodes);
    const std::set<int> new_loops_set(new_loops.begin(), new_loops.end());
    PathStateChangeRevertChangeCommit(path_state, filter, &nexts, old_paths,
                                      new_paths, new_loops_set, num_chains,
                                      changed_nodes, changed_paths);
  }
  {
    SCOPED_TRACE("Relocate nodes.");
    old_paths = new_paths;
    new_paths = DuplicateAndOffsetVectors(
        {{1, 13, 6}, {3, 0, 8}, {5, 10}, {7, 9, 14, 12}}, kCopies, kNumNodes);
    num_chains = DuplicateAndOffsetVector({3, 3, 2, 3}, kCopies, 0);
    changed_nodes =
        DuplicateAndOffsetVector({1, 13, 3, 0, 5, 7, 14}, kCopies, kNumNodes);
    changed_paths = DuplicateAndOffsetVector({0, 1, 2, 3}, kCopies, kNumPaths);
    PathStateChangeRevertChangeCommit(path_state, filter, &nexts, old_paths,
                                      new_paths, {}, num_chains, changed_nodes,
                                      changed_paths);
  }
  {
    SCOPED_TRACE("Relocate and swap nodes.");
    old_paths = new_paths;
    new_paths = DuplicateAndOffsetVectors(
        {{1, 6}, {3, 8}, {5, 0, 13, 10}, {7, 14, 9, 12}}, kCopies, kNumNodes);
    num_chains = DuplicateAndOffsetVector({2, 2, 4, 4}, kCopies, 0);
    changed_nodes = DuplicateAndOffsetVector({1, 3, 5, 0, 13, 7, 14, 9},
                                             kCopies, kNumNodes);
    changed_paths = DuplicateAndOffsetVector({0, 1, 2, 3}, kCopies, kNumPaths);
    PathStateChangeRevertChangeCommit(path_state, filter, &nexts, old_paths,
                                      new_paths, {}, num_chains, changed_nodes,
                                      changed_paths);
  }
}

INSTANTIATE_TEST_SUITE_P(
    PathStateTestInstance, PathStateTest,
    ::testing::Combine(::testing::Values(1, 2, 4),
                       ::testing::Values(FilterOrState::kChangeWithFilter,
                                         FilterOrState::kChangeWithState)));

std::vector<int> PathStateExtractPath(const PathState* path_state, int path) {
  std::vector<int> nodes;
  for (const auto chain : path_state->Chains(path)) {
    for (const int node : chain) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

// Tests that PathStateFilter translates Assignments and forwards them to
// PathState.
TEST(PathStateFilterTest, RelaxRevertCommit) {
  // Make a PathState.
  constexpr int kNumNodes = 15;
  constexpr int kNumPaths = 3;
  const std::vector<int> starts = {1, 2, 3};
  const std::vector<int> ends = {4, 5, 6};
  auto path_state_ownership =
      std::make_unique<PathState>(kNumNodes, starts, ends);
  PathState* path_state = path_state_ownership.get();
  // Make a PathStateFilter.
  Solver solver("test");
  std::vector<IntVar*> nexts;
  solver.MakeIntVarArray(kNumNodes, 0, kNumNodes + kNumPaths, &nexts);
  LocalSearchFilter* filter =
      MakePathStateFilter(&solver, std::move(path_state_ownership), nexts);
  // Make an assignment.
  Assignment* delta = solver.MakeAssignment();
  const std::vector<std::pair<int, int64_t>> next_values{
      {1, 0}, {0, 4}, {3, 9}, {9, 6}};
  for (const auto& [node, next_value] : next_values) {
    IntVarElement* element = delta->FastAdd(nexts[node]);
    element->Activate();
    element->SetValue(next_value);
  }
  // Pass assignment to filter with Relax(),
  // check that PathState got changed accordingly.
  filter->Relax(delta, nullptr);
  EXPECT_EQ(std::vector<int>({1, 0, 4}), PathStateExtractPath(path_state, 0));
  EXPECT_EQ(std::vector<int>({2, 5}), PathStateExtractPath(path_state, 1));
  EXPECT_EQ(std::vector<int>({3, 9, 6}), PathStateExtractPath(path_state, 2));
  const std::set<int> expected_changed_paths{0, 2};
  const std::set<int> actual_changed_paths(path_state->ChangedPaths().begin(),
                                           path_state->ChangedPaths().end());
  EXPECT_EQ(expected_changed_paths, actual_changed_paths);
  CheckPathStateChangedNodesEqualsSet(path_state, {1, 0, 3, 9});
  // Revert filter, check that PathState got reverted.
  filter->Revert();
  EXPECT_EQ(std::vector<int>({1, 4}), PathStateExtractPath(path_state, 0));
  EXPECT_EQ(std::vector<int>({2, 5}), PathStateExtractPath(path_state, 1));
  EXPECT_EQ(std::vector<int>({3, 6}), PathStateExtractPath(path_state, 2));
  EXPECT_FALSE(path_state->ChangedPaths().begin() !=
               path_state->ChangedPaths().end());
  EXPECT_FALSE(path_state->ChangedLoops().begin() !=
               path_state->ChangedLoops().end());
  // Commit filter, state must have new values and changeset must be empty.
  filter->Commit(delta, nullptr);
  EXPECT_EQ(std::vector<int>({1, 0, 4}), PathStateExtractPath(path_state, 0));
  EXPECT_EQ(std::vector<int>({2, 5}), PathStateExtractPath(path_state, 1));
  EXPECT_EQ(std::vector<int>({3, 9, 6}), PathStateExtractPath(path_state, 2));
  EXPECT_FALSE(path_state->ChangedPaths().begin() !=
               path_state->ChangedPaths().end());
  EXPECT_FALSE(path_state->ChangedLoops().begin() !=
               path_state->ChangedLoops().end());
}

// Test an infeasible case that needs backwards propagation on chains.
// Initial path:     0123456789
// Candidate path 1: 0612345789 chains are: 0, 6, 1->5, 7->9.
// Candidate path 2: 0124567839 chains are: 0->2, 4->8, 3, 9.
// Node 3 has capacity [2, 3], node 7 has [6, 7], others have (-inf, +inf).
// All transits have interval [0, +inf).
// Candidate is infeasible because node 3 must be before node 8.
// This case is not caught if backwards propagation is not performed when
// precomputing chain constraints, it is incorrectly deemed feasible.
// Then tests the forward propagation, and the parameter allows to do test
// with nonpositive transitions.

using DimensionCheckerChainPropagationWithSignTest =
    ::testing::TestWithParam<bool>;

TEST_P(DimensionCheckerChainPropagationWithSignTest,
       CheckBackwardsThenForwards) {
  using Interval = DimensionChecker::Interval;
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0}, {9});
  std::vector<Interval> node_capacities{kNumNodes, {kint64min, kint64max}};

  node_capacities[3] = {2, 3};
  node_capacities[6] = {6, 7};
  Interval transit{0, kint64max};
  if (GetParam()) {
    node_capacities[3] = {-3, -2};
    node_capacities[6] = {-7, -6};
    transit = {kint64min, 0};
  }

  DimensionChecker checker(&path_state, {{kint64min, kint64max}}, {0},
                           {[transit](int64_t, int64_t) { return transit; }},
                           node_capacities, 2);
  // Initial path is feasible, node 6 is after node 3.
  PathStateChangePaths(&path_state, {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  // Not feasible. Tests backwards propagation.
  PathStateChangePaths(&path_state, {{0, 6, 1, 2, 3, 4, 5, 7, 8, 9}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  // Not feasible. Tests forwards propagation.
  PathStateChangePaths(&path_state, {{0, 1, 2, 4, 5, 6, 7, 8, 3, 9}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
}

INSTANTIATE_TEST_SUITE_P(DimensionCheckerChainPropagationWithSignTestInstance,
                         DimensionCheckerChainPropagationWithSignTest,
                         ::testing::Bool());

using DimensionCheckerChainPropagationWithSignAndSizeTest =
    ::testing::TestWithParam<::testing::tuple<bool, int>>;

TEST_P(DimensionCheckerChainPropagationWithSignAndSizeTest,
       CheckBackwardsThenForwards) {
  using Interval = DimensionChecker::Interval;
  const auto [use_negative_transit, chain_size] = GetParam();
  const int num_nodes = 3 * chain_size;
  PathState state(num_nodes, {0}, {num_nodes - 1});
  std::vector<Interval> node_capacities(num_nodes, {kint64min, kint64max});

  // Using capacity windows and transits, node1 must be before node2.
  const int node1 = chain_size;
  const int node2 = chain_size * 2;

  node_capacities[node1] = {0, 0};
  node_capacities[node2] = {chain_size, chain_size};
  Interval transit{1, kint64max};
  if (use_negative_transit) {
    node_capacities[node2] = {-chain_size, -chain_size};
    transit = {kint64min, -1};
  }

  DimensionChecker checker(&state, {{kint64min, kint64max}}, {0},
                           {[transit](int64_t, int64_t) { return transit; }},
                           node_capacities, 2);
  // Initial path is feasible.
  std::vector<int> initial_path(num_nodes);
  std::iota(initial_path.begin(), initial_path.end(), 0);
  PathStateChangePaths(&state, {initial_path}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  state.Commit();
  const int index_start = state.CommittedIndex(0);
  const int index1 = state.CommittedIndex(node1);
  const int index2 = state.CommittedIndex(node2);
  const int index_end = state.CommittedIndex(num_nodes - 1);

  // Move chain [node1+1, node2) after node2.
  // There are no capacity window, this must be feasible.
  state.ChangePath(0, {{index_start, index1 + 1},
                       {index2, index2 + 1},
                       {index1 + 1, index2},
                       {index2 + 1, index_end + 1}});
  EXPECT_TRUE(checker.Check());
  state.Revert();

  // Move chain [node1+1, node2) before node1. Feasible.
  state.ChangePath(0, {{index_start, index1},
                       {index1 + 1, index2},
                       {index1, index1 + 1},
                       {index2, index_end + 1}});
  EXPECT_TRUE(checker.Check());
  state.Revert();

  // Move chain [node1, node2) after node2. Infeasible.
  state.ChangePath(0, {{index_start, index1},
                       {index2, index2 + 1},
                       {index1, index2},
                       {index2 + 1, index_end + 1}});
  EXPECT_FALSE(checker.Check());
  state.Revert();

  // Move chain [node1+1, node2+1) before node1. Feasible.
  state.ChangePath(0, {{index_start, index1},
                       {index1 + 1, index2 + 1},
                       {index1, index1 + 1},
                       {index2 + 1, index_end + 1}});
  EXPECT_FALSE(checker.Check());
  state.Revert();
}

INSTANTIATE_TEST_SUITE_P(
    DimensionCheckerChainPropagationWithSignAndSizeTestInstance,
    DimensionCheckerChainPropagationWithSignAndSizeTest,
    ::testing::Combine(::testing::Bool(), ::testing::Range(2, 16)));

using IntervalTransit =
    std::function<DimensionChecker::Interval(int64_t, int64_t)>;

IntervalTransit FixedIntervalTransit(std::vector<int64_t> values) {
  return [values = std::move(values)](int64_t node,
                                      int64_t) -> DimensionChecker::Interval {
    const int64_t value = values[node];
    return {value, value};
  };
}

IntervalTransit ConstantIntervalTransit(int64_t value) {
  return [value](int64_t, int64_t) -> DimensionChecker::Interval {
    return {value, value};
  };
}

using DimensionCheckerSignBoundAndLimitTest = ::testing::TestWithParam<
    ::testing::tuple<bool, DimensionChecker::Interval, int>>;

// Test feasible moves, with same-sign demands and same-class paths.
TEST_P(DimensionCheckerSignBoundAndLimitTest, FeasibleFixedMoves) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0, 6}, {5, 3});
  std::vector<int64_t> demands{3, 2, 2, 0, 0, 0, 3, 1, 1, 1};
  if (use_negative_demands) {
    for (int64_t& demand : demands) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 8}, {0, 8}}, {0, 0}, {FixedIntervalTransit(demands)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: 3, 3.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 8, 5.
  PathStateChangePaths(&path_state, {{0, 2, 4, 7, 8, 9, 5}, {6, 1, 3}}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 5, 8.
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 4, 7, 8, 9, 3}}, {});
  EXPECT_TRUE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Test infeasible moves, with same-sign demands, and same-class paths.
TEST_P(DimensionCheckerSignBoundAndLimitTest, InfeasibleFixedMoves) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0, 6}, {5, 3});
  std::vector<int64_t> demands{3, 2, 2, 0, 1, 0, 3, 1, 1, 1};
  if (use_negative_demands) {
    for (int64_t& demand : demands) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 8}, {0, 8}}, {0, 0}, {FixedIntervalTransit(demands)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: 3, 3.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 9, 5.
  PathStateChangePaths(&path_state, {{0, 2, 4, 7, 8, 9, 5}, {6, 1, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 8, 5.
  PathStateChangePaths(&path_state, {{0, 2, 7, 8, 9, 5}, {6, 1, 3}}, {4});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  // Capacity usage becomes: 5, 9.
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 7, 8, 9, 4, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Test feasible moves, with same-sign demands, and mixed-class paths.
TEST_P(DimensionCheckerSignBoundAndLimitTest, FeasibleFixedMovesMixedClasses) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0, 6}, {5, 3});
  std::vector<int64_t> demands0{3, 2, 2, 0, 0, 0, 3, 0, 0, 0};
  std::vector<int64_t> demands1{3, 2, 0, 0, 2, 0, 3, 0, 0, 0};
  if (use_negative_demands) {
    for (int64_t& demand : demands0) demand = -demand;
    for (int64_t& demand : demands1) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 5}, {0, 5}}, {1, 0},
      {FixedIntervalTransit(demands0), FixedIntervalTransit(demands1)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: 3, 3.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 5, 5.
  PathStateChangePaths(&path_state, {{0, 2, 4, 7, 8, 9, 5}, {6, 1, 3}}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 5, 5 (again).
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 4, 7, 8, 9, 3}}, {});
  EXPECT_TRUE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Test infeasible moves, with same-sign demands, and mixed-class paths.
TEST_P(DimensionCheckerSignBoundAndLimitTest,
       InfeasibleFixedMovesMixedClasses) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0, 6}, {5, 3});
  std::vector<int64_t> demands0{3, 2, 2, 0, 1, 0, 3, 0, 0, 0};
  std::vector<int64_t> demands1{3, 2, 0, 0, 2, 0, 3, 0, 0, 0};
  if (use_negative_demands) {
    for (int64_t& demand : demands0) demand = -demand;
    for (int64_t& demand : demands1) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 5}, {0, 5}}, {1, 0},
      {FixedIntervalTransit(demands0), FixedIntervalTransit(demands1)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: 3, 3.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 5, 5.
  PathStateChangePaths(&path_state, {{0, 2, 4, 7, 8, 9, 5}, {6, 1, 3}}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 5, 6.
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 4, 7, 8, 9, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 5, 5.
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 7, 8, 9, 3}}, {4});
  EXPECT_TRUE(checker.Check());
}

// Test feasible moves containing mixed-sign demands, same-class paths.
TEST_P(DimensionCheckerSignBoundAndLimitTest, FeasibleMixedDemandsSameClasses) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0, 6}, {5, 3});
  std::vector<int64_t> demands{2, 3, -1, 0, 3, 0, 1, -1, 2, -3};
  if (use_negative_demands) {
    for (int64_t& demand : demands) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 5}, {0, 5}}, {0, 0}, {FixedIntervalTransit(demands)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage changes: +2, +1.
  EXPECT_TRUE(checker.Check());
  // Capacity usage changes becomes: +2 -1 +3 -1 +2 -3,     +1,+3.
  PathStateChangePaths(&path_state, {{0, 2, 4, 7, 8, 9, 5}, {6, 1, 3}}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // Capacity usage changes becomes: +2,+3,     +1 -1 +3 -1 +2 -3.
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 4, 7, 8, 9, 3}}, {});
  EXPECT_TRUE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Test feasible moves containing mixed-sign demands, same-class paths.
TEST_P(DimensionCheckerSignBoundAndLimitTest,
       InfeasibleMixedDemandsSameClasses) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0, 6}, {5, 3});
  std::vector<int64_t> demands{3, 3, -1, 0, 3, 0, 0, -1, 2, -3};
  if (use_negative_demands) {
    for (int64_t& demand : demands) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 5}, {0, 5}}, {0, 0}, {FixedIntervalTransit(demands)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage changes: +3, +0.
  EXPECT_TRUE(checker.Check());
  // Capacity usage changes becomes: +3 -1 +3 -1 +2 -3,     +0,+3.
  PathStateChangePaths(&path_state, {{0, 2, 4, 7, 8, 9, 5}, {6, 1, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
  // Capacity usage changes becomes: +3,+3,     +0 -1 +3 -1 +2 -3.
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 4, 7, 8, 9, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Test feasible moves, containing mixed-sign demands, and mixed-class paths.
TEST_P(DimensionCheckerSignBoundAndLimitTest,
       FeasibleMixedDemandsMixedClasses) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0, 6}, {5, 3});
  std::vector<int64_t> demands0{9, 4, 4, 0, -3, 9, 1, -2, 1, -1};
  std::vector<int64_t> demands1{3, 2, -1, 9, 3, 0, 9, -2, 1, -4};
  if (use_negative_demands) {
    for (int64_t& demand : demands0) demand = -demand;
    for (int64_t& demand : demands1) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 5}, {0, 5}}, {1, 0},
      {FixedIntervalTransit(demands0), FixedIntervalTransit(demands1)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: +3, +3.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes:         +3 -1 +3 -2 +1 -4      +1 +4.
  PathStateChangePaths(&path_state, {{0, 2, 4, 7, 8, 9, 5}, {6, 1, 3}}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes:         +3 +2    , +1 +4 -3 -2 +1 -1.
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 4, 7, 8, 9, 3}}, {});
  EXPECT_TRUE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Test feasible moves, containing mixed-sign demands, and mixed-class paths.
TEST_P(DimensionCheckerSignBoundAndLimitTest,
       InfeasibleMixedDemandsMixedClasses) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 10;
  PathState path_state(kNumNodes, {0, 6}, {5, 3});
  std::vector<int64_t> demands1{3, 4, 4, 0, -3, 0, 1, -2, 1, -1};
  std::vector<int64_t> demands0{3, 2, -1, 0, 3, 0, 1, -2, 1, -4};
  if (use_negative_demands) {
    for (int64_t& demand : demands0) demand = -demand;
    for (int64_t& demand : demands1) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 5}, {0, 5}}, {1, 0},
      {FixedIntervalTransit(demands0), FixedIntervalTransit(demands1)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: +3, +3.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes:         +3 -1 +3 -2 +1 -4      +1 +4.
  PathStateChangePaths(&path_state, {{0, 2, 4, 7, 8, 9, 5}, {6, 1, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes:         +3 +2    , +1 +4 -3 -2 +1 -1.
  PathStateChangePaths(&path_state, {{0, 1, 5}, {6, 2, 4, 7, 8, 9, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Tests a feasible case where demand goes up and down.
TEST_P(DimensionCheckerSignBoundAndLimitTest, FeasibleWavyDemand) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 12;
  PathState path_state(kNumNodes, {0, 2}, {1, 3});
  std::vector<int64_t> demands{0, 0, 0, 0, 1, -1, 1, -1, 2, -2, 3, -3};
  if (use_negative_demands) {
    for (int64_t& demand : demands) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 3}, {0, 3}}, {0, 0}, {FixedIntervalTransit(demands)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: 0, 0.
  EXPECT_TRUE(checker.Check());
  // {min, max} capacity usage becomes {0, 1} or {-1, 0}.
  PathStateChangePaths(&path_state, {{0, 6, 7, 8, 9, 10, 11, 1}, {2, 4, 5, 3}},
                       {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // No change in min/max capacity usage
  PathStateChangePaths(&path_state, {{0, 1}, {2, 4, 5, 6, 7, 8, 9, 10, 11, 3}},
                       {});
  EXPECT_TRUE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Tests an infeasible case where demand goes up and down.
TEST_P(DimensionCheckerSignBoundAndLimitTest, InfeasibleWavyDemand) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 12;
  PathState path_state(kNumNodes, {0, 2}, {1, 3});
  std::vector<int64_t> demands{0, 0, 0, 0, 1, -1, 1, -1, 1, -1, 1, -1};
  if (use_negative_demands) {
    for (int64_t& demand : demands) demand = -demand;
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 1}, {0, 1}}, {0, 0}, {FixedIntervalTransit(demands)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: 0, 0.
  EXPECT_TRUE(checker.Check());
  // {min, max} capacity usage becomes {0, 1} or {-1, 0}.
  PathStateChangePaths(&path_state, {{0, 6, 7, 8, 9, 10, 11, 1}, {2, 4, 5, 3}},
                       {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // {min, max} capacity usage becomes {0, 2} or {-2, 0}, infeasible.
  PathStateChangePaths(&path_state, {{0, 1}, {2, 4, 6, 7, 8, 9, 10, 11, 5, 3}},
                       {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

IntervalTransit FixedIntervalBinaryTransit(
    std::vector<std::vector<int64_t>> values) {
  return [values = std::move(values)](
             int64_t node, int64_t next) -> DimensionChecker::Interval {
    const int64_t value = values[node][next];
    return {value, value};
  };
}

// Test feasible moves, with same-sign demands and same-class paths.
// Only two nontrivial paths are feasible: 0 1 2 3 4 5 and 0 4 3 2 1 5.
TEST_P(DimensionCheckerSignBoundAndLimitTest, FeasibleFixedBinaryMoves) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 7;
  PathState path_state(kNumNodes, {0}, {5});
  std::vector<std::vector<int64_t>> demands{{6, 1, 6, 6, 1, 5},
                                            {6, 6, 1, 6, 6, 1},
                                            {6, 1, 6, 1, 6, 6},
                                            {6, 6, 1, 6, 1, 6},
                                            {6, 6, 6, 1, 6, 1}};

  if (use_negative_demands) {
    for (auto& row : demands) {
      for (int64_t& demand : row) demand = -demand;
    }
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 5}}, {0}, {FixedIntervalBinaryTransit(demands)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: 5.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 1+1+1+1+1.
  PathStateChangePaths(&path_state, {{0, 1, 2, 3, 4, 5}}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 1+1+1+1+1.
  PathStateChangePaths(&path_state, {{0, 4, 3, 2, 1, 5}}, {});
  EXPECT_TRUE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

// Test infeasible moves, with same-sign demands and same-class paths.
// Only one nontrivial paths in infeasible: 0 1 2 3 4 5.
TEST_P(DimensionCheckerSignBoundAndLimitTest, InfeasibleFixedBinaryMoves) {
  const bool use_negative_demands = ::testing::get<0>(GetParam());
  constexpr int kNumNodes = 7;
  PathState path_state(kNumNodes, {0}, {5});
  std::vector<std::vector<int64_t>> demands{{0, 1, 0, 0, 0, 0},
                                            {0, 0, 1, 0, 0, 0},
                                            {0, 0, 0, 1, 0, 0},
                                            {0, 0, 0, 0, 1, 0},
                                            {0, 0, 0, 0, 0, 1}};

  if (use_negative_demands) {
    for (auto& row : demands) {
      for (int64_t& demand : row) demand = -demand;
    }
  }
  const DimensionChecker::Interval node_capacity =
      ::testing::get<1>(GetParam());
  DimensionChecker checker(
      &path_state, {{0, 4}}, {0}, {FixedIntervalBinaryTransit(demands)},
      std::vector<DimensionChecker::Interval>(kNumNodes, node_capacity),
      ::testing::get<2>(GetParam()));
  // Initial state's capacity usage: 0.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes 1.
  PathStateChangePaths(&path_state, {{0, 4, 1, 2, 3, 5}}, {});
  EXPECT_TRUE(checker.Check());
  checker.Commit();
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 1+1+1+1+1.
  PathStateChangePaths(&path_state, {{0, 1, 2, 3, 4, 5}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
}

constexpr DimensionChecker::Interval kNonNegativeInterval{0, kint64max};
constexpr DimensionChecker::Interval kAllNumbersInterval{kint64min, kint64max};

// Instantiating node_capacity to kNonNegativeInterval forces the generic check,
// instantiating to kAllNumbersInterval triggers the optimized check.
INSTANTIATE_TEST_SUITE_P(
    DimensionCheckerSignBoundAndLimitTestInstance,
    DimensionCheckerSignBoundAndLimitTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(kNonNegativeInterval, kAllNumbersInterval),
        ::testing::Values(0, DimensionChecker::kOptimalMinRangeSizeForRIQ,
                          kint32max)));

IntervalTransit VariableIntervalTransit(
    std::vector<DimensionChecker::Interval> intervals) {
  return
      [intervals = std::move(intervals)](int i, int) { return intervals[i]; };
}

using DimensionCheckerTest = ::testing::TestWithParam<int>;

// Feasible move where transit and node capacities are intervals.
TEST_P(DimensionCheckerTest, FeasibleIntervalDemandsAndNodeCapacities) {
  constexpr int kNumNodes = 4;
  PathState path_state(kNumNodes, {0}, {3});
  std::vector<DimensionChecker::Interval> demands(kNumNodes, {1, 2});
  std::vector<DimensionChecker::Interval> node_capacities{
      {1, 2}, {2, 4}, {3, 6}, {4, 8}};
  DimensionChecker checker(
      &path_state,
      /*path_capacity*/ {{0, 10}},
      /*path_class*/ {0},
      /*min_max_demand_per_class*/ {VariableIntervalTransit(demands)},
      /*node_capacity_*/ node_capacities, GetParam());
  // Initial state's final capacity usage: [2, 4] inter [4, 8] = [4, 4].
  EXPECT_TRUE(checker.Check());
  // Final capacity usage becomes: [4, 8] inter [4, 8] = [4, 8].
  PathStateChangePaths(&path_state, {{0, 1, 2, 3}}, {});
  EXPECT_TRUE(checker.Check());
}

// Infeasible moves where transit and node capacities are intervals.
TEST_P(DimensionCheckerTest, InfeasibleIntervalDemandsAndNodeCapacities) {
  constexpr int kNumNodes = 4;
  PathState path_state(kNumNodes, {0}, {3});
  std::vector<DimensionChecker::Interval> demands{
      {1, 2}, {1, 1}, {1, 2}, {1, 2}};
  std::vector<DimensionChecker::Interval> node_capacities{
      {0, 0}, {2, 4}, {3, 3}, {4, 8}};
  DimensionChecker checker(
      &path_state,
      /*path_capacity*/ {{0, 10}},
      /*path_class*/ {0},
      /*min_max_demand_per_class*/ {VariableIntervalTransit(demands)},
      /*node_capacity_*/ node_capacities, GetParam());
  // Initial state's final capacity usage: [2, 4] inter [4, 8] = [4, 4].
  EXPECT_TRUE(checker.Check());
  // Capacity at node 3 is [2, 3] inter [4, 8], infeasible.
  PathStateChangePaths(&path_state, {{0, 1, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  // Capacity at node 2 is [1, 2] inter [3, 3] = empty set, infeasible.
  PathStateChangePaths(&path_state, {{0, 2, 3}}, {1});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
}

TEST_P(DimensionCheckerTest, NoKInt64MaxOverflow) {
  constexpr int kNumNodes = 4;
  PathState path_state(kNumNodes, {0}, {3});
  const std::vector<DimensionChecker::Interval> node_capacities{kNumNodes,
                                                                {0, kint64max}};
  DimensionChecker checker(&path_state,
                           /*path_capacity*/ {{0, kint64max - 1}},
                           /*path_class*/ {0},
                           /*min_max_demand_per_class*/
                           {FixedIntervalTransit({2, kint64max - 1, -2, 0})},
                           /*node_capacity_*/ node_capacities, GetParam());
  // Initial state's capacity usage: 2 + 0.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 2, kint64max + 1, kint64max - 1, kint64max - 1.
  PathStateChangePaths(&path_state, {{0, 1, 2, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 2, 0, kint64max - 1, kint64max - 1.
  PathStateChangePaths(&path_state, {{0, 2, 1, 3}}, {});
  EXPECT_TRUE(checker.Check());
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
}

TEST_P(DimensionCheckerTest, NoKInt64MinOverflow) {
  constexpr int kNumNodes = 4;
  PathState path_state(kNumNodes, {0}, {3});
  const std::vector<DimensionChecker::Interval> node_capacities(
      kNumNodes, {kint64min, kint64max});
  DimensionChecker checker(&path_state,
                           /*path_capacity*/ {{kint64min + 1, 0}},
                           /*path_class*/ {0},
                           /*min_max_demand_per_class*/
                           {FixedIntervalTransit({-2, kint64min + 1, 2, 0})},
                           /*node_capacity_*/ node_capacities, GetParam());
  // Initial state's capacity usage: -2 + 0.
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: -2, kint64min - 1, kint64min + 1, kint64min + 1.
  PathStateChangePaths(&path_state, {{0, 1, 2, 3}}, {});
  EXPECT_FALSE(checker.Check());
  path_state.Revert();
  EXPECT_TRUE(checker.Check());
  // Capacity usage becomes: 2, 0, kint64min + 1, kint64min + 1.
  PathStateChangePaths(&path_state, {{0, 2, 1, 3}}, {});
  EXPECT_TRUE(checker.Check());
  path_state.Commit();
  EXPECT_TRUE(checker.Check());
}

TEST_P(DimensionCheckerTest, RepeatedCommits) {
  constexpr int kNumNodes = 24;
  constexpr int kCapacity = 11;
  PathState path_state(kNumNodes, {0, 1}, {2, 3});

  using Intervals = std::vector<DimensionChecker::Interval>;
  Intervals path_capacity = {{0, kCapacity}, {0, kCapacity}};
  const std::vector<int> path_class = {0, 0};
  const Intervals node_capacity(kNumNodes, {kint64min, kint64max});
  DimensionChecker checker(
      &path_state,
      /*path_capacity*/ path_capacity,
      /*path_class*/ path_class,
      /*min_max_demand_per_class*/ {ConstantIntervalTransit(1)},
      /*node_capacity_*/ node_capacity, GetParam());

  // Initial state, 23 is not used.
  const std::vector<std::vector<int>> initial_paths = {
      {0, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 2},
      {1, 5, 7, 9, 11, 13, 15, 17, 19, 21, 3}};
  // Path 0 is illegal, it has too much demand.
  const std::vector<std::vector<int>> illegal_paths = {
      {0, 7, 9, 11, 13, 15, 17, 19, 21, 18, 20, 22, 2},
      {1, 5, 4, 6, 8, 10, 12, 14, 16, 3}};
  // Those paths are legal, notice that chain lengths are 7 and 8 to exercise
  // the limits of power-of-2 based Range Intersection Query.
  const std::vector<std::vector<int>> alternate_paths = {
      {0, 9, 11, 13, 15, 17, 19, 21, 4, 6, 2},
      {1, 5, 7, 8, 10, 12, 14, 16, 18, 20, 22, 3}};
  // Iterate a large number of times to exercise amortized algorithms.
  constexpr int kNumIterations = 1000;
  for (int i = 0; i < kNumIterations; ++i) {
    // Initial state, commit.
    PathStateChangePaths(&path_state, initial_paths, {});
    ASSERT_TRUE(checker.Check());
    checker.Commit();
    path_state.Commit();
    // Illegal state, revert.
    PathStateChangePaths(&path_state, illegal_paths, {});
    ASSERT_FALSE(checker.Check());
    path_state.Revert();
    // Alternate state, commit.
    PathStateChangePaths(&path_state, alternate_paths, {});
    ASSERT_TRUE(checker.Check());
    checker.Commit();
    path_state.Commit();
  }
}

INSTANTIATE_TEST_SUITE_P(
    DimensionCheckerTestInstance, DimensionCheckerTest,
    ::testing::Values(0, DimensionChecker::kOptimalMinRangeSizeForRIQ,
                      kint32max));

TEST(DimensionFilterTest, FilterForwardsAccept) {
  constexpr int kNumNodes = 4;
  PathState path_state(kNumNodes, {0}, {3});
  Solver solver("test");
  const std::vector<DimensionChecker::Interval> path_capacity{{0, 5}};
  const std::vector<int> path_class = {0};
  const std::vector<IntervalTransit> demand{FixedIntervalTransit({0, 3, 3, 0})};
  LocalSearchFilter* filter = MakeDimensionFilter(
      &solver,
      std::make_unique<DimensionChecker>(
          &path_state, path_capacity, path_class, demand,
          std::vector<DimensionChecker::Interval>(4, {0, kint64max})),
      /*dimension_name*/ "dimension_name_test");
  EXPECT_EQ("DimensionFilter(dimension_name_test)", filter->DebugString());

  PathStateChangePaths(&path_state, {{0, 1, 3}}, {});
  EXPECT_TRUE(filter->Accept({}, {}, kint64min, kint64max));
  path_state.Revert();

  PathStateChangePaths(&path_state, {{0, 1, 2, 3}}, {});
  EXPECT_FALSE(filter->Accept({}, {}, kint64min, kint64max));
}

TEST(DimensionFilterTest, FilterForwardsSynchronize) {
  using Intervals = std::vector<DimensionChecker::Interval>;
  constexpr int kNumNodes = 5;
  PathState path_state(kNumNodes, {0}, {1});
  Solver solver("test");
  const Intervals path_capacity{{0, 6}};
  const std::vector<int> path_class = {0};
  const std::vector<IntervalTransit> demand{
      FixedIntervalTransit({0, 0, 3, 3, 3})};
  const Intervals node_capacities(kNumNodes, {kint64min, kint64max});
  // Setting fixed demands and trivial node capacities should make the checker
  // use internal state, that needs correct synchronized/committed path_state.
  auto checker_ownership = std::make_unique<DimensionChecker>(
      &path_state, path_capacity, path_class, demand, node_capacities);
  DimensionChecker* checker = checker_ownership.get();
  LocalSearchFilter* filter = MakeDimensionFilter(
      &solver, std::move(checker_ownership), /*dimension_name*/ "");
  // We make the path go through three states: 021, 0231 and 04231.
  // The first step is synchronized by hand, the second by the filter,
  // the decisive step happens in the third step:
  // if filter did not forward in the second step,
  // the filter will not reject the third step.
  // Step 1, total capacity used = 3.
  PathStateChangePaths(&path_state, {{0, 2, 1}}, {});
  checker->Commit();
  path_state.Commit();
  // Step 2, total capacity used = 6.
  PathStateChangePaths(&path_state, {{0, 2, 3, 1}}, {});
  EXPECT_TRUE(filter->Accept({}, {}, kint64min, kint64max));
  filter->Synchronize({}, {});  // Must forward to checker.
  path_state.Commit();
  // Step 3, total capacity used = 9.
  PathStateChangePaths(&path_state, {{0, 4, 2, 3, 1}}, {});
  EXPECT_FALSE(filter->Accept({}, {}, kint64min, kint64max));
}

// Represents the initial state of a routing dimension: cumuls and break
// initial domains.
struct DimensionInitialScenario {
  using Interval = DimensionValues::Interval;
  std::vector<Interval> cumuls;
  using VehicleBreak = DimensionValues::VehicleBreak;
  std::vector<std::vector<VehicleBreak>> vehicle_breaks;
  struct InterbreakLimit {
    int64_t max_interbreak_duration;
    int64_t min_break_duration;
  };
  std::vector<std::vector<InterbreakLimit>> interbreak_limits;
};

std::unique_ptr<DimensionValues> MakeDimensionValuesFromInitialScenario(
    const DimensionInitialScenario& scenario,
    absl::Span<const std::vector<int>> paths) {
  using Interval = DimensionValues::Interval;
  const int num_nodes = scenario.cumuls.size();
  const int num_paths = paths.size();
  auto dimension = std::make_unique<DimensionValues>(num_paths, num_nodes);
  for (int path = 0; path < num_paths; ++path) {
    const auto& nodes = paths[path];
    // Push all nodes in dimension, set path.
    for (const int node : nodes) dimension->PushNode(node);
    dimension->MakePathFromNewNodes(path);
    // Fill initial values for cumuls, neutral for the rest.
    absl::Span<Interval> cumuls = dimension->MutableCumuls(path);
    for (int i = 0; i < nodes.size(); ++i) {
      cumuls[i] = scenario.cumuls[nodes[i]];
    }
    for (auto& transit : dimension->MutableTransits(path)) {
      transit = {.min = 0, .max = kint64max};
    }
    for (auto& travel : dimension->MutableTravels(path)) travel = 0;
    for (auto& travel_sum : dimension->MutableTravelSums(path)) travel_sum = 0;
    dimension->MutableSpan(path) = {.min = 0, .max = kint64max};
    dimension->MutableVehicleBreaks(path) = scenario.vehicle_breaks[path];
  }
  dimension->Commit();
  return dimension;
}

TEST(FillDimensionValuesFromDimensionTest, GeneralBehavior) {
  using Interval = DimensionValues::Interval;
  const std::vector<Interval> input_cumuls{
      {.min = 10, .max = 1000},
      {.min = 11, .max = 1001},
      {.min = 12, .max = 1002},
      {.min = 13, .max = 1003},
  };
  const std::vector<Interval> input_slacks{
      {.min = 0, .max = kint64max},
      {.min = 100, .max = kint64max},
      {.min = 0, .max = 1000},
  };
  const std::vector<int64_t> expected_travels = {5, 6, 7};
  auto travel_evaluator = [&expected_travels](int64_t from, int64_t) {
    return expected_travels[from];
  };
  const std::vector<int64_t> expected_travel_sums = {0, 5, 11, 18};
  const std::vector<Interval> expected_transits = {
      {.min = 5, .max = kint64max},
      {.min = 106, .max = kint64max},
      {.min = 7, .max = 1007},
  };

  struct Scenario {
    int64_t capacity;
    int64_t span_upper_bound;
    bool expected_feasible;
  };
  const std::vector<Scenario> scenarios = {
      {.capacity = 10000, .span_upper_bound = 1000, .expected_feasible = true},
      {.capacity = 13, .span_upper_bound = 1000, .expected_feasible = true},
      {.capacity = 12, .span_upper_bound = 1000, .expected_feasible = false},
      {.capacity = 10000, .span_upper_bound = 18, .expected_feasible = true},
      {.capacity = 10000, .span_upper_bound = 17, .expected_feasible = false},
  };

  DimensionValues dimension_values(1, 4);

  for (int s = 0; s < scenarios.size(); ++s) {
    dimension_values.Revert();
    dimension_values.PushNode(0);
    dimension_values.PushNode(1);
    dimension_values.PushNode(2);
    dimension_values.PushNode(3);
    dimension_values.MakePathFromNewNodes(0);
    const Scenario& scenario = scenarios[s];
    const bool feasible = FillDimensionValuesFromDimension(
        0, scenario.capacity, scenario.span_upper_bound, input_cumuls,
        input_slacks, travel_evaluator, dimension_values);
    EXPECT_EQ(feasible, scenario.expected_feasible);
    if (!feasible) continue;
    EXPECT_THAT(dimension_values.Travels(0),
                ElementsAreArray(expected_travels));
    EXPECT_THAT(dimension_values.TravelSums(0),
                ElementsAreArray(expected_travel_sums));
    const absl::Span<const Interval> cumuls = dimension_values.Cumuls(0);
    for (int r = 0; r < cumuls.size(); ++r) {
      Interval expected_cumul = input_cumuls[r];
      expected_cumul.DecreaseMax(scenario.capacity);
      EXPECT_EQ(cumuls[r], expected_cumul);
    }
    EXPECT_THAT(dimension_values.Transits(0),
                ElementsAreArray(expected_transits));
    const Interval expected_span = {.min = expected_travel_sums.back(),
                                    .max = scenario.span_upper_bound};
    EXPECT_EQ(dimension_values.Span(0), expected_span);
  }
}

TEST(FillDimensionValuesFromDimensionTest, TravelCaching) {
  DimensionValues dimension_values(1, 6);
  using Interval = DimensionValues::Interval;
  std::vector<Interval> cumuls(6, {.min = 0, .max = 100});
  std::vector<Interval> slacks(6, {.min = 0, .max = kint64max});
  int num_evaluator_calls = 0;
  auto travel_evaluator = [&num_evaluator_calls](int64_t from,
                                                 int64_t to) -> int64_t {
    ++num_evaluator_calls;
    return 10 * from + to;
  };
  // Make initial state.
  const std::vector<int> initial_nodes = {1, 3, 0, 4, 2};
  for (const int node : initial_nodes) {
    dimension_values.PushNode(node);
  }
  dimension_values.MakePathFromNewNodes(0);
  EXPECT_TRUE(FillDimensionValuesFromDimension(
      0, 10000, 10000, cumuls, slacks, travel_evaluator, dimension_values));
  EXPECT_THAT(dimension_values.Travels(0), ElementsAreArray({13, 30, 4, 42}));
  dimension_values.Commit();
  // Test changes to paths with various parts in common with initial path:
  struct Scenario {
    std::vector<int> nodes;
    std::vector<int64_t> expected_travels;
    int expected_num_evaluator_calls;
  };
  std::vector<Scenario> scenarios = {
      // Same path.
      {.nodes = {1, 3, 0, 4, 2},
       .expected_travels = {13, 30, 4, 42},
       .expected_num_evaluator_calls = 0},
      // Removed one node.
      {.nodes = {1, 0, 4, 2},
       .expected_travels = {10, 4, 42},
       .expected_num_evaluator_calls = 1},
      {.nodes = {1, 3, 4, 2},
       .expected_travels = {13, 34, 42},
       .expected_num_evaluator_calls = 1},
      {.nodes = {1, 3, 0, 2},
       .expected_travels = {13, 30, 2},
       .expected_num_evaluator_calls = 1},
      // Added one node.
      {.nodes = {1, 3, 0, 4, 2},
       .expected_travels = {13, 30, 4, 42},
       .expected_num_evaluator_calls = 0},
      {.nodes = {1, 5, 3, 0, 4, 2},
       .expected_travels = {15, 53, 30, 4, 42},
       .expected_num_evaluator_calls = 2},
      {.nodes = {1, 3, 5, 0, 4, 2},
       .expected_travels = {13, 35, 50, 4, 42},
       .expected_num_evaluator_calls = 2},
      {.nodes = {1, 3, 0, 5, 4, 2},
       .expected_travels = {13, 30, 5, 54, 42},
       .expected_num_evaluator_calls = 2},
      {.nodes = {1, 3, 0, 4, 5, 2},
       .expected_travels = {13, 30, 4, 45, 52},
       .expected_num_evaluator_calls = 2},
      // Emptied path.
      {.nodes = {1, 2},
       .expected_travels = {12},
       .expected_num_evaluator_calls = 1},
      // Exchanged nodes.
      {.nodes = {1, 4, 0, 3, 2},
       .expected_travels = {14, 40, 3, 32},
       .expected_num_evaluator_calls = 4},
      // Moved a chain.
      {.nodes = {1, 0, 4, 3, 2},
       .expected_travels = {10, 4, 43, 32},
       .expected_num_evaluator_calls = 4},
  };
  for (const Scenario& scenario : scenarios) {
    dimension_values.Revert();
    num_evaluator_calls = 0;
    for (const int node : scenario.nodes) {
      dimension_values.PushNode(node);
    }
    dimension_values.MakePathFromNewNodes(0);
    EXPECT_TRUE(FillDimensionValuesFromDimension(
        0, 10000, 10000, cumuls, slacks, travel_evaluator, dimension_values));
    EXPECT_THAT(dimension_values.Travels(0),
                ElementsAreArray(scenario.expected_travels));
    EXPECT_EQ(num_evaluator_calls, scenario.expected_num_evaluator_calls);
  }
}

// Test FillPrePostVisitValues with nullptr evaluators, expect all-0 values.
TEST(FillPrePostVisitValuesTest, NullptrEvaluators) {
  DimensionValues dimension_values(/*num_paths=*/2, /*num_nodes=*/8);
  dimension_values.PushNode(2);
  dimension_values.PushNode(4);
  dimension_values.PushNode(5);
  dimension_values.MakePathFromNewNodes(1);
  PrePostVisitValues visit_values(/*num_paths=*/2, /*num_nodes=*/8);
  FillPrePostVisitValues(1, dimension_values, std::nullopt, std::nullopt,
                         visit_values);
  EXPECT_THAT(visit_values.PreVisits(1), ElementsAre(0, 0, 0));
  EXPECT_THAT(visit_values.PostVisits(1), ElementsAre(0, 0, 0));
  dimension_values.Commit();
  visit_values.Commit();
  dimension_values.PushNode(2);
  dimension_values.PushNode(1);
  dimension_values.PushNode(3);
  dimension_values.PushNode(5);
  dimension_values.MakePathFromNewNodes(1);
  FillPrePostVisitValues(1, dimension_values, std::nullopt, std::nullopt,
                         visit_values);
  EXPECT_THAT(visit_values.PreVisits(1), ElementsAre(0, 0, 0, 0));
  EXPECT_THAT(visit_values.PostVisits(1), ElementsAre(0, 0, 0, 0));
  EXPECT_THAT(visit_values.CommittedPreVisits(1), ElementsAre(0, 0, 0));
  EXPECT_THAT(visit_values.CommittedPostVisits(1), ElementsAre(0, 0, 0));
}

TEST(FillPrePostVisitValuesTest, GenericEvaluators) {
  DimensionValues dimension_values(/*num_paths=*/2, /*num_nodes=*/8);
  dimension_values.PushNode(2);
  dimension_values.PushNode(4);
  dimension_values.PushNode(5);
  dimension_values.MakePathFromNewNodes(1);
  PrePostVisitValues visit_values(/*num_paths=*/2, /*num_nodes=*/8);
  auto pre_travel_evaluator = [](int64_t from, int64_t to) {
    return 10 * from + to;
  };
  auto post_travel_evaluator = [](int64_t from, int64_t to) {
    return 1000 * from + 100 * to;
  };
  FillPrePostVisitValues(1, dimension_values, pre_travel_evaluator,
                         post_travel_evaluator, visit_values);
  const std::vector<int64_t> expected_pre_visits1 = {0, 2400, 4500};
  const std::vector<int64_t> expected_post_visits1 = {24, 45, 0};
  EXPECT_THAT(visit_values.PreVisits(1),
              ElementsAreArray(expected_pre_visits1));
  EXPECT_THAT(visit_values.PostVisits(1),
              ElementsAreArray(expected_post_visits1));
  dimension_values.Commit();
  visit_values.Commit();
  dimension_values.PushNode(2);
  dimension_values.PushNode(1);
  dimension_values.PushNode(3);
  dimension_values.PushNode(5);
  dimension_values.MakePathFromNewNodes(1);
  FillPrePostVisitValues(1, dimension_values, pre_travel_evaluator,
                         post_travel_evaluator, visit_values);
  const std::vector<int64_t> expected_pre_visits2 = {0, 2100, 1300, 3500};
  const std::vector<int64_t> expected_post_visits2 = {21, 13, 35, 0};
  EXPECT_THAT(visit_values.PreVisits(1),
              ElementsAreArray(expected_pre_visits2));
  EXPECT_THAT(visit_values.PostVisits(1),
              ElementsAreArray(expected_post_visits2));
  EXPECT_THAT(visit_values.CommittedPreVisits(1),
              ElementsAreArray(expected_pre_visits1));
  EXPECT_THAT(visit_values.CommittedPostVisits(1),
              ElementsAreArray(expected_post_visits1));
}

TEST(PropagateLightweightVehicleBreaksTest, RelaxAndCheckMandatoryBreaks) {
  using Interval = DimensionValues::Interval;
  const int kNumNodes = 6;
  DimensionInitialScenario kInitialScenario{
      .cumuls = {kNumNodes, {.min = 0, .max = 100}},
      .vehicle_breaks = {{},
                         {{.start = {.min = 40, .max = 45},
                           .end = {.min = 50, .max = 55},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = true, .max = true}},
                          {.start = {.min = 50, .max = 60},
                           .end = {.min = 65, .max = 75},
                           .duration = {.min = 15, .max = 100},
                           .is_performed = {.min = true, .max = true}},
                          {.start = {.min = 50, .max = 60},
                           .end = {.min = 65, .max = 75},
                           .duration = {.min = 15, .max = 100},
                           .is_performed = {.min = false, .max = true}}}},
      .interbreak_limits = {{}, {}},
  };
  const std::vector<std::vector<int>> paths = {{0, 2}, {1, 3}};
  std::unique_ptr<DimensionValues> dimension_values =
      MakeDimensionValuesFromInitialScenario(kInitialScenario, paths);
  const std::vector<std::pair<int64_t, int64_t>> interbreaks = {};

  DimensionValuesPathScenario kScenario{
      .nodes = {1, 3},
      .cumuls = {{.min = 0, .max = 40}, {.min = 75, .max = 100}},
      .transits = {{.min = 30, .max = 100}},
      .travels = {30},
  };
  DimensionValuesPathScenario kExpectedScenario{
      .cumuls = {{.min = 0, .max = 40}, {.min = 75, .max = 100}},
      .span = Interval(30 + 10 + 15, kint64max),
  };
  std::vector<DimensionValuesPathScenario> scenarios{5, kScenario};
  std::vector<DimensionValuesPathScenario> expected_scenarios{
      5, kExpectedScenario};
  // Scenario 1: smallest start/end interval to force inclusion of both breaks.
  scenarios[1].cumuls.front().max = 49;
  scenarios[1].cumuls.back().min = 61;
  expected_scenarios[1].span->min = 30 + 10 + 15;
  expected_scenarios[1].cumuls.front().max = 45;  // == end_max - 30 - 10 - 15
  expected_scenarios[1].cumuls.back().min = 65;   // == start_min + 30 + 10 + 15
  // Scenario 2: first break can finish before the route.
  scenarios[2].cumuls.front().max = 50;
  scenarios[2].cumuls.back().min = 61;
  expected_scenarios[2].span->min = 30 + 0 + 15;
  expected_scenarios[2].cumuls.front().max = 50;  // < end_max - 30 - 15
  expected_scenarios[2].cumuls.back().min = 65;   // breaks[1].end.min
  // Scenario 3: second break can start after the route.
  scenarios[3].cumuls.front().max = 49;
  scenarios[3].cumuls.back().min = 60;
  scenarios[3].travels[0] = 29;
  expected_scenarios[3].span->min = 29 + 10 + 0;
  expected_scenarios[3].cumuls.front().max = 45;  // breaks[0].start.max
  expected_scenarios[3].cumuls.back().min = 60;
  // Scenario 4: first break can start before, second break can end after.
  scenarios[4].cumuls.front().max = 50;
  scenarios[4].cumuls.back().min = 60;
  scenarios[4].travels[0] = 30;
  expected_scenarios[4].span->min = 30 + 0 + 0;
  expected_scenarios[4].cumuls.front().max = 50;
  expected_scenarios[4].cumuls.back().min = 60;
  // Run all scenarios.
  for (int s = 0; s < scenarios.size(); ++s) {
    FillDimensionValuesWithPathScenario(scenarios[s], 1, *dimension_values);
    dimension_values->MutableVehicleBreaks(1) =
        kInitialScenario.vehicle_breaks[1];
    EXPECT_TRUE(
        PropagateLightweightVehicleBreaks(1, *dimension_values, interbreaks));
    ExpectDimensionValuesScenario(expected_scenarios[s], 1, *dimension_values);
    dimension_values->Revert();
  }
  // Check that the filter returns false when it empties an interval.
  // Make sure span is checked.
  for (int s = 0; s < scenarios.size(); ++s) {
    DimensionValuesPathScenario scenario = scenarios[s];
    scenario.span = Interval(0, expected_scenarios[s].span->min - 1);
    FillDimensionValuesWithPathScenario(scenario, 1, *dimension_values);
    dimension_values->MutableVehicleBreaks(1) =
        kInitialScenario.vehicle_breaks[1];
    EXPECT_FALSE(
        PropagateLightweightVehicleBreaks(1, *dimension_values, interbreaks));
    dimension_values->Revert();
  }
  // Make sure start is checked.
  for (int s = 0; s < scenarios.size(); ++s) {
    DimensionValuesPathScenario scenario = scenarios[s];
    scenario.cumuls.front().min = expected_scenarios[s].cumuls.front().max + 1;
    FillDimensionValuesWithPathScenario(scenario, 1, *dimension_values);
    dimension_values->MutableVehicleBreaks(1) =
        kInitialScenario.vehicle_breaks[1];
    EXPECT_FALSE(
        PropagateLightweightVehicleBreaks(1, *dimension_values, interbreaks));
    dimension_values->Revert();
  }
  // Make sure end is checked.
  for (int s = 0; s < scenarios.size(); ++s) {
    DimensionValuesPathScenario scenario = scenarios[s];
    scenario.cumuls.back().max = expected_scenarios[s].cumuls.back().min - 1;
    FillDimensionValuesWithPathScenario(scenario, 1, *dimension_values);
    dimension_values->MutableVehicleBreaks(1) =
        kInitialScenario.vehicle_breaks[1];
    EXPECT_FALSE(
        PropagateLightweightVehicleBreaks(1, *dimension_values, interbreaks));
    dimension_values->Revert();
  }
}

TEST(PropagateLightweightVehicleBreaksTest, OrderedBreaks) {
  using Interval = DimensionValues::Interval;
  using VehicleBreak = DimensionValues::VehicleBreak;
  constexpr int kNumNodes = 6;
  const DimensionInitialScenario initial_scenario{
      .cumuls = {kNumNodes, {.min = 0, .max = 100}},
      .vehicle_breaks = {{},
                         {{.start = {.min = 36, .max = 60},
                           .end = {.min = 46, .max = 70},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = true, .max = true}},
                          {.start = {.min = 0, .max = 100},
                           .end = {.min = 10, .max = 100},
                           .duration = {.min = 10, .max = 100},
                           .is_performed = {.min = false, .max = true}},
                          {.start = {.min = 20, .max = 50},
                           .end = {.min = 35, .max = 65},
                           .duration = {.min = 15, .max = 100},
                           .is_performed = {.min = true, .max = true}}}},
      .interbreak_limits = {{}, {}},
  };
  const std::vector<std::vector<int>> paths = {{0, 2}, {1, 3}};
  std::unique_ptr<DimensionValues> dimension_values =
      MakeDimensionValuesFromInitialScenario(initial_scenario, paths);
  const std::vector<std::pair<int64_t, int64_t>> interbreaks = {};

  const DimensionValuesPathScenario path_scenario{
      .nodes = {1, 3},
      .cumuls = {{.min = 0, .max = 45}, {.min = 51, .max = 100}},
      .transits = {{.min = 10, .max = 100}},
      .travels = {10},
  };

  // Without break ordering, break 0 can occur after the route and break 2 can
  // occur before the route, so neither is forced inside the route.
  FillDimensionValuesWithPathScenario(path_scenario, 1, *dimension_values);
  dimension_values->MutableVehicleBreaks(1) =
      initial_scenario.vehicle_breaks[1];
  EXPECT_TRUE(PropagateLightweightVehicleBreaks(
      1, *dimension_values, interbreaks, /*breaks_are_ordered=*/false));
  EXPECT_THAT(dimension_values->VehicleBreaks(1),
              ElementsAreArray(initial_scenario.vehicle_breaks[1]));
  ExpectDimensionValuesScenario(
      {.cumuls = {{.min = 0, .max = 45}, {.min = 51, .max = 100}},
       .span = Interval(10, kint64max)},
      1, *dimension_values);
  dimension_values->Revert();

  // With break ordering, mandatory breaks 0 and 2 tighten each other (while
  // skipping optional break 1), forcing both inside the route.
  FillDimensionValuesWithPathScenario(path_scenario, 1, *dimension_values);
  dimension_values->MutableVehicleBreaks(1) =
      initial_scenario.vehicle_breaks[1];
  EXPECT_TRUE(PropagateLightweightVehicleBreaks(
      1, *dimension_values, interbreaks, /*breaks_are_ordered=*/true));
  const std::vector<VehicleBreak> expected_breaks = {
      {.start = {.min = 36, .max = 40},
       .end = {.min = 46, .max = 50},
       .duration = {.min = 10, .max = 100},
       .is_performed = {.min = true, .max = true}},
      initial_scenario.vehicle_breaks[1][1],
      {.start = {.min = 46, .max = 50},
       .end = {.min = 61, .max = 65},
       .duration = {.min = 15, .max = 100},
       .is_performed = {.min = true, .max = true}},
  };
  EXPECT_THAT(dimension_values->VehicleBreaks(1),
              ElementsAreArray(expected_breaks));
  ExpectDimensionValuesScenario(
      {.cumuls = {{.min = 0, .max = 40}, {.min = 61, .max = 100}},
       .span = Interval(10 + 10 + 15, kint64max)},
      1, *dimension_values);
  dimension_values->Revert();

  // If mandatory break 2 must start before mandatory break 0 can end,
  // propagation succeeds when unordered but fails when ordered.
  FillDimensionValuesWithPathScenario(path_scenario, 1, *dimension_values);
  dimension_values->MutableVehicleBreaks(1) =
      initial_scenario.vehicle_breaks[1];
  dimension_values->MutableVehicleBreaks(1)[2].start.max = 45;
  EXPECT_TRUE(PropagateLightweightVehicleBreaks(
      1, *dimension_values, interbreaks, /*breaks_are_ordered=*/false));
  EXPECT_FALSE(PropagateLightweightVehicleBreaks(
      1, *dimension_values, interbreaks, /*breaks_are_ordered=*/true));
  dimension_values->Revert();
}

std::vector<std::pair<int64_t, int64_t>>
MakeInterbreakLimitsFromInitialScenario(
    const DimensionInitialScenario& scenario, int path) {
  std::vector<std::pair<int64_t, int64_t>> interbreaks;
  if (scenario.interbreak_limits.empty()) return interbreaks;
  for (const auto& limit : scenario.interbreak_limits[path]) {
    interbreaks.push_back(
        {limit.max_interbreak_duration, limit.min_break_duration});
  }
  return interbreaks;
}

DimensionInitialScenario MakeDimensionInitialScenario() {
  return {.cumuls = {6, {.min = 0, .max = 100}},
          .vehicle_breaks = {{},
                             {{.start = {.min = 0, .max = 90},
                               .end = {.min = 10, .max = 100},
                               .duration = {.min = 10, .max = 100},
                               .is_performed = {.min = true, .max = true}},
                              {.start = {.min = 0, .max = 85},
                               .end = {.min = 15, .max = 100},
                               .duration = {.min = 15, .max = 100},
                               .is_performed = {.min = true, .max = true}}}},
          .interbreak_limits = {
              {},
              {{.max_interbreak_duration = 20, .min_break_duration = 10},
               {.max_interbreak_duration = 32, .min_break_duration = 15}}}};
}

TEST(PropagateLightweightVehicleBreaksTest, InterbreakLimits) {
  using Interval = DimensionValues::Interval;
  // Make initial instance of DimensionValues with interbreak limits.
  DimensionInitialScenario initial_scenario = MakeDimensionInitialScenario();
  initial_scenario.interbreak_limits = {
      {},
      {{.max_interbreak_duration = 20, .min_break_duration = 10},
       {.max_interbreak_duration = 32, .min_break_duration = 15}}};
  std::unique_ptr<DimensionValues> dimension_values =
      MakeDimensionValuesFromInitialScenario(initial_scenario,
                                             {{0, 2}, {1, 3}});
  // Some scenarios of how transit + interbreak limits affect expected span.
  struct ExpectedSpanForScenario {
    int64_t transit = 0;
    int64_t expected_span = 0;
    bool is_feasible = true;
  };
  const std::vector<ExpectedSpanForScenario> span_scenarios{
      {.transit = 20, .expected_span = 20},
      {.transit = 21, .expected_span = 21 + 10},
      {.transit = 22, .expected_span = 22 + 10},
      // First interbreak pushes span to 23 + 10 > 32, second pushes to 23 + 15.
      {.transit = 23, .expected_span = 23 + 15},
      {.transit = 40, .expected_span = 40 + 15},
      {.transit = 41, .expected_span = 41 + 2 * 10},
      {.transit = 44, .expected_span = 44 + 2 * 10},
      // First interbreak pushes span to 45 + 20 > 64,
      // second can only push to 45 + 15.
      {.transit = 45, .expected_span = 45 + 2 * 10},
      {.transit = 60, .expected_span = 60 + 2 * 10},
      {.transit = 61,
       .expected_span = 60 + 3 * 10,
       .is_feasible = false},  // There must be 3 breaks, but only 2 exist.
  };
  // Run all scenarios.
  std::vector<std::pair<int64_t, int64_t>> interbreaks =
      MakeInterbreakLimitsFromInitialScenario(initial_scenario, 1);
  for (const auto& span_scenario : span_scenarios) {
    FillDimensionValuesWithPathScenario(
        {.nodes = {1, 3},
         .cumuls = {{.min = 0, .max = 100}, {.min = 0, .max = 100}},
         .transits = {{.min = span_scenario.transit, .max = kint64max}},
         .travels = {span_scenario.transit}},
        1, *dimension_values);
    dimension_values->MutableVehicleBreaks(1) =
        initial_scenario.vehicle_breaks[1];
    EXPECT_EQ(
        PropagateLightweightVehicleBreaks(1, *dimension_values, interbreaks),
        span_scenario.is_feasible);
    if (span_scenario.is_feasible) {
      ExpectDimensionValuesScenario(
          {.cumuls = {{.min = 0, .max = 100 - span_scenario.expected_span},
                      {.min = span_scenario.expected_span, .max = 100}},
           .span = Interval(span_scenario.expected_span, kint64max)},
          1, *dimension_values);
    }
    dimension_values->Revert();
  }
}

TEST(PropagateLightweightVehicleBreaksTest,
     InterbreakLimitsWithZeroInterbreak) {
  // Make initial instance of DimensionValues with interbreak limits.
  DimensionInitialScenario initial_scenario = MakeDimensionInitialScenario();
  initial_scenario.interbreak_limits = {
      {}, {{.max_interbreak_duration = 0, .min_break_duration = 10}}};
  std::unique_ptr<DimensionValues> dimension_values =
      MakeDimensionValuesFromInitialScenario(initial_scenario,
                                             {{0, 2}, {1, 3}});
  std::vector<std::pair<int64_t, int64_t>> interbreaks =
      MakeInterbreakLimitsFromInitialScenario(initial_scenario, 1);
  // With total travel == 0, propagation should be successful.
  FillDimensionValuesWithPathScenario(
      {.nodes = {1, 3},
       .cumuls = {{.min = 0, .max = 100}, {.min = 0, .max = 100}}},
      1, *dimension_values);
  dimension_values->MutableVehicleBreaks(1) =
      initial_scenario.vehicle_breaks[1];
  EXPECT_TRUE(
      PropagateLightweightVehicleBreaks(1, *dimension_values, interbreaks));
  // With total travel > 0, propagation should fail.
  dimension_values->MutableTravels(1)[0] = 1;
  dimension_values->MutableTravelSums(1)[1] = 1;
  dimension_values->MutableVehicleBreaks(1) =
      initial_scenario.vehicle_breaks[1];
  EXPECT_FALSE(
      PropagateLightweightVehicleBreaks(1, *dimension_values, interbreaks));
}

// There are only two breaks in these scenarios, but their time windows prevent
// them from occurring during the route.
// Depending on the scenario, interbreak limits might force the vehicle to take
// a break. We test that propagation detects those particular infeasibilities.
TEST(PropagateLightweightVehicleBreaksTest, ReasonOnNumFeasibleBreaks) {
  // Make initial instance of DimensionValues with interbreak limits.
  DimensionInitialScenario initial_scenario = MakeDimensionInitialScenario();
  initial_scenario.vehicle_breaks[1][0].start = {.min = 0, .max = 19};
  initial_scenario.vehicle_breaks[1][1].end = {.min = 81, .max = 100};
  initial_scenario.interbreak_limits[1] = {
      {.max_interbreak_duration = 40, .min_break_duration = 10}};
  std::unique_ptr<DimensionValues> dimension_values =
      MakeDimensionValuesFromInitialScenario(initial_scenario,
                                             {{0, 2}, {1, 3}});
  std::vector<std::pair<int64_t, int64_t>> interbreaks =
      MakeInterbreakLimitsFromInitialScenario(initial_scenario, 1);

  // Restrict route time window so that both breaks are infeasible.
  DimensionValuesPathScenario base_scenario = {
      .nodes = {1, 3},
      .cumuls = {{.min = 20, .max = 100}, {.min = 0, .max = 80}},
      .transits = {{.min = 0, .max = kint64max}},
      .travels = {0}};
  std::vector<DimensionValuesPathScenario> scenarios(6, base_scenario);
  std::vector<bool> scenario_is_feasible(6, false);
  // Total travel is at its max before a break is mandatory.
  scenarios[0].travels = {40};
  scenario_is_feasible[0] = true;
  // Total travel is at the min value that makes a break mandatory.
  scenarios[1].travels = {41};
  scenario_is_feasible[1] = false;
  // Span is at its max before a break is mandatory.
  scenarios[2].span = DimensionValues::Interval(40, kint64max);
  scenario_is_feasible[2] = true;
  // Span is at the min value that makes a break mandatory.
  scenarios[3].span = DimensionValues::Interval(41, kint64max);
  scenario_is_feasible[3] = false;
  // end_min - start_max <= max_interbreak.
  scenarios[4].cumuls.front().max = 30;
  scenarios[4].cumuls.back().min = 70;
  scenario_is_feasible[4] = true;
  // end_min - start_max > max_interbreak.
  scenarios[5].cumuls.front().max = 30;
  scenarios[5].cumuls.back().min = 71;
  scenario_is_feasible[5] = false;

  for (int s = 0; s < scenarios.size(); ++s) {
    FillDimensionValuesWithPathScenario(scenarios[s], 1, *dimension_values);
    dimension_values->MutableVehicleBreaks(1) =
        initial_scenario.vehicle_breaks[1];
    EXPECT_EQ(
        PropagateLightweightVehicleBreaks(1, *dimension_values, interbreaks),
        scenario_is_feasible[s]);
    dimension_values->Revert();
  }
}

class LightVehicleBreaksCheckerTest : public ::testing::Test {
 protected:
  std::vector<LightVehicleBreaksChecker::PathData> path_data_;

  void SetUp() override {
    const std::vector<int> starts{0, 2};
    const std::vector<int> ends{1, 3};
    path_state_ = std::make_unique<PathState>(6, starts, ends);

    // Path 0 has no breaks.
    path_data_.push_back({
        .vehicle_breaks = {},
        .start_cumul = ls_state_.MakeVariableWithRelaxedDomain(0, 100),
        .end_cumul = ls_state_.MakeVariableWithRelaxedDomain(0, 100),
        .total_transit = ls_state_.MakeVariableWithRelaxedDomain(0, 100),
        .span = ls_state_.MakeVariableWithRelaxedDomain(0, 100),
    });
    // Path 1 has two breaks.
    path_data_.push_back({
        .vehicle_breaks = {{
                               .start_min = 0,
                               .start_max = 90,
                               .end_min = 10,
                               .end_max = 100,
                               .duration_min = 10,
                               .is_performed_min = true,
                               .is_performed_max = true,
                           },
                           {
                               .start_min = 0,
                               .start_max = 85,
                               .end_min = 15,
                               .end_max = 100,
                               .duration_min = 15,
                               .is_performed_min = true,
                               .is_performed_max = true,
                           }},
        .start_cumul = ls_state_.MakeVariableWithRelaxedDomain(10, 100),
        .end_cumul = ls_state_.MakeVariableWithRelaxedDomain(0, 100),
        .total_transit = ls_state_.MakeVariableWithRelaxedDomain(0, 100),
        .span = ls_state_.MakeVariableWithRelaxedDomain(0, 100),
    });
    ls_state_.CompileConstraints();  // No internal constraints.
  }

  LocalSearchState ls_state_;
  std::unique_ptr<PathState> path_state_;
};

TEST_F(LightVehicleBreaksCheckerTest, CheckerAndFilterAPI) {
  Solver solver("solver");
  enum class Event {
    kChecker,
    kFilterAccept,
    kFilterSynchronize,
  };
  for (const Event event :
       {Event::kChecker, Event::kFilterAccept, Event::kFilterSynchronize}) {
    // Do not std::move() path_data_, we want to keep a copy.
    auto checker = std::make_unique<LightVehicleBreaksChecker>(
        path_state_.get(), path_data_);
    LocalSearchFilter* filter = nullptr;
    if (event != Event::kChecker) {
      filter = MakeLightVehicleBreaksFilter(&solver, std::move(checker), "T");
      checker.reset(nullptr);  // Avoids spurious static analysis error.
    }
    // Simulate Synchronize(), change variables of both paths.
    for (int p = 0; p < 2; ++p) {
      path_data_[p].start_cumul.Relax();
      path_data_[p].start_cumul.SetMin(20);
      path_data_[p].end_cumul.Relax();
      path_data_[p].end_cumul.SetMax(80);
      path_data_[p].total_transit.Relax();
      path_data_[p].total_transit.SetMin(30);
      path_data_[p].span.Relax();
      path_data_[p].span.SetMin(50);
    }
    ls_state_.Commit();

    // No change in path_state_, checker should not relax any variable.
    if (event != Event::kChecker) {
      filter->Relax(nullptr, nullptr);
    } else {
      checker->Relax();
    }

    for (int p = 0; p < 2; ++p) {
      EXPECT_EQ(path_data_[p].start_cumul.Min(), 20);
      EXPECT_EQ(path_data_[p].end_cumul.Max(), 80);
      EXPECT_EQ(path_data_[p].total_transit.Min(), 30);
      EXPECT_EQ(path_data_[p].span.Min(), 50);
    }
    // If PathState has changed a path, relax corresponding variable(s).
    const int start1_index = path_state_->CommittedIndex(2);
    const int end1_index = path_state_->CommittedIndex(3);
    const int node_index = path_state_->CommittedIndex(4);
    path_state_->ChangePath(1, {{start1_index, start1_index + 1},
                                {node_index, node_index + 1},
                                {end1_index, end1_index + 1}});
    if (event != Event::kChecker) {
      filter->Relax(nullptr, nullptr);
    } else {
      checker->Relax();
    }
    // Variables of path 0 are not relaxed.
    EXPECT_EQ(path_data_[0].start_cumul.Min(), 20);
    EXPECT_EQ(path_data_[0].end_cumul.Max(), 80);
    EXPECT_EQ(path_data_[0].total_transit.Min(), 30);
    EXPECT_EQ(path_data_[0].span.Min(), 50);
    // Variables of path 1 are relaxed, except for total_transit.
    EXPECT_EQ(path_data_[1].start_cumul.Min(), 10);
    EXPECT_EQ(path_data_[1].end_cumul.Max(), 100);
    EXPECT_EQ(path_data_[1].total_transit.Min(), 30);  // This is not relaxed.
    EXPECT_EQ(path_data_[1].span.Min(), 0);
    // Breaks may be performed outside the path's relaxed time window,
    // but the span should still be at least total_transit.
    switch (event) {
      case Event::kChecker:
        EXPECT_TRUE(checker->Check());
        break;
      case Event::kFilterAccept:
        EXPECT_TRUE(filter->Accept(nullptr, nullptr, 0, 0));
        break;
      case Event::kFilterSynchronize:
        filter->Synchronize(nullptr, nullptr);
        break;
    }
    EXPECT_EQ(path_data_[1].span.Min(), 30);
    path_state_->Revert();
    ls_state_.Revert();
  }
}

TEST_F(LightVehicleBreaksCheckerTest, RelaxAndCheckMandatoryBreaks) {
  // First break during [40, 55), second break during [50, 75).
  path_data_[1].vehicle_breaks[0].start_min = 40;
  path_data_[1].vehicle_breaks[0].end_min = 50;
  path_data_[1].vehicle_breaks[0].start_max = 45;
  path_data_[1].vehicle_breaks[0].end_max = 55;
  path_data_[1].vehicle_breaks[1].start_min = 50;
  path_data_[1].vehicle_breaks[1].end_min = 65;
  path_data_[1].vehicle_breaks[1].start_max = 60;
  path_data_[1].vehicle_breaks[1].end_max = 75;
  path_data_[1].vehicle_breaks.push_back(path_data_[1].vehicle_breaks[1]);
  path_data_[1].vehicle_breaks[2].is_performed_min = false;
  // Do not std::move() path_data_, we want to keep a copy.
  LightVehicleBreaksChecker checker(path_state_.get(), path_data_);

  struct ExpectedSpanForScenario {
    int64_t start;
    int64_t end;
    int64_t transit;
    int64_t expected_span;
    int64_t expected_start_max;
    int64_t expected_end_min;
  };
  const std::vector<ExpectedSpanForScenario> scenarios{
      {.start = 40,
       .end = 75,
       .transit = 30,
       .expected_span = 30 + 10 + 15,
       .expected_start_max = 40,  // < end_max - 30 - 10 - 15,
       .expected_end_min = 75},   // > start_min + 30 + 10 + 15
      {.start = 49,
       .end = 61,
       .transit = 30,
       .expected_span = 30 + 10 + 15,
       .expected_start_max = 45,  // == end_max - 30 - 10 - 15
       .expected_end_min = 65},   // == start_min + 30 + 10 + 15},
      {.start = 50,
       .end = 61,
       .transit = 30,
       .expected_span = 30 + 0 + 15,
       .expected_start_max = 50,  // < end_max - 30 - 15,
       .expected_end_min = 65},   // vehicle_breaks[1].end_min
      {.start = 49,
       .end = 60,
       .transit = 29,
       .expected_span = 29 + 10 + 0,
       // not end_max - 29 - 10, instead vehicle_breaks[0].start_max
       .expected_start_max = 45,
       .expected_end_min = 60},  // max(end = 60, start_min + 29 + 10)
      {.start = 50,
       .end = 60,
       .transit = 30,
       .expected_span = 30 + 0 + 0,
       .expected_start_max = 50,  // < end_max - 30
       .expected_end_min = 60},   // > start_min + 30},
  };

  for (const bool feasible : {true, false}) {
    for (const auto& scenario : scenarios) {
      // Change path 1.
      const int start1_index = path_state_->CommittedIndex(2);
      const int end1_index = path_state_->CommittedIndex(3);
      const int node_index = path_state_->CommittedIndex(4);
      path_state_->ChangePath(1, {{start1_index, start1_index + 1},
                                  {node_index, node_index + 1},
                                  {end1_index, end1_index + 1}});
      // Simulate Relax phase.
      path_data_[1].start_cumul.Relax();
      path_data_[1].end_cumul.Relax();
      path_data_[1].total_transit.Relax();
      path_data_[1].span.Relax();
      checker.Relax();
      // Simulate time checker tightening some variables.
      path_data_[1].start_cumul.SetMax(scenario.start);
      path_data_[1].end_cumul.SetMin(scenario.end);
      path_data_[1].total_transit.SetMin(scenario.transit);
      path_data_[1].span.SetMax(scenario.expected_span - (feasible ? 0 : 1));
      // Check that vehicle break checker generates correct span bound.
      EXPECT_EQ(checker.Check(), feasible);
      if (feasible) {
        EXPECT_EQ(path_data_[1].span.Min(), scenario.expected_span);
        EXPECT_EQ(path_data_[1].start_cumul.Max(), scenario.expected_start_max);
        EXPECT_EQ(path_data_[1].end_cumul.Min(), scenario.expected_end_min);
      }
      // Revert for next scenario.
      path_state_->Revert();
      ls_state_.Revert();
    }
  }
}

TEST_F(LightVehicleBreaksCheckerTest, InterbreakLimits) {
  path_data_[1].interbreak_limits.push_back(
      {.max_interbreak_duration = 20, .min_break_duration = 10});
  path_data_[1].interbreak_limits.push_back(
      {.max_interbreak_duration = 32, .min_break_duration = 15});
  // Do not std::move() path_data_, we want to keep a copy.
  LightVehicleBreaksChecker checker(path_state_.get(), path_data_);

  struct ExpectedSpanForScenario {
    int64_t transit = 0;
    int64_t expected_span = 0;
    bool is_feasible = true;
  };
  const std::vector<ExpectedSpanForScenario> scenarios{
      {.transit = 20, .expected_span = 20},
      {.transit = 21, .expected_span = 21 + 10},
      {.transit = 22, .expected_span = 22 + 10},
      // First interbreak pushes span to 23 + 10 > 32, second pushes to 23 + 15.
      {.transit = 23, .expected_span = 23 + 15},
      {.transit = 40, .expected_span = 40 + 15},
      {.transit = 41, .expected_span = 41 + 2 * 10},
      {.transit = 44, .expected_span = 44 + 2 * 10},
      // First interbreak pushes span to 45 + 20 > 64,
      // second can only push to 45 + 15.
      {.transit = 45, .expected_span = 45 + 2 * 10},
      {.transit = 60, .expected_span = 60 + 2 * 10},
      {.transit = 61,
       .expected_span = 60 + 3 * 10,
       .is_feasible = false},  // There must be 3 breaks, but only 2 exist.
  };

  for (const bool feasible : {true, false}) {
    for (const auto& scenario : scenarios) {
      SCOPED_TRACE(absl::StrCat("Scenario feasible: ", feasible,
                                " transit: ", scenario.transit,
                                " expected_span: ", scenario.expected_span));
      // Change path 1.
      const int start1_index = path_state_->CommittedIndex(2);
      const int end1_index = path_state_->CommittedIndex(3);
      const int node_index = path_state_->CommittedIndex(4);
      path_state_->ChangePath(1, {{start1_index, start1_index + 1},
                                  {node_index, node_index + 1},
                                  {end1_index, end1_index + 1}});
      // Simulate Relax phase.
      path_data_[1].start_cumul.Relax();
      path_data_[1].end_cumul.Relax();
      path_data_[1].total_transit.Relax();
      path_data_[1].span.Relax();
      checker.Relax();
      // Simulate time checker tightening some variables.
      path_data_[1].total_transit.SetMin(scenario.transit);
      path_data_[1].span.SetMax(scenario.expected_span - (feasible ? 0 : 1));
      // Check that vehicle break checker generates correct span bound.
      EXPECT_EQ(checker.Check(), feasible && scenario.is_feasible);
      if (feasible && scenario.is_feasible) {
        EXPECT_EQ(path_data_[1].span.Min(), scenario.expected_span);
      }
      // Revert for next scenario.
      path_state_->Revert();
      ls_state_.Revert();
    }
  }
}

TEST_F(LightVehicleBreaksCheckerTest, InterbreakLimitsZeroInterbreak) {
  path_data_[1].interbreak_limits.push_back(
      {.max_interbreak_duration = 0, .min_break_duration = 10});
  // Do not std::move() path_data_, we want to keep a copy.
  LightVehicleBreaksChecker checker(path_state_.get(), path_data_);

  // Change path 1.
  const int start1_index = path_state_->CommittedIndex(2);
  const int end1_index = path_state_->CommittedIndex(3);
  const int node_index = path_state_->CommittedIndex(4);
  path_state_->ChangePath(1, {{start1_index, start1_index + 1},
                              {node_index, node_index + 1},
                              {end1_index, end1_index + 1}});
  // Simulate Relax phase.
  path_data_[1].start_cumul.Relax();
  path_data_[1].end_cumul.Relax();
  path_data_[1].total_transit.Relax();
  path_data_[1].span.Relax();
  checker.Relax();
  // Transit == 0 should be fine with interbreak == 0.
  path_data_[1].total_transit.SetMin(0);
  EXPECT_TRUE(checker.Check());
  // Transit == 1 is incompatible with interbreak == 0.
  path_data_[1].total_transit.SetMin(1);
  EXPECT_FALSE(checker.Check());
}

TEST_F(LightVehicleBreaksCheckerTest, ReasonOnNumFeasibleBreaks) {
  // Interbreak limits:
  // - force #breaks >= 1 through total_transits, span or time windows.
  // - none available because first must be before, second must be after.
  path_data_[1].interbreak_limits.push_back(
      {.max_interbreak_duration = 40, .min_break_duration = 10});
  path_data_[1].vehicle_breaks[0].start_min = 0;
  path_data_[1].vehicle_breaks[0].start_max = 19;
  path_data_[1].vehicle_breaks[1].end_min = 81;
  path_data_[1].vehicle_breaks[1].end_max = 100;
  // Do not std::move() path_data_, we want to keep a copy.
  LightVehicleBreaksChecker checker(path_state_.get(), path_data_);
  // Restrict total_transit, span or route time window, depending on scenario.
  for (int s = 0; s < 6; ++s) {
    // Change path 1.
    const int start1_index = path_state_->CommittedIndex(2);
    const int end1_index = path_state_->CommittedIndex(3);
    const int node_index = path_state_->CommittedIndex(4);
    path_state_->ChangePath(1, {{start1_index, start1_index + 1},
                                {node_index, node_index + 1},
                                {end1_index, end1_index + 1}});
    // Simulate Relax phase.
    path_data_[1].start_cumul.Relax();
    path_data_[1].end_cumul.Relax();
    path_data_[1].total_transit.Relax();
    path_data_[1].span.Relax();
    checker.Relax();
    // Restrict route time window so that both breaks are infeasible.
    path_data_[1].start_cumul.SetMin(20);
    path_data_[1].end_cumul.SetMax(80);
    // Apply scenario.
    SCOPED_TRACE(absl::StrCat("Scenario ", s));
    bool feasible = true;
    switch (s) {
      case 0:
        EXPECT_TRUE(path_data_[1].total_transit.SetMin(40));
        break;
      case 1:
        EXPECT_TRUE(path_data_[1].total_transit.SetMin(41));
        feasible = false;
        break;
      case 2:
        EXPECT_TRUE(path_data_[1].span.SetMin(40));
        break;
      case 3:
        EXPECT_TRUE(path_data_[1].span.SetMin(41));
        feasible = false;
        break;
      case 4:
        // end_min - start_max <= max_interbreak.
        EXPECT_TRUE(path_data_[1].start_cumul.SetMax(30));
        EXPECT_TRUE(path_data_[1].end_cumul.SetMin(70));
        break;
      case 5:
        // end_min - start_max > max_interbreak.
        EXPECT_TRUE(path_data_[1].start_cumul.SetMax(30));
        EXPECT_TRUE(path_data_[1].end_cumul.SetMin(71));
        feasible = false;
        break;
      default:
        FAIL() << "Invalid scenario " << s;
    }
    EXPECT_EQ(checker.Check(), feasible);
    if (feasible) {
      EXPECT_GE(40, path_data_[1].span.Min());
    }
    // Revert for next scenario.
    path_state_->Revert();
    ls_state_.Revert();
  }
}

bool ChangePathsAndExpectEnergyCost(
    PathState& state, PathEnergyCostChecker& checker,
    absl::Span<const std::vector<int64_t>> paths, int64_t expected_cost) {
  const int num_paths = paths.size();
  std::vector<PathState::ChainBounds> bounds;
  for (int p = 0; p < num_paths; ++p) {
    bounds.clear();
    for (const int node : paths[p]) {
      const int index = state.CommittedIndex(node);
      // Merge individual changes into chains.
      if (bounds.empty() || bounds.back().end_index != index) {
        bounds.push_back({index, -1});
      }
      bounds.back().end_index = index + 1;
    }
    state.ChangePath(p, bounds);
  }
  bool ok = true;
  EXPECT_TRUE(checker.Check()) << (ok = false);
  EXPECT_EQ(checker.AcceptedCost(), expected_cost) << (ok = false);
  checker.Commit();
  state.Commit();
  EXPECT_EQ(checker.CommittedCost(), expected_cost) << (ok = false);
  return ok;
}

struct EnergyCostSpecification {
  std::vector<int64_t> force_start_min;
  std::vector<int64_t> force_end_min;
  std::vector<int> force_class;
  std::vector<int> distance_class;
  std::vector<const std::function<int64_t(int64_t)>*> force_per_class;
  std::vector<const std::function<int64_t(int64_t, int64_t)>*>
      distance_per_class;
  std::vector<PathEnergyCostChecker::EnergyCost> path_energy_costs;
  std::vector<bool> path_has_cost_when_empty;
};

int64_t PathsEnergyCost(const EnergyCostSpecification& spec,
                        absl::Span<const std::vector<int64_t>> paths) {
  int64_t energy_cost = 0;
  const int num_paths = paths.size();
  for (int p = 0; p < num_paths; ++p) {
    if (paths[p].size() == 2 && !spec.path_has_cost_when_empty[p]) continue;
    const auto& cost = spec.path_energy_costs[p];
    const auto& force = *spec.force_per_class[spec.force_class[p]];
    const auto& distance = *spec.distance_per_class[spec.distance_class[p]];
    int64_t total_force = spec.force_start_min[p];
    // Find minimum total force on path if starting from 0.
    int64_t min_total_force = total_force;
    for (const int node : paths[p]) {
      total_force += force(node);
      min_total_force = std::min(min_total_force, total_force);
    }
    // Compute energy from 0.
    int64_t energy_below = 0;
    int64_t energy_above = 0;
    // Offset total_force upwards if force would go down to 0.
    total_force = std::max<int64_t>(
        {0, -min_total_force, spec.force_end_min[p] - total_force});
    total_force += spec.force_start_min[p];
    const int num_positions = paths[p].size();
    for (int pos = 1; pos < num_positions; ++pos) {
      total_force += force(paths[p][pos - 1]);
      const int64_t d = distance(paths[p][pos - 1], paths[p][pos]);
      energy_below += std::min(cost.threshold, total_force) * d;
      energy_above += std::max<int64_t>(0, total_force - cost.threshold) * d;
    }
    energy_cost += cost.cost_per_unit_below_threshold * energy_below;
    energy_cost += cost.cost_per_unit_above_threshold * energy_above;
  }
  return energy_cost;
}

TEST(PathsEnergyCostTest, SimpleCases) {
  std::function<int64_t(int64_t)> force_0 = [](int64_t node) -> int64_t {
    return node < 4 ? 0 : -2 * node;
  };
  std::function<int64_t(int64_t)> force_1 = [](int64_t node) -> int64_t {
    return node < 4 ? 0 : -3 * node;
  };
  std::function<int64_t(int64_t, int64_t)> distance_0 =
      [](int64_t a, int64_t b) -> int64_t { return a == b ? 0 : 5; };
  std::function<int64_t(int64_t, int64_t)> distance_1 =
      [](int64_t a, int64_t b) -> int64_t { return a == b ? 0 : 7; };

  const EnergyCostSpecification specification{
      .force_start_min = {0, 0},
      .force_end_min = {0, 0},
      .force_class = {1, 0},
      .distance_class = {1, 0},
      .force_per_class = {&force_0, &force_1},
      .distance_per_class = {&distance_0, &distance_1},
      .path_energy_costs = {{0, 0, 11}, {0, 0, 13}},
      .path_has_cost_when_empty = {false, true},
  };

  // Expected value is force * distance * cost + force * distance * cost + ...
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 4, 0}}), 12 * 7 * 11);
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 0}, {2, 4, 3}}), 8 * 5 * 13);
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 5, 0}}), 15 * 7 * 11);
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 4, 0}, {2, 5, 3}}),
            12 * 7 * 11 + 10 * 5 * 13);
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 4, 6, 5, 0}}),
            45 * 7 * 11 + 33 * 7 * 11 + 15 * 7 * 11);
}

TEST(PathsEnergyCostTest, SoftMax) {
  std::function<int64_t(int64_t)> force = [](int64_t) -> int64_t { return 1; };
  std::function<int64_t(int64_t, int64_t)> distance =
      [](int64_t a, int64_t b) -> int64_t { return a == b ? 0 : 1; };

  const EnergyCostSpecification specification{
      .force_start_min = {1, 1},
      .force_end_min = {0, 0},
      .force_class = {1, 0},
      .distance_class = {1, 0},
      .force_per_class = {&force, &force},
      .distance_per_class = {&distance, &distance},
      .path_energy_costs = {{1, 0, 1}, {2, 0, 3}},
      .path_has_cost_when_empty = {false, true},
  };

  // Force cumuls are {1, 2, 3}; 2 and 3 units are carried over 1 distance.
  // Force above max (violations) are 2 - 1 and 3 - 1, total violation is 3.
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 4, 0}}), 3 * 1);
  // Force cumuls are {1, 2} and {1, 2, 3}.
  // Total violations are 1 and 1, but first path is empty, thus free.
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 0}, {2, 4, 3}}), 0 + 1 * 3);
  // Force cumuls are {1, 2, 3} and {1, 2, 3, 4}.
  // Total violations are 3 and 3.
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 4, 0}, {2, 5, 6, 3}}),
            3 * 1 + 3 * 3);
  // Force cumuls are {1, 2, 3, 4, 5}.
  // Total violation is 10.
  EXPECT_EQ(PathsEnergyCost(specification, {{1, 4, 6, 5, 0}}), 10 * 1);
}

using EnergyCostCheckerTest = ::testing::TestWithParam<bool>;

// Monotonic dimension, simple case.

TEST_P(EnergyCostCheckerTest, MonotonicDimension) {
  const int num_nodes = 10;
  std::vector<int> path_start{0, 2};
  std::vector<int> path_end{1, 3};
  auto state = std::make_unique<PathState>(num_nodes, path_start, path_end);
  const bool opposite = GetParam();

  std::function<int64_t(int64_t)> force_0 =
      [opposite](int64_t node) -> int64_t {
    const int64_t force = node < 4 ? 0 : (int64_t{1} << (node - 4));
    return opposite ? -force : force;
  };
  std::function<int64_t(int64_t)> force_1 =
      [opposite](int64_t node) -> int64_t {
    const int64_t force = node < 4 ? 0 : ((int64_t{1} << (node - 4)) + 7);
    return opposite ? -force : force;
  };
  std::function<int64_t(int64_t, int64_t)> distance_0 =
      [](int64_t a, int64_t b) -> int64_t { return a == b ? 0 : a * b; };
  std::function<int64_t(int64_t, int64_t)> distance_1 =
      [](int64_t a, int64_t b) -> int64_t { return a == b ? 0 : a * b + 1; };

  const EnergyCostSpecification specification{
      .force_start_min = {0, 0},
      .force_end_min = {0, 0},
      .force_class = {1, 0},
      .distance_class = {1, 0},
      .force_per_class = {&force_0, &force_1},
      .distance_per_class = {&distance_0, &distance_1},
      .path_energy_costs = {{5, 777, 9}, {3, 1111, 2}},
      .path_has_cost_when_empty = {true, true},
  };

  auto checker = std::make_unique<PathEnergyCostChecker>(
      state.get(), specification.force_start_min, specification.force_end_min,
      specification.force_class, specification.force_per_class,
      specification.distance_class, specification.distance_per_class,
      specification.path_energy_costs, specification.path_has_cost_when_empty);

  EXPECT_EQ(checker->CommittedCost(), 0);
  const std::vector<std::vector<std::vector<int64_t>>> scenarios = {
      {{0, 4, 1}, {2, 3}},
      {{0, 4, 5, 6, 7, 8, 9, 1}, {2, 3}},
      {{0, 1}, {2, 4, 5, 6, 7, 8, 9, 3}},
      {{0, 7, 8, 9, 4, 5, 6, 1}, {2, 3}},
      {{0, 7, 6, 1}, {2, 4, 5, 8, 9, 3}},
  };
  for (const auto& paths : scenarios) {
    const int64_t expected_cost = PathsEnergyCost(specification, paths);
    EXPECT_TRUE(
        ChangePathsAndExpectEnergyCost(*state, *checker, paths, expected_cost));
  }
}

INSTANTIATE_TEST_SUITE_P(Monotonic, EnergyCostCheckerTest, ::testing::Bool());

TEST_P(EnergyCostCheckerTest, WaveShapedLoad) {
  const int num_nodes = 12;
  std::vector<int> path_start{0, 2};
  std::vector<int> path_end{1, 3};
  auto state = std::make_unique<PathState>(num_nodes, path_start, path_end);
  const bool opposite = GetParam();
  std::function<int64_t(int64_t)> force_0 =
      [opposite](int64_t node) -> int64_t {
    const int64_t force = node < 4 ? 0 : (node % 2 == 0);
    return opposite ? -force : force;
  };
  std::function<int64_t(int64_t)> force_1 =
      [opposite](int64_t node) -> int64_t {
    const int64_t force = 2 * (node < 4 ? 0 : (node % 2 == 0));
    return opposite ? -force : force;
  };
  std::function<int64_t(int64_t, int64_t)> distance_0 =
      [](int64_t a, int64_t b) -> int64_t { return a == b ? 0 : a * b; };
  std::function<int64_t(int64_t, int64_t)> distance_1 =
      [](int64_t a, int64_t b) -> int64_t { return a == b ? 0 : a * b + 1; };

  const EnergyCostSpecification specification{
      .force_start_min = {0, 0},
      .force_end_min = {0, 0},
      .force_class = {1, 0},
      .distance_class = {1, 0},
      .force_per_class = {&force_0, &force_1},
      .distance_per_class = {&distance_0, &distance_1},
      .path_energy_costs = {{5, 777, 9}, {3, 1111, 2}},
      .path_has_cost_when_empty = {true, true},
  };

  auto checker = std::make_unique<PathEnergyCostChecker>(
      state.get(), specification.force_start_min, specification.force_end_min,
      specification.force_class, specification.force_per_class,
      specification.distance_class, specification.distance_per_class,
      specification.path_energy_costs, specification.path_has_cost_when_empty);

  EXPECT_EQ(checker->CommittedCost(), 0);
  const std::vector<std::vector<std::vector<int64_t>>> scenarios = {
      {{0, 4, 5, 1}, {2, 3}},
      {{0, 4, 5, 10, 11, 1}, {2, 6, 8, 7, 9, 3}},
      {{0, 4, 8, 7, 5, 1}, {2, 10, 11, 6, 9, 3}},
      {{0, 6, 9, 4, 8, 7, 5, 1}, {2, 10, 11, 3}},
      {{0, 1}, {2, 8, 7, 6, 9, 10, 4, 5, 11, 3}},
  };
  for (const auto& paths : scenarios) {
    const int64_t expected_cost = PathsEnergyCost(specification, paths);
    EXPECT_TRUE(
        ChangePathsAndExpectEnergyCost(*state, *checker, paths, expected_cost));
  }
}

INSTANTIATE_TEST_SUITE_P(WaveShaped, EnergyCostCheckerTest, ::testing::Bool());

TEST(EnergyCostCheckerTest, StartEndForce) {
  const int num_nodes = 12;
  std::vector<int> path_start{0, 2, 4};
  std::vector<int> path_end{1, 3, 5};
  auto state = std::make_unique<PathState>(num_nodes, path_start, path_end);
  std::function<int64_t(int64_t)> force = [](int64_t node) -> int64_t {
    if (node == 0) return 10;
    if (node < 6) return 0;
    return 2 * (node % 2) - 1;
  };
  std::function<int64_t(int64_t, int64_t)> distance =
      [](int64_t a, int64_t b) -> int64_t { return a == b ? 0 : 1; };

  const EnergyCostSpecification specification{
      .force_start_min = {10, 0, 0},
      .force_end_min = {0, 10, 0},
      .force_class = {0, 0, 0},
      .distance_class = {0, 0, 0},
      .force_per_class = {&force},
      .distance_per_class = {&distance},
      .path_energy_costs = {{0, 0, 1}, {0, 0, 1}, {0, 0, 1}},
      .path_has_cost_when_empty = {false, false, false},
  };

  auto checker = std::make_unique<PathEnergyCostChecker>(
      state.get(), specification.force_start_min, specification.force_end_min,
      specification.force_class, specification.force_per_class,
      specification.distance_class, specification.distance_per_class,
      specification.path_energy_costs, specification.path_has_cost_when_empty);

  EXPECT_EQ(checker->CommittedCost(), 0);
  const std::vector<std::vector<std::vector<int64_t>>> scenarios = {
      {{0, 1}, {2, 3}, {4, 5}},
      {{0, 6, 8, 10, 7, 9, 11, 1}, {2, 3}, {4, 5}},
      {{0, 1}, {2, 6, 8, 10, 7, 9, 11, 3}, {4, 5}},
      {{0, 1}, {2, 3}, {4, 6, 8, 10, 7, 9, 11, 5}},
  };
  for (const auto& paths : scenarios) {
    const int64_t expected_cost = PathsEnergyCost(specification, paths);
    EXPECT_TRUE(
        ChangePathsAndExpectEnergyCost(*state, *checker, paths, expected_cost));
  }
}

// Straightforward algorithm that computes queries q(h, first_index, last_index)
// of an array of elements with height/weight fields E, with value
// sum_{first_index <= i <= last_index} E[i].height >= h ? E[i].weight : 0.
// Packaged in a class for convenience.
class WeightedRangeSumWithThreshold {
 public:
  WeightedRangeSumWithThreshold() = default;

  void Clear() {
    heights_.clear();
    weights_.clear();
    tree_elements_end_ = 0;
  }

  int Size() { return tree_elements_end_; }

  void MakeTreeFromNewElements() { tree_elements_end_ = heights_.size(); }

  void PushBack(int64_t height, int64_t weight) {
    heights_.push_back(height);
    weights_.push_back(weight);
  }

  int64_t RangeSumWithThreshold(int64_t threshold, int range_begin_index,
                                int range_end_index) const {
    int64_t sum = 0;
    for (int i = range_begin_index; i < range_end_index; ++i) {
      if (threshold <= heights_[i]) sum += weights_[i];
    }
    return sum;
  }

 private:
  std::vector<int64_t> heights_;
  std::vector<int64_t> weights_;
  int tree_elements_end_ = 0;
};

// Test both WeightedRangeSumWithThreshold and WeightedWaveletTree with
// hard values, further tests will compare the results of WeightedWaveletTree
// against the simpler WeightedRangeSumWithThreshold.
TEST(WeightedRangeSums, SimpleUseCase) {
  WeightedRangeSumWithThreshold wrst;
  WeightedWaveletTree tree;

  EXPECT_EQ(wrst.Size(), 0);
  EXPECT_EQ(tree.TreeSize(), 0);
  // Making a tree with no elements must not crash.
  tree.MakeTreeFromNewElements();

  const std::vector<int64_t> heights1 = {6, 4, 1, 3};
  const std::vector<int64_t> weights1 = {2, 4, 1, 6};
  for (int i = 0; i < heights1.size(); ++i) {
    wrst.PushBack(heights1[i], weights1[i]);
    tree.PushBack(heights1[i], weights1[i]);
  }
  EXPECT_EQ(wrst.Size(), 0);
  EXPECT_EQ(tree.TreeSize(), 0);
  wrst.MakeTreeFromNewElements();
  tree.MakeTreeFromNewElements();
  // Making a tree with no elements must not crash.
  tree.MakeTreeFromNewElements();
  EXPECT_EQ(wrst.Size(), heights1.size());
  EXPECT_EQ(tree.TreeSize(), heights1.size());
  EXPECT_EQ(wrst.RangeSumWithThreshold(3, 0, heights1.size()), 12);
  EXPECT_EQ(tree.RangeSumWithThreshold(3, 0, heights1.size()), 12);
  // Trivial case: threshold above all heights.
  EXPECT_EQ(wrst.RangeSumWithThreshold(7, 0, 3), 0);
  EXPECT_EQ(tree.RangeSumWithThreshold(7, 0, 3), 0);
  // Trivial case: threshold below all heights.
  EXPECT_EQ(wrst.RangeSumWithThreshold(1, 1, 3), 5);
  EXPECT_EQ(tree.RangeSumWithThreshold(1, 1, 3), 5);
  // 1-sized range, element above threshold.
  EXPECT_EQ(wrst.RangeSumWithThreshold(5, 0, 1), 2);
  EXPECT_EQ(tree.RangeSumWithThreshold(5, 0, 1), 2);
  // 1-sized range, element below threshold.
  EXPECT_EQ(wrst.RangeSumWithThreshold(5, 1, 2), 0);
  EXPECT_EQ(tree.RangeSumWithThreshold(5, 1, 2), 0);

  // Add another range, check that both ranges can be queried.
  // Then remove previous range and recheck.
  for (int iteration = 0; iteration < 2; ++iteration) {
    const int size1 = wrst.Size();
    const std::vector<int64_t> heights2 = {-4, 2, -7, -9};
    const std::vector<int64_t> weights2 = {-12, 1, -3, 4};
    for (int i = 0; i < heights2.size(); ++i) {
      wrst.PushBack(heights2[i], weights2[i]);
      tree.PushBack(heights2[i], weights2[i]);
    }
    EXPECT_EQ(wrst.Size(), (1 - iteration) * heights1.size());
    EXPECT_EQ(tree.TreeSize(), (1 - iteration) * heights1.size());
    wrst.MakeTreeFromNewElements();
    tree.MakeTreeFromNewElements();
    EXPECT_EQ(wrst.Size(), size1 + heights2.size());
    EXPECT_EQ(tree.TreeSize(), size1 + heights2.size());

    EXPECT_EQ(wrst.RangeSumWithThreshold(-7, size1 + 1, size1 + 3), -2);
    EXPECT_EQ(tree.RangeSumWithThreshold(-7, size1 + 1, size1 + 3), -2);
    // Clear data structures, next iteration will re-add second range.
    wrst.Clear();
    tree.Clear();
  }
}

struct HeightWeight {
  int64_t height;
  int64_t weight;
  bool operator<(const HeightWeight& other) const {
    return std::tie(height, weight) < std::tie(other.height, other.weight);
  }
};

using HeightWeightSequences = std::vector<std::vector<HeightWeight>>;

class WeightedRangeSumWithThresholdTest
    : public ::testing::TestWithParam<HeightWeightSequences> {
 public:
  void SetUp() override {
    for (const auto& sequence : GetParam()) {
      const int begin_index = tree_.TreeSize();
      for (int i = 0; i < sequence.size(); ++i) {
        wrst_.PushBack(sequence[i].height, sequence[i].weight);
        tree_.PushBack(sequence[i].height, sequence[i].weight);
      }
      wrst_.MakeTreeFromNewElements();
      tree_.MakeTreeFromNewElements();

      int64_t min_height = 0;
      int64_t max_height = 1;
      if (!sequence.empty()) {
        // Test whole height range of queries, and values just outside range.
        min_height = CapSub(
            std::min_element(sequence.begin(), sequence.end())->height, 1);
        max_height = CapAdd(
            std::max_element(sequence.begin(), sequence.end())->height, 1);
      }
      queries_.push_back({
          .begin_index = begin_index,
          .end_index = tree_.TreeSize(),
          .min_threshold = min_height,
          .max_threshold = max_height,
      });
      CheckAllRangeQueries(wrst_, tree_, queries_.back());
    }
  }

  void CheckAllSequences() {
    for (const SequenceQueries& query : queries_) {
      CheckAllRangeQueries(wrst_, tree_, query);
    }
  }

 private:
  struct SequenceQueries {
    int begin_index;
    int end_index;
    int64_t min_threshold;
    int64_t max_threshold;
  };
  std::vector<SequenceQueries> queries_;

  void CheckAllRangeQueries(const WeightedRangeSumWithThreshold& wrst,
                            const WeightedWaveletTree& tree,
                            const SequenceQueries& queries) {
    // Min/max thresholds can be kint64min/kint64max, be careful with the loop.
    int64_t t = queries.min_threshold;
    while (true) {
      for (int i = queries.begin_index; i < queries.end_index; ++i) {
        for (int j = i; j <= queries.end_index; ++j) {
          EXPECT_EQ(wrst.RangeSumWithThreshold(t, i, j),
                    tree.RangeSumWithThreshold(t, i, j));
        }
      }
      if (t < queries.max_threshold) {
        ++t;
      } else {
        break;
      }
    }
  }

  WeightedRangeSumWithThreshold wrst_;
  WeightedWaveletTree tree_;
};

TEST_P(WeightedRangeSumWithThresholdTest, CheckAll) { CheckAllSequences(); }

INSTANTIATE_TEST_SUITE_P(
    ComparingWithStraightForwardImplementation,
    WeightedRangeSumWithThresholdTest,
    ::testing::Values(
        HeightWeightSequences{{{0, 2}, {7, 4}, {2, 1}},
                              {{1, 6}, {7, 3}, {4, 5}, {2, 7}, {6, 4}, {1, 2}},
                              {{2, 2}, {7, 1}, {3, 5}, {6, 7}, {6, 2}, {3, 6}}},
        HeightWeightSequences{{{2, 5},
                               {4, 5},
                               {1, 5},
                               {6, 5},
                               {3, 5},
                               {5, 5},
                               {7, 5},
                               {4, 5},
                               {2, 5},
                               {2, 5},
                               {1, 5},
                               {5, 5},
                               {7, 5},
                               {2, 5},
                               {6, 5}}},
        HeightWeightSequences{{{2, 5},
                               {4, 1},
                               {1, 5},
                               {6, 5},
                               {3, 1},
                               {5, 1},
                               {7, 5},
                               {4, 1},
                               {2, 5},
                               {2, 5},
                               {1, 5},
                               {5, 1},
                               {7, 5},
                               {2, 5},
                               {6, 1}}},
        HeightWeightSequences{{{2, 3},
                               {4, 1},
                               {1, 5},
                               {6, 3},
                               {3, 3},
                               {5, 1},
                               {7, 5},
                               {4, 3},
                               {2, 5},
                               {2, 5},
                               {1, 3},
                               {5, 1},
                               {7, 5},
                               {2, 3},
                               {6, 1}}},
        HeightWeightSequences{{{6, 2},
                               {4, 4},
                               {1, 1},
                               {3, 6},
                               {6, 3},
                               {1, 5},
                               {7, 7},
                               {4, 4},
                               {2, 2}}},
        HeightWeightSequences{{{0, 2},
                               {7, 4},
                               {2, 1},
                               {1, 6},
                               {7, 3},
                               {4, 5},
                               {2, 7},
                               {6, 4},
                               {1, 2},
                               {2, 2},
                               {7, 1},
                               {3, 5},
                               {6, 7},
                               {6, 2},
                               {3, 6}}},
        HeightWeightSequences{{{kint64min, 3}, {kint64min + 1, 5}}},
        HeightWeightSequences{{{kint64max, 3}, {kint64max - 1, 5}}}));

TEST(WeightedWaveletTreeDeathTest, ReversedRange) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  WeightedWaveletTree tree;
  tree.PushBack(1, 1);
  tree.PushBack(2, 2);
  tree.MakeTreeFromNewElements();
  EXPECT_EQ(tree.RangeSumWithThreshold(/*threshold*/ 1, 1, 1), 0);
  EXPECT_DEBUG_DEATH(tree.RangeSumWithThreshold(/*threshold*/ 1, 1, 0), ".*");
}

TEST(WeightedWaveletTreeDeathTest, EndIndexWithinSequence) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  WeightedWaveletTree tree;
  tree.PushBack(1, 1);
  tree.PushBack(2, 2);
  tree.MakeTreeFromNewElements();
  EXPECT_EQ(tree.RangeSumWithThreshold(/*threshold*/ 1, 1, 2), 2);
  EXPECT_DEBUG_DEATH(tree.RangeSumWithThreshold(/*threshold*/ 1, 1, 3), ".*");
}

TEST(WeightedWaveletTreeDeathTest, NoPendingNewElementOnQuery) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  WeightedWaveletTree tree;
  tree.PushBack(1, 1);
  tree.PushBack(2, 2);
  tree.MakeTreeFromNewElements();
  tree.PushBack(3, 3);
  EXPECT_DEBUG_DEATH(tree.RangeSumWithThreshold(/*threshold*/ 1, 0, 1), ".*");
}

TEST(WeightedWaveletTreeDeathTest, RangeIncludedInSingleSequence) {
  if constexpr (!DEBUG_MODE) GTEST_SKIP() << "Skipped in opt mode";
  WeightedWaveletTree tree;
  tree.PushBack(1, 1);
  tree.PushBack(2, 2);
  tree.MakeTreeFromNewElements();
  tree.PushBack(3, 3);
  tree.PushBack(4, 4);
  tree.MakeTreeFromNewElements();
  EXPECT_EQ(tree.RangeSumWithThreshold(/*threshold*/ 1, 0, 2), 3);
  EXPECT_EQ(tree.RangeSumWithThreshold(/*threshold*/ 3, 2, 4), 7);
  EXPECT_DEBUG_DEATH(tree.RangeSumWithThreshold(/*threshold*/ 1, 1, 3), ".*");
}

/////////////////////// Filters in Model tests ///////////////////////

void MakeDelta(const int initial_solution[], const int new_solution[],
               const std::vector<IntVar*>& nexts, Assignment* delta) {
  CHECK(delta != nullptr);
  IntVar* objective = nullptr;
  int64_t objective_max = kint64max;
  if (delta->HasObjective()) {
    objective = delta->Objective();
    objective_max = delta->ObjectiveMax();
  }
  delta->Clear();
  for (int i = 0; i < nexts.size(); ++i) {
    if (initial_solution == nullptr || new_solution[i] != initial_solution[i]) {
      if (new_solution[i] != -1) {
        delta->Add(nexts[i])->SetValue(new_solution[i]);
      }
    }
  }
  if (objective != nullptr) {
    delta->AddObjective(objective);
    delta->SetObjectiveMax(objective_max);
  }
}

void MakeDelta(const std::vector<int>& initial_solution,
               const std::vector<int>& new_solution,
               const std::vector<IntVar*>& nexts, Assignment* delta) {
  MakeDelta(initial_solution.data(), new_solution.data(), nexts, delta);
}

void MakeAssignment(const int solution[], const std::vector<IntVar*>& nexts,
                    Assignment* assignment) {
  MakeDelta(nullptr, solution, nexts, assignment);
}

void MakeAssignment(const std::vector<int>& solution,
                    const std::vector<IntVar*>& nexts, Assignment* assignment) {
  MakeDelta(nullptr, solution.data(), nexts, assignment);
}

using RoutingFilterTestWithUnaryCallbackAndSolverType =
    ::testing::TestWithParam<
        std::tuple<bool, RoutingSearchParameters::SchedulingSolver>>;

INSTANTIATE_TEST_SUITE_P(
    RoutingFilterTestWithUnaryCallbackAndSolverType,
    RoutingFilterTestWithUnaryCallbackAndSolverType,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(RoutingSearchParameters::SCHEDULING_GLOP,
                          RoutingSearchParameters::SCHEDULING_CP_SAT)));

TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       DimensionVehicleSpanUpperBoundFilter) {
  static const int kNodes = 4;
  static const int kVehicles = 3;
  static const char* kDimension = "test_dimension";
  static const int kSpanBounds[] = {3, 2, 1};
  static const int kInitialSolution[] = {1, 2, 6, 7, 3, 8};
  static const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {1, 2, 6, 7, 3, 8},
      // Move 3 on first path (already full).
      {1, 2, 3, 6, 7, 8},
      // Move 1 on second path (already full).
      {2, 3, 6, 7, 1, 8},
      // Exchange 1 and 3.
      {3, 7, 6, 2, 1, 8},
      // Make 1 on third path (already full).
      {2, 8, 6, 7, 3, 1}};
  static const bool kExpectedAccept[] = {true, false, false, true, false};
  static_assert(
      std::size(kDeltaSolutions) == std::size(kExpectedAccept),
      "The number of deltas differs from the number of expected results.");
  static const bool kUseGlobalLPFiltering[] = {true, false};

  const auto [use_unary_callback, solver_type] = GetParam();
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(solver_type);
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  for (int lp = 0; lp < std::size(kUseGlobalLPFiltering); lp++) {
    Model model(manager);
    const int callback =
        use_unary_callback
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model.GetMutableDimension(kDimension);
    for (int vehicle = 0; vehicle < kVehicles; ++vehicle) {
      dimension->SetSpanUpperBoundForVehicle(kSpanBounds[vehicle], vehicle);
    }
    model.CloseModelWithParameters(parameters);
    // The model should not have a GlobalDimensionCumulOptimizer of its own
    // for this dimension.
    EXPECT_FALSE(model.HasGlobalCumulOptimizer(*dimension));
    SearchStats stats;
    GlobalDimensionCumulOptimizer optimizer(dimension, solver_type, &stats);
    LocalSearchFilter* const filter =
        kUseGlobalLPFiltering[lp]
            ? MakeGlobalLPCumulFilter(&optimizer, &optimizer,
                                      /*filter_objective_cost=*/true)
            : MakePathCumulFilter(*dimension, true,
                                  /*filter_objective_cost=*/false, true);
    Solver* const solver = model.solver();
    Assignment initial_solution(solver);
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);  // No costs.
    Assignment delta(solver);
    for (int i = 0; i < std::size(kDeltaSolutions); ++i) {
      SCOPED_TRACE(absl::StrCat("Delta=", i));
      MakeDelta(kInitialSolution, kDeltaSolutions[i], model.Nexts(), &delta);
      EXPECT_EQ(kExpectedAccept[i],
                filter->Accept(&delta, nullptr, kint64min, kint64max));
      if (kExpectedAccept[i]) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), 0);  // No costs.
      }
    }
    const int kNumExpectedCalls =
        kUseGlobalLPFiltering[lp] ? std::size(kDeltaSolutions) + 1 : 0;
    EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling +
                  stats.num_cp_sat_cache_hits_in_lp_scheduling,
              solver_type == RoutingSearchParameters::SCHEDULING_CP_SAT
                  ? kNumExpectedCalls
                  : 0);
    EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling,
              solver_type == RoutingSearchParameters::SCHEDULING_GLOP
                  ? kNumExpectedCalls
                  : 0);
    EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
  }
}

using RoutingFilterTestWithSolverType =
    ::testing::TestWithParam<RoutingSearchParameters::SchedulingSolver>;

INSTANTIATE_TEST_SUITE_P(
    RoutingFilterTestWithSolverType, RoutingFilterTestWithSolverType,
    ::testing::Values(RoutingSearchParameters::SCHEDULING_GLOP,
                      RoutingSearchParameters::SCHEDULING_CP_SAT));

// Testing cumul filters with disjoint time windows and soft lower and upper
// bounds which impact the optimal objective value (including vehicle offset).
// The problem has a single vehicle and a single node to visit; its optimal
// cumul value due to soft costs is 25 (+offset, with a cost of 1 per unit for
// being early or late) but there is a forbidden interval between 21 and 29
// (+offset). The optimal cumul value is therefore 20 or 30 resulting in a cost
// of 5. On the filtering side this means we need a mixed boolean program to
// catch this case which means glop will not find the exact optimal but the
// MIP solver eventually will.
TEST_P(RoutingFilterTestWithSolverType,
       DimensionLowerUpperBoundWithDisjointWindowsFilter) {
  static const int kInitialSolution[] = {1, 2};
  const int64_t kOffset = 10;
  const std::vector<int64_t> starts = {21 + kOffset};
  const std::vector<int64_t> ends = {29 + kOffset};
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(GetParam());
  const IndexManager manager(2, 1, NodeIndex(0));
  Model model(manager);
  const int callback =
      model.RegisterUnaryTransitCallback([](int64_t) { return 10LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* const dimension = model.GetMutableDimension("dim");
  dimension->CumulVar(model.Start(0))->SetMin(kOffset);
  dimension->SetCumulVarSoftLowerBound(1, 25 + kOffset, 1);
  dimension->SetCumulVarSoftUpperBound(1, 25 + kOffset, 1);
  dimension->CumulVar(1)->SetRange(15 + kOffset, 35 + kOffset);
  Solver* const solver = model.solver();
  solver->AddConstraint(
      solver->MakeNotMemberCt(dimension->CumulVar(1), starts, ends));
  model.CloseModelWithParameters(parameters);
  LocalSearchFilter* const filter = MakePathCumulFilter(
      *dimension, true, /*filter_objective_cost=*/true, true);
  Assignment initial_solution(solver);
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 5);
}

// Test that the VehicleAmortizedCostFilter filters solutions correctly based on
// the cost resulting from the route lengths.
// The cost of the model in this test is:
// Cost = 1000*(number of used vehicles) - (sum of squares of route lengths).
//
// In the example, the initial solution is the following:
//
// Index of Node0:  0/8 (Vehicle0)        6/9 (Vehicle1)        7/10 (Vehicle2)
//                   /     |               /          \
//                  /      |              /            \
//                 /       |             /              \
//                /        |            /                \
//            Node1------Node2       Node3----Node4----Node5
//
// Cost = 1000 + 1000 - (2*2) - (3*3) = 1987.
//
// The deltas correspond respectively to:
//   1) Moving Node2 from Vehicle0 to Vehicle2, Cost = 3000 - 1 - 1 - 9 = 2989.
//   2) Moving Node3 from Vehicle1 to Vehicle0, Cost (unchanged) = 1987.
//   3) Moving Node2 from Vehicle0 to Vehicle1, Cost = 2000 - 1 - 16 = 1983.
TEST(RoutingFilterTest, VehicleAmortizedCostFilterTest) {
  const int kNodes = 6;
  const int kVehicles = 3;
  // Create a model aiming at minimizing the number of vehicles.
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  model.SetAmortizedCostFactorsOfAllVehicles(1000, 1);
  model.CloseModel();
  Solver* const solver = model.solver();
  // First solution:
  // Vehicle 0 serves nodes 1 and 2, Vehicle 1 serves 3, 4, 5, Vehicle 2 unused.
  // Cost = 1987.
  const int kInitialSolution[] = {1, 2, 8, 4, 5, 9, 3, 10};
  Assignment initial_solution(solver);
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  const int64_t kInitialSolutionCost = 1987;

  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // Move Node 2 to Vehicle 2. New Cost = 2989.
      {1, 8, 10, 4, 5, 9, 3, 2},
      // Move Node 3 to Vehicle0. New Cost = 1987.
      {1, 2, 3, 8, 5, 9, 4, 10},
      // Move Node2 to Vehicle1. New Cost = 1983.
      {1, 8, 3, 4, 5, 9, 2, 10}};

  Assignment delta(solver);
  const int kExpectedDeltaCosts[] = {2989, 1987, 1983};

  LocalSearchFilter* const filter = MakeVehicleAmortizedCostFilter(model);

  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), kInitialSolutionCost);

  // The filter should only accept solutions with lower (or equal) costs.
  for (int i = 0; i < std::size(kDeltaSolutions); ++i) {
    SCOPED_TRACE(absl::StrCat("Delta=", i));
    MakeDelta(kInitialSolution, kDeltaSolutions[i], model.Nexts(), &delta);
    const bool expect_accepted = kExpectedDeltaCosts[i] <= kInitialSolutionCost;
    EXPECT_EQ(expect_accepted,
              filter->Accept(&delta, nullptr, kint64min, kInitialSolutionCost));
    if (expect_accepted) {
      EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedDeltaCosts[i]);
    }
  }

  delta.Clear();
  // Only the last delta should now be accepted by the filter.
  const int64_t kImprovedSolutionCost = 1986;
  for (int i = 0; i < std::size(kDeltaSolutions); ++i) {
    SCOPED_TRACE(absl::StrCat("Delta=", i));
    MakeDelta(kInitialSolution, kDeltaSolutions[i], model.Nexts(), &delta);
    const bool expect_accepted =
        kExpectedDeltaCosts[i] <= kImprovedSolutionCost;
    EXPECT_EQ(expect_accepted, filter->Accept(&delta, nullptr, kint64min,
                                              kImprovedSolutionCost));
    if (expect_accepted) {
      EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedDeltaCosts[i]);
    }
  }
}

// Testing SameVehicleCost filtering.
TEST(RoutingFilterTest, SameVehicleCostFilterTest) {
  const int kNodes = 5 + 1;
  const int kVehicles = 3;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Same vehicle groups
  model.AddSoftSameVehicleConstraint({1, 2, 3}, 11);
  model.AddSoftSameVehicleConstraint({3, 4}, 13);
  model.AddSoftSameVehicleConstraint({5}, 15);
  model.CloseModel();
  Solver* const solver = model.solver();
  // First solution:
  // Vehicle 0: 0 -> 1 -> 5 -> 8
  // Vehicle 1: 6 -> 2 -> 3 -> 9
  // Vehicle 2: 7 -> 4 -> 10
  // Cost = 10 + 10 = 20.
  const int kInitialSolution[] = {1, 5, 3, 9, 10, 8, 2, 4};
  Assignment initial_solution(solver);
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  const int64_t kInitialSolutionCost = 11 + 13;

  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // Move 1 to Vehicle 1. New Cost = 13.
      {5, 2, 3, 9, 10, 8, 1, 4},
      // Move 3 to Vehicle 2. New Cost = 11 * 2.
      {1, 5, 9, 4, 10, 8, 2, 3},
      // Move 3 to Vehicle 2 and 4 to vehicle 0. New Cost = 11 * 2 + 13.
      {1, 4, 9, 10, 5, 8, 2, 3},
      // Move 1 and 3 to Vehicle 1. New Cost = 0.
      {5, 2, 3, 4, 9, 8, 1, 10}};

  Assignment delta(solver);
  const int kExpectedDeltaCosts[] = {13, 22, 35, 0};

  LocalSearchFilter* const filter = MakeSameVehicleCostFilter(model);

  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), kInitialSolutionCost);

  // The filter should only accept solutions with lower (or equal) costs.
  for (int i = 0; i < std::size(kDeltaSolutions); ++i) {
    SCOPED_TRACE(absl::StrCat("Delta=", i));
    MakeDelta(kInitialSolution, kDeltaSolutions[i], model.Nexts(), &delta);
    const bool expect_accepted = kExpectedDeltaCosts[i] <= kInitialSolutionCost;
    EXPECT_EQ(expect_accepted,
              filter->Accept(&delta, nullptr, kint64min, kInitialSolutionCost));
    if (expect_accepted) {
      EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedDeltaCosts[i]);
    }
  }

  delta.Clear();
  // Only the last delta should now be accepted by the filter.
  const int64_t kImprovedSolutionCost = 1;
  for (int i = 0; i < std::size(kDeltaSolutions); ++i) {
    SCOPED_TRACE(absl::StrCat("Delta=", i));
    MakeDelta(kInitialSolution, kDeltaSolutions[i], model.Nexts(), &delta);
    const bool expect_accepted =
        kExpectedDeltaCosts[i] <= kImprovedSolutionCost;
    EXPECT_EQ(expect_accepted, filter->Accept(&delta, nullptr, kint64min,
                                              kImprovedSolutionCost));
    if (expect_accepted) {
      EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedDeltaCosts[i]);
    }
  }
}

// Testing RouteConstraint filtering.
TEST(RoutingFilterTest, RouteConstraintFilterTest) {
  const int kNodes = 3;
  const int kVehicles = 2;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Adding 2 route constraints to make sure both are taken into account.
  model.AddRouteConstraint(
      [](absl::Span<const int64_t> route) -> std::optional<int64_t> {
        // Forbid arc 1->2, otherwise cost is route length ^2.
        int count = 0;
        int prev = -1;
        for (int64_t node : route) {
          if (prev == 1 && node == 2) return std::nullopt;
          ++count;
          prev = node;
        }
        return count * count;
      });
  model.AddRouteConstraint(
      [](absl::Span<const int64_t> /*route*/) -> std::optional<int64_t> {
        return 1;
      });
  model.CloseModel();
  Solver* const solver = model.solver();
  // First solution:
  // Vehicle 0: 0 -> 1 -> 4
  // Vehicle 1: 3 -> 2 -> 5
  // Cost = 18 + 2 x 1.
  const int kInitialSolution[] = {1, 4, 5, 2};
  Assignment initial_solution(solver);
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  const int64_t kInitialSolutionCost = 20;

  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // Vehicle 0: 0 -> 1 -> 2 -> 4
      // Vehicle 1: 3 -> 5
      // Infeasible.
      {1, 2, 4, 5},
      // Vehicle 0: 0 -> 2 -> 1 -> 4
      // Vehicle 1: 3 -> 5
      // Cost = 22.
      {2, 4, 1, 5},
      // Vehicle 0: 0 -> 4
      // Vehicle 1: 3 -> 1 -> 2 -> 5
      // Infeasible.
      {4, 2, 5, 1},
      // Vehicle 0: 0 -> 4
      // Vehicle 1: 3 -> 2 -> 1 -> 5
      // Cost = 22.
      {4, 5, 1, 2},
      // Vehicle 0: 0 -> 2 -> 4
      // Vehicle 1: 3 -> 1 -> 5
      // Cost = 20.
      {2, 5, 4, 1}};

  Assignment delta(solver);

  const bool kExpectedAccept[] = {false, true, false, true, true};
  const int kExpectedDeltaCosts[] = {0, 22, 0, 22, 20};

  LocalSearchFilter* const filter = MakeRouteConstraintFilter(model);

  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), kInitialSolutionCost);

  for (bool relax_ub : {false, true}) {
    for (int i = 0; i < std::size(kDeltaSolutions); ++i) {
      SCOPED_TRACE(absl::StrCat("Delta=", i));
      MakeDelta(kInitialSolution, kDeltaSolutions[i], model.Nexts(), &delta);
      const bool expect_accepted =
          kExpectedAccept[i] &&
          (relax_ub || kExpectedDeltaCosts[i] <= kInitialSolutionCost);
      EXPECT_EQ(expect_accepted,
                filter->Accept(&delta, nullptr, kint64min,
                               relax_ub ? 1000 : kInitialSolutionCost));
      if (expect_accepted) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedDeltaCosts[i]);
      }
    }
  }
}

// TODO(user): Add variants of tests covering PathCumulFilter on dimensions
// with vehicle-dependent transits.

TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       GlobalSpanDimensionCostInCumulFilters) {
  const int kNodes = 4;
  const int kVehicles = 3;
  const char* kDimension = "test_dimension";
  const int64_t kGlobalSpanCostCoef = 1;
  const int kInitialSolution[] = {6, 1, 2, 3, 7, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {6, 1, 2, 3, 7, 8},
      // Make nodes 2 and 3 performed on first path.
      {2, 1, 3, 6, 7, 8},
      // Make 1 performed on second path.
      {2, 7, 3, 6, 1, 8},
      // Make 1 performed on first path and move 2 to third path.
      {1, 3, 8, 6, 7, 2},
      // Make 1 performed on first path.
      {1, 2, 3, 6, 7, 8},
      // Make 1 performed on second path and move 2 to third path.
      {3, 7, 8, 6, 1, 2}};
  const int64_t kStarts[][kNodes - 1 + 2 * kVehicles] = {
      // No windows.
      {0, 0, 0, 0, 0, 0, 0, 0, 0},
      // Upper bound on path ends.
      {0, 0, 0, 0, 0, 0, 0, 0, 0},
      // Fixed path ends.
      {0, 0, 0, 0, 0, 0, 10, 10, 10},
      // First path forced to length >= 9.
      {0, 0, 0, 0, 0, 0, 10, 10, 10},
      // Second path forced to length >= 9.
      {0, 0, 0, 0, 0, 0, 10, 10, 10},
      // Third path forced to length >= 9.
      {0, 0, 0, 0, 0, 0, 10, 10, 10},
      // Lower bound on path starts.
      {10, 0, 0, 0, 10, 10, 0, 0, 0},
      // Fixed path starts.
      {10, 0, 0, 0, 10, 10, 0, 0, 0},
      // 1 must be performed before "1", 2 and 3 after "10", stretching the
      // first path.
      {0, 0, 10, 10, 0, 0, 0, 0, 0},
      // Force 1 after 2.
      {0, 10, 0, 0, 0, 0, 0, 0, 0}};
  const int64_t kEnds[][kNodes - 1 + 2 * kVehicles] = {
      // No windows.
      {kint64max, kint64max, kint64max, kint64max, kint64max, kint64max,
       kint64max, kint64max, kint64max},
      // Upper bound on path ends.
      {kint64max, kint64max, kint64max, kint64max, kint64max, kint64max, 10, 10,
       10},
      // Fixed path ends.
      {kint64max, kint64max, kint64max, kint64max, kint64max, kint64max, 10, 10,
       10},
      // First path forced to length >= 9.
      {1, kint64max, kint64max, kint64max, kint64max, kint64max, 10, 10, 10},
      // Second path forced to length >= 9.
      {kint64max, kint64max, kint64max, kint64max, 1, kint64max, 10, 10, 10},
      // Third path forced to length >= 9.
      {kint64max, kint64max, kint64max, kint64max, kint64max, 1, 10, 10, 10},
      // Lower bound on path starts.
      {kint64max, kint64max, kint64max, kint64max, kint64max, kint64max,
       kint64max, kint64max, kint64max},
      // Fixed path starts.
      {10, kint64max, kint64max, kint64max, 10, 10, kint64max, kint64max,
       kint64max},
      // 1 must be performed before "1", 2 and 3 after "10", stretching the
      // first path.
      {kint64max, 1, kint64max, kint64max, kint64max, kint64max, kint64max,
       kint64max, kint64max},
      // Force 1 after 2.
      {kint64max, kint64max, 9, kint64max, kint64max, kint64max, kint64max,
       kint64max, kint64max}};
  const int64_t kExpectedDeltaCosts[][std::size(kDeltaSolutions)] = {
      {1, 3, 3, 3, 4, 2},        {1, 3, 3, 3, 4, 2}, {1, 3, 3, 3, 4, 2},
      {9, 9, 9, 9, 9, 9},        {9, 9, 9, 9, 9, 9}, {1, 3, 3, 9, 4, 9},
      {1, 3, 3, 3, 4, 2},        {1, 3, 3, 3, 4, 2}, {1, 3, 12, 11, 12, 11},
      {1, 3, 3, 4, kint64max, 3}};
  const int KExpectNumberOfVehicleClasses[] = {2, 2, 2, 3, 3, 2, 2, 2, 2, 2};
  const bool kFilterObjectiveCost[] = {true, false};
  const bool kUseGlobalLPFiltering[] = {true, false};
  // Consider empty vehicle costs for vehicles 1 and 2, but not vehicle 3.
  const bool kConsiderEmptyVehicleCosts[] = {true, true, false};

  const auto [use_unary_callback, solver_type] = GetParam();
  for (int i = 0; i < std::size(kFilterObjectiveCost); ++i) {
    for (int j = 0; j < std::size(kExpectedDeltaCosts); ++j) {
      for (int lp = 0; lp < std::size(kUseGlobalLPFiltering); lp++) {
        for (int k = 0; k < 25; ++k) {
          RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
          parameters.set_continuous_scheduling_solver(solver_type);

          SCOPED_TRACE(absl::StrCat(i, " ", j, " ", k));
          IndexManager manager(kNodes, kVehicles, NodeIndex(0));
          Model model(manager);
          for (int v = 0; v < model.vehicles(); v++) {
            model.SetVehicleUsedWhenEmpty(kConsiderEmptyVehicleCosts[v], v);
          }
          const int callback = use_unary_callback
                                   ? model.RegisterUnaryTransitCallback(
                                         [](int64_t) { return 1LL; })
                                   : model.RegisterTransitCallback(
                                         [](int64_t, int64_t) { return 1LL; });
          model.AddDimension(callback, kint64max, kint64max,
                             /*fix_start_cumul_to_zero=*/false, kDimension);
          Dimension* const dimension = model.GetMutableDimension(kDimension);
          dimension->SetGlobalSpanCostCoefficient(kGlobalSpanCostCoef);
          for (int l = 0; l < kNodes - 1 + 2 * kVehicles; ++l) {
            dimension->CumulVar(l)->SetMin(kStarts[j][l]);
            dimension->CumulVar(l)->SetMax(kEnds[j][l]);
          }
          model.CloseModelWithParameters(parameters);
          EXPECT_EQ(KExpectNumberOfVehicleClasses[j],
                    model.GetVehicleClassesCount());
          // The model should have a GlobalDimensionCumulOptimizer for this
          // dimension.
          GlobalDimensionCumulOptimizer* const lp_optimizer =
              model.GetMutableGlobalCumulLPOptimizer(*dimension);
          GlobalDimensionCumulOptimizer* const mp_optimizer =
              model.GetMutableGlobalCumulMPOptimizer(*dimension);
          ASSERT_NE(lp_optimizer, nullptr);
          ASSERT_NE(mp_optimizer, nullptr);
          LocalSearchFilter* const filter =
              kUseGlobalLPFiltering[lp]
                  ? MakeGlobalLPCumulFilter(lp_optimizer, mp_optimizer,
                                            kFilterObjectiveCost[i])
                  : MakePathCumulFilter(*dimension, true,
                                        kFilterObjectiveCost[i], true);
          Solver* const solver = model.solver();
          Assignment initial_solution(solver);
          for (int l = 0; l < kNodes; ++l) {
            EXPECT_EQ(manager.NodeToIndex(NodeIndex(l)), l);
          }
          MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
          if (model.CostVar()->Min() > k) {
            continue;
          }
          filter->Synchronize(&initial_solution, nullptr);
          // initial solution == first delta, so they have the same cost.
          const int64_t expected_synchronized_objective_value =
              kFilterObjectiveCost[i] ? kExpectedDeltaCosts[j][0] : 0;
          EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                    expected_synchronized_objective_value);
          int last_synchronized_delta_index = 0;

          Assignment delta(solver);
          for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
            SCOPED_TRACE(absl::StrCat(
                "Cost ub=", k, ", delta=", l, ", model=", j,
                ", using LP=", kUseGlobalLPFiltering[lp] ? "true" : "false"));
            MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                      kDeltaSolutions[l], model.Nexts(), &delta);
            const bool expect_accepted =
                kFilterObjectiveCost[i] ? kExpectedDeltaCosts[j][l] <= k
                                        : kExpectedDeltaCosts[j][l] < kint64max;
            EXPECT_EQ(expect_accepted,
                      filter->Accept(&delta, nullptr, kint64min, k));
            if (expect_accepted) {
              const int64_t expected_accepted_objective_value =
                  kFilterObjectiveCost[i] ? kExpectedDeltaCosts[j][l] : 0;
              EXPECT_EQ(filter->GetAcceptedObjectiveValue(),
                        expected_accepted_objective_value);
              Assignment current_solution(model.solver());
              MakeAssignment(kDeltaSolutions[l], model.Nexts(),
                             &current_solution);
              filter->Synchronize(&current_solution, &delta);
              last_synchronized_delta_index = l;
            }
          }
        }
      }
    }
  }
}

// TODO(user): refactor all such tests that try multiple scenarios
// into more readable and faster TEST_P.

TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       VehicleSpanAndSlackCostFilter) {
  const int kNodes = 4;
  const int kVehicles = 3;
  const char* kDimension = "test_dimension";
  // TODO(user): make the test data actually sensible to the different vehicle
  // span cost coefficients. Also, test the scenario where all vehicles have
  // the same cost coefficient.
  const int64_t kVehicleSpanCostCoef[] = {2, 0, 3};
  const int kInitialSolution[] = {2, 1, 3, 6, 7, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta. Due to time windows, node 2 starts at 4 and
      // node 3 starts 7 at the earliest, resulting in a minimum slack of
      // 3 - 1 (1 being the transit from 2 to 3) and a slack cost of 2 * 2.
      {2, 1, 3, 6, 7, 8},
      // Make 1 performed on second path. This does not change the first path
      // which support all of the slack cost.
      {2, 7, 3, 6, 1, 8},
      // Make 1 performed on first path before 2, which does not add any slack.
      {1, 2, 3, 6, 7, 8},
      // Make 1 performed on first path and move 2 to third path. 2 and 3 being
      // on different paths, the total slack is now 0.
      {1, 3, 8, 6, 7, 2},
      // Make 1 performed on second path and move 2 to third path. Similarly to
      // the previous case, the total slack is 0.
      {3, 7, 8, 6, 1, 2}};
  const int64_t kWindows[][2] = {{1, kint64max}, {4, 4}, {7, 10}};
  // See explanations in the deltas justifying the costs.
  const int64_t kExpectedDeltaCosts[] = {4, 4, 4, 0, 0};
  const bool kFilterObjectiveCost[] = {true, false};
  const bool kUseGlobalLPFiltering[] = {true, false};

  const auto [use_unary_callback, solver_type] = GetParam();
  for (bool use_span_costs : {true, false}) {
    for (int i = 0; i < std::size(kFilterObjectiveCost); ++i) {
      for (int lp = 0; lp < std::size(kUseGlobalLPFiltering); lp++) {
        for (int k = 0; k < 12; ++k) {
          RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
          parameters.set_continuous_scheduling_solver(solver_type);

          SCOPED_TRACE(
              absl::StrCat("Local LP=", i, " cost_ub=", k, " Global lp=", lp));
          IndexManager manager(kNodes, kVehicles, NodeIndex(0));
          Model model(manager);
          const int callback = use_unary_callback
                                   ? model.RegisterUnaryTransitCallback(
                                         [](int64_t) { return 1LL; })
                                   : model.RegisterTransitCallback(
                                         [](int64_t, int64_t) { return 1LL; });
          model.AddDimension(callback,
                             /*slack_max=*/kint64max,
                             /*capacity=*/kint64max,
                             /*fix_start_cumul_to_zero=*/false, kDimension);
          Dimension* dimension = model.GetMutableDimension(kDimension);
          for (int j = 0; j < std::size(kVehicleSpanCostCoef); ++j) {
            if (use_span_costs) {
              dimension->SetSpanCostCoefficientForVehicle(
                  kVehicleSpanCostCoef[j], j);
            } else {
              dimension->SetSlackCostCoefficientForVehicle(
                  kVehicleSpanCostCoef[j], j);
            }
          }
          for (int j = 1; j < kNodes; ++j) {
            dimension->CumulVar(j)->SetRange(kWindows[j - 1][0],
                                             kWindows[j - 1][1]);
          }
          model.CloseModelWithParameters(parameters);
          // The model should not have a GlobalDimensionCumulOptimizer of its
          // own for this dimension.
          EXPECT_FALSE(model.HasGlobalCumulOptimizer(*dimension));
          SearchStats stats;
          GlobalDimensionCumulOptimizer optimizer(dimension, solver_type,
                                                  &stats);
          // Note: the filters below only consider the slack cost resulting
          // from span costs. Whether span or slack costs are used, the filter
          // objective value should be the same.
          LocalSearchFilter* const filter =
              kUseGlobalLPFiltering[lp]
                  ? MakeGlobalLPCumulFilter(&optimizer, &optimizer,
                                            kFilterObjectiveCost[i])
                  : MakePathCumulFilter(*dimension, true,
                                        kFilterObjectiveCost[i], true);
          Solver* const solver = model.solver();
          Assignment initial_solution(solver);
          MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
          if (model.CostVar()->Min() > k) {
            continue;
          }
          int filter_calls = 1;
          filter->Synchronize(&initial_solution, nullptr);
          const int64_t expected_synchronized_cost =
              kFilterObjectiveCost[i] ? kExpectedDeltaCosts[0] : 0;
          EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                    expected_synchronized_cost);  // No change on first delta.
          Assignment delta(solver);
          int last_synchronized_delta_index = 0;
          for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
            SCOPED_TRACE(absl::StrCat("Delta=", l));
            MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                      kDeltaSolutions[l], model.Nexts(), &delta);
            const bool expect_accepted =
                kExpectedDeltaCosts[l] <=
                (kFilterObjectiveCost[i] ? k : (kint64max - 1));
            EXPECT_EQ(expect_accepted,
                      filter->Accept(&delta, nullptr, kint64min, k));
            ++filter_calls;
            if (expect_accepted) {
              const int64_t expected_accepted_cost =
                  kFilterObjectiveCost[i] ? kExpectedDeltaCosts[l] : 0;
              EXPECT_EQ(filter->GetAcceptedObjectiveValue(),
                        expected_accepted_cost);
              Assignment current_solution(model.solver());
              MakeAssignment(kDeltaSolutions[l], model.Nexts(),
                             &current_solution);
              filter->Synchronize(&current_solution, &delta);
              last_synchronized_delta_index = l;
              ++filter_calls;
            }
          }
          const int kExpectedNumCalls =
              kUseGlobalLPFiltering[lp] ? filter_calls : 0;
          EXPECT_EQ(stats.num_cp_sat_calls_in_lp_scheduling +
                        stats.num_cp_sat_cache_hits_in_lp_scheduling,
                    solver_type == RoutingSearchParameters::SCHEDULING_CP_SAT
                        ? kExpectedNumCalls
                        : 0);
          EXPECT_EQ(stats.num_glop_calls_in_lp_scheduling,
                    solver_type == RoutingSearchParameters::SCHEDULING_GLOP
                        ? kExpectedNumCalls
                        : 0);
          EXPECT_EQ(stats.num_min_cost_flow_calls, 0);
        }
      }
    }
  }
}

// Same as above with soft cumul costs to exercise LP filtering in
// PathCumulFilter. Whether span, slack or none of these are used, the soft
// costs are the only ones which will appear in the filter as it's cheaper to
// make a node late or early than to pay for the slack costs.
void TestVehicleSpanAndSoftCumulCostFilter(
    bool use_unary_callback,
    RoutingSearchParameters::SchedulingSolver solver_type,
    bool use_global_lp_filtering, bool clear_lns_time_limit, bool set_span_cost,
    bool set_slack_cost) {
  const int kNodes = 4;
  const int kVehicles = 3;
  const char* kDimension = "test_dimension";
  const int64_t kVehicleSpanCostCoef[] = {2, 0, 3};
  const int64_t kVehicleSlackCostCoef[] = {1, 1, 3};
  const int kInitialSolution[] = {2, 1, 3, 6, 7, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {2, 1, 3, 6, 7, 8},
      // Make 1 performed on second path.
      {2, 7, 3, 6, 1, 8},
      // Make 1 performed on first path.
      {1, 2, 3, 6, 7, 8},
      // Make 1 performed on first path and move 2 to third path.
      {1, 3, 8, 6, 7, 2},
      // Make 1 performed on second path and move 2 to third path.
      {3, 7, 8, 6, 1, 2}};
  const int64_t kSoftWindows[][2] = {{1, 6}, {4, 4}, {8, 9}};
  const int64_t kSoftCoef = 1;
  const int64_t kExpectedDeltaCosts[] = {3, 3, 3, 1, 0};

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  if (clear_lns_time_limit) {
    parameters.clear_lns_time_limit();
  }
  for (int k = 0; k < 12; ++k) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_continuous_scheduling_solver(solver_type);

    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const int callback =
        use_unary_callback
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.AddDimension(callback,
                       /*slack_max=*/0,
                       /*capacity=*/kint64max,
                       /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* dimension = model.GetMutableDimension(kDimension);
    if (set_span_cost) {
      for (int j = 0; j < std::size(kVehicleSpanCostCoef); ++j) {
        dimension->SetSpanCostCoefficientForVehicle(kVehicleSpanCostCoef[j], j);
      }
    }
    if (set_slack_cost) {
      for (int j = 0; j < std::size(kVehicleSlackCostCoef); ++j) {
        dimension->SetSlackCostCoefficientForVehicle(kVehicleSlackCostCoef[j],
                                                     j);
      }
    }
    for (int j = 1; j < kNodes; ++j) {
      dimension->SetCumulVarSoftLowerBound(j, kSoftWindows[j - 1][0],
                                           kSoftCoef);
      dimension->SetCumulVarSoftUpperBound(j, kSoftWindows[j - 1][1],
                                           kSoftCoef);
    }
    model.CloseModelWithParameters(parameters);

    // Setting filter_objective_cost to true in the filters below
    // ensures the cost is optimized and that the local LP is called in the
    // PathCumulFilter.
    const bool filter_objective_cost = true;
    // The model should not have a GlobalDimensionCumulOptimizer of its own
    // for this dimension.
    EXPECT_FALSE(model.HasGlobalCumulOptimizer(*dimension));
    GlobalDimensionCumulOptimizer optimizer(dimension, solver_type, nullptr);
    LocalSearchFilter* const filter =
        use_global_lp_filtering
            ? MakeGlobalLPCumulFilter(&optimizer, &optimizer,
                                      filter_objective_cost)
            : MakePathCumulFilter(*dimension, true, filter_objective_cost,
                                  true);
    Solver* const solver = model.solver();
    Assignment initial_solution(solver);
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    if (model.CostVar()->Min() > k) {
      continue;
    }
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
              kExpectedDeltaCosts[0]);  // No change on first delta.
    Assignment delta(solver);
    int last_synchronized_delta_index = 0;
    for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
      SCOPED_TRACE(absl::StrCat("Cost ub=", k, " delta=", l));
      MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                kDeltaSolutions[l], model.Nexts(), &delta);
      const bool expect_accepted = kExpectedDeltaCosts[l] <= k;
      EXPECT_EQ(expect_accepted, filter->Accept(&delta, nullptr, kint64min, k));
      if (expect_accepted) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedDeltaCosts[l]);
        Assignment current_solution(model.solver());
        MakeAssignment(kDeltaSolutions[l], model.Nexts(), &current_solution);
        filter->Synchronize(&current_solution, &delta);
        last_synchronized_delta_index = l;
      }
    }
  }
}

TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       VehicleSpanAndSoftCumulCostFilter) {
  const auto [use_unary_callback, solver_type] = GetParam();
  for (bool use_global_lp_filtering : {true, false}) {
    for (bool clear_lns_time_limit : {true, false}) {
      TestVehicleSpanAndSoftCumulCostFilter(
          use_unary_callback, solver_type, use_global_lp_filtering,
          clear_lns_time_limit, /*set_span_cost=*/true,
          /*set_slack_cost=*/false);
    }
  }
}

// Make sure filtering is also happening properly when there are only soft upper
// and lower cumul bounds.
TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       SoftCumulCostFilterWithoutSpanCost) {
  const auto [use_unary_callback, solver_type] = GetParam();
  for (bool use_global_lp_filtering : {true, false}) {
    for (bool clear_lns_time_limit : {true, false}) {
      TestVehicleSpanAndSoftCumulCostFilter(
          use_unary_callback, solver_type, use_global_lp_filtering,
          clear_lns_time_limit,
          /*set_span_cost=*/false, /*set_slack_cost=*/false);
    }
  }
}

TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       VehicleSlackAndSoftCumulCostFilter) {
  const auto [use_unary_callback, solver_type] = GetParam();
  for (bool use_global_lp_filtering : {true, false}) {
    for (bool clear_lns_time_limit : {true, false}) {
      TestVehicleSpanAndSoftCumulCostFilter(
          use_unary_callback, solver_type, use_global_lp_filtering,
          clear_lns_time_limit,
          /*set_span_cost=*/false, /*set_slack_cost=*/true);
    }
  }
}

TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       VehicleSlackSpanAndSoftCumulCostFilter) {
  const auto [use_unary_callback, solver_type] = GetParam();
  for (bool use_global_lp_filtering : {true, false}) {
    for (bool clear_lns_time_limit : {true, false}) {
      TestVehicleSpanAndSoftCumulCostFilter(
          use_unary_callback, solver_type, use_global_lp_filtering,
          clear_lns_time_limit,
          /*set_span_cost=*/true, /*set_slack_cost=*/true);
    }
  }
}

// The following example shows a case in which the PathCumulFilter
// under-estimates the cost of a model, and the LPCumulFilter computes the
// exact value for it.
TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       VehicleSoftCumulAndLocalAndGlobalSpanCosts) {
  const int kNumNodes = 4;
  const int kNumVehicles = 3;
  const auto [use_unary_callback, solver_type] = GetParam();

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(solver_type);

  const char* kDimension = "Time";
  const int kVehicleSpanOrSlackCostCoef = 1;
  const int kSoftCostCoef = 10;
  const int kGlobalSpanCostCoef = 100;
  // Time windows ({start_time, end_time} or
  // {start_time, soft_end_time, end_time}) of each of the 3 non start-end
  // nodes. Only node 1 has a soft end time of 5.
  const std::vector<std::vector<int>> kTimeWindows = {
      {0, 5, 10}, {15, 30}, {5, 20}};
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  // Initial solution:
  // V0: NODE0 --> 1 --> 2 --> NODE6
  // V1: NODE4 --> 3 --> NODE7
  // V2: NODE5 --> NODE8
  const int kInitialSolution[] = {1, 2, 6, 7, 3, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {1, 2, 6, 7, 3, 8},
      // Move node 3 to the first path, after 2.
      {1, 2, 3, 6, 7, 8},
      // Exchange nodes 2 and 3.
      {1, 3, 7, 6, 2, 8},
      // Move node 2 to the second path, before node 3.
      {1, 6, 3, 7, 2, 8},
      // Move node 2 to the second path, after node 3.
      {1, 6, 7, 2, 3, 8}};
  const bool kUseGlobalLPFiltering[] = {false, true};
  // Costs expected for each of the deltas, respectively without and with the
  // global LP filter. In most cases, the PathCumulFilter under-estimates the
  // cost.
  // Note: The part of the span cost due to transit, which is equal to 25*1,
  // is not taken into account by these filters.
  const int kExpectedDeltaCosts[][std::size(kUseGlobalLPFiltering)] = {
      {/*(15*100)+((0+5)*10)+[(20+10)*1] - 25*/ 1555,
       /*(15*100)+((5+10)*10)+[(15+10)*1] - 25*/ 1650},
      {/*(20*100)+((0+5)*10)+[(25+5)*1] - 25*/ 2055,
       /*(20*100)+((5+10)*10)+[(20+5)*1] - 25*/ 2150},
      {/*(15*100)+((0+5)*10)+[(15+10)*1] - 25*/ 1550,
       /*(15*100)+((5+10)*10)+[(15+10)*1] - 25*/ 1650},
      {/*(20*100)+((0+5)*10)+[(10+15)*1] - 25*/ 2050,
       /*(20*100)+((5+10)*10)+[(10+15)*1] - 25*/ 2150},
      {/*(15*100)+((0+5)*10)+[(10+15)*1] - 25*/ 1550,
       /*(15*100)+((5+10)*10)+[(10+15)*1] - 25*/ 1650}};

  const int kCostUpperBounds[] = {3000, 2050, 2000, 1550};

  for (bool set_slack_cost : {true, false}) {
    Model model(manager);
    const int callback =
        use_unary_callback
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 5LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 5LL; });
    model.AddDimension(callback,
                       /*slack_max=*/kint64max,
                       /*capacity=*/kint64max,
                       /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* dimension = model.GetMutableDimension(kDimension);

    for (int v = 0; v < kNumVehicles; v++) {
      if (set_slack_cost) {
        dimension->SetSlackCostCoefficientForVehicle(
            kVehicleSpanOrSlackCostCoef, v);
      } else {
        dimension->SetSpanCostCoefficientForVehicle(kVehicleSpanOrSlackCostCoef,
                                                    v);
      }
    }
    for (int n = 1; n < kNumNodes; n++) {
      const std::vector<int>& time_window = kTimeWindows[n - 1];
      dimension->CumulVar(n)->SetRange(time_window[0], time_window.back());
      if (time_window.size() > 2) {
        dimension->SetCumulVarSoftUpperBound(n, time_window[1], kSoftCostCoef);
      }
    }
    // Add a soft upper bound at 0 for the end of the third vehicle, and make it
    // "used if route is empty".
    dimension->SetCumulVarSoftUpperBound(model.End(2), 0, kSoftCostCoef);
    model.SetVehicleUsedWhenEmpty(true, 2);

    dimension->SetGlobalSpanCostCoefficient(kGlobalSpanCostCoef);

    model.CloseModelWithParameters(parameters);

    // The model should have a GlobalDimensionCumulOptimizer for this dimension.
    GlobalDimensionCumulOptimizer* const lp_optimizer =
        model.GetMutableGlobalCumulLPOptimizer(*dimension);
    ASSERT_NE(lp_optimizer, nullptr);
    GlobalDimensionCumulOptimizer* const mp_optimizer =
        model.GetMutableGlobalCumulMPOptimizer(*dimension);
    ASSERT_NE(mp_optimizer, nullptr);

    // Setting filter_objective_cost to true in the filters below
    // ensures the cost is optimized and that the local LP is called in the
    // PathCumulFilter.
    const bool filter_objective_cost = true;
    for (int upper_bound : kCostUpperBounds) {
      for (int lp = 0; lp < std::size(kUseGlobalLPFiltering); lp++) {
        LocalSearchFilter* const filter =
            kUseGlobalLPFiltering[lp]
                ? MakeGlobalLPCumulFilter(lp_optimizer, mp_optimizer,
                                          filter_objective_cost)
                : MakePathCumulFilter(*dimension, true, filter_objective_cost,
                                      true);
        Solver* const solver = model.solver();
        Assignment initial_solution(solver);
        MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
        filter->Synchronize(&initial_solution, nullptr);
        EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                  kExpectedDeltaCosts[0][lp]);  // No change on first delta.
        Assignment delta(solver);
        int last_synchronized_delta_index = 0;
        for (int d = 0; d < std::size(kDeltaSolutions); d++) {
          SCOPED_TRACE(
              absl::StrCat("Cost ub=", upper_bound, " delta=", d, " lp=", lp));
          MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                    kDeltaSolutions[d], model.Nexts(), &delta);
          const int expected_cost = kExpectedDeltaCosts[d][lp];
          const bool expect_accepted = expected_cost <= upper_bound;
          EXPECT_EQ(expect_accepted,
                    filter->Accept(&delta, nullptr, kint64min, upper_bound));
          if (expect_accepted) {
            EXPECT_EQ(filter->GetAcceptedObjectiveValue(), expected_cost);
            Assignment current_solution(model.solver());
            MakeAssignment(kDeltaSolutions[d], model.Nexts(),
                           &current_solution);
            filter->Synchronize(&current_solution, &delta);
            EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), expected_cost);
            last_synchronized_delta_index = d;
          }
        }
      }
    }
  }
}

// Same example as above, but this time test that AppendDimensionCumulFilters()
// works as intended by using the GlobalLPCumulFilter.
TEST_P(
    RoutingFilterTestWithUnaryCallbackAndSolverType,
    AppendDimensionCumulFiltersWithVehicleSoftCumulAndLocalAndGlobalSpanCosts) {
  const int kNumNodes = 4;
  const int kNumVehicles = 2;
  const char* kDimension = "Time";
  const int kVehicleSpanCostCoef = 1;
  const int kSoftCostCoef = 10;
  const int kGlobalSpanCostCoef = 100;
  // Time windows ({start_time, end_time} or
  // {start_time, soft_end_time, end_time}) of each of the 3 non start-end
  // nodes. Only node 1 has a soft end time of 5.
  const std::vector<std::vector<int>> kTimeWindows = {
      {0, 5, 10}, {15, 30}, {5, 20}};

  // Initial solution:
  // V0: NODE0 --> 1 --> 2 --> NODE5
  // V1: NODE4 --> 3 --> NODE6
  const int kInitialSolution[] = {1, 2, 5, 6, 3};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {1, 2, 5, 6, 3},
      // Move node 3 to the first path, after 2.
      {1, 2, 3, 5, 6},
      // Exchange nodes 2 and 3.
      {1, 3, 6, 5, 2},
      // Move node 2 to the second path, before node 3.
      {1, 5, 3, 6, 2},
      // Move node 2 to the second path, after node 3.
      {1, 5, 6, 2, 3}};
  // Costs expected for each of the deltas.
  // Note: The part of the span cost due to transit, which is equal to 25*1,
  // is not taken into account by these filters.
  const int kExpectedDeltaCosts[] = {/*(15*100)+(5*10)+[(15+10)*1] - 25*/ 1550,
                                     /*(20*100)+(5*10)+[(20+5)*1] - 25*/ 2050,
                                     /*(15*100)+(5*10)+[(15+10)*1] - 25*/ 1550,
                                     /*(20*100)+(5*10)+[(10+15)*1] - 25*/ 2050,
                                     /*(15*100)+(5*10)+[(10+15)*1] - 25*/ 1550};

  const int kCostUpperBounds[] = {3000, 2050, 2000, 1550};

  const auto [use_unary_callback, solver_type] = GetParam();

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(solver_type);

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);
  const int callback =
      use_unary_callback
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 5LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 5LL; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/kint64max,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  Dimension* dimension = model.GetMutableDimension(kDimension);

  for (int v = 0; v < kNumVehicles; v++) {
    dimension->SetSpanCostCoefficientForVehicle(kVehicleSpanCostCoef, v);
  }
  for (int n = 1; n < kNumNodes; n++) {
    const std::vector<int>& time_window = kTimeWindows[n - 1];
    dimension->CumulVar(n)->SetRange(time_window[0], time_window.back());
    if (time_window.size() > 2) {
      dimension->SetCumulVarSoftUpperBound(n, time_window[1], kSoftCostCoef);
    }
  }
  dimension->SetGlobalSpanCostCoefficient(kGlobalSpanCostCoef);

  model.CloseModelWithParameters(parameters);

  // The non-null objective callback ensures that local LP is used in the
  // PathCumulFilter.
  for (int upper_bound : kCostUpperBounds) {
    std::vector<LocalSearchFilterManager::FilterEvent> filters;
    AppendDimensionCumulFilters({dimension}, parameters,
                                /*filter_objective_cost=*/true,
                                /*use_chain_cumul_filter=*/true, &filters);
    ASSERT_EQ(filters.size(), 2);
    Solver* const solver = model.solver();
    Assignment initial_solution(solver);
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    for (int f = 0; f < filters.size(); f++) {
      LocalSearchFilter* const filter = filters[f].filter;
      filter->Synchronize(&initial_solution, nullptr);
      // The PathCumulFilter (first filter) doesn't synchronize its cost
      // because the GlobalLPFilter is used.
      const int expected_synchronized_value =
          f == 0 ? 0 : kExpectedDeltaCosts[0];
      EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                expected_synchronized_value);
    }
    Assignment delta(solver);
    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      SCOPED_TRACE(absl::StrCat("Cost ub=", upper_bound, " delta=", d));
      MakeDelta(kInitialSolution, kDeltaSolutions[d], model.Nexts(), &delta);
      const int expected_propagated_value = kExpectedDeltaCosts[d];
      const bool expect_accepted = expected_propagated_value <= upper_bound;
      if (expect_accepted) {
        for (int f = 0; f < filters.size(); f++) {
          LocalSearchFilter* const filter = filters[f].filter;
          EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, upper_bound));
          // The PathCumulFilter doesn't add its cost to the accepted value
          // because the GlobalLPFilter is used.
          const int expected_accepted_value =
              f == 0 ? 0 : kExpectedDeltaCosts[d];
          EXPECT_EQ(expected_accepted_value,
                    filter->GetAcceptedObjectiveValue());
        }
      }
    }
  }
}

class NodePrecedenceCumulFiltersTest
    : public ::testing::TestWithParam<::testing::tuple<
          bool, Dimension::NodePrecedence::PerformedConstraint>> {
 protected:
  static const int kInitialSolution[];
  static const int kDeltaSolutions[][6];

  ~NodePrecedenceCumulFiltersTest() override = default;
  void SetUp() override {
    const int kNumNodes = 5;
    const int kNumVehicles = 2;

    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);

    const char* kDimension = "Time";
    const int callback =
        ::testing::get<0>(GetParam())
            ? model_->RegisterUnaryTransitCallback([](int64_t) { return 5LL; })
            : model_->RegisterTransitCallback(
                  [](int64_t, int64_t) { return 5LL; });
    model_->AddDimension(callback, /*slack_max=*/20,
                         /*capacity=*/kint64max,
                         /*fix_start_cumul_to_zero=*/false, kDimension);

    time_dimension_ = model_->GetMutableDimension(kDimension);
    // Precedences: Node3 must be done at or after Node1 + 12, and Node4
    // at/after Node2 + 7.
    time_dimension_->AddNodePrecedence({/*first_node*/ 1, /*second_node*/ 3,
                                        /*offset*/ 12,
                                        ::testing::get<1>(GetParam())});
    time_dimension_->AddNodePrecedence({/*first_node*/ 2, /*second_node*/ 4,
                                        /*offset*/ 7,
                                        ::testing::get<1>(GetParam())});

    // Add an offset to all vehicle starts.
    for (int v = 0; v < model_->vehicles(); v++) {
      time_dimension_->CumulVar(model_->Start(v))->SetMin(dimension_offset_);
    }
  }

  void RunAndExpectSuccess(
      std::vector<bool> expected_acceptance, std::vector<int64_t> expected_cost,
      std::vector<int> initial_solution_override = {},
      std::vector<std::vector<int>> delta_solution_override = {}) {
    model_->CloseModel();
    Assignment initial_solution(model_->solver());
    if (initial_solution_override.empty()) {
      MakeAssignment(kInitialSolution, model_->Nexts(), &initial_solution);
    } else {
      ASSERT_TRUE(!delta_solution_override.empty());
      MakeAssignment(initial_solution_override, model_->Nexts(),
                     &initial_solution);
    }

    for (bool filter_objective : {false, true}) {
      std::vector<LocalSearchFilterManager::FilterEvent> filters;
      AppendDimensionCumulFilters(
          {time_dimension_}, DefaultRoutingSearchParameters(), filter_objective,
          /*use_chain_cumul_filter*/ true, &filters);
      EXPECT_EQ(filters.size(), 2);
      LocalSearchFilter* const filter = filters[1].filter;
      filter->Synchronize(&initial_solution, nullptr);
      if (filter_objective) {
        EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), expected_cost[0]);
      }
      for (int d = 0; d < expected_acceptance.size(); d++) {
        SCOPED_TRACE(absl::StrCat("filter_objective = ", filter_objective,
                                  ", delta = ", d));
        Assignment delta(model_->solver());
        if (initial_solution_override.empty()) {
          MakeDelta(kInitialSolution, kDeltaSolutions[d], model_->Nexts(),
                    &delta);
        } else {
          MakeDelta(initial_solution_override.data(),
                    delta_solution_override[d].data(), model_->Nexts(), &delta);
        }
        EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                  expected_acceptance[d])
            << filter->DebugString();
        if (filter_objective) {
          EXPECT_EQ(filter->GetAcceptedObjectiveValue(), expected_cost[d]);
        } else {
          EXPECT_EQ(filter->GetAcceptedObjectiveValue(), 0);
        }
      }
    }
  }

  std::unique_ptr<Model> model_;
  Dimension* time_dimension_;
  const int64_t dimension_offset_ = 1234;
};

// Initial solution:
// V0: NODE0 --> 1 --> 2 --> NODE6
// V1: NODE5 --> 3 --> 4 --> NODE7
const int NodePrecedenceCumulFiltersTest::kInitialSolution[] = {1, 2, 6,
                                                                4, 7, 3};
const int NodePrecedenceCumulFiltersTest::kDeltaSolutions[][6] = {
    // No change, empty delta.
    {1, 2, 6, 4, 7, 3},
    // Exchange nodes 1 and 2.
    {2, 6, 1, 4, 7, 3},
    // Exchange nodes 1 and 3.
    {3, 4, 6, 2, 7, 1},
    // Make node 1 unperformed and put node 3 before 2.
    {3, 1, 6, 2, 7, 4},
    // Exchange nodes 2 and 4.
    {1, 4, 7, 2, 6, 3},
    // Exchange nodes 3 and 4.
    {1, 2, 6, 7, 3, 4}};

TEST_P(NodePrecedenceCumulFiltersTest, NodePrecedenceWithGlobalSpanCostTest) {
  time_dimension_->SetGlobalSpanCostCoefficient(100);
  // Set the slack max to 18.
  for (IntVar* slack_var : time_dimension_->slacks()) {
    slack_var->SetMax(18);
  }

  switch (::testing::get<1>(GetParam())) {
    case Dimension::NodePrecedence::PerformedConstraint::
        kFirstAndSecondIndependent:
      RunAndExpectSuccess({true, true, false, true, false, true},
                          {2700, 3200, kint64max, 2200, kint64max, 2700});
      break;
    case Dimension::NodePrecedence::PerformedConstraint::kFirstImpliesSecond:
      RunAndExpectSuccess({true, true, false, true, false, true},
                          {2700, 3200, kint64max, 2200, kint64max, 2700});
      break;
    default:
      RunAndExpectSuccess({true, true, false, false, false, true},
                          {2700, 3200, kint64max, kint64max, kint64max, 2700});
  }
}

TEST_P(NodePrecedenceCumulFiltersTest,
       NodePrecedenceWithGlobalSpanCostAndUnassignedNodeTest) {
  time_dimension_->SetGlobalSpanCostCoefficient(100);
  // Set the slack max to 18.
  for (IntVar* slack_var : time_dimension_->slacks()) {
    slack_var->SetMax(18);
  }

  RunAndExpectSuccess({true, true, true, true, true},
                      {1700, 3200, 2200, 2200, 2200}, {2, -1, 6, 4, 7, 3},
                      {// No change, empty delta.
                       {2, -1, 6, 4, 7, 3},
                       // Insert node 1 and move 2.
                       {2, 6, 1, 4, 7, 3},
                       // Put node 3 before 2.
                       {3, -1, 6, 2, 7, 4},
                       // Exchange nodes 2 and 4.
                       {4, -1, 7, 2, 6, 3},
                       // Exchange nodes 3 and 4.
                       {2, -1, 6, 7, 3, 4}});
}

TEST_P(NodePrecedenceCumulFiltersTest, NodePrecedenceWithLocalSpanCostTest) {
  time_dimension_->SetSpanCostCoefficientForAllVehicles(100);

  // Set the slack min to 2.
  for (IntVar* slack_var : time_dimension_->slacks()) {
    slack_var->SetMin(2);
  }

  // NOTE: Only the slack is taken into account for the vehicle span cost.
  switch (::testing::get<1>(GetParam())) {
    case Dimension::NodePrecedence::PerformedConstraint::
        kFirstAndSecondIndependent:
      RunAndExpectSuccess({true, true, false, true, false, true},
                          {1200, 1200, kint64max, 1000, kint64max, 1200});
      break;
    case Dimension::NodePrecedence::PerformedConstraint::kFirstImpliesSecond:
      RunAndExpectSuccess({true, true, false, true, false, true},
                          {1200, 1200, kint64max, 1000, kint64max, 1200});
      break;
    default:
      RunAndExpectSuccess({true, true, false, false, false, true},
                          {1200, 1200, kint64max, kint64max, kint64max, 1200});
  }
}

TEST_P(NodePrecedenceCumulFiltersTest, NodePrecedenceWithBreaksTest) {
  // Add one break constraint of duration 20, starting at 5, to the first
  // vehicle.
  IntervalVar* const break_interval =
      model_->solver()->MakeFixedDurationIntervalVar(
          5 + dimension_offset_, 5 + dimension_offset_, 20, false, "break");
  time_dimension_->SetBreakIntervalsOfVehicle({break_interval}, 0, -1, -1);

  // Make the first vehicle start at its min, and make its start's slack 0, so
  // the break always happens right after the first visit.
  time_dimension_->CumulVar(0)->SetValue(dimension_offset_);
  time_dimension_->SlackVar(0)->SetMax(0);
  // Set Node4's max cumul to 46 so kDeltaSolutions[2] is not valid.
  time_dimension_->CumulVar(4)->SetMax(dimension_offset_ + 46);

  time_dimension_->SetSpanCostCoefficientForAllVehicles(100);

  switch (::testing::get<1>(GetParam())) {
    case Dimension::NodePrecedence::PerformedConstraint::
        kFirstAndSecondIndependent:
      RunAndExpectSuccess({true, false, false, true, true, true},
                          {2000, kint64max, kint64max, 2000, 2000, 2000});
      break;
    case Dimension::NodePrecedence::PerformedConstraint::kFirstImpliesSecond:
      RunAndExpectSuccess({true, false, false, true, true, true},
                          {2000, kint64max, kint64max, 2000, 2000, 2000});
      break;
    default:
      RunAndExpectSuccess({true, false, false, false, true, true},
                          {2000, kint64max, kint64max, kint64max, 2000, 2000});
  }
}

INSTANTIATE_TEST_SUITE_P(
    NodePrecedenceCumulFiltersTestWithUnaryCallback,
    NodePrecedenceCumulFiltersTest,
    ::testing::Combine(
        ::testing::Bool(),
        ::testing::Values(
            Dimension::NodePrecedence::PerformedConstraint::
                kFirstAndSecondIndependent,
            Dimension::NodePrecedence::PerformedConstraint::kSecondImpliesFirst,
            Dimension::NodePrecedence::PerformedConstraint::kFirstImpliesSecond,
            Dimension::NodePrecedence::PerformedConstraint::
                kFirstAndSecondEqual)));

// Tests the filtering on node precedences with both the PathCumulFilter light
// filtering and the LPCumulFilter.
TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       NodePrecedenceFiltersTest) {
  const int kNumNodes = 4;
  const int kNumVehicles = 3;
  const auto [use_unary_callback, solver_type] = GetParam();

  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(solver_type);

  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
  Model model(manager);

  const char* kDimension = "Time";
  const int callback =
      use_unary_callback
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, /*slack_max=*/0,
                     /*capacity=*/kint64max,
                     /*fix_start_cumul_to_zero=*/false, kDimension);

  Dimension* time_dimension = model.GetMutableDimension(kDimension);
  // Precedences: Node1 must be done at or after Node3 - 1, and Node3 at/after
  // Node2 + 1.
  time_dimension->AddNodePrecedence(
      {/*first_node*/ 3, /*second_node*/ 1, /*offset*/ -1});
  time_dimension->AddNodePrecedence(
      {/*first_node*/ 2, /*second_node*/ 3, /*offset*/ 1});

  model.CloseModelWithParameters(parameters);

  // The model should have a GlobalDimensionCumulOptimizer for this dimension.
  GlobalDimensionCumulOptimizer* const lp_optimizer =
      model.GetMutableGlobalCumulLPOptimizer(*time_dimension);
  ASSERT_NE(lp_optimizer, nullptr);
  GlobalDimensionCumulOptimizer* const mp_optimizer =
      model.GetMutableGlobalCumulMPOptimizer(*time_dimension);
  ASSERT_NE(mp_optimizer, nullptr);

  // Initial solution: All nodes unperformed.
  const int kInitialSolution[] = {6, 1, 2, 3, 7, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // Empty delta.
      // Vehicle 0: 0 --> 6
      // Vehicle 1: 4 --> 7
      // Vehicle 2: 5 --> 8
      {6, 1, 2, 3, 7, 8},
      // Vehicle 0: 0 --> 1 --> 2 --> 3 --> 6
      // Vehicle 1: 4 --> 7
      // Vehicle 2: 5 --> 8
      // This solution is accepted by the PathCumulFilter, which subfilters by
      // looking only at the min/max cumul of the first/second_node in a
      // precedence. The solution is not really acceptable though (Node1 can't
      // be done after Node3 - 1), and it gets rejected by the LPCumulFilter.
      {1, 2, 3, 6, 7, 8},
      // Make 2 unperformed, move 1-->3 to vehicle 2.
      // Vehicle 0: 0 --> 6
      // Vehicle 1: 4 --> 7
      // Vehicle 2: 5 --> 1 --> 3 --> 8
      {6, 3, 2, 8, 7, 1},
      // Put 2 on the second vehicle, move 1-->3 to vehicle 2.
      // Vehicle 0: 0 --> 6
      // Vehicle 1: 4 --> 2 --> 7
      // Vehicle 2: 5 --> 1 --> 3 --> 8
      {6, 3, 7, 8, 2, 1}};
  EXPECT_EQ(model.Start(0), 0);
  EXPECT_EQ(model.End(0), 6);
  EXPECT_EQ(model.Start(1), 4);
  EXPECT_EQ(model.End(1), 7);
  EXPECT_EQ(model.Start(2), 5);
  EXPECT_EQ(model.End(2), 8);
  // NOTE: The following restrictions on vehicle start/end times are meant to
  // kill mutants on GetNewSynchronizedUnperformedNodes() in PathCumulFilter.
  // The second vehicle cannot start before 1, or end after 4.
  time_dimension->CumulVar(model.End(0))->SetMax(5);
  time_dimension->CumulVar(model.Start(1))->SetMin(1);
  time_dimension->CumulVar(model.End(1))->SetMax(4);
  time_dimension->CumulVar(model.End(2))->SetMax(3);
  // Filters must be made after the cumul vars have their final bounds.
  const std::vector<LocalSearchFilter*> filters = {
      MakePathCumulFilter(*time_dimension, false, false, true),
      MakeGlobalLPCumulFilter(lp_optimizer, mp_optimizer, false),
      MakeCumulBoundsPropagatorFilter(*time_dimension)};

  const bool kExpectedAccept[][std::size(kDeltaSolutions)] = {
      {true, true, true},
      {false, false, false},
      {true, true, true},
      {false, false, false}};

  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);

  for (int f = 0; f < filters.size(); f++) {
    LocalSearchFilter* const filter = filters[f];
    filter->Synchronize(&initial_solution, nullptr);
    int last_synchronized_delta_index = 0;

    Assignment delta(model.solver());

    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      SCOPED_TRACE(
          absl::StrCat("f = ", f, ", delta = ", d, ", solver = ", solver_type));
      MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                kDeltaSolutions[d], model.Nexts(), &delta);
      const bool expected_accept = kExpectedAccept[d][f];
      EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                expected_accept);
      if (expected_accept) {
        Assignment current_solution(model.solver());
        MakeAssignment(kDeltaSolutions[d], model.Nexts(), &current_solution);
        filter->Synchronize(&current_solution, &delta);
        last_synchronized_delta_index = d;
      }
    }
  }
}

struct ResourceFilterSetup {
  ResourceFilterSetup() : create_filters_independently(false) {}
  ResourceFilterSetup(bool create_filters_independently,
                      std::vector<bool> filter_has_cost)
      : create_filters_independently(create_filters_independently),
        filter_has_cost(std::move(filter_has_cost)) {}
  bool create_filters_independently;
  std::vector<bool> filter_has_cost;
};

using RoutingFilterTestWithResources = ::testing::TestWithParam<
    ::testing::tuple<bool, bool, bool, int64_t, ResourceFilterSetup>>;

// Tests the filtering on node precedences with resources.
TEST_P(RoutingFilterTestWithResources, NodePrecedenceAndResourcesFiltersTest) {
  const int kNumNodes = 4;
  const int kNumVehicles = 3;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  const bool use_unary_callback = ::testing::get<0>(GetParam());
  const bool filter_objective_cost = ::testing::get<1>(GetParam());
  const bool use_cp_sat = ::testing::get<2>(GetParam());
  const int64_t resource_fixed_cost = ::testing::get<3>(GetParam());
  const ResourceFilterSetup& resource_filter_setup =
      ::testing::get<4>(GetParam());

  Model model(manager);

  const char* kDimension = "Time";
  const int callback =
      use_unary_callback
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
          : model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; });
  model.AddDimension(callback, /*slack_max=*/0,
                     /*capacity=*/kint64max,
                     /*fix_start_cumul_to_zero=*/false, kDimension);

  Dimension* time_dimension = model.GetMutableDimension(kDimension);
  // Precedences: Node1 must be done at or after Node3 - 1, and Node3
  // at/after Node2 + 1.
  time_dimension->AddNodePrecedence(
      {/*first_node*/ 3, /*second_node*/ 1, /*offset*/ -1});
  time_dimension->AddNodePrecedence(
      {/*first_node*/ 2, /*second_node*/ 3, /*offset*/ 1});

  Model::ResourceGroup* resource_group = model.AddResourceGroup();
  // Resources: One resource must start before 5, another must end
  // after 10.
  resource_group->AddResource(Attributes(Domain(-1, 5), Domain::AllValues(),
                                         kint64max, resource_fixed_cost),
                              time_dimension);
  resource_group->AddResource(
      Attributes(Domain::AllValues(), Domain(10, kint64max), kint64max,
                 resource_fixed_cost),
      time_dimension);
  for (int v = 0; v < model.vehicles(); v++) {
    resource_group->NotifyVehicleRequiresAResource(v);
  }
  // V0 is used (thus requires a resource) even when empty.
  model.SetVehicleUsedWhenEmpty(true, 0);
  if (use_cp_sat) {
    // Add a trivial break to the vehicles to force the use of CP-SAT.
    IntervalVar* const break_interval =
        model.solver()->MakeFixedDurationIntervalVar(0, 1000, 5, true, "");
    for (int v = 0; v < model.vehicles(); v++) {
      time_dimension->SetBreakIntervalsOfVehicle({break_interval}, v, -1, -1);
    }
  }

  model.CloseModel();

  std::vector<LocalSearchFilterManager::FilterEvent> filters;
  if (!resource_filter_setup.create_filters_independently) {
    AppendDimensionCumulFilters({time_dimension},
                                DefaultRoutingSearchParameters(),
                                filter_objective_cost,
                                /*use_chain_cumul_filter=*/true, &filters);
    // There should be 3 filters:
    // Chain/PathCumulFilter, ResourceAssignmentFilter, LPCumulFilter.
    ASSERT_EQ(filters.size(), 3);
  } else {
    const auto kAccept = LocalSearchFilterManager::FilterEventType::kAccept;
    if (use_cp_sat) {
      filters.push_back(
          {MakePathCumulFilter(*time_dimension,
                               /*propagate_own_objective_value*/ true,
                               filter_objective_cost, true),
           kAccept, /*priority*/ 1});
    } else {
      filters.push_back(
          {MakeChainCumulFilter(*time_dimension), kAccept, /*priority*/ 0});
    }
    filters.push_back(
        {MakeResourceAssignmentFilter(
             model.GetMutableLocalCumulLPOptimizer(*time_dimension),
             model.GetMutableLocalCumulMPOptimizer(*time_dimension),
             /*propagate_own_objective_value*/ true, filter_objective_cost),
         kAccept, /*priority*/ 1});
    filters.push_back(
        {MakeGlobalLPCumulFilter(
             model.GetMutableGlobalCumulLPOptimizer(*time_dimension),
             model.GetMutableGlobalCumulMPOptimizer(*time_dimension),
             filter_objective_cost),
         kAccept, /*priority*/ 1});
  }
  ASSERT_EQ(filters.size(), resource_filter_setup.filter_has_cost.size());

  // Initial solution: All nodes unperformed.
  const int kInitialSolution[] = {6, 1, 2, 3, 7, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // Empty delta.
      {6, 1, 2, 3, 7, 8},
      // V0: NODE0 --> 1 --> 2 --> 3 --> NODE6
      {1, 2, 3, 6, 7, 8},
      // V0: NODE0 --> 3 --> 2 --> 1 --> NODE6
      {3, 6, 1, 2, 7, 8},
      // Make Node2 unperformed.
      {1, 3, 2, 6, 7, 8},
      // V1: NODE4 --> 1 --> 3 --> NODE7, V2: NODE5 --> 2 --> NODE8
      {6, 3, 8, 7, 1, 2},
      // V0: NODE0 --> 1 --> 3 --> NODE6, V1: NODE4 --> 2 --> NODE7
      {1, 3, 7, 6, 2, 8},
      // V0: NODE0 --> 2 --> 1 --> NODE6, V1: NODE4 --> 3 --> NODE7
      {2, 6, 1, 7, 3, 8}};
  const int kExpectedCost[] = {1, 1, 1, 1, 3, 2, 2};
  const bool kExpectedAccept[][std::size(kDeltaSolutions)] = {
      /* Chain/PathCumulFilter */ {true, !use_cp_sat, !use_cp_sat, true, true,
                                   true, true},
      /* ResourceAssignmentFilter */
      {true, true, true, true, false, true, true},
      /* LPCumulFilter */ {true, false, false, true, false, true, false}};

  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);

  for (int f = 0; f < filters.size(); f++) {
    LocalSearchFilter* const filter = filters[f].filter;
    filter->Synchronize(&initial_solution, nullptr);
    int last_synchronized_delta_index = 0;

    Assignment delta(model.solver());

    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                kDeltaSolutions[d], model.Nexts(), &delta);
      const bool expected_accept = kExpectedAccept[f][d];
      EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                expected_accept);
      if (expected_accept) {
        Assignment current_solution(model.solver());
        MakeAssignment(kDeltaSolutions[d], model.Nexts(), &current_solution);
        filter->Synchronize(&current_solution, &delta);
        const int64_t expected_objective_value =
            filter_objective_cost && resource_filter_setup.filter_has_cost[f]
                ? kExpectedCost[d] * resource_fixed_cost
                : 0;
        EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                  expected_objective_value);
        last_synchronized_delta_index = d;
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    RoutingFilterTestWithResourcesScenario, RoutingFilterTestWithResources,
    ::testing::Combine(
        ::testing::Bool(), ::testing::Bool(), ::testing::Bool(),
        ::testing::Values(0, 1000),
        ::testing::Values(
            // Only LPCumulFilter will update resource costs when it is used
            // with other DimensionCumul filters.
            ResourceFilterSetup{/*create_filters_independently=*/false,
                                /*filter_has_cost=*/{false, false, true}},
            ResourceFilterSetup{/*create_filters_independently=*/true,
                                /*filter_has_cost=*/{false, true, true}})),
    [](const ::testing::TestParamInfo<
        RoutingFilterTestWithResources::ParamType>& info) {
      const bool use_unary_callback = ::testing::get<0>(info.param);
      const bool filter_objective_cost = ::testing::get<1>(info.param);
      const bool use_cp_sat = ::testing::get<2>(info.param);
      const int64_t resource_fixed_cost = ::testing::get<3>(info.param);
      const ResourceFilterSetup& resource_filter_setup =
          ::testing::get<4>(info.param);
      return absl::StrCat(
          "use_unary_callback_", use_unary_callback, "_",
          "filter_objective_cost_", filter_objective_cost, "_", "use_cp_sat_",
          use_cp_sat, "_", "resource_fixed_cost_", resource_fixed_cost, "_",
          "create_filters_independently_",
          resource_filter_setup.create_filters_independently, "_",
          "filter_has_cost_",
          absl::StrJoin(resource_filter_setup.filter_has_cost, "_"));
    });

TEST(RoutingFilterTest, NodePrecedenceFiltersSynchronizeWithEmptyAssignment) {
  IndexManager manager(3, 1, NodeIndex(0));
  Model model(manager);
  model.AddDimension(
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1LL; }),
      /*slack_max=*/0, /*capacity=*/kint64max,
      /*fix_start_cumul_to_zero=*/false, "dim");
  Dimension* dimension = model.GetMutableDimension("dim");
  dimension->AddNodePrecedence(
      {/*first_node*/ 2, /*second_node*/ 1, /*offset*/ 1});

  model.CloseModel();

  // The model should have a GlobalDimensionCumulOptimizer for this dimension.
  GlobalDimensionCumulOptimizer* const lp_optimizer =
      model.GetMutableGlobalCumulLPOptimizer(*dimension);
  ASSERT_NE(lp_optimizer, nullptr);
  GlobalDimensionCumulOptimizer* const mp_optimizer =
      model.GetMutableGlobalCumulMPOptimizer(*dimension);
  ASSERT_NE(mp_optimizer, nullptr);

  const std::vector<LocalSearchFilter*> filters = {
      MakePathCumulFilter(*dimension, false, false, true),
      MakeGlobalLPCumulFilter(lp_optimizer, mp_optimizer, false),
      MakeCumulBoundsPropagatorFilter(*dimension)};

  Assignment empty(model.solver());

  for (LocalSearchFilter* const filter : filters) {
    filter->Synchronize(&empty, nullptr);
  }
}

class PickupToDeliveryLimitTest : public ::testing::Test {
 protected:
  static const int kInitialSolution[];
  static const int kDeltaSolutions[][6];
  static const int kExpectedDeltaCost[];

  ~PickupToDeliveryLimitTest() override = default;
  void SetUp() override {
    const int kNumNodes = 5;
    const int kNumVehicles = 2;

    IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);
    // Two pickup/delivery pairs: 1-->2 and 3-->4.
    model_->AddPickupAndDelivery(1, 2);
    model_->AddPickupAndDelivery(3, 4);
  }

  Dimension* CreateDimensionWithPickupDeliveryLimits() {
    const char* kDimension = "test_dimension";
    model_->AddConstantDimensionWithSlack(
        /*value*/ 2, /*capacity*/ kint64max,
        /*slack_max*/ kint64max,
        /*fix_start_cumul_to_zero*/ false, kDimension);

    Dimension* dimension = model_->GetMutableDimension(kDimension);
    const int64_t kPickupDeliveryLimits[] = {/*first pair*/ 5,
                                             /*second pair*/ 4};

    const int num_pickup_delivery_pairs =
        model_->GetPickupAndDeliveryPairs().size();
    CHECK_EQ(std::size(kPickupDeliveryLimits), num_pickup_delivery_pairs);
    for (int i = 0; i < num_pickup_delivery_pairs; i++) {
      const int64_t limit = kPickupDeliveryLimits[i];
      dimension->SetPickupToDeliveryLimitFunctionForPair(
          [limit](int /*p*/, int /*d*/) { return limit; }, i);
    }

    const int64_t kWindows[][2] = {{0, 5}, {8, 15}, {8, 11}, {13, 13}};
    for (int i = 0; i < std::size(kWindows); i++) {
      dimension->CumulVar(i + 1)->SetRange(kWindows[i][0], kWindows[i][1]);
    }

    // Add span cost coefficients.
    dimension->SetSpanCostCoefficientForAllVehicles(1);
    dimension->SetGlobalSpanCostCoefficient(10);
    // Add a soft_end_time to the route starts so the LP is used on both routes
    // when the objective cost is filtered.
    // We make it large so it doesn't impact the cost.
    dimension->SetCumulVarSoftUpperBound(0, 100, 5);
    dimension->SetCumulVarSoftUpperBound(5, 100, 5);

    model_->CloseModel();

    return dimension;
  }

  std::unique_ptr<Model> model_;
};

// Initial solution: 1-->2 on first route, 3-->4 on second route.
const int PickupToDeliveryLimitTest::kInitialSolution[] = {1, 2, 6, 4, 7, 3};
// In the following, nodes 1 and 2 are referred to as p1 and d1 (first pickup
// and delivery), and 3 and 4 as p2 and d2.
const int PickupToDeliveryLimitTest::kDeltaSolutions[][6] = {
    // No change, empty delta.
    // On each route the delivery is visited right after the pickup, so the
    // limits are respected.
    {1, 2, 6, 4, 7, 3},
    // Put all nodes on a single route: p1-->d1-->p2-->d2.
    // Just as before, each delivery comes right after its pickup, so the
    // limits are once again respected.
    {1, 2, 3, 4, 6, 7},
    // Single route: p1-->p2-->d1-->d2.
    // Here, we can't respect both limits simultaneously.
    // To go from p1 to d1 in 5 units, we should leave p1 at 5 (upper limit),
    // p2 at 8 and then get to d1 at 10.
    // To get from p2 to d2 in 4 units, we should leave p2 at 9, d1 at 11, and
    // get to d2 at 13.
    // So both p-->d limits cannot be respected simultaneously: either we
    // respect the first by leaving p2 at 8, or the second by leaving at 9.
    // This is however a case of sub-filtering by the PathCumulFilter without
    // LP, which doesn't take the feasibility of the two limits into account
    // simultaneously.
    {1, 3, 4, 2, 6, 7},
    // Single route: p1-->p2-->d2-->d1.
    // The limit for p2-->d2 is respected, but not for p1-->d1.
    {1, 3, 6, 4, 2, 7},
    // First route: p2-->d1-->d2. p1 on second route.
    // Valid, we can go from p2 to d2 (only pair on the same route) in 4.
    {3, 7, 4, 2, 6, 1},
    // First route: p1-->d2-->d1. p2 on second route.
    // We get to d1 at 15 at best, so p1-->d1 violated.
    {1, 4, 6, 7, 2, 3}};

// NOTE: The part of the span costs related to transit is not included by the
// filters.
const int PickupToDeliveryLimitTest::kExpectedDeltaCost[] = {
    133 - (6 + 6), 134 - (10 + 2), 134 - (10 + 2), -1, 132 - (8 + 4), -1};

TEST_F(PickupToDeliveryLimitTest, FilterWithAndWithoutLPTest) {
  const Dimension* const dimension = CreateDimensionWithPickupDeliveryLimits();

  // The model should have a GlobalDimensionCumulOptimizer for this dimension.
  GlobalDimensionCumulOptimizer* const lp_optimizer =
      model_->GetMutableGlobalCumulLPOptimizer(*dimension);
  ASSERT_NE(lp_optimizer, nullptr);
  GlobalDimensionCumulOptimizer* const mp_optimizer =
      model_->GetMutableGlobalCumulMPOptimizer(*dimension);
  ASSERT_NE(mp_optimizer, nullptr);

  const std::vector<LocalSearchFilter*> filters = {
      /*PathCumulFilter without LP*/
      MakePathCumulFilter(*dimension, true,
                          /*filter_objective_cost=*/false, true),
      /*PathCumulFilter with LP*/
      MakePathCumulFilter(*dimension, true,
                          /*filter_objective_cost=*/true, true),
      /*GlobalLPCumulFilter*/
      MakeGlobalLPCumulFilter(lp_optimizer, mp_optimizer,
                              /*filter_objective_cost=*/true)};

  const bool kExpectedAcceptDelta[][3] = {
      {true, true, true},    {true, true, true}, {false, false, false},
      {false, false, false}, {true, true, true}, {false, false, false}};

  const Model* const model = dimension->model();
  Solver* const solver = model->solver();
  Assignment initial_solution(solver);

  MakeAssignment(kInitialSolution, model->Nexts(), &initial_solution);

  for (int f = 0; f < filters.size(); f++) {
    LocalSearchFilter* const filter = filters[f];
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
              (f == 0) ? 0 : kExpectedDeltaCost[0]);

    Assignment delta(solver);
    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      MakeDelta(kInitialSolution, kDeltaSolutions[d], model->Nexts(), &delta);
      const bool expect_accept = kExpectedAcceptDelta[d][f];
      EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                expect_accept);
      if (expect_accept) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(),
                  (f == 0) ? 0 : kExpectedDeltaCost[d]);
      }
    }
  }
}

TEST_F(PickupToDeliveryLimitTest, AppendDimensionCumulFiltersTest) {
  Dimension* const dimension = CreateDimensionWithPickupDeliveryLimits();

  std::vector<LocalSearchFilterManager::FilterEvent> filters;
  AppendDimensionCumulFilters({dimension}, DefaultRoutingSearchParameters(),
                              /*filter_objective_cost=*/true,
                              /*use_chain_cumul_filter=*/true, &filters);
  // The PathCumulFilter doesn't propagate its own objective value when called
  // before the global LPCumulFilter.
  const bool propagates_objective_value[] = {false, true};

  const auto expected_delta_cost = [&propagates_objective_value](int delta,
                                                                 int filter) {
    return propagates_objective_value[filter] ? kExpectedDeltaCost[delta] : 0;
  };

  const bool kExpectedAcceptDelta[] = {true, true, false, false, true, false};

  const Model* const model = dimension->model();
  Solver* const solver = model->solver();
  Assignment initial_solution(solver);
  for (int i = 0; i < std::size(kInitialSolution); i++) {
    initial_solution.Add(model->Nexts()[i])->SetValue(kInitialSolution[i]);
  }

  for (int f = 0; f < filters.size(); f++) {
    LocalSearchFilter* const filter = filters[f].filter;
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
              expected_delta_cost(0, f));

    Assignment delta(solver);
    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      MakeDelta(kInitialSolution, kDeltaSolutions[d], model->Nexts(), &delta);
      EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                kExpectedAcceptDelta[d]);
      if (kExpectedAcceptDelta[d]) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(),
                  expected_delta_cost(d, f));
      }
      filter->Revert();
    }
  }
}

using PathCumulFilterTest = ::testing::TestWithParam<bool>;

TEST_P(PathCumulFilterTest, BreaksInTimeCost) {
  // A few scenarios where a break may influence cost.
  const int kNodes = 3;
  const int kVehicles = 1;
  const char* kDimension = "time";
  // The route will be start -> visit1 -> visit2 -> end.
  const int kInitialSolution[] = {3, 1, 2};
  const int kDeltaSolution[] = {1, 2, 3};
  const int64_t kVehicleSpanCost = 3;
  const std::vector<int64_t> kTransits = {0, 7, 0};
  // kNodeWindows[scenario] = {node1 min, node1 max, node2 min, node2 max}.
  const std::vector<std::vector<int64_t>> kNodeWindows = {
      {1, 10, 1, 10},    // Route must contain break.
      {0, 10, 1, 10},    // Route can be before break.
      {1, 10, 1, 11},    // Route can be after break.
      {10, 11, 20, 30},  // Route must contain break, and min slack = 2.
      {10, 11, 22, 30},  // Route must contain break, and min slack = 4.
      {10, 11, 22, 30},  // Route must contain break, and min slack = 4.
      {1, 9, 1, 9}       // Route must contain break, but is infeasible.
  };
  const std::vector<std::vector<int64_t>> kBreakWindows = {
      // {start min, start max, duration min}.
      {2, 7, 2},  {2, 7, 2},  {2, 7, 2}, {9, 19, 3},
      {9, 21, 3}, {9, 10, 3}, {0, 7, 3}};
  // NOTE: Best possible route length are reached by the following schedules:
  // 0: start[1), node1[1), travel[1,2), break[2,4), travel[4,10), node2[10),
  // end[10).
  // 1: start[0), node1[0), travel[0,7), node2[7), end[7), break[7,9).
  // 2: break[2,4), start[4), node1[4), travel[4,11), node2[11), end[11).
  // 3: start[10), node1[10), break[10,13), travel[13,20), node2[20), end[20).
  // 4: start[11), node1[11), break[11,14), travel[14,21), node2[22), end[22).
  // 5: start[10), node1[10), break[10,13), travel[13,20), node2[22), end[22).
  // 6: infeasible
  const std::vector<int64_t> kBestRouteLength = {9,  7,  7,        10,
                                                 11, 12, kint64max};
  const int64_t kTotalTransit = 7;

  for (const int64_t route_length_limit : {6, 7, 9, 10, 11, 12, 1000}) {
    const int num_scenarios = kNodeWindows.size();
    for (int s = 0; s < num_scenarios; ++s) {
      IndexManager manager(kNodes, kVehicles, NodeIndex(0));
      Model model(manager);
      const int callback =
          GetParam()
              ? model.RegisterUnaryTransitCallback(
                    [&](int64_t node) { return kTransits[node]; })
              : model.RegisterTransitCallback(
                    [&](int64_t node1, int64_t) { return kTransits[node1]; });
      model.AddDimension(callback,
                         /*slack_max=*/kint64max,
                         /*capacity=*/1000,
                         /*fix_start_cumul_to_zero=*/false, kDimension);
      Dimension* dimension = model.GetMutableDimension(kDimension);
      if (route_length_limit != 1000) {
        dimension->SetSpanUpperBoundForVehicle(route_length_limit, 0);
      }
      dimension->SetSpanCostCoefficientForVehicle(kVehicleSpanCost, 0);
      for (int node = 1; node < kNodes; ++node) {
        const int64_t node_min = kNodeWindows[s][2 * (node - 1)];
        const int64_t node_max = kNodeWindows[s][2 * (node - 1) + 1];
        dimension->CumulVar(node)->SetRange(node_min, node_max);
      }
      IntervalVar* interval = model.solver()->MakeFixedDurationIntervalVar(
          kBreakWindows[s][0], kBreakWindows[s][1], kBreakWindows[s][2], false,
          "break");
      dimension->SetBreakIntervalsOfVehicle({interval}, 0, {0, 0, 0, 0});
      model.CloseModel();
      for (const bool may_use_optimizers : {true, false}) {
        SCOPED_TRACE(absl::StrCat("Running scenario ", s,
                                  " route_length_limit ", route_length_limit,
                                  " can use lp ", may_use_optimizers));
        LocalSearchFilter* const filter =
            MakePathCumulFilter(*dimension, true, true, may_use_optimizers);
        Solver* const solver = model.solver();
        Assignment initial_solution(solver);
        MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
        filter->Synchronize(&initial_solution, nullptr);
        Assignment delta(solver);
        MakeDelta(kInitialSolution, kDeltaSolution, model.Nexts(), &delta);
        const bool accepted =
            filter->Accept(&delta, nullptr, kint64min, kint64max);
        const int64_t expected_route_length = kBestRouteLength[s];
        EXPECT_EQ(accepted, expected_route_length <= route_length_limit);
        if (accepted) {
          EXPECT_EQ(
              filter->GetAcceptedObjectiveValue(),
              kVehicleSpanCost * CapSub(expected_route_length, kTotalTransit));
        }
      }
    }
  }
}

TEST_P(PathCumulFilterTest, SimpleInterbreakReasoningInPathCumulFilter) {
  // Set up a problem with one visit, two interbreak constraints, only the
  // second interbreak can have an influence. Interbreaks are 1000/100, 100/10,
  // total travel and max span depend on the scenario.
  const int kNodes = 2;
  const int kVehicles = 1;
  const char* kDimension = "time";
  // The route will go from start -> end to start -> visit -> end.
  const int kInitialSolution[] = {2, 1};
  const int kDeltaSolution[] = {1, 2};
  // We test that the filter has a lower bound on the trivial case where
  // the travel time is longer than an interbreak limit.
  // Travel values: totals of 100, 101, 200, 201, 300, 301.
  // Associated num breaks: 0, 1, 1, 2, 2, 3.
  // Associated min spans = travel + num_breaks * min_break_duration:
  // 100, 111, 210, 221, 320, 331.
  const int64_t kMaxInterbreakDuration = 100;
  const int64_t kMinBreakDuration = 10;
  const std::vector<int64_t> kTransits = {100, 101, 200, 201, 300, 301};
  const std::vector<int64_t> kMinSpans = {100, 111, 210, 221, 320, 331};
  const int64_t kVehicleSpanCost = 3;

  for (const bool max_span_is_feasible : {true, false}) {
    const int num_scenarios = kTransits.size();
    for (int s = 0; s < num_scenarios; ++s) {
      IndexManager manager(kNodes, kVehicles, NodeIndex(0));
      Model model(manager);
      const int callback =
          GetParam()
              ? model.RegisterUnaryTransitCallback(
                    [&](int64_t node) { return node == 1 ? kTransits[s] : 0; })
              : model.RegisterTransitCallback([&](int64_t node1, int64_t) {
                  return node1 == 1 ? kTransits[s] : 0;
                });
      model.AddDimension(callback,
                         /*slack_max=*/kint64max,
                         /*capacity=*/1000,
                         /*fix_start_cumul_to_zero=*/false, kDimension);
      model.CloseModel();
      Dimension* dimension = model.GetMutableDimension(kDimension);
      const int64_t max_span =
          max_span_is_feasible ? kMinSpans[s] : (kMinSpans[s] - 1);
      dimension->SetSpanUpperBoundForVehicle(max_span, 0);
      dimension->SetSpanCostCoefficientForVehicle(kVehicleSpanCost, 0);
      // Add break variables and interbreak constraints.
      std::vector<IntervalVar*> break_intervals;
      const int num_intervals = 3;
      for (int i = 0; i < num_intervals; ++i) {
        break_intervals.push_back(model.solver()->MakeFixedDurationIntervalVar(
            0, 1000, 1, false, "break"));
      }
      dimension->SetBreakIntervalsOfVehicle(break_intervals, 0, {0, 0, 0, 0});
      dimension->SetBreakDistanceDurationOfVehicle(kMaxInterbreakDuration,
                                                   kMinBreakDuration, 0);
      // Make the filter, check acceptance and cost.
      SCOPED_TRACE(absl::StrCat("Running scenario ", s, " min span ",
                                kMinSpans[s], " max_span_is_feasible ",
                                max_span_is_feasible));
      LocalSearchFilter* const filter =
          MakePathCumulFilter(*dimension, true, true, false);
      Solver* const solver = model.solver();
      Assignment initial_solution(solver);
      MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
      filter->Synchronize(&initial_solution, nullptr);
      Assignment delta(solver);
      MakeDelta(kInitialSolution, kDeltaSolution, model.Nexts(), &delta);
      const bool accepted =
          filter->Accept(&delta, nullptr, kint64min, kint64max);
      EXPECT_EQ(accepted, max_span_is_feasible);
      if (accepted) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(),
                  kVehicleSpanCost * CapSub(kMinSpans[s], kTransits[s]));
      }
    }
  }
}

TEST_P(PathCumulFilterTest, SoftSpanLimitInLP) {
  // Make model with one vehicle, two shipments.
  constexpr int kNodes = 3;
  constexpr int kVehicles = 1;
  const char* kDimension = "time";
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback(
                [&](int64_t) -> int64_t { return 1; })
          : model.RegisterTransitCallback([&](int64_t, int64_t) { return 1; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/10000,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  Dimension* dimension = model.GetMutableDimension(kDimension);
  // Visit 1 has soft start at 10 with cost per unit 2,
  // visit 2 has soft end at 0 with cost 1,
  // soft span limit is at 5 with cost 10.
  dimension->SetCumulVarSoftLowerBound(1, 10, 2);
  dimension->SetCumulVarSoftUpperBound(2, 0, 1);
  dimension->SetSoftSpanUpperBoundForVehicle({5, 10}, 0);
  model.CloseModel();

  // The filter under test is PathCumulFilter, it must use LP.
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*dimension, true, true, true);
  Solver* const solver = model.solver();
  // Initial solution: start -> 1 -> 2 -> end.
  // The minimal solution is visit_1 at 10, visit_2 at 11, cost 11.
  Assignment initial_solution(solver);
  const int kInitialSolution[] = {1, 2, 3};
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(11, filter->GetSynchronizedObjectiveValue());
  // Delta solution: start -> 2 -> 1 -> end.
  // If there were no soft span limit, the optimal would be
  // visit_2 at 1, visit 1 at 10, with cost 1.
  // With the soft span cost, this solution has total cost 100.
  // To accommodate the soft span limit, the route must be short enough, so the
  // optimal is start at 6, visit_2 at 7, visit 1 at 10, end at 11, cost 7.
  Assignment delta(solver);
  const int kDeltaSolution[] = {2, 3, 1};
  MakeDelta(kInitialSolution, kDeltaSolution, model.Nexts(), &delta);
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  EXPECT_EQ(7, filter->GetAcceptedObjectiveValue());
  filter->Synchronize(&delta, nullptr);
  EXPECT_EQ(7, filter->GetSynchronizedObjectiveValue());
}

TEST_P(PathCumulFilterTest, SoftSpanLimitInLPEmptyVehicleNotConsidered) {
  // Make model with one vehicle, two shipments.
  constexpr int kNodes = 3;
  constexpr int kVehicles = 1;
  const char* kDimension = "time";
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback(
                [&](int64_t) -> int64_t { return 1; })
          : model.RegisterTransitCallback([&](int64_t, int64_t) { return 1; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/10000,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  model.AddDisjunction({1}, 0, /*max_cardinality=*/1);
  model.AddDisjunction({2}, 0, /*max_cardinality=*/1);
  Dimension* dimension = model.GetMutableDimension(kDimension);
  // The filter under test is PathCumulFilter, it must use LP,
  // set a route limit along soft span cost to force LP usage.
  // The optimal solution in every case is to start at 10, end at 20, cost 100.
  dimension->SetSpanCostCoefficientForVehicle(5, 0);
  dimension->SetSoftSpanUpperBoundForVehicle({5, 10}, 0);
  dimension->CumulVar(0)->SetRange(0, 10);
  dimension->CumulVar(3)->SetRange(20, 30);
  model.CloseModel();
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*dimension, true, true, true);
  Solver* const solver = model.solver();
  // Initial solution: start -> 1 -> 2 -> end.
  Assignment initial_solution(solver);
  const int kInitialSolution[] = {1, 2, 3};
  Assignment delta(solver);
  const int kEmptySolution[] = {3, 1, 2};

  // Vehicle cost is considered, the cost is 100 in non-empty and empty cases.
  // PathCumulFilter removes the fixed transit cost of 15 and 5, hence the final
  // values.
  model.SetVehicleUsedWhenEmpty(true, 0);
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(100 - 15, filter->GetSynchronizedObjectiveValue());
  {
    MakeDelta(kInitialSolution, kEmptySolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(100 - 5, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(100 - 5, filter->GetSynchronizedObjectiveValue());
  }
  // Vehicle cost is not considered, the empty case should yield 0 cost.
  model.SetVehicleUsedWhenEmpty(false, 0);
  {
    MakeDelta(kEmptySolution, kInitialSolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(100 - 15, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(100 - 15, filter->GetSynchronizedObjectiveValue());
  }
  {
    MakeDelta(kInitialSolution, kEmptySolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(0, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());
  }
}

TEST_P(PathCumulFilterTest, SoftSpanLimitInFilter) {
  // Make model with one vehicle, two shipments.
  constexpr int kNodes = 5;
  constexpr int kVehicles = 1;
  const char* kDimension = "time";
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback(
                [&](int64_t) -> int64_t { return 1; })
          : model.RegisterTransitCallback([&](int64_t, int64_t) { return 1; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/10000,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  model.CloseModel();
  Dimension* dimension = model.GetMutableDimension(kDimension);
  // Soft span limit is at 5 with cost 10.
  dimension->SetSoftSpanUpperBoundForVehicle({6, 10}, 0);
  dimension->CumulVar(0)->SetRange(0, 0);
  dimension->CumulVar(4)->SetRange(4, 10);
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*dimension, true, true, true);
  Solver* const solver = model.solver();
  Assignment initial_solution(solver);
  // Path: 0 1 2 3 4 5
  // Time: 0 1 2 3 4 5
  std::vector<int> old_solution = {1, 2, 3, 4, 5};
  MakeAssignment(old_solution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());

  // Forcing end to be later increases the minimal span.
  // We swap the internal nodes of the path to force a filter update.
  // First, increase at the soft span limit, 5.
  Assignment delta(solver);
  // Path: 0 1 2 4 3 5
  // Time: 0 1 2 4 5 6
  std::vector<int> new_solution = {1, 2, 4, 5, 3};
  {
    MakeDelta(old_solution, new_solution, model.Nexts(), &delta);
    ASSERT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(0, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&initial_solution, &delta);
    initial_solution.CopyIntersection(&delta);
    EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());
  }
  // Increasing the minimal span to 6, the filter should increase cost to 10.
  // Path: 0 1 4 2 3 5
  // Time: 0 1 4 5 6 7
  old_solution = new_solution;
  new_solution = {1, 4, 3, 5, 2};
  {
    MakeDelta(old_solution, new_solution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(10, filter->GetAcceptedObjectiveValue()) << delta.DebugString();
    filter->Synchronize(&initial_solution, &delta);
    initial_solution.CopyIntersection(&delta);
    EXPECT_EQ(10, filter->GetSynchronizedObjectiveValue());
  }
  // Increasing the minimal span to 7, the filter should increase cost to 20.
  // Path: 0 4 1 2 3 5
  // Time: 0 4 5 6 7 8
  old_solution = new_solution;
  new_solution = {4, 2, 3, 5, 1};
  {
    MakeDelta(old_solution, new_solution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(20, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&initial_solution, &delta);
    initial_solution.CopyIntersection(&delta);
    EXPECT_EQ(20, filter->GetSynchronizedObjectiveValue());
  }
}

// When a vehicle is empty, its cost might not be considered
TEST_P(PathCumulFilterTest, SoftSpanLimitInFilterEmptyVehicle) {
  // Make model with one vehicle, two shipments.
  constexpr int kNodes = 3;
  constexpr int kVehicles = 1;
  const char* kDimension = "time";
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback(
                [&](int64_t) -> int64_t { return 1; })
          : model.RegisterTransitCallback([&](int64_t, int64_t) { return 1; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/10000,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  model.CloseModel();
  Dimension* dimension = model.GetMutableDimension(kDimension);
  model.AddDisjunction({1}, 0, /*max_cardinality=*/1);
  model.AddDisjunction({2}, 0, /*max_cardinality=*/1);
  // Soft span limit is at 5 with cost 10, span is at least 7.
  dimension->SetSoftSpanUpperBoundForVehicle({5, 10}, 0);
  dimension->CumulVar(0)->SetRange(0, 0);
  dimension->CumulVar(3)->SetRange(7, 10);
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*dimension, true, true, true);
  Solver* const solver = model.solver();
  model.SetVehicleUsedWhenEmpty(true, 0);
  // Initial solution: start -> 1 -> 2 -> end.
  // The minimal solution is visit_1 at 1, visit_2 at 2, end at 3, cost 0.
  Assignment initial_solution(solver);
  const int kInitialSolution[] = {1, 2, 3};
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(20, filter->GetSynchronizedObjectiveValue());

  // We switch between the initial solution and the empty one to force filter
  // update. Even if empty, the vehicle should add to the cost.
  const int kEmptySolution[] = {3, 1, 2};
  Assignment delta(solver);
  {
    MakeDelta(kInitialSolution, kEmptySolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(20, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(20, filter->GetSynchronizedObjectiveValue());
  }
  // Setting the vehicle to not incur cost when empty.
  model.SetVehicleUsedWhenEmpty(false, 0);
  // When nonempty, check that it has soft span cost.
  {
    MakeDelta(kEmptySolution, kInitialSolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(20, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(20, filter->GetSynchronizedObjectiveValue());
  }
  // When empty, check that it has no soft span cost.
  {
    MakeDelta(kInitialSolution, kEmptySolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(0, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());
  }
}

TEST_P(PathCumulFilterTest, QuadraticSoftSpanLimitInLP) {
  // Make model with one vehicle, two shipments.
  constexpr int kNodes = 3;
  constexpr int kVehicles = 1;
  const char* kDimension = "time";
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback(
                [&](int64_t) -> int64_t { return 1; })
          : model.RegisterTransitCallback([&](int64_t, int64_t) { return 1; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/10000,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  Dimension* dimension = model.GetMutableDimension(kDimension);
  // Visit 1 has soft start at 10 with cost per unit 2,
  // visit 2 has soft end at 0 with cost 1,
  // quadratic soft span limit is at 5 with cost 10.
  dimension->SetCumulVarSoftLowerBound(1, 10, 2);
  dimension->SetCumulVarSoftUpperBound(2, 0, 1);
  dimension->SetQuadraticCostSoftSpanUpperBoundForVehicle({5, 10}, 0);
  model.CloseModel();

  // The filter under test is PathCumulFilter, it must use LP.
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*dimension, true, true, true);
  Solver* const solver = model.solver();
  // Initial solution: start -> 1 -> 2 -> end.
  // The minimal solution is visit_1 at 10, visit_2 at 11, cost 11.
  Assignment initial_solution(solver);
  const int kInitialSolution[] = {1, 2, 3};
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(11, filter->GetSynchronizedObjectiveValue());
  // Delta solution: start -> 2 -> 1 -> end.
  // If there were no quadratic soft span limit, the optimal would be
  // visit_2 at 1, visit 1 at 10, with cost 1.
  // With the quadratic soft span cost, this solution has total cost (12-5)²*10.
  // To avoid the soft span limit, the route must be short enough, so the
  // optimal is start at 6, visit_2 at 7, visit 1 at 10, end at 11, cost 7.
  Assignment delta(solver);
  const int kDeltaSolution[] = {2, 3, 1};
  MakeDelta(kInitialSolution, kDeltaSolution, model.Nexts(), &delta);
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  EXPECT_EQ(7, filter->GetAcceptedObjectiveValue());
  filter->Synchronize(&delta, nullptr);
  EXPECT_EQ(7, filter->GetSynchronizedObjectiveValue());
}

TEST_P(PathCumulFilterTest,
       QuadraticSoftSpanLimitInLPEmptyVehicleNotConsidered) {
  // Make model with one vehicle, two shipments.
  constexpr int kNodes = 3;
  constexpr int kVehicles = 1;
  const char* kDimension = "time";
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback(
                [&](int64_t) -> int64_t { return 1; })
          : model.RegisterTransitCallback([&](int64_t, int64_t) { return 1; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/10000,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  model.AddDisjunction({1}, 0, /*max_cardinality=*/1);
  model.AddDisjunction({2}, 0, /*max_cardinality=*/1);
  Dimension* dimension = model.GetMutableDimension(kDimension);
  // The filter under test is PathCumulFilter, it must use LP,
  // set a route limit along soft span cost to force LP usage.
  // The optimal solution in every case is to start at 10, end at 20, cost 100.
  dimension->SetSpanCostCoefficientForVehicle(5, 0);
  dimension->SetQuadraticCostSoftSpanUpperBoundForVehicle({5, 10}, 0);
  dimension->CumulVar(0)->SetRange(0, 10);
  dimension->CumulVar(3)->SetRange(20, 30);
  model.CloseModel();
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*dimension, true, true, true);
  Solver* const solver = model.solver();
  // Initial solution: start -> 1 -> 2 -> end.
  Assignment initial_solution(solver);
  const int kInitialSolution[] = {1, 2, 3};
  Assignment delta(solver);
  const int kEmptySolution[] = {3, 1, 2};

  // Vehicle cost is considered, the cost is 100 in non-empty and empty cases.
  // PathCumulFilter removes the fixed transit cost of 15 and 5, hence the final
  // values.
  model.SetVehicleUsedWhenEmpty(true, 0);
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(50 + 5 * 5 * 10 - 15, filter->GetSynchronizedObjectiveValue());
  {
    MakeDelta(kInitialSolution, kEmptySolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(50 + 5 * 5 * 10 - 5, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(50 + 5 * 5 * 10 - 5, filter->GetSynchronizedObjectiveValue());
  }
  // Vehicle cost is not considered, the empty case should yield 0 cost.
  model.SetVehicleUsedWhenEmpty(false, 0);
  {
    MakeDelta(kEmptySolution, kInitialSolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(50 + 5 * 5 * 10 - 15, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(50 + 5 * 5 * 10 - 15, filter->GetSynchronizedObjectiveValue());
  }
  {
    MakeDelta(kInitialSolution, kEmptySolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(0, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());
  }
}

TEST_P(PathCumulFilterTest, QuadraticCostSoftSpanLimitInFilter) {
  // Make model with one vehicle, two shipments.
  constexpr int kNodes = 5;
  constexpr int kVehicles = 1;
  const char* kDimension = "time";
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback(
                [&](int64_t) -> int64_t { return 1; })
          : model.RegisterTransitCallback([&](int64_t, int64_t) { return 1; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/10000,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  model.CloseModel();
  Dimension* dimension = model.GetMutableDimension(kDimension);
  // Soft span limit is at 5 with cost 10.
  dimension->SetQuadraticCostSoftSpanUpperBoundForVehicle({6, 10}, 0);
  dimension->CumulVar(0)->SetRange(0, 0);
  dimension->CumulVar(4)->SetRange(4, 10);
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*dimension, true, true, true);
  Solver* const solver = model.solver();
  Assignment initial_solution(solver);
  // Path: 0 1 2 3 4 5
  // Time: 0 1 2 3 4 5
  std::vector<int> old_solution = {1, 2, 3, 4, 5};
  MakeAssignment(old_solution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());

  // Forcing end to be later increases the minimal span.
  // We swap the internal nodes of the path to force a filter update.
  // First, increase at the soft span limit, 5.
  Assignment delta(solver);
  // Path: 0 1 2 4 3 5
  // Time: 0 1 2 4 5 6
  std::vector<int> new_solution = {1, 2, 4, 5, 3};
  {
    MakeDelta(old_solution, new_solution, model.Nexts(), &delta);
    ASSERT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(0, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&initial_solution, &delta);
    initial_solution.CopyIntersection(&delta);
    EXPECT_EQ(0, filter->GetSynchronizedObjectiveValue());
  }
  // Increasing the minimal span to 6, the filter should increase cost to 10.
  // Path: 0 1 4 2 3 5
  // Time: 0 1 4 5 6 7
  old_solution = new_solution;
  new_solution = {1, 4, 3, 5, 2};
  {
    MakeDelta(old_solution, new_solution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(10, filter->GetAcceptedObjectiveValue()) << delta.DebugString();
    filter->Synchronize(&initial_solution, &delta);
    initial_solution.CopyIntersection(&delta);
    EXPECT_EQ(10, filter->GetSynchronizedObjectiveValue());
  }
  // Increasing the minimal span to 7, the filter should increase cost to 20.
  // Path: 0 4 1 2 3 5
  // Time: 0 4 5 6 7 8
  old_solution = new_solution;
  new_solution = {4, 2, 3, 5, 1};
  {
    MakeDelta(old_solution, new_solution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(40, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&initial_solution, &delta);
    initial_solution.CopyIntersection(&delta);
    EXPECT_EQ(40, filter->GetSynchronizedObjectiveValue());
  }
}

// When a vehicle is empty, its cost might not be considered
TEST_P(PathCumulFilterTest, QuadraticCostSoftSpanLimitInFilterEmptyVehicle) {
  // Make model with one vehicle, two shipments.
  constexpr int kNodes = 3;
  constexpr int kVehicles = 1;
  const char* kDimension = "time";
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      GetParam()
          ? model.RegisterUnaryTransitCallback(
                [&](int64_t) -> int64_t { return 1; })
          : model.RegisterTransitCallback([&](int64_t, int64_t) { return 1; });
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/10000,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  model.CloseModel();
  Dimension* dimension = model.GetMutableDimension(kDimension);
  model.AddDisjunction({1}, 0, /*max_cardinality=*/1);
  model.AddDisjunction({2}, 0, /*max_cardinality=*/1);
  // Quadratic cost soft span limit is at 5 with cost 10, span is at least 7.
  dimension->SetQuadraticCostSoftSpanUpperBoundForVehicle({5, 10}, 0);
  dimension->CumulVar(0)->SetRange(0, 0);
  dimension->CumulVar(3)->SetRange(7, 10);
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*dimension, true, true, true);
  Solver* const solver = model.solver();
  // Start with empty solution.
  Assignment empty_solution(solver);
  const int kEmptySolution[] = {3, 1, 2};
  MakeAssignment(kEmptySolution, model.Nexts(), &empty_solution);
  filter->Synchronize(&empty_solution, nullptr);
  // Make the solver switch between empty and nonempty solution,
  // there is a difference in cost iff empty route costs are not considered.
  Assignment delta(solver);
  const int kNonEmptySolution[] = {1, 2, 3};
  for (const bool consider_empty_route_costs : {true, false}) {
    model.SetVehicleUsedWhenEmpty(consider_empty_route_costs, 0);
    // When nonempty, check that the candidate always has soft span quadratic
    // cost.
    MakeDelta(kEmptySolution, kNonEmptySolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(40, filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(40, filter->GetSynchronizedObjectiveValue());
    // When empty, check that the candidate has some soft span quadratic cost
    // iff consider_empty_route_costs is true.
    MakeDelta(kNonEmptySolution, kEmptySolution, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(consider_empty_route_costs ? 40 : 0,
              filter->GetAcceptedObjectiveValue());
    filter->Synchronize(&delta, nullptr);
    EXPECT_EQ(consider_empty_route_costs ? 40 : 0,
              filter->GetSynchronizedObjectiveValue());
  }
}

INSTANTIATE_TEST_SUITE_P(PathCumulFilterTestWithUnaryCallback,
                         PathCumulFilterTest, ::testing::Bool());

TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       CumulVarSoftUpperBoundFilter) {
  const char* kDimension = "test_dimension";
  const int64_t kVehicles = 3;
  const int64_t kNodes = 4;
  const int64_t kCumulUpperBounds[] = {0, 5, 1};
  const int64_t kCumulCosts[] = {5, 1, 2};
  const int kInitialSolution[] = {2, 1, 3, 6, 7, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {2, 1, 3, 6, 7, 8},
      // Make 1 performed on second path.
      {2, 7, 3, 6, 1, 8},
      // Make 1 performed on first path.
      {1, 2, 3, 6, 7, 8},
      // Make 1 performed on first path and move 2 to third path.
      {1, 3, 8, 6, 7, 2},
      // Make 1 performed on second path and move 2 to third path.
      {3, 7, 8, 6, 1, 2}};
  const int64_t kExpectedDeltaCosts[] = {2 + 20, 7 + 20, 9 + 25, 7 + 22,
                                         5 + 17};
  const bool kUseGlobalLPFiltering[] = {true, false};

  const auto [use_unary_callback, solver_type] = GetParam();
  for (bool filter_objective_cost : {true, false}) {
    for (int lp = 0; lp < std::size(kUseGlobalLPFiltering); ++lp) {
      for (int k = 0; k < 25; ++k) {
        RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
        parameters.set_continuous_scheduling_solver(solver_type);

        IndexManager manager(kNodes, kVehicles, NodeIndex(0));
        Model model(manager);
        const int callback = use_unary_callback
                                 ? model.RegisterUnaryTransitCallback(
                                       [](int64_t) { return 1LL; })
                                 : model.RegisterTransitCallback(
                                       [](int64_t, int64_t) { return 1LL; });
        model.AddDimension(callback, kint64max, kint64max,
                           /*fix_start_cumul_to_zero=*/false, kDimension);
        Dimension* const dimension = model.GetMutableDimension(kDimension);
        for (NodeIndex node(1); node < NodeIndex(kNodes); ++node) {
          dimension->SetCumulVarSoftUpperBound(
              manager.NodeToIndex(node), kCumulUpperBounds[node.value() - 1],
              kCumulCosts[node.value() - 1]);
        }
        for (int vehicle = 0; vehicle < kVehicles; ++vehicle) {
          dimension->SetCumulVarSoftUpperBound(model.Start(vehicle),
                                               kCumulUpperBounds[vehicle] - 1,
                                               kCumulCosts[vehicle]);
          dimension->SetCumulVarSoftUpperBound(model.End(vehicle),
                                               kCumulUpperBounds[vehicle],
                                               kCumulCosts[vehicle]);
        }
        model.CloseModelWithParameters(parameters);

        // The model should not have a GlobalDimensionCumulOptimizer of its
        // own for this dimension.
        EXPECT_FALSE(model.HasGlobalCumulOptimizer(*dimension));
        GlobalDimensionCumulOptimizer optimizer(dimension, solver_type,
                                                nullptr);
        LocalSearchFilter* const filter =
            kUseGlobalLPFiltering[lp]
                ? MakeGlobalLPCumulFilter(&optimizer, &optimizer,
                                          filter_objective_cost)
                : MakePathCumulFilter(*dimension, true, filter_objective_cost,
                                      true);
        Solver* const solver = model.solver();
        Assignment initial_solution(solver);
        MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
        if (model.CostVar()->Min() > k) {
          continue;
        }
        filter->Synchronize(&initial_solution, nullptr);
        const int64_t expected_synchronized_cost =
            filter_objective_cost ? kExpectedDeltaCosts[0] : 0;
        EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                  expected_synchronized_cost);  // No change on first delta.
        Assignment delta(solver);
        for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
          SCOPED_TRACE(absl::StrCat("Cost ub=", k, " delta=", l));
          MakeDelta(kInitialSolution, kDeltaSolutions[l], model.Nexts(),
                    &delta);
          const bool expect_accepted =
              filter_objective_cost ? (kExpectedDeltaCosts[l] <= k) : true;
          EXPECT_EQ(expect_accepted,
                    filter->Accept(&delta, nullptr, kint64min, k));
          if (expect_accepted) {
            const int64_t expected_accepted_cost =
                filter_objective_cost ? kExpectedDeltaCosts[l] : 0;
            EXPECT_EQ(filter->GetAcceptedObjectiveValue(),
                      expected_accepted_cost);
          }
        }
      }
    }
  }
}

TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       CumulVarSoftLowerBoundFilter) {
  const char* kDimension = "test_dimension";
  const int64_t kVehicles = 3;
  const int64_t kNodes = 4;
  const int64_t kCumulSoftLowerBounds[] = {20, 5, 1};
  const int64_t kCumulUpperBound[] = {20, 20, 4};
  const int64_t kCumulCosts[] = {5, 1, 2};
  const int kInitialSolution[] = {2, 1, 3, 6, 7, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {2, 1, 3, 6, 7, 8},
      // Make 1 performed on second path.
      {2, 7, 3, 6, 1, 8},
      // Make 1 performed on first path.
      {1, 2, 3, 6, 7, 8},
      // Make 1 performed on first path and move 2 to third path.
      {1, 3, 8, 6, 7, 2},
      // Make 1 performed on second path and move 2 to third path.
      {3, 7, 8, 6, 1, 2}};
  const int64_t kExpectedDeltaCosts[] = {
      17 * 5 + 2, 17 * 5 + 2, 18 * 5 + 18 * 5 + 2, 17 * 5 + 17 * 5, 16 * 5};
  const bool kUseGlobalLPFiltering[] = {true, false};

  const auto [use_unary_callback, solver_type] = GetParam();
  for (bool filter_objective_cost : {true, false}) {
    for (int lp = 0; lp < std::size(kUseGlobalLPFiltering); lp++) {
      for (int k = 0; k < 200; ++k) {
        RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
        parameters.set_continuous_scheduling_solver(solver_type);

        IndexManager manager(kNodes, kVehicles, NodeIndex(0));
        Model model(manager);
        const int callback = use_unary_callback
                                 ? model.RegisterUnaryTransitCallback(
                                       [](int64_t) { return 1LL; })
                                 : model.RegisterTransitCallback(
                                       [](int64_t, int64_t) { return 1LL; });
        model.AddDimension(callback, kint64max, kint64max,
                           /*fix_start_cumul_to_zero=*/false, kDimension);
        Dimension* const dimension = model.GetMutableDimension(kDimension);
        for (NodeIndex node(1); node < NodeIndex(kNodes); ++node) {
          dimension->CumulVar(node.value())
              ->SetMax(kCumulUpperBound[node.value() - 1]);
          dimension->SetCumulVarSoftLowerBound(
              manager.NodeToIndex(node),
              kCumulSoftLowerBounds[node.value() - 1],
              kCumulCosts[node.value() - 1]);
        }
        for (int vehicle = 0; vehicle < kVehicles; ++vehicle) {
          dimension->SetCumulVarSoftLowerBound(
              model.Start(vehicle), kCumulSoftLowerBounds[vehicle] - 1,
              kCumulCosts[vehicle]);
          dimension->SetCumulVarSoftLowerBound(model.End(vehicle),
                                               kCumulSoftLowerBounds[vehicle],
                                               kCumulCosts[vehicle]);
        }
        model.CloseModelWithParameters(parameters);

        // The model should not have a GlobalDimensionCumulOptimizer of its
        // own for this dimension.
        EXPECT_FALSE(model.HasGlobalCumulOptimizer(*dimension));
        GlobalDimensionCumulOptimizer optimizer(dimension, solver_type,
                                                nullptr);
        LocalSearchFilter* const filter =
            kUseGlobalLPFiltering[lp]
                ? MakeGlobalLPCumulFilter(&optimizer, &optimizer,
                                          filter_objective_cost)
                : MakePathCumulFilter(*dimension, true, filter_objective_cost,
                                      true);
        Solver* const solver = model.solver();
        Assignment initial_solution(solver);
        MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
        if (model.CostVar()->Min() > k) {
          continue;
        }
        filter->Synchronize(&initial_solution, nullptr);
        const int64_t expected_synchronized_cost =
            filter_objective_cost ? kExpectedDeltaCosts[0] : 0;
        EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                  expected_synchronized_cost);  // No change on first delta.
        Assignment delta(solver);
        for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
          SCOPED_TRACE(absl::StrCat("Cost ub=", k, " delta=", l));
          MakeDelta(kInitialSolution, kDeltaSolutions[l], model.Nexts(),
                    &delta);
          const bool expect_accepted =
              filter_objective_cost ? kExpectedDeltaCosts[l] <= k : true;
          EXPECT_EQ(expect_accepted,
                    filter->Accept(&delta, nullptr, kint64min, k));
          if (expect_accepted) {
            const int64_t expected_accepted_cost =
                filter_objective_cost ? kExpectedDeltaCosts[l] : 0;
            EXPECT_EQ(filter->GetAcceptedObjectiveValue(),
                      expected_accepted_cost);
          }
        }
      }
    }
  }
}

using RoutingFilterTestWithUnaryCallback = ::testing::TestWithParam<bool>;

INSTANTIATE_TEST_SUITE_P(RoutingFilterTestWithUnaryCallback,
                         RoutingFilterTestWithUnaryCallback, ::testing::Bool());

TEST_P(RoutingFilterTestWithUnaryCallback, CumulVarPiecewiseLinearCostFilter) {
  const char* kDimension = "test_dimension";
  const int64_t kVehicles = 3;
  const int64_t kNodes = 4;
  // Piecewise linear (continuous) costs:
  // - [kint64min, kCumulUpperBounds1): 0,
  // - [kCumulUpperBounds1, kCumulUpperBounds2): slope = kCumulCosts1,
  // - [kCumulUpperBounds2, kint64max): slope = kCumulCosts2.
  const int64_t kCumulUpperBounds1[] = {0, 5, 1};
  const int64_t kCumulUpperBounds2[] = {1, 9, 3};
  const int64_t kCumulCosts1[] = {5, 1, 2};
  const int64_t kCumulCosts2[] = {6, 0, 7};
  const int kInitialSolution[] = {2, 1, 3, 6, 7, 8};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {2, 1, 3, 6, 7, 8},
      // Make 1 performed on second path.
      {2, 7, 3, 6, 1, 8},
      // Make 1 performed on first path.
      {1, 2, 3, 6, 7, 8},
      // Make 1 performed on first path and move 2 to third path.
      {1, 3, 8, 6, 7, 2},
      // Make 1 performed on second path and move 2 to third path.
      {3, 7, 8, 6, 1, 2}};
  const int64_t kExpectedDeltaCosts[] = {2 + 22, 7 + 22, 9 + 27, 7 + 24,
                                         5 + 18};
  for (int k = 0; k < 25; ++k) {
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const int callback =
        GetParam()
            ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
            : model.RegisterTransitCallback(
                  [](int64_t, int64_t) { return 1LL; });
    model.AddDimension(callback, 0, kint64max,
                       /*fix_start_cumul_to_zero=*/false, kDimension);
    Dimension* const dimension = model.GetMutableDimension(kDimension);
    for (NodeIndex node(1); node < NodeIndex(kNodes); ++node) {
      std::unique_ptr<PiecewiseLinearFunction> f(
          PiecewiseLinearFunction::CreateFullDomainFunction(
              0,
              {kCumulUpperBounds1[node.value() - 1],
               kCumulUpperBounds2[node.value() - 1]},
              {0, kCumulCosts1[node.value() - 1],
               kCumulCosts2[node.value() - 1]}));
      dimension->SetCumulVarPiecewiseLinearCost(manager.NodeToIndex(node), *f);
    }
    for (int vehicle = 0; vehicle < kVehicles; ++vehicle) {
      std::unique_ptr<PiecewiseLinearFunction> f(
          PiecewiseLinearFunction::CreateFullDomainFunction(
              0,
              {kCumulUpperBounds1[vehicle] - 1,
               kCumulUpperBounds2[vehicle] - 1},
              {0, kCumulCosts1[vehicle], kCumulCosts2[vehicle]}));
      dimension->SetCumulVarPiecewiseLinearCost(model.Start(vehicle), *f);
      f.reset(PiecewiseLinearFunction::CreateFullDomainFunction(
          0, {kCumulUpperBounds1[vehicle], kCumulUpperBounds2[vehicle]},
          {0, kCumulCosts1[vehicle], kCumulCosts2[vehicle]}));
      dimension->SetCumulVarPiecewiseLinearCost(model.End(vehicle), *f);
    }
    model.CloseModel();
    LocalSearchFilter* const filter =
        MakePathCumulFilter(*dimension, true, true, true);
    Solver* const solver = model.solver();
    Assignment initial_solution(solver);
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    if (model.CostVar()->Min() > k) {
      continue;
    }
    model.CostVar()->SetMax(k);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
              kExpectedDeltaCosts[0]);  // No change on first delta.
    Assignment delta(solver);
    for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
      SCOPED_TRACE(absl::StrCat("Cost ub=", k, " delta=", l));
      MakeDelta(kInitialSolution, kDeltaSolutions[l], model.Nexts(), &delta);
      const bool expect_accepted = kExpectedDeltaCosts[l] <= k;
      EXPECT_EQ(expect_accepted, filter->Accept(&delta, nullptr, kint64min, k));
      if (expect_accepted) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedDeltaCosts[l]);
      }
    }
  }
}

// Test the filter on visit type incompatibilities between nodes of different
// visit type policies.
// There are 5 types, T0 to T4.
// Incompatibilities: (T0, T2), (T0, T3), (T1, T3).
// The model has a single node of each type (nodes 0 to 4), and 1 depot
// (type-less) for each of the 3 vehicles.
TEST(RoutingFilterTest, TypeIncompatibilityFilterTypePolicies) {
  const int kNumNodes = 6;
  const int kNumVehicles = 3;
  IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(5));

  const std::vector<VisitTypePolicy> kTypePolicies = {
      Model::TYPE_ADDED_TO_VEHICLE, Model::ADDED_TYPE_REMOVED_FROM_VEHICLE,
      Model::TYPE_ON_VEHICLE_UP_TO_VISIT,
      Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED};

  const std::vector<bool> kHardIncompatibilities = {true, false};

  const int kEmptySolution[] = {0, 1, 2, 3, 4, 8, 9, 10};
  // Three routes (baseline for the following deltas) :
  // depot-->0-->1-->depot, depot-->2-->depot, depot-->3-->4-->depot.
  const int kInitialSolution[] = {1, 8, 9, 4, 10, 0, 2, 3};

  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // Move node 2 to the third route
      {1, 8, 3, 4, 10, 0, 9, 2},
      // Add node 2 to the first route (incompatible with node 0)
      {1, 2, 8, 4, 10, 0, 9, 3},
      // Move node 2 to the first route and remove 0 from it
      {0, 2, 8, 4, 10, 1, 9, 3},
      // Move node 1 to the second route
      {8, 2, 9, 4, 10, 0, 1, 3},
      // Merge first and last routes on third vehicle (0 and 1 incompatible with
      // 3)
      {1, 3, 9, 4, 10, 8, 2, 0},
      // Put 0 and 1 on third vehicle, but remove 3
      {1, 4, 9, 3, 10, 8, 2, 0}};

  const std::vector<std::vector<std::vector<bool>>> kExpectedAccept = {
      {{true, false, true, true, false, true},
       {true, false, true, true, false, true}},
      {{true, true, true, true, true, true},
       {true, true, true, true, true, true}},
      {{true, false, true, true, false, true},
       {true, false, true, true, false, true}},
      {{true, false, true, true, false, true},
       {true, true, true, true, true, true}}};

  for (int p = 0; p < kTypePolicies.size(); p++) {
    for (int i = 0; i < kHardIncompatibilities.size(); i++) {
      Model model(manager);

      std::vector<int> ends;
      for (int v = 0; v < kNumVehicles; v++) {
        ends.push_back(model.End(v));
      }

      for (int index = 0; index < 5; index++) {
        model.SetVisitType(manager.NodeToIndex(NodeIndex(index)), index,
                           kTypePolicies[p]);
      }
      if (kHardIncompatibilities[i]) {
        model.AddHardTypeIncompatibility(0, 2);
        model.AddHardTypeIncompatibility(0, 3);
        model.AddHardTypeIncompatibility(1, 3);
      } else {
        model.AddTemporalTypeIncompatibility(0, 2);
        model.AddTemporalTypeIncompatibility(0, 3);
        model.AddTemporalTypeIncompatibility(1, 3);
      }
      model.CloseModel();

      IntVarLocalSearchFilter* filter = MakeTypeRegulationsFilter(model);

      Assignment empty_solution(model.solver());
      MakeAssignment(kEmptySolution, model.Nexts(), &empty_solution);
      filter->Synchronize(&empty_solution, nullptr);
      Assignment delta(model.solver());
      // Empty delta
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));

      MakeDelta(kEmptySolution, kInitialSolution, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
      filter->Synchronize(&empty_solution, &delta);
      for (int d = 0; d < std::size(kDeltaSolutions); d++) {
        SCOPED_TRACE(absl::StrCat("Type Policy #", p, ", Incompatibility #", i,
                                  ", delta #", d));
        MakeDelta(kInitialSolution, kDeltaSolutions[d], model.Nexts(), &delta);
        EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                  kExpectedAccept[p][i][d]);
      }
    }
  }
}

class TypeRequirementFilterTest : public ::testing::Test {
 public:
  ~TypeRequirementFilterTest() override = default;

 protected:
  enum RequirementMode { WHEN_TYPE_ADDED = 0, WHEN_TYPE_REMOVED, SAME_VEHICLE };

  void SetupModelWithRequirements(RequirementMode requirement_mode,
                                  VisitTypePolicy required_policy,
                                  VisitTypePolicy dependent_policy) {
    // There are 5 types, T1 to T5.
    // Requirements:
    //   - T1 requires {T3 OR T4},
    //   - T1 requires T5,
    //   - T2 requires {T4 OR T5}.
    // The model has a single node of each type (nodes 1 to 5), and 1 depot
    // (type-less) for the 3 vehicles.
    const int kNumNodes = 6;
    const int kNumVehicles = 2;
    const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);

    const int kDependentNodes[] = {1, 2};
    const int kRequiredNodes[] = {3, 4, 5};
    for (int dependent_node : kDependentNodes) {
      model_->SetVisitType(dependent_node, dependent_node, dependent_policy);
    }
    for (int required_node : kRequiredNodes) {
      model_->SetVisitType(required_node, required_node, required_policy);
    }

    switch (requirement_mode) {
      case WHEN_TYPE_ADDED:
        model_->AddRequiredTypeAlternativesWhenAddingType(1, {3, 4});
        model_->AddRequiredTypeAlternativesWhenAddingType(1, {5});
        model_->AddRequiredTypeAlternativesWhenAddingType(2, {4, 5});
        break;
      case WHEN_TYPE_REMOVED:
        model_->AddRequiredTypeAlternativesWhenRemovingType(1, {3, 4});
        model_->AddRequiredTypeAlternativesWhenRemovingType(1, {5});
        model_->AddRequiredTypeAlternativesWhenRemovingType(2, {4, 5});
        break;
      case SAME_VEHICLE:
        model_->AddSameVehicleRequiredTypeAlternatives(1, {3, 4});
        model_->AddSameVehicleRequiredTypeAlternatives(1, {5});
        model_->AddSameVehicleRequiredTypeAlternatives(2, {4, 5});
    }
    model_->CloseModel();
  }

  void FilterDeltasAndExpect(bool accept_initial_solution,
                             const std::vector<bool>& expected_accept) {
    ASSERT_EQ(expected_accept.size(), std::size(kDeltaSolutions));

    IntVarLocalSearchFilter* filter = MakeTypeRegulationsFilter(*model_);

    Assignment empty_solution(model_->solver());
    MakeAssignment(kEmptySolution, model_->Nexts(), &empty_solution);
    filter->Synchronize(&empty_solution, nullptr);
    Assignment delta(model_->solver());
    // Empty delta
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));

    const int (*synchronized_solution)[std::size(kEmptySolution)] =
        &kEmptySolution;

    MakeDelta(kEmptySolution, kInitialSolution, model_->Nexts(), &delta);
    EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
              accept_initial_solution);
    if (accept_initial_solution) {
      filter->Synchronize(&empty_solution, &delta);
      synchronized_solution = &kInitialSolution;
    }

    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      SCOPED_TRACE(absl::StrCat("Delta = ", d));
      MakeDelta(*synchronized_solution, kDeltaSolutions[d], model_->Nexts(),
                &delta);
      EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                expected_accept[d]);
    }
  }

  void FilterDeltasAndExpectAccept() {
    FilterDeltasAndExpect(true,
                          std::vector<bool>(std::size(kDeltaSolutions), true));
  }

  void FilterDeltasAndExpectNotAccept() {
    FilterDeltasAndExpect(false,
                          std::vector<bool>(std::size(kDeltaSolutions), false));
  }

  static constexpr VisitTypePolicy kAllVisitTypePolicies[] = {
      Model::TYPE_ADDED_TO_VEHICLE, Model::ADDED_TYPE_REMOVED_FROM_VEHICLE,
      Model::TYPE_ON_VEHICLE_UP_TO_VISIT,
      Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED};

 private:
  static constexpr int kEmptySolution[] = {7, 1, 2, 3, 4, 5, 8};
  static constexpr int kInitialSolution[] = {4, 7, 5, 8, 2, 1, 3};

  static constexpr int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // Reverse the first route: depot-->1-->5-->2-->4-->depot
      {1, 5, 4, 8, 7, 2, 3},
      // Make node 1 unperformed.
      {4, 1, 5, 8, 2, 7, 3},
      // Move node 4 to second route (T1 needs {T3 or T4}).
      {2, 7, 5, 8, 3, 1, 4},
      // Exchange nodes 3 and 4 (T2 needs {T4 or T5}).
      {3, 7, 5, 2, 8, 1, 4},
      // Move node 4 to the second route and node 3 to the first:
      // depot-->5-->3-->2-->1-->depot, depot-->4-->depot.
      {5, 7, 1, 2, 8, 3, 4},
      // Move node 4 to the second route and node 3 to the first:
      // depot-->5-->2-->3-->1-->depot, depot-->4-->depot.
      {5, 7, 3, 1, 8, 2, 4},
      // Move node 4 to the second route and node 3 to the first:
      // depot-->2-->5-->3-->1-->depot, depot-->4-->depot.
      {2, 7, 5, 1, 8, 3, 4},
      // Move node 2 to second route:
      // depot-->4-->5-->1-->depot, depot-->3-->2-->depot
      {4, 7, 8, 2, 5, 1, 3},
      // Move nodes 2 and 4 to the second route, and node 3 to the first:
      // depot-->3-->5-->1-->depot, depot-->4-->2-->depot.
      {3, 7, 8, 5, 2, 1, 4},
      // Put all nodes on 1 route: depot-->5-->4-->3-->2-->1-->depot
      {5, 7, 1, 2, 3, 4, 8},
      // Put all nodes on 1 route: depot-->1-->2-->3-->4-->5-->depot
      {1, 2, 3, 4, 5, 7, 8}};
  std::unique_ptr<Model> model_;
};

// Test the filter on visit type requirements for adding the dependent type,
// when the dependent types are never actually added to the route.
// Since in this case the requirements are only effective when the dependent
// type is being added, the solutions are always valid if the dependent type's
// policy isn't to add the type to the route.
TEST_F(TypeRequirementFilterTest, InactiveRequirementWhenTypeAdded) {
  for (VisitTypePolicy required_policy : kAllVisitTypePolicies) {
    SCOPED_TRACE(absl::StrCat("Required node policy: ", required_policy));
    for (VisitTypePolicy dependent_policy :
         {Model::ADDED_TYPE_REMOVED_FROM_VEHICLE,
          Model::TYPE_ON_VEHICLE_UP_TO_VISIT}) {
      SCOPED_TRACE(absl::StrCat("Dependent node policy: ", dependent_policy));
      SetupModelWithRequirements(WHEN_TYPE_ADDED, required_policy,
                                 dependent_policy);

      FilterDeltasAndExpectAccept();
    }
  }
}

// Test the filter on visit type requirements when adding the dependent type,
// when the dependent type is actually added to the route (requirement active).
TEST_F(TypeRequirementFilterTest, ActiveRequirementWhenTypeAdded) {
  const std::vector<bool> kExpectedAcceptWhenRequiredAdded = {
      false, true, false, false, true, true, false, false, true, true, false};

  const std::vector<bool> kExpectedAcceptWhenRequiredUpToVisit = {
      true, true, false, false, false, false, false, false, false, false, true};

  for (VisitTypePolicy required_policy : kAllVisitTypePolicies) {
    SCOPED_TRACE(absl::StrCat("Required node policy: ", required_policy));
    for (VisitTypePolicy dependent_policy :
         {Model::TYPE_ADDED_TO_VEHICLE,
          Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED}) {
      SCOPED_TRACE(absl::StrCat("Dependent node policy: ", dependent_policy));
      SetupModelWithRequirements(WHEN_TYPE_ADDED, required_policy,
                                 dependent_policy);

      if (required_policy == Model::ADDED_TYPE_REMOVED_FROM_VEHICLE ||
          required_policy == Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED) {
        // The required type is not on the route after the required visit, so
        // the requirement is never satisfied.
        FilterDeltasAndExpectNotAccept();
      } else if (required_policy == Model::TYPE_ADDED_TO_VEHICLE) {
        FilterDeltasAndExpect(true, kExpectedAcceptWhenRequiredAdded);
      } else {
        FilterDeltasAndExpect(false, kExpectedAcceptWhenRequiredUpToVisit);
      }
    }
  }
}

// Test the filter on visit type requirements for removing the dependent type,
// when the dependent types are never actually removed from the route.
// Since in this case the requirements are only effective when the dependent
// type is being removed, the solutions are always valid if the dependent type
// nodes don't actually remove the type from the route.
// This happens when the dependent nodes have as policy to add the type to the
// route, and also when they all have the policy TYPE_REMOVED_FROM_ROUTE, since
// the dependent type is not added so the removal is not actually happening.
TEST_F(TypeRequirementFilterTest, InactiveRequirementWhenTypeRemoved) {
  for (VisitTypePolicy required_policy : kAllVisitTypePolicies) {
    SCOPED_TRACE(absl::StrCat("Required node policy: ", required_policy));
    for (VisitTypePolicy dependent_policy :
         {Model::TYPE_ADDED_TO_VEHICLE,
          Model::ADDED_TYPE_REMOVED_FROM_VEHICLE}) {
      SCOPED_TRACE(absl::StrCat("Dependent node policy: ", dependent_policy));
      SetupModelWithRequirements(WHEN_TYPE_REMOVED, required_policy,
                                 dependent_policy);
      FilterDeltasAndExpectAccept();
    }
  }
}

// Test the filter on visit type requirements when removing the dependent type,
// when the dependent type is actually removed from the route (requirement
// active).
TEST_F(TypeRequirementFilterTest, ActiveRequirementWhenTypeRemoved) {
  const std::vector<bool> kExpectedAcceptWhenRequiredAdded = {
      false, true, false, false, true, true, false, false, true, true, false};

  const std::vector<bool> kExpectedAcceptWhenRequiredUpToVisit = {
      true, true, false, false, false, false, false, false, false, false, true};

  for (VisitTypePolicy required_policy : kAllVisitTypePolicies) {
    SCOPED_TRACE(absl::StrCat("Required node policy: ", required_policy));
    for (VisitTypePolicy dependent_policy :
         {Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED,
          Model::TYPE_ON_VEHICLE_UP_TO_VISIT}) {
      SCOPED_TRACE(absl::StrCat("Dependent node policy: ", dependent_policy));
      SetupModelWithRequirements(WHEN_TYPE_REMOVED, required_policy,
                                 dependent_policy);

      if (required_policy == Model::ADDED_TYPE_REMOVED_FROM_VEHICLE ||
          required_policy == Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED) {
        // The required type is not on the route after the required visit, so
        // the requirement is never satisfied.
        FilterDeltasAndExpectNotAccept();
      } else if (required_policy == Model::TYPE_ADDED_TO_VEHICLE) {
        FilterDeltasAndExpect(true, kExpectedAcceptWhenRequiredAdded);
      } else {
        FilterDeltasAndExpect(false, kExpectedAcceptWhenRequiredUpToVisit);
      }
    }
  }
}

// Test the filter on same-vehicle visit type requirements, for different
// visit type policies for the required and dependent types.
TEST_F(TypeRequirementFilterTest, SameVehicleTypeRequirement) {
  const std::vector<bool> kExpectedAcceptWhenRequirementEffective = {
      true, true, false, true, true, true, true, false, true, true, true};

  for (VisitTypePolicy required_policy : kAllVisitTypePolicies) {
    SCOPED_TRACE(absl::StrCat("Required node policy: ", required_policy));
    for (VisitTypePolicy dependent_policy : kAllVisitTypePolicies) {
      SCOPED_TRACE(absl::StrCat("Dependent node policy: ", dependent_policy));
      SetupModelWithRequirements(SAME_VEHICLE, required_policy,
                                 dependent_policy);

      if (dependent_policy == Model::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
        // NOTE: If the dependent nodes are only being removed from the route,
        // the requirement isn't effective as the dependent node is never
        // actually on the route.
        FilterDeltasAndExpectAccept();
      } else if (required_policy == Model::ADDED_TYPE_REMOVED_FROM_VEHICLE) {
        // If the dependent type is on the vehicle and the required type never
        // is, the requirement is never fulfilled.
        FilterDeltasAndExpectNotAccept();
      } else {
        FilterDeltasAndExpect(true, kExpectedAcceptWhenRequirementEffective);
      }
    }
  }
}

// Test the filter on type incompatibilities with visit type policies adding
// and removing each of the types.
// 4 types, T0 to T3.
// Incompatibilities: (T0, T2), (T0, T3), (T1, T3).
// The model has 1 vehicle, and for each type T0 to T3, there's 1 node adding
// the type and 1 removing it.
TEST(RoutingFilterTest, TemporalTypeIncompatibilityFilterTypesAddedAndRemoved) {
  const int kNumNodes = 9;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(8));
  Model model(manager);

  const int kNumTypePairs = kNumNodes / 2;
  for (int i = 0; i < kNumTypePairs; i++) {
    const int add_type_visit = i;
    const int remove_type_visit = i + kNumTypePairs;
    model.SetVisitType(add_type_visit, i, Model::TYPE_ADDED_TO_VEHICLE);
    model.SetVisitType(remove_type_visit, i,
                       Model::ADDED_TYPE_REMOVED_FROM_VEHICLE);
  }

  const int kIncompatibleTypes[][2] = {{0, 2}, {0, 3}, {1, 3}};
  for (int t = 0; t < std::size(kIncompatibleTypes); t++) {
    model.AddTemporalTypeIncompatibility(kIncompatibleTypes[t][0],
                                         kIncompatibleTypes[t][1]);
  }
  model.CloseModel();

  // Initial solution: DEPOT-->A0-->R0-->A1-->R1-->A2-->R2-->A3-->R3-->DEPOT
  // NOTE: A{t} and R{t} respectively add and remove type t from the route.
  // Valid because all types are removed right after they're added, so there's a
  // single type at a time on the route.
  const int kInitialSolution[] = {4, 5, 6, 7, 1, 2, 3, 9, 0};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change.
      {4, 5, 6, 7, 1, 2, 3, 9, 0},
      // Visit R0 before A0. Type 0 stays on the route as it's not removed after
      // being added:
      // DEPOT-->R0-->A0-->A1-->R1-->A2-->R2-->A3-->R3-->DEPOT
      {1, 5, 6, 7, 0, 2, 3, 9, 4},
      // Visit A1 before A0. OK Because T0 and T1 are compatible.
      {1, 4, 6, 7, 5, 2, 3, 9, 0},
      // Visit A1/R1 before R0. OK as well.
      {1, 5, 6, 7, 2, 4, 3, 9, 0},
      // DEPOT-->A0-->A1-->R1-->A2-->R2-->R0-->A3-->R3-->DEPOT.
      // Not OK because T0 and T2 are incompatible.
      {1, 5, 6, 7, 3, 2, 4, 9, 0},
      // DEPOT-->A0-->A1-->R1-->A2-->R0-->R2-->A3-->R3-->DEPOT. Still not OK.
      {1, 5, 4, 7, 6, 2, 3, 9, 0},
      // R1 after R2. OK because T1 and T2 are compatible.
      {4, 2, 6, 7, 1, 3, 5, 9, 0},
      // R1 after A3 (T1 and T3 incompatible).
      {4, 2, 6, 5, 1, 7, 3, 9, 0}};
  const bool kExpectedAccept[] = {true,  false, true, true,
                                  false, false, true, false};

  LocalSearchFilter* filter = MakeTypeRegulationsFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  Assignment delta(model.solver());
  for (int d = 0; d < std::size(kDeltaSolutions); d++) {
    MakeDelta(kInitialSolution, kDeltaSolutions[d], model.Nexts(), &delta);
    EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
              kExpectedAccept[d]);
  }
}

// Test the filter on type requirements with nodes adding/removing each of the
// types.
// 4 types, T1 to T4.
// Requirements:
//   - T4 requires {T2 OR T3},
//   - T2 requires T1.
// The model has 4 add/remove type pairs of types T1 to T4, and 2 vehicles.
TEST(RoutingFilterTest, TypeRequirementFilterWithAddRemoveTypePolicies) {
  const int kNumNodes = 9;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  // Initial solution: All unperformed.
  const int kInitialSolution[] = {10, 1, 2, 3, 4, 5, 6, 7, 8, 11};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change.
      {10, 1, 2, 3, 4, 5, 6, 7, 8, 11},
      // v0: DEPOT-->A2-->A1-->R2-->R1-->A4-->A3-->R4-->R3-->DEPOT
      {2, 6, 1, 8, 3, 4, 5, 10, 7, 11},
      // v0: DEPOT-->A1-->A2-->R1-->R2-->DEPOT
      // v1: DEPOT-->A3-->A4-->R4-->R3-->DEPOT
      {1, 2, 5, 4, 8, 6, 10, 11, 7, 3},
      // v0: DEPOT-->A1-->A2-->R1-->R2-->A3-->R3-->DEPOT
      // v1: DEPOT-->A4-->R4-->DEPOT
      {1, 2, 5, 7, 8, 6, 3, 10, 11, 4},
      // v0: DEPOT-->A2-->A1-->R2-->R1-->DEPOT
      // v1: DEPOT-->A4-->A3-->R4-->R3-->DEPOT
      {2, 6, 1, 8, 3, 10, 5, 11, 7, 4},
      // v0: DEPOT-->A1-->A2-->R1-->R2-->A3-->A4-->R4-->R3-->DEPOT
      {1, 2, 5, 4, 8, 6, 3, 10, 7, 11},
      // v0: DEPOT-->A1-->A2-->R1-->A4-->R4-->R2-->DEPOT
      // v1: DEPOT-->A3-->R3-->DEPOT
      {1, 2, 5, 7, 8, 4, 10, 11, 6, 3},
      // v0: DEPOT-->A3-->A1-->A2-->R3-->R2-->A4-->R1-->R4-->DEPOT
      {3, 2, 7, 1, 5, 8, 4, 6, 10, 11},
      // v0: DEPOT-->R1-->A3-->A2-->A1-->R2-->A4-->R4-->R3-->DEPOT
      {5, 6, 1, 2, 8, 3, 4, 10, 7, 11},
      // v0: DEPOT-->A3-->A1-->A2-->R3-->A4-->R2-->R1-->R4-->DEPOT
      {3, 2, 7, 1, 6, 8, 5, 4, 10, 11}};
  const std::string kRequirementMode[] = {"same_vehicle", "when_added",
                                          "when_removed"};
  const bool kExpectedAccept[][std::size(kDeltaSolutions)] = {
      {true, true, true, false, true, true, true, true, true, true},
      {true, false, true, false, false, true, true, false, false, true},
      {true, true, false, false, true, false, false, false, true, false}};

  for (int m = 0; m < std::size(kRequirementMode); m++) {
    Model model(manager);

    const int kNumPickupDeliveryPairs = kNumNodes / 2;
    for (int i = 1; i <= kNumPickupDeliveryPairs; i++) {
      const int pickup = i;
      const int delivery = i + kNumPickupDeliveryPairs;
      model.AddPickupAndDelivery(pickup, delivery);
      model.SetVisitType(pickup, i, Model::TYPE_ADDED_TO_VEHICLE);
      model.SetVisitType(delivery, i, Model::ADDED_TYPE_REMOVED_FROM_VEHICLE);
    }

    if (kRequirementMode[m] == "same_vehicle") {
      model.AddSameVehicleRequiredTypeAlternatives(4, {2, 3});
      model.AddSameVehicleRequiredTypeAlternatives(2, {1});
    } else if (kRequirementMode[m] == "when_added") {
      model.AddRequiredTypeAlternativesWhenAddingType(4, {2, 3});
      model.AddRequiredTypeAlternativesWhenAddingType(2, {1});
    } else {
      model.AddRequiredTypeAlternativesWhenRemovingType(4, {2, 3});
      model.AddRequiredTypeAlternativesWhenRemovingType(2, {1});
    }

    model.CloseModel();

    LocalSearchFilter* filter = MakeTypeRegulationsFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    Assignment delta(model.solver());
    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      MakeDelta(kInitialSolution, kDeltaSolutions[d], model.Nexts(), &delta);
      EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                kExpectedAccept[m][d]);
    }
  }
}

// Small test to verify that type incompatibilities are filtered correctly in
// the presence of all visit type policies, and for both "hard" and "temporal"
// incompatibility types.
// In the example, there are 6 nodes other than the depot, with:
// - Nodes 1 and 4 are of the same type T1, respectively of policy
//   TYPE_ADDED_TO_VEHICLE (A1) and ADDED_TYPE_REMOVED_FROM_VEHICLE (R1).
// - Node 2 is of type T2 and policy TYPE_ON_VEHICLE_UP_TO_VISIT (UV2).
// - Node 3 is of type T3 and policy TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED (AR3)
// - All types T1, T2 and T3 are pair-wise incompatible.
TEST(RoutingFilterTest, TypeIncompatibilityFilterMixedVisitTypePolicies) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  // Initial solution: all unperformed.
  const int kInitialSolution[] = {6, 1, 2, 3, 4, 7};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change.
      {6, 1, 2, 3, 4, 7},
      // START(0) --> UV2 --> A1 --> AR3 --> R1 --> END(0)
      {2, 3, 1, 4, 6, 7},
      // START(0) --> UV2 --> A1 --> R1 --> AR3 --> END(0)
      {2, 4, 1, 6, 3, 7},
      // START(0) --> A1 --> R1 --> UV2 --> AR3 --> END(0)
      {1, 4, 3, 6, 2, 7},
      // START(0) --> UV2 --> R1 --> A1 --> AR3 --> END(0)
      {2, 3, 4, 6, 1, 7},
      // START(0) --> A1 --> R1 --> END(0),  START(1) --> UV2 --> AR3 --> END(1)
      {1, 4, 3, 7, 6, 2},
      // START(0) --> UV2 --> R1 --> END(0),  START(1) --> AR3 --> END(1)
      {2, 1, 4, 7, 6, 3},
      // START(0) --> A1 --> R1 --> END(0),  START(1) --> AR3 --> UV2 --> END(1)
      {1, 4, 7, 2, 6, 3}};

  const bool kHardIncompatibility[] = {true, false};
  const bool kExpectedAccept[][std::size(kHardIncompatibility)] = {
      {true, true},   {false, false}, {false, true}, {false, false},
      {false, false}, {false, true},  {true, true},  {false, false}};

  for (int i = 0; i < std::size(kHardIncompatibility); i++) {
    Model model(manager);

    model.SetVisitType(/*index*/ 1, /*type*/ 1, Model::TYPE_ADDED_TO_VEHICLE);
    model.SetVisitType(4, 1, Model::ADDED_TYPE_REMOVED_FROM_VEHICLE);
    model.SetVisitType(2, 2, Model::TYPE_ON_VEHICLE_UP_TO_VISIT);
    model.SetVisitType(3, 3, Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);

    if (kHardIncompatibility[i]) {
      model.AddHardTypeIncompatibility(1, 2);
      model.AddHardTypeIncompatibility(1, 3);
      model.AddHardTypeIncompatibility(2, 3);
    } else {
      model.AddTemporalTypeIncompatibility(1, 2);
      model.AddTemporalTypeIncompatibility(1, 3);
      model.AddTemporalTypeIncompatibility(2, 3);
    }
    model.CloseModel();

    LocalSearchFilter* filter = MakeTypeRegulationsFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    Assignment delta(model.solver());
    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      MakeDelta(kInitialSolution, kDeltaSolutions[d], model.Nexts(), &delta);
      EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                kExpectedAccept[d][i]);
    }
  }
}

// Similar to above, but to test type requirements in the presence of mixed
// visit type policies and requirement modes.
// - Nodes 1 and 4 are of the same type T1, respectively of policy
//   TYPE_ADDED_TO_VEHICLE (A1) and ADDED_TYPE_REMOVED_FROM_VEHICLE (R1).
// - Node 2 is of type T2 and policy TYPE_ON_VEHICLE_UP_TO_VISIT (UV2).
// - Node 3 is of type T3 and policy TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED (AR3)
// - T1 requires T2 (same-vehicle or when removing the type), and T3 requires
//   T1 (same-vehicle or when added to the route).
TEST(RoutingFilterTest, MixedTypeRequirementFilterWithMixedVisitTypePolicies) {
  const int kNumNodes = 5;
  const int kNumVehicles = 2;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  // Initial solution: all unperformed.
  const int kInitialSolution[] = {6, 1, 2, 3, 4, 7};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change.
      {6, 1, 2, 3, 4, 7},
      // START(0) --> A1 --> R1 --> AR3 --> UV2 --> END(0)
      {1, 4, 6, 2, 3, 7},
      // START(0) --> UV2 --> R1 --> A1 --> AR3 --> END(0)
      {2, 3, 4, 6, 1, 7},
      // START(0) --> A1 --> UV2 --> AR3 --> R1 --> END(0)
      {1, 2, 3, 4, 6, 7},
      // START(0) --> A1 --> AR3 --> UV2 --> R1 --> END(0)
      {1, 3, 4, 2, 6, 7},
      // START(0) --> A1 --> R1 --> END(0),  START(1) --> AR3 --> UV2 --> END(1)
      {1, 4, 7, 2, 6, 3},
      // START(0) --> UV2 --> R1 --> END(0), A1 and AR3 unperformed.
      {2, 1, 4, 3, 6, 7},
      // START(0) --> AR3 --> R1 --> UV2 --> END(0), A1  unperformed.
      {3, 1, 4, 2, 6, 7}};

  const bool kTemporalRequirements[] = {true, false};
  const bool kExpectedAccept[][std::size(kTemporalRequirements)] = {
      {true, true},  {false, true},  {true, true}, {false, true},
      {false, true}, {false, false}, {true, true}, {false, false}};

  for (int t = 0; t < std::size(kTemporalRequirements); t++) {
    Model model(manager);

    model.SetVisitType(/*index*/ 1, /*type*/ 1, Model::TYPE_ADDED_TO_VEHICLE);
    model.SetVisitType(4, 1, Model::ADDED_TYPE_REMOVED_FROM_VEHICLE);
    model.SetVisitType(2, 2, Model::TYPE_ON_VEHICLE_UP_TO_VISIT);
    model.SetVisitType(3, 3, Model::TYPE_SIMULTANEOUSLY_ADDED_AND_REMOVED);

    if (kTemporalRequirements[t]) {
      model.AddRequiredTypeAlternativesWhenRemovingType(1, {2});
      model.AddRequiredTypeAlternativesWhenAddingType(3, {1});
    } else {
      model.AddSameVehicleRequiredTypeAlternatives(1, {2});
      model.AddSameVehicleRequiredTypeAlternatives(3, {1});
    }
    model.CloseModel();

    LocalSearchFilter* filter = MakeTypeRegulationsFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    Assignment delta(model.solver());
    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      MakeDelta(kInitialSolution, kDeltaSolutions[d], model.Nexts(), &delta);
      EXPECT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                kExpectedAccept[d][t]);
    }
  }
}

TEST(RoutingFilterTest, PickupDeliveryFilterNoOrder) {
  const int kNodes = 4;
  const int kVehicles = 1;
  const int kInitialSolution[] = {4, 1, 2, 3};  // 0 -> 4.
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(1)),
                             manager.NodeToIndex(NodeIndex(2)));
  model.CloseModel();
  std::vector<Model::PickupAndDeliveryPolicy> policies(
      kVehicles, Model::PICKUP_AND_DELIVERY_NO_ORDER);
  PathState path_state(5, {0}, {4});
  LocalSearchFilter* filter = MakePickupDeliveryFilter(
      model, &path_state, model.GetPickupAndDeliveryPairs(), policies);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  PathStateChangePaths(&path_state, {{0, 4}}, {});
  filter->Synchronize(&initial_solution, nullptr);
  path_state.Commit();
  Assignment delta(model.solver());
  // Empty delta
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  struct TestCase {
    std::vector<int> path;
    std::vector<int> loops;
    bool expected_accept;
  };
  const std::vector<TestCase> test_cases = {
      // No paired node is performed: feasible.
      {{0, 3, 4}, {}, true},
      {{0, 3, 4}, {1}, true},
      {{0, 3, 4}, {2}, true},
      {{0, 3, 4}, {1, 2}, true},
      // 1 alone: feasible if 2 unassigned.
      {{0, 1, 3, 4}, {}, true},
      {{0, 1, 3, 4}, {2}, false},
      // 2 alone: feasible if 1 unassigned.
      {{0, 2, 3, 4}, {}, true},
      {{0, 2, 3, 4}, {1}, false},
      // 1 then 2: feasible.
      {{0, 1, 2, 3, 4}, {}, true},
      {{0, 1, 3, 2, 4}, {}, true},
      {{0, 3, 1, 2, 4}, {}, true},
      // 2 then 1: infeasible.
      {{0, 2, 1, 3, 4}, {}, false},
      {{0, 2, 3, 1, 4}, {}, false},
      {{0, 3, 2, 1, 4}, {}, false},
  };
  for (int t = 0; t < test_cases.size(); ++t) {
    SCOPED_TRACE(absl::StrCat("Test case ", t));
    const TestCase& test_case = test_cases[t];
    PathStateChangePaths(&path_state, {test_case.path}, test_case.loops);
    const bool accept = filter->Accept(&delta, nullptr, kint64min, kint64max);
    EXPECT_EQ(accept, test_case.expected_accept);
    path_state.Revert();
  }
}

TEST(RoutingFilterTest, PickupDeliveryFilterLifoAndFifo) {
  const int kNodes = 6;
  const int kVehicles = 1;
  const int kInitialSolution[] = {6, 1, 2, 3, 4, 5};
  for (const bool lifo : {true, false}) {
    const auto policy = lifo ? Model::PICKUP_AND_DELIVERY_LIFO
                             : Model::PICKUP_AND_DELIVERY_FIFO;
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(1)),
                               manager.NodeToIndex(NodeIndex(2)));
    model.AddPickupAndDelivery(manager.NodeToIndex(NodeIndex(4)),
                               manager.NodeToIndex(NodeIndex(5)));
    model.CloseModel();
    std::vector<Model::PickupAndDeliveryPolicy> policies(kVehicles, policy);
    PathState path_state(7, {0}, {6});
    LocalSearchFilter* filter = MakePickupDeliveryFilter(
        model, &path_state, model.GetPickupAndDeliveryPairs(), policies);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    Assignment delta(model.solver());
    // Empty delta
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    struct TestCase {
      std::vector<int> path;
      std::vector<int> loops;
      bool expected_lifo_accept;
      bool expected_fifo_accept;
    };
    const std::vector<TestCase> test_cases = {
        // No paired node is performed: feasible.
        {{0, 3, 6}, {}, true, true},
        {{0, 3, 6}, {1}, true, true},
        {{0, 3, 6}, {2}, true, true},
        {{0, 3, 6}, {1, 2}, true, true},
        // 1->2 in correct order, or one is unassigned: feasible.
        {{0, 1, 3, 2, 6}, {}, true, true},
        {{0, 3, 2, 6}, {}, true, true},
        {{0, 1, 3, 6}, {}, true, true},
        // If 1 or 2 is performed and the other is a loop: infeasible.
        {{0, 3, 2, 6}, {1}, false, false},
        {{0, 1, 3, 6}, {2}, false, false},
        // 1->2, 4->5: feasible for both
        {{0, 1, 2, 3, 4, 5, 6}, {}, true, true},
        // Same but one paired node is unassigned: feasible for both.
        {{0, 2, 3, 4, 5, 6}, {}, true, true},
        {{0, 1, 3, 4, 5, 6}, {}, true, true},
        {{0, 1, 2, 3, 5, 6}, {}, true, true},
        {{0, 1, 2, 3, 4, 6}, {}, true, true},
        // Same but one paired node is a loop: infeasible for both.
        {{0, 2, 3, 4, 5, 6}, {1}, false, false},
        {{0, 1, 3, 4, 5, 6}, {2}, false, false},
        {{0, 1, 2, 3, 5, 6}, {4}, false, false},
        {{0, 1, 2, 3, 4, 6}, {5}, false, false},
        // (1 (4 5) 2): LIFO feasible, FIFO infeasible.
        {{0, 1, 4, 3, 5, 2, 6}, {}, true, false},
        // Same but one paired node is unassigned: feasible for both.
        {{0, 4, 3, 5, 2, 6}, {}, true, true},
        {{0, 1, 3, 5, 2, 6}, {}, true, true},
        {{0, 1, 4, 3, 2, 6}, {}, true, true},
        {{0, 1, 4, 3, 5, 6}, {}, true, true},
        // Same but one paired node is a loop: infeasible for both.
        {{0, 4, 3, 5, 2, 6}, {1}, false, false},
        {{0, 1, 3, 5, 2, 6}, {4}, false, false},
        {{0, 1, 4, 3, 2, 6}, {5}, false, false},
        {{0, 1, 4, 3, 5, 6}, {2}, false, false},
        // 1 4 2 5: LIFO infeasible, FIFO feasible.
        {{0, 1, 4, 3, 2, 5, 6}, {}, false, true},
        // Same but one paired node is unassigned: feasible for both.
        {{0, 4, 3, 2, 5, 6}, {}, true, true},
        {{0, 1, 3, 2, 5, 6}, {}, true, true},
        {{0, 1, 4, 3, 5, 6}, {}, true, true},
        {{0, 1, 4, 3, 2, 6}, {}, true, true},
        // Same but one paired node is a loop: infeasible for both.
        {{0, 4, 3, 2, 5, 6}, {1}, false, false},
        {{0, 1, 3, 2, 5, 6}, {4}, false, false},
        {{0, 1, 4, 3, 5, 6}, {2}, false, false},
        {{0, 1, 4, 3, 2, 6}, {5}, false, false}};

    for (int t = 0; t < test_cases.size(); ++t) {
      SCOPED_TRACE(
          absl::StrCat("Test case ", t, " policy is ", lifo ? "LIFO" : "FIFO"));
      const TestCase& test_case = test_cases[t];
      PathStateChangePaths(&path_state, {test_case.path}, test_case.loops);
      const bool accept = filter->Accept(&delta, nullptr, kint64min, kint64max);
      EXPECT_EQ(accept, lifo ? test_case.expected_lifo_accept
                             : test_case.expected_fifo_accept);
      path_state.Revert();
    }
  }
}

TEST(RoutingFilterTest, PickupDeliveryFilterNoOrderAlternatives) {
  const int kNodes = 6;
  const int kVehicles = 1;
  const int kInitialSolution[] = {6, 1, 2, 3, 4, 5};  // 0->6
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  model.AddPickupAndDeliverySets(
      model.AddDisjunction({manager.NodeToIndex(NodeIndex(1)),
                            manager.NodeToIndex(NodeIndex(5))}),
      model.AddDisjunction({manager.NodeToIndex(NodeIndex(3)),
                            manager.NodeToIndex(NodeIndex(4))}));
  model.CloseModel();
  std::vector<Model::PickupAndDeliveryPolicy> policies(
      kVehicles, Model::PICKUP_AND_DELIVERY_NO_ORDER);
  PathState path_state(7, {0}, {6});
  LocalSearchFilter* filter = MakePickupDeliveryFilter(
      model, &path_state, model.GetPickupAndDeliveryPairs(), policies);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  path_state.Commit();
  Assignment delta(model.solver());
  // Empty delta
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  struct TestCase {
    std::vector<int> path;
    std::vector<int> loops;
    bool expected_accept;
  };
  const std::vector<TestCase> test_cases = {
      // 1 then 3: feasible, no matter the loops.
      {{0, 1, 2, 3, 6}, {}, true},
      {{0, 1, 2, 3, 6}, {4}, true},
      {{0, 1, 2, 3, 6}, {5}, true},
      {{0, 1, 2, 3, 6}, {4, 5}, true},
      // 5 then 3: feasible, no matter the loops.
      {{0, 5, 2, 3, 6}, {}, true},
      {{0, 5, 2, 3, 6}, {1}, true},
      {{0, 5, 2, 3, 6}, {4}, true},
      {{0, 5, 2, 3, 6}, {1, 4}, true},
      // 1 then 4: feasible, no matter the loops.
      {{0, 1, 2, 4, 6}, {}, true},
      {{0, 1, 2, 4, 6}, {3}, true},
      {{0, 1, 2, 4, 6}, {5}, true},
      {{0, 1, 2, 4, 6}, {3, 5}, true},
      // 5 then 4: feasible, no matter the loops.
      {{0, 5, 2, 4, 6}, {}, true},
      {{0, 5, 2, 4, 6}, {1}, true},
      {{0, 5, 2, 4, 6}, {3}, true},
      {{0, 5, 2, 4, 6}, {3, 1}, true},
      // 5 alone: feasible if both 3 and 4 are unassigned.
      {{0, 5, 2, 6}, {}, true},
      {{0, 5, 2, 6}, {1}, true},
      {{0, 5, 2, 6}, {3}, false},
      {{0, 5, 2, 6}, {4}, false},
      {{0, 5, 2, 6}, {3, 4}, false},
      // no paired nodes: feasible.
      {{0, 2, 6}, {}, true},
      {{0, 2, 6}, {1}, true},
      {{0, 2, 6}, {3}, true},
      // 3 alone: feasible if 1 and 5 are unassigned.
      {{0, 3, 2, 6}, {}, true},
      {{0, 3, 2, 6}, {4}, true},
      {{0, 3, 2, 6}, {4, 1}, false},
      {{0, 3, 2, 6}, {1}, false},
      {{0, 3, 2, 6}, {5}, false},
      {{0, 3, 2, 6}, {1, 5}, false},
      // 3 then 5: infeasible.
      {{0, 3, 2, 5, 6}, {}, false},
      {{0, 3, 2, 5, 6}, {1}, false},
      {{0, 3, 2, 5, 6}, {4}, false},
      {{0, 3, 2, 5, 6}, {1, 4}, false},
      // 4 then 1: infeasible.
      {{0, 4, 2, 1, 6}, {}, false},
      {{0, 4, 2, 1, 6}, {3}, false},
      {{0, 4, 2, 1, 6}, {5}, false},
      {{0, 4, 2, 1, 6}, {5, 3}, false},
  };
  for (int t = 0; t < test_cases.size(); ++t) {
    SCOPED_TRACE(absl::StrCat("Test case ", t));
    const TestCase& test_case = test_cases[t];
    PathStateChangePaths(&path_state, {test_case.path}, test_case.loops);
    const bool accept = filter->Accept(&delta, nullptr, kint64min, kint64max);
    EXPECT_EQ(accept, test_case.expected_accept);
    path_state.Revert();
  }
}

TEST(RoutingFilterTest, PickupDeliveryFilterLifoAndFifoWithAlternatives) {
  const int kNodes = 10;
  const int kVehicles = 1;
  const int kInitialSolution[] = {10, 1, 2, 3, 4, 5, 6, 7, 8, 9};  // 0 -> 10.
  for (const bool lifo : {true, false}) {
    const auto policy = lifo ? Model::PICKUP_AND_DELIVERY_LIFO
                             : Model::PICKUP_AND_DELIVERY_FIFO;
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    model.AddPickupAndDeliverySets(
        model.AddDisjunction({manager.NodeToIndex(NodeIndex(1)),
                              manager.NodeToIndex(NodeIndex(5))}),
        model.AddDisjunction({manager.NodeToIndex(NodeIndex(3)),
                              manager.NodeToIndex(NodeIndex(4))}));
    model.AddPickupAndDeliverySets(
        model.AddDisjunction({manager.NodeToIndex(NodeIndex(6)),
                              manager.NodeToIndex(NodeIndex(7))}),
        model.AddDisjunction({manager.NodeToIndex(NodeIndex(8)),
                              manager.NodeToIndex(NodeIndex(9))}));
    model.CloseModel();
    std::vector<Model::PickupAndDeliveryPolicy> policies(kVehicles, policy);
    PathState path_state(11, {0}, {10});
    LocalSearchFilter* filter = MakePickupDeliveryFilter(
        model, &path_state, model.GetPickupAndDeliveryPairs(), policies);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    Assignment delta(model.solver());
    struct TestCase {
      std::vector<int> path;
      std::vector<int> loops;
      bool expected_lifo_accept;
      bool expected_fifo_accept;
    };
    const std::vector<TestCase> test_cases = {
        // 1 3 6 8: feasible for both.
        {{0, 1, 2, 3, 6, 8, 10}, {}, true, true},
        // 5 4 7 9: feasible for both.
        {{0, 5, 2, 4, 7, 9, 10}, {}, true, true},
        // 1 6 8 3: lifo only.
        {{0, 1, 6, 2, 8, 3, 10}, {}, true, false},
        // 5 6 8 3: lifo only.
        {{0, 5, 6, 2, 8, 3, 10}, {}, true, false},
        // 5 7 8 3: lifo only.
        {{0, 5, 7, 2, 8, 3, 10}, {}, true, false},
        // 5 7 9 3: lifo only.
        {{0, 5, 7, 2, 9, 3, 10}, {}, true, false},
        // 1 6 3 8: fifo only.
        {{0, 1, 6, 2, 3, 8, 10}, {}, false, true},
        // 5 6 3 8: fifo only.
        {{0, 5, 6, 2, 3, 8, 10}, {}, false, true},
        // 5 7 3 8: fifo only.
        {{0, 5, 7, 2, 3, 8, 10}, {}, false, true},
        // 5 7 3 9: fifo only.
        {{0, 5, 7, 2, 3, 9, 10}, {}, false, true},
        // 4 5 7 9: infeasible for both.
        {{0, 4, 2, 5, 7, 9, 10}, {}, false, false},
        // 5 4 9 7: infeasible for both.
        {{0, 5, 2, 4, 9, 7, 10}, {}, false, false},
    };
    for (int t = 0; t < test_cases.size(); ++t) {
      SCOPED_TRACE(
          absl::StrCat("Test case ", t, " policy is ", lifo ? "LIFO" : "FIFO"));
      const TestCase& test_case = test_cases[t];
      PathStateChangePaths(&path_state, {test_case.path}, test_case.loops);
      const bool accept = filter->Accept(&delta, nullptr, kint64min, kint64max);
      EXPECT_EQ(accept, policy == Model::PICKUP_AND_DELIVERY_LIFO
                            ? test_case.expected_lifo_accept
                            : test_case.expected_fifo_accept);
      path_state.Revert();
    }
  }
}

TEST(RoutingFilterTest, VehicleVarFilter) {
  const int kNodes = 4;
  const int kVehicles = 2;
  // 0->1->3->5, 4->6. 2 is a loop.
  const int kInitialSolution[] = {1, 3, 2, 5, 6};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  model.VehicleVar(1)->RemoveValue(1);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(1)}), 0);
  model.VehicleVar(2)->RemoveValue(0);
  model.AddDisjunction(manager.NodesToIndices({NodeIndex(2)}), 0);
  model.CloseModel();
  PathState path_state(7, {0, 4}, {5, 6});
  LocalSearchFilter* filter = MakeVehicleVarFilter(model, &path_state);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  PathStateChangePaths(&path_state, {{0, 1, 3, 5}, {4, 6}}, {2});
  path_state.Commit();
  filter->Synchronize(&initial_solution, nullptr);
  Assignment delta(model.solver());
  // Empty delta
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  // Infeasible delta, moving 2 to vehicle 0.
  // 0->1->2->3->5, 4->6.
  const int kDelta1[] = {1, 2, 3, 5, 6};
  PathStateChangePaths(&path_state, {{0, 1, 2, 3, 5}, {4, 6}}, {});
  MakeDelta(kInitialSolution, kDelta1, model.Nexts(), &delta);
  EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  path_state.Revert();
  // Infeasible delta, moving 1 to vehicle 1.
  // 0->3->5, 4->1->6, 2 is a loop.
  const int kDelta2[] = {3, 6, 2, 5, 1};
  PathStateChangePaths(&path_state, {{0, 3, 5}, {4, 1, 6}}, {});
  MakeDelta(kInitialSolution, kDelta2, model.Nexts(), &delta);
  EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  path_state.Revert();
  // Feasible delta, moving 2 to vehicle 1.
  // 0->1->3->5, 4->2->6.
  const int kDelta3[] = {1, 3, 6, 5, 2};
  PathStateChangePaths(&path_state, {{0, 1, 3, 5}, {4, 2, 6}}, {});
  MakeDelta(kInitialSolution, kDelta3, model.Nexts(), &delta);
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  path_state.Revert();
  // Feasible delta, moving 3 to vehicle 1.
  // 0->1->5, 4->3->6.
  const int kDelta4[] = {1, 5, 2, 6, 3};
  PathStateChangePaths(&path_state, {{0, 1, 5}, {4, 3, 6}}, {});
  MakeDelta(kInitialSolution, kDelta4, model.Nexts(), &delta);
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  path_state.Revert();
  // Infeasible delta, moving 1 and 3 to vehicle 1.
  // 0->5, 4->1->3->6.
  const int kDelta5[] = {5, 3, 2, 6, 1};
  PathStateChangePaths(&path_state, {{0, 5}, {4, 1, 3, 6}}, {});
  MakeDelta(kInitialSolution, kDelta5, model.Nexts(), &delta);
  EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  path_state.Revert();
}

// Checking that vehicle variables get filtered whether vehicle costs are
// homogeneous or not.
TEST(RoutingFilterTest, VehicleVarFiltering) {
  for (bool homogeneous : {true, false}) {
    IndexManager manager(2, 3, NodeIndex(0));
    RoutingModelParameters model_parameters = DefaultRoutingModelParameters();
    ConstraintSolverParameters* const solver_parameters =
        model_parameters.mutable_solver_parameters();
    solver_parameters->set_profile_local_search(true);
    Model model(manager, model_parameters);
    model.VehicleVar(1)->RemoveValue(0);
    model.VehicleVar(1)->RemoveValue(2);
    if (!homogeneous) {
      model.SetArcCostEvaluatorOfVehicle(
          model.RegisterTransitCallback([](int64_t, int64_t) { return 1; }), 0);
    }
    model.AddDisjunction({1}, 1000);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_first_solution_strategy(
        FirstSolutionStrategy::ALL_UNPERFORMED);
    const Assignment* const solution = model.SolveWithParameters(parameters);
    EXPECT_NE(solution, nullptr);
    const LocalSearchStatistics stats =
        model.solver()->GetLocalSearchStatistics();
    for (const LocalSearchStatistics::LocalSearchFilterStatistics& stat :
         stats.local_search_filter_statistics()) {
      if (stat.local_search_filter() == "VehicleVariableFilter") {
        EXPECT_EQ(stat.num_rejects(), homogeneous ? 1 : 0);
      } else if (stat.local_search_filter() == "VariableDomainFilter") {
        EXPECT_EQ(stat.num_rejects(), homogeneous ? 0 : 1);
      }
    }
  }
}

TEST(RoutingFilterTest, MakeActiveNodeGroupFilterNoSameActivityConstraints) {
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-1-6, 4-2-7, 5-8, 3 unperformed
  const int kInitialSolution[] = {1, 6, 7, 3, 2, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  model.CloseModel();
  LocalSearchFilter* const filter = MakeActiveNodeGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0, no unperformed
    delta.Clear();
    // 0-1-3-6, 4-2-7, 5-8
    const int kFeasibleDelta[] = {1, 3, 7, 6, 2, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 unperformed
    delta.Clear();
    // 0-6, 4-2-7, 5-8, 1 and 3 unperformed
    const int kFeasibleDelta[] = {6, 1, 7, 3, 2, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes 1 and 2 have to have the same activity, 3 is unconstrained.
TEST(RoutingFilterTest, MakeActiveNodeGroupFilterWithSameActivityConstraints) {
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-1-6, 4-2-7, 5-8, 3 unperformed
  const int kInitialSolution[] = {1, 6, 7, 3, 2, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  Solver* solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(1), model.ActiveVar(2)));
  model.CloseModel();
  LocalSearchFilter* const filter = MakeActiveNodeGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0, no unperformed
    delta.Clear();
    // 0-1-3-6, 4-2-7, 5-8
    const int kFeasibleDelta[] = {1, 3, 7, 6, 2, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 unperformed
    delta.Clear();
    // 0-6, 4-2-7, 5-8, 1 and 3 unperformed
    const int kUnfeasibleDelta[] = {6, 1, 7, 3, 2, 8};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 and 2 unperformed
    delta.Clear();
    // 0-6, 4-7, 5-8, 1, 2 and 3 unperformed
    const int kFeasibleDelta[] = {6, 1, 2, 3, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes 1, 2, 3 have to have the same activity, testing with LNS fragments.
TEST(RoutingFilterTest,
     MakeActiveNodeGroupFilterWithSameActivityConstraintsAndLNS) {  // NOLINT
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-1-6, 4-2-7, 5-3-8
  const int kInitialSolution[] = {1, 6, 7, 8, 2, 3};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  Solver* solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(1), model.ActiveVar(2)));
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(1), model.ActiveVar(3)));
  model.CloseModel();
  LocalSearchFilter* const filter = MakeActiveNodeGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Adding 2 to LNS fragment
    delta.Clear();
    // 0-1-6, 4-7, 5-3-8
    const int kFeasibleDelta[] = {1, 6, 2, 8, 7, 3};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 and 3 unperformed, adding 2 to LNS fragment
    delta.Clear();
    // 0-6, 4-7, 5-8, 1 and 3 unperformed
    const int kFeasibleDelta[] = {6, 1, 2, 3, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 unperformed, adding 2 to LNS fragment
    delta.Clear();
    // 0-6, 4-7, 5-3-8, 1 unperformed
    const int kUnfeasibleDelta[] = {6, 1, 2, 8, 7, 3};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 3 unperformed, adding 2 to LNS fragment
    delta.Clear();
    // 0-1-6, 4-7, 5-8, 3 unperformed
    const int kUnfeasibleDelta[] = {1, 6, 2, 3, 7, 8};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes 1 and 2 have to have the same activity, 3 is unconstrained.
TEST(RoutingFilterTest,
     MakeActiveNodeGroupFilterWithSameActivityConstraintsInitiallyUnperformed) {
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-6, 4-7, 5-8, 1,2,3 unperformed
  const int kInitialSolution[] = {6, 1, 2, 3, 7, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  Solver* solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(1), model.ActiveVar(2)));
  model.CloseModel();
  LocalSearchFilter* const filter = MakeActiveNodeGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0
    delta.Clear();
    // 0-3-6, 4-7, 5-8, 1,2 unperformed
    const int kFeasibleDelta[] = {3, 1, 2, 6, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 performed
    delta.Clear();
    // 0-1-6, 4-7, 5-8, 2 and 3 unperformed
    const int kUnfeasibleDelta[] = {1, 6, 2, 3, 7, 8};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 and 2 performed
    delta.Clear();
    // 0-1-6, 4-2-7, 5-8, 1, 3 unperformed
    const int kFeasibleDelta[] = {1, 6, 7, 3, 2, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes 1 and 2 have to have the same activity, but node 2 is not synced.
// 3 is unconstrained.
TEST(RoutingFilterTest,
     MakeActiveNodeGroupFilterWithUnsyncedAndSameActivityConstraints) {
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-1-6, 4-7, 5-8, 3 unperformed, 2 unsynced
  const int kInitialSolution[] = {1, 6, -1, 3, 7, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  Solver* solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(1), model.ActiveVar(2)));
  model.CloseModel();
  LocalSearchFilter* const filter = MakeActiveNodeGroupFilter(model);
  Assignment initial_solution(model.solver());
  for (int i = 0; i < model.Nexts().size(); ++i) {
    if (kInitialSolution[i] > 0) {
      initial_solution.Add(model.Nexts()[i])->SetValue(kInitialSolution[i]);
    }
  }
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0, no unperformed
    delta.Clear();
    // 0-1-3-6, 4-7, 5-8
    const int kFeasibleDelta[] = {1, 3, -1, 6, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 unperformed, ok because 2 is not synced
    delta.Clear();
    // 0-6, 4-7, 5-8, 1 and 3 unperformed
    const int kFeasibleDelta[] = {6, 1, -1, 3, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes 1 and 2 have to have the same activity, but node 2 is not synced.
// 3 is unconstrained.
TEST(
    RoutingFilterTest,
    MakeActiveNodeGroupFilterWithUnsyncedAndSameActivityConstraintsInitiallyUnperformed) {  // NOLINT
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-6, 4-7, 5-8, 1 and 3 unperformed, 2 unsynced
  const int kInitialSolution[] = {6, 1, -1, 3, 7, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  Solver* solver = model.solver();
  solver->AddConstraint(
      solver->MakeEquality(model.ActiveVar(1), model.ActiveVar(2)));
  model.CloseModel();
  LocalSearchFilter* const filter = MakeActiveNodeGroupFilter(model);
  Assignment initial_solution(model.solver());
  for (int i = 0; i < model.Nexts().size(); ++i) {
    if (kInitialSolution[i] > 0) {
      initial_solution.Add(model.Nexts()[i])->SetValue(kInitialSolution[i]);
    }
  }
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0, 1 unperformed
    delta.Clear();
    // 0-3-6, 4-7, 5-8
    const int kFeasibleDelta[] = {3, 1, -1, 6, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 performed, ok because 2 is not synced
    delta.Clear();
    // 0-1-6, 4-7, 5-8, 3 unperformed
    const int kFeasibleDelta[] = {1, 6, -1, 3, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes 1,2,3 are constrained by an ordered activity constraint,
// 4 is unconstrained.
TEST(RoutingFilterTest,
     MakeOrderedActivityGroupFilterWithOrderedActivityConstraints) {
  const int kNodes = 5;
  const int kVehicles = 3;
  for (bool split_groups : {true, false}) {
    // First solution: 0-1-7, 5-2-8, 6-9, 3,4 unperformed
    const int kInitialSolution[] = {1, 7, 8, 3, 4, 2, 9};
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const DisjunctionIndex one = model.AddDisjunction({int64_t{1}}, 0);
    const DisjunctionIndex two = model.AddDisjunction({int64_t{2}}, 0);
    const DisjunctionIndex three = model.AddDisjunction({int64_t{3}}, 0);
    if (split_groups) {
      model.AddOrderedActivityGroup({one, two});
      model.AddOrderedActivityGroup({two, three});
    } else {
      model.AddOrderedActivityGroup({one, two, three});
    }
    model.CloseModel();
    LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    Assignment delta(model.solver());
    {
      // Empty delta
      delta.Clear();
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Moving 3 to route 0
      delta.Clear();
      // 0-1-3-7, 5-2-8, 6-9, 4 unperformed
      const int kFeasibleDelta[] = {1, 3, 8, 7, 4, 2, 9};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 unperformed
      delta.Clear();
      // 0-7, 5-2-8, 6-9, 1,3,4 unperformed
      const int kUnfeasibleDelta[] = {7, 1, 8, 3, 4, 2, 9};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 and 2 unperformed
      delta.Clear();
      // 0-7, 5-8, 6-9, 1,2,3,4 unperformed
      const int kFeasibleDelta[] = {7, 1, 2, 3, 4, 8, 9};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 2 unperformed and 3 performed
      delta.Clear();
      // 0-1-7, 5-8, 6-3-9, 2,4 unperformed
      const int kFeasibleDelta[] = {1, 7, 2, 9, 4, 8, 3};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
  }
}

// Ordered disjunction activity groups: {0,1} and {2,1,0,3}.
// If 1, 2 and 3 are unknown, and 0 becomes active, then:
// - 1 becomes active due to group 0,
// - then 2 becomes active due to group 1,
// - we need to be careful when we propagate 2 in group 1 to detect that 2 is
//   now outside the propagation range of group 1 (since it has laready been
//   propagated).
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithOrderedActivityConstraintsWithOOBPropagation) {  // NOLINT
  const int kNodes = 5;
  const int kVehicles = 1;
  // First solution: 0-5, 1, 2, 3 and 4 unsynced.
  const int kInitialSolution[] = {5, -1, -1, -1, -1};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex zero = model.AddDisjunction({int64_t{1}}, 0);
  const DisjunctionIndex one = model.AddDisjunction({int64_t{2}}, 0);
  const DisjunctionIndex two = model.AddDisjunction({int64_t{3}}, 0);
  const DisjunctionIndex three = model.AddDisjunction({int64_t{4}}, 0);
  model.AddOrderedActivityGroup({zero, one});
  model.AddOrderedActivityGroup({two, one, zero, three});
  model.CloseModel();
  LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Performing 1
    delta.Clear();
    // 0-1-5, 2, 3, 4 unsynced.
    const int kDelta[] = {1, 5, -1, -1, -1};
    MakeDelta(kInitialSolution, kDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Making sure the filtering works properly when there are duplicates in the
// ordered activity groups. In particular, for the group {0, 1, 2, 1, 3, 1}, if
// 1 becomes active then 0, 2 and 3 become active too (it shouldn't stop at 0).
// Likewise when 1 becomes inactive.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithOrderedActivityConstraintsWithDuplicates) {  // NOLINT
  const int kNodes = 5;
  const int kVehicles = 1;
  // First solution: 0-4-5, 1, 2 unsynced, 3 inactive.
  const int kInitialSolution[] = {4, -1, -1, 3, 5};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex zero = model.AddDisjunction({int64_t{1}}, 0);
  const DisjunctionIndex one = model.AddDisjunction({int64_t{2}}, 0);
  const DisjunctionIndex two = model.AddDisjunction({int64_t{3}}, 0);
  const DisjunctionIndex three = model.AddDisjunction({int64_t{4}}, 0);
  model.AddOrderedActivityGroup({zero, one, two, one, three, one});
  model.CloseModel();
  LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Performing 2 (disjunction one)
    delta.Clear();
    // 0-2-4-5, 1 unsynced, 3 inactive.
    const int kDelta[] = {2, -1, 4, 3, 5};
    MakeDelta(kInitialSolution, kDelta, model.Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Unperforming 2 (disjunction one), performing 1 (disjunction zero)
    delta.Clear();
    // 0-1-4-5, 2, 3 inactive.
    const int kDelta[] = {1, 4, 2, 3, 5};
    MakeDelta(kInitialSolution, kDelta, model.Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes {1,2},{3,4},{5,6} are constrained by an ordered activity constraint,
// 7 is unconstrained.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithOrderedActivityConstraintsWithAlternatives) {  // NOLINT
  const int kNodes = 8;
  const int kVehicles = 3;
  for (bool split_groups : {true, false}) {
    // First solution: 0-1-10, 8-3-11, 9-12, 2,4,5,6,7 unperformed
    const int kInitialSolution[] = {1, 10, 2, 11, 4, 5, 6, 7, 3, 12};
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const DisjunctionIndex one = model.AddDisjunction({1, 2}, 0);
    const DisjunctionIndex two = model.AddDisjunction({3, 4}, 0);
    const DisjunctionIndex three = model.AddDisjunction({5, 6}, 0);
    if (split_groups) {
      model.AddOrderedActivityGroup({one, two});
      model.AddOrderedActivityGroup({two, three});
    } else {
      model.AddOrderedActivityGroup({one, two, three});
    }
    model.CloseModel();
    LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    Assignment delta(model.solver());
    {
      // Empty delta
      delta.Clear();
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Moving 6 to route 0
      delta.Clear();
      // 0-1-6-10, 8-3-11, 9-12, 2,4,5,7 unperformed
      const int kFeasibleDelta[] = {1, 6, 2, 11, 4, 5, 10, 7, 3, 12};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 unperformed
      delta.Clear();
      // 0-10, 8-3-11, 9-12, 1,2,4,5,6,7 unperformed
      const int kUnfeasibleDelta[] = {10, 1, 2, 11, 4, 5, 6, 7, 3, 12};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 and 3 unperformed
      delta.Clear();
      // 0-10, 8-11, 9-12, 1,2,3,4,5,6,7 unperformed
      const int kFeasibleDelta[] = {10, 1, 2, 3, 4, 5, 6, 7, 11, 12};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 3 unperformed and 6 performed
      delta.Clear();
      // 0-1-10, 8-11, 9-6-12, 2,3,4,5,7 unperformed
      const int kFeasibleDelta[] = {1, 10, 2, 3, 4, 5, 12, 7, 11, 6};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
  }
}

// Nodes {1,2},{3,4},{5,6} are constrained by an ordered activity constraint,
// 7 is unconstrained. This is a variant with max cardinality 2 on disjunctions.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithOrderedActivityConstraintsWithAlternativesAndMaxCardinality) {  // NOLINT
  const int kNodes = 8;
  const int kVehicles = 3;
  for (bool split_groups : {true, false}) {
    // First solution: 0-1-2-10, 8-3-4-11, 9-12, 5,6,7 unperformed
    const int kInitialSolution[] = {1, 2, 10, 4, 11, 5, 6, 7, 3, 12};
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const DisjunctionIndex one =
        model.AddDisjunction({1, 2}, 0, /*max_cardinality=*/2);
    const DisjunctionIndex two =
        model.AddDisjunction({3, 4}, 0, /*max_cardinality=*/2);
    const DisjunctionIndex three =
        model.AddDisjunction({5, 6}, 0, /*max_cardinality=*/2);
    if (split_groups) {
      model.AddOrderedActivityGroup({one, two});
      model.AddOrderedActivityGroup({two, three});
    } else {
      model.AddOrderedActivityGroup({one, two, three});
    }
    model.CloseModel();
    LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    Assignment delta(model.solver());
    {
      // Empty delta
      delta.Clear();
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Moving 6 to route 0
      delta.Clear();
      // 0-1-2-6-10, 8-3-4-11, 9-12, 5,7 unperformed
      const int kFeasibleDelta[] = {1, 2, 6, 4, 11, 5, 10, 7, 3, 12};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 unperformed
      delta.Clear();
      // 0-2-10, 8-3-4-11, 9-12, 1,5,6,7 unperformed
      const int kUnfeasibleDelta[] = {2, 1, 10, 4, 11, 5, 6, 7, 3, 12};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 and 3 unperformed
      delta.Clear();
      // 0-2-10, 8-4-11, 9-12, 1,3,5,6,7 unperformed
      const int kFeasibleDelta[] = {2, 1, 10, 3, 11, 5, 6, 7, 4, 12};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 3 unperformed and 5,6 performed
      delta.Clear();
      // 0-1-2-10, 8-4-11, 9-5-6-12, 3,7 unperformed
      const int kFeasibleDelta[] = {1, 2, 10, 3, 11, 6, 12, 7, 4, 5};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
  }
}

// An ordered activity constraint with cycles. Should behave as same activity
// constraints.
TEST(RoutingFilterTest,
     MakeOrderedActivityGroupFilterWithCyclicOrderedActivityConstraints) {
  const int kNodes = 5;
  const int kVehicles = 3;
  for (bool split_groups : {true, false}) {
    // First solution: 0-1-7, 5-2-8, 6-3-9, 4 unperformed
    const int kInitialSolution[] = {1, 7, 8, 9, 4, 2, 3};
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const DisjunctionIndex one = model.AddDisjunction({int64_t{1}}, 0);
    const DisjunctionIndex two = model.AddDisjunction({int64_t{2}}, 0);
    const DisjunctionIndex three = model.AddDisjunction({int64_t{3}}, 0);
    if (split_groups) {
      model.AddOrderedActivityGroup({one, two});
      model.AddOrderedActivityGroup({two, three});
    } else {
      model.AddOrderedActivityGroup({one, two, three});
    }
    model.AddOrderedActivityGroup({three, one});
    model.CloseModel();
    LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    Assignment delta(model.solver());
    {
      // Empty delta
      delta.Clear();
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 unperformed
      delta.Clear();
      // 0-7, 5-2-8, 6-3-9, 1,4 unperformed
      const int kUnfeasibleDelta[] = {7, 1, 8, 9, 4, 2, 3};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 and 2 unperformed
      delta.Clear();
      // 0-7, 5-8, 6-3-9, 1,2,4 unperformed
      const int kFeasibleDelta[] = {7, 1, 2, 9, 4, 8, 3};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1,2,3 unperformed
      delta.Clear();
      // 0-7, 5-8, 6-9, 1,2,3,4 unperformed
      const int kFeasibleDelta[] = {7, 1, 2, 3, 4, 8, 9};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
  }
}

// An ordered activity constraint with cycles and alternatives. Should behave as
// same activity constraints.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithCyclicOrderedActivityConstraintsWithAlternatives) {  // NOLINT
  const int kNodes = 8;
  const int kVehicles = 3;
  for (bool split_groups : {true, false}) {
    // First solution: 0-1-10, 8-3-11, 9-5-12, 2,4,6,7 unperformed
    const int kInitialSolution[] = {1, 10, 2, 11, 4, 12, 6, 7, 3, 5};
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const DisjunctionIndex one = model.AddDisjunction({1, 2}, 0);
    const DisjunctionIndex two = model.AddDisjunction({3, 4}, 0);
    const DisjunctionIndex three = model.AddDisjunction({5, 6}, 0);
    if (split_groups) {
      model.AddOrderedActivityGroup({one, two});
      model.AddOrderedActivityGroup({two, three});
    } else {
      model.AddOrderedActivityGroup({one, two, three});
    }
    model.AddOrderedActivityGroup({three, one});
    model.CloseModel();
    LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    Assignment delta(model.solver());
    {
      // Empty delta
      delta.Clear();
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 unperformed
      delta.Clear();
      // 0-10, 8-3-11, 9-5-12, 1,2,4,6,7 unperformed
      const int kUnfeasibleDelta[] = {10, 1, 2, 11, 4, 12, 6, 7, 3, 5};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 and 3 unperformed
      delta.Clear();
      // 0-10, 8-11, 9-5-12, 1,2,3,4,6,7 unperformed
      const int kFeasibleDelta[] = {10, 1, 2, 3, 4, 12, 6, 7, 11, 5};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1,3,5 unperformed
      delta.Clear();
      // 0-10, 8-11, 9-12, 1,2,3,4,5,6,7 unperformed
      const int kFeasibleDelta[] = {10, 1, 2, 3, 4, 5, 6, 7, 11, 12};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
  }
}

// Nodes 1,2,3 are constrained by an ordered activity constraint,
// testing with LNS fragments.
TEST(RoutingFilterTest,
     MakeOrderedActivityGroupFilterWithOrderedActivityConstraintsAndLNS) {
  const int kNodes = 4;
  const int kVehicles = 3;
  for (bool split_groups : {true, false}) {
    // First solution: 0-1-6, 4-2-7, 5-3-8
    const int kInitialSolution[] = {1, 6, 7, 8, 2, 3};
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const DisjunctionIndex one = model.AddDisjunction({int64_t{1}}, 0);
    const DisjunctionIndex two = model.AddDisjunction({int64_t{2}}, 0);
    const DisjunctionIndex three = model.AddDisjunction({int64_t{3}}, 0);
    if (split_groups) {
      model.AddOrderedActivityGroup({one, two});
      model.AddOrderedActivityGroup({two, three});
    } else {
      model.AddOrderedActivityGroup({one, two, three});
    }
    model.CloseModel();
    LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    Assignment delta(model.solver());
    {
      // Adding 2 to LNS fragment
      delta.Clear();
      // 0-1-6, 4-7, 5-3-8
      const int kFeasibleDelta[] = {1, 6, 2, 8, 7, 3};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      delta.SetRange(model.Nexts()[2], 0, 8);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 and 3 unperformed, adding 2 to LNS fragment
      delta.Clear();
      // 0-6, 4-7, 5-8, 1 and 3 unperformed
      const int kFeasibleDelta[] = {6, 1, 2, 3, 7, 8};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      delta.SetRange(model.Nexts()[2], 0, 8);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 unperformed, adding 2 to LNS fragment
      delta.Clear();
      // 0-6, 4-7, 5-3-8, 1 unperformed
      const int kUnfeasibleDelta[] = {6, 1, 2, 8, 7, 3};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      delta.SetRange(model.Nexts()[2], 0, 8);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 3 unperformed, adding 2 to LNS fragment
      delta.Clear();
      // 0-1-6, 4-7, 5-8, 3 unperformed
      const int kUnfeasibleDelta[] = {1, 6, 2, 3, 7, 8};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      delta.SetRange(model.Nexts()[2], 0, 8);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
  }
}

// Nodes {1,2},{3,4},{5,6} are constrained by an ordered activity constraint,
// testing with LNS fragments.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithOrderedActivityConstraintsWithAlternativesAndLNS) {  // NOLINT
  const int kNodes = 7;
  const int kVehicles = 3;
  for (bool split_groups : {true, false}) {
    // First solution: 0-1-9, 7-3-10, 8-5-11
    const int kInitialSolution[] = {1, 9, 2, 10, 4, 11, 6, 3, 5};
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    const DisjunctionIndex one = model.AddDisjunction({1, 2}, 0);
    const DisjunctionIndex two = model.AddDisjunction({3, 4}, 0);
    const DisjunctionIndex three = model.AddDisjunction({5, 6}, 0);
    if (split_groups) {
      model.AddOrderedActivityGroup({one, two});
      model.AddOrderedActivityGroup({two, three});
    } else {
      model.AddOrderedActivityGroup({one, two, three});
    }
    model.CloseModel();
    LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    Assignment delta(model.solver());
    {
      // Adding 3 to LNS fragment
      delta.Clear();
      // 0-1-9, 7-10, 8-5-11
      const int kFeasibleDelta[] = {1, 9, 2, 3, 4, 11, 6, 10, 5};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      delta.SetRange(model.Nexts()[3], 0, 8);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 and 5 unperformed, adding 3 to LNS fragment
      delta.Clear();
      // 0-9, 7-10, 8-11, 1 and 5 unperformed
      const int kFeasibleDelta[] = {0, 1, 2, 3, 4, 5, 6, 10, 11};
      MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
      delta.SetRange(model.Nexts()[3], 0, 11);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 1 unperformed, adding 3 to LNS fragment
      delta.Clear();
      // 0-9, 7-10, 8-5-11, 1 unperformed
      const int kUnfeasibleDelta[] = {9, 1, 2, 3, 4, 11, 6, 10, 5};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      delta.SetRange(model.Nexts()[3], 0, 11);
      EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
    {
      // Making 5 unperformed, adding 3 to LNS fragment
      delta.Clear();
      // 0-1-9, 7-10, 8-11, 5 unperformed
      const int kUnfeasibleDelta[] = {1, 9, 2, 3, 4, 5, 6, 10, 11};
      MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
      delta.SetRange(model.Nexts()[3], 0, 11);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    }
  }
}

// 3 intersecting groups of 3 nodes.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithIntersectingOrderedActivityConstraintsAndLNS) {  // NOLINT
  const int kNodes = 8;
  const int kVehicles = 3;
  // First solution: 0-1-2-3-10, 8-4-5-11, 9-6-7-12
  const int kInitialSolution[] = {1, 2, 3, 10, 5, 11, 7, 12, 4, 6};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex one = model.AddDisjunction({int64_t{1}}, 0);
  const DisjunctionIndex two = model.AddDisjunction({int64_t{2}}, 0);
  const DisjunctionIndex three = model.AddDisjunction({int64_t{3}}, 0);
  const DisjunctionIndex four = model.AddDisjunction({int64_t{4}}, 0);
  const DisjunctionIndex five = model.AddDisjunction({int64_t{5}}, 0);
  const DisjunctionIndex six = model.AddDisjunction({int64_t{6}}, 0);
  const DisjunctionIndex seven = model.AddDisjunction({int64_t{7}}, 0);
  model.AddOrderedActivityGroup({one, two, three});
  model.AddOrderedActivityGroup({four, two, five});
  model.AddOrderedActivityGroup({six, five, seven});
  model.CloseModel();
  LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Adding 2 to LNS fragment
    delta.Clear();
    // 0-1-3-10, 8-4-5-11, 9-6-7-12
    const int kFeasibleDelta[] = {1, 3, 2, 10, 5, 11, 7, 12, 4, 6};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 unperformed, adding 2 to LNS fragment
    delta.Clear();
    // 0-3-10, 8-4-5-11, 9-6-7-12, 1 unperformed
    const int kFeasibleDelta[] = {3, 1, 2, 10, 5, 11, 7, 12, 4, 6};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1, 5 unperformed, adding 2 to LNS fragment
    delta.Clear();
    // 0-3-10, 8-4-11, 9-6-7-12, 1,5 unperformed
    const int kUnfeasibleDelta[] = {3, 1, 2, 10, 11, 5, 7, 12, 4, 6};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1, 5, 7 unperformed, adding 2 to LNS fragment
    delta.Clear();
    // 0-3-10, 8-4-11, 9-6-12, 1,5,7 unperformed
    const int kUnfeasibleDelta[] = {3, 1, 2, 10, 11, 5, 12, 7, 4, 6};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1, 3, 5, 7 unperformed, adding 2 to LNS fragment
    delta.Clear();
    // 0-10, 8-4-11, 9-6-12, 1,3,5,7 unperformed
    const int kUnfeasibleDelta[] = {10, 1, 2, 3, 11, 5, 12, 7, 4, 6};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1, 3, 5, 7 unperformed, adding 2,3 to LNS fragment
    delta.Clear();
    // 0-10, 8-4-11, 9-6-12, 1,3,5,7 unperformed
    const int kUnfeasibleDelta[] = {10, 1, 2, 3, 11, 5, 12, 7, 4, 6};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[2], 0, 8);
    delta.SetRange(model.Nexts()[3], 0, 8);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// 3 intersecting groups of 3 disjunctions.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithIntersectingOrderedActivityConstraintsWithAlternativesAndLNS) {  // NOLINT
  const int kNodes = 15;
  const int kVehicles = 3;
  // First solution: 0-1-3-5-17, 15-7-9-18, 16-11-13-19
  const int kInitialSolution[] = {1,  3,  2,  5,  4,  17, 6, 9, 8,
                                  18, 10, 13, 12, 19, 14, 7, 11};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex one = model.AddDisjunction({1, 2}, 0);
  const DisjunctionIndex two = model.AddDisjunction({3, 4}, 0);
  const DisjunctionIndex three = model.AddDisjunction({5, 6}, 0);
  const DisjunctionIndex four = model.AddDisjunction({7, 8}, 0);
  const DisjunctionIndex five = model.AddDisjunction({9, 10}, 0);
  const DisjunctionIndex six = model.AddDisjunction({11, 12}, 0);
  const DisjunctionIndex seven = model.AddDisjunction({13, 14}, 0);
  model.AddOrderedActivityGroup({one, two, three});
  model.AddOrderedActivityGroup({four, two, five});
  model.AddOrderedActivityGroup({six, five, seven});
  model.CloseModel();
  LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Adding 3,4 to LNS fragment
    delta.Clear();
    // 0-1-5-17, 15-7-9-18, 16-11-13-19
    const int kFeasibleDelta[] = {1,  5,  2,  4,  3,  17, 6, 9, 8,
                                  18, 10, 13, 12, 19, 14, 7, 11};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[3], 0, 19);
    delta.SetRange(model.Nexts()[4], 0, 19);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1,2 unperformed, adding 3,4 to LNS fragment
    delta.Clear();
    // 0-5-17, 15-7-9-18, 16-11-13-19, 1,2 unperformed
    const int kFeasibleDelta[] = {5,  1,  2,  4,  3,  17, 6, 9, 8,
                                  18, 10, 13, 12, 19, 14, 7, 11};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[3], 0, 19);
    delta.SetRange(model.Nexts()[4], 0, 19);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1,2,9.10 unperformed, adding 3,4 to LNS fragment
    delta.Clear();
    // 0-5-17, 15-7-18, 16-11-13-19, 1,2,9,10 unperformed
    const int kUnfeasibleDelta[] = {5, 1,  2,  4,  3,  17, 6, 18, 8,
                                    9, 10, 13, 12, 19, 14, 7, 11};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[3], 0, 19);
    delta.SetRange(model.Nexts()[4], 0, 19);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1,2, 9,10, 13,14 unperformed, adding 3,4 to LNS fragment
    delta.Clear();
    // 0-5-17, 15-7-18, 16-11-19, 1,2,9,10,13,14 unperformed
    const int kUnfeasibleDelta[] = {5, 1,  2,  4,  3,  17, 6, 18, 8,
                                    9, 10, 19, 12, 13, 14, 7, 11};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[3], 0, 19);
    delta.SetRange(model.Nexts()[4], 0, 19);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1,2, 5,6, 9,10, 13,14 unperformed, adding 3,4 to LNS fragment
    delta.Clear();
    // 0-17, 15-7-18, 16-11-19, 1,2,5,6,9,10,13,14 unperformed
    const int kUnfeasibleDelta[] = {17, 1,  2,  4,  3,  5,  6, 18, 8,
                                    9,  10, 19, 12, 13, 14, 7, 11};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[3], 0, 19);
    delta.SetRange(model.Nexts()[4], 0, 19);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1,2, 9,10, 13,14 unperformed, adding 3,4,5,6 to LNS fragment
    delta.Clear();
    // 0-17, 15-7-18, 16-11-19, 1,2,9,10,13,14 unperformed
    const int kUnfeasibleDelta[] = {17, 1,  2,  4,  3,  6,  5, 18, 8,
                                    9,  10, 19, 12, 13, 14, 7, 11};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    delta.SetRange(model.Nexts()[3], 0, 19);
    delta.SetRange(model.Nexts()[4], 0, 19);
    delta.SetRange(model.Nexts()[5], 0, 19);
    delta.SetRange(model.Nexts()[6], 0, 19);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes 1,2 are constrained by an ordered activity constraint, 3 is
// unconstrained.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithOrderedActivityConstraintsInitiallyUnperformed) {  // NOLINT
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-6, 4-7, 5-8, 1,2,3 unperformed
  const int kInitialSolution[] = {6, 1, 2, 3, 7, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex one = model.AddDisjunction({int64_t{1}}, 0);
  const DisjunctionIndex two = model.AddDisjunction({int64_t{2}}, 0);
  model.AddOrderedActivityGroup({one, two});
  model.CloseModel();
  LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0
    delta.Clear();
    // 0-3-6, 4-7, 5-8, 1,2 unperformed
    const int kFeasibleDelta[] = {3, 1, 2, 6, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 performed
    delta.Clear();
    // 0-1-6, 4-7, 5-8, 2 and 3 unperformed
    const int kUnfeasibleDelta[] = {1, 6, 2, 3, 7, 8};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 2 performed
    delta.Clear();
    // 0-6, 4-2-7, 5-8, 1 and 3 unperformed
    const int kUnfeasibleDelta[] = {6, 1, 7, 3, 2, 8};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 and 2 performed
    delta.Clear();
    // 0-1-6, 4-2-7, 5-8, 1, 3 unperformed
    const int kFeasibleDelta[] = {1, 6, 7, 3, 2, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// {1,2}, {3,4} are constrained by an ordered activity constraint, 5 is
// unconstrained.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithOrderedActivityConstraintsWithAlternativesInitiallyUnperformed) {  // NOLINT
  const int kNodes = 6;
  const int kVehicles = 3;
  // First solution: 0-8, 6-9, 7-10, 1,2,3,4,5 unperformed
  const int kInitialSolution[] = {8, 1, 2, 3, 4, 5, 9, 10};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex one = model.AddDisjunction({1, 2}, 0);
  const DisjunctionIndex two = model.AddDisjunction({3, 4}, 0);
  model.AddOrderedActivityGroup({one, two});
  model.CloseModel();
  LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 5 to route 0
    delta.Clear();
    // 0-5-8, 6-9, 7-10, 1,2,3,4 unperformed
    const int kFeasibleDelta[] = {5, 1, 2, 3, 4, 8, 9, 10};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1,2 performed
    delta.Clear();
    // 0-1-2-8, 6-9, 7-10, 3,4,5 unperformed
    const int kUnfeasibleDelta[] = {1, 2, 8, 3, 4, 5, 9, 10};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 3,4 performed
    delta.Clear();
    // 0-8, 6-3-4-9, 7-10, 1,2,5 unperformed
    const int kUnfeasibleDelta[] = {8, 1, 2, 4, 9, 5, 3, 10};
    MakeDelta(kInitialSolution, kUnfeasibleDelta, model.Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1,2 and 3,4 performed
    delta.Clear();
    // 0-1-2-8, 6-3-4-9, 7-10, 5 unperformed
    const int kFeasibleDelta[] = {1, 2, 8, 3, 9, 5, 3, 10};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// TODO(user): Port the following unit tests to variants with alternatives.

// Nodes 1,2 are constrained by an ordered activity constraint, but node 2 is
// not synced. 3 is unconstrained.
TEST(RoutingFilterTest,
     MakeOrderedActivityGroupFilterWithUnsyncedAndOrderedActivityConstraints) {
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-1-6, 4-7, 5-8, 3 unperformed, 2 unsynced
  const int kInitialSolution[] = {1, 6, -1, 3, 7, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex one = model.AddDisjunction({int64_t{1}}, 0);
  const DisjunctionIndex two = model.AddDisjunction({int64_t{2}}, 0);
  model.AddOrderedActivityGroup({one, two});
  model.CloseModel();
  LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
  Assignment initial_solution(model.solver());
  for (int i = 0; i < model.Nexts().size(); ++i) {
    if (kInitialSolution[i] > 0) {
      initial_solution.Add(model.Nexts()[i])->SetValue(kInitialSolution[i]);
    }
  }
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0, no unperformed
    delta.Clear();
    // 0-1-3-6, 4-7, 5-8
    const int kFeasibleDelta[] = {1, 3, -1, 6, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 unperformed, ok because 2 is not synced
    delta.Clear();
    // 0-6, 4-7, 5-8, 1 and 3 unperformed
    const int kFeasibleDelta[] = {6, 1, -1, 3, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Nodes 1,2 are constrained by an ordered activity constraint, but node 2 is
// not synced. 3 is unconstrained.
TEST(
    RoutingFilterTest,
    MakeOrderedActivityGroupFilterWithUnsyncedAndOrderedActivityConstraintsInitiallyUnperformed) {  // NOLINT
  const int kNodes = 4;
  const int kVehicles = 3;
  // First solution: 0-6, 4-7, 5-8, 1 and 3 unperformed, 2 unsynced
  const int kInitialSolution[] = {6, 1, -1, 3, 7, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  const DisjunctionIndex one = model.AddDisjunction({int64_t{1}}, 0);
  const DisjunctionIndex two = model.AddDisjunction({int64_t{2}}, 0);
  model.AddOrderedActivityGroup({one, two});
  model.CloseModel();
  LocalSearchFilter* const filter = MakeOrderedActivityGroupFilter(model);
  Assignment initial_solution(model.solver());
  for (int i = 0; i < model.Nexts().size(); ++i) {
    if (kInitialSolution[i] > 0) {
      initial_solution.Add(model.Nexts()[i])->SetValue(kInitialSolution[i]);
    }
  }
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0, 1 unperformed
    delta.Clear();
    // 0-3-6, 4-7, 5-8
    const int kFeasibleDelta[] = {3, 1, -1, 6, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Making 1 performed, ok because 2 is not synced
    delta.Clear();
    // 0-1-6, 4-7, 5-8, 3 unperformed
    const int kFeasibleDelta[] = {1, 6, -1, 3, 7, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

TEST(RoutingFilterTest, MaxActiveVehiclesFilter) {
  const int kNodes = 4;
  const int kVehicles = 3;
  const int kMaxActiveVehicles = 2;
  // Initial solution: 0-1-6, 4-2-3-7, 5-8
  const int kInitialSolution[] = {1, 6, 3, 7, 2, 8};
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  model.SetMaximumNumberOfActiveVehicles(kMaxActiveVehicles);
  model.CloseModel();
  LocalSearchFilter* const filter = MakeMaxActiveVehiclesFilter(model);
  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
  Assignment delta(model.solver());
  {
    // Empty delta
    delta.Clear();
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 0, still 2 vehicles used
    delta.Clear();
    const int kFeasibleDelta[] = {1, 3, 7, 6, 2, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 3 to route 2, 3 vehicles used, not ok
    delta.Clear();
    const int kInfeasibleDelta[] = {1, 6, 7, 8, 2, 3};
    MakeDelta(kInitialSolution, kInfeasibleDelta, model.Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 1 to route 1, 1 vehicle used
    delta.Clear();
    const int kFeasibleDelta[] = {6, 2, 3, 7, 1, 8};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
  {
    // Moving 1 to route 2, 2 vehicles used
    delta.Clear();
    const int kFeasibleDelta[] = {6, 8, 3, 7, 2, 1};
    MakeDelta(kInitialSolution, kFeasibleDelta, model.Nexts(), &delta);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

TEST(HasHardDisjunctionsTest, NoDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  EXPECT_FALSE(model.HasHardDisjunctions());
}

TEST(HasHardDisjunctionsTest, NoMandatoryDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, 10);
  model.AddDisjunction({2, 3}, 5);
  EXPECT_TRUE(model.HasHardDisjunctions());
}

TEST(HasHardDisjunctionsTest, ImplicitMandatoryDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({3});
  EXPECT_TRUE(model.HasHardDisjunctions());
}

TEST(HasHardDisjunctionsTest, ExplicitMandatoryDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({3}, Model::kNoPenalty);
  EXPECT_TRUE(model.HasHardDisjunctions());
}

TEST(HasHardDisjunctionsTest, UnaryDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, 10);
  model.AddDisjunction({3});
  EXPECT_TRUE(model.HasHardDisjunctions());
}

TEST(HasHardDisjunctionsTest, ConstrainedNonUnaryDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, 10);
  model.AddDisjunction({3, 2});
  EXPECT_TRUE(model.HasHardDisjunctions());
}

TEST(HasHardDisjunctionsTest, UnconstrainedNonUnaryDisjunctions) {
  IndexManager manager(4, 1, NodeIndex(0));
  Model model(manager);
  model.AddDisjunction({1}, 10);
  model.AddDisjunction({3, 2}, 100, 2);
  EXPECT_FALSE(model.HasHardDisjunctions());
}

struct NodeDisjunctionFilterScenario {
  struct Delta {
    std::vector<std::vector<int>> paths;
    std::vector<int> unperformed;
    int relaxed_node = -1;
    int relaxed_min = 0;
    int relaxed_max = 0;
    bool feasible = true;
    int64_t expected_objective_value = 0;
  };

  int num_nodes;
  int num_vehicles;
  std::vector<Model::Disjunction> disjunctions;
  std::vector<std::vector<int>> initial_paths;
  std::vector<int> initial_unperformed;
  int64_t expected_synchronized_objective_value;
  std::vector<Delta> deltas;
};

class NodeDisjunctionFilterTestWithScenario
    : public ::testing::TestWithParam<NodeDisjunctionFilterScenario> {
 protected:
  void SetUp() override {
    const NodeDisjunctionFilterScenario& scenario = GetParam();
    manager_ = std::make_unique<IndexManager>(
        scenario.num_nodes, scenario.num_vehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(*manager_);
    for (const Model::Disjunction& disj : scenario.disjunctions) {
      std::vector<int64_t> node_indices;
      for (const int64_t node : disj.indices) {
        node_indices.push_back(manager_->NodeToIndex(NodeIndex(node)));
      }
      Model::DisjunctionIndex index = model_->MakeDisjunction(node_indices);
      model_->SetDisjunctionHardMinimum(
          index,
          std::clamp<int64_t>(disj.min_cardinality, 0, disj.indices.size()));
      model_->SetDisjunctionSoftMinimum(
          index,
          std::clamp<int64_t>(disj.soft_min_cardinality, 0,
                              disj.indices.size()),
          disj.soft_min_penalty, disj.soft_min_penalty_type);
      model_->SetDisjunctionHardMaximum(
          index,
          std::clamp<int64_t>(disj.max_cardinality, 0, disj.indices.size()));
      model_->SetDisjunctionSoftMaximum(
          index,
          std::clamp<int64_t>(disj.soft_max_cardinality, 0,
                              disj.indices.size()),
          disj.soft_max_penalty, disj.soft_max_penalty_type);
    }
    model_->CloseModel();
  }

  void RunTest(bool filter_cost) {
    const NodeDisjunctionFilterScenario& scenario = GetParam();
    Solver* const solver = model_->solver();
    LocalSearchFilter* const filter =
        MakeNodeDisjunctionFilter(*model_, filter_cost);

    auto initial_solution = std::make_unique<Assignment>(solver);
    for (const std::vector<int>& path : scenario.initial_paths) {
      for (int i = 1; i < path.size(); ++i) {
        initial_solution->Add(model_->Nexts()[path[i - 1]])->SetValue(path[i]);
      }
    }
    for (const int loop : scenario.initial_unperformed) {
      initial_solution->Add(model_->Nexts()[loop])->SetValue(loop);
    }

    filter->Synchronize(initial_solution.get(), nullptr);
    if (filter_cost) {
      EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                scenario.expected_synchronized_objective_value);
    } else {
      EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    }

    for (const auto& delta_scenario : scenario.deltas) {
      std::unique_ptr<Assignment> delta;
      if (delta_scenario.relaxed_node >= 0) {
        delta = std::make_unique<Assignment>(solver);
        delta->Add(model_->Nexts()[delta_scenario.relaxed_node])
            ->SetRange(delta_scenario.relaxed_min, delta_scenario.relaxed_max);
      } else {
        delta = MakeDeltaFromPaths(solver, model_->Nexts(),
                                   scenario.initial_paths, delta_scenario.paths,
                                   delta_scenario.unperformed);
      }

      const int64_t expected_value =
          filter_cost ? delta_scenario.expected_objective_value : 0;
      EXPECT_EQ(delta_scenario.feasible,
                filter->Accept(delta.get(), nullptr, kint64min, kint64max));
      if (delta_scenario.feasible && delta_scenario.relaxed_node < 0) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), expected_value);
      }
    }
  }

  std::unique_ptr<IndexManager> manager_;
  std::unique_ptr<Model> model_;
};

TEST_P(NodeDisjunctionFilterTestWithScenario, WithFilterCostOn) {
  RunTest(/*filter_cost=*/true);
}

TEST_P(NodeDisjunctionFilterTestWithScenario, WithFilterCostOff) {
  RunTest(/*filter_cost=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    SeveralDisjunctionsSoftMinHardMax, NodeDisjunctionFilterTestWithScenario,
    ::testing::Values(
        NodeDisjunctionFilterScenario{
            .num_nodes = 4,
            .num_vehicles = 1,
            .disjunctions =
                {
                    {.indices = {1, 2},
                     .soft_min_cardinality = 1,
                     .soft_min_penalty = 10,
                     .max_cardinality = 1},
                    {.indices = {1, 3},
                     .soft_min_cardinality = 1,
                     .soft_min_penalty = 10,
                     .max_cardinality = 1},
                    {.indices = {1, 2, 3},
                     .soft_min_cardinality = 2,
                     .soft_min_penalty = 10,
                     .max_cardinality = 2},
                    {.indices = {3},
                     .soft_min_cardinality = 1,
                     .soft_min_penalty = 10,
                     .max_cardinality = 1},
                },
            .initial_paths = {{0, 2, 3, 4}},
            .initial_unperformed = {1},
            .expected_synchronized_objective_value = 0,
            .deltas =
                {
                    // Empty delta.
                    {.paths = {{0, 2, 3, 4}},
                     .unperformed = {1},
                     .feasible = true,
                     .expected_objective_value = 0},
                    // Feasible delta, 1 active, 2 and 3 inactive.
                    {.paths = {{0, 1, 4}},
                     .unperformed = {2, 3},
                     .feasible = true,
                     .expected_objective_value = 20},
                    // Unfeasible delta, 1 and 3 active.
                    {.paths = {{0, 1, 3, 4}},
                     .unperformed = {2},
                     .feasible = false},
                    // Unfeasible delta, 1, 2 and 3 active.
                    {.paths = {{0, 1, 2, 3, 4}},
                     .unperformed = {},
                     .feasible = false},
                    // One disjunction unperformed, accepted with a cost
                    // increase.
                    {.paths = {{0, 3, 4}},
                     .unperformed = {1, 2},
                     .feasible = true,
                     .expected_objective_value = 20},
                    // All disjunctions unperformed, accepted with a cost
                    // increase.
                    {.paths = {{0, 4}},
                     .unperformed = {1, 2, 3},
                     .feasible = true,
                     .expected_objective_value = 40},
                    // Make disjunction singleton inactive.
                    {.paths = {{0, 2, 4}},
                     .unperformed = {1, 3},
                     .feasible = true,
                     .expected_objective_value = 30},
                    // LNS case, relaxed next variable contains node (the node
                    // can possibly be non-active).
                    {.paths = {},
                     .unperformed = {},
                     .relaxed_node = 1,
                     .relaxed_min = 0,
                     .relaxed_max = 3,
                     .feasible = true,
                     .expected_objective_value = 0},
                    // LNS case, relaxed next variable does not contain node
                    // (the node cannot be non-active).
                    {.paths = {},
                     .unperformed = {},
                     .relaxed_node = 1,
                     .relaxed_min = 2,
                     .relaxed_max = 3,
                     .feasible = false},
                },
        }));

INSTANTIATE_TEST_SUITE_P(
    OneDisjunctionSoftMinHardMaxPenalizeOnce,
    NodeDisjunctionFilterTestWithScenario,
    ::testing::Values(
        NodeDisjunctionFilterScenario{
            .num_nodes = 7,
            .num_vehicles = 1,
            .disjunctions =
                {
                    {.indices = {1, 2, 3, 4, 5, 6},
                     .soft_min_cardinality = 4,
                     .soft_min_penalty = 10,
                     .soft_min_penalty_type = Model::PENALIZE_ONCE,
                     .max_cardinality = 4},
                },
            .initial_paths = {{0, 1, 2, 7}},
            .initial_unperformed = {3, 4, 5, 6},
            .expected_synchronized_objective_value = 10,
            .deltas =
                {
                    // Empty delta.
                    {.paths = {{0, 1, 2, 7}},
                     .unperformed = {3, 4, 5, 6},
                     .feasible = true,
                     .expected_objective_value = 10},
                    // Unfeasible delta, 5 actives.
                    {.paths = {{0, 1, 2, 3, 4, 5, 7}},
                     .unperformed = {6},
                     .feasible = false},
                    // Degraded delta, 1 active.
                    {.paths = {{0, 1, 7}},
                     .unperformed = {2, 3, 4, 5, 6},
                     .feasible = true,
                     .expected_objective_value = 10},
                    // Improved delta, 3 actives.
                    {.paths = {{0, 1, 2, 3, 7}},
                     .unperformed = {4, 5, 6},
                     .feasible = true,
                     .expected_objective_value = 10},
                    // Optimal delta, 4 actives.
                    {.paths = {{0, 1, 2, 3, 4, 7}},
                     .unperformed = {5, 6},
                     .feasible = true,
                     .expected_objective_value = 0},
                },
        }));

INSTANTIATE_TEST_SUITE_P(
    OneDisjunctionSoftMinHardMaxPenalizePerInactive,
    NodeDisjunctionFilterTestWithScenario,
    ::testing::Values(
        NodeDisjunctionFilterScenario{
            .num_nodes = 7,
            .num_vehicles = 1,
            .disjunctions =
                {
                    {.indices = {1, 2, 3, 4, 5, 6},
                     .soft_min_cardinality = 4,
                     .soft_min_penalty = 10,
                     .soft_min_penalty_type = Model::PENALIZE_PER_INACTIVE,
                     .max_cardinality = 4},
                },
            .initial_paths = {{0, 1, 2, 7}},
            .initial_unperformed = {3, 4, 5, 6},
            .expected_synchronized_objective_value = 20,
            .deltas =
                {
                    // Empty delta.
                    {.paths = {{0, 1, 2, 7}},
                     .unperformed = {3, 4, 5, 6},
                     .feasible = true,
                     .expected_objective_value = 20},
                    // Unfeasible delta, 5 actives.
                    {.paths = {{0, 1, 2, 3, 4, 5, 7}},
                     .unperformed = {6},
                     .feasible = false},
                    // Degraded delta, 1 active.
                    {.paths = {{0, 1, 7}},
                     .unperformed = {2, 3, 4, 5, 6},
                     .feasible = true,
                     .expected_objective_value = 30},
                    // Improved delta, 3 actives.
                    {.paths = {{0, 1, 2, 3, 7}},
                     .unperformed = {4, 5, 6},
                     .feasible = true,
                     .expected_objective_value = 10},
                    // Optimal delta, 4 actives.
                    {.paths = {{0, 1, 2, 3, 4, 7}},
                     .unperformed = {5, 6},
                     .feasible = true,
                     .expected_objective_value = 0},
                },
        }));

INSTANTIATE_TEST_SUITE_P(
    SeveralDisjunctionsSoftMinHardMaxUnsyncedNodes,
    NodeDisjunctionFilterTestWithScenario,
    ::testing::Values(
        NodeDisjunctionFilterScenario{
            .num_nodes = 4,
            .num_vehicles = 1,
            .disjunctions =
                {
                    {.indices = {1, 2},
                     .soft_min_cardinality = 1,
                     .soft_min_penalty = 10,
                     .max_cardinality = 1},
                    {.indices = {1, 3},
                     .soft_min_cardinality = 1,
                     .soft_min_penalty = 10,
                     .max_cardinality = 1},
                    {.indices = {1, 2, 3},
                     .soft_min_cardinality = 2,
                     .soft_min_penalty = 10,
                     .max_cardinality = 2},
                    {.indices = {3},
                     .soft_min_cardinality = 1,
                     .soft_min_penalty = 10,
                     .max_cardinality = 1},
                },
            .initial_paths = {{0, 2, 4}},
            .initial_unperformed = {},
            .expected_synchronized_objective_value = 0,
            .deltas =
                {
                    // Empty delta.
                    {.paths = {{0, 2, 4}},
                     .unperformed = {},
                     .feasible = true,
                     .expected_objective_value = 0},
                    // Feasible delta, 1 active, 2 and 3 inactive.
                    {.paths = {{0, 1, 4}},
                     .unperformed = {2, 3},
                     .feasible = true,
                     .expected_objective_value = 20},
                    // Unfeasible delta, 1 and 3 active.
                    {.paths = {{0, 1, 3, 4}},
                     .unperformed = {2},
                     .feasible = false},
                    // Unfeasible delta, 0 and 1 active.
                    {.paths = {{0, 1, 2, 3, 4}},
                     .unperformed = {},
                     .feasible = false},
                    // One disjunction unperformed, accepted with a cost
                    // increase.
                    {.paths = {{0, 3, 4}},
                     .unperformed = {1, 2},
                     .feasible = true,
                     .expected_objective_value = 20},
                    // All disjunctions unperformed, accepted with a cost
                    // increase.
                    {.paths = {{0, 4}},
                     .unperformed = {1, 2, 3},
                     .feasible = true,
                     .expected_objective_value = 40},
                    // Make disjunction singleton inactive.
                    {.paths = {{0, 2, 4}},
                     .unperformed = {1, 3},
                     .feasible = true,
                     .expected_objective_value = 30},
                    // LNS case, relaxed next variable contains node (the node
                    // can possibly be non-active).
                    {.paths = {},
                     .unperformed = {},
                     .relaxed_node = 1,
                     .relaxed_min = 0,
                     .relaxed_max = 3,
                     .feasible = true,
                     .expected_objective_value = 0},
                    // LNS case, relaxed next variable does not contain node
                    // (the node cannot be non-active).
                    {.paths = {},
                     .unperformed = {},
                     .relaxed_node = 1,
                     .relaxed_min = 2,
                     .relaxed_max = 3,
                     .feasible = false},
                },
        }));

// Ensures all combinations of soft costs work correctly.
INSTANTIATE_TEST_SUITE_P(
    SeveralDisjunctionsSoftMinSoftMax, NodeDisjunctionFilterTestWithScenario,
    ::testing::Values(
        NodeDisjunctionFilterScenario{
            .num_nodes = 5,
            .num_vehicles = 1,
            .disjunctions =
                {
                    {.indices = {1, 2, 4},
                     .soft_min_cardinality = 2,
                     .soft_min_penalty = 1,
                     .soft_min_penalty_type = Model::PENALIZE_ONCE},
                    {.indices = {1, 2, 4},
                     .soft_min_cardinality = 2,
                     .soft_min_penalty = 10,
                     .soft_min_penalty_type = Model::PENALIZE_PER_INACTIVE},
                    {.indices = {1, 3, 4},
                     .soft_max_cardinality = 1,
                     .soft_max_penalty = 100,
                     .soft_max_penalty_type = Model::PENALIZE_ONCE},
                    {.indices = {1, 3, 4},
                     .soft_max_cardinality = 1,
                     .soft_max_penalty = 1000,
                     .soft_max_penalty_type = Model::PENALIZE_PER_INACTIVE},
                },
            .initial_paths = {{0, 2, 3, 4, 5}},
            .initial_unperformed = {1},
            .expected_synchronized_objective_value = 0 + 0 + 100 + 1000,
            .deltas =
                {
                    // Empty delta.
                    {.paths = {{0, 2, 3, 4, 5}},
                     .unperformed = {1},
                     .feasible = true,
                     .expected_objective_value = 0 + 0 + 100 + 1000},
                    // Penalties: soft min 2x, soft max 0x
                    {.paths = {{0, 3, 5}},
                     .unperformed = {1, 2, 4},
                     .feasible = true,
                     .expected_objective_value = 1 + 20 + 0 + 0},
                    // Penalties: soft min 1x, soft max 0x
                    {.paths = {{0, 2, 3, 5}},
                     .unperformed = {1, 4},
                     .feasible = true,
                     .expected_objective_value = 1 + 10 + 0 + 0},
                    // Penalties: soft min 0x, soft max 1x
                    {.paths = {{0, 1, 2, 3, 5}},
                     .unperformed = {4},
                     .feasible = true,
                     .expected_objective_value = 0 + 0 + 100 + 1000},
                    // Penalties: soft min 0x, soft max 2x
                    {.paths = {{0, 1, 3, 4, 5}},
                     .unperformed = {2},
                     .feasible = true,
                     .expected_objective_value = 0 + 0 + 100 + 2000},
                    // Penalties: soft min 1x, soft max 1x
                    {.paths = {{0, 3, 4, 5}},
                     .unperformed = {1, 2},
                     .feasible = true,
                     .expected_objective_value = 1 + 10 + 100 + 1000},
                },
        }));

INSTANTIATE_TEST_SUITE_P(
    SeveralDisjunctionsHardMinUnsyncedNodes,
    NodeDisjunctionFilterTestWithScenario,
    ::testing::Values(NodeDisjunctionFilterScenario{
        .num_nodes = 5,
        .num_vehicles = 1,
        .disjunctions =
            {
                {.indices = {1, 2, 3, 4}, .min_cardinality = 2},
            },
        .initial_paths = {{0, 2, 5}},  // initial num actives: [1, 3].
        .initial_unperformed = {3},
        .expected_synchronized_objective_value = 0,
        .deltas =
            {
                {.paths = {{0, 2, 5}},  // Empty delta.
                 .unperformed = {3},
                 .feasible = true},
                {.paths = {{0, 2, 4, 5}},  // num actives: [2, 3].
                 .unperformed = {3},
                 .feasible = true},
                {.paths = {{0, 1, 3, 4, 5}},  // num actives: [3, 3].
                 .unperformed = {2},
                 .feasible = true},
                {.paths = {{0, 2, 5}},  // num actives: [1, 2].
                 .unperformed = {3, 1},
                 .feasible = true},
                {.paths = {{0, 1, 5}},  // num actives: [1, 1].
                 .unperformed = {2, 3, 4},
                 .feasible = false},
            },
    }));

class NodeDisjunctionFilterOnEmptyAssignmentDB : public DecisionBuilder {
 public:
  explicit NodeDisjunctionFilterOnEmptyAssignmentDB(Model* model)
      : model_(model) {}
  Decision* Next(Solver* const solver) override {
    LocalSearchFilter* filter = MakeNodeDisjunctionFilter(*model_, true);
    Assignment empty_solution(solver);
    filter->Synchronize(&empty_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);
    Assignment delta(solver);
    EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    EXPECT_EQ(filter->GetAcceptedObjectiveValue(), 0);
    return nullptr;
  }

 private:
  Model* const model_;
};

TEST(RoutingFilterTest, NodeDisjunctionFilterOnEmptyAssignment) {
  const int kNodes = 4;
  const int64_t kDisjunctionPenalty = 10;
  IndexManager manager(kNodes, 1, NodeIndex(0));
  Model model(manager);
  Solver* solver = model.solver();
  for (NodeIndex node(0); node < kNodes; ++node) {
    model.AddDisjunction(std::vector<int64_t>(1, manager.NodeToIndex(node)),
                         kDisjunctionPenalty);
  }
  model.CloseModel();
  DecisionBuilder* const db =
      solver->RevAlloc(new NodeDisjunctionFilterOnEmptyAssignmentDB(&model));
  EXPECT_EQ(true, solver->Solve(db));
}

TEST_P(RoutingFilterTestWithSolverType, VehicleDependentCapacityFilter) {
  const char* kDimension = "dimension";
  const int64_t kStartEnd[][2] = {{2, 2}, {0, 0}, {3, 3}};
  std::vector<std::pair<NodeIndex, NodeIndex>> start_end(std::size(kStartEnd));
  for (int i = 0; i < std::size(kStartEnd); ++i) {
    start_end[i] =
        std::make_pair(NodeIndex(kStartEnd[i][0]), NodeIndex(kStartEnd[i][1]));
  }
  const int kInitialSolution[] = {6, 5, 1, 4, 7};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta (1 on vehicle 0, 4 on vehicle 2).
      {6, 5, 1, 4, 7},
      // 4 on vehicle 1
      {4, 5, 1, 7, 6},
      // 4 on vehicle 0 - overload
      {6, 4, 1, 7, 5},
      // 1 and 4 on vehicle 1
      {1, 4, 5, 7, 6},
      // 1 and 4 on vehicle 3 - overload
      {6, 4, 5, 1, 7}};
  const bool kExpectFeasibleDelta[] = {true, true, false, true, false};
  const bool kFilterObjectiveCost[] = {true, false};
  const bool kUseGlobalLPFiltering[] = {true, false};

  for (int i = 0; i < std::size(kFilterObjectiveCost); ++i) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    const RoutingSearchParameters::SchedulingSolver solver_type = GetParam();
    parameters.set_continuous_scheduling_solver(solver_type);

    IndexManager manager(5, 3, start_end);
    Model model(manager);
    model.AddDimensionWithVehicleCapacity(
        model.RegisterTransitCallback(
            absl::bind_front(GetFixedDemand, &model, 6)),
        kint64max, GetVehicleCapacities(3, 1),
        /*fix_start_cumul_to_zero=*/true, kDimension);
    model.CloseModelWithParameters(parameters);
    Dimension* const dimension = model.GetMutableDimension(kDimension);

    // The model should not have a GlobalDimensionCumulOptimizer of its own
    // for this dimension.
    EXPECT_FALSE(model.HasGlobalCumulOptimizer(*dimension));
    GlobalDimensionCumulOptimizer optimizer(dimension, solver_type, nullptr);

    for (int lp = 0; lp < std::size(kUseGlobalLPFiltering); lp++) {
      // NOTE: When the objective cost is filtered, the global LP optimizes
      // the cost.
      const bool filter_objective_cost = kFilterObjectiveCost[i];
      LocalSearchFilter* const filter =
          kUseGlobalLPFiltering[lp]
              ? MakeGlobalLPCumulFilter(&optimizer, &optimizer,
                                        filter_objective_cost)
              : MakePathCumulFilter(*dimension, true, filter_objective_cost,
                                    true);

      Solver* const solver = model.solver();
      Assignment initial_solution(solver);
      MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
      filter->Synchronize(&initial_solution, nullptr);
      EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);  // No cost.
      Assignment delta(solver);
      for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
        SCOPED_TRACE(absl::StrCat("Delta=", l, " Callback=", i));
        MakeDelta(kInitialSolution, kDeltaSolutions[l], model.Nexts(), &delta);
        EXPECT_EQ(kExpectFeasibleDelta[l],
                  filter->Accept(&delta, nullptr, kint64min, kint64max));
        if (kExpectFeasibleDelta[l]) {
          EXPECT_EQ(filter->GetAcceptedObjectiveValue(), 0);
        }
      }
    }
  }
}

// Test dimension LP and path filter with disjoint time windows on a model with
// 1 non start/end node and 3 vehicles. The node has two forbidden intervals:
// [10, 20] and [30, 40] (the latter created from 2 overlapping intervals).
// The three vehicles have a horizon of 50 and transitions between nodes is 10.
// The first vehicle has a min start time of 0, so on this vehicle the node
// starts at 21 (10 travel, in first interval, jump to end of interval + 1); the
// solution is valid.
// The second vehicle has a min start time of 5, so on this vehicle the node
// starts at 21 (10 travel, in first interval, jump to end of interval + 1); the
// solution is valid.
// The third vehicle has a min start time of 25, so on this vehicle the node
// starts at 41 (10 travel, in second interval, jump to end of interval + 1);
// the solution is invalid.
TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       NotMemberLPAndPathFilter) {
  const char* kDimension = "dimension";
  const int kInitialSolution[] = {1, 4, 5, 6};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta
      {1, 4, 5, 6},
      // 1 on vehicle 1
      {4, 5, 1, 6},
      // 1 on vehicle 2
      {4, 6, 5, 1}};
  const bool kExpectFeasibleDelta[] = {true, true, false};
  const std::vector<int64_t> starts1 = {10, 30};
  const std::vector<int64_t> ends1 = {20, 32};
  const std::vector<int64_t> starts2 = {31};
  const std::vector<int64_t> ends2 = {40};

  const auto [use_unary_callback, solver_type] = GetParam();
  RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
  parameters.set_continuous_scheduling_solver(solver_type);

  IndexManager manager(2, 3, NodeIndex(0));
  Model model(manager);
  const int callback =
      use_unary_callback
          ? model.RegisterUnaryTransitCallback([](int64_t) { return 10LL; })
          : model.RegisterTransitCallback(
                [](int64_t, int64_t) { return 10LL; });
  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/false, kDimension);
  Dimension* const dimension = model.GetMutableDimension(kDimension);
  Solver* solver = model.solver();
  dimension->CumulVar(1)->SetMax(50);
  solver->AddConstraint(
      solver->MakeNotMemberCt(dimension->CumulVar(1), starts1, ends1));
  dimension->CumulVar(model.Start(0))->SetMin(0);
  solver->AddConstraint(
      solver->MakeNotMemberCt(dimension->CumulVar(1), starts2, ends2));
  dimension->CumulVar(model.Start(1))->SetMin(5);
  dimension->CumulVar(model.Start(2))->SetMin(25);
  dimension->CumulVar(model.End(0))->SetMax(50);
  dimension->CumulVar(model.End(1))->SetMax(50);
  dimension->CumulVar(model.End(2))->SetMax(50);

  model.CloseModelWithParameters(parameters);

  GlobalDimensionCumulOptimizer optimizer(dimension, solver_type, nullptr);

  for (const bool filter_with_lp : {true, false}) {
    LocalSearchFilter* const filter =
        filter_with_lp
            ? MakeGlobalLPCumulFilter(&optimizer, &optimizer,
                                      /*filter_objective_cost=*/false)
            : MakePathCumulFilter(*dimension, true,
                                  /*filter_objective_cost=*/false, true);
    Assignment initial_solution(solver);
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);  // No cost.
    Assignment delta(solver);
    for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
      SCOPED_TRACE(absl::StrCat("Delta=", l));
      MakeDelta(kInitialSolution, kDeltaSolutions[l], model.Nexts(), &delta);
      const bool expect_accepted = kExpectFeasibleDelta[l];
      EXPECT_EQ(expect_accepted,
                filter->Accept(&delta, nullptr, kint64min, kint64max));
      if (expect_accepted) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), 0);  // No cost.
      }
    }
  }
}

// Unit test that verifies the reverse propagation (of max cumul of nodes on the
// route) correctly skips impossible values.
// The example below has 1 non start/end node with 2 disjoint time windows,
// [5, 7] and [17, 30].
// The start node has no time window, and the end node has a time window of
// [15, 20]. Each arc costs 5 in time.
// In the forward pass, the cumul min of nodes are computed as 0 --> 5 --> 15.
// In the backwards pass, the cumul max on the route are 2 --> 7 --> 20, as
// 20-5 = 15 is a forbidden value for Node1.
// The optimal value of cumuls on the route is therefore 2 --> 7 --> 15,
// with a slack of 3.
// In the PathCumulFilter, the backwards propagation returns the minimum max
// start obtained from the min end without forbidden interval jumps (5->10->15)
// and from the max end with jumps (2-->7-->15), leading to the correct minimum
// span of 13 here as well.
TEST_P(RoutingFilterTestWithSolverType, LPAndPathCumulReverseBoundPropagation) {
  const int kNumNodes = 2;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  const char* kDimension = "time";

  const int kInitialSolution[] = {2, 1};  // Route empty.
  const int kDeltaSolution[] = {1, 2};    // Node1 on the route.

  const int64_t kExpectedCost = 13;

  for (int cumul_offset : {0, 10, 1000}) {
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    parameters.set_continuous_scheduling_solver(GetParam());

    Model model(manager);
    model.AddConstantDimensionWithSlack(5, kint64max, kint64max,
                                        /*fix_start_cumul_to_zero=*/false,
                                        kDimension);
    Dimension* const dimension = model.GetMutableDimension(kDimension);
    Solver* solver = model.solver();

    dimension->CumulVar(1)->SetMax(cumul_offset + 30);
    const std::vector<int64_t> starts = {cumul_offset + 0, cumul_offset + 8};
    const std::vector<int64_t> ends = {cumul_offset + 4, cumul_offset + 16};
    solver->AddConstraint(
        solver->MakeNotMemberCt(dimension->CumulVar(1), starts, ends));

    dimension->CumulVar(model.Start(0))->SetMin(cumul_offset);
    dimension->CumulVar(model.End(0))
        ->SetRange(cumul_offset + 15, cumul_offset + 20);
    dimension->SetGlobalSpanCostCoefficient(1);

    model.CloseModelWithParameters(parameters);
    ASSERT_TRUE(model.HasGlobalCumulOptimizer(*dimension));

    for (const bool filter_with_lp : {true, false}) {
      LocalSearchFilter* const filter =
          filter_with_lp
              ? MakeGlobalLPCumulFilter(
                    model.GetMutableGlobalCumulLPOptimizer(*dimension),
                    model.GetMutableGlobalCumulMPOptimizer(*dimension),
                    /*filter_objective_cost=*/true)
              : MakePathCumulFilter(*dimension, true,
                                    /*filter_objective_cost=*/true, true);
      Assignment initial_solution(solver);
      MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
      filter->Synchronize(&initial_solution, nullptr);
      EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), 0);  // No cost.

      Assignment delta(solver);
      MakeDelta(kInitialSolution, kDeltaSolution, model.Nexts(), &delta);
      EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
      EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedCost);
    }
  }
}

// Test dimension path filter having constrained slacks.
// Slacks have two sources in the test:
// - fixed start cumul, nodes with constrained min cumul will create slack,
// - nodes with constrained min slack.
TEST_P(RoutingFilterTestWithUnaryCallbackAndSolverType,
       SlackDimensionPathFilter) {
  const char* kDimension = "dimension";
  const int64_t kTimeWindows[][2] = {{0, 100}, {0, 10}, {0, 11}, {9, 9}};
  const int kNumNodes = std::size(kTimeWindows);
  const int kInitialSolution[] = {2, 6, 5, 1, 3};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta (2 on vehicle 0, 1 & 3 on vehicle 1).
      {2, 6, 5, 1, 3},
      // 3 on vehicle 0, 2 on vehicle 1 (if slack min of start of vehicle 0 is
      // greater than 8, node 3 cannot be reached).
      {3, 2, 6, 5, 1},
      // 1 on vehicle 0, 2 on vehicle 1 (if slack min of start of vehicle 0 is
      // greater than 9, node 1 cannot be reached)
      {1, 5, 6, 2, 3}};
  const bool kExpectFeasibleDelta[] = {true, false, false};
  // Min slack on start of vehicle 0 produces 10 of slack.
  // Min cumul of node 3 creates 8 of slack when node 3 is first node on path
  // after start node since all vehicle start cumuls are fixed to 0,
  const int64_t kExpectedSlackCost[][3] = {{18, -1, -1}, {8, 8, 8}};
  const bool kUseLocalLPFiltering[] = {true, false};
  const bool kUseSlackMin[] = {true, false};

  const auto [use_unary_callback, solver_type] = GetParam();
  for (int i = 0; i < std::size(kUseLocalLPFiltering); ++i) {
    for (int slack = 0; slack < std::size(kUseSlackMin); ++slack) {
      RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
      parameters.set_continuous_scheduling_solver(solver_type);

      IndexManager manager(kNumNodes, 2, NodeIndex(0));
      Model model(manager);
      const int callback =
          use_unary_callback
              ? model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; })
              : model.RegisterTransitCallback(
                    [](int64_t, int64_t) { return 1LL; });
      model.AddDimension(callback, kint64max, kint64max,
                         /*fix_start_cumul_to_zero=*/true, kDimension);
      Dimension* const dimension = model.GetMutableDimension(kDimension);
      dimension->SetSpanCostCoefficientForAllVehicles(1);
      // NOTE(user): The following doesn't affect the cost, but makes sure
      // LP is used in the PathCumulFilter when kUseLocalLPFiltering[i] is
      // true.
      if (kUseLocalLPFiltering[i]) {
        dimension->SetCumulVarSoftLowerBound(model.Start(0), 0, 25);
      }
      if (kUseSlackMin[slack]) {
        dimension->SlackVar(model.Start(0))->SetMin(10);
      }
      for (NodeIndex node(0); node < kNumNodes; ++node) {
        dimension->CumulVar(manager.NodeToIndex(node))
            ->SetRange(kTimeWindows[node.value()][0],
                       kTimeWindows[node.value()][1]);
      }
      model.CloseModelWithParameters(parameters);
      LocalSearchFilter* const filter =
          MakePathCumulFilter(*dimension, true, true, true);
      Solver* const solver = model.solver();
      Assignment initial_solution(solver);
      MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
      filter->Synchronize(&initial_solution, nullptr);
      EXPECT_EQ(filter->GetSynchronizedObjectiveValue(),
                kExpectedSlackCost[slack][0]);  // First delta has no change.
      Assignment delta(solver);
      for (int l = 0; l < std::size(kDeltaSolutions); ++l) {
        SCOPED_TRACE(
            absl::StrCat("Delta=", l, " Slack=", slack, " Callback=", i));
        MakeDelta(kInitialSolution, kDeltaSolutions[l], model.Nexts(), &delta);
        const bool expect_accepted =
            (!kUseSlackMin[slack] || kExpectFeasibleDelta[l]);
        EXPECT_EQ(expect_accepted,
                  filter->Accept(&delta, nullptr, kint64min, kint64max));
        if (expect_accepted) {
          EXPECT_EQ(filter->GetAcceptedObjectiveValue(),
                    kExpectedSlackCost[slack][l]);
        }
      }
    }
  }
}

TEST(RoutingFilterTest, ValidPathInPathCumulFilter) {
  static const int kNodes = 3;
  static const int kVehicles = 1;
  for (bool incremental_sync : {true, false}) {
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    Model model(manager);
    // Disjoint time windows added to trigger cp-sat scheduling.
    const int callback =
        model.RegisterUnaryTransitCallback([](int64_t) { return 1LL; });
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, "dim");
    Dimension* const dimension = model.GetMutableDimension("dim");
    dimension->CumulVar(1)->SetRange(5, 15);
    const std::vector<int64_t> starts = {10};
    const std::vector<int64_t> ends = {12};
    Solver* const solver = model.solver();
    solver->AddConstraint(
        solver->MakeNotMemberCt(dimension->CumulVar(1), starts, ends));
    dimension->SetSpanCostCoefficientForAllVehicles(1);
    dimension->SetSoftSpanUpperBoundForVehicle({1, 10}, 0);
    model.CloseModel();
    LocalSearchFilter* const filter = MakePathCumulFilter(
        *dimension,
        /*propagate_own_objective_value=*/true,
        /*filter_objective_cost=*/true, /*may_use_optimizers=*/true);
    const int kInitialSolution[] = {2, 1, 3};  // 0 - 2 - 3, 1 skipped
    Assignment initial_solution(solver);
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    const int kDelta[] = {1, 2, 3};  // 0 - 1 - 2 - 3
    Assignment delta(solver);
    MakeDelta(kInitialSolution, kDelta, model.Nexts(), &delta);
    filter->Accept(&delta, nullptr, kint64min, kint64max);
    // If delta not cleaned up, a cycle will be created in the path
    const int kFinalSolution[] = {2, 3, 1};  // 0 - 2 - 1 - 3
    Assignment final_solution(solver);
    MakeAssignment(kFinalSolution, model.Nexts(), &final_solution);
    MakeDelta(kInitialSolution, kFinalSolution, model.Nexts(), &delta);
    filter->Synchronize(&final_solution, incremental_sync ? &delta : nullptr);
  }
}

// Test the CPFeasibilityFilter.
// -- When a mandatory node is not served, the delta is accepted iff there are
//    vehicles available (hence initial solution and Delta[1] accepted,
//    Delta[3] rejected).
// -- Delta[2] has a vehicle used over-capacity, delta should not be accepted.
TEST(RoutingFilterTest, CPFeasibilityFilter) {
  static const int kNodes = 4;
  static const int kVehicles = 2;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);

  model.AddConstantDimension(1, 3, /*fix_start_cumul_to_zero*/ true, "load");

  static const int kInitialSolution[] = {5, 1, 2, 3, 6};
  static const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {5, 1, 2, 3, 6},
      // Serve 1 and 2 on first path (v1 full, v2 available and 3 not served).
      {1, 2, 5, 3, 6},
      // Serve 1, 2, and 3 on first path (over capacity).
      {1, 2, 3, 5, 6},
      // Make 1 on first path, 2 on second path (3 not served, no more vehicles
      // available).
      {1, 5, 6, 3, 2}};
  static const bool kExpectedAccept[] = {true, true, false, false};

  model.CloseModel();
  LocalSearchFilter* const filter = MakeCPFeasibilityFilter(&model);
  Solver* const solver = model.solver();
  Assignment initial_solution(solver);
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  Assignment delta(solver);
  for (int i = 0; i < std::size(kDeltaSolutions); ++i) {
    SCOPED_TRACE(absl::StrCat("Delta=", i));
    MakeDelta(kInitialSolution, kDeltaSolutions[i], model.Nexts(), &delta);
    EXPECT_EQ(kExpectedAccept[i],
              filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

// Same as above but with VehicleVars in deltas.
TEST(RoutingFilterTest, CPFeasibilityFilterWithVehicleVars) {
  static const int kNodes = 4;
  static const int kVehicles = 2;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);

  model.AddConstantDimension(1, 3, /*fix_start_cumul_to_zero*/ true, "load");

  static const int kInitialSolution[] = {5, 1, 2, 3, 6, 0, -1, -1, -1, 1};
  static const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change, empty delta.
      {5, 1, 2, 3, 6, 0, -1, -1, -1, 1},
      // Serve 1 and 2 on first path (v1 full, v2 available and 3 not served).
      {1, 2, 5, 3, 6, 0, 0, 0, -1, 1},
      // Serve 1, 2, and 3 on first path (over capacity).
      {1, 2, 3, 5, 6, 0, 0, 0, 0, 1},
      // Make 1 on first path, 2 on second path (3 not served, no more vehicles
      // available).
      {1, 5, 6, 3, 2, 0, 0, 1, -1, 1}};
  static const bool kExpectedAccept[] = {true, true, false, false};

  model.CloseModel();
  LocalSearchFilter* const filter = MakeCPFeasibilityFilter(&model);
  Solver* const solver = model.solver();
  std::vector<IntVar*> vars = model.Nexts();
  for (int i = 0; i < model.Nexts().size(); ++i) {
    vars.push_back(model.VehicleVar(i));
  }
  Assignment initial_solution(solver);
  MakeAssignment(kInitialSolution, vars, &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  Assignment delta(solver);
  for (int i = 0; i < std::size(kDeltaSolutions); ++i) {
    SCOPED_TRACE(absl::StrCat("Delta=", i));
    MakeDelta(kInitialSolution, kDeltaSolutions[i], vars, &delta);
    EXPECT_EQ(kExpectedAccept[i],
              filter->Accept(&delta, nullptr, kint64min, kint64max));
  }
}

TEST(RoutingFilterTest, LightWeightDimensions) {
  for (bool is_unary : {true, false}) {
    RoutingModelParameters model_parameters = DefaultRoutingModelParameters();
    ConstraintSolverParameters* const solver_parameters =
        model_parameters.mutable_solver_parameters();
    solver_parameters->set_profile_local_search(true);
    IndexManager manager(5, 2, NodeIndex(0));
    Model model(manager, model_parameters);
    const int callback =
        is_unary ? model.RegisterUnaryTransitCallback(
                       absl::bind_front(GetUnaryFixedDemand, &model, 100))
                 : model.RegisterTransitCallback(
                       absl::bind_front(GetFixedDemand, &model, 100));
    model.AddDimension(callback, 0, 1000,
                       /*fix_start_cumul_to_zero=*/true, "dim");
    const Assignment* const assignment = model.Solve();
    ASSERT_NE(assignment, nullptr);
    bool filter_found = false;
    const LocalSearchStatistics& ls_stats =
        model.solver()->GetLocalSearchStatistics();
    for (const LocalSearchStatistics::LocalSearchFilterStatistics& stats :
         ls_stats.local_search_filter_statistics()) {
      if (stats.local_search_filter() == "DimensionFilter(dim)") {
        filter_found = true;
      }
    }
    EXPECT_TRUE(filter_found);
  }
}

// The following tests the resource assignment filtering
// (done by ResourceAssignmentFilter and GlobalLPCumulFilter) with a model with:
// - 3 vehicles, where:
//   - The first vehicle is only compatible with the first resource.
//   - The third vehicle is incompatible with all resources.
// - 4 resources, where the third one is unusable by any vehicle because of
//   its time windows, and the fourth isn't allowed for any vehicle.
// - 3 non start-end nodes, where the third node is only compatible with the
//   first resource.
TEST_P(RoutingFilterTestWithUnaryCallback, ResourceAssignmentFilteringTest) {
  const int kNumNodes = 4;
  const int kNumVehicles = 3;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (int64_t resource_fixed_cost : {0, 1000}) {
    Model model(manager);
    const std::string kDimName = "time";

    const int callback = GetParam()
                             ? model.RegisterUnaryTransitCallback(
                                   [](int64_t) { return 10LL; },
                                   Model::kTransitEvaluatorSignPositiveOrZero)
                             : model.RegisterTransitCallback(
                                   [](int64_t, int64_t) { return 10LL; },
                                   Model::kTransitEvaluatorSignPositiveOrZero);
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    // Setup vehicle time windows.
    // The first vehicle must start in [100, 125] so it's only compatible with
    // the first resource. The third vehicle must end in [500, 1000] so it's
    // incompatible with all resources.
    const int64_t kStartTimeWindows[][2] = {{100, 125}, {130, 200}, {150, 500}};
    const int64_t kEndTimeWindows[][2] = {{175, 500}, {200, 300}, {500, 1000}};
    for (int v = 0; v < model.vehicles(); v++) {
      const auto& start_tw = kStartTimeWindows[v];
      dimension->SetCumulVarRange(model.Start(v), start_tw[0], start_tw[1]);
      const auto& end_tw = kEndTimeWindows[v];
      dimension->SetCumulVarRange(model.End(v), end_tw[0], end_tw[1]);
    }

    // Setup resources. The third resource is unusable by all vehicles, and the
    // fourth one not in any of the ResourceVar domains.
    Model::ResourceGroup* const resource_group = model.AddResourceGroup();
    resource_group->AddResource(Attributes(Domain(125, 175), Domain(0, 300),
                                           kint64max, resource_fixed_cost),
                                dimension);
    resource_group->AddResource(Attributes(Domain(150, 200), Domain(250, 499),
                                           kint64max, resource_fixed_cost),
                                dimension);
    resource_group->AddResource(Attributes(Domain(25, 75), Domain(50, 90),
                                           kint64max, resource_fixed_cost),
                                dimension);
    resource_group->AddResource(Attributes(), dimension);
    for (int v = 0; v < model.vehicles(); v++) {
      resource_group->NotifyVehicleRequiresAResource(v);
      model.ResourceVar(v, 0)->SetMax(2);
    }

    // Node3's time window makes it compatible with the first resource only.
    dimension->SetCumulVarRange(3, 0, 145);

    // Setup a span cost coefficient of 1 on all vehicles.
    dimension->SetSpanCostCoefficientForAllVehicles(1);

    model.CloseModel();

    // Initial solution: all unperformed.
    const int kInitialSolution[] = {6, 1, 2, 3, 7, 8};
    const int kDeltaSolutions[][std::size(kInitialSolution)] = {
        // No change.
        {6, 1, 2, 3, 7, 8},
        // V0: NODE0->1->2->3->NODE6
        {1, 2, 3, 6, 7, 8},
        // V0: NODE0->1->3->2->NODE6
        {1, 3, 6, 2, 7, 8},
        // V1: NODE4->1->3->2->NODE7
        {6, 3, 7, 2, 1, 8},
        // V0: NODE0->3->NODE6, V1: NODE4->2->NODE7, V2: NODE5->1->NODE8
        {3, 8, 7, 6, 2, 1},
        // V0: NODE0->1->3->NODE6, V1: NODE4->2->NODE7
        {1, 3, 7, 6, 2, 8},
        // V0: NODE0->1->2->NODE6, V1: NODE4->3->NODE7
        {1, 2, 6, 7, 3, 8},
        // V1: NODE4->3->2->1->NODE7
        {6, 7, 1, 2, 3, 8}};
    const int kActiveVehicles[] = {0, 1, 1, 1, 3, 2, 2, 1};

    const bool kExpectedAccept[] = {true,  false, true,  false,
                                    false, true,  false, true};
    const int64_t kExpectedCost[] = {0,
                                     -1,
                                     (175 - 125) - 40,
                                     -1,
                                     -1,
                                     ((175 - 125) - 30) + ((250 - 200) - 20),
                                     -1,
                                     (200 - 135) - 40};

    GlobalDimensionCumulOptimizer lp_optimizer(
        dimension, RoutingSearchParameters::SCHEDULING_GLOP, nullptr);
    GlobalDimensionCumulOptimizer mp_optimizer(
        dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, nullptr);
    for (bool use_global_lp_filter : {true, false}) {
      for (bool filter_objective_cost : {true, false}) {
        for (bool propagate_objective_value : {true, false}) {
          LocalSearchFilter* const filter =
              use_global_lp_filter
                  ? MakeGlobalLPCumulFilter(&lp_optimizer, &mp_optimizer,
                                            filter_objective_cost)
                  : MakeResourceAssignmentFilter(
                        model.GetMutableLocalCumulLPOptimizer(*dimension),
                        model.GetMutableLocalCumulMPOptimizer(*dimension),
                        propagate_objective_value, filter_objective_cost);
          Assignment initial_solution(model.solver());
          MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
          filter->Synchronize(&initial_solution, nullptr);
          Assignment delta(model.solver());
          int last_synchronized_delta_index = 0;
          for (int d = 0; d < std::size(kDeltaSolutions); d++) {
            MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                      kDeltaSolutions[d], model.Nexts(), &delta);
            const bool expected_accept = kExpectedAccept[d];
            ASSERT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                      expected_accept);
            if (expected_accept) {
              const int64_t expected_cost =
                  filter_objective_cost &&
                          (use_global_lp_filter || propagate_objective_value)
                      ? kExpectedCost[d] +
                            kActiveVehicles[d] * resource_fixed_cost
                      : 0;
              EXPECT_EQ(filter->GetAcceptedObjectiveValue(), expected_cost);

              Assignment current_solution(model.solver());
              MakeAssignment(kDeltaSolutions[d], model.Nexts(),
                             &current_solution);
              filter->Synchronize(&current_solution, &delta);
              EXPECT_EQ(filter->GetSynchronizedObjectiveValue(), expected_cost);
              last_synchronized_delta_index = d;
            }
          }
        }
      }
    }
  }
}

// Tests that the resource assignment filters properly detect infeasibility in
// cases where there are fewer resources than "used" vehicles requiring them.
// In the following, the model has 3 vehicles and 1 resource.
//  - The first vehicle doesn't require a resource and is used when empty,
//  - The second vehicle requires a resource and is used when empty,
//  - The third vehicle requires a resource and is *not* used when empty.
TEST_P(RoutingFilterTestWithUnaryCallback,
       ResourceAssignmentFilteringTest_ThreeVehiclesOneResource) {
  const int kNumNodes = 3;
  const int kNumVehicles = 3;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  for (int64_t resource_fixed_cost : {0, 1000}) {
    Model model(manager);
    const std::string kDimName = "time";

    const int callback = GetParam()
                             ? model.RegisterUnaryTransitCallback(
                                   [](int64_t) { return 10LL; },
                                   Model::kTransitEvaluatorSignPositiveOrZero)
                             : model.RegisterTransitCallback(
                                   [](int64_t, int64_t) { return 10LL; },
                                   Model::kTransitEvaluatorSignPositiveOrZero);
    model.AddDimension(callback, kint64max, kint64max,
                       /*fix_start_cumul_to_zero=*/false, kDimName);
    Dimension* const dimension = model.GetMutableDimension(kDimName);

    // Add a resource.
    Model::ResourceGroup& resource_group = *model.AddResourceGroup();
    resource_group.AddResource(
        Attributes(Domain::AllValues(), Domain::AllValues(), kint64max,
                   resource_fixed_cost),
        dimension);
    // The last two vehicles require the resource, and the first two vehicles
    // are used when empty.
    const bool kRequiresResource[] = {false, true, true};
    const bool kUsedWhenEmpty[] = {true, true, false};
    for (int v = 0; v < model.vehicles(); v++) {
      if (kRequiresResource[v]) {
        resource_group.NotifyVehicleRequiresAResource(v);
      }
      model.SetVehicleUsedWhenEmpty(kUsedWhenEmpty[v], v);
      dimension->SetCumulVarRange(model.Start(v), 0, 25);
      dimension->SetCumulVarRange(model.End(v), 100, 200);
    }
    // Setup a span cost coefficient of 1 on all vehicles.
    dimension->SetSpanCostCoefficientForAllVehicles(1);

    model.CloseModel();

    // Initial solution: all unperformed.
    const int kInitialSolution[] = {5, 1, 2, 6, 7};
    const int kDeltaSolutions[][std::size(kInitialSolution)] = {
        // No change.
        {5, 1, 2, 6, 7},
        // V0: NODE0->1->2->NODE5
        {1, 2, 5, 6, 7},
        // V1: NODE3->1->2->NODE6
        {5, 2, 6, 1, 7},
        // V2: NODE4->1->2->NODE7
        {5, 2, 7, 6, 1},
        // V0: NODE0->1->NODE5, V1: NODE3->2->NODE6
        {1, 5, 6, 2, 7},
        // V1: NODE3->1->NODE6, V2: NODE4->2->NODE7
        {5, 6, 7, 1, 2},
        // V0: NODE0->1->NODE5, V2: NODE4->2->NODE7
        {1, 5, 7, 6, 2}};

    const bool kExpectedAccept[] = {true, true,  true, false,
                                    true, false, false};
    const int64_t kExpectedCost[] = {
        2 * ((100 - 25) - 10) + resource_fixed_cost,
        (75 - 30) + (75 - 10) + resource_fixed_cost,
        (75 - 10) + (75 - 30) + resource_fixed_cost,
        -1,
        2 * (75 - 20) + resource_fixed_cost,
        -1,
        -1};

    Assignment initial_solution(model.solver());
    MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);

    GlobalDimensionCumulOptimizer lp_optimizer(
        dimension, RoutingSearchParameters::SCHEDULING_GLOP, nullptr);
    GlobalDimensionCumulOptimizer mp_optimizer(
        dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, nullptr);
    for (bool use_global_lp_filter : {true, false}) {
      LocalSearchFilter* const filter =
          use_global_lp_filter
              ? MakeGlobalLPCumulFilter(&lp_optimizer, &mp_optimizer,
                                        /*filter_objective_cost=*/true)
              : MakeResourceAssignmentFilter(
                    model.GetMutableLocalCumulLPOptimizer(*dimension),
                    model.GetMutableLocalCumulMPOptimizer(*dimension),
                    /*propagate_own_objective_value=*/true,
                    /*filter_objective_cost=*/true);
      filter->Synchronize(&initial_solution, nullptr);
      Assignment delta(model.solver());
      int last_synchronized_delta_index = 0;
      for (int d = 0; d < std::size(kDeltaSolutions); d++) {
        MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                  kDeltaSolutions[d], model.Nexts(), &delta);
        const bool expected_accept = kExpectedAccept[d];
        ASSERT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                  expected_accept);
        if (expected_accept) {
          EXPECT_EQ(filter->GetAcceptedObjectiveValue(), kExpectedCost[d]);
          Assignment current_solution(model.solver());
          MakeAssignment(kDeltaSolutions[d], model.Nexts(), &current_solution);
          filter->Synchronize(&current_solution, &delta);
          last_synchronized_delta_index = d;
        }
      }
    }
  }
}

using RoutingFilterTestWithFixedCost = ::testing::TestWithParam<bool>;

// Tests that the resource assignment filters properly handle vehicles with a
// resource var bound to a positive value.
TEST_P(RoutingFilterTestWithFixedCost,
       ResourceAssignmentFilteringTest_BoundResourceVarPositive) {
  const int kNumNodes = 3;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  Model model(manager);
  const std::string kDimName = "time";

  const int callback = model.RegisterTransitCallback(
      [](int64_t from, int64_t to) {
        // Transit for start->Node1 is 5.
        if (from == 0 && to == 1) return 5;
        // Transit for start->Node2 is 100.
        if (from == 0 && to == 2) return 100;
        return 0;
      },
      Model::kTransitEvaluatorSignPositiveOrZero);

  model.AddDimension(callback, kint64max, kint64max,
                     /*fix_start_cumul_to_zero=*/false, kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  // Add a resource group.
  const int64_t resource_fixed_cost = GetParam() ? 1000 : 0;
  Model::ResourceGroup& resource_group = *model.AddResourceGroup();
  resource_group.AddResource(
      Attributes(Domain::AllValues(), Domain::AllValues(), kint64max,
                 resource_fixed_cost),
      dimension);
  resource_group.AddResource(
      Attributes(Domain::AllValues(), Domain::AllValues(), kint64max,
                 resource_fixed_cost),
      dimension);

  ASSERT_EQ(model.vehicles(), 1);
  resource_group.NotifyVehicleRequiresAResource(0);
  model.SetVehicleUsedWhenEmpty(false, 0);
  // Vehicle must start before 10 and end before 50.
  dimension->SetCumulVarRange(model.Start(0), 0, 10);
  dimension->SetCumulVarRange(model.End(0), 0, 50);
  // Node1 must be visited after 20.
  dimension->SetCumulVarRange(1, 20, 50);
  // Node2 must be visited after 25.
  dimension->SetCumulVarRange(2, 25, 50);
  // Setup a span cost coefficient of 1 the vehicle.
  dimension->SetSpanCostCoefficientForAllVehicles(1);

  // Vehicle resource var bound to 0.
  model.ResourceVar(0, 0)->SetValue(0);

  model.CloseModel();

  // Initial solution: start->Node1->end.
  const int kInitialSolution[] = {1, 3, 2};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change - Slack of 5
      {1, 3, 2},
      // V0: NODE0->2->NODE3 - Infeasible because of the start->Node2 transit.
      {2, 1, 3},
      // V0: NODE0->1->2->NODE3 - Total slack of 10.
      {1, 2, 3},
      // Route empty - Infeasible because of bound resource var.
      {3, 1, 2}};

  const bool kExpectedAccept[] = {true, false, true, false};
  const int64_t kExpectedCost[] = {5, -1, 10, -1};

  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);

  GlobalDimensionCumulOptimizer lp_optimizer(
      dimension, RoutingSearchParameters::SCHEDULING_GLOP, nullptr);
  GlobalDimensionCumulOptimizer mp_optimizer(
      dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, nullptr);
  for (bool use_global_lp_filter : {true, false}) {
    LocalSearchFilter* const filter =
        use_global_lp_filter
            ? MakeGlobalLPCumulFilter(&lp_optimizer, &mp_optimizer,
                                      /*filter_objective_cost=*/true)
            : MakeResourceAssignmentFilter(
                  model.GetMutableLocalCumulLPOptimizer(*dimension),
                  model.GetMutableLocalCumulMPOptimizer(*dimension),
                  /*propagate_own_objective_value=*/true,
                  /*filter_objective_cost=*/true);
    filter->Synchronize(&initial_solution, nullptr);
    Assignment delta(model.solver());
    int last_synchronized_delta_index = 0;
    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      MakeDelta(kDeltaSolutions[last_synchronized_delta_index],
                kDeltaSolutions[d], model.Nexts(), &delta);
      const bool expected_accept = kExpectedAccept[d];
      ASSERT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                expected_accept);
      if (expected_accept) {
        const int64_t expected_cost = kExpectedCost[d] + resource_fixed_cost;
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), expected_cost);
        EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min,
                                    /*objective_max=*/expected_cost - 1));
        Assignment current_solution(model.solver());
        MakeAssignment(kDeltaSolutions[d], model.Nexts(), &current_solution);
        filter->Synchronize(&current_solution, &delta);
        last_synchronized_delta_index = d;
      }
    }
  }
}

// Same as above but with a negative bound resource var.
TEST_P(RoutingFilterTestWithFixedCost,
       ResourceAssignmentFilteringTest_BoundResourceVarNegative) {
  const int kNumNodes = 3;
  const int kNumVehicles = 1;
  const IndexManager manager(kNumNodes, kNumVehicles, NodeIndex(0));

  Model model(manager);
  const std::string kDimName = "time";

  model.AddConstantDimension(10, kint64max, false, kDimName);
  Dimension* const dimension = model.GetMutableDimension(kDimName);

  // Add a resource group.
  const int64_t resource_fixed_cost = GetParam() ? 1000 : 0;
  Model::ResourceGroup& resource_group = *model.AddResourceGroup();
  resource_group.AddResource(
      Attributes(Domain::AllValues(), Domain::AllValues(), kint64max,
                 resource_fixed_cost),
      dimension);

  ASSERT_EQ(model.vehicles(), 1);
  resource_group.NotifyVehicleRequiresAResource(0);
  model.SetVehicleUsedWhenEmpty(false, 0);

  // Vehicle resource var bound to -1.
  model.ResourceVar(0, 0)->SetValue(-1);

  model.CloseModel();

  // Initial solution: vehicle unused.
  const int kInitialSolution[] = {3, 1, 2};
  const int kDeltaSolutions[][std::size(kInitialSolution)] = {
      // No change - Slack of 5
      {3, 1, 2},
      // V0: NODE0->1->NODE3.
      {1, 3, 2},
      // V0: NODE0->2->NODE3.
      {2, 1, 3},
      // V0: NODE0->1->2->NODE3.
      {1, 2, 3},
      // V0: NODE0->2->1->NODE3.
      {2, 3, 1}};

  const bool kExpectedAccept[] = {true, false, false, false, false};

  Assignment initial_solution(model.solver());
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);

  GlobalDimensionCumulOptimizer lp_optimizer(
      dimension, RoutingSearchParameters::SCHEDULING_GLOP, nullptr);
  GlobalDimensionCumulOptimizer mp_optimizer(
      dimension, RoutingSearchParameters::SCHEDULING_CP_SAT, nullptr);
  for (bool use_global_lp_filter : {true, false}) {
    LocalSearchFilter* const filter =
        use_global_lp_filter
            ? MakeGlobalLPCumulFilter(&lp_optimizer, &mp_optimizer,
                                      /*filter_objective_cost=*/true)
            : MakeResourceAssignmentFilter(
                  model.GetMutableLocalCumulLPOptimizer(*dimension),
                  model.GetMutableLocalCumulMPOptimizer(*dimension),
                  /*propagate_own_objective_value=*/true,
                  /*filter_objective_cost=*/true);
    filter->Synchronize(&initial_solution, nullptr);
    Assignment delta(model.solver());
    for (int d = 0; d < std::size(kDeltaSolutions); d++) {
      MakeDelta(kInitialSolution, kDeltaSolutions[d], model.Nexts(), &delta);
      const bool expected_accept = kExpectedAccept[d];
      ASSERT_EQ(filter->Accept(&delta, nullptr, kint64min, kint64max),
                expected_accept);
      if (expected_accept) {
        EXPECT_EQ(filter->GetAcceptedObjectiveValue(), 0);
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(RoutingFilterTestWithFixedCost,
                         RoutingFilterTestWithFixedCost, ::testing::Bool());

// Tests that the LP encoding of vehicle breaks detects enough infeasibilities.
class BreakInLPTest : public ::testing::TestWithParam<std::tuple<int, bool>> {
  void SetUp() override {
    // Make model.
    constexpr int kNodes = 3;
    constexpr int kVehicles = 1;
    IndexManager manager(kNodes, kVehicles, NodeIndex(0));
    model_ = std::make_unique<Model>(manager);
    // Add dimension.
    bool use_unary_callback = ::testing::get<1>(GetParam());
    const int callback = use_unary_callback
                             ? model_->RegisterUnaryTransitCallback(
                                   [](int64_t) { return 3; },
                                   Model::kTransitEvaluatorSignPositiveOrZero)
                             : model_->RegisterTransitCallback(
                                   [](int64_t, int64_t) { return 3; },
                                   Model::kTransitEvaluatorSignPositiveOrZero);
    model_->AddDimension(callback,
                         /*slack_max=*/kint64max,
                         /*capacity=*/10000,
                         /*fix_start_cumul_to_zero=*/false, "time");

    // Add time cost and breaks. All time values are > 0 to exercise LP offset.
    Dimension* time = model_->GetMutableDimension("time");
    time->SetSpanCostCoefficientForVehicle(1, 0);
    time->CumulVar(0)->SetMin(10);
    time->CumulVar(1)->SetRange(23, 23);
    time->CumulVar(2)->SetRange(28, 28);
    time->CumulVar(3)->SetMin(10);
    model_->AddDisjunction({1}, 0, 1);
    model_->AddDisjunction({2}, 0, 1);

    const BreakScenario& scenario = scenarios_[::testing::get<0>(GetParam())];
    IntervalVar* br = model_->solver()->MakeIntervalVar(
        scenario.start_min, scenario.start_max, scenario.duration_min,
        scenario.duration_max, scenario.end_min, scenario.end_max,
        scenario.optional, "");
    const int pre_travel = model_->RegisterTransitCallback(
        [](int64_t, int64_t) -> int64_t { return 1; });
    const int post_travel = model_->RegisterTransitCallback(
        [](int64_t, int64_t) -> int64_t { return 1; });
    time->SetBreakIntervalsOfVehicle({br}, 0, pre_travel, post_travel);
    model_->CloseModel();
  }

 protected:
  std::unique_ptr<Model> model_;
  struct BreakScenario {
    int64_t start_min;
    int64_t start_max;
    int64_t duration_min;
    int64_t duration_max;
    int64_t end_min;
    int64_t end_max;
    int64_t expected_cost;
    bool optional;
  };
  // The test will set route = start 1 2 end.
  // Without breaks, the optimal schedule would be times 20, 23, 28 and 31.
  // Note that the 1 -> 2 step has slack 2.
  // With the duration 2 break, the optimal path is still the same,
  // in cases where the break can be done during the 1 -> 2 slack.
  // We test various cases where the break cannot be done there:
  // when the break must be before, after, or when it is too large.
  // The transit of 3 units between node 1 and node 2 is composed of:
  // - 1 unit of pre-travel, in [23, 24)
  // - 1 unit of travel, starting at earliest at 24 and ending at most at 27.
  // - 1 unit of post-travel, in [27, 28).
  // To be done between nodes 1 and 2, the break has to:
  // - Have a duration <= 2.
  // - Start at 24 or 25.
  const std::vector<BreakScenario> scenarios_ = {
      // break can be done outside the route.
      {10, 30, 2, 4, 10, 30, 3 + 5 + 3, false},
      // break must be done inside the route.
      {20, 29, 2, 4, 22, 31, 3 + 5 + 3, false},
      // break must start strictly before 24.
      {20, 23, 2, 4, 22, 31, 2 + 3 + 5 + 3, false},
      // break can be skipped.
      {20, 23, 2, 4, 22, 31, 3 + 5 + 3, true},
      // break can be done during [24, 26).
      {20, 24, 2, 4, 22, 31, 3 + 5 + 3, false},
      // break must be done strictly after 26.
      {26, 29, 2, 4, 22, 31, 3 + 5 + 3 + 2, false},
      // break can be skipped.
      {26, 29, 2, 4, 22, 31, 3 + 5 + 3, true},
      // break can be done during [25, 27).
      {25, 29, 2, 4, 22, 31, 3 + 5 + 3, false},
      // break is too long to fit in slack.
      {19, 29, 3, 4, 22, 31, 3 + 3 + 5 + 3, false},
      // break can be skipped.
      {20, 29, 3, 4, 22, 31, 3 + 5 + 3, true}};
};

TEST_P(BreakInLPTest, PathCumulFilterComputesCorrectCost) {
  // The LP model that includes breaks is called through PathCumulFilter.
  Dimension* time = model_->GetMutableDimension("time");
  LocalSearchFilter* const filter =
      MakePathCumulFilter(*time, true, true, true);
  // Initial solution: start -> end.
  Solver* const solver = model_->solver();
  Assignment initial_solution(solver);
  const int kInitialSolution[] = {3, 1, 2};
  MakeAssignment(kInitialSolution, model_->Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  // Delta solution: start -> 1 -> 2 -> end.
  Assignment delta(solver);
  const int kDeltaSolution[] = {1, 2, 3};
  MakeDelta(kInitialSolution, kDeltaSolution, model_->Nexts(), &delta);
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  const BreakScenario& scenario = scenarios_[::testing::get<0>(GetParam())];
  constexpr int64_t total_transit = 9;
  EXPECT_EQ(scenario.expected_cost - total_transit,
            filter->GetAcceptedObjectiveValue());
  filter->Synchronize(&delta, nullptr);
  EXPECT_EQ(scenario.expected_cost - total_transit,
            filter->GetSynchronizedObjectiveValue());
}

INSTANTIATE_TEST_SUITE_P(BreaksInLPScenarios, BreakInLPTest,
                         ::testing::Combine(::testing::Range(0, 10),
                                            ::testing::Bool()));

std::unique_ptr<Model> CreateModelWithInterbreakLimit(
    const std::string& dim_name, const RoutingSearchParameters& params,
    const int64_t pre_travel_time = 0, const int64_t post_travel_time = 0) {
  // Create a model with two visits: start - node1 - node2 - end, three breaks,
  // and 2 interbreak limits.
  // The setup is such that an optimal solution for the cumuls of those nodes
  // is: 1, 11, 19, 29, with the breaks scheduled during
  // [6, 10), [14, 16), and [20, 24).
  // Interbreak limit/duration are: 5/2 and 10/4.
  constexpr int kNodes = 3;
  constexpr int kVehicles = 1;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  auto model = std::make_unique<Model>(manager);
  // Add dimension.
  const int callback = model->RegisterTransitCallback(
      [](int64_t, int64_t) { return 6; },
      Model::kTransitEvaluatorSignPositiveOrZero);
  model->AddDimension(callback,
                      /*slack_max=*/kint64max,
                      /*capacity=*/100,
                      /*fix_start_cumul_to_zero=*/false, dim_name);

  // Add time cost, make visits non-mandatory.
  Dimension* time = model->GetMutableDimension(dim_name);
  time->SetSpanCostCoefficientForVehicle(1, 0);
  model->AddDisjunction({1}, 100, 1);
  model->AddDisjunction({2}, 100, 1);

  // Setup breaks and interbreaks.
  std::vector<IntervalVar*> vehicle_breaks;
  for (int br = 0; br < 3; ++br) {
    vehicle_breaks.push_back(
        model->solver()->MakeIntervalVar(0, 100, 0, 100, 0, 100, false, ""));
  }
  const int pre_travel = model->RegisterTransitCallback(
      [pre_travel_time](int64_t, int64_t) -> int64_t {
        return pre_travel_time;
      });
  const int post_travel = model->RegisterTransitCallback(
      [post_travel_time](int64_t, int64_t) -> int64_t {
        return post_travel_time;
      });
  time->SetBreakIntervalsOfVehicle(vehicle_breaks, 0, pre_travel, post_travel);
  time->SetBreakDistanceDurationOfVehicle(5, 2, 0);
  time->SetBreakDistanceDurationOfVehicle(10, 4, 0);
  model->CloseModelWithParameters(params);

  return model;
}

TEST(InterbreakInLPTest, GeneralInterbreakBehaviour) {
  const std::string kDimName = "time";
  std::unique_ptr<Model> model = CreateModelWithInterbreakLimit(
      kDimName, DefaultRoutingSearchParameters());
  Dimension* const time = model->GetMutableDimension(kDimName);

  LocalSearchFilter* const filter =
      MakePathCumulFilter(*time, true, true, true);
  // Initial solution: start -> end.
  Solver* const solver = model->solver();
  Assignment initial_solution(solver);
  const int kInitialSolution[] = {3, 1, 2};
  MakeAssignment(kInitialSolution, model->Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  // Delta solution: start -> 1 -> 2 -> end.
  Assignment delta(solver);
  const int kDeltaSolution[] = {1, 2, 3};
  MakeDelta(kInitialSolution, kDeltaSolution, model->Nexts(), &delta);
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  constexpr int64_t total_transit = 18;
  constexpr int64_t expected_span = 28;
  EXPECT_EQ(expected_span - total_transit, filter->GetAcceptedObjectiveValue());
  filter->Synchronize(&delta, nullptr);
  EXPECT_EQ(expected_span - total_transit,
            filter->GetSynchronizedObjectiveValue());
}

TEST(InterbreakInLPTest, InterbreakInfeasibleBecauseOfPretravel) {
  const std::string kDimName = "time";
  struct PrePosTravelTime {
    int64_t pre;
    int64_t post;
  };
  std::vector<PrePosTravelTime> travel_times = {{0, 6}, {6, 0}, {3, 3}};
  for (const auto& travel_time : travel_times) {
    std::unique_ptr<Model> model = CreateModelWithInterbreakLimit(
        kDimName, DefaultRoutingSearchParameters(), travel_time.pre,
        travel_time.post);
    Dimension* const time = model->GetMutableDimension(kDimName);

    LocalSearchFilter* const filter =
        MakePathCumulFilter(*time, true, true, true);
    // Initial solution: start -> end.
    Solver* const solver = model->solver();
    Assignment initial_solution(solver);
    const int kInitialSolution[] = {3, 1, 2};
    MakeAssignment(kInitialSolution, model->Nexts(), &initial_solution);
    filter->Synchronize(&initial_solution, nullptr);
    // Delta solution: start -> 1 -> end.
    Assignment delta(solver);
    const std::vector<int> solution1 = {1, 3, 2};
    MakeDelta(kInitialSolution, solution1.data(), model->Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    filter->Revert();
    // Delta solution: start -> 2 -> end.
    const std::vector<int> solution2 = {2, 3, 1};
    MakeDelta(kInitialSolution, solution2.data(), model->Nexts(), &delta);
    EXPECT_FALSE(filter->Accept(&delta, nullptr, kint64min, kint64max));
    filter->Revert();
  }
}

// End-to-end version of the test above, to verify that the cumul and break
// values are correctly propagated to the solution by the LP-based finalizer.
TEST(InterbreakInLPTest, GeneralInterbreakBehaviourEndToEnd) {
  if (kAsanEnabled || kHwAsanEnabled)
    GTEST_SKIP() << "This test times out in ASAN mode";

  const std::string kDimName = "time";
  for (const auto solver : {RoutingSearchParameters::SCHEDULING_CP_SAT,
                            RoutingSearchParameters::SCHEDULING_GLOP}) {
    if (kAnyXsanEnabled && solver == RoutingSearchParameters::SCHEDULING_GLOP) {
      // NOTE(user): This test times out in xsan modes with the GLOP solver.
      continue;
    }
    for (const bool disable_lp_scheduling : {true, false}) {
      RoutingSearchParameters params = DefaultRoutingSearchParameters();
      params.set_mixed_integer_scheduling_solver(solver);
      params.set_disable_scheduling_beware_this_may_degrade_performance(
          disable_lp_scheduling);
      if (disable_lp_scheduling ||
          solver == RoutingSearchParameters::SCHEDULING_GLOP) {
        // NOTE: Without LP scheduling, the GlobalVehicleBreaksConstraint runs
        // for a long time to get to a solution, so we set a time limit and
        // don't expect the solver to come up with a solution.
        params.mutable_time_limit()->set_seconds(5);
      }
      std::unique_ptr<Model> model =
          CreateModelWithInterbreakLimit(kDimName, params);

      const Assignment* assignment = model->SolveWithParameters(params);
      if (!disable_lp_scheduling) {
        ASSERT_NE(assignment, nullptr);
      }
    }
  }
}

TEST(InterbreakInLPTest, InterbreakWithNoBreaks) {
  // This instance has no breaks, but two interbreaks.
  // In this case, interbreak constraints are the same as span constraints.
  constexpr int kNodes = 3;
  constexpr int kVehicles = 1;
  IndexManager manager(kNodes, kVehicles, NodeIndex(0));
  Model model(manager);
  // Add dimension.
  const int callback =
      model.RegisterTransitCallback([](int64_t, int64_t) { return 1; },
                                    Model::kTransitEvaluatorSignPositiveOrZero);
  model.AddDimension(callback,
                     /*slack_max=*/kint64max,
                     /*capacity=*/1000,
                     /*fix_start_cumul_to_zero=*/false, "time");

  // Add lateness cost to visit 1 and earliness cost to visit 2, to encourage
  // the solver to extend the route duration. The interbreak limit forces the
  // solution to have some lateness or earliness.
  Dimension* time = model.GetMutableDimension("time");
  time->SetCumulVarSoftUpperBound(1, 5, 1);
  time->SetCumulVarSoftLowerBound(2, 20, 2);
  // Make visits non-mandatory.
  model.AddDisjunction({1}, 0, 1);
  model.AddDisjunction({2}, 0, 1);

  // Setup breaks and interbreaks.
  time->SetBreakDistanceDurationOfVehicle(25, 10, 0);
  time->SetBreakDistanceDurationOfVehicle(10, 4, 0);
  model.CloseModel();

  LocalSearchFilter* const filter =
      MakePathCumulFilter(*time, true, true, true);
  // Initial solution: start -> end.
  Solver* const solver = model.solver();
  Assignment initial_solution(solver);
  const int kInitialSolution[] = {3, 1, 2};
  MakeAssignment(kInitialSolution, model.Nexts(), &initial_solution);
  filter->Synchronize(&initial_solution, nullptr);
  // Delta solution: start -> 1 -> 2 -> end.
  Assignment delta(solver);
  const int kDeltaSolution[] = {1, 2, 3};
  // The expected optimal solution is start(11) node1(12) node2(20) end(21),
  // which reduces the higher lateness cost to 0 and keep the span at 10.
  // The cost of that solution is (12 - 5) * 1 = 7.
  MakeDelta(kInitialSolution, kDeltaSolution, model.Nexts(), &delta);
  EXPECT_TRUE(filter->Accept(&delta, nullptr, kint64min, kint64max));
  EXPECT_EQ(7, filter->GetAcceptedObjectiveValue());
  filter->Synchronize(&delta, nullptr);
  EXPECT_EQ(7, filter->GetSynchronizedObjectiveValue());
}

}  // namespace
}  // namespace operations_research::routing
