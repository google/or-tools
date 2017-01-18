// Copyright 2010-2014 Google
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

// ChristofidesPathSolver computes an approximate solution to the Traveling
// Salesman Problen using the Christofides algorithm (c.f.
// https://en.wikipedia.org/wiki/Christofides_algorithm).
// Note that the algorithm guarantees finding a solution within 3/2 of the
// optimum. Its complexity is O(n^2 * log(n)) where n is the number of nodes.

#ifndef OR_TOOLS_GRAPH_CHRISTOFIDES_H_
#define OR_TOOLS_GRAPH_CHRISTOFIDES_H_

#include "base/integral_types.h"
#include "base/logging.h"
#include "graph/eulerian_path.h"
#include "graph/minimum_spanning_tree.h"

namespace operations_research {

template <typename CostType>
class ChristofidesPathSolver {
  // TODO(user): Add an API taking a std::vector<std::vector<CostType>> for cost.
 public:
  ChristofidesPathSolver(int num_nodes, std::function<CostType(int, int)> cost);

  // Returns the cost of the approximate TSP tour.
  CostType TravelingSalesmanCost();

  // Returns the approximate TSP tour.
  std::vector<int> TravelingSalesmanPath();

 private:
  // Runs the Christofides algorithm.
  void Solve();

  // The complete graph on the nodes of the problem.
  CompleteGraph<> graph_;

  // Function returning the cost between nodes of the problem.
  const std::function<CostType(int, int)> costs_;

  // The cost of the computed TSP path.
  CostType tsp_cost_;

  // The path of the computed TSP,
  std::vector<int> tsp_path_;

  // True if the TSP has been solved, false otherwise.
  bool solved_;
};

template <typename CostType>
ChristofidesPathSolver<CostType>::ChristofidesPathSolver(
    int num_nodes, std::function<CostType(int, int)> costs)
    : graph_(num_nodes),
      costs_(std::move(costs)),
      tsp_cost_(0),
      solved_(false) {}

template <typename CostType>
CostType ChristofidesPathSolver<CostType>::TravelingSalesmanCost() {
  if (!solved_) {
    Solve();
  }
  return tsp_cost_;
}

template <typename CostType>
std::vector<int> ChristofidesPathSolver<CostType>::TravelingSalesmanPath() {
  if (!solved_) {
    Solve();
  }
  return tsp_path_;
}

template <typename CostType>
void ChristofidesPathSolver<CostType>::Solve() {
  const int num_nodes = graph_.num_nodes();
  tsp_path_.clear();
  tsp_cost_ = 0;
  if (num_nodes == 1) {
    tsp_path_ = {0, 0};
  }
  if (num_nodes <= 1) {
    return;
  }
  // Compute Minimum Spanning Tree.
  const std::vector<int> mst = BuildPrimMinimumSpanningTree(
      graph_,
      [this](int arc) { return costs_(graph_.Tail(arc), graph_.Head(arc)); });
  // Detect odd degree nodes.
  std::vector<int> degrees(num_nodes, 0);
  for (int arc : mst) {
    degrees[graph_.Tail(arc)]++;
    degrees[graph_.Head(arc)]++;
  }
  std::vector<int> odd_degree_nodes;
  for (int i = 0; i < degrees.size(); ++i) {
    if (degrees[i] % 2 != 0) {
      odd_degree_nodes.push_back(i);
    }
  }
  // Find minimum-weight perfect matching on odd-degree-node complete graph.
  // TODO(user): Make this code available as an independent algorithm.
  // TODO(user): Cost caching was added and can gain up to 20% but increases
  // memory usage; see if we can avoid caching.
  const int reduced_size = odd_degree_nodes.size();
  DCHECK_NE(0, reduced_size);
  CompleteGraph<> reduced_graph(reduced_size);
  std::vector<int> ordered_arcs(reduced_graph.num_arcs());
  std::vector<CostType> ordered_arc_costs(reduced_graph.num_arcs(), 0);
  for (const int arc : reduced_graph.AllForwardArcs()) {
    ordered_arcs[arc] = arc;
    ordered_arc_costs[arc] = costs_(odd_degree_nodes[reduced_graph.Tail(arc)],
                                    odd_degree_nodes[reduced_graph.Head(arc)]);
  }
  std::sort(ordered_arcs.begin(), ordered_arcs.end(),
            [&ordered_arc_costs](int arc_a, int arc_b) {
              return ordered_arc_costs[arc_a] < ordered_arc_costs[arc_b];
            });
  std::vector<int> closure_arcs;
  std::vector<bool> touched_nodes(reduced_size, false);
  for (int arc_index = 0; closure_arcs.size() * 2 < reduced_size; ++arc_index) {
    const int arc = ordered_arcs[arc_index];
    const int tail = reduced_graph.Tail(arc);
    const int head = reduced_graph.Head(arc);
    if (head != tail && !touched_nodes[tail] && !touched_nodes[head]) {
      touched_nodes[tail] = true;
      touched_nodes[head] = true;
      closure_arcs.push_back(arc);
    }
  }
  // Build Eulerian path on minimum spanning tree + closing edges from matching
  // and extract a solution to the Traveling Salesman from the path by skipping
  // duplicate nodes.
  ReverseArcListGraph<> egraph(num_nodes, closure_arcs.size() + mst.size());
  for (int arc : mst) {
    const int tail = graph_.Tail(arc);
    const int head = graph_.Head(arc);
    egraph.AddArc(tail, head);
  }
  for (int arc : closure_arcs) {
    const int tail = odd_degree_nodes[reduced_graph.Tail(arc)];
    const int head = odd_degree_nodes[reduced_graph.Head(arc)];
    egraph.AddArc(tail, head);
  }
  std::vector<bool> touched(num_nodes, false);
  DCHECK(IsEulerianGraph(egraph));
  for (const int node : BuildEulerianTourFromNode(egraph, 0)) {
    if (touched[node]) continue;
    touched[node] = true;
    tsp_cost_ += tsp_path_.empty() ? 0 : costs_(tsp_path_.back(), node);
    tsp_path_.push_back(node);
  }
  tsp_cost_ += tsp_path_.empty() ? 0 : costs_(tsp_path_.back(), 0);
  tsp_path_.push_back(0);
  solved_ = true;
}
}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_CHRISTOFIDES_H_
