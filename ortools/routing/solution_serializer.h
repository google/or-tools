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

// Utilities to serialize VRP-like solutions in standardised formats: either
// TSPLIB or CVRPLIB.

#ifndef OR_TOOLS_ROUTING_SOLUTION_SERIALIZER_H_
#define OR_TOOLS_ROUTING_SOLUTION_SERIALIZER_H_

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/file.h"
#include "ortools/base/helpers.h"
#include "ortools/base/logging.h"
#include "ortools/routing/simple_graph.h"

namespace operations_research {

// Indicates the format in which the output should be done. This enumeration is
// used for solutions and solver statistics.
enum class RoutingOutputFormat {
  kNone,
  kTSPLIB,
  kCVRPLIB,
  kCARPLIB,
  kNEARPLIB
};

// Parses a user-provided description of the output format. Expected inputs look
// like (without quotes): "tsplib", "cvrplib", "carplib". Unrecognized strings
// are parsed as kNone.
RoutingOutputFormat RoutingOutputFormatFromString(std::string_view format);

// Describes completely a solution to a routing problem in preparation of its
// serialization as a string.
class RoutingSolution {
 public:
  // Describes a state transition performed by a vehicle: starting from/ending
  // at a given depot, serving a given customer, etc.
  // When need be, each event can have a specific demand ID (this is mostly
  // useful when servicing arcs and edges). An event always stores an arc:
  // this is simply the edge when servicing the edge (it should correspond to
  // the direction in which the edge is traversed); when the event is about
  // a node (either a depot or a demand), both ends of the arc should be the
  // node the event is about.
  struct Event {
    // Describes the type of events that occur along a route.
    enum class Type {
      // The vehicle starts its route at a depot.
      kStart,
      // The vehicle ends its route at a depot (not necessarily the same as the
      // starting one).
      kEnd,
      // The vehicle traverses the arc while servicing it.
      kServeArc,
      // The vehicle traverses the edge while servicing it.
      kServeEdge,
      // The vehicle serves the demand of the node.
      kServeNode,
      // The vehicle simply goes through an edge or an arc without servicing.
      kTransit
    };

    Type type;
    int64_t demand_id;
    Arc arc;
    std::string arc_name;

    Event(Type type, int64_t demand_id, Arc arc)
        : type(type), demand_id(demand_id), arc(arc) {}
    Event(Type type, int64_t demand_id, Arc arc, std::string_view arc_name)
        : type(type), demand_id(demand_id), arc(arc), arc_name(arc_name) {}

    bool operator==(const Event& other) const {
      return type == other.type && demand_id == other.demand_id &&
             arc == other.arc && arc_name == other.arc_name;
    }
    bool operator!=(const Event& other) const { return !(*this == other); }
  };

  using Route = std::vector<Event>;

  RoutingSolution(std::vector<Route> routes, std::vector<int64_t> total_demands,
                  std::vector<int64_t> total_distances, int64_t total_cost = -1,
                  int64_t total_distance = -1, double total_time = -1.0,
                  std::string_view name = "")
      : routes_(std::move(routes)),
        total_demands_(std::move(total_demands)),
        total_distances_(std::move(total_distances)),
        total_cost_(total_cost),
        total_distance_(total_distance),
        total_time_(total_time),
        name_(name) {
    CHECK_EQ(routes_.size(), total_demands_.size());
    CHECK_EQ(routes_.size(), total_distances_.size());
  }

  bool operator==(const RoutingSolution& other) const {
    return routes_ == other.routes_ && total_demands_ == other.total_demands_ &&
           total_distances_ == other.total_distances_ &&
           total_cost_ == other.total_cost_ && total_time_ == other.total_time_;
  }
  bool operator!=(const RoutingSolution& other) const {
    return !(*this == other);
  }

  // Setters for solution metadata.
  void SetTotalTime(double total_time) { total_time_ = total_time; }
  void SetTotalCost(int64_t total_cost) { total_cost_ = total_cost; }
  void SetTotalDistance(int64_t total_distance) {
    total_distance_ = total_distance;
  }
  void SetName(std::string_view name) { name_ = name; }
  void SetAuthors(std::string_view authors) { authors_ = authors; }

  // Public-facing builders.

  // Splits a list of nodes whose routes are separated by the given separator
  // (TSPLIB uses -1; it is crucial that the separator cannot be a node) into
  // a vector per route, for use in FromSplit* functions.
  static std::vector<std::vector<int64_t>> SplitRoutes(
      const std::vector<int64_t>& solution, int64_t separator);

  // Builds a RoutingSolution object from a vector of routes, each represented
  // as a vector of nodes being traversed. All the routes are supposed to start
  // and end at the depot if specified.
  static RoutingSolution FromSplitRoutes(
      const std::vector<std::vector<int64_t>>& routes,
      std::optional<int64_t> depot = std::nullopt);

  // Serializes the bare solution to a string, i.e. only the routes for the
  // vehicles, without other metadata that is typically present in solution
  // files.
  std::string SerializeToString(RoutingOutputFormat format) const {
    switch (format) {
      case RoutingOutputFormat::kNone:
        return "";
      case RoutingOutputFormat::kTSPLIB:
        return SerializeToTSPLIBString();
      case RoutingOutputFormat::kCVRPLIB:
        return SerializeToCVRPLIBString();
      case RoutingOutputFormat::kCARPLIB:
        return SerializeToCARPLIBString();
      case RoutingOutputFormat::kNEARPLIB:
        return SerializeToNEARPLIBString();
    }
  }

