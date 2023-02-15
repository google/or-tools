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

// A TSPLIB parser. The TSPLIB is a library containing Traveling
// Salesman Problems and other vehicle routing problems.
// Limitations:
// - only TSP and CVRP files are currently supported.
// - XRAY1, XRAY2 and SPECIAL edge weight types are not supported.
//
// Takes as input a data file, potentially gzipped. The data must
// follow the TSPLIB95 format (described at
// http://www.iwr.uni-heidelberg.de/groups/comopt/software/TSPLIB95/DOC.PS).

#ifndef OR_TOOLS_ROUTING_TSPLIB_PARSER_H_
#define OR_TOOLS_ROUTING_TSPLIB_PARSER_H_

#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/integral_types.h"
#include "ortools/routing/simple_graph.h"

namespace operations_research {

class TspLibParser final {
 public:
  // Routing model types (cf. the link above for a description).
  enum Types { TSP, ATSP, SOP, HCP, CVRP, TOUR, UNDEFINED_TYPE };

  TspLibParser();
  // Loads and parses a routing problem from a given file.
  bool LoadFile(const std::string& file_name);
  // Returns the number of nodes in the routing problem stored in a given file.
  int SizeFromFile(const std::string& file_name) const;
  // Returns a function returning edge weights between nodes.
  EdgeWeights GetEdgeWeights() const { return distance_function_; }
  // Returns the index of the depot.
  int depot() const { return depot_; }
  // Returns the number of nodes in the current routing problem.
  int size() const { return size_; }
  // Returns the type of the current routing problem.
  Types type() const { return type_; }
  // Returns the coordinates of the nodes in the current routing problem (if
  // they exist).
  const std::vector<Coordinates3<double>>& coordinates() const {
    return coords_;
  }
  // Returns the capacity of the vehicles in the current routing problem.
  int64_t capacity() const { return capacity_; }
  // Returns the maximal distance vehicles can travel.
  int64_t max_distance() const { return max_distance_; }
  // Returns the demands (or quantities picked up) at each node.
  const std::vector<int64_t>& demands() const { return demands_; }
  // Returns the pairs of nodes corresponding to forced edges (second node is
  // directly after the first).
  const std::set<std::pair<int, int>> fixed_edges() const {
    return fixed_edges_;
  }
  // Returns edges of the graph on which Hamiltonian cycles need to be built.
  // Edges are represented as adjacency lists for each node.
  const std::vector<std::vector<int>>& edges() const { return edges_; }
  // Returns the name of the current routing model.
  const std::string& name() const { return name_; }
  // Returns the comments attached to the data.
  const std::string& comments() const { return comments_; }
  // Build a tour output in TSPLIB95 format from a vector of routes, a route
  // being a sequence of node indices.
  std::string BuildTourFromRoutes(
      const std::vector<std::vector<int>>& routes) const;

 private:
  enum Sections {
    NAME,
    TYPE,
    COMMENT,
    DIMENSION,
    DISTANCE,
    CAPACITY,
    EDGE_DATA_FORMAT,
    EDGE_DATA_SECTION,
    EDGE_WEIGHT_TYPE,
    EDGE_WEIGHT_FORMAT,
    EDGE_WEIGHT_SECTION,
    FIXED_EDGES_SECTION,
    NODE_COORD_TYPE,
    DISPLAY_DATA_TYPE,
    DISPLAY_DATA_SECTION,
    NODE_COORD_SECTION,
    DEPOT_SECTION,
    DEMAND_SECTION,
    END_OF_FILE,
    UNDEFINED_SECTION
  };
  enum EdgeDataFormat { EDGE_LIST, ADJ_LIST };
  enum EdgeWeightTypes {
    EXPLICIT,
    EUC_2D,
    EUC_3D,
    MAX_2D,
    MAX_3D,
    MAN_2D,
    MAN_3D,
    CEIL_2D,
    GEO,
    GEOM,
    ATT,
    XRAY1,
    XRAY2,
    SPECIAL,
    UNDEFINED_EDGE_WEIGHT_TYPE
  };
  enum EdgeWeightFormats {
    FUNCTION,
    FULL_MATRIX,
    UPPER_ROW,
    LOWER_ROW,
    UPPER_DIAG_ROW,
    LOWER_DIAG_ROW,
    UPPER_COL,
    LOWER_COL,
    UPPER_DIAG_COL,
    LOWER_DIAG_COL,
    UNDEFINED_EDGE_WEIGHT_FORMAT
  };

#ifndef SWIG
  TspLibParser(const TspLibParser&) = delete;
  void operator=(const TspLibParser&) = delete;
#endif

