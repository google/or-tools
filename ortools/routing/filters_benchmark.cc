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

#include <cstdint>
#include <functional>
#include <memory>
#include <numeric>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/routing/filters.h"

namespace operations_research::routing {
namespace {

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
    CHECK(!node_is_in_paths[loop]) << "Loop " << loop << " is in paths.";
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
    CHECK(!is_new_loop[node] || node_is_in_new_loops[node]);
  }
  return delta;
}

std::vector<int> PathStateExtractPath(const PathState* path_state, int path) {
  std::vector<int> nodes;
  for (const auto chain : path_state->Chains(path)) {
    for (const int node : chain) {
      nodes.push_back(node);
    }
  }
  return nodes;
}

using IntervalTransit =
    std::function<DimensionChecker::Interval(int64_t, int64_t)>;

IntervalTransit ConstantIntervalTransit(int64_t value) {
  return [value](int64_t, int64_t) -> DimensionChecker::Interval {
    return {value, value};
  };
}

// Micro-benchmarks for PathState.
enum class ChangeSparsity { kNone, kSparse, kDense };

// Exchanges the inside nodes of path0 and path1.
void ExchangePaths(PathState* path_state, int path0, int path1) {
  const auto [begin0_index, end0_index] = path_state->CommittedPathRange(path0);
  const auto [begin1_index, end1_index] = path_state->CommittedPathRange(path1);
  path_state->ChangePath(path0, {{begin0_index, begin0_index + 1},
                                 {begin1_index + 1, end1_index - 1},
                                 {end0_index - 1, end0_index}});
  path_state->ChangePath(path1, {{begin1_index, begin1_index + 1},
                                 {begin0_index + 1, end0_index - 1},
                                 {end1_index - 1, end1_index}});
}

// Reverses internal nodes of a path:
// from start -> first -> first + 1 -> ... -> last -> end
// to start -> last - 1 -> last - 2 ... first + 1 -> first -> last.
void ReversePath(PathState* path_state, int path,
                 std::vector<PathState::ChainBounds>& tmp) {
  const auto [begin_index, end_index] = path_state->CommittedPathRange(path);

  tmp.clear();
  tmp.emplace_back(begin_index, begin_index + 1);
  for (int index = end_index - 2; index > begin_index; --index) {
    tmp.emplace_back(index, index + 1);
  }
  tmp.emplace_back(end_index - 1, end_index);

  path_state->ChangePath(path, tmp);
}

constexpr int min_nodes = 8;  // Must be >= 8 or the moves will not yield paths.
constexpr int max_nodes = 4096;

// Benchmark browsing changed paths with Chains() range, Nodes() range,
// Chains() range + Chain range.
enum class BenchmarkBrowsingMode { kChains, kNodes, kChainNodes };

