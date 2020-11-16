// Copyright 2010-2018 Google LLC
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

// An implementation of the Held-Karp symmetric Traveling Salesman (TSP) lower
// bound algorithm, inspired by "Estimating the Held-Karp lower bound for the
// geometric TSP" by Christine L. Valenzuela and Antonia J. Jones, European
// Journal of Operational Research, Volume 102, Issue 1, 1 October 1997,
// Pages 157-175.
//
// The idea is to compute minimum 1-trees to evaluate a lower bound to the
// corresponding TSP. A minimum 1-tree is a minimum spanning tree on all nodes
// but one, to which are added the two shortest edges from the left-out node to
// the nodes of the spanning tree. The sum of the cost of the edges of the
// minimum 1-tree is a lower bound to the cost of the TSP.
// In order to improve (increase) this lower bound, the idea is to add weights
// to each nodes, weights which are added to the cost function used when
// computing the 1-tree. If weight[i] is the weight of node i, the cost function
// therefore becomes weighed_cost(i,j) = cost(i,j) + weight[i] + weight[j]. One
// can see that w = weighed_cost(minimum 1-tree) - Sum(2 * weight[i])
//                = cost(minimum 1-tree) + Sum(weight[i] * (degree[i] - 2))
// is a valid lower bound to the TSP:
// 1) let T be the set of 1-trees on the nodes;
// 2) let U be the set of tours on the nodes; U is a subset of T (tours are
//    1-trees with all degrees equal to 2), therefore:
//      min(t in T) Cost(t) <= min(t in U) Cost(t)
//    and
//      min(t in T) WeighedCost(t) <= min(t in U) WeighedCost(t)
// 3) weighed_cost(i,j) = cost(i,j) + weight[i] + weight[j], therefore:
//      for all t in T, WeighedCost(t) = Cost(t) + Sum(weight[i] * degree[i])
//    and
//      for all i in U, WeighedCost(t) = Cost(t) + Sum(weight[i] * 2)
// 4) let t* in U s.t. WeighedCost(t*) = min(t in U) WeighedCost(t), therefore:
//      min(t in T)  (Cost(t) + Sum(weight[i] * degree[i]))
//      <= Cost(t*) + Sum(weight[i] * 2)
//    and
//      min(t in T)  (Cost(t) + Sum(weight[i] * (degree[i] - 2))) <= Cost(t*)
//    and
//      cost(minimum 1-tree) + Sum(weight[i] * (degree[i] - 2)) <= Cost(t*)
//    and
//      w <= Cost(t*)
// 5) because t* is also the tour minimizing Cost(t) with t in U (weights do not
//    affect the optimality of a tour), Cost(t*) is the cost of the optimal
//    solution to the TSP and w is a lower bound to this cost.
//
// The best lower bound is the one for which weights maximize w. Intuitively as
// degrees get closer to 2 the minimum 1-trees gets closer to a tour.
//
// At each iteration m, weights are therefore updated as follows:
//   weight(m+1)[i] = weight(m)[i] + step(m) * (degree(m)[i] - 2)
// where degree(m)[i] is the degree of node i in the 1-tree at iteration i,
// step(m) is a subgradient optimization step.
//
// This implementation uses two variants of Held-Karp's initial subgradient
// optimization iterative estimation approach described in "The
// traveling-salesman problem and minimum spanning trees: Part I and II", by
// Michael Held and Richard M. Karp, Operations Research Vol. 18,
// No. 6 (Nov. - Dec., 1970), pp. 1138-1162 and Mathematical Programming (1971).
//
// The first variant comes from Volgenant, T., and Jonker, R. (1982), "A branch
// and bound algorithm for the symmetric traveling salesman problem based on the
// 1-tree relaxation", European Journal of Operational Research. 9:83-89.".
// It suggests using
//   step(m) = (1.0 * (m - 1) * (2 * M - 5) / (2 * (M - 1))) * step1
//           - (m - 2) * step1
//           + (0.5 * (m - 1) * (m - 2) / ((M - 1) * (M - 2))) * step1
// where M is the maximum number of iterations and step1 is initially set to
// L / (2 * number of nodes), where L is the un-weighed cost of the 1-tree;
// step1 is updated each time a better w is found. The intuition is to have a
// positive decreasing step which is equal to 0 after M iterations; Volgenant
// and Jonker suggest that:
//   step(m) - 2 * step(m-1) + t(m-2) = constant,
//   step(M) = 0
// and
//   step(1) - step(2) = 3 * (step(M-1) - step(M)).
// The step(m) formula above derives from this recursive formulation.
// This is the default algorithm used in this implementation.
//
// The second variant comes from Held, M., Wolfe, P., and Crowder, H. P. (1974),
// "Validation of subgradient optimization", Mathematical Programming 6:62-88.
// It derives from the original Held-Karp formulation:
//   step(m) = lambda(m) * (wlb - w(m)) / Sum((degree[i] - 2)^2),
// where wlb is a lower bound to max(w(m)) and lambda(m) in [0, 2].
// Help-Karp prove that
// if w(m') > w(m) and 0 < step < 2 * (w(m') - w(m))/norm(degree(m) - 2)^2,
// then weight(m+1) is closer to w' than w from which they derive the above
// formula.
// Held-Wolfe-Crowder show that using an overestimate UB is as effective as
// using the underestimate wlb while UB is easier to compute. The resulting
// formula is:
//   step(m) = lambda(m) * (UB - w(m)) / Sum((degree[i] - 2)^2),
// where UB is an upper bound to the TSP (here computed with the Christofides
// algorithm), and lambda(m) in [0, 2] initially set to 2. Held-Wolfe-Crowder
// suggest running the algorithm for M = 2 * number of nodes iterations, then
// dividing lambda and M by 2 until M is small enough (less than 2 in this
// implementation).
//
// To speed up the computation, minimum spanning trees are actually computed on
// a graph limited to the nearest neighbors of each node. Valenzuela-Jones 1997
// experiments have shown that this does not harm the lower bound computation
// significantly. At the end of the algorithm a last iteration is run on the
// complete graph to ensure the bound is correct (the cost of a minimum 1-tree
// on a partial graph is an upper bound to the one on a complete graph).
//
// Usage:
// std::function<int64(int,int)> cost_function =...;
// const double lower_bound =
//     ComputeOneTreeLowerBound(number_of_nodes, cost_function);
// where number_of_nodes is the number of nodes in the TSP and cost_function
// is a function returning the cost between two nodes.