  void ParseExplicitFullMatrix(const std::vector<std::string>& words);
  void ParseExplicitUpperRow(const std::vector<std::string>& words);
  void ParseExplicitLowerRow(const std::vector<std::string>& words);
  void ParseExplicitUpperDiagRow(const std::vector<std::string>& words);
  void ParseExplicitLowerDiagRow(const std::vector<std::string>& words);
  void ParseNodeCoord(const std::vector<std::string>& words);
  void SetUpEdgeWeightSection();
  void FinalizeEdgeWeights();
  void ParseSections(const std::vector<std::string>& words);
  void ProcessNewLine(const std::string& line);
  void SetExplicitCost(int from, int to, int64_t cost) {
    if (explicit_costs_.size() != size_ * size_) {
      explicit_costs_.resize(size_ * size_, 0);
    }
    explicit_costs_[from * size_ + to] = cost;
  }

  // Model data
  int64_t size_;
  int64_t capacity_;
  int64_t max_distance_;
  std::vector<int64_t> demands_;
  EdgeWeights distance_function_;
  std::vector<int64_t> explicit_costs_;
  std::set<std::pair<int, int>> fixed_edges_;
  int depot_;
  std::vector<std::vector<int>> edges_;

  // Parsing data
  static const absl::flat_hash_map<std::string, Sections>* const kSections;
  Sections section_;
  static const absl::flat_hash_map<std::string, Types>* const kTypes;
  Types type_;
  static const absl::flat_hash_map<std::string, EdgeDataFormat>* const
      kEdgeDataFormats;
  EdgeDataFormat edge_data_format_;
  static const absl::flat_hash_map<std::string, EdgeWeightTypes>* const
      kEdgeWeightTypes;
  EdgeWeightTypes edge_weight_type_;
  static const absl::flat_hash_map<std::string, EdgeWeightFormats>* const
      kEdgeWeightFormats;
  EdgeWeightFormats edge_weight_format_;
  int edge_row_;
  int edge_column_;
  std::vector<Coordinates3<double>> coords_;
  std::string name_;
  std::string comments_;
  int64_t to_read_;
};

// Class parsing tour (solution) data in TSLIB95 format.

class TspLibTourParser final {
 public:
  TspLibTourParser();
  // Loads and parses a given tour file.
  bool LoadFile(const std::string& file_name);
  // Returns a vector corresponding to the sequence of nodes of the tour.
  const std::vector<int>& tour() const { return tour_; }
  // Returns the size of the tour.
  int size() const { return size_; }
  // Returns the comments attached to the data.
  const std::string& comments() const { return comments_; }

 private:
  enum Sections {
    NAME,
    TYPE,
    COMMENT,
    DIMENSION,
    TOUR_SECTION,
    END_OF_FILE,
    UNDEFINED_SECTION
  };

#ifndef SWIG
  TspLibTourParser(const TspLibTourParser&) = delete;
  void operator=(const TspLibTourParser&) = delete;
#endif

  void ProcessNewLine(const std::string& line);

  static const absl::flat_hash_map<std::string, Sections>* const kSections;
  Sections section_;
  std::string comments_;
  int64_t size_;
  std::vector<int> tour_;
};

// Class parsing tours (solution) data in CVRPlib format.

class CVRPToursParser final {
 public:
  CVRPToursParser();
  // Loads and parses a given tours file.
  bool LoadFile(const std::string& file_name);
  // Returns a vector corresponding to the sequence of nodes of tours.
  const std::vector<std::vector<int>>& tours() const { return tours_; }
  int64_t cost() const { return cost_; }

 private:
  void ProcessNewLine(const std::string& line);

  std::vector<std::vector<int>> tours_;
  int64_t cost_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_TSPLIB_PARSER_H_
