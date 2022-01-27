// Copyright 2010-2021 Google LLC
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

//
// Traveling Salesman Sample.
//
// This is a sample using the routing library to solve a Traveling Salesman
// Problem.
// The description of the problem can be found here:
// http://en.wikipedia.org/wiki/Travelling_salesman_problem.
// For small problems one can use the hamiltonian path library directly (cf
// graph/hamiltonian_path.h).
// The optimization engine uses local search to improve solutions, first
// solutions being generated using a cheapest addition heuristic.
// Optionally one can randomly forbid a set of random connections between nodes
// (forbidden arcs).

#include <memory>

#include "absl/memory/memory.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/text_format.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/constraint_solver/routing.h"
#include "ortools/constraint_solver/routing_index_manager.h"
#include "ortools/constraint_solver/routing_parameters.h"
#include "ortools/constraint_solver/routing_parameters.pb.h"
#include "ortools/util/random_engine.h"

ABSL_FLAG(int, tsp_size, 10, "Size of Traveling Salesman Problem instance.");
ABSL_FLAG(bool, tsp_use_random_matrix, true, "Use random cost matrix.");
ABSL_FLAG(int, tsp_random_forbidden_connections, 0,
          "Number of random forbidden connections.");
ABSL_FLAG(bool, tsp_use_deterministic_random_seed, false,
          "Use deterministic random seeds.");
ABSL_FLAG(std::string, routing_search_parameters,
          "local_search_operators {"
          "  use_path_lns:BOOL_TRUE"
          "  use_inactive_lns:BOOL_TRUE"
          "}",
          "Text proto RoutingSearchParameters (possibly partial) that will "
          "override the DefaultRoutingSearchParameters()");

namespace operations_research {

// Random seed generator.
int32_t GetSeed() {
  if (absl::GetFlag(FLAGS_tsp_use_deterministic_random_seed)) {
    return 0;
  } else {
    return std::random_device()();
  }
}

// Cost/distance functions.

// Sample function.
int64_t MyDistance(RoutingIndexManager::NodeIndex from,
                 RoutingIndexManager::NodeIndex to) {
  // Put your distance code here.
  return (from + to).value();  // for instance
}

// Random matrix.
class RandomMatrix {
 public:
  explicit RandomMatrix(int size) : size_(size) {}
  void Initialize() {
    matrix_ = absl::make_unique<int64_t[]>(size_ * size_);
    const int64_t kDistanceMax = 100;
    random_engine_t randomizer(GetSeed());
    for (RoutingIndexManager::NodeIndex from(0); from < size_; ++from) {
      for (RoutingIndexManager::NodeIndex to(0); to < size_; ++to) {
        if (to != from) {
          matrix_[MatrixIndex(from, to)] = absl::Uniform(randomizer, 0, kDistanceMax);
        } else {
          matrix_[MatrixIndex(from, to)] = 0LL;
        }
      }
    }
  }
  int64_t Distance(RoutingIndexManager::NodeIndex from,
                 RoutingIndexManager::NodeIndex to) const {
    return matrix_[MatrixIndex(from, to)];
  }

 private:
  int64_t MatrixIndex(RoutingIndexManager::NodeIndex from,
                    RoutingIndexManager::NodeIndex to) const {
    return (from * size_ + to).value();
  }
  std::unique_ptr<int64_t[]> matrix_;
  const int size_;
};

void Tsp() {
  if (absl::GetFlag(FLAGS_tsp_size) > 0) {
    // TSP of size absl::GetFlag(FLAGS_tsp_size).
    // Second argument = 1 to build a single tour (it's a TSP).
    // Nodes are indexed from 0 to absl::GetFlag(FLAGS_tsp_size) - 1, by default
    // the start of
    // the route is node 0.
    RoutingIndexManager manager(absl::GetFlag(FLAGS_tsp_size), 1,
                                RoutingIndexManager::NodeIndex(0));
    RoutingModel routing(manager);
    RoutingSearchParameters parameters = DefaultRoutingSearchParameters();
    CHECK(google::protobuf::TextFormat::MergeFromString(
        absl::GetFlag(FLAGS_routing_search_parameters), &parameters));

    // Setting the cost function.
    // Put a permanent callback to the distance accessor here. The callback
    // has the following signature: ResultCallback2<int64_t, int64_t, int64_t>.
    // The two arguments are the from and to node inidices.
    RandomMatrix matrix(absl::GetFlag(FLAGS_tsp_size));
    if (absl::GetFlag(FLAGS_tsp_use_random_matrix)) {
      matrix.Initialize();
      const int vehicle_cost = routing.RegisterTransitCallback(
          [&matrix, &manager](int64_t i, int64_t j) {
            return matrix.Distance(manager.IndexToNode(i),
                                   manager.IndexToNode(j));
          });
      routing.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    } else {
      const int vehicle_cost =
          routing.RegisterTransitCallback([&manager](int64_t i, int64_t j) {
            return MyDistance(manager.IndexToNode(i), manager.IndexToNode(j));
          });
      routing.SetArcCostEvaluatorOfAllVehicles(vehicle_cost);
    }
    // Forbid node connections (randomly).
    random_engine_t randomizer(GetSeed());
    int64_t forbidden_connections = 0;
    while (forbidden_connections <
           absl::GetFlag(FLAGS_tsp_random_forbidden_connections)) {
      const int64_t from = absl::Uniform(randomizer, 0, absl::GetFlag(FLAGS_tsp_size) - 1);
      const int64_t to =
          absl::Uniform(randomizer, 0 , absl::GetFlag(FLAGS_tsp_size) - 1) + 1;
      if (routing.NextVar(from)->Contains(to)) {
        LOG(INFO) << "Forbidding connection " << from << " -> " << to;
        routing.NextVar(from)->RemoveValue(to);
        ++forbidden_connections;
      }
    }
    // Solve, returns a solution if any (owned by RoutingModel).
    const Assignment* solution = routing.SolveWithParameters(parameters);
    if (solution != nullptr) {
      // Solution cost.
      LOG(INFO) << "Cost " << solution->ObjectiveValue();
      // Inspect solution.
      // Only one route here; otherwise iterate from 0 to routing.vehicles() - 1
      const int route_number = 0;
      std::string route;
      for (int64_t node = routing.Start(route_number); !routing.IsEnd(node);
           node = solution->Value(routing.NextVar(node))) {
        absl::StrAppend(&route, manager.IndexToNode(node).value(), " (", node,
                        ") -> ");
      }
      const int64_t end = routing.End(route_number);
      absl::StrAppend(&route, manager.IndexToNode(end).value(), " (", end, ")");
      LOG(INFO) << route;
    } else {
      LOG(INFO) << "No solution found.";
    }
  } else {
    LOG(INFO) << "Specify an instance size greater than 0.";
  }
}
}  // namespace operations_research

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  operations_research::Tsp();
  return EXIT_SUCCESS;
}
