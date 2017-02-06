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

#include <unordered_map>

#include "base/integral_types.h"
#include "base/logging.h"
#include "graph/eulerian_path.h"
#include "graph/minimum_spanning_tree.h"
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver.pb.h"

namespace operations_research {

template <typename CostType, typename ArcIndex = int64,
          typename NodeIndex = int32,
          typename CostFunction = std::function<CostType(NodeIndex, NodeIndex)>>
class ChristofidesPathSolver {
 public:
  enum class MatchingAlgorithm {
    MINIMUM_WEIGHT_MATCHING,
    MINIMAL_WEIGHT_MATCHING,
  };
  ChristofidesPathSolver(NodeIndex num_nodes, CostFunction costs);

  // Sets the matching algorith to use. A minimum weight perfect matching
  // (MINIMUM_WEIGHT_MATCHING) guarantees the 3/2 upper bound to the optimal
  // solution. A minimal weight perfect matching (MINIMAL_WEIGHT_MATCHING)
  // finds a locally minimal weight matching which does not offer any bound
  // guarantee but, as of 1/2017, is orders of magnitude faster than the
  // minimum matching.
  // By default, MINIMAL_WEIGHT_MATCHING is selected.
  // TODO(user): Change the default when minimum matching gets faster.
  void SetMatchingAlgorithm(MatchingAlgorithm matching) {
    matching_ = matching;
  }

  // Returns the cost of the approximate TSP tour.
  CostType TravelingSalesmanCost();

  // Returns the approximate TSP tour.
  std::vector<NodeIndex> TravelingSalesmanPath();

 private:
  // Runs the Christofides algorithm.
  void Solve();

  // Matching algorithm to use.
  MatchingAlgorithm matching_;

  // The complete graph on the nodes of the problem.
  CompleteGraph<NodeIndex, ArcIndex> graph_;

  // Function returning the cost between nodes of the problem.
  const CostFunction costs_;

  // The cost of the computed TSP path.
  CostType tsp_cost_;

  // The path of the computed TSP,
  std::vector<NodeIndex> tsp_path_;

  // True if the TSP has been solved, false otherwise.
  bool solved_;
};

// Computes a minimum weight perfect matching on an undirected graph using a
// Mixed Integer Programming model.
// TODO(user): Handle infeasible cases if this algorithm is used outside of
// Christofides.
template <typename WeightFunctionType, typename GraphType>
std::vector<typename GraphType::ArcIndex> ComputeMinimumWeightMatchingWithMIP(
    const GraphType& graph, const WeightFunctionType& weight) {
  using ArcIndex = typename GraphType::ArcIndex;
  using NodeIndex = typename GraphType::NodeIndex;
  MPModelProto model;
  model.set_maximize(false);
  // The model is composed of Boolean decision variables to select matching arcs
  // and constraints ensuring that each node appears in exactly one selected
  // arc. The objective is to minimize the sum of the weights of selected arcs.
  // It is assumed the graph is symmetrical.
  std::unordered_map<ArcIndex, ArcIndex> variable_indices;
  for (NodeIndex node : graph.AllNodes()) {
    // Creating arc-selection Boolean variable.
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      const NodeIndex head = graph.Head(arc);
      if (node < head) {
        variable_indices[arc] = model.variable_size();
        MPVariableProto* const arc_var = model.add_variable();
        arc_var->set_lower_bound(0);
        arc_var->set_upper_bound(1);
        arc_var->set_is_integer(true);
        arc_var->set_objective_coefficient(weight(arc));
      }
    }
    // Creating matching constraint:
    // for all node i, sum(j) arc(i,j) + sum(j) arc(j,i) = 1
    MPConstraintProto* const one_of_ct = model.add_constraint();
    one_of_ct->set_lower_bound(1);
    one_of_ct->set_upper_bound(1);
  }
  for (NodeIndex node : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      const NodeIndex head = graph.Head(arc);
      if (node < head) {
        MPConstraintProto* one_of_ct = model.mutable_constraint(node);
        one_of_ct->add_var_index(variable_indices[arc]);
        one_of_ct->add_coefficient(1);
        one_of_ct = model.mutable_constraint(head);
        one_of_ct->add_var_index(variable_indices[arc]);
        one_of_ct->add_coefficient(1);
      }
    }
  }
