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

#include "ortools/routing/parsers/nearp_parser.h"

#include <array>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/base/numbers.h"
#include "ortools/util/filelineiter.h"

namespace operations_research::routing {

NearpParser::NearpParser() { Initialize(); }

void NearpParser::Initialize() {
  name_.clear();
  comment_.clear();
  num_arcs_ = 0;
  num_edges_ = 0;
  num_nodes_ = 0;
  num_arcs_with_servicing_ = 0;
  num_edges_with_servicing_ = 0;
  num_nodes_with_servicing_ = 0;
  depot_ = 0;
  arc_traversing_costs_.clear();
  edge_traversing_costs_.clear();
  arc_servicing_demands_.clear();
  edge_servicing_demands_.clear();
  node_servicing_demands_.clear();
  num_vehicles_ = 0;
  capacity_ = 0;
  section_ = METADATA;
}

bool NearpParser::LoadFile(absl::string_view file_name) {
  Initialize();
  return ParseFile(file_name);
}

bool NearpParser::ParseFile(absl::string_view file_name) {
  // Only put the first word as header, as the main check is just done on this
  // first word (no ambiguity is possible for well-formed files; a more precise
  // check is done for metadata).
  static auto section_headers = std::array<const char*, 14>({
      "Name",
      "Optimal",  // "value"
      "#Vehicles",
      "Capacity",
      "Depot",  // "Node"
      "#Nodes",
      "#Edges",
      "#Arcs",
      "#Required",  // "N", "E", or "A"
      "ReN.",
      "ReE.",
      "EDGE",
      "ReA.",
      "ARC",
  });

  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    const std::vector<std::string> words =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());

    if (words.empty()) continue;

    if (absl::c_linear_search(section_headers, words[0])) {
      // First, check if a new section has been met.
      if (words[0] == "ReN.") {
        node_servicing_demands_.reserve(NumberOfNodesWithServicing());
        section_ = NODES_WITH_SERVICING;
      } else if (words[0] == "ReE.") {
        edge_traversing_costs_.reserve(NumberOfEdges());
        edge_servicing_demands_.reserve(NumberOfEdgesWithServicing());
        section_ = EDGES_WITH_SERVICING;
      } else if (words[0] == "EDGE") {
        edge_traversing_costs_.reserve(NumberOfEdges());
        section_ = EDGES_WITHOUT_SERVICING;
      } else if (words[0] == "ReA.") {
        arc_traversing_costs_.reserve(NumberOfArcs());
        arc_servicing_demands_.reserve(NumberOfArcsWithServicing());
        section_ = ARCS_WITH_SERVICING;
      } else if (words[0] == "ARC") {
        arc_traversing_costs_.reserve(NumberOfArcs());
        section_ = ARCS_WITHOUT_SERVICING;
      } else {
        if (!ParseMetadataLine(words)) {
          LOG(ERROR) << "Error when parsing a metadata line: " << line;
          return false;
        }
      }
    } else {
      // If no new section is detected, process according to the current state.

      // Is there still data expected? Don't process the line if every element
      // it should contain have been read: there might be some garbage at the
      // end (like comments without delimiter).
      switch (section_) {
        case NODES_WITH_SERVICING:
          if (node_servicing_demands_.size() == NumberOfNodesWithServicing())
            continue;
          break;
        case EDGES_WITH_SERVICING:
          if (edge_servicing_demands_.size() == NumberOfEdgesWithServicing())
            continue;
          break;
        case EDGES_WITHOUT_SERVICING:
          // edge_traversing_costs_ is filled with all edges, whether they need
          // servicing or not. The section EDGES_WITHOUT_SERVICING must be after
          // EDGES_WITH_SERVICING: once edge_traversing_costs_ has one value
          // per edge (coming first from EDGES_WITH_SERVICING, then from
          // EDGES_WITHOUT_SERVICING), no more value should come.
          if (edge_traversing_costs_.size() == NumberOfEdges()) continue;
          break;
        case ARCS_WITH_SERVICING:
          if (arc_servicing_demands_.size() == NumberOfArcsWithServicing())
            continue;
          break;
        case ARCS_WITHOUT_SERVICING:
          // arc_traversing_costs_ is filled with all arcs, do they need
          // servicing or not. The section ARCS_WITHOUT_SERVICING must be after
          // ARCS_WITH_SERVICING: once edge_traversing_costs_ has one value
          // per arc (coming first from ARCS_WITH_SERVICING, then from
          // ARCS_WITHOUT_SERVICING), no more value should come.
          if (arc_traversing_costs_.size() == NumberOfArcs()) continue;
          break;
        default:
          break;
      }

      // Data is still expected: parse the current line according to the state.
      switch (section_) {
        case NODES_WITH_SERVICING:
          if (!ParseNode(line)) {
            LOG(ERROR) << "Could not parse line in required nodes: " << line;
            return false;
          }
          break;
        case EDGES_WITH_SERVICING:
          if (!ParseEdge(line, true)) {
            LOG(ERROR) << "Could not parse line in required edges: " << line;
            return false;
          }
          break;
        case EDGES_WITHOUT_SERVICING:
          if (!ParseEdge(line, false)) {
            LOG(ERROR) << "Could not parse line in edges: " << line;
            return false;
          }
          break;
        case ARCS_WITH_SERVICING:
          if (!ParseArc(line, true)) {
            LOG(ERROR) << "Could not parse line in required arcs: " << line;
            return false;
          }
          break;
        case ARCS_WITHOUT_SERVICING:
          if (!ParseArc(line, false)) {
            LOG(ERROR) << "Could not parse line in arcs: " << line;
            return false;
          }
          break;
        default:
          LOG(ERROR) << "Could not parse line outside node-edge-arc lists: "
                     << line;
          return false;
      }
    }
  }

  return section_ != METADATA;
}