template <BenchmarkBrowsingMode browsing_mode, ChangeSparsity sparsity>
void BM_PathState_Browsing_Changes(benchmark::State& bench_state) {
  const int num_nodes = bench_state.range(0);
  CHECK_GE(num_nodes, 8);
  const int start1 = num_nodes / 2;
  // Initial state: 0 -> ... -> start1-1, start1 -> ... -> num_nodes-1.
  PathState path_state(num_nodes, {0, start1}, {start1 - 1, num_nodes - 1});
  std::vector<int> path0(start1, 0);
  std::vector<int> path1(num_nodes - start1, 0);
  absl::c_iota(path0, 0);
  absl::c_iota(path1, start1);
  PathStateChangePaths(&path_state, {path0, path1}, {});
  path_state.Commit();
  std::vector<PathState::ChainBounds> tmp;
  switch (sparsity) {
    case ChangeSparsity::kNone:
      break;
    case ChangeSparsity::kSparse:
      ExchangePaths(&path_state, 0, 1);
      break;
    case ChangeSparsity::kDense:
      ReversePath(&path_state, 0, tmp);
      ReversePath(&path_state, 1, tmp);
      break;
    default:
      LOG(FATAL) << "Unknown ChangeSparsity enumerator.";
  }
  // Benchmark browsing. Prevent the compiler from optimizing some loops away
  // by making some useless computation at each node/chain.
  int useless_computation = 0;
  int64_t num_items = 0;
  switch (browsing_mode) {
    case BenchmarkBrowsingMode::kChains:
      for (auto _ : bench_state) {
        for (const auto chain : path_state.Chains(0)) {
          useless_computation ^= chain.NumNodes();
          ++num_items;
        }
        for (const auto chain : path_state.Chains(1)) {
          useless_computation ^= chain.NumNodes();
          ++num_items;
        }
      }
      break;
    case BenchmarkBrowsingMode::kNodes:
      for (auto _ : bench_state) {
        for (const int node : path_state.Nodes(0)) {
          useless_computation ^= node;
          ++num_items;
        }
        for (const int node : path_state.Nodes(1)) {
          useless_computation ^= node;
          ++num_items;
        }
      }
      break;
    case BenchmarkBrowsingMode::kChainNodes:
      for (auto _ : bench_state) {
        for (const int path : {0, 1}) {
          for (const auto chain : path_state.Chains(path)) {
            for (const int node : chain) {
              useless_computation ^= node;
              ++num_items;
            }
          }
        }
      }
      break;
    default:
      LOG(FATAL) << "Unknown BenchmarkBrowsingMode enumerator.";
  }
  if (useless_computation == 0) ++num_items;
  bench_state.SetItemsProcessed(num_items);
}

BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kChains,
                                        ChangeSparsity::kNone>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kChains,
                                        ChangeSparsity::kSparse>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kChains,
                                        ChangeSparsity::kDense>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kNodes,
                                        ChangeSparsity::kNone>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kNodes,
                                        ChangeSparsity::kSparse>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kNodes,
                                        ChangeSparsity::kDense>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kChainNodes,
                                        ChangeSparsity::kNone>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kChainNodes,
                                        ChangeSparsity::kSparse>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Browsing_Changes<BenchmarkBrowsingMode::kChainNodes,
                                        ChangeSparsity::kDense>)
    ->Range(min_nodes, max_nodes);

// Benchmark ChangeNext()/CutChains()/Revert() in a loop.
template <ChangeSparsity sparsity>
void BM_PathState_Change_Revert(benchmark::State& bench_state) {
  const int num_nodes = bench_state.range(0);
  CHECK_GE(num_nodes, 8);
  const int start1 = num_nodes / 2;
  // Initial state: 0 -> ... -> start1-1, start1 -> ... -> num_nodes-1.
  PathState path_state(num_nodes, {0, start1}, {start1 - 1, num_nodes - 1});
  std::vector<int> path0(start1, 0);
  std::vector<int> path1(num_nodes - start1, 0);
  absl::c_iota(path0, 0);
  absl::c_iota(path1, start1);
  PathStateChangePaths(&path_state, {path0, path1}, {});
  path_state.Commit();

  std::vector<PathState::ChainBounds> tmp;
  int num_moves = 0;
  for (auto _ : bench_state) {
    switch (sparsity) {
      case ChangeSparsity::kNone:
        break;
      case ChangeSparsity::kSparse:
        ExchangePaths(&path_state, 0, 1);
        break;
      case ChangeSparsity::kDense:
        ReversePath(&path_state, 0, tmp);
        ReversePath(&path_state, 1, tmp);
        break;
      default:
        LOG(FATAL) << "Unknown ChangeSparsity enumerator: "
                   << static_cast<int>(sparsity);
    }
    // Revert().
    path_state.Revert();
    ++num_moves;
  }
  bench_state.SetItemsProcessed(num_moves);
}

BENCHMARK(BM_PathState_Change_Revert<ChangeSparsity::kSparse>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Change_Revert<ChangeSparsity::kDense>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathState_Change_Revert<ChangeSparsity::kNone>)
    ->Range(min_nodes, max_nodes);

