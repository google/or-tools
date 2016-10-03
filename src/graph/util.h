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

// A collections of utilities for the Graph classes in ./graph.h.

#ifndef OR_TOOLS_GRAPH_UTIL_H_
#define OR_TOOLS_GRAPH_UTIL_H_

#include <algorithm>
#include <memory>
#include <numeric>
#include <string>

#include "base/hash.h"
#include "base/join.h"
#include "base/numbers.h"
#include "base/split.h"
#include "base/join.h"
#include "base/map_util.h"
#include "base/murmur.h"
#include "graph/graph.h"
#include "util/filelineiter.h"
#include "base/status.h"
#include "base/statusor.h"

namespace operations_research {

// Diagnoses whether a graph is symmetric. A graph is symmetric iff
// for all (a, b), the number of arcs a->b is equal to the number of arcs b->a.
// Works in O(graph size).
template <class Graph>
bool GraphIsSymmetric(const Graph& graph);

// Creates a remapped copy of graph "graph", where node i becomes node
// new_node_index[i]. The caller takes ownership of the returned graph.
// "new_node_index" must be a valid permutation of [0..num_nodes-1] or the
// behavior is undefined (it may die).
// Note that you can call IsValidPermutation() to check it yourself.
template <class Graph>
std::unique_ptr<Graph> RemapGraph(const Graph& graph,
                                  const std::vector<int>& new_node_index);

// Returns true iff the given vector is a permutation of [0..size()-1].
bool IsValidPermutation(const std::vector<int>& v);

// Returns a std::string representation of a graph.
enum GraphToStringFormat {
  // One arc per line, eg. "3->1".
  PRINT_GRAPH_ARCS,

  // One space-separated adjacency list per line, eg. "5 1 3 1".
  // Nodes with no outgoing arc get an empty line.
  PRINT_GRAPH_ADJACENCY_LISTS,

  // Ditto, but the adjacency lists are sorted.
  PRINT_GRAPH_ADJACENCY_LISTS_SORTED,
};
template <class Graph>
std::string GraphToString(const Graph& graph, GraphToStringFormat format);

// Returns a copy of "graph", without self-arcs and duplicate arcs.
template <class Graph>
std::unique_ptr<Graph> RemoveSelfArcsAndDuplicateArcs(const Graph& graph);

// Given an arc path, changes it to a sub-path with the same source and
// destination but without any cycle. Nothing happen if the path was already
// without cycle.
//
// The graph class should support Tail(arc) and Head(arc). They should both
// return an integer representing the corresponding tail/head of the passed arc.
//
// TODO(user): In some cases, there is more than one possible solution. We could
// take some arc costs and return the cheapest path instead. Or return the
// shortest path in term of number of arcs.
template <class Graph>
void RemoveCyclesFromPath(const Graph& graph, std::vector<int>* arc_path);

// Returns true iff the given path contains a cycle.
template <class Graph>
bool PathHasCycle(const Graph& graph, const std::vector<int>& arc_path);

// Returns a vector representing a mapping from arcs to arcs such that each arc
// is mapped to another arc with its (tail, head) flipped, if such an arc
// exists (otherwise it is mapped to -1).
// If the graph is symmetric, the returned mapping is bijective and reflexive,
// i.e. out[out[arc]] = arc for all "arc", where "out" is the returned vector.
// If "die_if_not_symmetric" is true, this function CHECKs() that the graph
// is symmetric.
//
// Self-arcs are always mapped to themselves.
//
// Note that since graphs may have multi-arcs, the mapping isn't necessarily
// unique, hence the function name.
template <class Graph>
std::vector<int> ComputeOnePossibleReverseArcMapping(const Graph& graph,
                                                bool die_if_not_symmetric);

// Read a graph file in the simple ".g" format: the file should be a text file
// containing only space-separated integers, whose first line is:
//   <num nodes> <num edges> [<num_colors> <index of first node with color #1>
//                            <index of first node with color #2> ...]
// and whose subsequent lines represent edges if "directed" is false, or arcs if
// "directed" is true:
//   <node1> <node2>.
//
// This returns a newly created graph upon success, which the user needs to take
// ownership of, or a failure status. See base/statusor.h.
//
// If "num_nodes_with_color_or_null" is not nullptr, it will be filled with the
// color information: num_nodes_with_color_or_null[i] will be the number of
// nodes with color #i. Furthermore, nodes are sorted by color.
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
util::StatusOr<Graph*> ReadGraphFile(const std::string& filename, bool directed,
                                     std::vector<int>* num_nodes_with_color_or_null);

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
util::Status WriteGraphToFile(const Graph& graph, const std::string& filename,
                              bool directed,
                              const std::vector<int>& num_nodes_with_color);

// Implementations of the templated methods.

template <class Graph>
bool GraphIsSymmetric(const Graph& graph) {
  typedef typename Graph::NodeIndex NodeIndex;
  typedef typename Graph::ArcIndex ArcIndex;
  // Create a reverse copy of the graph.
  StaticGraph<NodeIndex, ArcIndex> reverse_graph(graph.num_nodes(),
                                                 graph.num_arcs());
  for (const NodeIndex node : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      reverse_graph.AddArc(graph.Head(arc), node);
    }
  }
  reverse_graph.Build();
  // Compare the graph to its reverse, one adjacency list at a time.
  std::vector<ArcIndex> count(graph.num_nodes(), 0);
  for (const NodeIndex node : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      ++count[graph.Head(arc)];
    }
    for (const ArcIndex arc : reverse_graph.OutgoingArcs(node)) {
      if (--count[reverse_graph.Head(arc)] < 0) return false;
    }
    for (const ArcIndex arc : graph.OutgoingArcs(node)) {
      if (count[graph.Head(arc)] != 0) return false;
    }
  }
  return true;
}

