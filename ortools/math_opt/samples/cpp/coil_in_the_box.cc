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

// Solves the coil in the box problem, a variant of the snake in the box
// problem, see https://en.wikipedia.org/wiki/Snake-in-the-box.
//
// The problem is to find the longest cycle traversing a subset of the corners
// of the n-dimensional hypercube, such that for each corner you visit, you
// visit at most two adjacent corners. The cube has 2^n corners, giving an
// upper bound on the longest cycle length. We use a prize collecting TSP
// like MIP model to solve the problem below. We introduce the "Medusa cuts"
// for this problem, linear constraints that improve the LP relaxation without
// cutting off any integer points, that can be optionally included in the model.
//
// The best known solutions for coil in a box as a function of n are:
//
// n  | best solution | best bound
// ---|---------------|------------
// 2  | 4             | 4
// 3  | 6             | 6
// 4  | 8             | 8
// 5  | 14            | 14
// 6  | 26            | 26
// 7  | 48            | 48
// 8  | 96            | 96
// 9  | 188           | ?
// 10 | 366           | ?
// 11 | 692           | ?
// 12 | 1344          | ?
//
// Below, we show results with the Medusa cuts on (and --fix). We are optimal
// up to n=6 (45s). All solves were run locally to avoid callback RPC overhead.
//
// clang-format off
// n  | best solution | lp bound    | 20m bound     | logs
// ---|---------------|-------------|--------------|-------
// 6  | 26            | 30.8        | 26 (6.5s)    | https://paste.googleplex.com/5930768672751616
// 7  | 46            | 59.6        | 57.1 (5m)    | https://paste.googleplex.com/6517880031805440
// 8  | 78            | 116.2 (21s) | 114.9        | https://paste.googleplex.com/4818696077574144
// 9  | 86            | 228.1 (6m)  | 227.7        | https://paste.googleplex.com/4856789015330816
// clang-format on
//
// For n=9, the problem (post presolve) has ~3k columns, ~12k rows, and ~150k
// nonzeros.
//
// clang-format off
// Without the Medusa cuts, we better primal solutions but worse bounds.
// n  | best solution | lp bound | 20m bound  | logs
// ---|---------------|-----------------------------
// 6  | 26 (<1s)      | 35.2     | 26 (17s)   | https://paste.googleplex.com/5421390584610816
// 7  | 48 (1m)       | 70       | 63.7 (5m)  | https://paste.googleplex.com/4812751909945344
// 8  | 88            | 140.8    | 134.5      | https://paste.googleplex.com/6498606298955776
// 9  | 158           | 281      | 277        | https://paste.googleplex.com/5250122858102784
// clang-format on
//
// Notes for someone trying to do better:
//  * Using only 2-Medusa cuts (edit the code) has almost no effect (tiny
//    improvement on the root LP relaxation).
//  * The gurobi symmetry breaking does not seem to work, even when --fix=false
//    and the "Symmetry" parameter is set to 2. The results are much worse
//    whenever --fix=false.
//  * For smaller instances, the remote callbacks are painful (182/200s in
//    callback for n=6, medusa=false), but for large instances the RPC overhead
//    does not matter.
//  * With Medusa cuts, we have many more constraints than variables, and the LP
//    becomes slow for large instances.
//  * No attempt was made to optimize the parameters of QuickSeparate() below.
//
// Our MIP model is as follows.
//
// Data:
//  * n: the dimension of the hypercube
//  * G = (V, E): the hypercube as a graph, with vertices at the 2^n corners and
//    edges between the corners differing in only one coordinate.
//  * E(v) subset E: the edges where v is an endpoint.
//  * N(v) subset V: the nodes neighboring v.
//  * Cut(S) subset E: edges with exactly one endpoint in S.
//
// Variables:
//   * y_e: do we use edge e in E
//   * x_v: do we visit vertex v
//
// Model:
//   max   sum_{e in E} y_e
//   s.t.  sum_{e in E(v)} y_e = 2 x_v                  for all v in V
//         x_v + x_w <= 1 + y_{v,w}                     for all (v, w) in E
//         sum_{e in Cut(S)} y_e >= 2 (x_k + x_l - 1)   for all S subset N
//                                                              3 < |S| < |N|
//                                                              k in S,
//                                                              l not in S
//
// The first constraint (the degree constraint) says to use exactly two edges if
// we visit the node, and none otherwise. The second constraint enforces the
// invalidation of adjacent corners that are not visited directly from a node,
// it requires that we only visit two adjacent nodes if we include the edge
// between them.
//
// The final constraint is the "cutset" constraint from the PC-TSP, which
// ensures that all arcs selected form a single single, rather than multiple
// cycles (if you select a node inside S and outside S, there must be 2 units of
// flow over the cut). For more details on the implementation, see:
//   * cs/ortools/math_opt/models/tsp/math_opt_circuit.h
//   * cs/ortools/math_opt/models/tsp/circuit_constraint.h
// In particular, when --fix is set (so we know the first node is visited),
// there is a slightly simpler formulation of the constraint (see
// math_opt_circuit.h).
//
// The cutset constraints are separated exactly on integer solutions by
// traversing the selected arcs are looking for cycles. They are separated
// approximately for fractional solutions by rounding the solution and then
// checking each connected component.
//
// We can strengthen this constraint with the "Medusa cuts" as follows. Let
// d_v = 2*x_v (don't create the variable, just use a linear expression) give
// the degree of node v (we need this to make the constraints sparse). The cuts
// are parameterized by a dimension d < n. Take all n choose d hypercubes of
// size d that are a subset the n dimension hypercube. For each cube, take the
// internal edges (Medusa's head) and the edges on the cut with one exactly
// endpoint in the cube (Medusa's snakes). For d=2, we can use at most 4 of
// these edges, and for d=3, we can use at most 6 of these edges (this was shown
// by enumeration/cases, a clean proof would be an improvement). We can write
// these constraints as:
//   sum_{v in Head} d_v - sum_{e in Head} y_e <= UB(d)
// Using O(2^d) nonzeros per cut.
#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "base/context.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/strong_int.h"
#include "ortools/base/strong_vector.h"
#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/remote_solve.h"
#include "ortools/math_opt/cpp/stubby_remote_streaming_solve.h"
#include "ortools/math_opt/models/tsp/circuit_constraint.h"
#include "ortools/math_opt/models/tsp/math_opt_circuit.h"
#include "ortools/math_opt/rpc.stubby.h"

