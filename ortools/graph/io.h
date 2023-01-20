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

// A collections of i/o utilities for the Graph classes in ./graph.h.

#ifndef UTIL_GRAPH_IO_H_
#define UTIL_GRAPH_IO_H_

#include <algorithm>
#include <cstdint>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "ortools/base/numbers.h"
#include "ortools/graph/graph.h"
#include "ortools/util/filelineiter.h"

namespace util {

// Returns a string representation of a graph.
enum GraphToStringFormat {
  // One arc per line, eg. "3->1".
  PRINT_GRAPH_ARCS,

  // One space-separated adjacency list per line, eg. "3: 5 1 3 1".
  // Nodes with no outgoing arc get an empty list.
  PRINT_GRAPH_ADJACENCY_LISTS,

  // Ditto, but the adjacency lists are sorted.
  PRINT_GRAPH_ADJACENCY_LISTS_SORTED,
};
template <class Graph>
std::string GraphToString(const Graph& graph, GraphToStringFormat format);

// Writes a graph to the ".g" file format described above. If "directed" is
// true, all arcs are written to the file. If it is false, the graph is expected
// to be undirected (i.e. the number of arcs a->b is equal to the number of arcs
// b->a for all nodes a,b); and only the arcs a->b where a<=b are written. Note
// however that in this case, the symmetry of the graph is not fully checked
// (only the parity of the number of non-self arcs is).
//
// "num_nodes_with_color" is optional. If it is not empty, then the color
// information will be written to the header of the .g file. See ReadGraphFile.
//
// This method is the reverse of ReadGraphFile (with the same value for
// "directed").
template <class Graph>
absl::Status WriteGraphToFile(const Graph& graph, const std::string& filename,
                              bool directed,
                              const std::vector<int>& num_nodes_with_color);

// Implementations of the templated methods.

template <class Graph>
std::string GraphToString(const Graph& graph, GraphToStringFormat format) {
  std::string out;
  std::vector<typename Graph::NodeIndex> adj;
  for (const typename Graph::NodeIndex node : graph.AllNodes()) {
    if (format == PRINT_GRAPH_ARCS) {
      for (const typename Graph::ArcIndex arc : graph.OutgoingArcs(node)) {
        if (!out.empty()) out += '\n';
        absl::StrAppend(&out, node, "->", graph.Head(arc));
      }
    } else {  // PRINT_GRAPH_ADJACENCY_LISTS[_SORTED]
      adj.clear();
      for (const typename Graph::ArcIndex arc : graph.OutgoingArcs(node)) {
        adj.push_back(graph.Head(arc));
      }
      if (format == PRINT_GRAPH_ADJACENCY_LISTS_SORTED) {
        std::sort(adj.begin(), adj.end());
      }
      if (node != 0) out += '\n';
      absl::StrAppend(&out, node, ": ", absl::StrJoin(adj, " "));
    }
  }
  return out;
}

template <class Graph>
absl::Status WriteGraphToFile(const Graph& graph, const std::string& filename,
                              bool directed,
                              const std::vector<int>& num_nodes_with_color) {
  FILE* f = fopen(filename.c_str(), "w");
  if (f == nullptr) {
    return absl::Status(absl::StatusCode::kInvalidArgument,
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
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "WriteGraphToFile() called with directed=false"
                          " and with a graph with an odd number of (non-self)"
                          " arcs!");
    }
  }
  absl::FPrintF(
      f, "%d %d", static_cast<int64_t>(graph.num_nodes()),
      static_cast<int64_t>(directed ? graph.num_arcs()
                                    : (graph.num_arcs() + num_self_arcs) / 2));
  if (!num_nodes_with_color.empty()) {
    if (std::accumulate(num_nodes_with_color.begin(),
                        num_nodes_with_color.end(), 0) != graph.num_nodes() ||
        *std::min_element(num_nodes_with_color.begin(),
                          num_nodes_with_color.end()) <= 0) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "WriteGraphToFile() called with invalid coloring.");
    }
    absl::FPrintF(f, " %d", num_nodes_with_color.size());
    for (int i = 0; i < num_nodes_with_color.size() - 1; ++i) {
      absl::FPrintF(f, " %d", static_cast<int64_t>(num_nodes_with_color[i]));
    }
  }
  absl::FPrintF(f, "\n");

  for (const typename Graph::NodeIndex node : graph.AllNodes()) {
    for (const typename Graph::ArcIndex arc : graph.OutgoingArcs(node)) {
      const typename Graph::NodeIndex head = graph.Head(arc);
      if (directed || head >= node) {
        absl::FPrintF(f, "%d %d\n", static_cast<int64_t>(node),
                      static_cast<uint64_t>(head));
      }
    }
  }

  if (fclose(f) != 0) {
    return absl::Status(absl::StatusCode::kInternal,
                        "Could not close file '" + filename + "'");
  }

  return ::absl::OkStatus();
}

}  // namespace util

#endif  // UTIL_GRAPH_IO_H_