// Benchmark ChangePath()/Commit() in a loop, long path scenario.
template <ChangeSparsity sparsity>
void BM_PathStateChangeCommitLongPaths(benchmark::State& bench_state) {
  const int num_nodes = bench_state.range(0);
  CHECK_GE(num_nodes, 8);
  // Initial state: start0 = 0 -> ... -> num_nodes / 2 - 1 = end0,
  // start1 = num_nodes / 2 -> ... -> end1 = num_nodes - 1.
  const int start0 = 0;
  const int end0 = num_nodes / 2 - 1;
  const int start1 = num_nodes / 2;
  const int end1 = num_nodes - 1;
  const std::vector<int> path_start = {start0, start1};
  const std::vector<int> path_end = {end0, end1};
  std::vector<std::vector<int>> initial_paths;
  initial_paths.resize(2);
  for (int i = 0; i < num_nodes / 2; ++i) {
    initial_paths[0].push_back(i);
    initial_paths[1].push_back(start1 + i);
  }
  PathState path_state(num_nodes, path_start, path_end);
  PathStateChangePaths(&path_state, initial_paths, {});
  path_state.Commit();

  std::vector<PathState::ChainBounds> tmp;
  int num_moves = 0;
  for (auto _ : bench_state) {
    switch (sparsity) {
      case ChangeSparsity::kNone:
        break;
      case ChangeSparsity::kSparse:
        ExchangePaths(&path_state, 0, 1);
        break;
      case ChangeSparsity::kDense:
        ReversePath(&path_state, 0, tmp);
        ReversePath(&path_state, 1, tmp);
        break;
      default:
        LOG(FATAL) << "Unknown ChangeSparsity enumerator: "
                   << static_cast<int>(sparsity);
    }
    path_state.Commit();
    ++num_moves;
  }
  bench_state.SetItemsProcessed(num_moves);
}

BENCHMARK(BM_PathStateChangeCommitLongPaths<ChangeSparsity::kSparse>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathStateChangeCommitLongPaths<ChangeSparsity::kDense>)
    ->Range(min_nodes, max_nodes);
// The long path's ChangeSparsity::kNone is redundant with
// the kNone the short path scenario below.

// Benchmark ChangeNext()/CutChains()/Commit() in a loop, short paths scenario.
template <ChangeSparsity sparsity>
void BM_PathStateChangeCommitShortPaths(benchmark::State& bench_state) {
  const int num_nodes = bench_state.range(0);
  CHECK_GE(num_nodes, 8);
  // Initial state: 4k->4k+1->4k+2->4k+3 for k in [0, num_nodes/4).
  std::vector<int> path_start;
  std::vector<int> path_end;
  const int num_paths = num_nodes / 4;
  for (int path = 0; path < num_paths; ++path) {
    path_start.push_back(4 * path);
    path_end.push_back(4 * path + 3);
  }
  std::vector<std::vector<int>> initial_paths(num_paths);
  for (int path = 0; path < num_paths; ++path) {
    for (int i = 0; i < 4; ++i) {
      initial_paths[path].push_back(4 * path + i);
    }
  }
  PathState path_state(num_nodes, path_start, path_end);
  PathStateChangePaths(&path_state, initial_paths, {});
  path_state.Commit();

  std::vector<PathState::ChainBounds> tmp;
  int num_moves = 0;
  for (auto _ : bench_state) {
    switch (sparsity) {
      case ChangeSparsity::kNone:
        break;
      case ChangeSparsity::kSparse:
        ExchangePaths(&path_state, 0, 1);
        break;
      case ChangeSparsity::kDense:
        ReversePath(&path_state, 0, tmp);
        ReversePath(&path_state, 1, tmp);
        break;
      default:
        LOG(FATAL) << "Unknown ChangeSparsity enumerator: "
                   << static_cast<int>(sparsity);
    }
    path_state.Commit();
    ++num_moves;
  }
  bench_state.SetItemsProcessed(num_moves);
}

