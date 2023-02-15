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

// A parser for NEARPLIB instances. The base files are available online, as well
// as a description of the format:
// https://www.sintef.no/projectweb/top/nearp/documentation/
//
// The goal is to find routes starting and ending at a depot which visit a
// set of arcs (directed), edges (undirected), and nodes, whereas a VRP only
// visits nodes. The objective is to minimize the total cost, which is due to
// either servicing a part of the graph (i.e. performing the required action)
// or traversing an edge (to get to another point in space). Not all arcs/edges
// in the graph must be serviced. These components are summarized in NEARP:
// node-edge-arc routing problem. The problem is sometimes also called MCGRP:
// mixed capacitated generalized routing problem.
//
// All vehicles start at the same node, the depot. Its index is often 1, but
// many instances have another depot.
// Servicing a part of the graph requires resources, vehicles have a limited
// capacity. All vehicles have the same capacity.
//
// The format of the data is the following (from
// https://www.sintef.no/projectweb/top/nearp/documentation/):
//
//   Name:          <Instance name>
//   Optimal value: <Optimal value, -1 if unknown>
//   #Vehicles:     <Max. number of vehicles, -1 if unconstrained>
//   Capacity:      <Vehicle capacity Q>
//   Depot:         <Index of depot node>
//   #Nodes:        <number of nodes>
//   #Edges:        <number of edges>
//   #Arcs:         <number of arcs>
//   #Required N:   <number of required nodes>
//   #Required E:   <number of required edges>
//   #Required A:   <number of required arcs>
//
//   % Required nodes:  Ni q_i s_i
//   NODE INDEX, DEMAND, SERVICE COST
//
//   % Required edges: Ek i j q_ij c_ij s_ij
//   EDGE INDEX, FROM NODE, TO NODE, TRAVERSAL COST, DEMAND, SERVICE COST
//
//   % Non-required edges: NrEl i j c_ij
//   EDGE INDEX, FROM NODE, TO NODE, TRAVERSAL COST
//
//   % Required arcs: Ar i j q_ij c_ij
//   ARC INDEX, FROM NODE, TO NODE, TRAVERSAL COST, DEMAND, SERVICE COST
//
//   % Non-required arcs: NrAs i j c_ij
//   ARC INDEX, FROM NODE, TO NODE, TRAVERSAL COST
//
// For nodes, the index is of the form NX, where X is the node index (for
// instance, N1 is the first node that requires servicing). The elements of
// each section are not necessarily sorted. Nodes are indexed together, with no
// separation between those that require servicing and those that do not,
// from 1 to the number of nodes. Conversely, arcs and edges have separate
// indexing depending on whether they require indexing: E1 to EM all require
// servicing, NrE1 to NrEN do not, for a total of M + N edges (respectively,
// for arcs, A1 to AK and NrA1 to NrAL for K + L arcs).
//
// While the file format is defined with 1-based indexing, the output of the
// parser is always 0-based. Users of this parser should never see any 1-based
// index; only 0-based index should be used to query values.

#ifndef OR_TOOLS_ROUTING_NEARP_PARSER_H_
#define OR_TOOLS_ROUTING_NEARP_PARSER_H_

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "ortools/base/linked_hash_map.h"
#include "ortools/base/logging.h"
#include "ortools/routing/simple_graph.h"

namespace operations_research {
class NearpParser {
 public:
  NearpParser();

#ifndef SWIG
  NearpParser(const NearpParser&) = delete;
  const NearpParser& operator=(const NearpParser&) = delete;
#endif

  // Loads instance from a file into this parser object.
  bool LoadFile(const std::string& file_name);

  // Returns the name of the instance being solved.
  const std::string& name() const { return name_; }
  // Returns the comment of the instance being solved, typically an upper bound.
  const std::string& comment() const { return comment_; }
  // Returns the index of the depot.
  int64_t depot() const { return depot_; }

  // Returns the maximum number of vehicles to use.
  int NumberOfVehicles() const { return num_vehicles_; }
  // Returns the capacity of the vehicles.
  int64_t capacity() const { return capacity_; }

  // Returns the number of nodes in the current routing problem.
  int NumberOfNodes() const { return num_nodes_; }
  // Returns the number of arc in the current routing problem, with or
  // without servicing required.
  int NumberOfArcs() const { return num_arcs_; }
  // Returns the number of edges in the current routing problem, with or
  // without servicing required.
  int NumberOfEdges() const { return num_edges_; }

  // Returns the number of arcs in the current routing problem that require
  // servicing.
  int NumberOfArcsWithServicing() const { return num_arcs_with_servicing_; }
  // Returns the number of edges in the current routing problem that require
  // servicing.
  int NumberOfEdgesWithServicing() const { return num_edges_with_servicing_; }
  // Returns the number of nodes in the current routing problem that require
  // servicing.
  int NumberOfNodesWithServicing() const { return num_nodes_with_servicing_; }

