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

#include "ortools/routing/parsers/carp_parser.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/numbers.h"
#include "ortools/util/filelineiter.h"

namespace operations_research::routing {

CarpParser::CarpParser() { Initialize(); }

void CarpParser::Initialize() {
  name_.clear();
  comment_.clear();
  number_of_nodes_ = 0;
  number_of_edges_with_servicing_ = 0;
  number_of_edges_without_servicing_ = 0;
  total_servicing_cost_ = 0;
  depot_ = 0;
  traversing_costs_.clear();
  servicing_demands_.clear();
  n_vehicles_ = 0;
  capacity_ = 0;
  section_ = METADATA;
}

bool CarpParser::LoadFile(absl::string_view file_name) {
  Initialize();
  return ParseFile(file_name);
}

bool CarpParser::ParseFile(absl::string_view file_name) {
  static auto section_headers = std::array<const char*, 12>({
      "NOMBRE",
      "COMENTARIO",
      "VERTICES",
      "ARISTAS_REQ",
      "ARISTAS_NOREQ",
      "VEHICULOS",
      "CAPACIDAD",
      "TIPO_COSTES_ARISTAS",
      "COSTE_TOTAL_REQ",
      "LISTA_ARISTAS_REQ",
      "LISTA_ARISTAS_NOREQ",
      "DEPOSITO",
  });

  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    const std::vector<std::string> words =
        absl::StrSplit(line, absl::ByAnyChar(" :\t"), absl::SkipEmpty());

    if (absl::c_linear_search(section_headers, words[0])) {
      // First, check if a new section has been met.
      if (words[0] == "LISTA_ARISTAS_REQ") {
        traversing_costs_.reserve(NumberOfEdges());
        servicing_demands_.reserve(NumberOfEdgesWithServicing());
        section_ = ARCS_WITH_SERVICING;
      } else if (words[0] == "LISTA_ARISTAS_NOREQ") {
        traversing_costs_.reserve(NumberOfEdges());
        section_ = ARCS_WITHOUT_SERVICING;
      } else {
        if (!ParseMetadataLine(words)) {
          LOG(ERROR) << "Error when parsing the following metadata line: "
                     << line;
          return false;
        }
      }
    } else {
      // If no new section is detected, process according to the current state.
      switch (section_) {
        case ARCS_WITH_SERVICING:
          if (!ParseEdge(line, true)) {
            LOG(ERROR) << "Could not parse line in LISTA_ARISTAS_REQ: " << line;
            return false;
          }
          break;
        case ARCS_WITHOUT_SERVICING:
          if (!ParseEdge(line, false)) {
            LOG(ERROR) << "Could not parse line in LISTA_ARISTAS_NOREQ: "
                       << line;
            return false;
          }
          break;
        default:
          LOG(ERROR) << "Could not parse line outside edge lists: " << line;
          return false;
      }
    }
  }

  return !servicing_demands_.empty();
}

namespace {
std::optional<int64_t> ParseNodeIndex(std::string_view text);
}  // namespace

