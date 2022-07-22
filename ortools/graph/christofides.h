// Copyright 2010-2022 Google LLC
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
// optimum when using minimum weight perfect matching in the matching phase.
// The complexity of the algorithm is dominated by the complexity of the
// matching algorithm: O(n^2 * log(n)) if minimal matching is used, or at least
// O(n^3) or O(nmlog(n)) otherwise, depending on the implementation of the
// perfect matching algorithm used, where n is the number of nodes and m is the
// number of edges of the subgraph induced by odd-degree nodes of the minimum
// spanning tree.

#ifndef OR_TOOLS_GRAPH_CHRISTOFIDES_H_
#define OR_TOOLS_GRAPH_CHRISTOFIDES_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/graph/eulerian_path.h"
#include "ortools/graph/graph.h"
#include "ortools/graph/minimum_spanning_tree.h"
#include "ortools/graph/perfect_matching.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/saturated_arithmetic.h"

namespace operations_research {

using ::util::CompleteGraph;

template <typename CostType, typename ArcIndex = int64_t,
          typename NodeIndex = int32_t,
          typename CostFunction = std::function<CostType(NodeIndex, NodeIndex)>>
class ChristofidesPathSolver {
 public:
  enum class MatchingAlgorithm {
    MINIMUM_WEIGHT_MATCHING,
#if defined(USE_CBC) || defined(USE_SCIP)
    MINIMUM_WEIGHT_MATCHING_WITH_MIP,
#endif  // defined(USE_CBC) || defined(USE_SCIP)
    MINIMAL_WEIGHT_MATCHING,
  };
  ChristofidesPathSolver(NodeIndex num_nodes, CostFunction costs);

  // Sets the matching algorithm to use. A minimum weight perfect matching
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

  // Runs the Christofides algorithm. Returns true if a solution was found,
  // false otherwise.
  bool Solve();

 private:
  int64_t SafeAdd(int64_t a, int64_t b) { return CapAdd(a, b); }

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

// Computes a minimum weight perfect matching on an undirected graph.
template <typename WeightFunctionType, typename GraphType>
absl::StatusOr<std::vector<
    std::pair<typename GraphType::NodeIndex, typename GraphType::NodeIndex>>>
ComputeMinimumWeightMatching(const GraphType& graph,
                             const WeightFunctionType& weight) {
  using ArcIndex = typename GraphType::ArcIndex;
  using NodeIndex = typename GraphType::NodeIndex;
  MinCostPerfectMatching matching(graph.num_nodes());
  for (NodeIndex tail : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(tail)) {
      const NodeIndex head = graph.Head(arc);
      // Adding both arcs is redudant for MinCostPerfectMatching.
      if (tail < head) {
        matching.AddEdgeWithCost(tail, head, weight(arc));
      }
    }
  }
  MinCostPerfectMatching::Status status = matching.Solve();
  if (status != MinCostPerfectMatching::OPTIMAL) {
    return absl::InvalidArgumentError("Perfect matching failed");
  }
  std::vector<std::pair<NodeIndex, NodeIndex>> match;
  for (NodeIndex tail : graph.AllNodes()) {
    const NodeIndex head = matching.Match(tail);
    if (tail < head) {  // Both arcs are matched for a given edge, we keep one.
      match.emplace_back(tail, head);
    }
  }
  return match;
}

#if defined(USE_CBC) || defined(USE_SCIP)
// Computes a minimum weight perfect matching on an undirected graph using a
// Mixed Integer Programming model.
// TODO(user): Handle infeasible cases if this algorithm is used outside of
// Christofides.
template <typename WeightFunctionType, typename GraphType>
absl::StatusOr<std::vector<
    std::pair<typename GraphType::NodeIndex, typename GraphType::NodeIndex>>>
ComputeMinimumWeightMatchingWithMIP(const GraphType& graph,
                                    const WeightFunctionType& weight) {
  using ArcIndex = typename GraphType::ArcIndex;
  using NodeIndex = typename GraphType::NodeIndex;
  MPModelProto model;
  model.set_maximize(false);
  // The model is composed of Boolean decision variables to select matching arcs
  // and constraints ensuring that each node appears in exactly one selected
  // arc. The objective is to minimize the sum of the weights of selected arcs.
  // It is assumed the graph is symmetrical.
  std::vector<int> variable_indices(graph.num_arcs(), -1);
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
        const int arc_var = variable_indices[arc];
        DCHECK_GE(arc_var, 0);
        MPConstraintProto* one_of_ct = model.mutable_constraint(node);
        one_of_ct->add_var_index(arc_var);
        one_of_ct->add_coefficient(1);
        one_of_ct = model.mutable_constraint(head);
        one_of_ct->add_var_index(arc_var);
        one_of_ct->add_coefficient(1);
      }
    }
  }
