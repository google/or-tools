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

// A parser for CARPLIB instances. The base files are available online, as well
// as a description of the (Spanish-based) format:
// https://www.uv.es/belengue/carp.html ("CARPLIB")
// https://www.uv.es/~belengue/carp/READ_ME
//
// The goal is to find routes starting and ending at a depot which visit a
// set of arcs (whereas a VRP visits nodes). The objective is to minimize the
// total cost, which is due to either servicing an edge (i.e. performing the
// required action) or traversing an edge (to get to another point in space).
// Not all arcs/edges in the graph must be serviced.
//
// By this formulation, the total cost of servicing is known in advance.
// All vehicles start at the same node, the depot, having index 1.
// Servicing an edge requires resources, vehicles have a limited capacity. All
// vehicles have the same capacity.
//
// The format of the data is the following:
//
// NOMBRE : <INSTANCE-NAME>
// COMENTARIO : <ARBITRARY-COMMENT>
// VERTICES : <NUMBER-OF-NODES, int>
// ARISTAS_REQ : <NUMBER-OF-EDGES-WITH-NONZERO-SERVICING, int>
// ARISTAS_NOREQ : <NUMBER-OF-EDGES-WITH-ZERO-SERVICING, int>
// VEHICULOS : <NUMBER-OF-VEHICLES, int>
// CAPACIDAD : <CAPACITY-OF-EACH-VEHICLE, int>
// TIPO_COSTES_ARISTAS : EXPLICITOS
// COSTE_TOTAL_REQ : <TOTAL-SERVICING-COST>
// LISTA_ARISTAS_REQ :
// ( <HEAD-NODE-OF-EDGE, int>, <TAIL-NODE-OF-EDGE, int> )
//          coste <TRAVERSING-COST, int> demanda <SERVICING, int>
// <repeated, one edge per line>
// LISTA_ARISTAS_NOREQ :
// ( <HEAD-NODE-OF-EDGE, int>, <TAIL-NODE-OF-EDGE, int> )
//          coste <TRAVERSING-COST, int>
// <repeated, one edge per line>
// DEPOSITO :   1
//
// While the file format is defined with 1-based indexing, the output of the
// parser is always 0-based. Users of this parser should never see any 1-based
// index; only 0-based index should be used to query values.

#ifndef OR_TOOLS_ROUTING_CARP_PARSER_H_
#define OR_TOOLS_ROUTING_CARP_PARSER_H_

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "ortools/base/linked_hash_map.h"
#include "ortools/base/logging.h"
#include "ortools/routing/simple_graph.h"

namespace operations_research {
class CarpParser {
 public:
  CarpParser();

#ifndef SWIG
  CarpParser(const CarpParser&) = delete;
  const CarpParser& operator=(const CarpParser&) = delete;
#endif

  // Loads instance from a file into this parser object.
  bool LoadFile(const std::string& file_name);

  // Returns the name of the instance being solved.
  const std::string& name() const { return name_; }
  // Returns the comment of the instance being solved, typically an upper bound.
  const std::string& comment() const { return comment_; }
  // Returns the index of the depot.
  int64_t depot() const { return depot_; }
  // Returns the number of nodes in the current routing problem.
  int64_t NumberOfNodes() const { return number_of_nodes_; }
  // Returns the number of edges in the current routing problem, with or
  // without servicing required.
  int64_t NumberOfEdges() const {
    return NumberOfEdgesWithServicing() + NumberOfEdgesWithoutServicing();
  }
  // Returns the number of edges in the current routing problem that require
  // servicing.
  int64_t NumberOfEdgesWithServicing() const {
    return number_of_edges_with_servicing_;
  }
  // Returns the number of edges in the current routing problem that do not
  // require servicing.
  int64_t NumberOfEdgesWithoutServicing() const {
    return number_of_edges_without_servicing_;
  }
  // Returns the total servicing cost for all arcs.
  int64_t TotalServicingCost() const { return total_servicing_cost_; }
  // Returns the servicing of the edges in the current routing problem.
  const gtl::linked_hash_map<Edge, int64_t>& servicing_demands() const {
    return servicing_demands_;
  }
  // Returns the traversing costs of the edges in the current routing problem.
  const gtl::linked_hash_map<Edge, int64_t>& traversing_costs() const {
    return traversing_costs_;
  }
  // Returns the maximum number of vehicles to use.
  int64_t NumberOfVehicles() const { return n_vehicles_; }
  // Returns the capacity of the vehicles.
  int64_t capacity() const { return capacity_; }

  // Returns the traversing cost for an edge. All edges are supposed to have
  // a traversing cost.
  int64_t GetTraversingCost(Edge edge) const {
    CHECK(traversing_costs_.contains(edge))
        << "Unknown edge: " << edge.tail() << " - " << edge.head();
    return traversing_costs_.at(edge);
  }
  int64_t GetTraversingCost(int64_t tail, int64_t head) const {
    return GetTraversingCost({tail, head});
  }

  // Checks whether this edge requires servicing.
  int64_t HasServicingNeed(Edge edge) const {
    return servicing_demands_.contains(edge);
  }
  int64_t HasServicingNeed(int64_t tail, int64_t head) const {
    return HasServicingNeed({tail, head});
  }

  // Returns the servicing for an edge. Only a subset of edges have a servicing
  // need.
  int64_t GetServicing(Edge edge) const {
    CHECK(HasServicingNeed(edge))
        << "Unknown edge: " << edge.tail() << " - " << edge.head();
    return servicing_demands_.at(edge);
  }
  int64_t GetServicing(int64_t tail, int64_t head) const {
    return GetServicing({tail, head});
  }

 private:
  // Parsing.
  enum Section {
    METADATA,
    ARCS_WITH_SERVICING,
    ARCS_WITHOUT_SERVICING,
    UNDEFINED_SECTION
  };

  void Initialize();
  bool ParseFile(const std::string& file_name);
  bool ParseMetadataLine(const std::vector<std::string>& words);
  bool ParseEdge(std::string_view line, bool with_servicing);

  // Parsing data.
  Section section_;

  // Instance data:
  // - metadata
  std::string name_;
  std::string comment_;
  int64_t number_of_nodes_;
  int64_t number_of_edges_with_servicing_;
  int64_t number_of_edges_without_servicing_;
  int64_t total_servicing_cost_;
  int64_t depot_;
  // - graph costs and servicing demands. Keep track of the order of the
  //   demands: the output format requires to use the servicing-demands IDs,
  //   which are indices when iterating over this map.
  gtl::linked_hash_map<Edge, int64_t> traversing_costs_;
  gtl::linked_hash_map<Edge, int64_t> servicing_demands_;
  // - vehicles
  int64_t n_vehicles_;
  int64_t capacity_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_CARP_PARSER_H_
