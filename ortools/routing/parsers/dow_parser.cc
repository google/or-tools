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

#include "ortools/routing/parsers/dow_parser.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "ortools/base/filesystem.h"
#include "ortools/base/options.h"
#include "ortools/routing/parsers/capacity_planning.pb.h"
#include "ortools/util/filelineiter.h"

namespace operations_research::routing {
::absl::Status ReadFile(absl::string_view file_name,
                        CapacityPlanningInstance* request) {
  if (!file::Exists(file_name, file::Defaults()).ok()) {
    return absl::NotFoundError(absl::StrCat(file_name, " not found"));
  }
  int line_num = 0;
  int num_nodes = 0;
  int num_arcs = 0;
  int num_commodities = 0;
  int arc_num = 0;
  int commodity_num = 0;
  for (const std::string& line :
       FileLines(file_name, FileLineIterator::REMOVE_INLINE_CR)) {
    if (line == "MULTIGEN.DAT:") {
      CHECK_EQ(line_num, 0);
      ++line_num;
      continue;
    }
    const std::vector<std::string> fields =
        absl::StrSplit(line, absl::ByAnyChar(" \t"), absl::SkipEmpty());
    if (fields.size() == 3) {
      if (line_num == 1) {  // Sizes.
        CHECK(absl::SimpleAtoi(fields[0], &num_nodes));
        CHECK(absl::SimpleAtoi(fields[1], &num_arcs));
        CHECK(absl::SimpleAtoi(fields[2], &num_commodities));
        VLOG(1) << "num_nodes = " << num_nodes << ", num_arcs  = " << num_arcs
                << ", num_commodities = " << num_commodities;
      } else {  // Demand per commodity. The commodity number is implicit.
        CHECK_GT(line_num, num_arcs + 1);
        CHECK_LE(line_num, num_arcs + num_commodities + 1);
        int from_node;
        CHECK(absl::SimpleAtoi(fields[0], &from_node));
        int to_node;
        CHECK(absl::SimpleAtoi(fields[1], &to_node));
        int64_t demand;
        CHECK(absl::SimpleAtoi(fields[2], &demand));
        CHECK_GT(demand, 0);
        ++commodity_num;
        request->mutable_commodities()->add_from_node(from_node);
        request->mutable_commodities()->add_to_node(to_node);
        request->mutable_commodities()->add_demand(demand);
      }
    } else if (fields.size() == 7) {  // Information per arc. Arc number is
                                      // implicit.
      CHECK_GT(line_num, 1);
      CHECK_LE(line_num, num_arcs + 1);
      CHECK_LT(arc_num, num_arcs);
      int from_node;
      CHECK(absl::SimpleAtoi(fields[0], &from_node));
      int to_node;
      CHECK(absl::SimpleAtoi(fields[1], &to_node));
      int variable_cost;
      CHECK(absl::SimpleAtoi(fields[2], &variable_cost));
      int capacity;
      CHECK(absl::SimpleAtoi(fields[3], &capacity));
      int fixed_cost;
      CHECK(absl::SimpleAtoi(fields[4], &fixed_cost));
      int unused;
      CHECK(absl::SimpleAtoi(fields[5], &unused));
      CHECK(absl::SimpleAtoi(fields[6], &unused));
      ++arc_num;
      request->mutable_topology()->add_from_node(from_node);
      request->mutable_topology()->add_to_node(to_node);
      request->mutable_topology()->add_variable_cost(variable_cost);
      request->mutable_topology()->add_capacity(capacity);
      request->mutable_topology()->add_fixed_cost(fixed_cost);
    }
    ++line_num;
  }
  CHECK_EQ(commodity_num, num_commodities);
  return absl::OkStatus();
}

}  // namespace operations_research::routing