#if defined(USE_SCIP)
  MPSolver mp_solver("MatchingWithSCIP",
                     MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
#elif defined(USE_CBC)
  MPSolver mp_solver("MatchingWithCBC",
                     MPSolver::CBC_MIXED_INTEGER_PROGRAMMING);
#endif
  std::string error;
  mp_solver.LoadModelFromProto(model, &error);
  MPSolver::ResultStatus status = mp_solver.Solve();
  if (status != MPSolver::OPTIMAL) {
    return absl::InvalidArgumentError("MIP-based matching failed");
  }
  MPSolutionResponse response;
  mp_solver.FillSolutionResponseProto(&response);
  std::vector<std::pair<NodeIndex, NodeIndex>> matching;
  for (ArcIndex arc = 0; arc < variable_indices.size(); ++arc) {
    const int arc_var = variable_indices[arc];
    if (arc_var >= 0 && response.variable_value(arc_var) > .9) {
      DCHECK_GE(response.variable_value(arc_var), 1.0 - 1e-4);
      matching.emplace_back(graph.Tail(arc), graph.Head(arc));
    }
  }
  return matching;
}
#endif  // defined(USE_CBC) || defined(USE_SCIP)

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
    bool const ok = Solve();
    DCHECK(ok);
  }
  return tsp_cost_;
}

template <typename CostType, typename ArcIndex, typename NodeIndex,
          typename CostFunction>
std::vector<NodeIndex> ChristofidesPathSolver<
    CostType, ArcIndex, NodeIndex, CostFunction>::TravelingSalesmanPath() {
  if (!solved_) {
    const bool ok = Solve();
    DCHECK(ok);
  }
  return tsp_path_;
}

template <typename CostType, typename ArcIndex, typename NodeIndex,
          typename CostFunction>
bool ChristofidesPathSolver<CostType, ArcIndex, NodeIndex,
                            CostFunction>::Solve() {
  const NodeIndex num_nodes = graph_.num_nodes();
  tsp_path_.clear();
  tsp_cost_ = 0;
  if (num_nodes == 1) {
    tsp_path_ = {0, 0};
  }
  if (num_nodes <= 1) {
    return true;
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
  std::vector<std::pair<NodeIndex, NodeIndex>> closure_arcs;
  switch (matching_) {
    case MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING: {
      auto result = ComputeMinimumWeightMatching(
          reduced_graph, [this, &reduced_graph,
                          &odd_degree_nodes](CompleteGraph<>::ArcIndex arc) {
            return costs_(odd_degree_nodes[reduced_graph.Tail(arc)],
                          odd_degree_nodes[reduced_graph.Head(arc)]);
          });
      if (!result.ok()) {
        return false;
      }
      result->swap(closure_arcs);
      break;
    }
#if defined(USE_CBC) || defined(USE_SCIP)
    case MatchingAlgorithm::MINIMUM_WEIGHT_MATCHING_WITH_MIP: {
      auto result = ComputeMinimumWeightMatchingWithMIP(
          reduced_graph, [this, &reduced_graph,
                          &odd_degree_nodes](CompleteGraph<>::ArcIndex arc) {
            return costs_(odd_degree_nodes[reduced_graph.Tail(arc)],
                          odd_degree_nodes[reduced_graph.Head(arc)]);
          });
      if (!result.ok()) {
        return false;
      }
      result->swap(closure_arcs);
      break;
    }
#endif  // defined(USE_CBC) || defined(USE_SCIP)
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
          closure_arcs.emplace_back(tail, head);
        }
      }
      break;
    }
  }
  // Build Eulerian path on minimum spanning tree + closing edges from matching
  // and extract a solution to the Traveling Salesman from the path by skipping
  // duplicate nodes.
  ::util::ReverseArcListGraph<NodeIndex, ArcIndex> egraph(
      num_nodes, closure_arcs.size() + mst.size());
  for (ArcIndex arc : mst) {
    egraph.AddArc(graph_.Tail(arc), graph_.Head(arc));
  }
  for (const auto arc : closure_arcs) {
    egraph.AddArc(odd_degree_nodes[arc.first], odd_degree_nodes[arc.second]);
  }
  std::vector<bool> touched(num_nodes, false);
  DCHECK(IsEulerianGraph(egraph));
  for (const NodeIndex node : BuildEulerianTourFromNode(egraph, 0)) {
    if (touched[node]) continue;
    touched[node] = true;
    tsp_cost_ = SafeAdd(tsp_cost_,
                        tsp_path_.empty() ? 0 : costs_(tsp_path_.back(), node));
    tsp_path_.push_back(node);
  }
  tsp_cost_ =
      SafeAdd(tsp_cost_, tsp_path_.empty() ? 0 : costs_(tsp_path_.back(), 0));
  tsp_path_.push_back(0);
  solved_ = true;
  return true;
}
}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_CHRISTOFIDES_H_