  // Returns the number of arcs in the current routing problem that do not
  // require servicing.
  int NumberOfArcsWithoutServicing() const {
    return NumberOfArcs() - NumberOfArcsWithServicing();
  }
  // Returns the number of edges in the current routing problem that do not
  // require servicing.
  int NumberOfEdgesWithoutServicing() const {
    return NumberOfEdges() - NumberOfEdgesWithServicing();
  }
  // Returns the number of nodes in the current routing problem that do not
  // require servicing.
  int64_t NumberOfNodesWithoutServicing() const {
    return NumberOfNodes() - NumberOfNodesWithServicing();
  }

  // Returns the servicing demands of the arcs in the current routing problem.
  const gtl::linked_hash_map<Arc, int64_t>& arc_servicing_demands() const {
    return arc_servicing_demands_;
  }
  // Returns the servicing demands of the edges in the current routing problem.
  const gtl::linked_hash_map<Edge, int64_t>& edge_servicing_demands() const {
    return edge_servicing_demands_;
  }
  // Returns the servicing demands of the nodes in the current routing problem.
  const gtl::linked_hash_map<int64_t, int64_t>& node_servicing_demands() const {
    return node_servicing_demands_;
  }

  // Returns the servicing costs of the arcs in the current routing problem.
  const gtl::linked_hash_map<Arc, int64_t>& arc_servicing_costs() const {
    return arc_servicing_costs_;
  }
  // Returns the servicing costs of the edges in the current routing problem.
  const gtl::linked_hash_map<Edge, int64_t>& edge_servicing_costs() const {
    return edge_servicing_costs_;
  }
  // Returns the servicing costs of the nodes in the current routing problem.
  const gtl::linked_hash_map<int64_t, int64_t>& node_servicing_costs() const {
    return node_servicing_costs_;
  }

  // Returns the traversing costs of the arcs in the current routing problem.
  const gtl::linked_hash_map<Arc, int64_t>& arc_traversing_costs() const {
    return arc_traversing_costs_;
  }
  // Returns the traversing costs of the edges in the current routing problem.
  const gtl::linked_hash_map<Edge, int64_t>& edge_traversing_costs() const {
    return edge_traversing_costs_;
  }

  // Returns the name of graph objects. The implementations should fit all
  // instances of NEARP files,
  std::string GetArcName(Arc arc) const;
  std::string GetArcName(int64_t tail, int64_t head) const {
    return GetArcName({tail, head});
  }
  std::string GetEdgeName(Edge edge) const;
  std::string GetEdgeName(int64_t tail, int64_t head) const {
    return GetEdgeName({tail, head});
  }
  std::string GetNodeName(int64_t node) const {
    CHECK_GE(node, 0);
    CHECK_LT(node, NumberOfNodes());
    return absl::StrCat("N", node + 1);
  }

 private:
  // Parsing.
  enum Section {
    METADATA,
    ARCS_WITH_SERVICING,
    ARCS_WITHOUT_SERVICING,
    EDGES_WITH_SERVICING,
    EDGES_WITHOUT_SERVICING,
    NODES_WITH_SERVICING,
    // No need for a state to parse nodes without servicing demands: they do
    // not have any data associated with them (their number is known in the
    // header of the data file).
    UNDEFINED_SECTION
  };

  void Initialize();
  bool ParseFile(const std::string& file_name);
  bool ParseMetadataLine(const std::vector<std::string>& words);
  bool ParseArc(std::string_view line, bool with_servicing);
  bool ParseEdge(std::string_view line, bool with_servicing);
  bool ParseNode(std::string_view line);

  // Parsing data.
  Section section_;

  // Instance data:
  // - metadata
  std::string name_;
  std::string comment_;
  int num_arcs_;
  int num_edges_;
  int num_nodes_;
  int num_arcs_with_servicing_;
  int num_edges_with_servicing_;
  int num_nodes_with_servicing_;
  int64_t depot_;

  // - graph costs and servicing demands. Keep track of the order of the
  //   demands: the output format requires to use the servicing-demands IDs,
  //   which are indices when iterating through these maps.
  //   Specifically, for nodes, a vector is not suitable, as indices are not
  //   necessarily contiguous.
  gtl::linked_hash_map<Arc, int64_t> arc_traversing_costs_;
  gtl::linked_hash_map<Edge, int64_t> edge_traversing_costs_;

  gtl::linked_hash_map<Arc, int64_t> arc_servicing_demands_;
  gtl::linked_hash_map<Edge, int64_t> edge_servicing_demands_;
  gtl::linked_hash_map<int64_t, int64_t> node_servicing_demands_;

  gtl::linked_hash_map<Arc, int64_t> arc_servicing_costs_;
  gtl::linked_hash_map<Edge, int64_t> edge_servicing_costs_;
  gtl::linked_hash_map<int64_t, int64_t> node_servicing_costs_;

  // - vehicles
  int num_vehicles_;
  int64_t capacity_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_NEARP_PARSER_H_