bool CarpParser::ParseMetadataLine(absl::Span<const std::string> words) {
  if (words[0] == "NOMBRE") {
    name_ = absl::StrJoin(words.begin() + 1, words.end(), " ");
  } else if (words[0] == "COMENTARIO") {
    comment_ = absl::StrJoin(words.begin() + 1, words.end(), " ");
  } else if (words[0] == "VERTICES") {
    number_of_nodes_ = strings::ParseLeadingInt64Value(words[1], -1);
    if (number_of_nodes_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of nodes: " << words[1];
      return false;
    }
  } else if (words[0] == "ARISTAS_REQ") {
    number_of_edges_with_servicing_ =
        strings::ParseLeadingInt64Value(words[1], -1);
    if (number_of_edges_with_servicing_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of edges with servicing: "
                 << words[1];
      return false;
    }
  } else if (words[0] == "ARISTAS_NOREQ") {
    number_of_edges_without_servicing_ =
        strings::ParseLeadingInt64Value(words[1], -1);
    if (number_of_edges_without_servicing_ < 0) {
      // It is possible to have a valid instance with zero edges that have no
      // servicing (i.e. number_of_edges_without_servicing_ == 0).
      LOG(ERROR) << "Error when parsing the number of edges without servicing: "
                 << words[1];
      return false;
    }
  } else if (words[0] == "VEHICULOS") {
    n_vehicles_ = strings::ParseLeadingInt64Value(words[1], -1);
    if (n_vehicles_ <= 0) {
      LOG(ERROR) << "Error when parsing the number of vehicles: " << words[1];
      return false;
    }
  } else if (words[0] == "CAPACIDAD") {
    capacity_ = strings::ParseLeadingInt64Value(words[1], -1);
    if (capacity_ <= 0) {
      LOG(ERROR) << "Error when parsing the capacity: " << words[1];
      return false;
    }
  } else if (words[0] == "TIPO_COSTES_ARISTAS") {
    if (words[1] != "EXPLICITOS") {
      // Actually, this is the only defined value for this file format.
      LOG(ERROR) << "Value of TIPO_COSTES_ARISTAS is unexpected, only "
                    "EXPLICITOS is supported, but "
                 << words[1] << " was found";
      return false;
    }
  } else if (words[0] == "COSTE_TOTAL_REQ") {
    total_servicing_cost_ = strings::ParseLeadingInt64Value(words[1], -1);
    if (total_servicing_cost_ == -1) {
      LOG(ERROR) << "Error when parsing the total servicing cost: " << words[1];
      return false;
    }
  } else if (words[0] == "DEPOSITO") {
    // Supposed to be the last value of the file.
    const std::optional<int64_t> depot = ParseNodeIndex(words[1]);
    if (!depot.has_value()) {
      LOG(ERROR) << "Error when parsing the depot: " << words[1];
      return false;
    }
    depot_ = depot.value();
  }
  return true;
}

bool CarpParser::ParseEdge(std::string_view line, bool with_servicing) {
  const std::vector<std::string> words =
      absl::StrSplit(line, absl::ByAnyChar(" :\t(),"), absl::SkipEmpty());

  // Parse the edge.
  std::optional<int64_t> opt_head = ParseNodeIndex(words[0]);
  if (!opt_head.has_value()) {
    LOG(ERROR) << "Error when parsing the head node: " << words[0];
    return false;
  }
  const int64_t head = opt_head.value();

  std::optional<int64_t> opt_tail = ParseNodeIndex(words[1]);
  if (!opt_tail.has_value()) {
    LOG(ERROR) << "Error when parsing the tail node: " << words[1];
    return false;
  }
  const int64_t tail = opt_tail.value();

  if (head == tail) {
    LOG(ERROR) << "The head and tail nodes are identical: " << line;
    return false;
  }

  // Parse the cost.
  if (words[2] != "coste") {
    LOG(ERROR) << "Unexpected keyword: " << words[2];
    return false;
  }
  const int64_t cost = strings::ParseLeadingInt64Value(words[3], -1);
  traversing_costs_[{tail, head}] = cost;

  // Parse the servicing if needed.
  if (with_servicing) {
    if (words[4] != "demanda") {
      LOG(ERROR) << "Unexpected keyword: " << words[2];
      return false;
    }
    const int64_t servicing = strings::ParseLeadingInt64Value(words[5], -1);
    servicing_demands_[{tail, head}] = servicing;
  }

  // Ensure there are no extraneous elements.
  const int64_t next_id = (with_servicing) ? 6 : 4;
  if (words.size() > next_id) {
    LOG(ERROR) << "Extraneous elements in line, starting with: "
               << words[next_id];
    return false;
  }

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
}  // namespace
}  // namespace operations_research::routing