  // Serializes the full solution to the given file, including metadata like
  // instance name or total cost, depending on the format.
  // For TSPLIB, solution files are typically called "tours".
  std::string SerializeToSolutionFile(RoutingOutputFormat format) const {
    switch (format) {
      case RoutingOutputFormat::kNone:
        return "";
      case RoutingOutputFormat::kTSPLIB:
        return SerializeToTSPLIBSolutionFile();
      case RoutingOutputFormat::kCVRPLIB:
        return SerializeToCVRPLIBSolutionFile();
      case RoutingOutputFormat::kCARPLIB:
        return SerializeToCARPLIBSolutionFile();
      case RoutingOutputFormat::kNEARPLIB:
        return SerializeToNEARPLIBSolutionFile();
    }
  }

  // Serializes the full solution to the given file, including metadata like
  // instance name or total cost, depending on the format.
  void WriteToSolutionFile(RoutingOutputFormat format,
                           const std::string& file_name) const;

 private:
  // Description of the solution. Typically, one element per route (e.g., one
  // vector of visited nodes per route). These elements are supposed to be
  // returned by a solver.
  // Depots are not explicitly stored as a route-level attribute, but rather by
  // specific transitions (starting or ending at a depot).
  std::vector<std::vector<Event>> routes_;
  std::vector<int64_t> total_demands_;
  std::vector<int64_t> total_distances_;

  // Solution metadata. These elements could be set either by the solver or by
  // the caller.
  int64_t total_cost_;
  int64_t total_distance_;
  double total_time_;
  std::string name_;
  std::string authors_;

  int64_t NumberOfNonemptyRoutes() const;

  // The various implementations of SerializeToString depending on the format.

  // Generates a string representation of a solution in the TSPLIB format.
  // TSPLIB explicitly outputs the depot in its tours.
  // It has been defined in
  // http://comopt.ifi.uni-heidelberg.de/software/TSPLIB95/ where solutions are
  // referred to as "tours".
  std::string SerializeToTSPLIBString() const;
  // Generates a string representation of a solution in the CVRPLIB format.
  // CVRPLIB doesn't explicitly output the depot in its tours.
  // Format used in http://vrp.atd-lab.inf.puc-rio.br/
  // Better description of the format:
  // http://dimacs.rutgers.edu/programs/challenge/vrp/cvrp/
  std::string SerializeToCVRPLIBString() const;
  // Generates a string representation of a solution in the CARPLIB format.
  // Format used in https://www.uv.es/belengue/carp.html
  // Formal description of the format: https://www.uv.es/~belengue/carp/READ_ME
  // Another description of the format:
  // http://dimacs.rutgers.edu/programs/challenge/vrp/carp/
  std::string SerializeToCARPLIBString() const;
  // Generates a string representation of a solution in the NEARPLIB format.
  // Format used in https://www.sintef.no/projectweb/top/nearp/
  // Formal description of the format:
  // https://www.sintef.no/projectweb/top/nearp/documentation/
  // Example:
  // https://www.sintef.no/globalassets/project/top/nearp/solutionformat.txt
  std::string SerializeToNEARPLIBString() const;

  // The various implementations of SerializeToSolutionFile depending on the
  // format. These methods are highly similar to the previous ones.
  std::string SerializeToTSPLIBSolutionFile() const;
  std::string SerializeToCVRPLIBSolutionFile() const;
  std::string SerializeToCARPLIBSolutionFile() const;
  std::string SerializeToNEARPLIBSolutionFile() const;
};

// Formats a solution or solver statistic according to the given format.
template <typename T>
std::string FormatStatistic(const std::string& name, T value,
                            RoutingOutputFormat format) {
  // TODO(user): think about using an enum instead of names (or even a
  // full-fledged struct/class) for the various types of fields.
  switch (format) {
    case RoutingOutputFormat::kNone:
      ABSL_FALLTHROUGH_INTENDED;
    case RoutingOutputFormat::kTSPLIB:
      return absl::StrCat(name, " = ", value);
    case RoutingOutputFormat::kCVRPLIB:
      return absl::StrCat(name, " ", value);
    case RoutingOutputFormat::kCARPLIB:
      // For CARPLIB, the statistics do not have names, it's up to the user to
      // memorize their order.
      return absl::StrCat(value);
    case RoutingOutputFormat::kNEARPLIB:
      return absl::StrCat(name, " : ", value);
  }
}

// Specialization for doubles to show a higher precision: without this
// specialization, 591.556557 is displayed as 591.557.
template <>
inline std::string FormatStatistic(const std::string& name, double value,
                                   RoutingOutputFormat format) {
  switch (format) {
    case RoutingOutputFormat::kNone:
      ABSL_FALLTHROUGH_INTENDED;
    case RoutingOutputFormat::kTSPLIB:
      return absl::StrFormat("%s = %f", name, value);
    case RoutingOutputFormat::kCVRPLIB:
      return absl::StrFormat("%s %f", name, value);
    case RoutingOutputFormat::kCARPLIB:
      return absl::StrFormat("%f", value);
    case RoutingOutputFormat::kNEARPLIB:
      return absl::StrFormat("%s : %f", name, value);
  }
}

// Prints a formatted solution or solver statistic according to the given
// format.
template <typename T>
void PrintStatistic(const std::string& name, T value,
                    RoutingOutputFormat format) {
  absl::PrintF("%s\n", FormatStatistic(name, value, format));
}
}  // namespace operations_research

#endif  // OR_TOOLS_ROUTING_SOLUTION_SERIALIZER_H_