ABSL_FLAG(int, dim, 4, "Dimension of the hyper cube to solve in.");
ABSL_FLAG(bool, gurobi, false, "Use gurobi instead of SCIP.");
ABSL_FLAG(std::optional<int>, threads, std::nullopt,
          "How many threads, the solver default if unset.");
ABSL_FLAG(bool, medusa, false, "Add medusa cuts to the formulation");
ABSL_FLAG(bool, fix, false, "break symmetry by fixing variables");
ABSL_FLAG(bool, remote, false, "Solve remotely with stubby");
ABSL_FLAG(absl::Duration, time_limit, absl::Minutes(5),
          "A limit on how long to run");

namespace {

namespace math_opt = ::operations_research::math_opt;
namespace models = ::operations_research::models;
using Node = models::CircuitConstraint::Node;
using Edge = models::CircuitConstraint::Edge;
using ::util_intops::MakeStrongIntRange;

// The cube has 2**n nodes, represented by the integers [0..2**n). The binary
// encoding of each node says which coordinates are zero and which are one. This
// function flips the dth bit, getting a neighboring node.
Node Neighbor(const Node v, int d) { return Node(v.value() ^ (1 << d)); }

absl::StatusOr<math_opt::SolveResult> RunSolve(const math_opt::Model& model,
                                               math_opt::SolverType solver_type,
                                               math_opt::SolveArguments args) {
  if (absl::GetFlag(FLAGS_remote)) {
    args.message_callback = math_opt::PrinterMessageCallback();
    const base::WithDeadline deadline(absl::Now() +
                                      3 * absl::GetFlag(FLAGS_time_limit));
    ASSIGN_OR_RETURN(const std::unique_ptr<math_opt::SolveService> stub,
                     math_opt::SolveServerStub());
    return math_opt::StubbyRemoteStreamingSolve(stub.get(), model, solver_type,
                                                args);
  }
  return math_opt::Solve(model, solver_type, std::move(args));
}

absl::StatusOr<math_opt::CallbackResult> OnMipSolution(
    const models::MathOptCircuit& circuit, const math_opt::CallbackData& data) {
  CHECK(data.solution.has_value());
  math_opt::CallbackResult result;

  ASSIGN_OR_RETURN(const std::vector<models::MathOptCircuit::Cutset> cuts,
                   circuit.ExactSeparateIntegerSolution(*data.solution));
  for (const models::MathOptCircuit::Cutset& cutset : cuts) {
    result.AddLazyConstraint(circuit.CreateCutsetConstraint(cutset));
  }
  return result;
}

absl::StatusOr<math_opt::CallbackResult> OnMipNode(
    const models::MathOptCircuit& circuit, const math_opt::CallbackData& data) {
  math_opt::CallbackResult result;
  if (!data.solution.has_value()) {
    return result;
  }
  // The values of edge_threshold and min_violation should be in (0, 1). They
  // were not tuned for this problem, values that worked well on other problems
  // were reused.
  ASSIGN_OR_RETURN(const std::vector<models::MathOptCircuit::Cutset> cuts,
                   circuit.QuickSeparate(*data.solution,
                                         /*edge_threshold=*/0.5,
                                         /*min_violation*/ 0.05));

  for (const models::MathOptCircuit::Cutset& cutset : cuts) {
    result.AddLazyConstraint(circuit.CreateCutsetConstraint(cutset));
  }
  return result;
}

absl::Status Main() {
  int dim = absl::GetFlag(FLAGS_dim);
  Node num_nodes{std::pow(2, dim)};
  std::vector<Edge> edges;
  for (Node v : MakeStrongIntRange(num_nodes)) {
    for (int d = 0; d < dim; ++d) {
      const Node w = Neighbor(v, d);
      if (v < w) {
        edges.push_back({v, w});
      }
    }
  }
  // All nodes are optional.
  util_intops::StrongVector<Node, bool> must_be_visited(num_nodes);
  // To break symmetry, we can fix a few variables.
  if (absl::GetFlag(FLAGS_fix)) {
    Node start{0};
    Node one = Neighbor(start, 0);
    Node two = Neighbor(one, 1);
    must_be_visited[start] = true;
    must_be_visited[one] = true;
    must_be_visited[two] = true;
  }
  const models::CircuitConstraint circuit(must_be_visited, /*directed=*/false,
                                          edges);

  math_opt::Model model;
  const models::MathOptCircuit math_circuit(circuit, &model);
  // We can also fix some edges.
  if (absl::GetFlag(FLAGS_fix)) {
    Node start{0};
    Node one = Neighbor(start, 0);
    Node two = Neighbor(one, 1);
    model.set_lower_bound(math_circuit.edge_var_or_die({start, one}), 1.0);
    model.set_lower_bound(math_circuit.edge_var_or_die({one, two}), 1.0);
  }
  math_opt::LinearExpression nodes_hit;
  for (Node v : MakeStrongIntRange(num_nodes)) {
    nodes_hit += math_circuit.node(v);
    for (int d = 0; d < dim; ++d) {
      const Node w = Neighbor(v, d);
      if (v < w) {
        model.AddLinearConstraint(math_circuit.node(v) + math_circuit.node(w) <=
                                  1 + math_circuit.edge_var_or_die({v, w}));
      }
    }
  }
  if (absl::GetFlag(FLAGS_medusa)) {
    util_intops::StrongVector<Node, math_opt::LinearExpression> num_adj;
    for (const Node n : MakeStrongIntRange(num_nodes)) {
      num_adj.push_back(2 * math_circuit.node(n));
    }
    // 2-Medusa cuts
    for (Node v1 : MakeStrongIntRange(num_nodes)) {
      for (int i = 0; i < dim; ++i) {
        for (int j = i + 1; j < dim; ++j) {
          Node v2 = Neighbor(v1, i);
          Node v3 = Neighbor(v1, j);
          Node v4 = Neighbor(v2, j);
          if (v1 > v2 && v1 > v3 && v1 > v4) {
            math_opt::LinearExpression arcs =
                num_adj[v1] + num_adj[v2] + num_adj[v3] + num_adj[v4];
            for (const auto edge :
                 std::vector<Edge>({{v1, v2}, {v1, v3}, {v2, v4}, {v3, v4}})) {
              arcs -= math_circuit.edge_var_or_die(edge);
            }
            model.AddLinearConstraint(arcs <= 4.0);
          }
        }
      }
    }
    // 3-Medusa cuts
    for (Node flip_none : MakeStrongIntRange(num_nodes)) {
      for (int i = 0; i < dim; ++i) {
        for (int j = i + 1; j < dim; ++j) {
          for (int j = i + 1; j < dim; ++j) {
            for (int k = j + 1; k < dim; ++k) {
              Node flip_i = Neighbor(flip_none, i);
              Node flip_j = Neighbor(flip_none, j);
              Node flip_k = Neighbor(flip_none, k);
              Node flip_ij = Neighbor(flip_i, j);
              Node flip_ik = Neighbor(flip_i, k);
              Node flip_jk = Neighbor(flip_j, k);
              Node flip_ijk = Neighbor(flip_ij, k);
              const std::vector<Node> nodes = {flip_none, flip_i,  flip_j,
                                               flip_k,    flip_ij, flip_ik,
                                               flip_jk,   flip_ijk};
              if (flip_none != *absl::c_max_element(nodes)) {
                continue;
              }
              math_opt::LinearExpression arcs;
              for (const Node n : nodes) {
                arcs += num_adj[n];
              }
              std::vector<Edge> internal_edges = {
                  {flip_none, flip_i}, {flip_none, flip_j},
                  {flip_none, flip_k}, {flip_i, flip_ij},
                  {flip_i, flip_ik},   {flip_j, flip_ij},
                  {flip_j, flip_jk},   {flip_k, flip_ik},
                  {flip_k, flip_jk},   {flip_ij, flip_ijk},
                  {flip_jk, flip_ijk}, {flip_ik, flip_ijk}};

              for (const auto edge : internal_edges) {
                arcs -= math_circuit.edge_var_or_die(edge);
              }
              model.AddLinearConstraint(arcs <= 6.0);
            }
          }
        }
      }
    }
  }
  model.Maximize(nodes_hit);
  const math_opt::SolverType solver = absl::GetFlag(FLAGS_gurobi)
                                          ? math_opt::SolverType::kGurobi
                                          : math_opt::SolverType::kGscip;
  math_opt::SolveArguments args;
  args.callback_registration = {
      .events = {math_opt::CallbackEvent::kMipNode,
                 math_opt::CallbackEvent::kMipSolution},
      .add_lazy_constraints = true};
  args.parameters = {.enable_output = true,
                     .time_limit = absl::GetFlag(FLAGS_time_limit),
                     .threads = absl::GetFlag(FLAGS_threads)};
  args.callback =
      [&math_circuit](
          const math_opt::CallbackData& data) -> math_opt::CallbackResult {
    switch (data.event) {
      case math_opt::CallbackEvent::kMipNode:
        return OnMipNode(math_circuit, data).value();
      case math_opt::CallbackEvent::kMipSolution:
        return OnMipSolution(math_circuit, data).value();
      default:
        LOG(FATAL) << "unexpected event: " << data.event;
    }
  };
  ASSIGN_OR_RETURN(const math_opt::SolveResult result,
                   RunSolve(model, solver, std::move(args)));
  RETURN_IF_ERROR(result.termination.EnsureIsOptimalOrFeasible());
  std::cout << "Objective value: " << result.objective_value() << std::endl;
  return absl::OkStatus();
}
}  // namespace

int main(int argc, char** argv) {
  InitGoogle(argv[0], &argc, &argv, true);
  const absl::Status status = Main();
  if (!status.ok()) {
    LOG(QFATAL) << status;
  }
  return 0;
}