namespace {
std::optional<int64_t> ParseNodeIndex(std::string_view text);

struct ArcOrEdge {
  const int64_t tail;
  const int64_t head;
  const int64_t traversing_cost;
  const int64_t servicing_demand;
  const int64_t servicing_cost;
};

std::optional<ArcOrEdge> ParseArcOrEdge(std::string_view line,
                                        bool with_servicing);
}  // namespace

bool NearpParser::ParseMetadataLine(const std::vector<std::string>& words) {
  if (words[0] == "Name") {
    name_ = absl::StrJoin(words.begin() + 1, words.end(), " ");
  } else if (words[0] == "Optimal" && words[1] == "value") {
    comment_ = absl::StrJoin(words.begin() + 2, words.end(), " ");
  } else if (words[0] == "#Vehicles") {
    num_vehicles_ = strings::ParseLeadingInt32Value(words[1], -1);
    // -1 indicates that the number of vehicles is unknown.
    if (num_vehicles_ < -1) {
      LOG(ERROR) << "Error when parsing the number of vehicles: " << words[1];
      return false;
    }
  } else if (words[0] == "Capacity") {
    capacity_ = strings::ParseLeadingInt64Value(words[1], -1);
    if (capacity_ <= 0) {
      LOG(ERROR) << "Error when parsing the capacity: " << words[1];
      return false;
    }
  } else if (words[0] == "Depot" && words[1] == "Node") {
    const std::optional<int64_t> depot = ParseNodeIndex(words[2]);
    if (!depot.has_value()) {
      LOG(ERROR) << "Error when parsing the depot: " << words[1];
      return false;
    }
    depot_ = depot.value();
  } else if (words[0] == "#Nodes") {
    num_nodes_ = strings::ParseLeadingInt32Value(words[1], -1);
    if (num_nodes_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of nodes: " << words[1];
      return false;
    }
  } else if (words[0] == "#Edges") {
    num_edges_ = strings::ParseLeadingInt32Value(words[1], -1);
    if (num_edges_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of edges: " << words[1];
      return false;
    }
  } else if (words[0] == "#Arcs") {
    num_arcs_ = strings::ParseLeadingInt32Value(words[1], -1);
    if (num_arcs_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of arcs: " << words[1];
      return false;
    }
  } else if (words[0] == "#Required" && words[1] == "N") {
    num_nodes_with_servicing_ = strings::ParseLeadingInt32Value(words[2], -1);
    if (num_nodes_with_servicing_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of nodes with servicing: "
                 << words[1];
      return false;
    }
  } else if (words[0] == "#Required" && words[1] == "E") {
    num_edges_with_servicing_ = strings::ParseLeadingInt32Value(words[2], -1);
    if (num_edges_with_servicing_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of edges with servicing: "
                 << words[1];
      return false;
    }
  } else if (words[0] == "#Required" && words[1] == "A") {
    num_arcs_with_servicing_ = strings::ParseLeadingInt32Value(words[2], -1);
    if (num_arcs_with_servicing_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of arcs with servicing: "
                 << words[1];
      return false;
    }
  } else {
    LOG(ERROR) << "Unrecognized metadata line: " << absl::StrJoin(words, " ");
    return false;
  }
  return true;
}

