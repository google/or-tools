// Copyright 2010-2013 Google
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
// A collections of utilities for the Graph classes in ./graph.h.

#ifndef OR_TOOLS_GRAPH_UTIL_H_
#define OR_TOOLS_GRAPH_UTIL_H_

#include "base/unique_ptr.h"
#include <string>

#include "base/join.h"
#include "graph/graph.h"
#include "util/filelineiter.h"
#include "base/status.h"
#include "base/statusor.h"

namespace operations_research {

// Read a graph file in the simple ".g" format: the file should be a text file
// containing only space-separated integers, whose first line is:
//   <num nodes> <num edges> [arbitrary number of ignored field]
// and whose subsequent lines represent edges if "directed" is false, or arcs if
// "directed" is true:
//   <node1> <node2>.
//
// This returns a newly created graph upon success, which the user needs to take
// ownership of, or a failure status. See base/statusor.h.
//
// Examples:
//   // Simply crash if the graph isn't successfully read from the file.
//   typedef StaticGraph<> MyGraph;  // This is just an example.
//   std::unique_ptr<MyGraph> my_graph(
//       ReadGraphFile<MyGraph>("graph.g", /*directed=*/ false).ValueOrDie());
//
//   // More complicated error handling.
//   util::StatusOr<MyGraph*> error_or_graph =
//       ReadGraphFile<MyGraph>("graph.g", /*directed=*/ false);
//   if (!error_or_graph.ok()) {
//     LOG(ERROR) << "Error: " << error_or_graph.status().error_message();
//   } else {
//     std::unique_ptr<MyGraph> my_graph(error_or_graph.ValueOrDie());
//     ...
//   }
template <class Graph>
util::StatusOr<Graph*> ReadGraphFile(const std::string& filename, bool directed);

// Writes a graph to the ".g" file format described above. If "directed" is
// true, all arcs are written to the file. If it is false, the graph is expected
// to be undirected (i.e. the number of arcs a->b is equal to the number of arcs
// b->a for all nodes a,b); and only the arcs a->b where a<=b are written. Note
// however that in this case, the symmetry of the graph is not fully checked
// (only the parity of the number of non-self arcs is).
//
// This method is the reverse of ReadGraphFile (with the same value for
// "directed").
template <class Graph>
util::Status WriteGraphToFile(const Graph& graph, const std::string& filename,
                              bool directed);

template <class Graph>
util::StatusOr<Graph*> ReadGraphFile(const std::string& filename, bool directed) {
  std::unique_ptr<Graph> graph;
  int64 num_nodes = -1;
  int64 num_expected_lines = -1;
  int64 num_lines_read = 0;
  for (const std::string& line : FileLines(filename)) {
    ++num_lines_read;
    if (num_lines_read == 1) {
      if (sscanf(line.c_str(), "%lld %lld", &num_nodes, &num_expected_lines) !=
              2 ||
          num_nodes < 0 || num_expected_lines < 0) {
        return util::Status(
            util::error::INVALID_ARGUMENT,
            StrCat("First line of '", filename,
                   "' should start with two"
                   " nonnegative integers <num nodes> and <num arcs>"));
      }
      const int64 num_arcs = (directed ? 1 : 2) * num_expected_lines;
      graph.reset(new Graph(num_nodes, num_arcs));
      continue;
    }
    int64 node1 = -1;
    int64 node2 = -1;
    if (sscanf(line.c_str(), "%lld %lld", &node1, &node2) != 2 || node1 < 0 ||
        node2 < 0 || node1 >= num_nodes || node2 >= num_nodes) {
      return util::Status(
          util::error::INVALID_ARGUMENT,
          StrCat("In '", filename, "', line ", num_lines_read, ": Expected two",
                 " integers in the range [0, ", num_nodes, ")."));
    }
    // We don't add superfluous arcs to the graph, but we still keep reading
    // the file, to get better error messages: we want to know the actual
    // number of lines, and also want to check the validity of the superfluous
    // arcs (i.e. that their src/dst nodes are ok).
    if (num_lines_read > num_expected_lines + 1) continue;
    graph->AddArc(node1, node2);
    if (!directed && node1 != node2) graph->AddArc(node2, node1);
  }
  if (num_lines_read == 0) {
    return util::Status(util::error::INVALID_ARGUMENT, "Unknown or empty file");
  }
  if (num_lines_read != num_expected_lines + 1) {
    return util::Status(
        util::error::INVALID_ARGUMENT,
        StrCat("The number of arcs/edges in '", filename, "' (",
               num_lines_read - 1, " does not match the value announced in",
               " the header (", num_expected_lines, ")"));
  }
  graph->Build();
  return graph.release();
}

template <class Graph>
util::Status WriteGraphToFile(const Graph& graph, const std::string& filename,
                              bool directed) {
  FILE* f = fopen(filename.c_str(), "w");
  if (f == nullptr) {
    return util::Status(util::error::INVALID_ARGUMENT,
                        "Could not open file: '" + filename + "'");
  }
  // In undirected mode, we must count the self-arcs separately. All other arcs
  // should be duplicated.
  int num_self_arcs = 0;
  if (!directed) {
    for (const typename Graph::NodeIndex node : graph.AllNodes()) {
      for (const typename Graph::ArcIndex arc : graph.OutgoingArcs(node)) {
        if (graph.Head(arc) == node) ++num_self_arcs;
      }
    }
    if ((graph.num_arcs() - num_self_arcs) % 2 != 0) {
      fclose(f);
      return util::Status(util::error::INVALID_ARGUMENT,
                          "WriteGraphToFile() called with directed=false"
                          " and with a graph with an odd number of (non-self)"
                          " arcs!");
    }
  }
  fprintf(
      f, "%lld %lld\n", static_cast<int64>(graph.num_nodes()),
      static_cast<int64>(directed ? graph.num_arcs()
                                  : (graph.num_arcs() + num_self_arcs) / 2));
  for (const typename Graph::NodeIndex node : graph.AllNodes()) {
    for (const typename Graph::ArcIndex arc : graph.OutgoingArcs(node)) {
      const typename Graph::NodeIndex head = graph.Head(arc);
      if (directed || head >= node) {
        fprintf(f, "%lld %lld\n", static_cast<int64>(node),
                static_cast<uint64>(head));
      }
    }
  }
  // COV_NF_START
  if (fclose(f) != 0) {
    return util::Status(util::error::INTERNAL,
                        "Could not close file '" + filename + "'");
  }
  // COV_NF_END
  return util::Status::OK;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_UTIL_H_
