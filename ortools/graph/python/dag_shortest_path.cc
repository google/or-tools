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

#include "ortools/graph/dag_shortest_path.h"

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/graph_base/graph.h"
#include "pybind11/pybind11.h"
#include "pybind11/stl.h"

namespace py = pybind11;

using ::operations_research::ArcWithLength;
using ::operations_research::KShortestPathsOnDag;
using ::operations_research::KShortestPathsOnDagWrapper;
using ::operations_research::PathWithLength;
using ::operations_research::ShortestPathsOnDag;
using ::operations_research::ShortestPathsOnDagWrapper;
using ::util::StaticGraph;

PYBIND11_MODULE(dag_shortest_path, m) {
  m.doc() = "DAG shortest path algorithms";

  // Map "from" and "to" to "from_node" and "to_node": Python has a `from`
  // keyword; apply the same modification for "to" for consistency.
  py::class_<ArcWithLength>(m, "ArcWithLength")
      .def(py::init([](int from_node, int to_node, double length) {
             return ArcWithLength{from_node, to_node, length};
           }),
           py::arg("from_node") = 0, py::arg("to_node") = 0,
           py::arg("length") = 0.0)
      .def(py::init<>())
      .def_readwrite("from_node", &ArcWithLength::from)
      .def_readwrite("to_node", &ArcWithLength::to)
      .def_readwrite("length", &ArcWithLength::length)
      .def("__repr__", [](const ArcWithLength& arc) {
        return absl::StrCat("ArcWithLength(from_node=", arc.from,
                            ", to_node=", arc.to, ", length=", arc.length, ")");
      });

  py::class_<PathWithLength>(m, "PathWithLength")
      .def(py::init([](double length, const std::vector<int>& arc_path,
                       const std::vector<int>& node_path) {
             return PathWithLength{length, arc_path, node_path};
           }),
           py::arg("length") = 0.0, py::arg("arc_path") = std::vector<int>(),
           py::arg("node_path") = std::vector<int>())
      .def(py::init<>())
      .def_readwrite("length", &PathWithLength::length)
      .def_readwrite("arc_path", &PathWithLength::arc_path)
      .def_readwrite("node_path", &PathWithLength::node_path)
      .def("__repr__", [](const PathWithLength& path) {
        return absl::StrCat("PathWithLength(length=", path.length,
                            ", arc_path=[", absl::StrJoin(path.arc_path, ", "),
                            "], node_path=[",
                            absl::StrJoin(path.node_path, ", "), "])");
      });

  m.def(
      "shortest_paths_on_dag",
      [](int num_nodes, const std::vector<ArcWithLength>& arcs_with_length,
         int source, int destination) {
        return ShortestPathsOnDag(num_nodes, arcs_with_length, source,
                                  destination);
      },
      py::arg("num_nodes"), py::arg("arcs_with_length"), py::arg("source"),
      py::arg("destination"),
      "Computes the shortest path on a directed acyclic graph (DAG).");

  m.def(
      "k_shortest_paths_on_dag",
      [](int num_nodes, const std::vector<ArcWithLength>& arcs_with_length,
         int source, int destination, int path_count) {
        return KShortestPathsOnDag(num_nodes, arcs_with_length, source,
                                   destination, path_count);
      },
      py::arg("num_nodes"), py::arg("arcs_with_length"), py::arg("source"),
      py::arg("destination"), py::arg("path_count"),
      "Computes the k-shortest paths on a directed acyclic graph (DAG).");

  // TODO(user): remove this once the graph implementations are properly
  // wrapped. Also move the tests from dag_shortest_path_test.py.
  py::class_<StaticGraph<>>(m, "StaticGraph")
      .def(py::init<>())
      .def("num_nodes", &StaticGraph<>::num_nodes)
      .def("num_arcs", &StaticGraph<>::num_arcs);

  // TODO(user): idem.
  py::class_<StaticGraph<>::Builder>(m, "StaticGraphBuilder")
      .def(py::init<int, int>(), py::arg("num_nodes"), py::arg("num_arcs"))
      .def("add_arc", &StaticGraph<>::Builder::AddArc, py::arg("tail"),
           py::arg("head"))
      .def(
          "build",
          [](StaticGraph<>::Builder* builder) {
            std::vector<StaticGraph<>::ArcIndex> permutation;
            auto graph = builder->Build(&permutation);
            return py::make_tuple(graph.release(), permutation);
          },
          "Builds the graph and returns (graph, arc_permutation).");

  // TODO(user): template this once the graph implementations are properly
  // wrapped (so that users can choose their own).
  // TODO(user): also wrap FastTopologicalSort (ortools/graph_base/graph.h) in
  // Python.
  py::class_<ShortestPathsOnDagWrapper<StaticGraph<>>,
             std::shared_ptr<ShortestPathsOnDagWrapper<StaticGraph<>>>>(
      m, "ShortestPathsOnDagWrapper")
      .def(py::init([](const StaticGraph<>* graph,
                       std::vector<double> arc_lengths,
                       std::vector<int> topological_order) {
             if (topological_order.empty()) {
               throw std::invalid_argument(
                   "topological_order must not be empty.");
             }
             auto* arc_lengths_ptr =
                 new std::vector<double>(std::move(arc_lengths));
             auto* topo_order_ptr =
                 new std::vector<int>(std::move(topological_order));
             auto* raw_wrapper = new ShortestPathsOnDagWrapper<StaticGraph<>>(
                 graph, arc_lengths_ptr, *topo_order_ptr);
             return std::shared_ptr<ShortestPathsOnDagWrapper<StaticGraph<>>>(
                 raw_wrapper, [arc_lengths_ptr, topo_order_ptr](
                                  ShortestPathsOnDagWrapper<StaticGraph<>>* w) {
                   delete w;
                   delete arc_lengths_ptr;
                   delete topo_order_ptr;
                 });
           }),
           py::arg("graph"), py::arg("arc_lengths"),
           py::arg("topological_order"), py::keep_alive<1, 2>())
      .def(
          "run_shortest_path_on_dag",
          [](ShortestPathsOnDagWrapper<StaticGraph<>>* w,
             const std::vector<int>& sources) {
            w->RunShortestPathOnDag(sources);
          },
          py::arg("sources"))
      .def("is_reachable",
           &ShortestPathsOnDagWrapper<StaticGraph<>>::IsReachable,
           py::arg("node"))
      .def("reached_nodes",
           &ShortestPathsOnDagWrapper<StaticGraph<>>::reached_nodes)
      .def("length_to",
           py::overload_cast<int>(
               &ShortestPathsOnDagWrapper<StaticGraph<>>::LengthTo, py::const_),
           py::arg("node"))
      .def("arc_path_to", &ShortestPathsOnDagWrapper<StaticGraph<>>::ArcPathTo,
           py::arg("node"))
      .def("node_path_to",
           &ShortestPathsOnDagWrapper<StaticGraph<>>::NodePathTo,
           py::arg("node"));

  py::class_<KShortestPathsOnDagWrapper<StaticGraph<>>,
             std::shared_ptr<KShortestPathsOnDagWrapper<StaticGraph<>>>>(
      m, "KShortestPathsOnDagWrapper")
      .def(py::init([](const StaticGraph<>* graph,
                       std::vector<double> arc_lengths,
                       std::vector<int> topological_order, int path_count) {
             if (topological_order.empty()) {
               throw std::invalid_argument(
                   "topological_order must not be empty.");
             }
             auto* arc_lengths_ptr =
                 new std::vector<double>(std::move(arc_lengths));
             auto* topo_order_ptr =
                 new std::vector<int>(std::move(topological_order));
             auto* raw_wrapper = new KShortestPathsOnDagWrapper<StaticGraph<>>(
                 graph, arc_lengths_ptr, *topo_order_ptr, path_count);
             return std::shared_ptr<KShortestPathsOnDagWrapper<StaticGraph<>>>(
                 raw_wrapper,
                 [arc_lengths_ptr, topo_order_ptr](
                     KShortestPathsOnDagWrapper<StaticGraph<>>* w) {
                   delete w;
                   delete arc_lengths_ptr;
                   delete topo_order_ptr;
                 });
           }),
           py::arg("graph"), py::arg("arc_lengths"),
           py::arg("topological_order"), py::arg("path_count"),
           py::keep_alive<1, 2>())
      .def(
          "run_k_shortest_path_on_dag",
          [](KShortestPathsOnDagWrapper<StaticGraph<>>* w,
             const std::vector<int>& sources) {
            w->RunKShortestPathOnDag(sources);
          },
          py::arg("sources"))
      .def("is_reachable",
           &KShortestPathsOnDagWrapper<StaticGraph<>>::IsReachable,
           py::arg("node"))
      .def("reached_nodes",
           &KShortestPathsOnDagWrapper<StaticGraph<>>::reached_nodes)
      .def("lengths_to", &KShortestPathsOnDagWrapper<StaticGraph<>>::LengthsTo,
           py::arg("node"))
      .def("arc_paths_to",
           &KShortestPathsOnDagWrapper<StaticGraph<>>::ArcPathsTo,
           py::arg("node"))
      .def("node_paths_to",
           &KShortestPathsOnDagWrapper<StaticGraph<>>::NodePathsTo,
           py::arg("node"))
      .def("path_count",
           &KShortestPathsOnDagWrapper<StaticGraph<>>::path_count);
}