#ifndef OR_TOOLS_GRAPH_ONE_TREE_LOWER_BOUND_H_
#define OR_TOOLS_GRAPH_ONE_TREE_LOWER_BOUND_H_

#include <math.h>

#include <limits>
#include <set>

#include "ortools/base/integral_types.h"
#include "ortools/graph/christofides.h"
#include "ortools/graph/minimum_spanning_tree.h"

namespace operations_research {

// Implementation of algorithms computing Held-Karp bounds. They have to provide
// the following methods:
// - bool Next(): returns false when the algorithm must stop;
// - double GetStep(): returns the current step computed by the algorithm;
// - void OnOneTree(CostType one_tree_cost,
//                  double w,
//                  const std::vector<int>& degrees):
//     called each time a new minimum 1-tree is computed;
//     - one_tree_cost: the un-weighed cost of the 1-tree,
//     - w the current value of w,
//     - degrees: the degree of nodes in the 1-tree.
// - OnNewWMax(CostType one_tree_cost): called when a better value of w is
//     found, one_tree_cost being the un-weighed cost of the corresponding
//     minimum 1-tree.

// Implementation of the Volgenant Jonker algorithm (see the comments at the
// head of the file for explanations).
template <typename CostType>
class VolgenantJonkerEvaluator {
 public:
  VolgenantJonkerEvaluator(int number_of_nodes, int max_iterations)
      : step1_initialized_(false),
        step1_(0),
        iteration_(0),
        max_iterations_(max_iterations > 0 ? max_iterations
                                           : MaxIterations(number_of_nodes)),
        number_of_nodes_(number_of_nodes) {}

  bool Next() { return iteration_++ < max_iterations_; }

  double GetStep() const {
    return (1.0 * (iteration_ - 1) * (2 * max_iterations_ - 5) /
            (2 * (max_iterations_ - 1))) *
               step1_ -
           (iteration_ - 2) * step1_ +
           (0.5 * (iteration_ - 1) * (iteration_ - 2) /
            ((max_iterations_ - 1) * (max_iterations_ - 2))) *
               step1_;
  }

