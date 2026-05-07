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
#include <random>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/random/distributions.h"
#include "benchmark/benchmark.h"
#include "ortools/graph/min_cost_flow.h"
#include "ortools/graph_base/graph.h"

namespace operations_research {
namespace {

// Benchmark inspired from the existing problem of matching Youtube ads channels
// to Youtube users, maximizing the expected revenue:
// - Each channel needs an exact number of users assigned to it.
// - Each user has an upper limit on the number of channels they can be assigned
//   to, with a guarantee that this upper limit won't prevent the channels to
//   get their required number of users.
// - Each pair (user, channel) has a known expected revenue, which is modeled
//   as a small-ish integer (<3K). Using larger ranges can slightly impact
//   performance, and you should look for a good trade-off with the accuracy.
//
// IMPORTANT: don't run this with default flags! Use:
//   blaze run -c opt --linkopt=-static [--run_under=perflab] --
//   ortools/graph/min_cost_flow_test --benchmarks=all
//   --benchmark_min_iters=1 --heap_check= --benchmark_memory_usage
template <typename Graph, typename ArcFlowType, typename ArcScaledCostType,
          int kNumChannels, int kNumUsers>
void BM_MinCostFlowOnMultiMatchingProblem(benchmark::State& state) {
  std::mt19937 my_random(12345);
  const double kDensity = 1.0 / 200;
  const int kMaxChannelsPerUser = 5 * static_cast<int>(kDensity * kNumChannels);
  const int kAverageNumUsersPerChannels =
      static_cast<int>(kDensity * kNumUsers);
  std::vector<int> num_users_per_channel(kNumChannels, -1);
  int total_demand = 0;
  for (int i = 0; i < kNumChannels; ++i) {
    num_users_per_channel[i] =
        1 + absl::Uniform(my_random, 0, 2 * kAverageNumUsersPerChannels - 1);
    total_demand += num_users_per_channel[i];
  }
  // User #j, when assigned to channel #i, is expected to generate
  // -expected_cost_per_channel_user[kNumUsers * i + j]: since MinCostFlow
  // only *minimizes* costs, and doesn't maximizes revenue, we just set
  // cost = -revenue.
  // To stress the algorithm, we generate a cost matrix that is highly skewed
  // and that would probably challenge greedy approaches: each user gets
  // a random coefficient, each channel as well, and then the expected revenue
  // of a (user, channel) pairing is the product of these coefficient, plus a
  // small (per-cell) random perturbation.
  std::vector<int16_t> expected_cost_per_channel_user(kNumChannels * kNumUsers);
  {
    std::vector<int16_t> channel_coeff(kNumChannels);
    for (int i = 0; i < kNumChannels; ++i) {
      channel_coeff[i] = absl::Uniform(my_random, 0, 48);
    }
    std::vector<int16_t> user_coeff(kNumUsers);
    for (int j = 0; j < kNumUsers; ++j) {
      user_coeff[j] = absl::Uniform(my_random, 0, 48);
    }
    for (int i = 0; i < kNumChannels; ++i) {
      for (int j = 0; j < kNumUsers; ++j) {
        expected_cost_per_channel_user[kNumUsers * i + j] =
            -channel_coeff[i] * user_coeff[j] - absl::Uniform(my_random, 0, 10);
      }
    }
  }
  CHECK_LE(total_demand, kNumUsers * kMaxChannelsPerUser);

  for (auto _ : state) {
    typename Graph::Builder builder(
        /*num_nodes=*/kNumUsers + kNumChannels + 1,
        /*arc_capacity=*/kNumChannels * kNumUsers + kNumUsers);
    // We model this problem as a graph (on which we'll do a min-cost flow):
    // - Each channel #i is a source node (index #i + 1) with supply
    //   num_users_per_channel[i].
    // - There is a global sink node (index 0) with a demand equal to the
    //   sum of num_users_per_channel.
    // - Each user #j is an intermediate node (index 1 + kNumChannels + j)
    //   with no supply or demand, but with an arc of capacity
    //   kMaxChannelsPerUser towards the global sink node (and of unit cost 0).
    // - There is an arc from each channel #i to each user #j, with capacity 1
    //   and unit cost expected_cost_per_channel_user[kNumUsers * i + j].

    for (int i = 0; i < kNumChannels; ++i) {
      for (int j = 0; j < kNumUsers; ++j) {
        builder.AddArc(/*tail=*/i + 1, /*head=*/kNumChannels + 1 + j);
      }
    }
    for (int j = 0; j < kNumUsers; ++j) {
      builder.AddArc(/*tail=*/kNumChannels + 1 + j, /*head=*/0);
    }
    std::vector<typename Graph::ArcIndex> permutation;
    const auto graph = std::move(builder).Build(&permutation);
    // To spare memory, we added arcs in the right order, so that no permutation
    // is needed. See graph.h.
    CHECK(permutation.empty());

    // To spare memory, the types are chosen as small as possible.
    GenericMinCostFlow<Graph, ArcFlowType, ArcScaledCostType> min_cost_flow(
        graph.get());

    // We also disable the feasibility check which takes a huge amount of
    // memory.
    min_cost_flow.SetCheckFeasibility(false);

    min_cost_flow.SetNodeSupply(/*node=*/0, /*supply=*/-total_demand);
    // Now, set the arcs capacity and cost, in the same order as we created them
    // above.
    typename Graph::ArcIndex arc_index = 0;
    for (int i = 0; i < kNumChannels; ++i) {
      min_cost_flow.SetNodeSupply(
          /*node=*/i + 1, /*supply=*/num_users_per_channel[i]);
      for (int j = 0; j < kNumUsers; ++j) {
        min_cost_flow.SetArcUnitCost(
            arc_index, expected_cost_per_channel_user[kNumUsers * i + j]);
        min_cost_flow.SetArcCapacity(arc_index, 1);
        arc_index++;
      }
    }
    for (int j = 0; j < kNumUsers; ++j) {
      min_cost_flow.SetArcUnitCost(arc_index, 0);
      min_cost_flow.SetArcCapacity(arc_index, kMaxChannelsPerUser);
      arc_index++;
    }
    const bool solved_ok = min_cost_flow.Solve();
    CHECK(solved_ok);
    LOG(INFO) << "Maximum revenue: " << -min_cost_flow.GetOptimalCost();
  }
}

// We don't have more than 65536 nodes, so we use 16-bit integers to spare
// memory (and potentially speed up the code). Arc indices must be 32 bits
// though, since we have much more.
BENCHMARK(BM_MinCostFlowOnMultiMatchingProblem<
          util::ReverseArcStaticGraph<uint16_t, int32_t>, int16_t, int32_t,
          /*kNumChannels=*/20000, /*kNumUsers=*/20000>);

// We also benchmark with default parameter types for reference.
// We use fewer channels and users to avoid running out of memory.
BENCHMARK(BM_MinCostFlowOnMultiMatchingProblem<
          ::util::ReverseArcListGraph<>, int64_t, int64_t,
          /*kNumChannels=*/5000, /*kNumUsers=*/5000>);

}  // namespace
}  // namespace operations_research