#if defined(USE_SCIP)
  MPSolver scip_solver("MatchingWithSCIP",
                       MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
#elif defined(USE_CBC)
  MPSolver scip_solver("MatchingWithCBC",
                       MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
#endif
  // TODO(user): Glip performs significantly faster on very small problems;
  // investigate the necessity of switching solvers depending on model size.
  std::string error;
  scip_solver.LoadModelFromProto(model, &error);
  MPSolver::ResultStatus status = scip_solver.Solve();
  CHECK_EQ(status, MPSolver::OPTIMAL);
  MPSolutionResponse response;
  scip_solver.FillSolutionResponseProto(&response);
  std::vector<ArcIndex> matching;
  for (auto arc : variable_indices) {
    if (response.variable_value(arc.second) > .9) {
      DCHECK_GE(response.variable_value(arc.second), 1.0 - 1e-4);
      matching.push_back(arc.first);
    }
  }
  return matching;
}

template <typename CostType, typename ArcIndex, typename NodeIndex,
          typename CostFunction>
ChristofidesPathSolver<CostType, ArcIndex, NodeIndex, CostFunction>::
    ChristofidesPathSolver(NodeIndex num_nodes, CostFunction costs)
    : matching_(MatchingAlgorithm::MINIMAL_WEIGHT_MATCHING),
      graph_(num_nodes),
      costs_(std::move(costs)),
      tsp_cost_(0),
      solved_(false) {}

template <typename CostType, typename ArcIndex, typename NodeIndex,
          typename CostFunction>
CostType ChristofidesPathSolver<CostType, ArcIndex, NodeIndex,
                                CostFunction>::TravelingSalesmanCost() {
  if (!solved_) {
    Solve();
  }
  return tsp_cost_;
}

template <typename CostType, typename ArcIndex, typename NodeIndex,
          typename CostFunction>
std::vector<NodeIndex> ChristofidesPathSolver<
    CostType, ArcIndex, NodeIndex, CostFunction>::TravelingSalesmanPath() {
  if (!solved_) {
    Solve();
  }
  return tsp_path_;
}

template <typename CostType, typename ArcIndex, typename NodeIndex,
          typename CostFunction>
void ChristofidesPathSolver<CostType, ArcIndex, NodeIndex,
                            CostFunction>::Solve() {
  const NodeIndex num_nodes = graph_.num_nodes();
  tsp_path_.clear();
  tsp_cost_ = 0;
  if (num_nodes == 1) {
    tsp_path_ = {0, 0};
  }
  if (num_nodes <= 1) {
    return;
  }
  // Compute Minimum Spanning Tree.
  const std::vector<ArcIndex> mst =
      BuildPrimMinimumSpanningTree(graph_, [this](ArcIndex arc) {
        return costs_(graph_.Tail(arc), graph_.Head(arc));
      });
  // Detect odd degree nodes.
  std::vector<NodeIndex> degrees(num_nodes, 0);
  for (ArcIndex arc : mst) {
    degrees[graph_.Tail(arc)]++;
    degrees[graph_.Head(arc)]++;
  }
  std::vector<NodeIndex> odd_degree_nodes;
  for (int i = 0; i < degrees.size(); ++i) {
    if (degrees[i] % 2 != 0) {
      odd_degree_nodes.push_back(i);
    }
  }
  // Find minimum-weight perfect matching on odd-degree-node complete graph.
  // TODO(user): Make this code available as an independent algorithm.
  const NodeIndex reduced_size = odd_degree_nodes.size();
  DCHECK_NE(0, reduced_size);
  CompleteGraph<NodeIndex, ArcIndex> reduced_graph(reduced_size);
  std::vector<ArcIndex> closure_arcs;
  switch (matching_) {
    case MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING: {
      closure_arcs = ComputeMinimumWeightMatchingWithMIP(
          reduced_graph, [this, &reduced_graph,
                          &odd_degree_nodes](CompleteGraph<>::ArcIndex arc) {
            return costs_(odd_degree_nodes[reduced_graph.Tail(arc)],
                          odd_degree_nodes[reduced_graph.Head(arc)]);
          });
      break;
    }
    case MatchingAlgorithm::MINIMAL_WEIGHT_MATCHING: {
      // TODO(user): Cost caching was added and can gain up to 20% but
      // increases memory usage; see if we can avoid caching.
      std::vector<ArcIndex> ordered_arcs(reduced_graph.num_arcs());
      std::vector<CostType> ordered_arc_costs(reduced_graph.num_arcs(), 0);
      for (const ArcIndex arc : reduced_graph.AllForwardArcs()) {
        ordered_arcs[arc] = arc;
        ordered_arc_costs[arc] =
            costs_(odd_degree_nodes[reduced_graph.Tail(arc)],
                   odd_degree_nodes[reduced_graph.Head(arc)]);
      }
      std::sort(ordered_arcs.begin(), ordered_arcs.end(),
                [&ordered_arc_costs](ArcIndex arc_a, ArcIndex arc_b) {
                  return ordered_arc_costs[arc_a] < ordered_arc_costs[arc_b];
                });
      std::vector<bool> touched_nodes(reduced_size, false);
      for (ArcIndex arc_index = 0; closure_arcs.size() * 2 < reduced_size;
           ++arc_index) {
        const ArcIndex arc = ordered_arcs[arc_index];
        const NodeIndex tail = reduced_graph.Tail(arc);
        const NodeIndex head = reduced_graph.Head(arc);
        if (head != tail && !touched_nodes[tail] && !touched_nodes[head]) {
          touched_nodes[tail] = true;
          touched_nodes[head] = true;
          closure_arcs.push_back(arc);
        }
      }
      break;
    }
  }
  // Build Eulerian path on minimum spanning tree + closing edges from matching
  // and extract a solution to the Traveling Salesman from the path by skipping
  // duplicate nodes.
  ReverseArcListGraph<NodeIndex, ArcIndex> egraph(
      num_nodes, closure_arcs.size() + mst.size());
  for (ArcIndex arc : mst) {
    const NodeIndex tail = graph_.Tail(arc);
    const NodeIndex head = graph_.Head(arc);
    egraph.AddArc(tail, head);
  }
  for (ArcIndex arc : closure_arcs) {
    const NodeIndex tail = odd_degree_nodes[reduced_graph.Tail(arc)];
    const NodeIndex head = odd_degree_nodes[reduced_graph.Head(arc)];
    egraph.AddArc(tail, head);
  }
  std::vector<bool> touched(num_nodes, false);
  DCHECK(IsEulerianGraph(egraph));
  for (const NodeIndex node : BuildEulerianTourFromNode(egraph, 0)) {
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