  void OnOneTree(CostType one_tree_cost, double w,
                 const std::vector<int>& degrees) {
    if (!step1_initialized_) {
      step1_initialized_ = true;
      UpdateStep(one_tree_cost);
    }
  }

  void OnNewWMax(CostType one_tree_cost) { UpdateStep(one_tree_cost); }

 private:
  // Automatic computation of the number of iterations based on empirical
  // results given in Valenzuela-Jones 1997.
  static int MaxIterations(int number_of_nodes) {
    return static_cast<int>(28 * std::pow(number_of_nodes, 0.62));
  }

  void UpdateStep(CostType one_tree_cost) {
    step1_ = one_tree_cost / (2 * number_of_nodes_);
  }

  bool step1_initialized_;
  double step1_;
  int iteration_;
  const int max_iterations_;
  const int number_of_nodes_;
};

// Implementation of the Held-Wolfe-Crowder algorithm (see the comments at the
// head of the file for explanations).
template <typename CostType, typename CostFunction>
class HeldWolfeCrowderEvaluator {
 public:
  HeldWolfeCrowderEvaluator(int number_of_nodes, const CostFunction& cost)
      : iteration_(0),
        number_of_iterations_(2 * number_of_nodes),
        upper_bound_(0),
        lambda_(2.0),
        step_(0) {
    // TODO(user): Improve upper bound with some local search; tighter upper
    // bounds lead to faster convergence.
    ChristofidesPathSolver<CostType, int64, int, CostFunction> solver(
        number_of_nodes, cost);
    upper_bound_ = solver.TravelingSalesmanCost();
  }

  bool Next() {
    const int min_iterations = 2;
    if (iteration_ >= number_of_iterations_) {
      number_of_iterations_ /= 2;
      if (number_of_iterations_ < min_iterations) return false;
      iteration_ = 0;
      lambda_ /= 2;
    } else {
      ++iteration_;
    }
    return true;
  }

  double GetStep() const { return step_; }

  void OnOneTree(CostType one_tree_cost, double w,
                 const std::vector<int>& degrees) {
    double norm = 0;
    for (int degree : degrees) {
      const double delta = degree - 2;
      norm += delta * delta;
    }
    step_ = lambda_ * (upper_bound_ - w) / norm;
  }

  void OnNewWMax(CostType one_tree_cost) {}

