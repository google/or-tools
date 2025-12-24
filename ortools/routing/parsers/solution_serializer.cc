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

#include "ortools/routing/parsers/solution_serializer.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/file.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/routing/parsers/simple_graph.h"

namespace operations_research::routing {

RoutingOutputFormat RoutingOutputFormatFromString(std::string_view format) {
  const std::string format_normalized =
      absl::AsciiStrToLower(absl::StripAsciiWhitespace(format));
  if (format_normalized == "tsplib") return RoutingOutputFormat::kTSPLIB;
  if (format_normalized == "cvrplib") return RoutingOutputFormat::kCVRPLIB;
  if (format_normalized == "carplib") return RoutingOutputFormat::kCARPLIB;
  if (format_normalized == "nearplib") return RoutingOutputFormat::kNEARPLIB;
  return RoutingOutputFormat::kNone;
}

// Helper for FromSplitRoutes.
namespace {
std::vector<RoutingSolution::Route> RoutesFromVector(
    absl::Span<const std::vector<int64_t>> routes,
    std::optional<int64_t> depot = std::nullopt);
}  // namespace

std::vector<std::vector<int64_t>> RoutingSolution::SplitRoutes(
    absl::Span<const int64_t> solution, int64_t separator) {
  // The solution vector separates routes by -1: split this vector into a vector
  // per route, where the other helpers can make the rest of the way to a proper
  // RoutingSolution object.
  std::vector<std::vector<int64_t>> routes;
  int64_t current_route = 0;
  for (int64_t node : solution) {
    if (routes.size() == current_route) {
      routes.emplace_back(std::vector<int64_t>());
    }

    if (node == separator) {
      current_route += 1;
    } else {
      routes[current_route].emplace_back(node);
    }
  }
  return routes;
}

RoutingSolution RoutingSolution::FromSplitRoutes(
    absl::Span<const std::vector<int64_t>> routes,
    std::optional<int64_t> depot) {
  std::vector<int64_t> total_demands(routes.size(), -1);
  std::vector<int64_t> total_distances(routes.size(), -1);

  return {RoutesFromVector(routes, depot), total_demands, total_distances};
}

int64_t RoutingSolution::NumberOfNonemptyRoutes() const {
  int64_t num_nonempty_routes = 0;
  for (const Route& route : routes_) {
    if (!route.empty()) num_nonempty_routes++;
  }
  return num_nonempty_routes;
}

void RoutingSolution::WriteToSolutionFile(RoutingOutputFormat format,
                                          const std::string& file_name) const {
  File* file;
  CHECK_OK(file::Open(file_name, "w", &file, file::Defaults()))
      << "Could not open the solution file '" << file_name << "'";
  CHECK_OK(file::WriteString(file, SerializeToSolutionFile(format),
                             file::Defaults()))
      << "Could not write the solution file '" << file_name << "'";
  CHECK_OK(file->Close(file::Defaults()));
}

std::string RoutingSolution::SerializeToTSPLIBString() const {
  std::string tour_out;
  for (const Route& route : routes_) {
    if (route.empty()) continue;

    for (const Event& event : route) {
      if (event.type != RoutingSolution::Event::Type::kEnd) {
        absl::StrAppendFormat(&tour_out, "%d\n", event.arc.head());
      }
    }
    absl::StrAppendFormat(&tour_out, "-1\n");
  }
  return tour_out;
}

std::string RoutingSolution::SerializeToTSPLIBSolutionFile() const {
  // Determine the number of nodes as the maximum index of a node in the
  // solution, plus one (due to TSPLIB being 1-based and C++ 0-based).
  int64_t number_of_nodes = 0;
  for (const Route& route : routes_) {
    for (const Event& event : route) {
      if (event.arc.tail() > number_of_nodes) {
        number_of_nodes = event.arc.tail();
      }
      if (event.arc.head() > number_of_nodes) {
        number_of_nodes = event.arc.head();
      }
    }
  }
  number_of_nodes += 1;

  std::string tour_out;
  absl::StrAppendFormat(&tour_out, "NAME : %s\n", name_);
  absl::StrAppendFormat(&tour_out, "COMMENT : Length = %d; Total time = %f s\n",
                        total_distance_, total_time_);
  absl::StrAppendFormat(&tour_out, "TYPE : TOUR\n");
  absl::StrAppendFormat(&tour_out, "DIMENSION : %d\n", number_of_nodes);
  absl::StrAppendFormat(&tour_out, "TOUR_SECTION\n");
  absl::StrAppendFormat(&tour_out, "%s", SerializeToTSPLIBString());
  absl::StrAppendFormat(&tour_out, "EOF");
  return tour_out;
}

namespace {
std::string SerializeRouteToCVRPLIBString(const RoutingSolution::Route& route);
}  // namespace

std::string RoutingSolution::SerializeToCVRPLIBString() const {
  std::string tour_out;  // The complete solution.
  int route_index = 1;   // Index of the route being written.

  for (const Route& route : routes_) {
    if (route.empty()) continue;
    std::string current_route = SerializeRouteToCVRPLIBString(route);

    // Output the current route only if it is not empty.
    if (!current_route.empty()) {
      absl::StrAppendFormat(&tour_out, "Route #%d: %s\n", route_index++,
                            absl::StripAsciiWhitespace(current_route));
    }
  }
  return tour_out;
}

std::string RoutingSolution::SerializeToCVRPLIBSolutionFile() const {
  std::string tour_out = SerializeToCVRPLIBString();
  absl::StrAppendFormat(&tour_out, "Cost %d", total_cost_);
  return tour_out;
}

std::string RoutingSolution::SerializeToCARPLIBString() const {
  std::string tour_out;             // The complete solution.
  int64_t num_out_route = 1;        // Index of the route being written.
  int64_t num_iteration_route = 0;  // Index of the route being considered.
  int64_t depot;

  for (const Route& route : routes_) {
    std::string current_route;

    for (const RoutingSolution::Event& event : route) {
      std::string type;
      switch (event.type) {
        case RoutingSolution::Event::Type::kStart:
          ABSL_FALLTHROUGH_INTENDED;
        case RoutingSolution::Event::Type::kEnd:
          CHECK_EQ(event.arc.tail(), event.arc.head());
          depot = event.arc.tail();
          type = "D";
          break;
        case RoutingSolution::Event::Type::kServeArc:
        case RoutingSolution::Event::Type::kServeEdge:
          ABSL_FALLTHROUGH_INTENDED;
        case RoutingSolution::Event::Type::kServeNode:
          // The only difference is in the arc: when serving a node, both the
          // head and the tail are the node being served.
          type = "S";
          break;
        case RoutingSolution::Event::Type::kTransit:
          // Not present in CARPLIB output.
          break;
      }

      if (!type.empty()) {
        absl::StrAppendFormat(&current_route, "(%s %d,%d,%d) ", type,
                              event.demand_id, event.arc.tail() + 1,
                              event.arc.head() + 1);
      }
    }

    // Output the current route only if it is not empty.
    if (!route.empty()) {
      const int64_t day = 1;
      const int64_t num_events = std::count_if(
          route.begin(), route.end(), [](const RoutingSolution::Event& event) {
            // Bare transitions are not output in CARPLIB, don't count them.
            return event.type != RoutingSolution::Event::Type::kTransit;
          });

      absl::StrAppendFormat(
          &tour_out, "%d %d %d %d %d %d %s\n",
          depot,  // Use a 0-based encoding for the depot here.
          day, num_out_route, total_demands_[num_iteration_route],
          total_distances_[num_iteration_route], num_events,
          absl::StripAsciiWhitespace(current_route));

      num_out_route += 1;
    }

    num_iteration_route += 1;
  }
  absl::StripTrailingAsciiWhitespace(&tour_out);
  return tour_out;
}

std::string RoutingSolution::SerializeToCARPLIBSolutionFile() const {
  std::string solution;
  absl::StrAppendFormat(&solution, "%d\n", total_cost_);
  absl::StrAppendFormat(&solution, "%d\n", NumberOfNonemptyRoutes());
  absl::StrAppendFormat(&solution, "%f\n", total_time_);
  absl::StrAppend(&solution, SerializeToCARPLIBString());
  return solution;
}

std::string RoutingSolution::SerializeToNEARPLIBString() const {
  std::string tour_out;     // The complete solution.
  int64_t route_index = 1;  // Index of the route being written.

  for (const Route& route : routes_) {
    std::string current_route;
    int64_t current_node = -2;  // Holds the last node that was output, i.e.
    // where the vehicle is located at the beginning of each iteration. -1 is
    // used for the depot, hence an even lower value.

    // Skip empty routes.
    if (route.size() <= 1) continue;
    if (route.size() == 2 &&
        route[0].type == RoutingSolution::Event::Type::kStart &&
        route[1].type == RoutingSolution::Event::Type::kEnd)
      continue;

    // Print the nodes that are traversed only when they are a depot or some end
    // of a serviced arc/edge, without repeating nodes when two consecutive
    // serviced arcs/edges are incident to the same node in the middle.
    // Hence, current_node is used to determine whether the sequence of
    // arcs/edges is continued or should start over.
    // Only set current_node when a sequence should be continued (e.g., not
    // when only traversing an arc/edge).
    for (const RoutingSolution::Event& event : route) {
      switch (event.type) {
        case RoutingSolution::Event::Type::kStart:
          // Always start at the depot.
          CHECK_EQ(event.arc.tail(), event.arc.head());
          current_node = event.arc.tail();
          absl::StrAppendFormat(&current_route, "%d", event.arc.tail() + 1);
          break;
        case RoutingSolution::Event::Type::kEnd:
          // Always print the end depot.
          CHECK_EQ(event.arc.tail(), event.arc.head());
          if (current_node != event.arc.tail()) {
            absl::StrAppendFormat(&current_route, " %d", event.arc.tail() + 1);
          }
          break;
        case RoutingSolution::Event::Type::kServeArc:
          ABSL_FALLTHROUGH_INTENDED;
        case RoutingSolution::Event::Type::kServeEdge:
          CHECK(!event.arc_name.empty())
              << "Arc " << event.arc.tail() << "-" << event.arc.head()
              << " does not have a name in the solution object.";

          // TODO(user): print the name of the node when it is served
          // (i.e. there is a kServeNode event just after). For now, it's only
          // done when the node happens before.
          if (current_node == event.arc.tail()) {
            // Direct continuation of the path: just add a hyphen and go on.
            absl::StrAppendFormat(&current_route, "-%s-%d", event.arc_name,
                                  event.arc.head() + 1);
          } else {
            // Some part of the path is not explicitly output before the
            // previous node and the one after this edge is served.
            absl::StrAppendFormat(&current_route, " %d-%s-%d",
                                  event.arc.tail() + 1, event.arc_name,
                                  event.arc.head() + 1);
          }
          current_node = event.arc.head();
          break;
        case RoutingSolution::Event::Type::kServeNode:
          CHECK_EQ(event.arc.tail(), event.arc.head());
          absl::StrAppendFormat(&current_route, " N%d", event.arc.head() + 1);
          current_node = event.arc.head();
          break;
        case RoutingSolution::Event::Type::kTransit:
          current_node = -2;
          break;
      }
    }

    // Output the current route only if it is not empty.
    if (!current_route.empty()) {
      absl::StrAppendFormat(&tour_out, "Route #%d : %s\n", route_index++,
                            absl::StripAsciiWhitespace(current_route));
    }
  }
  absl::StripTrailingAsciiWhitespace(&tour_out);
  return tour_out;
}

std::string RoutingSolution::SerializeToNEARPLIBSolutionFile() const {
  const std::string date =
      absl::FormatTime("%B %d, %E4Y", absl::Now(), absl::LocalTimeZone());

  std::string solution;
  absl::StrAppendFormat(&solution, "Instance name:   %s\n", name_);
  absl::StrAppendFormat(&solution, "Authors:         %s\n", authors_);
  absl::StrAppendFormat(&solution, "Date:            %s\n", date);
  absl::StrAppendFormat(&solution, "Reference:       OR-Tools\n");
  absl::StrAppendFormat(&solution, "Solution\n");
  absl::StrAppendFormat(&solution, "%s\n", SerializeToNEARPLIBString());
  absl::StrAppendFormat(&solution, "Total cost:       %d", total_cost_);
  // Official solutions for CBMix use "total cost", whereas the definition of
  // the output format rather uses "cost":
  // https://www.sintef.no/globalassets/project/top/nearp/cbmix-results/cbmix22.txt
  // https://www.sintef.no/globalassets/project/top/nearp/solutionformat.txt
  return solution;
}

namespace {
RoutingSolution::Route RouteFromVector(
    absl::Span<const int64_t> route_int,
    std::optional<int64_t> depot = std::nullopt);

std::vector<RoutingSolution::Route> RoutesFromVector(
    absl::Span<const std::vector<int64_t>> routes,
    std::optional<int64_t> depot) {
  std::vector<RoutingSolution::Route> solution_routes;
  solution_routes.reserve(routes.size());
  for (const std::vector<int64_t>& route : routes) {
    // TODO(user): explore merging RouteFromVector in this function.
    solution_routes.emplace_back(RouteFromVector(route, depot));
  }
  return solution_routes;
}

RoutingSolution::Route RouteFromVector(absl::Span<const int64_t> route_int,
                                       std::optional<int64_t> forced_depot) {
  // One route in input: from the node indices, create a Route object (not yet
  // a RoutingSolution one).
  RoutingSolution::Route route;

  // If no depot is given, guess one.
  int64_t depot =
      (forced_depot.has_value()) ? forced_depot.value() : route_int[0];

  route.emplace_back(
      RoutingSolution::Event{/*type=*/RoutingSolution::Event::Type::kStart,
                             /*demand_id=*/-1, /*arc=*/Arc{depot, depot}});
  for (int64_t i = 0; i < route_int.size() - 1; ++i) {
    int64_t tail = route_int[i];
    int64_t head = route_int[i + 1];
    route.emplace_back(RoutingSolution::Event{
        RoutingSolution::Event::Type::kTransit, -1, Arc{tail, head}});
  }
  route.emplace_back(RoutingSolution::Event{RoutingSolution::Event::Type::kEnd,
                                            -1, Arc{depot, depot}});

  return route;
}

std::string SerializeRouteToCVRPLIBString(const RoutingSolution::Route& route) {
  // Before serializing the route, make some tests to check that the hypotheses
  // are respected (otherwise, the output of the function is highly likely
  // pure garbage).
  const RoutingSolution::Event& first_event = route[0];
  CHECK(first_event.type == RoutingSolution::Event::Type::kStart)
      << "The route does not begin with a Start event to indicate "
         "the depot.";
  const int64_t depot = first_event.arc.tail();

  CHECK_GE(depot, 0) << "The given depot is negative: " << depot;
  CHECK_LE(depot, 1) << "The given depot is greater than 1: " << depot;

  // Serialize this route, ignoring the depot (already dealt with).
  std::string current_route;

  for (int64_t i = 1; i < route.size() - 1; ++i) {
    const RoutingSolution::Event& event = route[i];

    // Ignore the depot, as CVRPLIB doesn't output the depot in the routes
    // (all routes implicitly start and end at the depot).
    int64_t node = event.arc.head();
    if (node > depot) {
      absl::StrAppendFormat(&current_route, "%d ", node - depot);
    }
  }

  // Last event: end at a depot. Due to the strange way CVRPLIB
  // outputs nodes, the depot must be the same at the beginning and the
  // end of the route.
  const RoutingSolution::Event& last_event = route.back();
  if (last_event.type == RoutingSolution::Event::Type::kEnd) {
    CHECK_EQ(depot, last_event.arc.tail());
    CHECK_EQ(last_event.arc.tail(), last_event.arc.head());
  } else {
    LOG(FATAL) << "The route does not finish with an End event to "
                  "indicate the depot.";
  }

  return current_route;
}
}  // namespace
}  // namespace operations_research::routing