BENCHMARK(BM_PathStateChangeCommitShortPaths<ChangeSparsity::kSparse>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathStateChangeCommitShortPaths<ChangeSparsity::kDense>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathStateChangeCommitShortPaths<ChangeSparsity::kNone>)
    ->Range(min_nodes, max_nodes);

// Benchmark PathStateFilter::Relax() in a loop.
template <ChangeSparsity sparsity>
void BM_PathStateFilter_Relax(benchmark::State& bench_state) {
  const int num_nodes = bench_state.range(0);
  CHECK_GE(num_nodes, 8);
  const int start1 = num_nodes / 2;
  // Initial state: 0 -> ... -> start1-1, start1 -> ... -> num_nodes-1.
  auto path_state_owner =
      std::make_unique<PathState>(num_nodes, std::vector<int>{0, start1},
                                  std::vector<int>{start1 - 1, num_nodes - 1});
  PathState* const path_state = path_state_owner.get();
  std::vector<int> path0(start1, 0);
  std::vector<int> path1(num_nodes - start1, 0);
  absl::c_iota(path0, 0);
  absl::c_iota(path1, start1);
  PathStateChangePaths(path_state, {path0, path1}, {});
  path_state->Commit();

  std::vector<PathState::ChainBounds> tmp;
  switch (sparsity) {
    case ChangeSparsity::kNone:
      break;
    case ChangeSparsity::kSparse:
      ExchangePaths(path_state, 0, 1);
      break;
    case ChangeSparsity::kDense:
      ReversePath(path_state, 0, tmp);
      ReversePath(path_state, 1, tmp);
      break;
  }
  const std::vector<std::vector<int>> new_paths = {
      PathStateExtractPath(path_state, 0), PathStateExtractPath(path_state, 1)};
  path_state->Revert();

  Solver solver("bench");
  std::vector<IntVar*> nexts;
  solver.MakeIntVarArray(num_nodes, 0, num_nodes - 1, &nexts);
  const std::unique_ptr<Assignment> delta =
      MakeDeltaFromPaths(&solver, nexts, {path0, path1}, new_paths, {});
  LocalSearchFilter* const filter =
      MakePathStateFilter(&solver, std::move(path_state_owner), nexts);

  int64_t num_moves = 0;
  for (auto _ : bench_state) {
    filter->Relax(delta.get(), nullptr);
    ++num_moves;
  }
  bench_state.SetItemsProcessed(num_moves);
}

BENCHMARK(BM_PathStateFilter_Relax<ChangeSparsity::kSparse>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathStateFilter_Relax<ChangeSparsity::kDense>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_PathStateFilter_Relax<ChangeSparsity::kNone>)
    ->Range(min_nodes, max_nodes);

// Micro-benchmarks for DimensionChecker.
enum class Feasibility : bool { kFeasible, kInfeasible };