bool NearpParser::ParseArc(std::string_view line, bool with_servicing) {
  std::optional<ArcOrEdge> parsed_arc = ParseArcOrEdge(line, with_servicing);
  if (!parsed_arc.has_value()) {
    return false;
  }

  Arc arc{parsed_arc->tail, parsed_arc->head};
  arc_traversing_costs_[arc] = parsed_arc->traversing_cost;

  if (with_servicing) {
    arc_servicing_demands_[arc] = parsed_arc->servicing_demand;
    arc_servicing_costs_[arc] = parsed_arc->servicing_cost;
  }

  return true;
}

bool NearpParser::ParseEdge(std::string_view line, bool with_servicing) {
  std::optional<ArcOrEdge> parsed_edge = ParseArcOrEdge(line, with_servicing);
  if (!parsed_edge.has_value()) {
    return false;
  }

  Edge edge{parsed_edge->tail, parsed_edge->head};
  edge_traversing_costs_[edge] = parsed_edge->traversing_cost;

  if (with_servicing) {
    edge_servicing_demands_[edge] = parsed_edge->servicing_demand;
    edge_servicing_costs_[edge] = parsed_edge->servicing_cost;
  }

  return true;
}

bool NearpParser::ParseNode(std::string_view line) {
  const std::vector<std::string> words =
      absl::StrSplit(line, absl::ByAnyChar(" :\t(),"), absl::SkipEmpty());

  // Parse the name and ID.
  const int64_t id = strings::ParseLeadingInt64Value(words[0].substr(1), 0) - 1;
  if (id < 0) {
    LOG(ERROR) << "Error when parsing the node name: " << words[0];
    return false;
  }

  // Parse the servicing details if needed.
  const int64_t servicing_demand =
      strings::ParseLeadingInt64Value(words[1], -1);
  if (servicing_demand < 0) {
    LOG(ERROR) << "Error when parsing the node servicing demand: " << words[1];
    return false;
  }

  const int64_t servicing_cost = strings::ParseLeadingInt64Value(words[2], -1);
  if (servicing_cost < 0) {
    LOG(ERROR) << "Error when parsing the node servicing cost: " << words[1];
    return false;
  }

  // Once the values have been parsed successfully, save them.
  node_servicing_demands_.insert({id, servicing_demand});
  node_servicing_costs_.insert({id, servicing_cost});

  return true;
}

