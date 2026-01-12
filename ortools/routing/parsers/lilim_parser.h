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

// A parser for Li&Lim PDPTW (pickup and delivery problems with time windows)
// instances.
// The goal is to find routes starting and end at a depot which visit a set of
// nodes. Nodes are grouped in pairs of pickup and delivery nodes. The pickup
// node of each pair has to be performed before the delivery node and both nodes
// have to be on the same route. The objective is first to minimize the number
// of routes and then to minimize the total distance traveled, distances being
// measured with the Euclidean distance between nodes.
// Routes are subject to two other types of constraints:
// - time windows restricting the time during which a node can be visited,
// - vehicle capacity which limits the load of the vehicles performing the
//   routes (each node has a corresponding demand which must be picked up
//   or delivered by the vehicle).
//
// The format of the data is the following:
// - one row to describe vehicles (which are all identical):
//   <number of vehicles> <vehicle capacity> <speed>
// - followed by a row per node:
//   <node id> <x> <y> <demand> <ready time> <due date> <service time> <pickup
//   index> <delivery index>
//
// Node 0 corresponds to the depot. For pickup nodes, pickup index is 0, and
// delivery index gives the index of the corresponding delivery node. For
// delivery tasks, delivery index is 0, and pickup index gives the index of the
// corresponding pickup node. The value of travel time is equal to the value of
// distance.

#ifndef ORTOOLS_ROUTING_PARSERS_LILIM_PARSER_H_
#define ORTOOLS_ROUTING_PARSERS_LILIM_PARSER_H_

#include <math.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "ortools/routing/parsers/simple_graph.h"

namespace operations_research::routing {

// Li&Lim parser class
class LiLimParser {
 public:
  LiLimParser() = default;
#ifndef SWIG
  LiLimParser(LiLimParser&) = delete;
  const LiLimParser& operator=(const LiLimParser&) = delete;
#endif

  // Loading an instance. Both methods return false in case of failure to read
  // the instance. Loading a new instance clears the previously loaded instance.

  // Loads instance from a file.
  bool LoadFile(absl::string_view file_name);
  // Loads instance from a file contained in a zipped archive; the archive can
  // contain multiple files.
  bool LoadFile(absl::string_view file_name, absl::string_view archive_name);

  // Returns the index of the depot.
  int Depot() const { return 0; }
  // Returns the number of nodes in the current routing problem.
  int NumberOfNodes() const { return coordinates_.size(); }
  // Returns the maximum number of vehicles to use.
  int NumberOfVehicles() const { return vehicles_; }
  // Returns the coordinates of the nodes in the current routing problem.
  const std::vector<Coordinates2<int64_t>>& coordinates() const {
    return coordinates_;
  }
  // Returns the delivery of a pickup.
  std::optional<int> GetDelivery(int node) const;
  // Returns the pickup of a delivery.
  std::optional<int> GetPickup(int node) const;
  // Returns the capacity of the vehicles.
  int64_t capacity() const { return capacity_; }
  // Returns the demand of the nodes in the current routing problem.
  const std::vector<int64_t>& demands() const { return demands_; }
  // Returns the time windows of the nodes in the current routing problem.
  const std::vector<SimpleTimeWindow<int64_t>>& time_windows() const {
    return time_windows_;
  }
  // Returns the service times of the nodes in the current routing problem.
  const std::vector<int64_t>& service_times() const { return service_times_; }
  // Returns the distance between two nodes.
  double GetDistance(int from, int to) const {
    const Coordinates2<int64_t>& from_coords = coordinates_[from];
    const Coordinates2<int64_t>& to_coords = coordinates_[to];
    const double xd = from_coords.x - to_coords.x;
    const double yd = from_coords.y - to_coords.y;
    return sqrt(xd * xd + yd * yd);
  }
  // Returns the travel time between two nodes.
  double GetTravelTime(int from, int to) const {
    return service_times_[from] + GetDistance(from, to);
  }

 private:
  void Initialize();
  bool ParseFile(absl::string_view file_name);

  int vehicles_ = 0;
  std::vector<Coordinates2<int64_t>> coordinates_;
  std::vector<int> pickups_;
  std::vector<int> deliveries_;
  int64_t capacity_ = 0;
  int64_t speed_ = 0;
  std::vector<int64_t> demands_;
  std::vector<SimpleTimeWindow<int64_t>> time_windows_;
  std::vector<int64_t> service_times_;
};
}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_PARSERS_LILIM_PARSER_H_
