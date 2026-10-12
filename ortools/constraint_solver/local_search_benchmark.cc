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
#include <memory>
#include <utility>
#include <vector>

#include "absl/types/span.h"
#include "benchmark/benchmark.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/local_search.h"

namespace operations_research {
namespace {

using VariableDomainId = LocalSearchState::VariableDomainId;

std::pair<std::unique_ptr<LocalSearchState>, std::vector<VariableDomainId>>
MakeStateWithVariableDomains(
    absl::Span<const std::pair<int64_t, int64_t>> domains) {
  auto state = std::make_unique<LocalSearchState>();
  std::vector<VariableDomainId> domain_ids;
  domain_ids.reserve(domains.size());
  for (const auto [min, max] : domains) {
    domain_ids.push_back(state->AddVariableDomain(min, max));
  }
  return std::make_pair(std::move(state), domain_ids);
}

enum class VehicleHomogeneity : bool { kHomogeneous, kHeterogeneous };

// Micro-benchmarks for common local search operators, timing the time to
// generate a neighbor (this excludes local search filtering and propagation
// in the solver).
// Neighbors are generated from a solution containing 'num_paths' paths
// containing 'path_size' nodes (therefore all paths are balanced).
template <Solver::LocalSearchOperators operator_type,
          VehicleHomogeneity vehicle_homogeneity>
void BM_GenerateNeigbors(benchmark::State& state) {
  const int num_paths = state.range(0);
  const int path_size = state.range(1);

  const int num_nodes = num_paths * path_size;
  Solver solver("BM_GenerateNeigbors");

  std::vector<IntVar*> nexts;
  solver.MakeIntVarArray(num_nodes, num_paths, num_nodes + num_paths - 1, "",
                         &nexts);
  Assignment solution(&solver);
  solution.Add(nexts);
  std::vector<IntVar*> paths;
  if (vehicle_homogeneity == VehicleHomogeneity::kHeterogeneous) {
    solver.MakeIntVarArray(num_nodes + num_paths, -1, num_paths, "", &paths);
    solution.Add(paths);
  }

  // Path p goes from start node p to end node p + num_nodes by increments of
  // num_paths.
  for (int path = 0; path < num_paths; ++path) {
    for (int position = 0; position < path_size; ++position) {
      const int node = path + num_paths * position;
      const int next = node + num_paths;
      solution.SetValue(nexts[node], next);
      if (vehicle_homogeneity == VehicleHomogeneity::kHeterogeneous) {
        solution.SetValue(paths[node], path);
      }
    }
    if (vehicle_homogeneity == VehicleHomogeneity::kHeterogeneous) {
      solution.SetValue(paths[num_nodes + path], path);
    }
  }
  LocalSearchOperator* const op =
      solver.MakeOperator(nexts, paths, operator_type);

  op->Start(&solution);
  Assignment delta(&solver);
  Assignment deltadelta(&solver);
  int64_t neighbor_count = 0;

  for (auto s : state) {
    while (op->MakeNextNeighbor(&delta, &deltadelta)) {
      delta.Clear();
      deltadelta.Clear();
      ++neighbor_count;
    }
  }

  state.SetItemsProcessed(neighbor_count);
}

#define BENCHMARK_GENERATE_NEIGHBORS(operator_type)              \
  BENCHMARK_TEMPLATE(BM_GenerateNeigbors, Solver::operator_type, \
                     VehicleHomogeneity::kHomogeneous)           \
      ->RangePair(1, 50,  /* num_paths */                        \
                  5, 50); /* path_size */                        \
  BENCHMARK_TEMPLATE(BM_GenerateNeigbors, Solver::operator_type, \
                     VehicleHomogeneity::kHeterogeneous)         \
      ->RangePair(1, 50,  /* num_paths */                        \
                  5, 50); /* path_size */

BENCHMARK_GENERATE_NEIGHBORS(RELOCATE)
BENCHMARK_GENERATE_NEIGHBORS(EXCHANGE)
BENCHMARK_GENERATE_NEIGHBORS(CROSS)
BENCHMARK_GENERATE_NEIGHBORS(TWOOPT)
BENCHMARK_GENERATE_NEIGHBORS(OROPT)
BENCHMARK_GENERATE_NEIGHBORS(MAKEACTIVE)
BENCHMARK_GENERATE_NEIGHBORS(MAKEINACTIVE)
BENCHMARK_GENERATE_NEIGHBORS(MAKECHAININACTIVE)
BENCHMARK_GENERATE_NEIGHBORS(SWAPACTIVE)
BENCHMARK_GENERATE_NEIGHBORS(EXTENDEDSWAPACTIVE)
BENCHMARK_GENERATE_NEIGHBORS(PATHLNS)
BENCHMARK_GENERATE_NEIGHBORS(FULLPATHLNS)
BENCHMARK_GENERATE_NEIGHBORS(UNACTIVELNS)

#undef BENCHMARK_GENERATE_NEIGHBORS

enum class Feasibility : bool { kFeasible, kInfeasible };

template <Feasibility feasibility>
void BM_LocalSearchState_OneLayerWeightedSumRevert(
    benchmark::State& bench_state) {
  const int num_leaves = bench_state.range(0);
  const std::vector<std::pair<int64_t, int64_t>> domains(num_leaves + 1,
                                                         {0, 1000});
  auto [state, domain_ids] = MakeStateWithVariableDomains(domains);
  state->ChangeRelaxedVariableDomain(domain_ids.back(), 0, 999);
  const int64_t new_min = feasibility == Feasibility::kFeasible ? 1 : 1000;
  {
    const VariableDomainId output_domain_id = domain_ids.back();
    domain_ids.pop_back();
    std::vector<int64_t> weights(num_leaves, 1);
    state->AddWeightedSumConstraint(domain_ids, weights, 0, output_domain_id);
    state->CompileConstraints();
  }
  int64_t num_tightens = 0;
  for (auto s : bench_state) {
    const VariableDomainId domain_id = domain_ids[num_leaves / 2];
    state->RelaxVariableDomain(domain_id);
    state->TightenVariableDomainMin(domain_id, new_min);
    state->PropagateTighten(domain_id);
    state->Revert();
    ++num_tightens;
  }
  bench_state.SetItemsProcessed(num_tightens);
}

BENCHMARK(BM_LocalSearchState_OneLayerWeightedSumRevert<Feasibility::kFeasible>)
    ->Range(1 << 2, 1 << 14);
BENCHMARK(
    BM_LocalSearchState_OneLayerWeightedSumRevert<Feasibility::kInfeasible>)
    ->Range(1 << 2, 1 << 14);

enum class CommitRegularity {
  kCommitAfterEveryInterval,
  kCommitAfterAllIntervals,
  kNoCommit
};

template <CommitRegularity regularity>
void BM_LocalSearchState_ManyLayeredWeightedSumCommit(
    benchmark::State& bench_state) {
  const int num_leaves = bench_state.range(0);
  const int num_layers = bench_state.range(1);
  if (num_leaves < 1 || num_layers < 1) return;
  LocalSearchState state;
  std::vector<VariableDomainId> domain_ids(num_leaves, VariableDomainId(-1));
  for (VariableDomainId& domain_id : domain_ids) {
    domain_id = state.AddVariableDomain(0, 1'000'000'000'000'000);
  }
  VariableDomainId output_domain_id =
      state.AddVariableDomain(0, 1'000'000'000'000'000);
  {
    std::vector<int64_t> weights(num_leaves, 1);
    state.AddWeightedSumConstraint(domain_ids, weights, 0, output_domain_id);
    std::vector<VariableDomainId> mid_domain_ids;
    for (int l = 1; l < num_layers; ++l) {
      mid_domain_ids.clear();
      mid_domain_ids.push_back(output_domain_id);
      for (int i = 1; i < num_leaves; ++i) {
        mid_domain_ids.push_back(
            state.AddVariableDomain(0, 1'000'000'000'000'000));
      }
      output_domain_id = state.AddVariableDomain(0, 1'000'000'000'000'000);
      state.AddWeightedSumConstraint(mid_domain_ids, weights, 0,
                                     output_domain_id);
    }
    state.CompileConstraints();
  }
  int64_t new_min = 1;
  int64_t num_tightens = 0;
  for (const VariableDomainId domain_id : domain_ids) {
    state.RelaxVariableDomain(domain_id);
  }
  for (auto s : bench_state) {
    switch (regularity) {
      case CommitRegularity::kCommitAfterEveryInterval:
        for (const VariableDomainId domain_id : domain_ids) {
          state.RelaxVariableDomain(domain_id);
          state.TightenVariableDomainMin(domain_id, new_min);
          state.PropagateTighten(domain_id);
          state.Commit();
        }
        break;
      case CommitRegularity::kCommitAfterAllIntervals:
        for (const VariableDomainId domain_id : domain_ids) {
          state.RelaxVariableDomain(domain_id);
        }
        for (const VariableDomainId domain_id : domain_ids) {
          state.TightenVariableDomainMin(domain_id, new_min);
          state.PropagateTighten(domain_id);
        }
        state.Commit();
        break;
      case CommitRegularity::kNoCommit:
        for (const VariableDomainId domain_id : domain_ids) {
          state.TightenVariableDomainMin(domain_id, new_min);
          state.PropagateTighten(domain_id);
        }
        break;
    }
    num_tightens += num_leaves;
    ++new_min;
  }
  bench_state.SetItemsProcessed(num_tightens);
}

BENCHMARK(BM_LocalSearchState_ManyLayeredWeightedSumCommit<
              CommitRegularity::kCommitAfterEveryInterval>)
    ->ArgPair(1 << 2, 1 << 0)
    ->ArgPair(1 << 2, 1 << 2)
    ->ArgPair(1 << 2, 1 << 4)
    ->ArgPair(1 << 6, 1 << 0)
    ->ArgPair(1 << 6, 1 << 2)
    ->ArgPair(1 << 6, 1 << 4)
    ->ArgPair(1 << 10, 1 << 0)
    ->ArgPair(1 << 10, 1 << 2)
    ->ArgPair(1 << 10, 1 << 4);

BENCHMARK(BM_LocalSearchState_ManyLayeredWeightedSumCommit<
              CommitRegularity::kCommitAfterAllIntervals>)
    ->ArgPair(1 << 2, 1 << 0)
    ->ArgPair(1 << 2, 1 << 2)
    ->ArgPair(1 << 2, 1 << 4)
    ->ArgPair(1 << 6, 1 << 0)
    ->ArgPair(1 << 6, 1 << 2)
    ->ArgPair(1 << 6, 1 << 4)
    ->ArgPair(1 << 10, 1 << 0)
    ->ArgPair(1 << 10, 1 << 2)
    ->ArgPair(1 << 10, 1 << 4);

BENCHMARK(BM_LocalSearchState_ManyLayeredWeightedSumCommit<
              CommitRegularity::kNoCommit>)
    ->ArgPair(1 << 2, 1 << 0)
    ->ArgPair(1 << 2, 1 << 2)
    ->ArgPair(1 << 2, 1 << 4)
    ->ArgPair(1 << 6, 1 << 0)
    ->ArgPair(1 << 6, 1 << 2)
    ->ArgPair(1 << 6, 1 << 4)
    ->ArgPair(1 << 10, 1 << 0)
    ->ArgPair(1 << 10, 1 << 2)
    ->ArgPair(1 << 10, 1 << 4);

}  // namespace
}  // namespace operations_research