template <class Graph>
std::unique_ptr<Graph> RemapGraph(const Graph& old_graph,
                                  const std::vector<int>& new_node_index) {
  DCHECK(IsValidPermutation(new_node_index)) << "Invalid permutation";
  const int num_nodes = old_graph.num_nodes();
  CHECK_EQ(new_node_index.size(), num_nodes);
  std::unique_ptr<Graph> new_graph(new Graph(num_nodes, old_graph.num_arcs()));
  typedef typename Graph::NodeIndex NodeIndex;
  typedef typename Graph::ArcIndex ArcIndex;
  for (const NodeIndex node : old_graph.AllNodes()) {
    for (const ArcIndex arc : old_graph.OutgoingArcs(node)) {
      new_graph->AddArc(new_node_index[node],
                        new_node_index[old_graph.Head(arc)]);
    }
  }
  new_graph->Build();
  return new_graph;
}

template <class Graph>
std::string GraphToString(const Graph& graph, GraphToStringFormat format) {
  std::string out;
  std::vector<typename Graph::NodeIndex> adj;
  for (const typename Graph::NodeIndex node : graph.AllNodes()) {
    if (format == PRINT_GRAPH_ARCS) {
      for (const typename Graph::ArcIndex arc : graph.OutgoingArcs(node)) {
        if (!out.empty()) out += '\n';
        StrAppend(&out, node, "->", graph.Head(arc));
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
      StrAppend(&out, node, ": ", strings::Join(adj, " "));
    }
  }
  return out;
}

template <class Graph>
std::unique_ptr<Graph> RemoveSelfArcsAndDuplicateArcs(const Graph& graph) {
  std::unique_ptr<Graph> g(new Graph(graph.num_nodes(), graph.num_arcs()));
  typedef typename Graph::ArcIndex ArcIndex;
  typedef typename Graph::NodeIndex NodeIndex;
  hash_set<std::pair<NodeIndex, NodeIndex>> arcs;
  for (const NodeIndex tail : graph.AllNodes()) {
    for (const ArcIndex arc : graph.OutgoingArcs(tail)) {
      const NodeIndex head = graph.Head(arc);
      if (head != tail && arcs.insert({tail, head}).second) {
        g->AddArc(tail, head);
      }
    }
  }
  g->Build();
  return g;
}

template <class Graph>
void RemoveCyclesFromPath(const Graph& graph, std::vector<int>* arc_path) {
  if (arc_path->empty()) return;

  // This maps each node to the latest arc in the given path that leaves it.
  std::map<int, int> last_arc_leaving_node;
  for (const int arc : *arc_path) last_arc_leaving_node[graph.Tail(arc)] = arc;

  // Special case for the destination.
  // Note that this requires that -1 is not a valid arc of Graph.
  last_arc_leaving_node[graph.Head(arc_path->back())] = -1;

  // Reconstruct the path by starting at the source and then following the
  // "next" arcs. We override the given arc_path at the same time.
  int node = graph.Tail(arc_path->front());
  int new_size = 0;
  while (new_size < arc_path->size()) {  // To prevent cycle on bad input.
    const int arc = FindOrDie(last_arc_leaving_node, node);
    if (arc == -1) break;
    (*arc_path)[new_size++] = arc;
    node = graph.Head(arc);
  }
  arc_path->resize(new_size);
}

template <class Graph>
bool PathHasCycle(const Graph& graph, const std::vector<int>& arc_path) {
  if (arc_path.empty()) return false;
  std::set<int> seen;
  seen.insert(graph.Tail(arc_path.front()));
  for (const int arc : arc_path) {
    if (!InsertIfNotPresent(&seen, graph.Head(arc))) return true;
  }
  return false;
}

template <class Graph>
std::vector<int> ComputeOnePossibleReverseArcMapping(const Graph& graph,
                                                bool die_if_not_symmetric) {
  std::vector<int> reverse_arc(graph.num_arcs(), -1);
  hash_multimap<std::pair</*tail*/ int, /*head*/ int>, /*arc index*/ int> arc_map;
  for (int arc = 0; arc < graph.num_arcs(); ++arc) {
    const int tail = graph.Tail(arc);
    const int head = graph.Head(arc);
    if (tail == head) {
      // Special case: directly map any self-arc to itself.
      reverse_arc[arc] = arc;
      continue;
    }
    // Lookup for the reverse arc of the current one...
    auto it = arc_map.find({head, tail});
    if (it != arc_map.end()) {
      // Found a reverse arc! Store the mapping and remove the
      // reverse arc from the map.
      reverse_arc[arc] = it->second;
      reverse_arc[it->second] = arc;
      arc_map.erase(it);
    } else {
      // Reverse arc not in the map. Add the current arc to the map.
      arc_map.insert({{tail, head}, arc});
    }
  }
  // Algorithm check, for debugging.
  DCHECK_EQ(std::count(reverse_arc.begin(), reverse_arc.end(), -1),
            arc_map.size());
  if (die_if_not_symmetric) {
    CHECK_EQ(arc_map.size(), 0)
        << "The graph is not symmetric: " << arc_map.size() << " of "
        << graph.num_arcs() << " arcs did not have a reverse.";
  }
  return reverse_arc;
}

template <class Graph>
util::StatusOr<Graph*> ReadGraphFile(
    const std::string& filename, bool directed,
    std::vector<int>* num_nodes_with_color_or_null) {
  std::unique_ptr<Graph> graph;
  int64 num_nodes = -1;
  int64 num_expected_lines = -1;
  int64 num_lines_read = 0;
  for (const std::string& line : FileLines(filename)) {
    ++num_lines_read;
    if (num_lines_read == 1) {
      std::vector<int64> header_ints;
      if (!SplitStringAndParse(line, " ", &safe_strto64, &header_ints) ||
          header_ints.size() < 2 || header_ints[0] < 0 || header_ints[1] < 0) {
        return util::Status(
            util::error::INVALID_ARGUMENT,
            StrCat("First line of '", filename,
                   "' should be at least two nonnegative integers."));
      }
      num_nodes = header_ints[0];
      num_expected_lines = header_ints[1];
      if (num_nodes_with_color_or_null != nullptr) {
        num_nodes_with_color_or_null->clear();
        if (header_ints.size() == 2) {
          // No coloring: all the nodes have the same color.
          num_nodes_with_color_or_null->push_back(num_nodes);
        } else {
          const int num_colors = header_ints[2];
          if (header_ints.size() != num_colors + 2) {
            return util::Status(
                util::error::INVALID_ARGUMENT,
                StrCat(
                    "There should be num_colors-1 color cardinalities in the"
                    " header of '",
                    filename, "' (where num_colors=", num_colors,
                    "): the last color cardinality should be", " skipped."));
          }
          num_nodes_with_color_or_null->reserve(num_colors);
          int num_nodes_left = num_nodes;
          for (int i = 3; i < header_ints.size(); ++i) {
            num_nodes_with_color_or_null->push_back(header_ints[i]);
            num_nodes_left -= header_ints[i];
            if (header_ints[i] <= 0 || num_nodes_left <= 0) {
              return util::Status(
                  util::error::INVALID_ARGUMENT,
                  StrCat("The color cardinalities in the header of '", filename,
                         " should always be >0 and add up to less than the"
                         " total number of nodes."));
            }
          }
          num_nodes_with_color_or_null->push_back(num_nodes_left);
        }
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
                              bool directed,
                              const std::vector<int>& num_nodes_with_color) {
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
      f, "%lld %lld", static_cast<int64>(graph.num_nodes()),
      static_cast<int64>(directed ? graph.num_arcs()
                                  : (graph.num_arcs() + num_self_arcs) / 2));
  if (!num_nodes_with_color.empty()) {
    if (std::accumulate(num_nodes_with_color.begin(),
                        num_nodes_with_color.end(), 0) != graph.num_nodes() ||
        *std::min_element(num_nodes_with_color.begin(),
                          num_nodes_with_color.end()) <= 0) {
      return util::Status(util::error::INVALID_ARGUMENT,
                          "WriteGraphToFile() called with invalid coloring.");
    }
    fprintf(f, " %lu", num_nodes_with_color.size());
    for (int i = 0; i < num_nodes_with_color.size() - 1; ++i) {
      fprintf(f, " %lld", static_cast<int64>(num_nodes_with_color[i]));
    }
  }
  fprintf(f, "\n");

  for (const typename Graph::NodeIndex node : graph.AllNodes()) {
    for (const typename Graph::ArcIndex arc : graph.OutgoingArcs(node)) {
      const typename Graph::NodeIndex head = graph.Head(arc);
      if (directed || head >= node) {
        fprintf(f, "%lld %lld\n", static_cast<int64>(node),
                static_cast<uint64>(head));
      }
    }
  }
  if (fclose(f) != 0) {
    return util::Status(util::error::INTERNAL,
                        "Could not close file '" + filename + "'");
  }
  return util::Status::OK;
}

}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_UTIL_H_