namespace {
std::optional<int64_t> ParseNodeIndex(std::string_view text) {
  const int64_t node = strings::ParseLeadingInt64Value(text, -1);
  if (node < 0) {
    LOG(ERROR) << "Could not parse node index: " << text;
    return std::nullopt;
  }
  return {node - 1};
}

std::optional<ArcOrEdge> ParseArcOrEdge(std::string_view line,
                                        bool with_servicing) {
  const std::vector<std::string> words =
      absl::StrSplit(line, absl::ByAnyChar(" :\t(),"), absl::SkipEmpty());

  // Parse the name.

  // Parse the tail and the head of the arc/edge.
  std::optional<int64_t> opt_tail = ParseNodeIndex(words[1]);
  if (!opt_tail.has_value()) {
    LOG(ERROR) << "Error when parsing the tail node: " << words[1];
    return {};
  }
  const int64_t tail = opt_tail.value();

  std::optional<int64_t> opt_head = ParseNodeIndex(words[2]);
  if (!opt_head.has_value()) {
    LOG(ERROR) << "Error when parsing the head node: " << words[2];
    return {};
  }
  const int64_t head = opt_head.value();

  if (tail == head) {
    LOG(ERROR) << "The head and tail nodes are identical: " << line;
    return {};
  }

  // Parse the traversing cost.
  const int64_t traversing_cost = strings::ParseLeadingInt64Value(words[3], -1);
  if (traversing_cost < 0) {
    LOG(ERROR) << "Error when parsing the traversing cost: " << words[3];
    return {};
  }

  // Ensure there are no extraneous elements.
  const int64_t next_id = (with_servicing) ? 6 : 4;
  if (words.size() > next_id) {
    LOG(ERROR) << "Extraneous elements in line, starting with: "
               << words[next_id];
    return {};
  }

  // Parse the servicing details if needed and return the elements.
  if (!with_servicing) {
    return ArcOrEdge{tail, head, traversing_cost,
                     /*servicing_demand=*/-1,
                     /*servicing_cost=*/-1};
  } else {
    const int64_t servicing_demand =
        strings::ParseLeadingInt64Value(words[4], -1);
    if (servicing_demand < 0) {
      LOG(ERROR) << "Error when parsing the servicing demand: " << words[4];
      return {};
    }

    const int64_t servicing_cost =
        strings::ParseLeadingInt64Value(words[5], -1);
    if (servicing_cost < 0) {
      LOG(ERROR) << "Error when parsing the servicing cost: " << words[5];
      return {};
    }

    return ArcOrEdge{tail, head, traversing_cost, servicing_demand,
                     servicing_cost};
  }
}
}  // namespace

std::string NearpParser::GetArcName(Arc arc) const {
  if (arc_servicing_costs_.contains(arc)) {
    int64_t arc_position = std::distance(arc_servicing_demands_.begin(),
                                         arc_servicing_demands_.find(arc));
    CHECK_LT(arc_position, arc_servicing_demands_.size());
    return absl::StrCat("A", arc_position + 1);
  } else {
    int64_t arc_position = std::distance(arc_traversing_costs_.begin(),
                                         arc_traversing_costs_.find(arc));
    CHECK_LT(arc_position, arc_traversing_costs_.size());
    return absl::StrCat("NrA", arc_position - num_arcs_with_servicing_ + 1);
  }
}

std::string NearpParser::GetEdgeName(Edge edge) const {
  if (edge_servicing_costs_.contains(edge)) {
    int64_t edge_position = std::distance(edge_servicing_demands_.begin(),
                                          edge_servicing_demands_.find(edge));
    CHECK_LT(edge_position, edge_servicing_demands_.size());
    return absl::StrCat("E", edge_position + 1);
  } else {
    int64_t edge_position = std::distance(edge_traversing_costs_.begin(),
                                          edge_traversing_costs_.find(edge));
    CHECK_LT(edge_position, edge_traversing_costs_.size());
    return absl::StrCat("NrE", edge_position - num_edges_with_servicing_ + 1);
  }
}
}  // namespace operations_research::routing