// Parameters: sparsity, feasible or not.
template <ChangeSparsity sparsity, Feasibility feasibility>
void BM_DimensionChecker_Check(benchmark::State& bench_state) {
  const int num_nodes = bench_state.range(0);
  CHECK_GE(num_nodes, 8);
  const int start1 = num_nodes / 2;
  PathState path_state(num_nodes, {0, start1}, {start1 - 1, num_nodes - 1});
  std::vector<int> path_class = {0, 0};
  using Intervals = std::vector<DimensionChecker::Interval>;
  Intervals path_capacity = {{0, num_nodes / 2}, {0, num_nodes / 2}};
  std::vector<Intervals> demand = {Intervals(num_nodes, {1, 1}),
                                   Intervals(num_nodes, {1, 1})};
  Intervals node_capacity(num_nodes, {kint64min, kint64max});
  DimensionChecker checker(&path_state, path_capacity, path_class,
                           {ConstantIntervalTransit(1)}, node_capacity);

  // Initial state: 0 -> ... -> start1-1, start1 -> ... -> num_nodes-1.
  std::vector<int> path0(start1, 0);
  std::vector<int> path1(num_nodes - start1, 0);
  absl::c_iota(path0, 0);
  absl::c_iota(path1, start1);
  PathStateChangePaths(&path_state, {path0, path1}, {});
  checker.Check();
  checker.Commit();
  path_state.Commit();

  std::vector<PathState::ChainBounds> tmp;
  switch (sparsity) {
    case ChangeSparsity::kNone:
      break;
    case ChangeSparsity::kSparse:
      if (feasibility == Feasibility::kFeasible) {
        ExchangePaths(&path_state, 0, 1);
      } else {
        // Insert the inside of path 1 at the beginning of path 0.
        const auto [begin0_index, end0_index] =
            path_state.CommittedPathRange(0);
        const auto [begin1_index, end1_index] =
            path_state.CommittedPathRange(1);
        path_state.ChangePath(0, {{begin0_index, begin0_index + 1},
                                  {begin1_index + 1, end1_index - 1},
                                  {begin0_index + 1, end0_index}});
        path_state.ChangePath(1, {{begin1_index, begin1_index + 1},
                                  {end1_index - 1, end1_index}});
      }
      break;
    case ChangeSparsity::kDense:
      if (feasibility == Feasibility::kFeasible) {
        ReversePath(&path_state, 0, tmp);
        ReversePath(&path_state, 1, tmp);
      } else {
        // Put all inside nodes in path 0, in reverse order:
        // 0 -> start1-2 -> start1-3 ... -> 1 ->
        //      num_nodes-2 -> num_nodes-3 ... -> start1+1 -> start1-1.
        const auto [begin0_index, end0_index] =
            path_state.CommittedPathRange(0);
        const auto [begin1_index, end1_index] =
            path_state.CommittedPathRange(1);
        tmp.emplace_back(begin0_index, begin0_index + 1);
        for (int index = end0_index - 2; index > begin0_index; --index) {
          tmp.emplace_back(index, index + 1);
        }
        for (int index = end1_index - 2; index > begin1_index; --index) {
          tmp.emplace_back(index, index + 1);
        }
        tmp.emplace_back(end0_index - 1, end0_index);
        path_state.ChangePath(0, tmp);
        // Path 1 is empty.
        path_state.ChangePath(1, {{begin1_index, begin1_index + 1},
                                  {end1_index - 1, end1_index}});
      }
      break;
    default:
      LOG(FATAL) << "Unknown ChangeSparsity enumerator.";
  }

  // It is easy to get the benchmark data wrong, check that it is correct.
  CHECK_EQ(checker.Check(), feasibility == Feasibility::kFeasible);

  int64_t num_checks = 0;
  for (auto _ : bench_state) {
    checker.Check();
    ++num_checks;
  }
  bench_state.SetItemsProcessed(num_checks);
}

