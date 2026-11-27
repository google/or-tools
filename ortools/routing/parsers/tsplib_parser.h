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

// A TSPLIB parser. The TSPLIB is a library containing Traveling
// Salesman Problems and other vehicle routing problems.
// Limitations:
// - only TSP and CVRP files are currently supported.
// - XRAY1, XRAY2 and SPECIAL edge weight types are not supported.
//
// Takes as input a data file, potentially gzipped. The data must
// follow the TSPLIB95 format (described at
// http://www.iwr.uni-heidelberg.de/groups/comopt/software/TSPLIB95/DOC.PS).

#ifndef ORTOOLS_ROUTING_PARSERS_TSPLIB_PARSER_H_
#define ORTOOLS_ROUTING_PARSERS_TSPLIB_PARSER_H_

#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/routing/parsers/simple_graph.h"

namespace operations_research::routing {

class TspLibParser final {
 public:
  // Routing model types (cf. the link above for a description).
  enum Types { TSP, ATSP, SOP, HCP, CVRP, TOUR, UNDEFINED_TYPE };

  // Loads and parses a routing problem from a given file.
  static absl::StatusOr<TspLibParser> LoadFile(absl::string_view file_name);

  TspLibParser();
  TspLibParser(const TspLibParser&) = delete;
  TspLibParser& operator=(const TspLibParser&) = delete;
  TspLibParser(TspLibParser&&) = default;
  TspLibParser& operator=(TspLibParser&&) = default;

  // Returns a function returning edge weights between nodes.
  // Note: The returned callback references this TspLibParser instance and must
  // not outlive it.
  EdgeWeights GetEdgeWeights() const ABSL_ATTRIBUTE_LIFETIME_BOUND;
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
  int64_t capacity() const {
    return capacity_.value_or(std::numeric_limits<int64_t>::max());
  }
  // Returns the maximal distance vehicles can travel.
  int64_t max_distance() const { return max_distance_; }
  // Returns the demands (or quantities picked up) at each node.
  const std::vector<int64_t>& demands() const { return demands_; }
  // Returns the pairs of nodes corresponding to forced edges (second node is
  // directly after the first).
  std::set<std::pair<int, int>> fixed_edges() const { return fixed_edges_; }
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
      absl::Span<const std::vector<int>> routes) const;

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
  enum EdgeDataFormat { UNDEFINED_EDGE_DATA_FORMAT, EDGE_LIST, ADJ_LIST };
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

  absl::Status ParseExplicitFullMatrix(absl::Span<const std::string> words);
  absl::Status ParseExplicitUpperRow(absl::Span<const std::string> words,
                                     bool ignore_diagonal);
  absl::Status ParseExplicitLowerRow(absl::Span<const std::string> words,
                                     bool ignore_diagonal);
  absl::Status ParseNodeCoord(absl::Span<const std::string> words);
  absl::Status SetUpEdgeWeightSection();
  absl::Status ValidateEdgeWeights() const;
  void CheckNodeBounds(int from, int to) const;
  absl::Status ParseSections(absl::Span<const std::string> words);
  absl::Status ProcessNewLine(absl::string_view line);
  absl::Status SetExplicitCost(int from, int to, int64_t cost);

  // Model data
  int size_;
  std::optional<int64_t> capacity_;
  int64_t max_distance_;
  std::vector<int64_t> demands_;
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
  bool valid_section_found_ = false;
  bool sop_dimension_read_ = false;
  bool explicit_edge_weights_complete_ = false;
  int64_t edge_count_ = 0;
};

// Class parsing tour (solution) data in TSLIB95 format.

class TspLibTourParser final {
 public:
  TspLibTourParser();
  TspLibTourParser(const TspLibTourParser&) = delete;
  TspLibTourParser& operator=(const TspLibTourParser&) = delete;
  // Loads and parses a given tour file.
  absl::Status LoadFile(absl::string_view file_name);
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

  absl::Status ProcessNewLine(absl::string_view line);

  static const absl::flat_hash_map<std::string, Sections>* const kSections;
  Sections section_;
  std::string comments_;
  int size_;
  std::vector<int> tour_;
};

// Class parsing tours (solution) data in CVRPlib format.

class CVRPToursParser final {
 public:
  CVRPToursParser();
  // Loads and parses a given tours file.
  absl::Status LoadFile(absl::string_view file_name);
  // Returns a vector corresponding to the sequence of nodes of tours.
  const std::vector<std::vector<int>>& tours() const { return tours_; }
  int64_t cost() const { return cost_; }

 private:
  absl::Status ProcessNewLine(absl::string_view line);

  std::vector<std::vector<int>> tours_;
  int64_t cost_;
  int64_t total_nodes_ = 0;
};
}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_PARSERS_TSPLIB_PARSER_H_