 private:
  int iteration_;
  int number_of_iterations_;
  CostType upper_bound_;
  double lambda_;
  double step_;
};

// Computes the nearest neighbors of each node for the given cost function.
// The ith element of the returned vector contains the indices of the nearest
// nodes to node i. Note that these indices contain the number_of_neighbors
// nearest neighbors as well as all the nodes for which i is a nearest
// neighbor.
template <typename CostFunction>
std::set<std::pair<int, int>> NearestNeighbors(int number_of_nodes,
                                               int number_of_neighbors,
                                               const CostFunction& cost) {
  using CostType = decltype(cost(0, 0));
  std::set<std::pair<int, int>> nearest;
  for (int i = 0; i < number_of_nodes; ++i) {
    std::vector<std::pair<CostType, int>> neighbors;
    neighbors.reserve(number_of_nodes - 1);
    for (int j = 0; j < number_of_nodes; ++j) {
      if (i != j) {
        neighbors.emplace_back(cost(i, j), j);
      }
    }
    int size = neighbors.size();
    if (number_of_neighbors < size) {
      std::nth_element(neighbors.begin(),
                       neighbors.begin() + number_of_neighbors - 1,
                       neighbors.end());
      size = number_of_neighbors;
    }
    for (int j = 0; j < size; ++j) {
      nearest.insert({i, neighbors[j].second});
      nearest.insert({neighbors[j].second, i});
    }
  }
  return nearest;
}

// Let G be the complete graph on nodes in [0, number_of_nodes - 1]. Adds arcs
// from the minimum spanning tree of G to the arcs set argument.
template <typename CostFunction>
void AddArcsFromMinimumSpanningTree(int number_of_nodes,
                                    const CostFunction& cost,
                                    std::set<std::pair<int, int>>* arcs) {
  util::CompleteGraph<int, int> graph(number_of_nodes);
  const std::vector<int> mst =
      BuildPrimMinimumSpanningTree(graph, [&cost, &graph](int arc) {
        return cost(graph.Tail(arc), graph.Head(arc));
      });
  for (int arc : mst) {
    arcs->insert({graph.Tail(arc), graph.Head(arc)});
    arcs->insert({graph.Head(arc), graph.Tail(arc)});
  }
}

// Returns the index of the node in graph which minimizes cost(node, source)
// with the constraint that accept(node) is true.
template <typename CostFunction, typename GraphType, typename AcceptFunction>
int GetNodeMinimizingEdgeCostToSource(const GraphType& graph, int source,
                                      const CostFunction& cost,
                                      AcceptFunction accept) {
  int best_node = -1;
  double best_edge_cost = 0;
  for (const auto node : graph.AllNodes()) {
    if (accept(node)) {
      const double edge_cost = cost(node, source);
      if (best_node == -1 || edge_cost < best_edge_cost) {
        best_node = node;
        best_edge_cost = edge_cost;
      }
    }
  }
  return best_node;
}

// Computes a 1-tree for the given graph, cost function and node weights.
// Returns the degree of each node in the 1-tree and the un-weighed cost of the
// 1-tree.
template <typename CostFunction, typename GraphType, typename CostType>
std::vector<int> ComputeOneTree(const GraphType& graph,
                                const CostFunction& cost,
                                const std::vector<double>& weights,
                                const std::vector<int>& sorted_arcs,
                                CostType* one_tree_cost) {
  const auto weighed_cost = [&cost, &weights](int from, int to) {
    return cost(from, to) + weights[from] + weights[to];
  };
  // Compute MST on graph.
  std::vector<int> mst;
  if (!sorted_arcs.empty()) {
    mst = BuildKruskalMinimumSpanningTreeFromSortedArcs<GraphType>(graph,
                                                                   sorted_arcs);
  } else {
    mst = BuildPrimMinimumSpanningTree<GraphType>(
        graph, [&weighed_cost, &graph](int arc) {
          return weighed_cost(graph.Tail(arc), graph.Head(arc));
        });
  }
  std::vector<int> degrees(graph.num_nodes() + 1, 0);
  *one_tree_cost = 0;
  for (int arc : mst) {
    degrees[graph.Head(arc)]++;
    degrees[graph.Tail(arc)]++;
    *one_tree_cost += cost(graph.Tail(arc), graph.Head(arc));
  }
  // Add 2 cheapest edges from the nodes in the graph to the extra node not in
  // the graph.
  const int extra_node = graph.num_nodes();
  const auto update_one_tree = [extra_node, one_tree_cost, &degrees,
                                &cost](int node) {
    *one_tree_cost += cost(node, extra_node);
    degrees.back()++;
    degrees[node]++;
  };
  const int node = GetNodeMinimizingEdgeCostToSource(
      graph, extra_node, weighed_cost,
      [extra_node](int n) { return n != extra_node; });
  update_one_tree(node);
  update_one_tree(GetNodeMinimizingEdgeCostToSource(
      graph, extra_node, weighed_cost,
      [extra_node, node](int n) { return n != extra_node && n != node; }));
  return degrees;
}

// Computes the lower bound of a TSP using a given subgradient algorithm.
template <typename CostFunction, typename Algorithm>
double ComputeOneTreeLowerBoundWithAlgorithm(int number_of_nodes,
                                             int nearest_neighbors,
                                             const CostFunction& cost,
                                             Algorithm* algorithm) {
  if (number_of_nodes < 2) return 0;
  if (number_of_nodes == 2) return cost(0, 1) + cost(1, 0);
  using CostType = decltype(cost(0, 0));
  auto nearest = NearestNeighbors(number_of_nodes - 1, nearest_neighbors, cost);
  // Ensure nearest arcs result in a connected graph by adding arcs from the
  // minimum spanning tree; this will add arcs which are likely to be "good"
  // 1-tree arcs.
  AddArcsFromMinimumSpanningTree(number_of_nodes - 1, cost, &nearest);
  util::ListGraph<int, int> graph(number_of_nodes - 1, nearest.size());
  for (const auto& arc : nearest) {
    graph.AddArc(arc.first, arc.second);
  }
  std::vector<double> weights(number_of_nodes, 0);
  std::vector<double> best_weights(number_of_nodes, 0);
  double max_w = -std::numeric_limits<double>::infinity();
  double w = 0;
  // Iteratively compute lower bound using a partial graph.
  while (algorithm->Next()) {
    CostType one_tree_cost = 0;
    const std::vector<int> degrees =
        ComputeOneTree(graph, cost, weights, {}, &one_tree_cost);
    algorithm->OnOneTree(one_tree_cost, w, degrees);
    w = one_tree_cost;
    for (int j = 0; j < number_of_nodes; ++j) {
      w += weights[j] * (degrees[j] - 2);
    }
    if (w > max_w) {
      max_w = w;
      best_weights = weights;
      algorithm->OnNewWMax(one_tree_cost);
    }
    const double step = algorithm->GetStep();
    for (int j = 0; j < number_of_nodes; ++j) {
      weights[j] += step * (degrees[j] - 2);
    }
  }
  // Compute lower bound using the complete graph on the best weights. This is
  // necessary as the MSTs computed on nearest neighbors is not guaranteed to
  // lead to a lower bound.
  util::CompleteGraph<int, int> complete_graph(number_of_nodes - 1);
  CostType one_tree_cost = 0;
  // TODO(user): We are not caching here since this would take O(n^2) memory;
  // however the Kruskal algorithm will expand all arcs also consuming O(n^2)
  // memory; investigate alternatives to expanding all arcs (Prim's algorithm).
  const std::vector<int> degrees =
      ComputeOneTree(complete_graph, cost, best_weights, {}, &one_tree_cost);
  w = one_tree_cost;
  for (int j = 0; j < number_of_nodes; ++j) {
    w += best_weights[j] * (degrees[j] - 2);
  }
  return w;
}

// Parameters to configure the computation of the TSP lower bound.
struct TravelingSalesmanLowerBoundParameters {
  enum Algorithm {
    VolgenantJonker,
    HeldWolfeCrowder,
  };
  // Subgradient algorithm to use to compute the TSP lower bound.
  Algorithm algorithm = VolgenantJonker;
  // Number of iterations to use in the Volgenant-Jonker algorithm. Overrides
  // automatic iteration computation if positive.
  int volgenant_jonker_iterations = 0;
  // Number of nearest neighbors to consider in the miminum spanning trees.
  int nearest_neighbors = 40;
};

// Computes the lower bound of a TSP using given parameters.
template <typename CostFunction>
double ComputeOneTreeLowerBoundWithParameters(
    int number_of_nodes, const CostFunction& cost,
    const TravelingSalesmanLowerBoundParameters& parameters) {
  using CostType = decltype(cost(0, 0));
  switch (parameters.algorithm) {
    case TravelingSalesmanLowerBoundParameters::VolgenantJonker: {
      VolgenantJonkerEvaluator<CostType> algorithm(
          number_of_nodes, parameters.volgenant_jonker_iterations);
      return ComputeOneTreeLowerBoundWithAlgorithm(
          number_of_nodes, parameters.nearest_neighbors, cost, &algorithm);
      break;
    }
    case TravelingSalesmanLowerBoundParameters::HeldWolfeCrowder: {
      HeldWolfeCrowderEvaluator<CostType, CostFunction> algorithm(
          number_of_nodes, cost);
      return ComputeOneTreeLowerBoundWithAlgorithm(
          number_of_nodes, parameters.nearest_neighbors, cost, &algorithm);
    }
    default:
      LOG(ERROR) << "Unsupported algorithm: " << parameters.algorithm;
      return 0;
  }
}

// Computes the lower bound of a TSP using default parameters (Volgenant-Jonker
// algorithm, 200 iterations and 40 nearest neighbors) which have turned out to
// give good results on the TSPLIB.
template <typename CostFunction>
double ComputeOneTreeLowerBound(int number_of_nodes, const CostFunction& cost) {
  TravelingSalesmanLowerBoundParameters parameters;
  return ComputeOneTreeLowerBoundWithParameters(number_of_nodes, cost,
                                                parameters);
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_ONE_TREE_LOWER_BOUND_H_