BENCHMARK(
    BM_DimensionChecker_Check<ChangeSparsity::kNone, Feasibility::kFeasible>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(
    BM_DimensionChecker_Check<ChangeSparsity::kSparse, Feasibility::kFeasible>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(
    BM_DimensionChecker_Check<ChangeSparsity::kDense, Feasibility::kFeasible>)
    ->Range(min_nodes, max_nodes);
// Note: <ChangeSparsity::kNone, Feasibility::kInfeasible> makes no sense.
BENCHMARK(BM_DimensionChecker_Check<ChangeSparsity::kSparse,
                                    Feasibility::kInfeasible>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(
    BM_DimensionChecker_Check<ChangeSparsity::kDense, Feasibility::kInfeasible>)
    ->Range(min_nodes, max_nodes);

template <ChangeSparsity sparsity>
void BM_DimensionChecker_Commit(benchmark::State& bench_state) {
  const int num_nodes = bench_state.range(0);
  CHECK_GE(num_nodes, 8);
  // Initial state: 4k->4k+1->4k+2->4k+3 for k in [0, num_nodes/4).
  std::vector<int> path_start;
  std::vector<int> path_end;
  const int num_paths = num_nodes / 4;
  for (int path = 0; path < num_paths; ++path) {
    path_start.push_back(4 * path);
    path_end.push_back(4 * path + 3);
  }
  std::vector<std::vector<int>> initial_paths(num_paths);
  for (int path = 0; path < num_paths; ++path) {
    for (int i = 0; i < 4; ++i) {
      initial_paths[path].push_back(4 * path + i);
    }
  }
  PathState path_state(num_nodes, path_start, path_end);
  PathStateChangePaths(&path_state, initial_paths, {});
  // Make checker.
  std::vector<int> path_class(num_paths, 0);
  using Intervals = std::vector<DimensionChecker::Interval>;
  Intervals path_capacity(num_paths, {0, 4});
  Intervals node_capacity(num_nodes, {kint64min, kint64max});
  DimensionChecker checker(&path_state, path_capacity, path_class,
                           {ConstantIntervalTransit(1)}, node_capacity);
  checker.Check();
  checker.Commit();
  path_state.Commit();

  std::vector<PathState::ChainBounds> tmp;
  int num_moves = 0;
  for (auto _ : bench_state) {
    switch (sparsity) {
      case ChangeSparsity::kNone:
        break;
      case ChangeSparsity::kSparse:
        ExchangePaths(&path_state, 0, 1);
        break;
      case ChangeSparsity::kDense:
        ReversePath(&path_state, 0, tmp);
        ReversePath(&path_state, 1, tmp);
        break;
      default:
        LOG(FATAL) << "Unknown ChangeSparsity enumerator: "
                   << static_cast<int>(sparsity);
    }
    checker.Check();
    checker.Commit();
    path_state.Commit();
    ++num_moves;
  }
  bench_state.SetItemsProcessed(num_moves);
}

BENCHMARK(BM_DimensionChecker_Commit<ChangeSparsity::kSparse>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_DimensionChecker_Commit<ChangeSparsity::kDense>)
    ->Range(min_nodes, max_nodes);
BENCHMARK(BM_DimensionChecker_Commit<ChangeSparsity::kNone>)
    ->Range(min_nodes, max_nodes);

// Micro-benchmarks for PathEnergyCostChecker.
enum class ThresholdLocation { kBelow, kAbove, kMiddle };

template <ChangeSparsity sparsity, ThresholdLocation threshold_location>
void BM_PathEnergyCostChecker_Check(benchmark::State& bench_state) {
  const int num_nodes = bench_state.range(0);
  CHECK_GE(num_nodes, 8);
  const int num_heights = bench_state.range(1);
  CHECK_LE(num_heights, num_nodes / 4);
  const int start1 = num_nodes / 2;
  PathState path_state(num_nodes, {0, start1}, {start1 - 1, num_nodes - 1});
  std::vector<int> path_class = {0, 0};

  // To get different #heights:
  // 2 -> force is 1 -1 1 -1 1 -1 1 -1 ...
  // 4 -> force is 1 1 -1 -1 1 1 -1 -1 ...
  // 8 -> force is 1 1 1 1 -1 -1 -1 -1 ...
  // 2^h -> pattern of 2^h ones then 2^h minus ones.
  std::function<int64_t(int64_t)> force_evaluator =
      [num_heights](int64_t node) -> int64_t {
    const int64_t n = node % (2 * num_heights);
    return n < num_heights ? 1 : -1;
  };

  std::function<int64_t(int64_t, int64_t)> distance_evaluator =
      [](int64_t, int64_t) -> int64_t { return 1; };

  int64_t threshold = 0;
  switch (threshold_location) {
    case ThresholdLocation::kBelow:
      threshold = 0;
      break;
    case ThresholdLocation::kAbove:
      threshold = num_nodes;
      break;
    case ThresholdLocation::kMiddle:
      threshold = num_heights / 2;
      break;
    default:
      LOG(FATAL) << "Unknown ThresholdLocation enumerator.";
  }
  const std::vector<PathEnergyCostChecker::EnergyCost> path_energy_cost(
      2, {.threshold = threshold,
          .cost_per_unit_below_threshold = 1,
          .cost_per_unit_above_threshold = 2});

  PathEnergyCostChecker checker(
      /*path_state=*/&path_state,
      /*force_start_min=*/{0, 0},
      /*force_end_min=*/{0, 0},
      /*force_class=*/{0, 0},
      /*force_per_class=*/{&force_evaluator},
      /*distance_class=*/{0, 0},
      /*distance_per_class=*/{&distance_evaluator},
      /*path_energy_cost=*/path_energy_cost,
      /*path_has_cost_when_empty=*/{true, true});

  // Initial state: 0 -> ... -> start1-1, start1 -> ... -> num_nodes-1.
  std::vector<int> path0(start1, 0);
  std::vector<int> path1(num_nodes - start1, 0);
  absl::c_iota(path0, 0);
  absl::c_iota(path1, start1);
  PathStateChangePaths(&path_state, {path0, path1}, {});
  checker.Check();
  checker.Commit();
  path_state.Commit();

  std::vector<PathState::ChainBounds> tmp;
  switch (sparsity) {
    case ChangeSparsity::kNone: {
      break;
    }
    case ChangeSparsity::kSparse: {
      // Insert the inside of path 1 at the beginning of path 0.
      const auto [begin0_index, end0_index] = path_state.CommittedPathRange(0);
      const auto [begin1_index, end1_index] = path_state.CommittedPathRange(1);
      const int step = num_heights / 2;
      path_state.ChangePath(0, {{begin0_index, begin0_index + step},
                                {begin1_index + step, end1_index - step},
                                {end0_index - step, end0_index}});
      path_state.ChangePath(1, {{begin1_index, begin1_index + step},
                                {begin0_index + step, end0_index - step},
                                {end1_index - step, end1_index}});
      break;
    }
    case ChangeSparsity::kDense: {
      ReversePath(&path_state, 0, tmp);
      ReversePath(&path_state, 1, tmp);
      break;
    }
    default: {
      LOG(FATAL) << "Unknown ChangeSparsity enumerator.";
    }
  }

  int64_t num_checks = 0;
  for (auto _ : bench_state) {
    checker.Check();
    ++num_checks;
  }
  bench_state.SetItemsProcessed(num_checks);
}

BENCHMARK(BM_PathEnergyCostChecker_Check<ChangeSparsity::kSparse,
                                         ThresholdLocation::kBelow>)
    ->RangePair(min_nodes, max_nodes, 2, 2);
BENCHMARK(BM_PathEnergyCostChecker_Check<ChangeSparsity::kSparse,
                                         ThresholdLocation::kAbove>)
    ->RangePair(min_nodes, max_nodes, 2, 2);
BENCHMARK(BM_PathEnergyCostChecker_Check<ChangeSparsity::kSparse,
                                         ThresholdLocation::kMiddle>)
    ->ArgPair(8, 2)
    ->ArgPair(64, 2)
    ->ArgPair(64, 4)
    ->ArgPair(64, 8)
    ->ArgPair(64, 16)
    ->ArgPair(512, 2)
    ->ArgPair(512, 8)
    ->ArgPair(512, 32)
    ->ArgPair(512, 128)
    ->ArgPair(4096, 2)
    ->ArgPair(4096, 16)
    ->ArgPair(4096, 128)
    ->ArgPair(4096, 1024);
BENCHMARK(BM_PathEnergyCostChecker_Check<ChangeSparsity::kDense,
                                         ThresholdLocation::kMiddle>)
    ->RangePair(min_nodes, max_nodes, 2, 2);
BENCHMARK(BM_PathEnergyCostChecker_Check<ChangeSparsity::kNone,
                                         ThresholdLocation::kMiddle>)
    ->RangePair(min_nodes, max_nodes, 2, 2);

}  // namespace
}  // namespace operations_research::routing
